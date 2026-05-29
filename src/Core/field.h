// src/Core/field.h — Field: a type-safe variant for scalar values
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <memory>
#include <optional>
#include <variant>
#include <compare>

namespace mnesso::core {

// ── FieldType — discriminant for the variant ──
enum class FieldType : uint8_t {
    Null       = 0,
    UInt8      = 1,
    UInt16     = 2,
    UInt32     = 3,
    UInt64     = 4,
    Int8       = 5,
    Int16      = 6,
    Int32      = 7,
    Int64      = 8,
    Float32    = 9,
    Float64    = 10,
    String     = 11,
    FixedString= 12,
    Date       = 13,
    DateTime   = 14,
    Array      = 15,
    UUID       = 16,
    Bool       = 17,
    Decimal    = 18,
};

// ── Field — single scalar value of any Mnemosyne type ──
class Field {
public:
    Field();
    explicit Field(std::nullptr_t);

    explicit Field(int8_t v);
    explicit Field(int16_t v);
    explicit Field(int32_t v);
    explicit Field(int64_t v);
    explicit Field(uint8_t v);
    explicit Field(uint16_t v);
    explicit Field(uint32_t v);
    explicit Field(uint64_t v);
    explicit Field(float v);
    explicit Field(double v);
    explicit Field(bool v);
    explicit Field(std::string v);
    explicit Field(std::string_view v);
    explicit Field(std::shared_ptr<void> data);

    // Type discriminant
    [[nodiscard]] FieldType type() const;

    // Value access — returns std::nullopt if wrong type
    auto as_int64()  const -> std::optional<int64_t>;
    auto as_uint64() const -> std::optional<uint64_t>;
    auto as_float64() const -> std::optional<double>;
    auto as_string() const -> std::optional<std::string>;
    auto as_bool()   const -> std::optional<bool>;

    // Equality (type-safe)
    [[nodiscard]] bool operator==(const Field& other) const;

    // Comparison
    auto operator<=>(const Field&) const = default;

    // Is this field NULL?
    [[nodiscard]] bool is_null() const;

    // Set to NULL
    void make_null();

    // Get type name as string
    [[nodiscard]] std::string type_name() const;

    // Get the raw variant for introspection
    const auto& variant() const { return data_; }

private:
    std::variant<std::monostate,
                 int8_t, int16_t, int32_t, int64_t,
                 uint8_t, uint16_t, uint32_t, uint64_t,
                 float, double, bool,
                 std::string,
                 std::shared_ptr<void>> data_;
};

} // namespace mnesso::core
