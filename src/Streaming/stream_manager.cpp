// src/Streaming/stream_manager.cpp

#include "Streaming/stream_manager.h"
#include "Common/exceptions.h"
#include <algorithm>

namespace mnemo::streaming {

auto StreamManager::instance() -> StreamManager& {
    static StreamManager inst;
    return inst;
}

auto StreamManager::require_topic(std::string_view name) -> TopicEntry& {
    auto it = topics_.find(std::string{name});
    if (it == topics_.end()) {
        throw common::Exception{
            "Unknown topic: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    return it->second;
}

auto StreamManager::require_topic(std::string_view name) const -> const TopicEntry& {
    auto it = topics_.find(std::string{name});
    if (it == topics_.end()) {
        throw common::Exception{
            "Unknown topic: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    return it->second;
}

auto StreamManager::require_stream(std::string_view name) -> StreamEntry& {
    auto it = streams_.find(std::string{name});
    if (it == streams_.end()) {
        throw common::Exception{
            "Unknown stream: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    return it->second;
}

auto StreamManager::require_stream(std::string_view name) const -> const StreamEntry& {
    auto it = streams_.find(std::string{name});
    if (it == streams_.end()) {
        throw common::Exception{
            "Unknown stream: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    return it->second;
}

auto StreamManager::require_consumer_group(std::string_view name) -> ConsumerGroupEntry& {
    auto it = consumer_groups_.find(std::string{name});
    if (it == consumer_groups_.end()) {
        throw common::Exception{
            "Unknown consumer group: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    return it->second;
}

auto StreamManager::require_consumer_group(std::string_view name) const -> const ConsumerGroupEntry& {
    auto it = consumer_groups_.find(std::string{name});
    if (it == consumer_groups_.end()) {
        throw common::Exception{
            "Unknown consumer group: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    return it->second;
}

auto StreamManager::create_topic(TopicEntry entry, bool if_not_exists) -> void {
    std::lock_guard lock{mutex_};
    validate_topic_entry(entry);
    if (topics_.contains(entry.name)) {
        if (if_not_exists) return;
        throw common::Exception{
            "Topic already exists: " + entry.name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    entry.created_at = std::chrono::system_clock::now();
    topics_.emplace(entry.name, std::move(entry));
}

auto StreamManager::drop_topic(std::string name, bool if_exists) -> void {
    std::lock_guard lock{mutex_};
    auto it = topics_.find(name);
    if (it == topics_.end()) {
        if (if_exists) return;
        throw common::Exception{
            "Unknown topic: " + name,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    for (const auto& [stream_name, stream] : streams_) {
        if (stream.topic_name == name) {
            throw common::Exception{
                "Cannot drop topic " + name + ": stream " + stream_name + " is bound to it",
                static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
        }
    }
    topics_.erase(it);
}

auto StreamManager::has_topic(std::string_view name) const -> bool {
    std::lock_guard lock{mutex_};
    return topics_.contains(std::string{name});
}

auto StreamManager::get_topic(std::string_view name) const -> const TopicEntry* {
    std::lock_guard lock{mutex_};
    auto it = topics_.find(std::string{name});
    return it == topics_.end() ? nullptr : &it->second;
}

auto StreamManager::list_topics() const -> std::vector<TopicEntry> {
    std::lock_guard lock{mutex_};
    std::vector<TopicEntry> out;
    out.reserve(topics_.size());
    for (const auto& [_, entry] : topics_) {
        out.push_back(entry);
    }
    std::ranges::sort(out, [](const auto& a, const auto& b) { return a.name < b.name; });
    return out;
}

auto StreamManager::create_stream(StreamEntry entry, bool if_not_exists) -> void {
    std::lock_guard lock{mutex_};
    validate_stream_entry(entry);
    if (!entry.topic_name.empty() && !topics_.contains(entry.topic_name)) {
        throw common::Exception{
            "Unknown topic: " + entry.topic_name,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    if (streams_.contains(entry.name)) {
        if (if_not_exists) return;
        throw common::Exception{
            "Stream already exists: " + entry.name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    entry.created_at = std::chrono::system_clock::now();
    entry.status = StreamStatus::Active;
    const auto stream_name = entry.name;
    streams_.emplace(stream_name, std::move(entry));
    event_logs_[stream_name] = {};
}

auto StreamManager::drop_stream(std::string name, bool if_exists) -> void {
    std::lock_guard lock{mutex_};
    auto it = streams_.find(name);
    if (it == streams_.end()) {
        if (if_exists) return;
        throw common::Exception{
            "Unknown stream: " + name,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    streams_.erase(it);
    event_logs_.erase(name);
    for (auto& [_, group] : consumer_groups_) {
        group.offsets.erase(name);
    }
}

auto StreamManager::alter_stream(std::string_view name, std::optional<uint32_t> retention_days,
                                 std::optional<bool> retention_forever) -> void {
    std::lock_guard lock{mutex_};
    auto& stream = require_stream(name);
    if (retention_forever && *retention_forever) {
        stream.retention = RetentionPolicy::Forever;
    } else if (retention_days) {
        if (*retention_days == 0) {
            throw common::Exception{
                "RETENTION days must be > 0",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        stream.retention = RetentionPolicy::Days;
        stream.retention_days = *retention_days;
    }
}

auto StreamManager::has_stream(std::string_view name) const -> bool {
    std::lock_guard lock{mutex_};
    return streams_.contains(std::string{name});
}

auto StreamManager::get_stream(std::string_view name) const -> const StreamEntry* {
    std::lock_guard lock{mutex_};
    auto it = streams_.find(std::string{name});
    return it == streams_.end() ? nullptr : &it->second;
}

auto StreamManager::list_streams() const -> std::vector<StreamEntry> {
    std::lock_guard lock{mutex_};
    std::vector<StreamEntry> out;
    out.reserve(streams_.size());
    for (const auto& [_, entry] : streams_) {
        out.push_back(entry);
    }
    std::ranges::sort(out, [](const auto& a, const auto& b) { return a.name < b.name; });
    return out;
}

auto StreamManager::streams_bound_to_topic(std::string_view topic_name) const
    -> std::vector<std::string> {
    std::lock_guard lock{mutex_};
    std::vector<std::string> out;
    for (const auto& [name, stream] : streams_) {
        if (stream.topic_name == topic_name) {
            out.push_back(name);
        }
    }
    return out;
}

auto StreamManager::create_consumer_group(ConsumerGroupEntry entry, bool if_not_exists) -> void {
    std::lock_guard lock{mutex_};
    validate_consumer_group_entry(entry);
    if (consumer_groups_.contains(entry.name)) {
        if (if_not_exists) return;
        throw common::Exception{
            "Consumer group already exists: " + entry.name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    entry.created_at = std::chrono::system_clock::now();
    consumer_groups_.emplace(entry.name, std::move(entry));
}

auto StreamManager::drop_consumer_group(std::string name, bool if_exists) -> void {
    std::lock_guard lock{mutex_};
    auto it = consumer_groups_.find(name);
    if (it == consumer_groups_.end()) {
        if (if_exists) return;
        throw common::Exception{
            "Unknown consumer group: " + name,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    consumer_groups_.erase(it);
}

auto StreamManager::has_consumer_group(std::string_view name) const -> bool {
    std::lock_guard lock{mutex_};
    return consumer_groups_.contains(std::string{name});
}

auto StreamManager::get_consumer_group(std::string_view name) const -> const ConsumerGroupEntry* {
    std::lock_guard lock{mutex_};
    auto it = consumer_groups_.find(std::string{name});
    return it == consumer_groups_.end() ? nullptr : &it->second;
}

auto StreamManager::list_consumer_groups() const -> std::vector<ConsumerGroupEntry> {
    std::lock_guard lock{mutex_};
    std::vector<ConsumerGroupEntry> out;
    out.reserve(consumer_groups_.size());
    for (const auto& [_, entry] : consumer_groups_) {
        out.push_back(entry);
    }
    std::ranges::sort(out, [](const auto& a, const auto& b) { return a.name < b.name; });
    return out;
}

auto StreamManager::append_to_stream_unlocked(std::string_view stream_name,
                                              std::string payload) -> uint64_t {
    require_stream(stream_name);
    auto& log = event_logs_[std::string{stream_name}];
    const uint64_t offset = log.size();
    StreamEvent event;
    event.offset = offset;
    event.payload = std::move(payload);
    event.ts = std::chrono::system_clock::now();
    log.push_back(std::move(event));
    return offset;
}

auto StreamManager::append_to_stream(std::string_view stream_name, std::string payload) -> uint64_t {
    std::lock_guard lock{mutex_};
    return append_to_stream_unlocked(stream_name, std::move(payload));
}

auto StreamManager::publish_to_topic(std::string_view topic_name, std::string payload) -> uint64_t {
    std::lock_guard lock{mutex_};
    require_topic(topic_name);
    std::vector<std::string> bound;
    for (const auto& [name, stream] : streams_) {
        if (stream.topic_name == topic_name) {
            bound.push_back(name);
        }
    }
    if (bound.empty()) {
        throw common::Exception{
            "No streams bound to topic: " + std::string{topic_name},
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    uint64_t last_offset = 0;
    for (const auto& stream_name : bound) {
        last_offset = append_to_stream_unlocked(stream_name, payload);
    }
    return last_offset;
}

auto StreamManager::subscribe(std::string_view stream_name,
                              std::optional<std::string_view> consumer_group, uint32_t limit,
                              bool advance_offset) -> SubscribeResult {
    std::lock_guard lock{mutex_};
    require_stream(stream_name);
    const auto key = std::string{stream_name};
    const auto& log = event_logs_[key];

    uint64_t start = 0;
    if (consumer_group) {
        auto& group = require_consumer_group(*consumer_group);
        auto it = group.offsets.find(key);
        if (it != group.offsets.end()) {
            start = it->second;
        }
    }

    const uint32_t max_rows = limit == 0 ? 1000 : limit;
    SubscribeResult result;
    for (uint64_t i = start; i < log.size() && result.events.size() < max_rows; ++i) {
        result.events.push_back(log[i]);
    }
    result.next_offset = start + result.events.size();

    if (advance_offset && consumer_group) {
        auto& group = require_consumer_group(*consumer_group);
        group.offsets[key] = result.next_offset;
    }

    return result;
}

auto StreamManager::event_count(std::string_view stream_name) const -> uint64_t {
    std::lock_guard lock{mutex_};
    auto it = event_logs_.find(std::string{stream_name});
    if (it == event_logs_.end()) return 0;
    return it->second.size();
}

auto StreamManager::stream_metrics(std::string_view stream_name,
                                   std::string_view consumer_group) const -> StreamMetrics {
    std::lock_guard lock{mutex_};
    require_stream(stream_name);
    const auto key = std::string{stream_name};
    const auto count = event_logs_.contains(key) ? event_logs_.at(key).size() : 0;

    StreamMetrics metrics;
    metrics.stream_name = std::string{stream_name};
    metrics.event_count = count;
    metrics.consumer_group = std::string{consumer_group};

    if (!consumer_group.empty()) {
        const auto& group = require_consumer_group(consumer_group);
        uint64_t offset = 0;
        auto it = group.offsets.find(key);
        if (it != group.offsets.end()) {
            offset = it->second;
        }
        metrics.consumer_lag = count > offset ? count - offset : 0;
    }

    return metrics;
}

} // namespace mnemo::streaming
