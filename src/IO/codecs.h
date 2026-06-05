// src/IO/codecs.h — Block codec implementations (Binary, Parquet, CSV)
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Core/block.h"
#include <vector>
#include "Common/span_compat.h"
#include <memory>

namespace mnemo::io {

// ── BinaryCodec — compact binary serialization ──
class BinaryCodec {
public:
    static auto instance() -> BinaryCodec&;

    void encode(const core::Block& block, std::vector<uint8_t>& out);
    auto decode(std::span<const uint8_t> in) -> core::Block;
};

// ── ParquetCodec — Parquet format (stub) ──
class ParquetCodec {
public:
    static auto instance() -> ParquetCodec&;

    void encode(const core::Block& block, std::vector<uint8_t>& out);
    auto decode(std::span<const uint8_t> in) -> core::Block;
};

// ── CSVCodec — CSV text format ──
class CSVCodec {
public:
    static auto instance() -> CSVCodec&;

    void encode(const core::Block& block, std::vector<uint8_t>& out);
    auto decode(std::span<const uint8_t> in) -> core::Block;

private:
    static std::vector<std::string> split_csv_line(const std::string& line);
};

} // namespace mnemo::io
