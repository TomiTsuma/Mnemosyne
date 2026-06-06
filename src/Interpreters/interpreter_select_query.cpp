// src/Interpreters/interpreter_select_query.cpp — AST-driven SELECT execution

#include "Interpreters/interpreter_select_query.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Databases/database.h"
#include "Columns/column_string.h"
#include "Columns/column_vector.h"
#include "Common/exceptions.h"
#include "DataTypes/data_type_factory.h"
#include "Storages/i_storage.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
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
using parsers::QueryAST;

struct EvalCtx {
    Context& context;
    const core::Block* block = nullptr;
    size_t row = 0;
    std::unordered_map<std::string, std::string> alias_to_table;
    std::unordered_set<std::string>* expanding = nullptr;
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

auto field_to_int64(const core::Field& f) -> int64_t {
  return std::visit([](auto&& v) -> int64_t {
    using T = std::decay_t<decltype(v)>;
    if constexpr (std::is_integral_v<T>) return static_cast<int64_t>(v);
    if constexpr (std::is_floating_point_v<T>) return static_cast<int64_t>(v);
    if constexpr (std::is_same_v<T, bool>) return v ? 1 : 0;
    if constexpr (std::is_same_v<T, std::string>) {
      try { return std::stoll(v); } catch (...) { return 0; }
    }
    return 0;
  }, f.variant());
}

auto field_to_double(const core::Field& f) -> double {
  return std::visit([](auto&& v) -> double {
    using T = std::decay_t<decltype(v)>;
    if constexpr (std::is_floating_point_v<T>) return v;
    if constexpr (std::is_integral_v<T>) return static_cast<double>(v);
    if constexpr (std::is_same_v<T, bool>) return v ? 1.0 : 0.0;
    if constexpr (std::is_same_v<T, std::string>) {
      try { return std::stod(v); } catch (...) { return 0.0; }
    }
    return 0.0;
  }, f.variant());
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

auto eval_expr(const ASTExpr& expr, EvalCtx& ctx) -> core::Field;
auto aggregate_name(const ASTFunction& func) -> std::string;

auto eval_select_with_expanding(Context& context, const QueryAST& query,
                                std::unordered_set<std::string>& expanding) -> core::Block;

auto eval_expr(const ASTExpr& expr, EvalCtx& ctx) -> core::Field {
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
        if (!ctx.block) return core::Field{};
        auto idx = find_column(*ctx.block, col->table, col->column, ctx.alias_to_table);
        if (!idx) return core::Field{};
        return ctx.block->get_row_value(*idx, ctx.row);
    }
    if (auto* bin = dynamic_cast<const ASTBinaryOp*>(&expr)) {
        if (bin->op == ASTBinaryOp::Op::In) {
            return core::Field{false};
        }
        auto l = eval_expr(*bin->left, ctx);
        auto r = eval_expr(*bin->right, ctx);
        switch (bin->op) {
            case ASTBinaryOp::Op::Eq: return core::Field{field_to_int64(l) == field_to_int64(r)};
            case ASTBinaryOp::Op::Ne: return core::Field{field_to_int64(l) != field_to_int64(r)};
            case ASTBinaryOp::Op::Gt: return core::Field{field_to_double(l) > field_to_double(r)};
            case ASTBinaryOp::Op::Lt: return core::Field{field_to_double(l) < field_to_double(r)};
            case ASTBinaryOp::Op::Ge: return core::Field{field_to_double(l) >= field_to_double(r)};
            case ASTBinaryOp::Op::Le: return core::Field{field_to_double(l) <= field_to_double(r)};
            case ASTBinaryOp::Op::And: return core::Field{
                field_to_int64(l) != 0 && field_to_int64(r) != 0};
            case ASTBinaryOp::Op::Or: return core::Field{
                field_to_int64(l) != 0 || field_to_int64(r) != 0};
            default: break;
        }
    }
    if (auto* sub = dynamic_cast<const ASTSubQueryExpr*>(&expr)) {
        if (!sub->query) return core::Field{};
        std::unordered_set<std::string> local_expanding;
        auto& expanding = ctx.expanding ? *ctx.expanding : local_expanding;
        auto block = eval_select_with_expanding(ctx.context, *sub->query, expanding);
        if (block.row_count() == 0 || block.column_count() == 0) return core::Field{int64_t{0}};
        return block.get_row_value(0, 0);
    }
    if (auto* func = dynamic_cast<const ASTFunction*>(&expr)) {
        if (ctx.block) {
            const auto col_name = aggregate_name(*func);
            const auto names = ctx.block->column_names();
            for (size_t c = 0; c < names.size(); ++c) {
                if (names[c] == col_name) {
                    return ctx.block->get_row_value(c, ctx.row);
                }
            }
        }
    }
    return core::Field{};
}

auto eval_predicate(const ASTExpr& expr, EvalCtx& ctx) -> bool {
    if (auto* bin = dynamic_cast<const ASTBinaryOp*>(&expr)) {
        if (bin->op == ASTBinaryOp::Op::In) {
            auto left_val = eval_expr(*bin->left, ctx);
            if (auto* sub = dynamic_cast<const ASTSubQueryExpr*>(bin->right.get())) {
                if (!sub->query) return false;
                std::unordered_set<std::string> local_expanding;
                auto& expanding = ctx.expanding ? *ctx.expanding : local_expanding;
                auto sub_block = eval_select_with_expanding(ctx.context, *sub->query, expanding);
                if (sub_block.column_count() == 0) return false;
                for (size_t r = 0; r < sub_block.row_count(); ++r) {
                    auto v = sub_block.get_row_value(0, r);
                    if (field_to_int64(v) == field_to_int64(left_val)) return true;
                    if (std::visit([](auto&& a, auto&& b) {
                        using A = std::decay_t<decltype(a)>;
                        using B = std::decay_t<decltype(b)>;
                        if constexpr (std::is_same_v<A, std::string> && std::is_same_v<B, std::string>) {
                            return a == b;
                        }
                        return false;
                    }, v.variant(), left_val.variant())) {
                        return true;
                    }
                }
                return false;
            }
        }
    }
    return field_to_int64(eval_expr(expr, ctx)) != 0;
}

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

auto is_aggregate_expr(const ASTExpr& expr) -> bool {
    if (auto* func = dynamic_cast<const ASTFunction*>(&expr)) {
        std::string upper = func->name;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        return upper == "COUNT" || upper == "SUM" || upper == "AVG" ||
               upper == "MIN" || upper == "MAX";
    }
    return false;
}

auto aggregate_name(const ASTFunction& func) -> std::string {
    std::string upper = func.name;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    if (upper == "COUNT" && !func.args.empty()) {
        if (auto* col = dynamic_cast<const ASTColumnRef*>(func.args[0].get())) {
            if (col->column == "*") return "COUNT(*)";
        }
    }
    return func.name + "(" + (func.args.empty() ? "" : func.args[0]->to_string()) + ")";
}

auto compute_aggregate(const ASTFunction& func, const core::Block& block,
                       const std::vector<size_t>& rows,
                       const std::unordered_map<std::string, std::string>& alias_to_table)
    -> core::Field {
    std::string upper = func.name;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    if (upper == "COUNT") {
        if (!func.args.empty()) {
            if (auto* col = dynamic_cast<const ASTColumnRef*>(func.args[0].get())) {
                if (col->column == "*") {
                    return core::Field{static_cast<int64_t>(rows.empty() ? block.row_count() : rows.size())};
                }
            }
        }
        return core::Field{static_cast<int64_t>(rows.empty() ? block.row_count() : rows.size())};
    }
    if (func.args.empty()) return core::Field{int64_t{0}};
    auto* col_ref = dynamic_cast<const ASTColumnRef*>(func.args[0].get());
    if (!col_ref) return core::Field{int64_t{0}};
    auto idx = find_column(block, col_ref->table, col_ref->column, alias_to_table);
    if (!idx) return core::Field{int64_t{0}};
    std::vector<double> vals;
    auto row_list = rows.empty() ? std::vector<size_t>{} : rows;
    if (row_list.empty()) {
        for (size_t r = 0; r < block.row_count(); ++r) row_list.push_back(r);
    }
    for (auto r : row_list) {
        vals.push_back(field_to_double(block.get_row_value(*idx, r)));
    }
    if (vals.empty()) return core::Field{int64_t{0}};
    if (upper == "SUM") {
        double s = std::accumulate(vals.begin(), vals.end(), 0.0);
        return core::Field{s};
    }
    if (upper == "AVG") {
        double s = std::accumulate(vals.begin(), vals.end(), 0.0);
        return core::Field{s / static_cast<double>(vals.size())};
    }
    if (upper == "MIN") return core::Field{*std::min_element(vals.begin(), vals.end())};
    if (upper == "MAX") return core::Field{*std::max_element(vals.begin(), vals.end())};
    return core::Field{int64_t{0}};
}

auto unwrap_expr(const ASTExpr* expr) -> const ASTExpr* {
    if (auto* alias = dynamic_cast<const ASTAlias*>(expr)) {
        return alias->expression.get();
    }
    return expr;
}

auto select_output_name(const ASTExpr& expr) -> std::string {
    if (auto* alias = dynamic_cast<const ASTAlias*>(&expr)) {
        return alias->alias;
    }
    if (auto* func = dynamic_cast<const ASTFunction*>(&expr)) {
        return aggregate_name(*func);
    }
    if (auto* col = dynamic_cast<const ASTColumnRef*>(&expr)) {
        return col->column;
    }
    return "col";
}

auto project_block(const core::Block& in, const std::vector<std::shared_ptr<ASTExpr>>& cols,
                   EvalCtx ctx, bool has_group_by) -> core::Block {
    core::Block out;
    const bool global_agg = !has_group_by &&
        std::all_of(cols.begin(), cols.end(), [](const auto& e) {
            return is_aggregate_expr(*unwrap_expr(e.get()));
        });

    if (global_agg) {
        for (const auto& expr : cols) {
            if (auto* func = dynamic_cast<const ASTFunction*>(unwrap_expr(expr.get()))) {
                auto val = compute_aggregate(*func, in, {}, ctx.alias_to_table);
                auto int_type = datatypes::get_data_type("Int64");
                auto* raw = int_type->create_column();
                auto col = std::shared_ptr<core::IColumn>(
                    static_cast<core::IColumn*>(raw),
                    [](void* p) { delete static_cast<core::IColumn*>(p); });
                col->insert(val);
                out.add_column(aggregate_name(*func), col);
            }
        }
        return out;
    }

    for (const auto& expr : cols) {
        const auto* base = unwrap_expr(expr.get());
        if (auto* col_ref = dynamic_cast<const ASTColumnRef*>(base)) {
            if (col_ref->column == "*") {
                for (const auto& name : in.column_names()) {
                    if (auto col = in.get_column(name)) {
                        out.add_column(name, col->clone());
                    }
                }
                continue;
            }
            auto idx = find_column(in, col_ref->table, col_ref->column, ctx.alias_to_table);
            if (idx) {
                const std::string name = select_output_name(*expr);
                out.add_column(name, in.get_column_by_index(*idx)->clone());
            }
        }
    }
    return out;
}

auto group_and_aggregate(const core::Block& in,
                         const std::vector<std::shared_ptr<ASTExpr>>& group_exprs,
                         const std::vector<std::shared_ptr<ASTExpr>>& select_cols,
                         const ASTExpr* having, EvalCtx ctx) -> core::Block {
    using Key = std::vector<int64_t>;
    std::map<Key, std::vector<size_t>> groups;
    for (size_t r = 0; r < in.row_count(); ++r) {
        Key key;
        ctx.block = &in;
        ctx.row = r;
        for (const auto& ge : group_exprs) {
            key.push_back(field_to_int64(eval_expr(*ge, ctx)));
        }
        groups[key].push_back(r);
    }

    core::Block out;
    for (const auto& ge : group_exprs) {
        if (auto* col = dynamic_cast<const ASTColumnRef*>(ge.get())) {
            auto idx = find_column(in, col->table, col->column, ctx.alias_to_table);
            if (idx) {
                auto type_col = in.get_column_by_index(*idx);
                out.add_column(col->column, type_col->clone_empty());
            }
        }
    }
    std::vector<std::pair<std::string, const ASTFunction*>> agg_outputs;
    for (const auto& expr : select_cols) {
        if (auto* func = dynamic_cast<const ASTFunction*>(unwrap_expr(expr.get()))) {
            auto int_type = datatypes::get_data_type("Int64");
            auto* raw = int_type->create_column();
            auto col = std::shared_ptr<core::IColumn>(
                static_cast<core::IColumn*>(raw),
                [](void* p) { delete static_cast<core::IColumn*>(p); });
            const std::string out_name = select_output_name(*expr);
            out.add_column(out_name, col);
            agg_outputs.emplace_back(out_name, func);
        }
    }

    for (auto& [key, rows] : groups) {
        for (size_t i = 0; i < group_exprs.size(); ++i) {
            if (auto* col = dynamic_cast<const ASTColumnRef*>(group_exprs[i].get())) {
                auto idx = find_column(in, col->table, col->column, ctx.alias_to_table);
                if (idx) {
                    out.get_column(col->column)->insert(in.get_row_value(*idx, rows[0]));
                }
            }
        }
        for (const auto& [out_name, func] : agg_outputs) {
            auto val = compute_aggregate(*func, in, rows, ctx.alias_to_table);
            out.get_column(out_name)->insert(val);
        }
    }

    if (having) {
        core::Block filtered;
        for (const auto& name : out.column_names()) {
            if (auto col = out.get_column(name)) {
                filtered.add_column(name, col->clone_empty());
            }
        }
        for (size_t r = 0; r < out.row_count(); ++r) {
            ctx.block = &out;
            ctx.row = r;
            if (eval_predicate(*having, ctx)) {
                for (const auto& name : out.column_names()) {
                    filtered.get_column(name)->insert(out.get_column(name)->get(r));
                }
            }
        }
        out = std::move(filtered);
    }
    return out;
}

auto sort_block(core::Block block, const std::vector<std::pair<std::string, bool>>& order_by,
                const std::unordered_map<std::string, std::string>& alias_to_table) -> core::Block {
    if (order_by.empty() || block.row_count() == 0) return block;
    std::vector<size_t> order(block.row_count());
    std::iota(order.begin(), order.end(), 0);
    const auto& key = order_by[0];
    auto idx = find_column(block, "", key.first, alias_to_table);
    if (!idx) return block;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        auto fa = field_to_double(block.get_row_value(*idx, a));
        auto fb = field_to_double(block.get_row_value(*idx, b));
        return key.second ? fa > fb : fa < fb;
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

auto limit_block(core::Block block, size_t limit) -> core::Block {
    if (limit == 0 || block.row_count() <= limit) return block;
    core::Block out;
    for (const auto& name : block.column_names()) {
        auto col = block.get_column(name);
        auto new_col = col->clone_empty();
        for (size_t r = 0; r < limit; ++r) {
            new_col->insert(col->get(r));
        }
        out.add_column(name, new_col);
    }
    return out;
}

auto apply_window(const core::Block& in, const std::vector<std::shared_ptr<ASTExpr>>& cols,
                  EvalCtx ctx) -> core::Block {
    core::Block out;
    for (const auto& expr : cols) {
        if (auto* func = dynamic_cast<const ASTFunction*>(expr.get())) {
            if (func->window) continue;
        }
        if (auto* col = dynamic_cast<const ASTColumnRef*>(expr.get())) {
            if (col->column == "*") {
                for (const auto& name : in.column_names()) {
                    out.add_column(name, in.get_column(name)->clone());
                }
            } else {
                auto idx = find_column(in, col->table, col->column, ctx.alias_to_table);
            if (idx) {
                std::string name = col->column.empty() ? in.column_names()[*idx] : col->column;
                out.add_column(name, in.get_column_by_index(*idx)->clone());
            }
            }
        }
    }
    for (const auto& expr : cols) {
        auto* func = dynamic_cast<const ASTFunction*>(expr.get());
        if (!func || !func->window) continue;
        auto num_type = datatypes::get_data_type("Float64");
        auto* raw = num_type->create_column();
        auto result_col = std::shared_ptr<core::IColumn>(
            static_cast<core::IColumn*>(raw),
            [](void* p) { delete static_cast<core::IColumn*>(p); });
        const std::string out_name = aggregate_name(*func) + "_over";
        for (size_t r = 0; r < in.row_count(); ++r) {
            std::vector<size_t> partition_rows;
            for (size_t pr = 0; pr < in.row_count(); ++pr) {
                bool same = true;
                ctx.block = &in;
                for (const auto& part : func->window->partition_by) {
                    ctx.row = r;
                    auto vr = eval_expr(*part, ctx);
                    ctx.row = pr;
                    auto vp = eval_expr(*part, ctx);
                    if (field_to_int64(vr) != field_to_int64(vp)) {
                        same = false;
                        break;
                    }
                }
                if (same) partition_rows.push_back(pr);
            }
            auto val = compute_aggregate(*func, in, partition_rows, ctx.alias_to_table);
            result_col->insert(val);
        }
        out.add_column(out_name, result_col);
    }
    return out;
}

auto eval_select_with_expanding(Context& context, const QueryAST& query,
                                std::unordered_set<std::string>& expanding) -> core::Block {
    const auto& sel = query.select;
    EvalCtx ctx{context};
    ctx.expanding = &expanding;

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

    core::Block data;
    if (!sel.table.empty()) {
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

    const bool has_group = !sel.group_by.empty();
    const bool has_window = std::any_of(sel.columns.begin(), sel.columns.end(), [](const auto& e) {
        if (auto* f = dynamic_cast<const ASTFunction*>(e.get())) return f->window.has_value();
        return false;
    });

    core::Block result;
    if (has_window) {
        result = apply_window(data, sel.columns, ctx);
    } else if (has_group) {
        result = group_and_aggregate(data, sel.group_by, sel.columns, sel.having.get(), ctx);
    } else {
        result = project_block(data, sel.columns, ctx, false);
    }

    result = sort_block(std::move(result), sel.order_by, ctx.alias_to_table);
    if (sel.limit.first > 0) {
        result = limit_block(std::move(result), sel.limit.first);
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
