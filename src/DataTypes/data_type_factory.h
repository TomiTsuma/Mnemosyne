// src/DataTypes/data_type_factory.h — Registry of all registered data types
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "data_type.h"
#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>

namespace mnesso::datatypes {

// ── TypeFactory — singleton registry for all data types ──
class TypeFactory {
public:
    // Get the singleton
    static TypeFactory& instance();

    // Register a named type
    void register_type(std::string name, std::function<DataTypePtr()> creator);

    // Look up a type by name — returns nullptr if unknown
    [[nodiscard]] auto get(std::string_view name) -> DataTypePtr;

    // Resolve TypeId for a name — throws if not found
    [[nodiscard]] TypeId resolve(std::string_view name) const;

    // List all registered types
    [[nodiscard]] auto names() const -> std::vector<std::string>;

    // Check if a type exists
    [[nodiscard]] bool has(std::string_view name) const;

    // Get type by TypeId (for primitive types)
    [[nodiscard]] DataTypePtr get(TypeId id) const;

private:
    TypeFactory() = default;

    std::unordered_map<std::string, std::function<DataTypePtr()>> registry_;
    std::unordered_map<TypeId, DataTypePtr>                       id_map_;
};

// ── Global type lookup (auto-registers known types on first call) ──
DataTypePtr get_data_type(std::string_view name);

} // namespace mnesso::datatypes
