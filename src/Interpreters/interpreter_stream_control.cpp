// src/Interpreters/interpreter_stream_control.cpp

#include "Interpreters/interpreter_stream_control.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Streaming/stream_manager.h"
#include "Columns/column_string.h"
#include "Common/exceptions.h"

namespace mnemo::interpreters {

namespace {

auto strip_quotes(std::string value) -> std::string {
    if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

auto extract_payloads(const std::vector<std::vector<std::string>>& values)
    -> std::vector<std::string> {
    std::vector<std::string> payloads;
    payloads.reserve(values.size());
    for (const auto& row : values) {
        if (row.empty()) {
            throw common::Exception{
                "PUBLISH requires at least one value column",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        payloads.push_back(strip_quotes(row.front()));
    }
    return payloads;
}

} // namespace

auto InterpreterStreamControl::execute_publish(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    (void)context;
    auto& mgr = streaming::StreamManager::instance();
    const auto payloads = extract_payloads(query.stream_control.values);
    if (payloads.empty()) {
        throw common::Exception{
            "PUBLISH requires VALUES clause",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    uint64_t last_offset = 0;
    for (const auto& payload : payloads) {
        last_offset = mgr.publish_to_topic(query.stream_control.topic_name, payload);
    }

    auto offset_col = std::make_shared<columns::ColumnString>();
    offset_col->insert(core::Field(std::to_string(last_offset)));
    core::Block block;
    block.add_column("offset", offset_col);
    return block;
}

auto InterpreterStreamControl::execute_subscribe(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    (void)context;
    auto& mgr = streaming::StreamManager::instance();
    std::optional<std::string_view> group;
    if (!query.stream_control.consumer_group_name.empty()) {
        group = query.stream_control.consumer_group_name;
    }
    const auto result = mgr.subscribe(
        query.stream_control.stream_name,
        group,
        query.stream_control.limit,
        group.has_value());

    auto offset_col = std::make_shared<columns::ColumnString>();
    auto payload_col = std::make_shared<columns::ColumnString>();
    for (const auto& event : result.events) {
        offset_col->insert(core::Field(std::to_string(event.offset)));
        payload_col->insert(core::Field(event.payload));
    }

    core::Block block;
    block.add_column("offset", offset_col);
    block.add_column("payload", payload_col);
    return block;
}

} // namespace mnemo::interpreters
