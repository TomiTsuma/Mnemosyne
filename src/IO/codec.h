// src/IO/codec.h — ICompressionCodec: abstract compression codec interface
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <vector>
#include <span>
#include <string>
#include <memory>

namespace mnesso::io {

// ── CompressionCodec — base for all compression codecs ──
class CompressionCodec {
public:
    virtual ~CompressionCodec() = default;

    // Encode (compress) data
    virtual auto encode(std::span<const uint8_t> input)
        -> std::vector<uint8_t> = 0;

    // Decode (decompress) data
    [[nodiscard]] virtual auto decode(std::span<const uint8_t> input)
        -> std::shared_ptr<uint8_t[]> = 0;

    // Get codec name
    [[nodiscard]] virtual auto name() const -> std::string = 0;

    // Compression ratio (approximate)
    [[nodiscard]] virtual auto compression_ratio() const -> double { return 0.5; }
};

// ── Codec registry ──
class CodecFactory {
public:
    static auto& instance();

    void register_codec(std::string name,
                        std::function<std::shared_ptr<CompressionCodec>()> creator);

    [[nodiscard]] auto get(std::string_view name) -> std::shared_ptr<CompressionCodec>;
    [[nodiscard]] auto names() const -> std::vector<std::string>;
    [[nodiscard]] bool has(std::string_view name) const;

private:
    CodecFactory() = default;
    std::unordered_map<std::string, std::function<std::shared_ptr<CompressionCodec>()>> registry_;
};

} // namespace mnesso::io
