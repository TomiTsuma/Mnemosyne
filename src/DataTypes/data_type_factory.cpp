// src/DataTypes/data_type_factory.cpp — Registry of all registered data types
// Mnemosyne: A column-oriented analytical DBMS

#include "DataTypes/data_type_factory.h"
#include "DataTypes/data_type_number.h"
#include "DataTypes/data_type_string.h"
#include "DataTypes/data_type_date.h"
#include "Common/exceptions.h"

// Forward declarations for types not yet implemented
namespace mnesso::datatypes {
    class DataTypeArray;
    class DataTypeUUID;
    class DataTypeBool;
    class DataTypeDecimal;
}

namespace mnesso::datatypes {

// ── TypeFactory ──

auto& TypeFactory::instance() {
    static TypeFactory inst;
    return inst;
}

void TypeFactory::register_type(std::string name,
                                std::function<DataTypePtr()> creator) {
    registry_[std::move(name)] = std::move(creator);
}

auto TypeFactory::get(std::string_view name) -> DataTypePtr {
    auto it = registry_.find(std::string{name});
    if (it == registry_.end()) return nullptr;
    return it->second();
}

TypeId TypeFactory::resolve(std::string_view name) const {
    auto it = registry_.find(std::string{name});
    if (it == registry_.end()) {
        throw common::Exception{
            "TypeFactory::resolve: type '" + std::string{name} + "' not found",
            static_cast<int>(common::ErrorCode::UNKNOWN_TYPE)};
    }
    auto dt = it->second();
    return dt->id();
}

auto TypeFactory::names() const -> std::vector<std::string> {
    std::vector<std::string> result;
    result.reserve(registry_.size());
    for (const auto& [name, _] : registry_) {
        result.push_back(name);
    }
    return result;
}

bool TypeFactory::has(std::string_view name) const {
    return registry_.find(std::string{name}) != registry_.end();
}

auto TypeFactory::get(TypeId id) const -> DataTypePtr {
    auto it = id_map_.find(id);
    if (it == id_map_.end()) return nullptr;
    return it->second;
}

// ── Global type lookup ──

static TypeFactory& get_factory() {
    auto& factory = TypeFactory::instance();

    // Register all built-in types on first call
    static bool registered = false;
    if (!registered) {
        // Numeric types
        factory.register_type("UInt8", make_data_type_uint8);
        factory.register_type("UInt16", make_data_type_uint16);
        factory.register_type("UInt32", make_data_type_uint32);
        factory.register_type("UInt64", make_data_type_uint64);
        factory.register_type("Int8", make_data_type_int8);
        factory.register_type("Int16", make_data_type_int16);
        factory.register_type("Int32", make_data_type_int32);
        factory.register_type("Int64", make_data_type_int64);
        factory.register_type("Float32", make_data_type_float32);
        factory.register_type("Float64", make_data_type_float64);

        // String types
        factory.register_type("String", DataTypeString::make);
        factory.register_type("FixedString", []() {
            return DataTypeFixedString::make(0);
        });

        // Date types
        factory.register_type("Date", DataTypeDate::make);
        factory.register_type("DateTime", DataTypeDateTime::make);

        // Array type
        factory.register_type("Array", []() -> DataTypePtr {
            throw common::Exception{"DataTypeArray is not yet implemented", 0};
        });

        // UUID type
        factory.register_type("UUID", []() -> DataTypePtr {
            throw common::Exception{"DataTypeUUID is not yet implemented", 0};
        });

        // Bool type
        factory.register_type("Bool", []() -> DataTypePtr {
            throw common::Exception{"DataTypeBool is not yet implemented", 0};
        });

        // Decimal type
        factory.register_type("Decimal", []() -> DataTypePtr {
            throw common::Exception{"DataTypeDecimal is not yet implemented", 0};
        });

        registered = true;
    }
    return factory;
}

DataTypePtr get_data_type(std::string_view name) {
    auto& factory = get_factory();
    auto dt = factory.get(name);
    if (!dt) {
        throw common::Exception{
            "get_data_type: unknown type '" + std::string{name} + "'",
            static_cast<int>(common::ErrorCode::UNKNOWN_TYPE)};
    }
    return dt;
}

} // namespace mnesso::datatypes