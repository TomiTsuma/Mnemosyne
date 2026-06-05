// src/Planner/execution_plan.h — PlannerNode DAG structure
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <variant>
#include <optional>
#include <cstdint>
#include <functional>
#include "DataTypes/data_type.h"

namespace mnemo::planner {

// ── Operator types for execution plan nodes ──
enum class PlanNodeType : uint8_t {
    Source,      // Read from storage
    Filter,      // Predicate pushdown
    Project,     // Column projection
    Join,        // Join operation
    Aggregate,   // Aggregation
    Sort,        // Sorting
    Limit,       // Top-N / offset-limit
    Union,       // UNION ALL / UNION DISTINCT
    Window,      // Window function
    HashTable,   // Hash table build / probe
    Exchange,    // Data exchange (for distributed)
};

// ── JoinType — kind of join ──
enum class JoinType : uint8_t {
    Inner, Left, Right, Full, Cross, Semi, Anti,
};

// ── PlanNode — single node in the execution DAG ──
struct PlanNode {
    // ── Node type enum (used by the interpreter switch) ──
    enum class Type : uint8_t {
        SCAN,       // table scan
        FILTER,     // WHERE predicate
        PROJECT,    // SELECT columns
        GROUP_BY,   // GROUP BY aggregation
        SORT,       // ORDER BY
        LIMIT,      // LIMIT / OFFSET
        INSERT,     // INSERT INTO
        CREATE,     // CREATE TABLE
        DROP,       // DROP TABLE
        SHOW,       // SHOW TABLES / DATABASES
        DESCRIBE,   // DESCRIBE TABLE
        EXPLAIN,    // EXPLAIN PLAN
        USE,        // USE DATABASE (sets the session's current database)
    };

    PlanNodeType  type;
    Type          node_type;
    std::string   name;           // descriptive label
    std::shared_ptr<PlanNode> child;   // single parent pointer (tree building)
    std::vector<std::shared_ptr<PlanNode>> children;

    // ── Fields accessed by the interpreter ──
    std::string   table;              // table name (SCAN, INSERT)
    std::string   table_name;         // table name (CREATE, DROP, DESCRIBE)
    std::string   expression;         // WHERE expression (FILTER)
    std::vector<std::string> columns; // column names (PROJECT, INSERT, CREATE)
    std::vector<std::string> order_by;// ORDER BY columns
    size_t        offset = 0;         // OFFSET (LIMIT)
    size_t        limit = 0;          // LIMIT (LIMIT)
    std::string   show_type;          // what to show (SHOW)
    std::string   explain_plan;       // plan description (EXPLAIN)
    std::vector<std::vector<std::string>> values;  // row data (INSERT)

    // Per-operator metadata
    struct FilterSpec {
        datatypes::DataTypePtr column_type;
        bool                   nullable;
    };

    struct JoinSpec {
        JoinType    join_type;
        bool        is_outer;
        std::string on_condition;
        // Build side column names (left) and probe side (right)
        std::vector<std::string> build_keys;
        std::vector<std::string> probe_keys;
    };

    struct AggregateSpec {
        struct AggregateOp {
            std::string function_name;
            std::vector<std::string> input_columns;
            datatypes::DataTypePtr   result_type;
            bool is_distinct;
        };
        std::vector<AggregateOp> aggregates;
        std::vector<std::string> group_by_columns;
    };

    std::variant<std::monostate, FilterSpec, JoinSpec, AggregateSpec> spec;
    double                             cost_estimate = 0.0;
    size_t                             output_rows_estimate = 0;
    std::vector<datatypes::DataTypePtr> output_types;

    PlanNode() = default;
    explicit PlanNode(Type t) : type(PlanNodeType::Source), node_type(t) {}
};

// ── ExecutionPlan — the full plan as a DAG ──
class ExecutionPlan {
public:
    // Root node of the plan DAG
    std::shared_ptr<PlanNode> root;

    // Metadata
    uint64_t         plan_id;
    std::string      query_id;
    std::string      description;
    std::vector<std::shared_ptr<PlanNode>> all_nodes;

    // Build the plan
    ExecutionPlan(uint64_t id, const std::string& query_id);

    // Walk the plan — depth-first
    void walk(std::function<void(std::shared_ptr<PlanNode>)> visitor);

    // Serialize plan to string (for EXPLAIN)
    [[nodiscard]] auto explain() const -> std::string;
};

} // namespace mnemo::planner
