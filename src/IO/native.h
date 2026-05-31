// src/IO/native.h — Native (uncompressed) codec
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "codec.h"
#include <string>
#include <memory>
#include "Common/span_compat.h"
#include <vector>

namespace mnesso::io {

// ── NativeCodec — pass-through (no compression) ──
class NativeCodec final : public CompressionCodec {
public:
    static auto create() -> std::shared_ptr<NativeCodec>;

    [[nodiscard]] auto encode(std::span<const uint8_t> input)
        -> std::vector<uint8_t> override;
    [[nodiscard]] auto decode(std::span<const uint8_t> input)
        -> std::shared_ptr<uint8_t[]> override;
    [[nodiscard]] auto name() const -> std::string override;
    [[nodiscard]] auto compression_ratio() const -> double override;

    static constexpr auto CodecName = "Native";
};

} // namespace mnesso::io
