// src/DataTypes/data_type.cpp — IDataType: abstract type system for Mnemosyne columns
// Mnemosyne: A column-oriented analytical DBMS

#include "DataTypes/data_type.h"
#include <stdexcept>
#include <unordered_map>
#include "Common/exceptions.h"

namespace mnesso::datatypes {

using common::Exception;
using common::ErrorCode;

bool types_are_compatible(TypeId a, TypeId b) {
    // Numeric types are compatible with each other
    auto is_numeric = [](TypeId t) {
        return t >= TypeId::UInt8 && t <= TypeId::Float64;
    };
    if (is_numeric(a) && is_numeric(b)) return true;
    return a == b;
}

bool types_are_equivalent(const DataTypePtr& a, const DataTypePtr& b) {
    if (!a || !b) return false;
    return a->id() == b->id();
}

TypeId resolve_type_id(std::string_view name) {
    static const std::unordered_map<std::string, TypeId> mapping = {
        {"UInt8", TypeId::UInt8}, {"UInt16", TypeId::UInt16},
        {"UInt32", TypeId::UInt32}, {"UInt64", TypeId::UInt64},
        {"Int8", TypeId::Int8}, {"Int16", TypeId::Int16},
        {"Int32", TypeId::Int32}, {"Int64", TypeId::Int64},
        {"Float32", TypeId::Float32}, {"Float64", TypeId::Float64},
        {"String", TypeId::String}, {"FixedString", TypeId::FixedString},
        {"Date", TypeId::Date}, {"DateTime", TypeId::DateTime},
        {"Array", TypeId::Array}, {"UUID", TypeId::UUID},
        {"Bool", TypeId::Bool}, {"Decimal", TypeId::Decimal},
    };
    auto it = mapping.find(std::string{name});
    if (it == mapping.end()) {
        throw Exception{
            "resolve_type_id: unknown type '" + std::string{name} + "'",
            static_cast<int>(ErrorCode::UNKNOWN_TYPE)};
    }
    return it->second;
}

std::string type_id_to_string(TypeId id) {
    switch (id) {
        case TypeId::UInt8: return "UInt8";
        case TypeId::UInt16: return "UInt16";
        case TypeId::UInt32: return "UInt32";
        case TypeId::UInt64: return "UInt64";
        case TypeId::Int8: return "Int8";
        case TypeId::Int16: return "Int16";
        case TypeId::Int32: return "Int32";
        case TypeId::Int64: return "Int64";
        case TypeId::Float32: return "Float32";
        case TypeId::Float64: return "Float64";
        case TypeId::String: return "String";
        case TypeId::FixedString: return "FixedString";
        case TypeId::Date: return "Date";
        case TypeId::DateTime: return "DateTime";
        case TypeId::Array: return "Array";
        case TypeId::UUID: return "UUID";
        case TypeId::Bool: return "Bool";
        case TypeId::Decimal: return "Decimal";
        case TypeId::Unknown: return "Unknown";
    }
    return "Unknown";
}

} // namespace mnesso::datatypes