// src/Interpreters/interpreter_select_query.cpp — AST-driven SELECT execution

#include "Interpreters/interpreter_select_query.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Databases/database.h"
#include "Columns/column_string.h"
#include "Columns/column_vector.h"
#include "Common/exceptions.h"
#include "DataTypes/data_type_factory.h"
#include "Storages/i_storage.h"
#include "Connectors/connector_factory.h"
#include "Connectors/connector_manager.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <numeric>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace mnemo::interpreters {

namespace {

using parsers::ASTAlias;
using parsers::ASTBinaryOp;
using parsers::ASTColumnRef;
using parsers::ASTExpr;
using parsers::ASTFunction;
using parsers::ASTLiteral;
using parsers::ASTSubQueryExpr;
using parsers::ASTUnaryOp;
using parsers::QueryAST;

struct EvalCtx
{
    Context& context;
    const core::Block* block = nullptr;
    size_t row = 0;
    std::unordered_map<std::string, std::string> alias_to_table;
    std::unordered_set<std::string>* expanding = nullptr;

    // Rows of `block` that form the current group (or window partition). Aggregate
    // functions are only legal while this is set.
    const std::vector<size_t>* group_rows = nullptr;

    // Select-list aliases, used where no column of that name exists (WHERE, GROUP BY, HAVING).
    const std::unordered_map<std::string, const ASTExpr*>* select_aliases = nullptr;

    // Uncorrelated subquery results, so a subquery runs once per statement instead of once per row.
    std::unordered_map<const ASTSubQueryExpr*, std::shared_ptr<core::Block>>* subquery_cache = nullptr;

    int alias_depth = 0;
};

auto eval_select_with_expanding(Context& context, const QueryAST& query,
                                std::unordered_set<std::string>& expanding) -> core::Block;

auto require_table_storage(Context& ctx, const std::string& table)
    -> std::shared_ptr<storages::IStorage> {
    auto storage = ddl_utils::resolve_storage(ctx, table);
    if (!storage) {
        throw common::Exception{
            "Unknown table: " + table,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    return storage;
}

constexpr size_t kMaxViewDepth = 32;
constexpr int kMaxAliasDepth = 16;

auto load_table(Context& ctx, const std::string& table, const std::string& prefix,
                std::unordered_set<std::string>& expanding) -> core::Block {
    if (auto idb = ctx.get_database(ddl_utils::resolve_current_database(ctx))) {
        if (auto db = std::dynamic_pointer_cast<databases::Database>(idb)) {
            if (db->has_view(table)) {
                if (expanding.contains(table)) {
                    throw common::Exception{
                        "Circular view reference: " + table,
                        static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
                }
                if (expanding.size() >= kMaxViewDepth) {
                    throw common::Exception{
                        "View expansion depth exceeded",
                        static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
                }
                expanding.insert(table);
                auto entry = db->get_view(table).value();
                QueryAST view_query;
                view_query.query_type = QueryAST::QueryType::SELECT;
                view_query.select = entry.definition;
                auto block = eval_select_with_expanding(ctx, view_query, expanding);
                expanding.erase(table);
                if (prefix.empty() || prefix == table) {
                    return block;
                }
                core::Block out;
                for (const auto& name : block.column_names()) {
                    if (auto col = block.get_column(name)) {
                        out.add_column(prefix + "." + name, col->clone());
                    }
                }
                return out;
            }
        }
    }

    auto storage = require_table_storage(ctx, table);
    auto block = storage->read(storage->columns());
    if (prefix.empty() || prefix == table) {
        return block;
    }
    core::Block out;
    for (const auto& name : block.column_names()) {
        auto col = block.get_column(name);
        if (col) {
            out.add_column(prefix + "." + name, col->clone());
        }
    }
    return out;
}

// ── Field helpers: classification, conversion, comparison ──

auto to_upper(std::string text) -> std::string
{
    std::transform(text.begin(), text.end(), text.begin(), ::toupper);
    return text;
}

auto is_integer_field(const core::Field& f) -> bool
{
    return std::visit([](auto&& v) -> bool {
        return std::is_integral_v<std::decay_t<decltype(v)>>;
    }, f.variant());
}

auto is_float_field(const core::Field& f) -> bool
{
    return std::visit([](auto&& v) -> bool {
        return std::is_floating_point_v<std::decay_t<decltype(v)>>;
    }, f.variant());
}

auto is_string_field(const core::Field& f) -> bool
{
    return std::holds_alternative<std::string>(f.variant());
}

auto is_numeric_field(const core::Field& f) -> bool
{
    return is_integer_field(f) || is_float_field(f);
}

auto field_to_int64(const core::Field& f) -> int64_t {
  return std::visit([](auto&& v) -> int64_t {
    using T = std::decay_t<decltype(v)>;
    if constexpr (std::is_integral_v<T>) return static_cast<int64_t>(v);
    if constexpr (std::is_floating_point_v<T>) return static_cast<int64_t>(v);
    if constexpr (std::is_same_v<T, std::string>) {
      try { return std::stoll(v); } catch (...) { return 0; }
    }
    return 0;
  }, f.variant());
}

auto field_to_double(const core::Field& f) -> double {
  return std::visit([](auto&& v) -> double {
    using T = std::decay_t<decltype(v)>;
    if constexpr (std::is_floating_point_v<T>) return static_cast<double>(v);
    if constexpr (std::is_integral_v<T>) return static_cast<double>(v);
    if constexpr (std::is_same_v<T, std::string>) {
      try { return std::stod(v); } catch (...) { return 0.0; }
    }
    return 0.0;
  }, f.variant());
}

auto format_double(double value) -> std::string
{
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return std::string(buffer, result.ptr);
}

auto field_to_string(const core::Field& f) -> std::string
{
    return std::visit([](auto&& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) {
            return v;
        } else if constexpr (std::is_same_v<T, bool>) {
            return v ? "1" : "0";
        } else if constexpr (std::is_integral_v<T>) {
            return std::to_string(static_cast<int64_t>(v));
        } else if constexpr (std::is_floating_point_v<T>) {
            return format_double(static_cast<double>(v));
        } else {
            return std::string{};
        }
    }, f.variant());
}

auto is_truthy(const core::Field& f) -> bool
{
    if (f.is_null()) {
        return false;
    }
    if (is_string_field(f)) {
        return !std::get<std::string>(f.variant()).empty();
    }
    if (is_float_field(f)) {
        return field_to_double(f) != 0.0;
    }
    return field_to_int64(f) != 0;
}

// Three-way comparison. Returns nullopt when either side is NULL or when a string is compared
// with a number, so such predicates are simply not satisfied rather than matching by accident.
auto compare_fields(const core::Field& a, const core::Field& b) -> std::optional<int>
{
    if (a.is_null() || b.is_null()) {
        return std::nullopt;
    }
    const bool a_string = is_string_field(a);
    const bool b_string = is_string_field(b);
    if (a_string && b_string) {
        const int c = std::get<std::string>(a.variant()).compare(std::get<std::string>(b.variant()));
        return c < 0 ? -1 : (c > 0 ? 1 : 0);
    }
    if (a_string || b_string) {
        return std::nullopt;
    }
    if (is_integer_field(a) && is_integer_field(b)) {
        const auto x = field_to_int64(a);
        const auto y = field_to_int64(b);
        return x < y ? -1 : (x > y ? 1 : 0);
    }
    const double x = field_to_double(a);
    const double y = field_to_double(b);
    if (std::isnan(x) || std::isnan(y)) {
        return std::nullopt;
    }
    return x < y ? -1 : (x > y ? 1 : 0);
}

// Total order for ORDER BY: NULL < numbers < strings.
auto total_compare(const core::Field& a, const core::Field& b) -> int
{
    const auto rank = [](const core::Field& f) {
        return f.is_null() ? 0 : (is_string_field(f) ? 2 : 1);
    };
    const int ra = rank(a);
    const int rb = rank(b);
    if (ra != rb) {
        return ra < rb ? -1 : 1;
    }
    if (ra == 0) {
        return 0;
    }
    return compare_fields(a, b).value_or(0);
}

// Canonical text of a value for grouping: 1 and 1.0 share a key, 1.2 and 1.7 do not.
auto field_key(const core::Field& f) -> std::string
{
    if (f.is_null()) {
        return "n";
    }
    if (is_string_field(f)) {
        return "s" + std::get<std::string>(f.variant());
    }
    if (is_float_field(f)) {
        const double d = field_to_double(f);
        if (std::isfinite(d) && d == std::floor(d) && std::fabs(d) < 9.0e15) {
            return "i" + std::to_string(static_cast<int64_t>(d));
        }
        return "d" + format_double(d);
    }
    return "i" + std::to_string(field_to_int64(f));
}

auto arithmetic(ASTBinaryOp::Op op, const core::Field& l, const core::Field& r) -> core::Field
{
    if (l.is_null() || r.is_null()) {
        return core::Field{};
    }
    if (!is_numeric_field(l) || !is_numeric_field(r)) {
        throw common::Exception{
            "Arithmetic requires numeric operands",
            static_cast<int>(common::ErrorCode::ILLEGAL_TYPE)};
    }
    const bool both_int = is_integer_field(l) && is_integer_field(r);
    switch (op) {
        case ASTBinaryOp::Op::Add:
            return both_int ? core::Field{field_to_int64(l) + field_to_int64(r)}
                            : core::Field{field_to_double(l) + field_to_double(r)};
        case ASTBinaryOp::Op::Sub:
            return both_int ? core::Field{field_to_int64(l) - field_to_int64(r)}
                            : core::Field{field_to_double(l) - field_to_double(r)};
        case ASTBinaryOp::Op::Mul:
            return both_int ? core::Field{field_to_int64(l) * field_to_int64(r)}
                            : core::Field{field_to_double(l) * field_to_double(r)};
        case ASTBinaryOp::Op::Div:
            return core::Field{field_to_double(l) / field_to_double(r)};
        case ASTBinaryOp::Op::Mod:
            if (both_int) {
                const auto divisor = field_to_int64(r);
                if (divisor == 0) {
                    throw common::Exception{
                        "Division by zero in modulo",
                        static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
                }
                return core::Field{field_to_int64(l) % divisor};
            }
            return core::Field{std::fmod(field_to_double(l), field_to_double(r))};
        default:
            break;
    }
    throw common::Exception{
        "Unsupported arithmetic operator",
        static_cast<int>(common::ErrorCode::NOT_IMPLEMENTED)};
}

auto find_column(const core::Block& block, const std::string& table,
                 const std::string& column,
                 const std::unordered_map<std::string, std::string>& alias_to_table)
    -> std::optional<size_t> {
    std::vector<std::string> candidates;
    if (!table.empty()) {
        candidates.push_back(table + "." + column);
        auto it = alias_to_table.find(table);
        if (it != alias_to_table.end()) {
            candidates.push_back(it->second + "." + column);
            candidates.push_back(table + "." + column);
        }
    }
    candidates.push_back(column);
    if (table.empty()) {
        for (const auto& [alias, base] : alias_to_table) {
            candidates.push_back(alias + "." + column);
            candidates.push_back(base + "." + column);
        }
    }
    const auto names = block.column_names();
    for (const auto& cand : candidates) {
        for (size_t i = 0; i < names.size(); ++i) {
            if (names[i] == cand) return i;
        }
    }
    return std::nullopt;
}

// ── Aggregates ──

auto is_aggregate_name(const std::string& upper) -> bool
{
    return upper == "COUNT" || upper == "SUM" || upper == "AVG" || upper == "MIN" ||
           upper == "MAX" || upper == "QUANTILE" || upper == "PERCENTILE" || upper == "MEDIAN";
}

auto unwrap_expr(const ASTExpr* expr) -> const ASTExpr*
{
    if (auto* alias = dynamic_cast<const ASTAlias*>(expr)) {
        return alias->expression.get();
    }
    return expr;
}

auto as_window_function(const ASTExpr* item) -> const ASTFunction*
{
    const auto* func = dynamic_cast<const ASTFunction*>(unwrap_expr(item));
    return (func && func->window) ? func : nullptr;
}

// True when the expression uses an aggregate function outside of a window.
auto contains_aggregate(const ASTExpr& expr) -> bool
{
    if (auto* alias = dynamic_cast<const ASTAlias*>(&expr)) {
        return contains_aggregate(*alias->expression);
    }
    if (auto* bin = dynamic_cast<const ASTBinaryOp*>(&expr)) {
        return contains_aggregate(*bin->left) || contains_aggregate(*bin->right);
    }
    if (auto* un = dynamic_cast<const ASTUnaryOp*>(&expr)) {
        return contains_aggregate(*un->operand);
    }
    if (auto* func = dynamic_cast<const ASTFunction*>(&expr)) {
        if (func->window) {
            return false;
        }
        if (is_aggregate_name(to_upper(func->name))) {
            return true;
        }
        return std::any_of(func->args.begin(), func->args.end(),
                           [](const auto& arg) { return contains_aggregate(*arg); });
    }
    return false;
}

// Extracts a numeric literal argument (int or float) as a double.
auto literal_arg_as_double(const ASTExpr* expr) -> std::optional<double>
{
    auto* lit = dynamic_cast<const ASTLiteral*>(expr);
    if (!lit) return std::nullopt;
    if (std::holds_alternative<double>(lit->value)) return std::get<double>(lit->value);
    if (std::holds_alternative<int64_t>(lit->value)) return static_cast<double>(std::get<int64_t>(lit->value));
    return std::nullopt;
}

// Linear-interpolation quantile (level in [0, 1]) over a non-empty set of values.
auto interpolated_quantile(std::vector<double> vals, double level) -> double
{
    std::sort(vals.begin(), vals.end());
    const double pos = level * static_cast<double>(vals.size() - 1);
    const auto lo = static_cast<size_t>(std::floor(pos));
    const auto hi = static_cast<size_t>(std::ceil(pos));
    const double frac = pos - static_cast<double>(lo);
    return vals[lo] + (vals[hi] - vals[lo]) * frac;
}

// quantile(0.5)(x), median(x), percentile(x, 50) and the older quantile(x, 0.5) are all accepted.
auto quantile_level(const ASTFunction& func, const std::string& upper) -> double
{
    if (upper == "MEDIAN") {
        return 0.5;
    }
    const ASTExpr* level_expr = nullptr;
    if (!func.parameters.empty()) {
        level_expr = func.parameters.front().get();
    } else if (func.args.size() > 1) {
        level_expr = func.args[1].get();
    }
    const auto level = level_expr ? literal_arg_as_double(level_expr) : std::nullopt;
    if (!level) {
        throw common::Exception{
            upper + " needs a numeric level, e.g. quantile(0.5)(x)",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    const double normalised = upper == "PERCENTILE" ? *level / 100.0 : *level;
    if (normalised < 0.0 || normalised > 1.0) {
        throw common::Exception{
            upper + " level must be within the valid range",
            static_cast<int>(common::ErrorCode::ILLEGAL_TYPE)};
    }
    return normalised;
}

auto eval_expr(const ASTExpr& expr, EvalCtx& ctx) -> core::Field;

// Non-NULL values of the aggregate's argument over the rows of the current group.
auto aggregate_values(const ASTFunction& func, EvalCtx& ctx) -> std::vector<core::Field>
{
    if (func.args.empty()) {
        throw common::Exception{
            func.name + " needs an argument",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    EvalCtx row_ctx = ctx;
    row_ctx.group_rows = nullptr;
    std::vector<core::Field> values;
    values.reserve(ctx.group_rows->size());
    for (const auto row : *ctx.group_rows) {
        row_ctx.row = row;
        auto value = eval_expr(*func.args.front(), row_ctx);
        if (!value.is_null()) {
            values.push_back(std::move(value));
        }
    }
    return values;
}

auto compute_aggregate(const ASTFunction& func, EvalCtx& ctx) -> core::Field
{
    const auto upper = to_upper(func.name);
    const auto& rows = *ctx.group_rows;

    if (upper == "COUNT") {
        const bool count_all = func.args.empty() ||
            (dynamic_cast<const ASTColumnRef*>(func.args.front().get()) &&
             static_cast<const ASTColumnRef*>(func.args.front().get())->column == "*");
        if (count_all) {
            return core::Field{static_cast<int64_t>(rows.size())};
        }
        return core::Field{static_cast<int64_t>(aggregate_values(func, ctx).size())};
    }

    auto values = aggregate_values(func, ctx);

    if (upper == "MIN" || upper == "MAX") {
        if (values.empty()) {
            return core::Field{};
        }
        auto best = values.begin();
        for (auto it = values.begin() + 1; it != values.end(); ++it) {
            const auto c = compare_fields(*it, *best);
            if (c && (upper == "MIN" ? *c < 0 : *c > 0)) {
                best = it;
            }
        }
        return *best;
    }

    for (const auto& value : values) {
        if (!is_numeric_field(value)) {
            throw common::Exception{
                upper + " needs numeric values",
                static_cast<int>(common::ErrorCode::ILLEGAL_TYPE)};
        }
    }

    if (upper == "SUM") {
        const bool all_int = std::all_of(values.begin(), values.end(), is_integer_field);
        if (all_int) {
            int64_t sum = 0;
            for (const auto& value : values) sum += field_to_int64(value);
            return core::Field{sum};
        }
        double sum = 0.0;
        for (const auto& value : values) sum += field_to_double(value);
        return core::Field{sum};
    }

    if (upper == "AVG") {
        if (values.empty()) {
            return core::Field{};
        }
        double sum = 0.0;
        for (const auto& value : values) sum += field_to_double(value);
        return core::Field{sum / static_cast<double>(values.size())};
    }

    if (upper == "QUANTILE" || upper == "PERCENTILE" || upper == "MEDIAN") {
        const double level = quantile_level(func, upper);
        if (values.empty()) {
            return core::Field{};
        }
        std::vector<double> numbers;
        numbers.reserve(values.size());
        for (const auto& value : values) numbers.push_back(field_to_double(value));
        return core::Field{interpolated_quantile(std::move(numbers), level)};
    }

    throw common::Exception{
        "Unknown aggregate function: " + func.name,
        static_cast<int>(common::ErrorCode::UNKNOWN_FUNCTION)};
}

// ── Expression evaluation ──

auto subquery_block(const ASTSubQueryExpr& sub, EvalCtx& ctx) -> std::shared_ptr<core::Block>
{
    if (ctx.subquery_cache) {
        const auto it = ctx.subquery_cache->find(&sub);
        if (it != ctx.subquery_cache->end()) {
            return it->second;
        }
    }
    std::unordered_set<std::string> local_expanding;
    auto& expanding = ctx.expanding ? *ctx.expanding : local_expanding;
    auto block = std::make_shared<core::Block>(
        eval_select_with_expanding(ctx.context, *sub.query, expanding));
    if (ctx.subquery_cache) {
        (*ctx.subquery_cache)[&sub] = block;
    }
    return block;
}

auto eval_binary(const ASTBinaryOp& bin, EvalCtx& ctx) -> core::Field
{
    using Op = ASTBinaryOp::Op;

    if (bin.op == Op::And) {
        return core::Field{is_truthy(eval_expr(*bin.left, ctx)) && is_truthy(eval_expr(*bin.right, ctx))};
    }
    if (bin.op == Op::Or) {
        return core::Field{is_truthy(eval_expr(*bin.left, ctx)) || is_truthy(eval_expr(*bin.right, ctx))};
    }
    if (bin.op == Op::In) {
        const auto left = eval_expr(*bin.left, ctx);
        const auto* sub = dynamic_cast<const ASTSubQueryExpr*>(bin.right.get());
        if (!sub || !sub->query) {
            throw common::Exception{
                "IN supports only a subquery on the right-hand side",
                static_cast<int>(common::ErrorCode::NOT_IMPLEMENTED)};
        }
        const auto block = subquery_block(*sub, ctx);
        if (block->column_count() == 0) {
            return core::Field{false};
        }
        for (size_t r = 0; r < block->row_count(); ++r) {
            const auto c = compare_fields(left, block->get_row_value(0, r));
            if (c && *c == 0) {
                return core::Field{true};
            }
        }
        return core::Field{false};
    }

    const auto l = eval_expr(*bin.left, ctx);
    const auto r = eval_expr(*bin.right, ctx);
    switch (bin.op) {
        case Op::Add:
        case Op::Sub:
        case Op::Mul:
        case Op::Div:
        case Op::Mod:
            return arithmetic(bin.op, l, r);
        case Op::Eq: { const auto c = compare_fields(l, r); return core::Field{c.has_value() && *c == 0}; }
        case Op::Ne: { const auto c = compare_fields(l, r); return core::Field{c.has_value() && *c != 0}; }
        case Op::Gt: { const auto c = compare_fields(l, r); return core::Field{c.has_value() && *c > 0}; }
        case Op::Lt: { const auto c = compare_fields(l, r); return core::Field{c.has_value() && *c < 0}; }
        case Op::Ge: { const auto c = compare_fields(l, r); return core::Field{c.has_value() && *c >= 0}; }
        case Op::Le: { const auto c = compare_fields(l, r); return core::Field{c.has_value() && *c <= 0}; }
        default:
            break;
    }
    throw common::Exception{
        "LIKE is not supported yet",
        static_cast<int>(common::ErrorCode::NOT_IMPLEMENTED)};
}

auto eval_column_ref(const ASTColumnRef& col, EvalCtx& ctx) -> core::Field
{
    if (col.column == "*") {
        throw common::Exception{
            "'*' is not allowed in this expression",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    if (ctx.block) {
        if (const auto idx = find_column(*ctx.block, col.table, col.column, ctx.alias_to_table)) {
            if (ctx.row >= ctx.block->row_count()) {
                throw common::Exception{
                    "Column '" + col.column + "' has no value in this context",
                    static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
            }
            return ctx.block->get_row_value(*idx, ctx.row);
        }
    }
    if (col.table.empty() && ctx.select_aliases) {
        const auto it = ctx.select_aliases->find(col.column);
        if (it != ctx.select_aliases->end()) {
            if (ctx.alias_depth >= kMaxAliasDepth) {
                throw common::Exception{
                    "Circular alias: " + col.column,
                    static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
            }
            EvalCtx alias_ctx = ctx;
            alias_ctx.alias_depth = ctx.alias_depth + 1;
            return eval_expr(*it->second, alias_ctx);
        }
    }
    throw common::Exception{
        "Unknown column: " + (col.table.empty() ? col.column : col.table + "." + col.column),
        static_cast<int>(common::ErrorCode::UNKNOWN_COLUMN)};
}

auto eval_expr(const ASTExpr& expr, EvalCtx& ctx) -> core::Field
{
    if (auto* alias = dynamic_cast<const ASTAlias*>(&expr)) {
        return eval_expr(*alias->expression, ctx);
    }
    if (auto* lit = dynamic_cast<const ASTLiteral*>(&expr)) {
        if (lit->is_null()) return core::Field{nullptr};
        if (std::holds_alternative<int64_t>(lit->value)) {
            return core::Field{std::get<int64_t>(lit->value)};
        }
        if (std::holds_alternative<double>(lit->value)) {
            return core::Field{std::get<double>(lit->value)};
        }
        if (std::holds_alternative<std::string>(lit->value)) {
            return core::Field{std::get<std::string>(lit->value)};
        }
        if (std::holds_alternative<bool>(lit->value)) {
            return core::Field{std::get<bool>(lit->value)};
        }
        return core::Field{nullptr};
    }
    if (auto* col = dynamic_cast<const ASTColumnRef*>(&expr)) {
        return eval_column_ref(*col, ctx);
    }
    if (auto* un = dynamic_cast<const ASTUnaryOp*>(&expr)) {
        const auto operand = eval_expr(*un->operand, ctx);
        switch (un->op) {
            case ASTUnaryOp::Op::Neg:
                if (operand.is_null()) return core::Field{};
                if (!is_numeric_field(operand)) {
                    throw common::Exception{
                        "Unary minus requires a numeric operand",
                        static_cast<int>(common::ErrorCode::ILLEGAL_TYPE)};
                }
                return is_integer_field(operand) ? core::Field{-field_to_int64(operand)}
                                                 : core::Field{-field_to_double(operand)};
            case ASTUnaryOp::Op::Not:
                return core::Field{!is_truthy(operand)};
            case ASTUnaryOp::Op::Plus:
                return operand;
            default:
                break;
        }
        throw common::Exception{
            "Unsupported unary operator",
            static_cast<int>(common::ErrorCode::NOT_IMPLEMENTED)};
    }
    if (auto* bin = dynamic_cast<const ASTBinaryOp*>(&expr)) {
        return eval_binary(*bin, ctx);
    }
    if (auto* sub = dynamic_cast<const ASTSubQueryExpr*>(&expr)) {
        if (!sub->query) return core::Field{};
        const auto block = subquery_block(*sub, ctx);
        if (block->row_count() == 0 || block->column_count() == 0) return core::Field{int64_t{0}};
        return block->get_row_value(0, 0);
    }
    if (auto* func = dynamic_cast<const ASTFunction*>(&expr)) {
        if (func->window) {
            throw common::Exception{
                "Window function " + func->name + " is not allowed in this expression",
                static_cast<int>(common::ErrorCode::NOT_IMPLEMENTED)};
        }
        if (is_aggregate_name(to_upper(func->name))) {
            if (!ctx.group_rows) {
                throw common::Exception{
                    "Aggregate function " + func->name + " is not allowed here",
                    static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
            }
            return compute_aggregate(*func, ctx);
        }
        throw common::Exception{
            "Unknown function: " + func->name,
            static_cast<int>(common::ErrorCode::UNKNOWN_FUNCTION)};
    }
    throw common::Exception{
        "Unsupported expression: " + expr.to_string(),
        static_cast<int>(common::ErrorCode::NOT_IMPLEMENTED)};
}

auto eval_predicate(const ASTExpr& expr, EvalCtx& ctx) -> bool
{
    return is_truthy(eval_expr(expr, ctx));
}

// ── Typed output columns ──

auto make_column(const std::string& type_name) -> core::ColumnPtr
{
    auto type = datatypes::get_data_type(type_name);
    auto* raw = type->create_column();
    return std::shared_ptr<core::IColumn>(
        static_cast<core::IColumn*>(raw),
        [](void* p) { delete static_cast<core::IColumn*>(p); });
}

// Builds a column whose type fits the values: String if any value is text, Float64 if any is
// fractional, otherwise Int64. NULL becomes the type's default because columns have no null bitmap.
auto column_from_fields(const std::vector<core::Field>& values) -> core::ColumnPtr
{
    bool any_string = false;
    bool any_float = false;
    for (const auto& value : values) {
        if (is_string_field(value)) any_string = true;
        else if (is_float_field(value)) any_float = true;
    }
    auto column = make_column(any_string ? "String" : (any_float ? "Float64" : "Int64"));
    for (const auto& value : values) {
        if (any_string) {
            column->insert(core::Field{value.is_null() ? std::string{} : field_to_string(value)});
        } else if (any_float) {
            column->insert(core::Field{value.is_null() ? 0.0 : field_to_double(value)});
        } else {
            column->insert(core::Field{value.is_null() ? int64_t{0} : field_to_int64(value)});
        }
    }
    return column;
}

auto select_output_name(const ASTExpr& expr) -> std::string
{
    if (auto* alias = dynamic_cast<const ASTAlias*>(&expr)) {
        return alias->alias;
    }
    if (auto* col = dynamic_cast<const ASTColumnRef*>(&expr)) {
        return col->column;
    }
    return expr.to_string();
}

// ── Row-level operators ──

auto filter_block(const core::Block& in, const ASTExpr* where, EvalCtx base_ctx) -> core::Block {
    if (!where || in.row_count() == 0) return in.clone();
    std::vector<size_t> keep;
    for (size_t r = 0; r < in.row_count(); ++r) {
        base_ctx.block = &in;
        base_ctx.row = r;
        if (eval_predicate(*where, base_ctx)) keep.push_back(r);
    }
    core::Block out;
    for (const auto& name : in.column_names()) {
        auto col = in.get_column(name);
        if (!col) continue;
        auto new_col = col->clone_empty();
        for (auto r : keep) {
            new_col->insert(col->get(r));
        }
        out.add_column(name, new_col);
    }
    return out;
}

auto join_blocks(const core::Block& left, const core::Block& right,
                 const ASTExpr& on, EvalCtx ctx) -> core::Block {
    core::Block out;
    for (const auto& name : left.column_names()) {
        if (auto col = left.get_column(name)) {
            out.add_column(name, col->clone_empty());
        }
    }
    const size_t left_cols = out.column_count();
    for (const auto& name : right.column_names()) {
        if (auto col = right.get_column(name)) {
            out.add_column(name, col->clone_empty());
        }
    }
    for (size_t lr = 0; lr < left.row_count(); ++lr) {
        for (size_t rr = 0; rr < right.row_count(); ++rr) {
            core::Block row_block;
            for (const auto& name : left.column_names()) {
                auto col = left.get_column(name);
                if (!col) continue;
                auto single = col->clone_empty();
                single->insert(col->get(lr));
                row_block.add_column(name, single);
            }
            for (const auto& name : right.column_names()) {
                auto col = right.get_column(name);
                if (!col) continue;
                auto single = col->clone_empty();
                single->insert(col->get(rr));
                row_block.add_column(name, single);
            }
            EvalCtx jctx = ctx;
            jctx.block = &row_block;
            jctx.row = 0;
            if (!eval_predicate(on, jctx)) continue;
            for (size_t c = 0; c < left.column_count(); ++c) {
                out.get_column_by_index(c)->insert(left.get_row_value(c, lr));
            }
            for (size_t c = 0; c < right.column_count(); ++c) {
                out.get_column_by_index(left_cols + c)->insert(right.get_row_value(c, rr));
            }
        }
    }
    return out;
}

// Appends the output column(s) of one select item evaluated row by row over `in`.
// A plain column is copied as-is so its storage type is kept; "*" expands to every column.
auto append_item_columns(core::Block& out, const core::Block& in, const ASTExpr& item, EvalCtx& ctx)
    -> void
{
    const auto* base = unwrap_expr(&item);
    if (auto* col_ref = dynamic_cast<const ASTColumnRef*>(base)) {
        if (col_ref->column == "*") {
            for (const auto& name : in.column_names()) {
                if (auto col = in.get_column(name)) {
                    out.add_column(name, col->clone());
                }
            }
            return;
        }
        if (const auto idx = find_column(in, col_ref->table, col_ref->column, ctx.alias_to_table)) {
            out.add_column(select_output_name(item), in.get_column_by_index(*idx)->clone());
            return;
        }
    }
    std::vector<core::Field> values;
    values.reserve(in.row_count());
    ctx.block = &in;
    for (size_t r = 0; r < in.row_count(); ++r) {
        ctx.row = r;
        values.push_back(eval_expr(*base, ctx));
    }
    out.add_column(select_output_name(item), column_from_fields(values));
}

auto project_block(const core::Block& in, const std::vector<std::shared_ptr<ASTExpr>>& cols,
                   EvalCtx ctx) -> core::Block
{
    core::Block out;
    for (const auto& item : cols) {
        append_item_columns(out, in, *item, ctx);
    }
    return out;
}

// ── GROUP BY / aggregation ──

// Rejects a select/HAVING expression that mixes ungrouped columns with aggregates, which would
// otherwise silently report the first row of each group.
auto validate_grouped(const ASTExpr& expr, const std::unordered_set<std::string>& group_keys,
                      const std::unordered_map<std::string, const ASTExpr*>& aliases) -> void
{
    if (group_keys.contains(expr.to_string())) {
        return;
    }
    if (auto* alias = dynamic_cast<const ASTAlias*>(&expr)) {
        validate_grouped(*alias->expression, group_keys, aliases);
        return;
    }
    if (auto* func = dynamic_cast<const ASTFunction*>(&expr)) {
        if (!is_aggregate_name(to_upper(func->name))) {
            for (const auto& arg : func->args) validate_grouped(*arg, group_keys, aliases);
        }
        return;
    }
    if (auto* col = dynamic_cast<const ASTColumnRef*>(&expr)) {
        if (col->column == "*") {
            throw common::Exception{
                "SELECT * cannot be combined with aggregation",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        if (col->table.empty() && aliases.contains(col->column)) {
            return;
        }
        if (group_keys.contains(col->column)) {
            return;
        }
        throw common::Exception{
            "Column '" + col->to_string() + "' must appear in GROUP BY or inside an aggregate function",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    if (auto* bin = dynamic_cast<const ASTBinaryOp*>(&expr)) {
        validate_grouped(*bin->left, group_keys, aliases);
        validate_grouped(*bin->right, group_keys, aliases);
        return;
    }
    if (auto* un = dynamic_cast<const ASTUnaryOp*>(&expr)) {
        validate_grouped(*un->operand, group_keys, aliases);
    }
}

auto aggregate_block(const core::Block& in, const QueryAST::Select& sel, EvalCtx ctx) -> core::Block
{
    std::unordered_set<std::string> group_keys;
    for (const auto& group_expr : sel.group_by) {
        group_keys.insert(group_expr->to_string());
        // GROUP BY may name a select alias; accept the aliased expression's own text as well.
        if (auto* col = dynamic_cast<const ASTColumnRef*>(group_expr.get())) {
            const auto it = ctx.select_aliases->find(col->column);
            if (col->table.empty() && it != ctx.select_aliases->end()) {
                group_keys.insert(it->second->to_string());
            }
        }
    }
    for (const auto& item : sel.columns) {
        validate_grouped(*item, group_keys, *ctx.select_aliases);
    }
    if (sel.having) {
        validate_grouped(*sel.having, group_keys, *ctx.select_aliases);
    }

    struct Group
    {
        std::vector<size_t> rows;
    };
    std::vector<Group> groups;
    if (sel.group_by.empty()) {
        // Aggregates without GROUP BY always produce exactly one row, even over zero input rows.
        Group all;
        all.rows.resize(in.row_count());
        std::iota(all.rows.begin(), all.rows.end(), size_t{0});
        groups.push_back(std::move(all));
    } else {
        std::unordered_map<std::string, size_t> index;
        ctx.block = &in;
        for (size_t r = 0; r < in.row_count(); ++r) {
            ctx.row = r;
            std::string key;
            for (const auto& group_expr : sel.group_by) {
                const auto part = field_key(eval_expr(*group_expr, ctx));
                key += std::to_string(part.size()) + ":" + part;
            }
            const auto [it, inserted] = index.try_emplace(key, groups.size());
            if (inserted) {
                groups.emplace_back();
            }
            groups[it->second].rows.push_back(r);
        }
    }

    std::vector<std::vector<core::Field>> values(sel.columns.size());
    ctx.block = &in;
    for (const auto& group : groups) {
        ctx.group_rows = &group.rows;
        ctx.row = group.rows.empty() ? 0 : group.rows.front();
        if (sel.having && !eval_predicate(*sel.having, ctx)) {
            continue;
        }
        for (size_t i = 0; i < sel.columns.size(); ++i) {
            values[i].push_back(eval_expr(*sel.columns[i], ctx));
        }
    }

    core::Block out;
    for (size_t i = 0; i < sel.columns.size(); ++i) {
        out.add_column(select_output_name(*sel.columns[i]), column_from_fields(values[i]));
    }
    return out;
}

// ── ORDER BY / LIMIT ──

// "c.region_id" -> {"c", "region_id"}; a bare name has an empty table part.
auto split_qualified(const std::string& name) -> std::pair<std::string, std::string>
{
    const auto dot = name.find('.');
    if (dot == std::string::npos) {
        return {"", name};
    }
    return {name.substr(0, dot), name.substr(dot + 1)};
}

auto sort_block(core::Block block, const core::Block* source,
                const std::vector<std::pair<std::string, bool>>& order_by,
                const std::unordered_map<std::string, std::string>& alias_to_table) -> core::Block
{
    if (order_by.empty()) return block;

    // A key is looked up in the result first (so aliases win), then in the source rows, which
    // line up one-to-one with the result for queries without aggregation.
    struct Key
    {
        const core::Block* from;
        size_t column;
        bool descending;
    };
    std::vector<Key> keys;
    for (const auto& [name, descending] : order_by) {
        const auto [table, column] = split_qualified(name);
        if (const auto idx = find_column(block, table, column, alias_to_table)) {
            keys.push_back({&block, *idx, descending});
            continue;
        }
        if (source && source->row_count() == block.row_count()) {
            if (const auto sidx = find_column(*source, table, column, alias_to_table)) {
                keys.push_back({source, *sidx, descending});
                continue;
            }
        }
        throw common::Exception{
            "Unknown column in ORDER BY: " + name,
            static_cast<int>(common::ErrorCode::UNKNOWN_COLUMN)};
    }
    if (block.row_count() == 0) return block;

    std::vector<size_t> order(block.row_count());
    std::iota(order.begin(), order.end(), size_t{0});
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        for (const auto& key : keys) {
            const int c = total_compare(key.from->get_row_value(key.column, a),
                                        key.from->get_row_value(key.column, b));
            if (c != 0) {
                return key.descending ? c > 0 : c < 0;
            }
        }
        return false;
    });
    core::Block out;
    for (const auto& name : block.column_names()) {
        out.add_column(name, block.get_column(name)->clone_empty());
    }
    for (auto r : order) {
        for (const auto& name : block.column_names()) {
            out.get_column(name)->insert(block.get_column(name)->get(r));
        }
    }
    return out;
}

auto limit_block(core::Block block, size_t offset, size_t count) -> core::Block
{
    const size_t rows = block.row_count();
    const size_t begin = std::min(offset, rows);
    const size_t end = count > rows - begin ? rows : begin + count;
    if (begin == 0 && end == rows) return block;
    core::Block out;
    for (const auto& name : block.column_names()) {
        auto col = block.get_column(name);
        auto new_col = col->clone_empty();
        for (size_t r = begin; r < end; ++r) {
            new_col->insert(col->get(r));
        }
        out.add_column(name, new_col);
    }
    return out;
}

// ── Window functions ──

// Bucket number (1-indexed) for a 0-indexed rank among partition_size rows split into n tiles.
auto ntile_bucket(size_t rank, size_t partition_size, int64_t n) -> int64_t {
    if (n <= 0 || partition_size == 0) return 0;
    const auto buckets = static_cast<size_t>(n);
    const size_t base = partition_size / buckets;
    const size_t remainder = partition_size % buckets;
    const size_t larger_rows = remainder * (base + 1);
    if (rank < larger_rows) {
        return static_cast<int64_t>(rank / (base + 1)) + 1;
    }
    const size_t rem_rank = rank - larger_rows;
    return static_cast<int64_t>(remainder + rem_rank / std::max<size_t>(base, 1)) + 1;
}

auto partition_rows(const core::Block& in, const std::vector<std::shared_ptr<ASTExpr>>& partition_by,
                    EvalCtx& ctx) -> std::vector<std::vector<size_t>>
{
    std::vector<std::vector<size_t>> partitions;
    std::unordered_map<std::string, size_t> index;
    ctx.block = &in;
    for (size_t r = 0; r < in.row_count(); ++r) {
        ctx.row = r;
        std::string key;
        for (const auto& part : partition_by) {
            const auto piece = field_key(eval_expr(*part, ctx));
            key += std::to_string(piece.size()) + ":" + piece;
        }
        const auto [it, inserted] = index.try_emplace(key, partitions.size());
        if (inserted) {
            partitions.emplace_back();
        }
        partitions[it->second].push_back(r);
    }
    return partitions;
}

auto sort_partition(const core::Block& in, std::vector<size_t>& rows,
                    const std::vector<std::pair<std::string, bool>>& order_by,
                    const std::unordered_map<std::string, std::string>& alias_to_table) -> void
{
    std::vector<std::pair<size_t, bool>> keys;
    for (const auto& [name, descending] : order_by) {
        const auto [table, column] = split_qualified(name);
        const auto idx = find_column(in, table, column, alias_to_table);
        if (!idx) {
            throw common::Exception{
                "Unknown column in window ORDER BY: " + name,
                static_cast<int>(common::ErrorCode::UNKNOWN_COLUMN)};
        }
        keys.emplace_back(*idx, descending);
    }
    std::stable_sort(rows.begin(), rows.end(), [&](size_t a, size_t b) {
        for (const auto& [column, descending] : keys) {
            const int c = total_compare(in.get_row_value(column, a), in.get_row_value(column, b));
            if (c != 0) {
                return descending ? c > 0 : c < 0;
            }
        }
        return false;
    });
}

auto apply_window(const core::Block& in, const std::vector<std::shared_ptr<ASTExpr>>& cols,
                  EvalCtx ctx) -> core::Block
{
    core::Block out;
    ctx.block = &in;
    for (const auto& item : cols) {
        const auto* func = as_window_function(item.get());
        if (!func) {
            append_item_columns(out, in, *item, ctx);
            continue;
        }

        const auto upper = to_upper(func->name);
        const bool is_ntile = upper == "NTILE";
        if (!is_ntile && !is_aggregate_name(upper)) {
            throw common::Exception{
                "Unknown window function: " + func->name,
                static_cast<int>(common::ErrorCode::UNKNOWN_FUNCTION)};
        }
        int64_t tiles = 0;
        if (is_ntile) {
            const auto n = func->args.empty() ? std::nullopt : literal_arg_as_double(func->args.front().get());
            tiles = n ? static_cast<int64_t>(*n) : int64_t{0};
            if (tiles <= 0) {
                throw common::Exception{
                    "NTILE needs a positive integer number of buckets",
                    static_cast<int>(common::ErrorCode::ILLEGAL_TYPE)};
            }
        }

        std::vector<core::Field> values(in.row_count());
        for (auto& rows : partition_rows(in, func->window->partition_by, ctx)) {
            sort_partition(in, rows, func->window->order_by, ctx.alias_to_table);
            if (is_ntile) {
                for (size_t rank = 0; rank < rows.size(); ++rank) {
                    values[rows[rank]] = core::Field{ntile_bucket(rank, rows.size(), tiles)};
                }
                continue;
            }
            EvalCtx partition_ctx = ctx;
            partition_ctx.group_rows = &rows;
            partition_ctx.row = rows.front();
            const auto value = compute_aggregate(*func, partition_ctx);
            for (const auto row : rows) {
                values[row] = value;
            }
        }

        const auto* alias = dynamic_cast<const ASTAlias*>(item.get());
        out.add_column(alias ? alias->alias : func->to_string() + "_over", column_from_fields(values));
    }
    return out;
}

auto eval_select_with_expanding(Context& context, const QueryAST& query,
                                std::unordered_set<std::string>& expanding) -> core::Block {
    const auto& sel = query.select;
    EvalCtx ctx{context};
    ctx.expanding = &expanding;

    std::unordered_map<const ASTSubQueryExpr*, std::shared_ptr<core::Block>> subquery_cache;
    ctx.subquery_cache = &subquery_cache;

    std::unordered_map<std::string, const ASTExpr*> select_aliases;
    for (const auto& item : sel.columns) {
        if (auto* alias = dynamic_cast<const ASTAlias*>(item.get())) {
            select_aliases[alias->alias] = alias->expression.get();
        }
    }
    ctx.select_aliases = &select_aliases;

    if (!sel.table.empty()) {
        const std::string alias = sel.table_alias.empty() ? sel.table : sel.table_alias;
        ctx.alias_to_table[alias] = sel.table;
        if (!sel.table_alias.empty()) {
            ctx.alias_to_table[sel.table_alias] = sel.table;
        }
    }
    for (const auto& join : sel.joins) {
        const std::string alias = join.alias.empty() ? join.table : join.alias;
        ctx.alias_to_table[alias] = join.table;
    }

    const bool has_window = std::any_of(sel.columns.begin(), sel.columns.end(),
        [](const auto& item) { return as_window_function(item.get()) != nullptr; });
    const bool is_aggregate_query = !sel.group_by.empty() || sel.having != nullptr ||
        std::any_of(sel.columns.begin(), sel.columns.end(),
                    [](const auto& item) { return contains_aggregate(*item); });
    if (has_window && is_aggregate_query) {
        throw common::Exception{
            "Window functions cannot be combined with GROUP BY or aggregate functions yet",
            static_cast<int>(common::ErrorCode::NOT_IMPLEMENTED)};
    }

    core::Block data;
    if (sel.from_connector) {
        auto& mgr = connectors::ConnectorManager::instance();
        const auto* entry = mgr.get_connector(sel.connector_name);
        if (!entry) {
            throw common::Exception{
                "Unknown connector: " + sel.connector_name,
                static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
        }
        auto driver = connectors::ConnectorFactory::create_driver(entry->type);
        // Only stop the remote read early when nothing downstream can drop or reorder rows.
        const bool can_push_limit = sel.has_limit && !sel.where && sel.order_by.empty() &&
                                    !is_aggregate_query && !has_window;
        const size_t read_limit = can_push_limit ? sel.limit.first + sel.limit.second : 0;
        data = driver->read(*entry, sel.connector_resource, read_limit);
    } else if (!sel.table.empty()) {
        const std::string prefix = sel.table_alias.empty() ? sel.table : sel.table_alias;
        data = load_table(context, sel.table, prefix, expanding);
    }
    for (const auto& join : sel.joins) {
        const std::string prefix = join.alias.empty() ? join.table : join.alias;
        auto right = load_table(context, join.table, prefix, expanding);
        if (join.on) {
            data = join_blocks(data, right, *join.on, ctx);
        }
    }

    data = filter_block(data, sel.where.get(), ctx);

    core::Block result;
    const core::Block* order_source = nullptr;
    if (has_window) {
        result = apply_window(data, sel.columns, ctx);
        order_source = &data;
    } else if (is_aggregate_query) {
        result = aggregate_block(data, sel, ctx);
    } else {
        result = project_block(data, sel.columns, ctx);
        order_source = &data;
    }

    result = sort_block(std::move(result), order_source, sel.order_by, ctx.alias_to_table);
    if (sel.has_limit) {
        result = limit_block(std::move(result), sel.limit.first, sel.limit.second);
    }
    return result;
}

} // namespace

auto InterpreterSelectQuery::execute(Context& context, const QueryAST& query) -> core::Block {
    if (query.query_type != QueryAST::QueryType::SELECT) {
        throw common::Exception{
            "InterpreterSelectQuery: expected SELECT",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    std::unordered_set<std::string> expanding;
    return eval_select_with_expanding(context, query, expanding);
}

} // namespace mnemo::interpreters
