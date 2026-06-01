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

    // Mutators used by Settings during registration
    void set_name(std::string n) { name_ = std::move(n); }
    void set_help(std::string h) { help_ = std::move(h); }

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

// ── Pre-defined default values ──
class DefaultSettings {
public:
    // Network
    static constexpr std::string_view DEFAULT_HOST   = "127.0.0.1";
    static constexpr int64_t          DEFAULT_PORT   = 9000;
    static constexpr int64_t          HTTP_PORT      = 8123;

    // Storage
    static constexpr std::string_view DEFAULT_DATA_PATH = "/var/lib/mnemosyne/data";
    static constexpr std::string_view DEFAULT_LOG_PATH  = "/var/log/mnemosyne";

    // Concurrency
    static constexpr int64_t DEFAULT_MAX_THREADS = 0; // auto-detect
    static constexpr int64_t DEFAULT_MAX_MEMORY_USAGE = 10 * 1024 * 1024 * 1024; // 10 GB

    // Query
    static constexpr int64_t DEFAULT_MAX_QUERY_DURATION_MS = 120000; // 2 min
    static constexpr bool    DEFAULT_PARALLEL_WRITE = true;
    static constexpr int64_t DEFAULT_BLOCK_SIZE     = 65536;
};

// ── Pre-defined Mnemosyne settings ──
inline Settings default_settings() {
    Settings s;
    s.register_setting("host",       DefaultSettings::DEFAULT_HOST,    "Server bind address");
    s.register_setting("port",       DefaultSettings::DEFAULT_PORT,    "Server TCP port");
    s.register_setting("http_port",  DefaultSettings::HTTP_PORT,       "HTTP API port");
    s.register_setting("data_path",  DefaultSettings::DEFAULT_DATA_PATH, "Data directory");
    s.register_setting("log_path",   DefaultSettings::DEFAULT_LOG_PATH,  "Log directory");
    s.register_setting("max_threads", DefaultSettings::DEFAULT_MAX_THREADS, "Max worker threads");
    s.register_setting("max_memory", DefaultSettings::DEFAULT_MAX_MEMORY_USAGE, "Max memory usage (bytes)");
    s.register_setting("max_query_duration_ms", DefaultSettings::DEFAULT_MAX_QUERY_DURATION_MS, "Max query duration (ms)");
    s.register_setting("parallel_write", DefaultSettings::DEFAULT_PARALLEL_WRITE, "Enable parallel writes");
    s.register_setting("block_size", DefaultSettings::DEFAULT_BLOCK_SIZE, "Block size for column chunks");
    return s;
}

} // namespace mnesso::common
