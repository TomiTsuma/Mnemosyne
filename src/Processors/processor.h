// src/Processors/processor.h — Pipeline processor interface
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Core/block.h"
#include "Processors/i_input_stream.h"
#include "Processors/i_output_stream.h"
#include <memory>
#include <functional>
#include <optional>

namespace mnesso::processors {

// ── Processor — one stage in the execution pipeline ──
class Processor {
public:
    virtual ~Processor() = default;

    // Get input streams
    [[nodiscard]] virtual auto inputs()  const -> std::vector<std::shared_ptr<IInputStream>> = 0;
    [[nodiscard]] virtual auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> = 0;

    // Check if pipeline is ready to produce output
    [[nodiscard]] virtual auto is_finished() const -> bool = 0;

    // Start the pipeline
    virtual void start();

    // Get next block from output
    [[nodiscard]] virtual auto getHeader() const -> core::Block = 0;

    // Get final result of pipeline execution
    [[nodiscard]] virtual auto result() const -> std::optional<core::Block>;
};

// ── Pipe — connects two processors ──
class Pipe {
public:
    static auto connect(std::shared_ptr<Processor> producer,
                        std::shared_ptr<Processor> consumer) -> Pipe;
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>>;
    [[nodiscard]] auto inputs() const -> std::vector<std::shared_ptr<IInputStream>>;
    [[nodiscard]] auto size() const -> size_t;

private:
    std::vector<std::shared_ptr<IOutputStream>> outputs_;
    std::vector<std::shared_ptr<IInputStream>>  inputs_;
    size_t                                     n_pipes = 0;
};

// ── MultiPipe — fan-out/fan-in ──
class MultiPipe {
public:
    static auto connect(std::vector<std::shared_ptr<Processor>> producers,
                        std::vector<std::shared_ptr<Processor>> consumers)
        -> MultiPipe;
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>>;
    [[nodiscard]] auto inputs() const -> std::vector<std::shared_ptr<IInputStream>>;
    [[nodiscard]] auto size() const -> size_t;

private:
    std::vector<std::shared_ptr<IOutputStream>> outputs_;
    std::vector<std::shared_ptr<IInputStream>>  inputs_;
    size_t                                     n_pipes = 0;
};

} // namespace mnesso::processors
