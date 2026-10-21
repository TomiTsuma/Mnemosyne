// src/Common/time_format.h — timestamp rendering for SHOW / DESCRIBE output

#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

namespace mnemo::common {

/// "YYYY-MM-DD HH:MM:SS" in UTC, or an empty string for a time point that was never set.
inline auto format_utc_timestamp(std::chrono::system_clock::time_point time_point) -> std::string
{
    if (time_point == std::chrono::system_clock::time_point{})
    {
        return {};
    }
    const auto time = std::chrono::system_clock::to_time_t(time_point);
    std::tm tm_buf{};
#ifdef _WIN32
    gmtime_s(&tm_buf, &time);
#else
    gmtime_r(&time, &tm_buf);
#endif
    std::ostringstream out;
    out << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

} // namespace mnemo::common
