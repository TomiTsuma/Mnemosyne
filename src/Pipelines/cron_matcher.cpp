// src/Pipelines/cron_matcher.cpp

#include "Pipelines/cron_matcher.h"
#include <ctime>
#include <sstream>
#include <vector>

namespace mnemo::pipelines {

namespace {

auto split_fields(std::string_view expr) -> std::vector<std::string> {
    std::vector<std::string> fields;
    std::stringstream ss{std::string{expr}};
    std::string part;
    while (std::getline(ss, part, ' ')) {
        if (!part.empty()) {
            fields.push_back(part);
        }
    }
    return fields;
}

auto field_matches(const std::string& field, int value) -> bool {
    if (field == "*") return true;
    try {
        return std::stoi(field) == value;
    } catch (...) {
        return false;
    }
}

} // namespace

auto cron_matches(std::string_view cron_expr,
                  std::chrono::system_clock::time_point when) -> bool {
    const auto fields = split_fields(cron_expr);
    if (fields.size() != 5) {
        return false;
    }

    const std::time_t t = std::chrono::system_clock::to_time_t(when);
    std::tm local_tm{};
#if defined(_WIN32)
    localtime_s(&local_tm, &t);
#else
    localtime_r(&t, &local_tm);
#endif

    return field_matches(fields[0], local_tm.tm_min) &&
           field_matches(fields[1], local_tm.tm_hour) &&
           field_matches(fields[2], local_tm.tm_mday) &&
           field_matches(fields[3], local_tm.tm_mon + 1) &&
           field_matches(fields[4], local_tm.tm_wday);
}

} // namespace mnemo::pipelines
