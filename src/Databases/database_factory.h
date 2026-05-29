// src/Databases/database_factory.h — Registry of registered database engines
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "i_database.h"
#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>

namespace mnesso::databases {

// ── DatabaseFactory — singleton registry for database engines ──
class DatabaseFactory {
public:
    static auto& instance();

    void register_engine(std::string name,
                         std::function<std::shared_ptr<IDatabase>()> creator);

    [[nodiscard]] auto get_engine(std::string_view name)
        -> std::shared_ptr<IDatabase>;

    [[nodiscard]] auto names() const -> std::vector<std::string>;
    [[nodiscard]] bool has(std::string_view name) const;

private:
    DatabaseFactory() = default;
    std::unordered_map<std::string, std::function<std::shared_ptr<IDatabase>()>> registry_;
};

// ── Builtin engine names ──
namespace BuiltinEngines {
    inline constexpr auto MEMORY = "Memory";
    inline constexpr auto MYSQL  = "MySQL";
    inline constexpr auto FILE   = "File";
    inline constexpr auto NAMESPACE = "Namespace";
}

} // namespace mnesso::databases
