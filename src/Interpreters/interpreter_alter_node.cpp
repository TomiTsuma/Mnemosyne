// src/Interpreters/interpreter_alter_node.cpp

#include "Interpreters/interpreter_alter_node.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Nodes/node_catalog.h"
#include "Nodes/node_manager.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnemo::interpreters {

namespace {

auto upper(std::string s) -> std::string {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

} // namespace

auto InterpreterAlterNode::execute(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    (void)context;
    auto& mgr = nodes::NodeManager::instance();
    std::optional<nodes::NodeRole> role;
    std::optional<nodes::NodeType> type;
    std::optional<nodes::NodeStatus> status;
    std::optional<std::string_view> cluster_id;

    for (const auto& cmd : query.alter.node_sets) {
        const auto prop = upper(cmd.property);
        if (prop == "ROLE") {
            role = nodes::parse_node_role(cmd.value);
        } else if (prop == "TYPE") {
            type = nodes::parse_node_type(cmd.value);
        } else if (prop == "STATUS") {
            status = nodes::parse_node_status(cmd.value);
        } else if (prop == "CLUSTER") {
            cluster_id = cmd.value;
            if (!mgr.get_cluster(cmd.value)) {
                nodes::ClusterEntry cluster;
                cluster.name = cmd.value;
                mgr.create_cluster(cluster, true);
            }
        } else {
            throw common::Exception{
                "Unknown ALTER NODE property: " + cmd.property,
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
    }

    mgr.alter_node(query.alter.table, role, type, status, cluster_id);
    return ddl_utils::make_ok_block();
}

} // namespace mnemo::interpreters
