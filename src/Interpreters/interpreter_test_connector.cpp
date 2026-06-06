// src/Interpreters/interpreter_test_connector.cpp

#include "Interpreters/interpreter_test_connector.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Connectors/connector_factory.h"
#include "Connectors/connector_manager.h"
#include "Columns/column_string.h"
#include "Common/exceptions.h"

namespace mnemo::interpreters {

auto InterpreterTestConnector::execute(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    (void)context;
    auto& mgr = connectors::ConnectorManager::instance();
    auto* entry = mgr.get_connector(query.test_query.name);
    if (!entry) {
        throw common::Exception{
            "Unknown connector: " + query.test_query.name,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    auto driver = connectors::ConnectorFactory::create_driver(entry->type);
    auto result = driver->test(*entry);
    const auto status = result.ok ? connectors::ConnectorStatus::Active
                                  : connectors::ConnectorStatus::Degraded;
    mgr.update_test_result(query.test_query.name, result, status);

    auto name_col = std::make_shared<columns::ColumnString>();
    auto ok_col = std::make_shared<columns::ColumnString>();
    auto latency_col = std::make_shared<columns::ColumnString>();
    auto message_col = std::make_shared<columns::ColumnString>();
    name_col->insert_at(0, core::Field(query.test_query.name));
    ok_col->insert_at(0, core::Field(std::string{result.ok ? "true" : "false"}));
    latency_col->insert_at(0, core::Field(std::to_string(result.latency_ms)));
    message_col->insert_at(0, core::Field(result.message));

    core::Block block;
    block.add_column("name", name_col);
    block.add_column("ok", ok_col);
    block.add_column("latency_ms", latency_col);
    block.add_column("message", message_col);
    return block;
}

} // namespace mnemo::interpreters
