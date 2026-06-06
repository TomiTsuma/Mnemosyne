// src/Interpreters/interpreter_alter_replica_group.cpp

#include "Interpreters/interpreter_alter_replica_group.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "ReplicaGroups/replica_group_catalog.h"
#include "ReplicaGroups/replica_group_manager.h"
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

auto InterpreterAlterReplicaGroup::execute(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    (void)context;
    auto& mgr = replica_groups::ReplicaGroupManager::instance();
    std::optional<uint32_t> replicas;
    std::optional<replica_groups::ConsistencyMode> consistency;

    for (const auto& cmd : query.alter.replica_group_sets) {
        const auto prop = upper(cmd.property);
        if (prop == "REPLICAS") {
            replicas = static_cast<uint32_t>(std::stoul(cmd.value));
        } else if (prop == "CONSISTENCY") {
            consistency = replica_groups::parse_consistency_mode(cmd.value);
        } else {
            throw common::Exception{
                "Unknown ALTER REPLICA_GROUP property: " + cmd.property,
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
    }

    mgr.alter_group(query.alter.table, replicas, consistency);
    return ddl_utils::make_ok_block();
}

} // namespace mnemo::interpreters
