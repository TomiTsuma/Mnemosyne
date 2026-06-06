// src/Streaming/stream_manager.h — Cluster-wide streaming registry

#pragma once

#include "Streaming/stream_catalog.h"
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mnemo::streaming {

class StreamManager {
public:
    static auto instance() -> StreamManager&;

    auto create_topic(TopicEntry entry, bool if_not_exists) -> void;
    auto drop_topic(std::string name, bool if_exists) -> void;
    [[nodiscard]] auto has_topic(std::string_view name) const -> bool;
    [[nodiscard]] auto get_topic(std::string_view name) const -> const TopicEntry*;
    [[nodiscard]] auto list_topics() const -> std::vector<TopicEntry>;

    auto create_stream(StreamEntry entry, bool if_not_exists) -> void;
    auto drop_stream(std::string name, bool if_exists) -> void;
    auto alter_stream(std::string_view name, std::optional<uint32_t> retention_days,
                      std::optional<bool> retention_forever) -> void;
    [[nodiscard]] auto has_stream(std::string_view name) const -> bool;
    [[nodiscard]] auto get_stream(std::string_view name) const -> const StreamEntry*;
    [[nodiscard]] auto list_streams() const -> std::vector<StreamEntry>;
    [[nodiscard]] auto streams_bound_to_topic(std::string_view topic_name) const
        -> std::vector<std::string>;

    auto create_consumer_group(ConsumerGroupEntry entry, bool if_not_exists) -> void;
    auto drop_consumer_group(std::string name, bool if_exists) -> void;
    [[nodiscard]] auto has_consumer_group(std::string_view name) const -> bool;
    [[nodiscard]] auto get_consumer_group(std::string_view name) const -> const ConsumerGroupEntry*;
    [[nodiscard]] auto list_consumer_groups() const -> std::vector<ConsumerGroupEntry>;

    auto append_to_stream(std::string_view stream_name, std::string payload) -> uint64_t;
    auto publish_to_topic(std::string_view topic_name, std::string payload) -> uint64_t;
    auto subscribe(std::string_view stream_name, std::optional<std::string_view> consumer_group,
                   uint32_t limit, bool advance_offset) -> SubscribeResult;

    [[nodiscard]] auto event_count(std::string_view stream_name) const -> uint64_t;
    [[nodiscard]] auto stream_metrics(std::string_view stream_name,
                                      std::string_view consumer_group) const -> StreamMetrics;

private:
    StreamManager() = default;

    auto require_topic(std::string_view name) -> TopicEntry&;
    auto require_topic(std::string_view name) const -> const TopicEntry&;
    auto require_stream(std::string_view name) -> StreamEntry&;
    auto require_stream(std::string_view name) const -> const StreamEntry&;
    auto require_consumer_group(std::string_view name) -> ConsumerGroupEntry&;
    auto require_consumer_group(std::string_view name) const -> const ConsumerGroupEntry&;

    auto append_to_stream_unlocked(std::string_view stream_name, std::string payload) -> uint64_t;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, TopicEntry> topics_;
    std::unordered_map<std::string, StreamEntry> streams_;
    std::unordered_map<std::string, ConsumerGroupEntry> consumer_groups_;
    std::unordered_map<std::string, std::vector<StreamEvent>> event_logs_;
};

} // namespace mnemo::streaming
