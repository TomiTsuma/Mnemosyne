// src/Planner/execution_plan.cpp — ExecutionPlan implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "Planner/execution_plan.h"
#include <sstream>
#include <algorithm>

namespace mnemo::planner {

ExecutionPlan::ExecutionPlan(uint64_t id, const std::string& query_id)
    : plan_id(id), query_id(query_id) {}

void ExecutionPlan::walk(std::function<void(std::shared_ptr<PlanNode>)> visitor) {
    if (!root) return;

    // Depth-first walk
    std::function<void(std::shared_ptr<PlanNode>)> dfs;
    dfs = [&](std::shared_ptr<PlanNode> node) {
        visitor(node);
        all_nodes.push_back(node);
        for (auto& child : node->children) {
            dfs(child);
        }
    };

    dfs(root);
}

auto ExecutionPlan::explain() const -> std::string {
    std::ostringstream oss;
    oss << "Execution Plan (id=" << plan_id << "):\n";

    if (!root) {
        oss << "  (empty plan)\n";
        return oss.str();
    }

    // Format each node with indentation
    std::function<void(std::shared_ptr<PlanNode>, int indent)> format_node;
    format_node = [&](std::shared_ptr<PlanNode> node, int indent) {
        std::string prefix(indent, ' ');

        oss << prefix;
        switch (node->type) {
            case PlanNodeType::Source:   oss << "[Source]"; break;
            case PlanNodeType::Filter:   oss << "[Filter]"; break;
            case PlanNodeType::Project:  oss << "[Project]"; break;
            case PlanNodeType::Join:     oss << "[Join]"; break;
            case PlanNodeType::Aggregate: oss << "[Aggregate]"; break;
            case PlanNodeType::Sort:     oss << "[Sort]"; break;
            case PlanNodeType::Limit:    oss << "[Limit]"; break;
            case PlanNodeType::Union:    oss << "[Union]"; break;
            case PlanNodeType::Window:   oss << "[Window]"; break;
            case PlanNodeType::HashTable: oss << "[HashTable]"; break;
            case PlanNodeType::Exchange: oss << "[Exchange]"; break;
        }

        if (!node->name.empty()) {
            oss << " " << node->name;
        }

        if (node->cost_estimate > 0) {
            oss << " (cost=" << node->cost_estimate << ")";
        }
        if (node->output_rows_estimate > 0) {
            oss << " (rows=" << node->output_rows_estimate << ")";
        }

        oss << "\n";

        for (auto& child : node->children) {
            format_node(child, indent + 2);
        }
    };

    format_node(root, 2);
    return oss.str();
}

} // namespace mnemo::planner
