// src/IO/codecs.cpp — IO codec implementations
// Mnemosyne: A column-oriented analytical DBMS

#include "IO/codecs.h"
#include "Common/exceptions.h"
#include "Columns/column_vector.h"
#include <algorithm>
#include <cstring>
#include <optional>
#include <sstream>

namespace mnemo::io {

// ── BinaryCodec ──

auto BinaryCodec::instance() -> BinaryCodec& {
    static BinaryCodec inst;
    return inst;
}

void BinaryCodec::encode(const core::Block& block, std::vector<uint8_t>& out) {
    // Write number of columns
    uint32_t n_cols = static_cast<uint32_t>(block.column_count());
    out.insert(out.end(),
               reinterpret_cast<const uint8_t*>(&n_cols),
               reinterpret_cast<const uint8_t*>(&n_cols) + sizeof(n_cols));

    // Write each column
    for (const auto& col_name : block.column_names()) {
        auto col = block.get_column_by_name(col_name);
        if (!col) continue;

        // Write column name length and name
        std::string name = col_name;
        uint32_t name_len = static_cast<uint32_t>(name.size());
        out.insert(out.end(),
                   reinterpret_cast<const uint8_t*>(&name_len),
                   reinterpret_cast<const uint8_t*>(&name_len) + sizeof(name_len));
        out.insert(out.end(), name.begin(), name.end());

        // Write column data size
        uint64_t data_size = col->size() * 8; // approximate
        out.insert(out.end(),
                   reinterpret_cast<const uint8_t*>(&data_size),
                   reinterpret_cast<const uint8_t*>(&data_size) + sizeof(data_size));

        // Write column data
        for (size_t i = 0; i < col->size(); ++i) {
            auto val = col->get(i);
            auto num = val.as_float64();
            if (num) {
                auto raw = reinterpret_cast<const uint8_t*>(&*num);
                out.insert(out.end(), raw, raw + sizeof(double));
            } else {
                uint64_t null_val = 0;
                out.insert(out.end(),
                           reinterpret_cast<const uint8_t*>(&null_val),
                           reinterpret_cast<const uint8_t*>(&null_val) + sizeof(null_val));
            }
        }
    }
}

auto BinaryCodec::decode(std::span<const uint8_t> in) -> core::Block {
    core::Block block;
    size_t pos = 0;

    // Read number of columns
    if (pos + sizeof(uint32_t) > in.size()) return block;
    uint32_t n_cols;
    std::memcpy(&n_cols, in.data() + pos, sizeof(n_cols));
    pos += sizeof(n_cols);

    for (uint32_t c = 0; c < n_cols; ++c) {
        // Read column name
        if (pos + sizeof(uint32_t) > in.size()) break;
        uint32_t name_len;
        std::memcpy(&name_len, in.data() + pos, sizeof(name_len));
        pos += sizeof(name_len);

        if (pos + name_len > in.size()) break;
        std::string name(reinterpret_cast<const char*>(in.data() + pos), name_len);
        pos += name_len;

        // Read column data size
        if (pos + sizeof(uint64_t) > in.size()) break;
        uint64_t data_size;
        std::memcpy(&data_size, in.data() + pos, sizeof(data_size));
        pos += sizeof(data_size);

        // Read column data
        auto col = std::make_shared<columns::ColumnVector<double>>();
        size_t n_rows = data_size / sizeof(double);
        col->resize(n_rows);
        auto out = col->get_mutable_span();

        for (size_t i = 0; i < n_rows; ++i) {
            if (pos + sizeof(double) > in.size()) break;
            double val;
            std::memcpy(&val, in.data() + pos, sizeof(val));
            out[i] = val;
            pos += sizeof(val);
        }

        block.add_column(name, col);
    }

    return block;
}

// ── ParquetCodec ──

auto ParquetCodec::instance() -> ParquetCodec& {
    static ParquetCodec inst;
    return inst;
}

void ParquetCodec::encode(const core::Block& block, std::vector<uint8_t>& out) {
    // Simplified: just use binary encoding for now
    BinaryCodec::instance().encode(block, out);
}

auto ParquetCodec::decode(std::span<const uint8_t> in) -> core::Block {
    // Simplified: just use binary decoding for now
    return BinaryCodec::instance().decode(in);
}

// ── CSVCodec ──

auto CSVCodec::instance() -> CSVCodec& {
    static CSVCodec inst;
    return inst;
}

void CSVCodec::encode(const core::Block& block, std::vector<uint8_t>& out) {
    // Write header
    std::string header;
    for (const auto& col_name : block.column_names()) {
        if (!header.empty()) header += ",";
        header += col_name;
    }
    header += "\n";
    out.insert(out.end(), header.begin(), header.end());

    // Write data rows
    size_t n_rows = block.column_count() > 0 ?
                    block.get_column_by_index(0)->size() : 0;
    for (size_t i = 0; i < n_rows; ++i) {
        std::string row;
        for (const auto& col_name : block.column_names()) {
            if (!row.empty()) row += ",";
            auto col = block.get_column_by_name(col_name);
            if (col) {
                auto val = col->get(i);
                auto num = val.as_float64();
                if (num) {
                    row += std::to_string(*num);
                } else {
                    row += "";
                }
            }
        }
        row += "\n";
        out.insert(out.end(), row.begin(), row.end());
    }
}

auto CSVCodec::decode(std::span<const uint8_t> in) -> core::Block {
    core::Block block;
    std::string text{in.begin(), in.end()};

    // Parse CSV
    std::vector<std::string> lines;
    {
        std::istringstream stream{text};
        std::string line;
        while (std::getline(stream, line)) {
            lines.push_back(line);
        }
    }

    if (lines.empty()) return block;

    // Parse header
    std::vector<std::string> headers = split_csv_line(lines[0]);
    for (const auto& header : headers) {
        auto col = std::make_shared<columns::ColumnVector<double>>();
        block.add_column(header, col);
    }

    // Parse data rows
    for (size_t i = 1; i < lines.size(); ++i) {
        auto values = split_csv_line(lines[i]);
        for (size_t j = 0; j < values.size() && j < headers.size(); ++j) {
            auto col = block.get_column_by_name(headers[j]);
            if (col) {
                try {
                    double val = std::stod(values[j]);
                    col->insert(core::Field(val));
                } catch (...) {
                    // Skip invalid values
                }
            }
        }
    }

    return block;
}

std::vector<std::string> CSVCodec::split_csv_line(const std::string& line) {
    std::vector<std::string> result;
    std::string field;
    bool in_quotes = false;

    for (char c : line) {
        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (c == ',' && !in_quotes) {
            result.push_back(field);
            field.clear();
        } else {
            field += c;
        }
    }
    result.push_back(field);

    return result;
}

} // namespace mnemo::io