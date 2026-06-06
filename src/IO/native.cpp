// src/IO/native.cpp — Native (pass-through) codec
// Mnemosyne: A column-oriented analytical DBMS

#include "IO/native.h"
#include <algorithm>
#include <cstring>

namespace mnemo::io {

auto NativeCodec::create() -> std::shared_ptr<NativeCodec> {
    return std::make_shared<NativeCodec>();
}

auto NativeCodec::encode(std::span<const uint8_t> input) -> std::vector<uint8_t> {
    // No compression — pass through
    return std::vector<uint8_t>{input.begin(), input.end()};
}

auto NativeCodec::decode(std::span<const uint8_t> input) -> std::shared_ptr<uint8_t[]> {
    auto result = std::make_shared<uint8_t[]>(input.size());
    std::memcpy(result.get(), input.data(), input.size());
    return result;
}

auto NativeCodec::name() const -> std::string {
    return CodecName;
}

auto NativeCodec::compression_ratio() const -> double {
    return 1.0; // No compression
}

} // namespace mnemo::io
