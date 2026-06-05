// src/Core/field.cpp — Field: a type-safe variant for scalar values
// Mnemosyne: A column-oriented analytical DBMS

#include "Core/field.h"
#include <cmath>
#include <compare>
#include <stdexcept>

namespace mnemo::core {

Field::Field() : data_{std::monostate{}} {}

Field::Field(std::nullptr_t) : data_{std::monostate{}} {}

Field::Field(int8_t v) : data_{v} {}
Field::Field(int16_t v) : data_{v} {}
Field::Field(int32_t v) : data_{v} {}
Field::Field(int64_t v) : data_{v} {}
Field::Field(uint8_t v) : data_{v} {}
Field::Field(uint16_t v) : data_{v} {}
Field::Field(uint32_t v) : data_{v} {}
Field::Field(uint64_t v) : data_{v} {}
Field::Field(float v) : data_{v} {}
Field::Field(double v) : data_{v} {}
Field::Field(bool v) : data_{v} {}
Field::Field(std::string v) : data_{std::move(v)} {}
Field::Field(std::string_view v) : data_{std::string{v}} {}
Field::Field(std::shared_ptr<void> data) : data_{std::move(data)} {}

FieldType Field::type() const {
    if (std::holds_alternative<std::monostate>(data_)) return FieldType::Null;
    if (std::holds_alternative<int8_t>(data_)) return FieldType::Int8;
    if (std::holds_alternative<int16_t>(data_)) return FieldType::Int16;
    if (std::holds_alternative<int32_t>(data_)) return FieldType::Int32;
    if (std::holds_alternative<int64_t>(data_)) return FieldType::Int64;
    if (std::holds_alternative<uint8_t>(data_)) return FieldType::UInt8;
    if (std::holds_alternative<uint16_t>(data_)) return FieldType::UInt16;
    if (std::holds_alternative<uint32_t>(data_)) return FieldType::UInt32;
    if (std::holds_alternative<uint64_t>(data_)) return FieldType::UInt64;
    if (std::holds_alternative<float>(data_)) return FieldType::Float32;
    if (std::holds_alternative<double>(data_)) return FieldType::Float64;
    if (std::holds_alternative<bool>(data_)) return FieldType::Bool;
    if (std::holds_alternative<std::string>(data_)) return FieldType::String;
    if (std::holds_alternative<std::shared_ptr<void>>(data_)) return FieldType::Array;
    return FieldType::Null;
}

auto Field::as_int64() const -> std::optional<int64_t> {
    if (const auto* v = std::get_if<int64_t>(&data_)) return *v;
    if (const auto* v = std::get_if<int32_t>(&data_)) return *v;
    if (const auto* v = std::get_if<int16_t>(&data_)) return *v;
    if (const auto* v = std::get_if<int8_t>(&data_)) return *v;
    if (const auto* v = std::get_if<uint64_t>(&data_)) return static_cast<int64_t>(*v);
    if (const auto* v = std::get_if<uint32_t>(&data_)) return static_cast<int64_t>(*v);
    if (const auto* v = std::get_if<uint16_t>(&data_)) return static_cast<int64_t>(*v);
    if (const auto* v = std::get_if<uint8_t>(&data_)) return static_cast<int64_t>(*v);
    if (const auto* v = std::get_if<double>(&data_)) return static_cast<int64_t>(*v);
    if (const auto* v = std::get_if<float>(&data_)) return static_cast<int64_t>(*v);
    return std::nullopt;
}

auto Field::as_uint64() const -> std::optional<uint64_t> {
    if (const auto* v = std::get_if<uint64_t>(&data_)) return *v;
    if (const auto* v = std::get_if<uint32_t>(&data_)) return *v;
    if (const auto* v = std::get_if<uint16_t>(&data_)) return *v;
    if (const auto* v = std::get_if<uint8_t>(&data_)) return *v;
    if (const auto* v = std::get_if<int64_t>(&data_)) {
        int64_t val = *v;
        if (val < 0) return std::nullopt;
        return static_cast<uint64_t>(val);
    }
    if (const auto* v = std::get_if<int32_t>(&data_)) {
        int32_t val = *v;
        if (val < 0) return std::nullopt;
        return static_cast<uint64_t>(val);
    }
    if (const auto* v = std::get_if<double>(&data_)) return static_cast<uint64_t>(*v);
    if (const auto* v = std::get_if<float>(&data_)) return static_cast<uint64_t>(*v);
    return std::nullopt;
}

auto Field::as_float64() const -> std::optional<double> {
    if (const auto* v = std::get_if<double>(&data_)) return *v;
    if (const auto* v = std::get_if<float>(&data_)) return static_cast<double>(*v);
    if (const auto* v = std::get_if<int64_t>(&data_)) return static_cast<double>(*v);
    if (const auto* v = std::get_if<int32_t>(&data_)) return static_cast<double>(*v);
    if (const auto* v = std::get_if<int16_t>(&data_)) return static_cast<double>(*v);
    if (const auto* v = std::get_if<int8_t>(&data_)) return static_cast<double>(*v);
    if (const auto* v = std::get_if<uint64_t>(&data_)) return static_cast<double>(*v);
    if (const auto* v = std::get_if<uint32_t>(&data_)) return static_cast<double>(*v);
    if (const auto* v = std::get_if<uint16_t>(&data_)) return static_cast<double>(*v);
    if (const auto* v = std::get_if<uint8_t>(&data_)) return static_cast<double>(*v);
    return std::nullopt;
}

auto Field::as_string() const -> std::optional<std::string> {
    if (const auto* v = std::get_if<std::string>(&data_)) return *v;
    return std::nullopt;
}

auto Field::as_bool() const -> std::optional<bool> {
    if (const auto* v = std::get_if<bool>(&data_)) return *v;
    // Numeric types can be interpreted as bool (0 = false, non-zero = true)
    if (const auto* v = std::get_if<int64_t>(&data_)) return *v != 0;
    if (const auto* v = std::get_if<uint64_t>(&data_)) return *v != 0;
    return std::nullopt;
}

bool Field::operator==(const Field& other) const {
    if (data_.index() != other.data_.index()) return false;
    return data_ == other.data_;
}

#if (defined(__cplusplus) && __cplusplus >= 202002L) \
 || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
FieldOrdering Field::operator<=>(const Field& other) const {
    if (auto cmp = data_.index() <=> other.data_.index(); cmp != 0) return cmp;

    // Same type — compare values via type-specific dispatch
    // Variant indices (NOT FieldType enum values):
    //   0=monostate, 1-4=int8..int64, 5-8=uint8..uint64,
    //   9=float, 10=double, 11=bool, 12=string, 13=shared_ptr<void>
    switch (data_.index()) {
        case 0:  return FieldOrderingEqual;  // monostate vs monostate

        // Integer types (int8..int64 + uint8..uint64)
        case 1: case 2: case 3: case 4:
        case 5: case 6: case 7: case 8: {
            int64_t a = as_int64().value_or(0);
            int64_t b = other.as_int64().value_or(0);
            return a <=> b;
        }

        // Float types (float + double)
        case 9: case 10: {
            double a = as_float64().value_or(0.0);
            double b = other.as_float64().value_or(0.0);
            if (a < b) return FieldOrderingLess;
            if (a > b) return FieldOrderingGreater;
            return FieldOrderingEqual;
        }

        // Bool
        case 11: {
            bool a = as_bool().value_or(false);
            bool b = other.as_bool().value_or(false);
            return a <=> b;
        }

        // String
        case 12: {
            auto a = as_string();
            auto b = other.as_string();
            return (a ? std::string_view{*a} : std::string_view{}) <=>
                       (b ? std::string_view{*b} : std::string_view{});
        }

        // shared_ptr<void> — compare pointer addresses
        case 13: {
            auto* a = std::get_if<std::shared_ptr<void>>(&data_);
            auto* b = std::get_if<std::shared_ptr<void>>(&other.data_);
            return reinterpret_cast<const void*>(a->get()) <=>
                       reinterpret_cast<const void*>(b->get());
        }

        default: return FieldOrderingEqual;
    }
}
#else
FieldOrdering Field::compare(const Field& other) const {
    if (data_.index() != other.data_.index()) {
        return data_.index() < other.data_.index() ? FieldOrdering::less
                                                    : FieldOrdering::greater;
    }

    switch (data_.index()) {
        case 0:  return FieldOrdering::equal;

        case 1: case 2: case 3: case 4:
        case 5: case 6: case 7: case 8: {
            int64_t a = as_int64().value_or(0);
            int64_t b = other.as_int64().value_or(0);
            return a < b ? FieldOrdering::less
                 : a > b ? FieldOrdering::greater
                          : FieldOrdering::equal;
        }

        case 9: case 10: {
            double a = as_float64().value_or(0.0);
            double b = other.as_float64().value_or(0.0);
            return a < b ? FieldOrdering::less
                 : a > b ? FieldOrdering::greater
                          : FieldOrdering::equal;
        }

        case 11: {
            bool a = as_bool().value_or(false);
            bool b = other.as_bool().value_or(false);
            return a == b ? FieldOrdering::equal
                 : a ? FieldOrdering::greater
                     : FieldOrdering::less;
        }

        case 12: {
            auto a = as_string();
            auto b = other.as_string();
            auto lhs = a ? std::string_view{*a} : std::string_view{};
            auto rhs = b ? std::string_view{*b} : std::string_view{};
            return lhs < rhs ? FieldOrdering::less
                 : lhs > rhs ? FieldOrdering::greater
                              : FieldOrdering::equal;
        }

        case 13: {
            auto* a = std::get_if<std::shared_ptr<void>>(&data_);
            auto* b = std::get_if<std::shared_ptr<void>>(&other.data_);
            auto lhs = reinterpret_cast<const void*>(a->get());
            auto rhs = reinterpret_cast<const void*>(b->get());
            return lhs < rhs ? FieldOrdering::less
                 : lhs > rhs ? FieldOrdering::greater
                              : FieldOrdering::equal;
        }

        default: return FieldOrdering::equal;
    }
}
#endif

// ── Derived comparison operators ──

bool Field::operator<(const Field& other) const {
#if (defined(__cplusplus) && __cplusplus >= 202002L) \
 || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
    return (*this <=> other) == FieldOrderingLess;
#else
    return compare(other) == FieldOrdering::less;
#endif
}

bool Field::operator<=(const Field& other) const {
#if (defined(__cplusplus) && __cplusplus >= 202002L) \
 || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
    return (*this <=> other) != FieldOrderingGreater;
#else
    auto c = compare(other);
    return c == FieldOrdering::less || c == FieldOrdering::equal;
#endif
}

bool Field::operator>(const Field& other) const {
#if (defined(__cplusplus) && __cplusplus >= 202002L) \
 || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
    return (*this <=> other) == FieldOrderingGreater;
#else
    return compare(other) == FieldOrdering::greater;
#endif
}

bool Field::operator>=(const Field& other) const {
#if (defined(__cplusplus) && __cplusplus >= 202002L) \
 || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
    return (*this <=> other) != FieldOrderingLess;
#else
    auto c = compare(other);
    return c == FieldOrdering::greater || c == FieldOrdering::equal;
#endif
}

bool Field::is_null() const {
    return std::holds_alternative<std::monostate>(data_);
}

void Field::make_null() {
    data_ = std::monostate{};
}

static std::string type_name_for_index(size_t idx) {
    switch (idx) {
        case 0:  return "Null";
        case 1:  return "Int8";
        case 2:  return "Int16";
        case 3:  return "Int32";
        case 4:  return "Int64";
        case 5:  return "UInt8";
        case 6:  return "UInt16";
        case 7:  return "UInt32";
        case 8:  return "UInt64";
        case 9:  return "Float32";
        case 10: return "Float64";
        case 11: return "Bool";
        case 12: return "String";
        case 13: return "Array";
        default: return "Unknown";
    }
}

std::string Field::type_name() const {
    return type_name_for_index(data_.index());
}

} // namespace mnemo::core
