// src/DataTypes/data_type_date.cpp — Date/DateTime type implementations
// Mnemosyne: A column-oriented analytical DBMS

#include "DataTypes/data_type_date.h"
#include "Columns/i_column.h"
#include "Columns/column_vector.h"
#include "Common/exceptions.h"
#include <stdexcept>
#include <cstring>

#ifdef _WIN32
#include <ctime>
#endif

namespace mnesso::datatypes {

// ── Helper functions (defined before use to avoid forward-declaration issues) ──

static bool is_leap_year(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int days_in_year(int y) {
    return is_leap_year(y) ? 366 : 365;
}

// ── DataTypeDate ──

auto DataTypeDate::make() -> DataTypePtr {
    return std::shared_ptr<DataTypeDate>(new DataTypeDate());
}

auto DataTypeDate::id() const -> TypeId { return TypeId::Date; }
auto DataTypeDate::name() const -> std::string { return "Date"; }
auto DataTypeDate::data_size() const -> size_t { return sizeof(int32_t); }

auto DataTypeDate::create_column() const -> void* {
    return new columns::ColumnVector<int32_t>{};
}

void DataTypeDate::serialize(std::span<const uint8_t> data,
                             std::vector<uint8_t>& out) const {
    out.insert(out.end(), data.begin(), data.end());
}

auto DataTypeDate::deserialize(std::span<const uint8_t> in) -> void* {
    auto* col = new columns::ColumnVector<int32_t>{};
    return col;
}

void DataTypeDate::to_string(std::string& out,
                             std::span<const uint8_t> data) const {
    int32_t days = *reinterpret_cast<const int32_t*>(data.data());
    out = to_date_str(days);
}

auto DataTypeDate::from_string(std::string_view text)
    -> std::optional<std::vector<uint8_t>> {
    auto days = from_date_str(text);
    if (!days) return std::nullopt;
    std::vector<uint8_t> buf(sizeof(int32_t));
    std::memcpy(buf.data(), &days.value(), sizeof(int32_t));
    return buf;
}

auto DataTypeDate::to_date_str(int32_t days) const -> std::string {
    // Days since 1970-01-01
    // Simplified calculation
    int y = 1970, m = 1, d = 1;
    int remaining = days;

    while (remaining >= days_in_year(y)) {
        remaining -= days_in_year(y);
        y++;
    }

    int month_days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (is_leap_year(y)) month_days[2] = 29;

    m = 1;
    while (m <= 12 && remaining >= month_days[m]) {
        remaining -= month_days[m];
        m++;
    }
    d = remaining + 1;

    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, m, d);
    return std::string{buf};
}

auto DataTypeDate::from_date_str(std::string_view s) -> std::optional<int32_t> {
    int y, m, d;
    if (std::sscanf(s.data(), "%d-%d-%d", &y, &m, &d) != 3) return std::nullopt;
    if (y < 1970 || m < 1 || m > 12 || d < 1 || d > 31) return std::nullopt;
    return from_ymd(y, m, d);
}

auto DataTypeDate::from_ymd(int y, int m, int d) -> std::optional<int32_t> {
    if (y < 1970) return std::nullopt;

    int32_t days = 0;
    for (int yr = 1970; yr < y; ++yr) {
        days += days_in_year(yr);
    }

    int month_days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (is_leap_year(y)) month_days[2] = 29;

    for (int mo = 1; mo < m; ++mo) {
        days += month_days[mo];
    }
    days += d - 1;
    return days;
}

// ── DataTypeDateTime ──

auto DataTypeDateTime::make() -> DataTypePtr {
    return std::shared_ptr<DataTypeDateTime>(new DataTypeDateTime());
}

auto DataTypeDateTime::id() const -> TypeId { return TypeId::DateTime; }
auto DataTypeDateTime::name() const -> std::string { return "DateTime"; }
auto DataTypeDateTime::data_size() const -> size_t { return sizeof(int64_t); }

auto DataTypeDateTime::create_column() const -> void* {
    return new columns::ColumnVector<int64_t>{};
}

void DataTypeDateTime::serialize(std::span<const uint8_t> data,
                                 std::vector<uint8_t>& out) const {
    out.insert(out.end(), data.begin(), data.end());
}

auto DataTypeDateTime::deserialize(std::span<const uint8_t> in) -> void* {
    auto* col = new columns::ColumnVector<int64_t>{};
    return col;
}

void DataTypeDateTime::to_string(std::string& out,
                                 std::span<const uint8_t> data) const {
    int64_t ts = *reinterpret_cast<const int64_t*>(data.data());
    auto tm = std::tm{};
    time_t t = static_cast<time_t>(ts);
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    out = buf;
}

auto DataTypeDateTime::from_string(std::string_view text)
    -> std::optional<std::vector<uint8_t>> {
    // Parse "YYYY-MM-DD HH:MM:SS" — portable across Windows/POSIX
    int y, mo, d, h, mi, s;
    if (std::sscanf(text.data(), "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6)
        return std::nullopt;

    struct tm tm_val = {};
    tm_val.tm_year = y - 1900;
    tm_val.tm_mon = mo - 1;
    tm_val.tm_mday = d;
    tm_val.tm_hour = h;
    tm_val.tm_min = mi;
    tm_val.tm_sec = s;
    tm_val.tm_isdst = 0;

    time_t ts;
#ifdef _WIN32
    ts = _mkgmtime(&tm_val);
#else
    ts = timegm(&tm_val);
#endif

    std::vector<uint8_t> buf(sizeof(int64_t));
    std::memcpy(buf.data(), &ts, sizeof(int64_t));
    return buf;
}

auto DataTypeDateTime::to_timestamp() const -> int64_t {
    // Returns timestamp for the stored date value
    // Simplified: just return 0 as placeholder
    return 0;
}

auto DataTypeDateTime::from_timestamp(int64_t ts) -> int64_t {
    return ts;
}

} // namespace mnesso::datatypes
