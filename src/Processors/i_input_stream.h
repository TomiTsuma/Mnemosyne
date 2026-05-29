// src/Processors/i_input_stream.h — IInputStream abstract stream
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "core/block.h"
#include <memory>

namespace mnesso::processors {

// ── IInputStream — interface for reading blocks ──
class IInputStream {
public:
    virtual ~IInputStream() = default;

    // Get the stream header
    [[nodiscard]] virtual auto getHeader() -> core::Block = 0;

    // Read one block from the stream
    [[nodiscard]] virtual auto read() -> core::Block = 0;

    // Check if more data is available
    [[nodiscard]] virtual auto is_finished() -> bool = 0;
};

} // namespace mnesso::processors
