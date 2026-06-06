// src/Interpreters/interpreter_alter_shard_group.cpp

#include "Interpreters/interpreter_alter_shard_group.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "ShardGroups/shard_group_manager.h"
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

auto InterpreterAlterShardGroup::execute(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    (void)context;
    auto& mgr = shard_groups::ShardGroupManager::instance();
    std::optional<uint32_t> shards;

    for (const auto& cmd : query.alter.shard_group_sets) {
        const auto prop = upper(cmd.property);
        if (prop == "SHARDS") {
            shards = static_cast<uint32_t>(std::stoul(cmd.value));
        } else {
            throw common::Exception{
                "Unknown ALTER SHARD_GROUP property: " + cmd.property,
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
    }

    mgr.alter_group(query.alter.table, shards);
    return ddl_utils::make_ok_block();
}

} // namespace mnemo::interpreters
