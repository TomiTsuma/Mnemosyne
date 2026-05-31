// src/Processors/processors_source.cpp — Source processor implementations
// Mnemosyne: A column-oriented analytical DBMS

#include "processors_source.h"

namespace mnesso::processors {

// ── NoOpInputStream ──
auto NoOpInputStream::getHeader() -> core::Block { return header_; }
auto NoOpInputStream::read() -> core::Block {
    if (pos_ >= blocks_.size()) return create_empty_block();
    return blocks_[pos_++];
}
auto NoOpInputStream::is_finished() -> bool { return pos_ >= blocks_.size(); }
void NoOpInputStream::set_header(core::Block header) { header_ = std::move(header); }
void NoOpInputStream::set_blocks(std::vector<core::Block> blocks) {
    blocks_ = std::move(blocks);
    pos_ = 0;
}

// ── NoOpOutputStream ──
auto NoOpOutputStream::getHeader() -> core::Block { return header_; }
void NoOpOutputStream::write(const core::Block& block) { header_ = block; }

// ── EmptyBlockSource ──
auto EmptyBlockSource::is_finished() const -> bool { return finished_; }
auto EmptyBlockSource::getHeader() const -> core::Block { return create_empty_block(); }
void EmptyBlockSource::start() { finished_ = true; }
auto EmptyBlockSource::inputs() const -> std::vector<std::shared_ptr<IInputStream>> {
    std::vector<std::shared_ptr<IInputStream>> result;
    result.reserve(in_streams_.size());
    for (const auto& stream : in_streams_) {
        result.push_back(stream);
    }
    return result;
}

auto EmptyBlockSource::outputs() const -> std::vector<std::shared_ptr<IOutputStream>> {
    std::vector<std::shared_ptr<IOutputStream>> result;
    result.reserve(out_streams_.size());
    for (const auto& stream : out_streams_) {
        result.push_back(stream);
    }
    return result;
}

// ── Creating empty streams ──
auto create_empty_input_stream(core::Block header) -> std::shared_ptr<NoOpInputStream> {
    auto stream = std::make_shared<NoOpInputStream>();
    stream->set_header(std::move(header));
    return stream;
}

auto create_empty_output_stream(core::Block header) -> std::shared_ptr<NoOpOutputStream> {
    auto stream = std::make_shared<NoOpOutputStream>();
    stream->write(std::move(header));
    return stream;
}

// ── Creating empty blocks ──
auto create_empty_block() -> core::Block {
    return core::Block{};
}

auto create_partial_empty_block(std::unordered_map<std::string, size_t> column_indices) -> core::Block {
    core::Block block;
    // TODO: create partial empty block with specified columns
    return block;
}

} // namespace mnesso::processors
