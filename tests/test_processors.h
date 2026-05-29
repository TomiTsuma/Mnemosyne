// tests/test_processors.h — Unit tests for pipeline processors
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "processors/processor.h"
#include "processors/processors_source.h"
#include "core/block.h"
#include <catch2/catch_all.hpp>

// ── Processor tests ──
TEST_CASE("EmptyBlockSource emits empty block", "[processor]") {
    auto source = std::make_shared<mnesso::processors::EmptyBlockSource>();
    source->start();

    REQUIRE(source->is_finished());
}

TEST_CASE("NoOpInputStream passes through", "[processor]") {
    auto in_stream = mnesso::processors::create_empty_input_stream(core::Block{});
    auto block = in_stream->read();

    REQUIRE(block.is_empty());
}
