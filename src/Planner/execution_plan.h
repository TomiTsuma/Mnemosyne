// src/Planner/execution_plan.h — PlannerNode DAG structure
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <variant>
#include <optional>
#include <cstdint>
#include "data_types/data_type.h"

namespace mnesso::planner {

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
    PlanNodeType  type;
    std::string   name;           // descriptive label
    std::vector<std::shared_ptr<PlanNode>> children;

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
    explicit PlanNode(PlanNodeType t) : type(t) {}
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

} // namespace mnesso::planner
