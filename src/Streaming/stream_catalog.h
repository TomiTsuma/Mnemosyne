// src/Streaming/stream_catalog.h — Streaming layer catalog types

#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mnemo::streaming {

enum class StreamStatus {
    Creating,
    Active,
    Paused,
    Degraded,
    Offline,
    Archived
};

enum class RetentionPolicy { Days, Forever };

struct StreamEvent {
    uint64_t offset = 0;
    std::string payload;
    std::chrono::system_clock::time_point ts{};
};

struct TopicEntry {
    std::string name;
    uint32_t partition_count = 1;
    RetentionPolicy retention = RetentionPolicy::Forever;
    uint32_t retention_days = 7;
    std::chrono::system_clock::time_point created_at{};
};

struct StreamEntry {
    std::string name;
    std::string topic_name;
    StreamStatus status = StreamStatus::Active;
    RetentionPolicy retention = RetentionPolicy::Forever;
    uint32_t retention_days = 7;
    std::chrono::system_clock::time_point created_at{};
};

struct ConsumerGroupEntry {
    std::string name;
    std::unordered_map<std::string, uint64_t> offsets;
    std::chrono::system_clock::time_point created_at{};
};

struct StreamMetrics {
    std::string stream_name;
    uint64_t event_count = 0;
    uint64_t consumer_lag = 0;
    std::string consumer_group;
};

struct SubscribeResult {
    std::vector<StreamEvent> events;
    uint64_t next_offset = 0;
};

[[nodiscard]] auto stream_status_name(StreamStatus status) -> std::string;
[[nodiscard]] auto retention_policy_name(RetentionPolicy policy) -> std::string;

void validate_topic_entry(const TopicEntry& entry);
void validate_stream_entry(const StreamEntry& entry);
void validate_consumer_group_entry(const ConsumerGroupEntry& entry);

} // namespace mnemo::streaming
