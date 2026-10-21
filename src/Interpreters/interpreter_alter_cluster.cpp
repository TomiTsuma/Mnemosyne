// src/Interpreters/interpreter_alter_cluster.cpp — ALTER CLUSTER interpreter implementation

#include "Interpreters/interpreter_alter_cluster.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Nodes/node_manager.h"

namespace mnemo::interpreters {

auto InterpreterAlterCluster::execute(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    (void)context;
    const auto& alter = query.alter;
    std::vector<nodes::ClusterMembershipOp> ops;
    ops.reserve(alter.cluster_node_ops.size());
    for (const auto& op : alter.cluster_node_ops) {
        ops.push_back({op.add, op.node});
    }

    // All-or-nothing: a failing step (unknown cluster or node, REMOVE of a non-member, ADD of a
    // node that belongs to another cluster) leaves membership exactly as it was.
    nodes::NodeManager::instance().alter_cluster_membership(alter.table, ops);

    return ddl_utils::make_ok_block();
}

} // namespace mnemo::interpreters
