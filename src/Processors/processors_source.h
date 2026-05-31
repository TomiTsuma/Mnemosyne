// src/Processors/processors_source.h — Source processors
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "processor.h"
#include "Core/block.h"
#include "Processors/i_input_stream.h"
#include "Processors/i_output_stream.h"
#include <memory>

namespace mnesso::processors {

// ── NoOpStream — pass-through stream with no processing ──
class NoOpInputStream final : public IInputStream {
public:
    [[nodiscard]] auto getHeader() -> core::Block override;
    [[nodiscard]] auto read() -> core::Block override;
    [[nodiscard]] auto is_finished() -> bool override;

    void set_header(core::Block header);
    void set_blocks(std::vector<core::Block> blocks);

private:
    core::Block        header_;
    std::vector<core::Block> blocks_;
    size_t             pos_ = 0;
};

class NoOpOutputStream final : public IOutputStream {
public:
    [[nodiscard]] auto getHeader() -> core::Block override;
    void write(const core::Block& block) override;

private:
    core::Block header_;
};

// ── EmptyBlockSource — source that emits a single empty block ──
class EmptyBlockSource final : public Processor {
public:
    [[nodiscard]] auto is_finished() const -> bool override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    void start() override;
    [[nodiscard]] auto inputs() const -> std::vector<std::shared_ptr<IInputStream>> override;
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override;

private:
    std::vector<std::shared_ptr<NoOpInputStream>>  in_streams_;
    std::vector<std::shared_ptr<NoOpOutputStream>> out_streams_;
    bool finished_ = false;
};

// ── Creating empty streams ──
std::shared_ptr<NoOpInputStream> create_empty_input_stream(core::Block header);
std::shared_ptr<NoOpOutputStream> create_empty_output_stream(core::Block header);

// ── Creating empty blocks ──
core::Block create_empty_block();
core::Block create_partial_empty_block(std::unordered_map<std::string, size_t> column_indices);

} // namespace mnesso::processors
