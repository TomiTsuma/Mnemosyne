// src/Interpreters/interpreter_alter_connector.cpp

#include "Interpreters/interpreter_alter_connector.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Connectors/connector_manager.h"

namespace mnemo::interpreters {

auto InterpreterAlterConnector::execute(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    (void)context;
    std::vector<std::pair<std::string, std::string>> sets;
    sets.reserve(query.alter.connector_sets.size());
    for (const auto& cmd : query.alter.connector_sets) {
        sets.emplace_back(cmd.property, cmd.value);
    }
    connectors::ConnectorManager::instance().alter_connector(query.alter.table, sets);
    return ddl_utils::make_ok_block();
}

} // namespace mnemo::interpreters
