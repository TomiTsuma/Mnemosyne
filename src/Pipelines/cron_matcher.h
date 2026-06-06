// src/Pipelines/cron_matcher.h — Lightweight 5-field cron matcher

#pragma once

#include <chrono>
#include <string_view>

namespace mnemo::pipelines {

[[nodiscard]] auto cron_matches(std::string_view cron_expr,
                                std::chrono::system_clock::time_point when) -> bool;

} // namespace mnemo::pipelines
