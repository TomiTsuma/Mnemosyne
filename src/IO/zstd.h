// src/IO/zstd.h — ZSTD compression codec implementation
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "codec.h"
#include <string>
#include <memory>
#include "Common/span_compat.h"
#include <vector>

namespace mnemo::io {

// ── ZstdCodec — high-ratio compression using ZSTD ──
class ZstdCodec final : public CompressionCodec {
public:
    static auto create() -> std::shared_ptr<ZstdCodec>;

    [[nodiscard]] auto encode(std::span<const uint8_t> input)
        -> std::vector<uint8_t> override;
    [[nodiscard]] auto decode(std::span<const uint8_t> input)
        -> std::shared_ptr<uint8_t[]> override;
    [[nodiscard]] auto name() const -> std::string override;
    [[nodiscard]] auto compression_ratio() const -> double override;

    static constexpr auto CodecName = "ZSTD";
    static constexpr auto DefaultLevel = 3;

private:
    ZstdCodec(int level = DefaultLevel) : level_(level) {}
    int level_;
};

} // namespace mnemo::io
