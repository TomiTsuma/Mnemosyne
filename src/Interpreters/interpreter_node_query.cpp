// src/Interpreters/interpreter_node_query.cpp

#include "Interpreters/interpreter_node_query.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Nodes/node_manager.h"
#include "Common/exceptions.h"

namespace mnemo::interpreters {

auto InterpreterNodeQuery::execute_register(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    (void)context;
    nodes::NodeManager::instance().register_node(
        query.register_node.node_name,
        query.register_node.host,
        query.register_node.port);
    return ddl_utils::make_ok_block();
}

auto InterpreterNodeQuery::execute_drain(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    (void)context;
    nodes::NodeManager::instance().drain_node(query.drain_node.node_name);
    return ddl_utils::make_ok_block();
}

auto InterpreterNodeQuery::execute_remove(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    (void)context;
    nodes::NodeManager::instance().remove_node(
        query.remove_node.node_name,
        query.remove_node.if_exists);
    return ddl_utils::make_ok_block();
}

} // namespace mnemo::interpreters
