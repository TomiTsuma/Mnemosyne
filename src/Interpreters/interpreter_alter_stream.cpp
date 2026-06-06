// src/Interpreters/interpreter_alter_stream.cpp

#include "Interpreters/interpreter_alter_stream.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Streaming/stream_manager.h"
#include "Common/exceptions.h"

namespace mnemo::interpreters {

auto InterpreterAlterStream::execute(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    (void)context;
    std::optional<uint32_t> retention_days;
    std::optional<bool> retention_forever;
    for (const auto& set : query.alter.stream_sets) {
        if (set.property == "RETENTION" || set.property == "retention") {
            if (set.value == "FOREVER" || set.value == "forever") {
                retention_forever = true;
            } else {
                const auto space = set.value.find(' ');
                const auto days_str = space == std::string::npos
                    ? set.value
                    : set.value.substr(0, space);
                retention_days = static_cast<uint32_t>(std::stoul(days_str));
            }
        } else {
            throw common::Exception{
                "Unknown ALTER STREAM property: " + set.property,
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
    }
    streaming::StreamManager::instance().alter_stream(
        query.alter.table, retention_days, retention_forever);
    return ddl_utils::make_ok_block();
}

} // namespace mnemo::interpreters
