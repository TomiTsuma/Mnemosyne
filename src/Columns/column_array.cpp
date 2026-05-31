// src/Columns/column_array.cpp
#include "Columns/column_array.h"
#include "DataTypes/data_type_factory.h"

namespace mnesso::columns {

auto ColumnArray::create() -> std::shared_ptr<IColumn> {
    return std::make_shared<ColumnArray>();
}

auto ColumnArray::size() const -> size_t { return size_; }

auto ColumnArray::insert(const Field& value) -> size_t {
    throw common::Exception{"Not implemented", 0};
}

auto ColumnArray::insert_at(size_t row_idx, const Field& value) -> size_t {
    if (row_idx >= size_) {
        if (row_idx > size_) insert_many_default(row_idx - size_);
        return insert(value);
    }
    set(row_idx, value);
    return row_idx;
}

auto ColumnArray::insert_default() -> size_t {
    throw common::Exception{"Not implemented", 0};
}

auto ColumnArray::insert_many_default(size_t count) -> size_t {
    throw common::Exception{"Not implemented", 0};
}

auto ColumnArray::insert_range(IColumn& source, size_t start, size_t finish) -> size_t {
    throw common::Exception{"Not implemented", 0};
}

Field ColumnArray::get(size_t row_idx) const {
    throw common::Exception{"Not implemented", 0};
}

Field ColumnArray::get_at(size_t row_idx) const {
    return get(row_idx);
}

auto ColumnArray::get_data_type() const -> datatypes::DataTypePtr {
    return nullptr;
}

void ColumnArray::set(size_t row_idx, const Field& value) {
    throw common::Exception{"Not implemented", 0};
}

void ColumnArray::swap_rows(size_t a, size_t b) {
    throw common::Exception{"Not implemented", 0};
}

auto ColumnArray::clone() const -> ColumnPtr {
    throw common::Exception{"Not implemented", 0};
}

auto ColumnArray::clone_empty() -> ColumnPtr {
    throw common::Exception{"Not implemented", 0};
}

void ColumnArray::permute(std::vector<size_t> indices) {
    throw common::Exception{"Not implemented", 0};
}

void ColumnArray::filter(std::vector<bool> mask) {
    throw common::Exception{"Not implemented", 0};
}

auto ColumnArray::packed_size() const -> size_t {
    throw common::Exception{"Not implemented", 0};
}

void ColumnArray::pack(std::vector<uint8_t>& out) const {
    throw common::Exception{"Not implemented", 0};
}

void ColumnArray::unpack(std::span<const uint8_t> in) {
    throw common::Exception{"Not implemented", 0};
}

auto ColumnArray::compress(std::vector<uint8_t>& buffer) -> size_t {
    throw common::Exception{"Not implemented", 0};
}

void ColumnArray::decompress(std::span<const uint8_t> compressed) {
    throw common::Exception{"Not implemented", 0};
}

} // namespace mnesso::columns