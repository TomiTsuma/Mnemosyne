// src/IO/lz4.cpp — LZ4 compression codec stub
// Mnemosyne: A column-oriented analytical DBMS
// Note: Full LZ4 support requires linking the lz4 library.
// This stub passes through data uncompressed.

#include "IO/lz4.h"
#include <algorithm>
#include <cstring>

namespace mnemo::io {

auto LZ4Codec::create() -> std::shared_ptr<LZ4Codec> {
    return std::shared_ptr<LZ4Codec>(new LZ4Codec());
}

auto LZ4Codec::encode(std::span<const uint8_t> input) -> std::vector<uint8_t> {
    // Stub: pass-through without compression
    // In production, call LZ4_compress_default() here
    return std::vector<uint8_t>{input.begin(), input.end()};
}

auto LZ4Codec::decode(std::span<const uint8_t> input) -> std::shared_ptr<uint8_t[]> {
    auto result = std::make_shared<uint8_t[]>(input.size());
    std::memcpy(result.get(), input.data(), input.size());
    return result;
}

auto LZ4Codec::name() const -> std::string {
    return CodecName;
}

auto LZ4Codec::compression_ratio() const -> double {
    return 0.5; // LZ4 typically achieves ~2:1 ratio
}

} // namespace mnemo::io
