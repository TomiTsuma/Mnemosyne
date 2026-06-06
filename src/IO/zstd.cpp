// src/IO/zstd.cpp — ZSTD compression codec stub
// Mnemosyne: A column-oriented analytical DBMS
// Note: Full ZSTD support requires linking the zstd library.
// This stub passes through data uncompressed.

#include "IO/zstd.h"
#include <algorithm>
#include <cstring>

namespace mnemo::io {

auto ZstdCodec::create() -> std::shared_ptr<ZstdCodec> {
    return std::shared_ptr<ZstdCodec>(new ZstdCodec());
}

auto ZstdCodec::encode(std::span<const uint8_t> input) -> std::vector<uint8_t> {
    // Stub: pass-through without compression
    // In production, call ZSTD_compress() here
    return std::vector<uint8_t>{input.begin(), input.end()};
}

auto ZstdCodec::decode(std::span<const uint8_t> input) -> std::shared_ptr<uint8_t[]> {
    auto result = std::make_shared<uint8_t[]>(input.size());
    std::memcpy(result.get(), input.data(), input.size());
    return result;
}

auto ZstdCodec::name() const -> std::string {
    return CodecName;
}

auto ZstdCodec::compression_ratio() const -> double {
    return 0.3; // ZSTD typically achieves ~3:1 ratio
}

} // namespace mnemo::io
