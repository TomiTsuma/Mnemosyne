// tests/test_io.h — Unit tests for IO codecs
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "io/codec.h"
#include "io/lz4.h"
#include "io/zstd.h"
#include "io/native.h"
#include <catch2/catch_all.hpp>

// ── Codec tests ──
TEST_CASE("NativeCodec pass-through", "[codec]") {
    auto codec = mnesso::io::NativeCodec::create();
    auto data = std::vector<uint8_t>{1, 2, 3, 4, 5};

    auto encoded = codec->encode(data);
    auto decoded = codec->decode(encoded);

    REQUIRE(decoded != nullptr);
    REQUIRE(std::equal(data.begin(), data.end(), decoded.get(), decoded.get() + data.size()));
}

TEST_CASE("LZ4Codec round-trip", "[codec]") {
    auto codec = mnesso::io::LZ4Codec::create();
    auto data = std::vector<uint8_t>(1024); // 1KB of data
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(i % 256);
    }

    auto encoded = codec->encode(data);
    auto decoded = codec->decode(encoded);

    REQUIRE(decoded != nullptr);
    REQUIRE(std::equal(data.begin(), data.end(), decoded.get(), decoded.get() + data.size()));
}
