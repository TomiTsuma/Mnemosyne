// src/DataTypes/data_type_number.cpp — Numeric type implementations
// Mnemosyne: A column-oriented analytical DBMS

#include "DataTypes/data_type_number.h"
#include "Columns/i_column.h"
#include "Columns/column_vector.h"
#include "Common/exceptions.h"
#include <stdexcept>

namespace mnemo::datatypes {

using common::Exception;
using common::ErrorCode;

namespace {
    template<typename T>
    auto create_column_impl() -> void* {
        return new columns::ColumnVector<T>{};
    }

    template<typename T>
    void serialize_impl(std::span<const uint8_t> data,
                        std::vector<uint8_t>& out) {
        out.insert(out.end(), data.begin(), data.end());
    }

    template<typename T>
    auto deserialize_impl(std::span<const uint8_t> in) -> void* {
        // This is a simplified deserialization - in production,
        // this would read from a binary buffer
        auto* col = new columns::ColumnVector<T>{};
        return col;
    }

    template<typename T>
    void to_string_impl(std::string& out, std::span<const uint8_t> data) {
        const T* val = reinterpret_cast<const T*>(data.data());
        out = std::to_string(*val);
    }

    template<typename T>
    auto from_string_impl(std::string_view text)
        -> std::optional<std::vector<uint8_t>> {
        try {
            if constexpr (std::is_integral_v<T>) {
                T val = static_cast<T>(std::stoll(std::string{text}));
                return std::vector<uint8_t>(
                    reinterpret_cast<const uint8_t*>(&val),
                    reinterpret_cast<const uint8_t*>(&val) + sizeof(T));
            } else {
                T val = static_cast<T>(std::stod(std::string{text}));
                return std::vector<uint8_t>(
                    reinterpret_cast<const uint8_t*>(&val),
                    reinterpret_cast<const uint8_t*>(&val) + sizeof(T));
            }
        } catch (...) {
            return std::nullopt;
        }
    }
} // namespace

// ── DataTypeNumber ──

DataTypeNumber::DataTypeNumber(TypeId id, size_t size, bool signed_, bool floating)
    : id_{id}, data_size_{size}, signed_{signed_}, floating_{floating} {}

auto DataTypeNumber::id() const -> TypeId { return id_; }
auto DataTypeNumber::name() const -> std::string { return type_id_to_string(id_); }
auto DataTypeNumber::data_size() const -> size_t { return data_size_; }

auto DataTypeNumber::create_column() const -> void* {
    // Dispatch based on type
    switch (id_) {
        case TypeId::UInt8:  return create_column_impl<uint8_t>();
        case TypeId::UInt16: return create_column_impl<uint16_t>();
        case TypeId::UInt32: return create_column_impl<uint32_t>();
        case TypeId::UInt64: return create_column_impl<uint64_t>();
        case TypeId::Int8:   return create_column_impl<int8_t>();
        case TypeId::Int16:  return create_column_impl<int16_t>();
        case TypeId::Int32:  return create_column_impl<int32_t>();
        case TypeId::Int64:  return create_column_impl<int64_t>();
        case TypeId::Float32: return create_column_impl<float>();
        case TypeId::Float64: return create_column_impl<double>();
        default:
            throw Exception{"DataTypeNumber::create_column: unsupported type",
                static_cast<int>(ErrorCode::LOGICAL_ERROR)};
    }
}

void DataTypeNumber::serialize(std::span<const uint8_t> data,
                               std::vector<uint8_t>& out) const {
    serialize_impl<uint8_t>(data, out);
}

auto DataTypeNumber::deserialize(std::span<const uint8_t> in) -> void* {
    return deserialize_impl<uint8_t>(in);
}

void DataTypeNumber::to_string(std::string& out,
                               std::span<const uint8_t> data) const {
    to_string_impl<uint8_t>(out, data);
}

auto DataTypeNumber::from_string(std::string_view text)
    -> std::optional<std::vector<uint8_t>> {
    return from_string_impl<uint8_t>(text);
}

// ── Factory functions ──

auto make_data_type_uint8() -> DataTypePtr {
    auto dt = std::shared_ptr<DataTypeNumber>(new DataTypeNumber(TypeId::UInt8, sizeof(uint8_t), false, false));
    return dt;
}

auto make_data_type_uint16() -> DataTypePtr {
    auto dt = std::shared_ptr<DataTypeNumber>(new DataTypeNumber(TypeId::UInt16, sizeof(uint16_t), false, false));
    return dt;
}

auto make_data_type_uint32() -> DataTypePtr {
    auto dt = std::shared_ptr<DataTypeNumber>(new DataTypeNumber(TypeId::UInt32, sizeof(uint32_t), false, false));
    return dt;
}

auto make_data_type_uint64() -> DataTypePtr {
    auto dt = std::shared_ptr<DataTypeNumber>(new DataTypeNumber(TypeId::UInt64, sizeof(uint64_t), false, false));
    return dt;
}

auto make_data_type_int8() -> DataTypePtr {
    auto dt = std::shared_ptr<DataTypeNumber>(new DataTypeNumber(TypeId::Int8, sizeof(int8_t), true, false));
    return dt;
}

auto make_data_type_int16() -> DataTypePtr {
    auto dt = std::shared_ptr<DataTypeNumber>(new DataTypeNumber(TypeId::Int16, sizeof(int16_t), true, false));
    return dt;
}

auto make_data_type_int32() -> DataTypePtr {
    auto dt = std::shared_ptr<DataTypeNumber>(new DataTypeNumber(TypeId::Int32, sizeof(int32_t), true, false));
    return dt;
}

auto make_data_type_int64() -> DataTypePtr {
    auto dt = std::shared_ptr<DataTypeNumber>(new DataTypeNumber(TypeId::Int64, sizeof(int64_t), true, false));
    return dt;
}

auto make_data_type_float32() -> DataTypePtr {
    auto dt = std::shared_ptr<DataTypeNumber>(new DataTypeNumber(TypeId::Float32, sizeof(float), true, true));
    return dt;
}

auto make_data_type_float64() -> DataTypePtr {
    auto dt = std::shared_ptr<DataTypeNumber>(new DataTypeNumber(TypeId::Float64, sizeof(double), true, true));
    return dt;
}

// ── Static factory methods ──

auto DataTypeNumber::make(TypeId id) -> DataTypePtr {
    return NumberTypeFactory::create(id);
}

auto NumberTypeFactory::create(TypeId id) -> DataTypePtr {
    switch (id) {
        case TypeId::UInt8:  return make_data_type_uint8();
        case TypeId::UInt16: return make_data_type_uint16();
        case TypeId::UInt32: return make_data_type_uint32();
        case TypeId::UInt64: return make_data_type_uint64();
        case TypeId::Int8:   return make_data_type_int8();
        case TypeId::Int16:  return make_data_type_int16();
        case TypeId::Int32:  return make_data_type_int32();
        case TypeId::Int64:  return make_data_type_int64();
        case TypeId::Float32: return make_data_type_float32();
        case TypeId::Float64: return make_data_type_float64();
        default:
            throw common::Exception{
                "NumberTypeFactory::create: unsupported TypeId",
                static_cast<int>(ErrorCode::LOGICAL_ERROR)};
    }
}

auto NumberTypeFactory::all_ids() -> std::vector<TypeId> {
    return {
        TypeId::UInt8, TypeId::UInt16, TypeId::UInt32, TypeId::UInt64,
        TypeId::Int8,  TypeId::Int16,  TypeId::Int32,  TypeId::Int64,
        TypeId::Float32, TypeId::Float64
    };
}

} // namespace mnemo::datatypes