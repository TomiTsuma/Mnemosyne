// src/Processors/i_output_stream.h — IOutputStream abstract stream
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Core/block.h"
#include <memory>

namespace mnemo::processors {

// ── IOutputStream — interface for writing blocks ──
class IOutputStream {
public:
    virtual ~IOutputStream() = default;

    // Get the stream header
    [[nodiscard]] virtual auto getHeader() -> core::Block = 0;

    // Write a block to the stream
    virtual void write(const core::Block& block) = 0;
};

} // namespace mnemo::processors
