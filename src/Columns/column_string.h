// src/Columns/column_string.h — ColumnString: variable-length string column
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "i_column.h"
#include "Core/column.h"
#include "DataTypes/data_type_string.h"
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include "Common/span_compat.h"
#include "Core/field.h"
#include <algorithm>
#include <cstring>

namespace mnesso::columns {

// ── ColumnString — variable-length UTF-8 strings with offset table ──
// Strings are stored in a single contiguous buffer. Each row's offset
// is stored in a parallel uint32_t array.
class ColumnString final : public IColumn {
public:
    [[nodiscard]] auto size()    const -> size_t override { return size_; }
    [[nodiscard]] auto mutability() const -> bool override { return true; }
    [[nodiscard]] auto type_name() const -> std::string override { return "ColumnString"; }

    // Clear — reset column to empty
    auto clear() -> size_t override {
        size_t old_size = size_;
        slots_.clear();
        data_.clear();
        size_ = 0;
        return old_size;
    }

    // Value access
    [[nodiscard]] auto get(size_t row_idx) const -> core::Field override {
        if (row_idx >= size_) return core::Field{};
        auto& slot = slots_[row_idx];
        return core::Field{std::string{data_.data() + slot.offset, slot.length}};
    }
    [[nodiscard]] auto get_at(size_t row_idx) const -> core::Field override {
        return get(row_idx);
    }
    [[nodiscard]] auto get_data_type() const -> datatypes::DataTypePtr override {
        return datatypes::DataTypeString::make();
    }
    [[nodiscard]] auto get_span(size_t row_idx) const -> std::span<const char> {
        if (row_idx >= size_) return {};
        auto& slot = slots_[row_idx];
        return {data_.data() + slot.offset, slot.length};
    }

    // Insertion
    auto insert(const core::Field& value) -> size_t override {
        auto s = value.as_string();
        if (s) {
            return insert_str(*s);
        }
        return insert_str("");
    }
    auto insert_at(size_t row_idx, const Field& value) -> size_t override {
        if (row_idx >= size_) {
            if (row_idx > size_) insert_many_default(row_idx - size_);
            return insert(value);
        }
        set(row_idx, value);
        return row_idx;
    }
    auto insert_default() -> size_t override {
        slots_.push_back({0, 0});
        size_ = slots_.size();
        return size_ - 1;
    }
    auto insert_many_default(size_t count) -> size_t override {
        for (size_t i = 0; i < count; ++i) {
            slots_.push_back({0, 0});
        }
        size_ = slots_.size();
        return size_;
    }
    auto insert_range(IColumn& source, size_t start, size_t finish) -> size_t override {
        size_t inserted = 0;
        for (size_t i = start; i < finish && i < source.size(); ++i) {
            auto val = source.get(i);
            auto s = val.as_string();
            if (s) {
                insert_str(*s);
            } else {
                insert_str("");
            }
            ++inserted;
        }
        return inserted;
    }
    void set(size_t row_idx, const core::Field& value) override {
        if (row_idx >= size_) return;
        auto s = value.as_string();
        if (!s) return;
        auto& slot = slots_[row_idx];
        // Remove old string
        for (size_t j = row_idx + 1; j < slots_.size(); ++j) {
            slots_[j].offset -= static_cast<uint32_t>(slot.length);
        }
        // Write new string
        std::copy(s->begin(), s->end(), data_.begin() + slot.offset);
        size_t new_len = s->size();
        if (new_len > slot.length) {
            slot.length = static_cast<uint32_t>(new_len);
        }
    }

    // Bulk operations
    auto insert_str(std::string_view s) -> size_t {
        uint32_t offset = static_cast<uint32_t>(data_.size());
        data_.insert(data_.end(), s.begin(), s.end());
        slots_.push_back({offset, static_cast<uint32_t>(s.size())});
        size_ = slots_.size();
        return size_ - 1;
    }

    // Compression
    auto packed_size() const -> size_t override {
        size_t total = sizeof(uint32_t) * size_; // offset table
        total += data_.size(); // string data
        return total;
    }
    void pack(std::vector<uint8_t>& out) const override {
        for (const auto& slot : slots_) {
            uint32_t off = slot.offset;
            out.insert(out.end(), reinterpret_cast<const uint8_t*>(&off),
                       reinterpret_cast<const uint8_t*>(&off) + sizeof(off));
        }
        out.insert(out.end(), data_.begin(), data_.end());
    }
    void unpack(std::span<const uint8_t> in) override {
        size_t n_slots = in.size() / (sizeof(uint32_t) + 1);
        if (n_slots == 0) { size_ = 0; return; }
        size_t slot_bytes = n_slots * sizeof(uint32_t);
        slots_.resize(n_slots);
        std::memcpy(slots_.data(), in.data(), slot_bytes);
        if (in.size() > slot_bytes) {
            data_.assign(in.begin() + slot_bytes, in.end());
        }
        size_ = n_slots;
    }

    // Swap rows
    void swap_rows(size_t a, size_t b) override {
        if (a < size_ && b < size_) std::swap(slots_[a], slots_[b]);
    }
    void filter(std::vector<bool> mask) override {
        std::vector<StringSlot> new_slots;
        new_slots.reserve(size_);
        for (size_t i = 0; i < size_ && i < mask.size(); ++i) {
            if (mask[i]) new_slots.push_back(slots_[i]);
        }
        slots_ = std::move(new_slots);
        // Recalculate offsets
        size_t offset = 0;
        for (auto& slot : slots_) {
            slot.offset = static_cast<uint32_t>(offset);
            offset += slot.length;
        }
        size_ = slots_.size();
    }

    // Clone
    [[nodiscard]] auto clone()    const -> core::ColumnPtr override {
        auto col = std::make_shared<ColumnString>();
        col->slots_ = slots_;
        col->data_ = data_;
        col->size_ = size_;
        return col;
    }
    [[nodiscard]] auto clone_empty() -> core::ColumnPtr override {
        return std::make_shared<ColumnString>();
    }

    // Factory
    [[nodiscard]] static auto create() -> core::ColumnPtr {
        return std::make_shared<ColumnString>();
    }

    // Compression helper
    auto compress(std::vector<uint8_t>& buffer) -> size_t override {
        pack(buffer);
        return buffer.size();
    }
    void decompress(std::span<const uint8_t> compressed) override {
        unpack(compressed);
    }
    void permute(std::vector<size_t> indices) override {
        std::vector<StringSlot> new_slots(size_);
        for (size_t i = 0; i < indices.size() && i < size_; ++i) {
            if (indices[i] < size_) new_slots[i] = slots_[indices[i]];
        }
        slots_ = std::move(new_slots);
    }

private:
    // Internal representation
    struct StringSlot {
        uint32_t offset = 0;
        uint32_t length = 0;
    };

    std::vector<char> data_;     // contiguous string data
    std::vector<StringSlot> slots_;
    size_t size_ = 0;
};

} // namespace mnesso::columns
