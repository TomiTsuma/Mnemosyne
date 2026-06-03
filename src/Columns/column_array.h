// src/Columns/column_array.h
#pragma once

#include "i_column.h"
#include <vector>
#include <memory>
#include "Common/span_compat.h"
#include <optional>
#include "Core/field.h"
#include "DataTypes/data_type_factory.h"
#include "Common/exceptions.h"

namespace mnesso::columns {

class ColumnArray final : public IColumn {
public:
    static auto create() -> std::shared_ptr<IColumn>;

    auto clear() -> size_t override;
    [[nodiscard]] auto size() const -> size_t override;
    [[nodiscard]] auto mutability() const -> bool override { return true; }
    [[nodiscard]] auto type_name() const -> std::string override { return "ColumnArray"; }

    auto insert(const Field& value) -> size_t override;
    auto insert_at(size_t row_idx, const Field& value) -> size_t override;
    auto insert_default() -> size_t override;
    auto insert_many_default(size_t count) -> size_t override;
    auto insert_range(IColumn& source, size_t start, size_t finish) -> size_t override;

    [[nodiscard]] Field get(size_t row_idx) const override;
    [[nodiscard]] Field get_at(size_t row_idx) const override;
    [[nodiscard]] auto get_data_type() const -> datatypes::DataTypePtr override;
    void set(size_t row_idx, const Field& value) override;
    void swap_rows(size_t a, size_t b) override;

    auto clone() const -> ColumnPtr override;
    auto clone_empty() -> ColumnPtr override;

    void permute(std::vector<size_t> indices) override;
    void filter(std::vector<bool> mask) override;

    auto packed_size() const -> size_t override;
    void pack(std::vector<uint8_t>& out) const override;
    void unpack(std::span<const uint8_t> in) override;

    auto compress(std::vector<uint8_t>& buffer) -> size_t override;
    void decompress(std::span<const uint8_t> compressed) override;

private:
    std::vector<ColumnPtr> sub_columns_;
    std::vector<uint32_t>  offsets_;
    size_t                 size_ = 0;
};

} // namespace mnesso::columns
