// src/IO/lz4.h — LZ4 compression codec implementation
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "codec.h"
#include <string>
#include <memory>
#include <span>
#include <vector>

namespace mnesso::io {

// ── LZ4Codec — fast compression using LZ4 ──
class LZ4Codec final : public CompressionCodec {
public:
    static auto create() -> std::shared_ptr<LZ4Codec>;

    [[nodiscard]] auto encode(std::span<const uint8_t> input)
        -> std::vector<uint8_t> override;
    [[nodiscard]] auto decode(std::span<const uint8_t> input)
        -> std::shared_ptr<uint8_t[]> override;
    [[nodiscard]] auto name() const -> std::string override;
    [[nodiscard]] auto compression_ratio() const -> double override;

    static constexpr auto CodecName = "LZ4";

private:
    LZ4Codec() = default;
};

} // namespace mnesso::io
