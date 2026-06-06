// src/Interpreters/interpreter_refresh_query.cpp — REFRESH MATERIALIZED VIEW

#include "Interpreters/interpreter_refresh_query.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Interpreters/interpreter_select_query.h"
#include "Storages/memory_storage.h"
#include "Common/exceptions.h"

namespace mnemo::interpreters {

auto InterpreterRefreshQuery::execute(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    auto db = ddl_utils::require_catalog(context);
    const auto& name = query.refresh.name;

    auto entry = db->get_materialized_view(name);
    if (!entry) {
        throw common::Exception{
            "Unknown materialized view: " + name,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    auto storage = ddl_utils::resolve_storage(context, name);
    if (!storage) {
        throw common::Exception{
            "Materialized view storage missing: " + name,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    parsers::QueryAST select_query;
    select_query.query_type = parsers::QueryAST::QueryType::SELECT;
    select_query.select = entry->definition;
    auto result = InterpreterSelectQuery::execute(context, select_query);

    if (auto mem = std::dynamic_pointer_cast<storages::MemoryStorage>(storage)) {
        mem->load_block(result);
    } else {
        storage->alter([](storages::IStorage& s) {
            if (auto* mem = dynamic_cast<storages::MemoryStorage*>(&s)) {
                mem->truncate();
            }
        });
        if (!storage->write(result)) {
            throw common::Exception{
                "Failed to refresh materialized view: " + name,
                static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
        }
    }

    return ddl_utils::make_ok_block();
}

} // namespace mnemo::interpreters
