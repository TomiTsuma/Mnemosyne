// src/Interpreters/interpreter_discover_query.cpp

#include "Interpreters/interpreter_discover_query.h"
#include "Connectors/connector_factory.h"
#include "Connectors/connector_manager.h"
#include "Columns/column_string.h"
#include "Common/exceptions.h"

namespace mnemo::interpreters {

auto InterpreterDiscoverQuery::execute(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    (void)context;
    auto& mgr = connectors::ConnectorManager::instance();
    const auto* entry = mgr.get_connector(query.discover.connector_name);
    if (!entry) {
        throw common::Exception{
            "Unknown connector: " + query.discover.connector_name,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    auto driver = connectors::ConnectorFactory::create_driver(entry->type);
    const auto resources = driver->discover_schema(*entry);

    auto resource_col = std::make_shared<columns::ColumnString>();
    for (size_t i = 0; i < resources.size(); ++i) {
        resource_col->insert_at(i, core::Field(resources[i]));
    }
    core::Block block;
    block.add_column("resource", resource_col);
    return block;
}

} // namespace mnemo::interpreters
