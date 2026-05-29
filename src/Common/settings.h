#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <optional>
#include <cstdint>

namespace mnesso::common {

// ── Value types used throughout the settings system ──
using SettingValueType = std::variant<
    bool, int64_t, double, std::string, std::string_view
>;

// ── Setting — a typed configuration key with default and help ──
class Setting {
public:
    Setting();
    explicit Setting(SettingValueType default_value);

    [[nodiscard]] auto get() const -> SettingValueType;
    void            set(SettingValueType value);

    [[nodiscard]] std::string name()    const;
    [[nodiscard]] std::string help()    const;
    [[nodiscard]] bool        has_default() const;

private:
    std::string        name_;
    std::string        help_;
    SettingValueType   value_;
    bool               has_default_;
};

// ── Settings — thread-safe map of named settings ──
class Settings {
public:
    Settings();
    ~Settings() = default;

    // Register a new setting (call during construction)
    void register_setting(std::string name,
                          SettingValueType default_value,
                          std::string help = "");

    // Read / write a setting by name
    [[nodiscard]] auto get(std::string_view name) -> std::optional<SettingValueType>;
    void             set(std::string_view name, SettingValueType value);

    // All registered setting names
    [[nodiscard]] auto names() const -> std::vector<std::string>;

    // Check if a setting exists
    [[nodiscard]] bool has(std::string_view name) const;

private:
    std::unordered_map<std::string, Setting> settings_;
    // TODO: add std::shared_mutex for thread-safe concurrent reads
};

// ── Pre-defined Mnemosyne settings ──
class DefaultSettings {
public:
    // Network
    static constexpr auto DEFAULT_HOST   = "127.0.0.1";
    static constexpr int  DEFAULT_PORT   = 9000;
    static constexpr int  HTTP_PORT      = 8123;

    // Storage
    static constexpr auto DEFAULT_DATA_PATH = "/var/lib/mnemosyne/data";
    static constexpr auto DEFAULT_LOG_PATH  = "/var/log/mnemosyne";

    // Concurrency
    static constexpr size_t DEFAULT_MAX_THREADS = 0; // auto-detect
    static constexpr size_t DEFAULT_MAX_MEMORY_USAGE = 10ULL * 1024 * 1024 * 1024; // 10 GB

    // Query
    static constexpr size_t DEFAULT_MAX_QUERY_DURATION_MS = 120'000; // 2 min
    static constexpr bool   DEFAULT_PARALLEL_WRITE = true;
    static constexpr size_t DEFAULT_BLOCK_SIZE     = 65536;
};

} // namespace mnesso::common
