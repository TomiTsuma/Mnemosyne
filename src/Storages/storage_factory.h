// src/Storages/storage_factory.h — Registry of registered storage engines
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "i_storage.h"
#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>

namespace mnemo::storages {

// ── StorageFactory — singleton registry for storage engines ──
class StorageFactory {
public:
    static auto instance() -> StorageFactory&;

    void register_engine(std::string name,
                         std::function<std::shared_ptr<IStorage>(std::string)> creator);

    [[nodiscard]] auto create(std::string name, std::string engine)
        -> std::shared_ptr<IStorage>;

    [[nodiscard]] auto names() const -> std::vector<std::string>;
    [[nodiscard]] bool has(std::string_view name) const;

private:
    StorageFactory() = default;
    std::unordered_map<std::string, std::function<std::shared_ptr<IStorage>(std::string)>> registry_;
};

// ── Builtin engine names ──
namespace BuiltinEngines {
    inline constexpr auto FILE     = "File";
    inline constexpr auto MEMORY   = "Memory";
    inline constexpr auto DICTIONARY = "Dictionary";
    inline constexpr auto LOG      = "Log";
    inline constexpr auto NULL_ENGINE = "Null";
}

} // namespace mnemo::storages
