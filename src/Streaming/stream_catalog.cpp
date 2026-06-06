// src/Streaming/stream_catalog.cpp

#include "Streaming/stream_catalog.h"
#include "Common/exceptions.h"

namespace mnemo::streaming {

auto stream_status_name(StreamStatus status) -> std::string {
    switch (status) {
        case StreamStatus::Creating: return "CREATING";
        case StreamStatus::Active: return "ACTIVE";
        case StreamStatus::Paused: return "PAUSED";
        case StreamStatus::Degraded: return "DEGRADED";
        case StreamStatus::Offline: return "OFFLINE";
        case StreamStatus::Archived: return "ARCHIVED";
    }
    return "UNKNOWN";
}

auto retention_policy_name(RetentionPolicy policy) -> std::string {
    switch (policy) {
        case RetentionPolicy::Days: return "DAYS";
        case RetentionPolicy::Forever: return "FOREVER";
    }
    return "UNKNOWN";
}

void validate_topic_entry(const TopicEntry& entry) {
    if (entry.name.empty()) {
        throw common::Exception{
            "Topic name cannot be empty",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    if (entry.partition_count == 0) {
        throw common::Exception{
            "Topic partition_count must be >= 1",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
}

void validate_stream_entry(const StreamEntry& entry) {
    if (entry.name.empty()) {
        throw common::Exception{
            "Stream name cannot be empty",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
}

void validate_consumer_group_entry(const ConsumerGroupEntry& entry) {
    if (entry.name.empty()) {
        throw common::Exception{
            "Consumer group name cannot be empty",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
}

} // namespace mnemo::streaming
