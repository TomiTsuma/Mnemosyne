// src/Processors/processors_source.h — Source processors and pipeline stages
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "processor.h"
#include "Core/block.h"
#include "Core/column.h"
#include "Processors/i_input_stream.h"
#include "Processors/i_output_stream.h"
#include "Storages/i_storage.h"
#include "DataTypes/data_type.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

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

// ── ScanProcessor — reads data from storage ──
class ScanProcessor final : public Processor {
public:
    explicit ScanProcessor(std::shared_ptr<storages::IStorage> storage,
                           std::vector<std::string> column_names,
                           size_t max_block_size = 65536);

    [[nodiscard]] auto is_finished() const -> bool override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    void start() override;
    [[nodiscard]] auto inputs() const -> std::vector<std::shared_ptr<IInputStream>> override;
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override;

private:
    std::shared_ptr<storages::IStorage> storage_;
    std::vector<std::string> column_names_;
    size_t max_block_size_;
    bool finished_ = false;
    core::Block header_;
    core::Block current_block_;
    std::vector<std::shared_ptr<NoOpOutputStream>> out_streams_;
    std::vector<std::shared_ptr<NoOpInputStream>> in_streams_;
};

// ── FilterProcessor — evaluates WHERE expressions ──
class FilterProcessor final : public Processor {
public:
    explicit FilterProcessor(std::shared_ptr<IInputStream> input,
                             std::function<bool(const core::Field&)> predicate);

    [[nodiscard]] auto is_finished() const -> bool override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    void start() override;
    [[nodiscard]] auto inputs() const -> std::vector<std::shared_ptr<IInputStream>> override;
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override;

private:
    std::shared_ptr<IInputStream> input_;
    std::shared_ptr<NoOpInputStream> in_stream_;
    std::shared_ptr<NoOpOutputStream> out_stream_;
    std::function<bool(const core::Field&)> predicate_;
    core::Block header_;
    bool finished_ = false;
    size_t current_row_ = 0;
    core::Block current_block_;
};

// ── ProjectProcessor — projects selected columns ──
class ProjectProcessor final : public Processor {
public:
    explicit ProjectProcessor(std::shared_ptr<IInputStream> input,
                              std::vector<std::string> column_names);

    [[nodiscard]] auto is_finished() const -> bool override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    void start() override;
    [[nodiscard]] auto inputs() const -> std::vector<std::shared_ptr<IInputStream>> override;
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override;

private:
    std::shared_ptr<IInputStream> input_;
    std::shared_ptr<NoOpInputStream> in_stream_;
    std::shared_ptr<NoOpOutputStream> out_stream_;
    std::vector<std::string> column_names_;
    std::unordered_map<std::string, size_t> column_indices_;
    core::Block header_;
    bool finished_ = false;
    size_t current_row_ = 0;
    core::Block current_block_;
};

// ── GroupByProcessor — groups and aggregates data ──
class GroupByProcessor final : public Processor {
public:
    explicit GroupByProcessor(std::shared_ptr<IInputStream> input,
                              std::vector<std::string> group_by_columns,
                              std::vector<std::pair<std::string, std::vector<std::string>>> aggregates);

    [[nodiscard]] auto is_finished() const -> bool override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    void start() override;
    [[nodiscard]] auto inputs() const -> std::vector<std::shared_ptr<IInputStream>> override;
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override;

private:
    std::shared_ptr<IInputStream> input_;
    std::shared_ptr<NoOpInputStream> in_stream_;
    std::shared_ptr<NoOpOutputStream> out_stream_;
    std::vector<std::string> group_by_columns_;
    std::vector<std::pair<std::string, std::vector<std::string>>> aggregates_;
    core::Block header_;
    bool finished_ = false;
    core::Block current_block_;
};

// ── SortProcessor — sorts data by columns ──
class SortProcessor final : public Processor {
public:
    explicit SortProcessor(std::shared_ptr<IInputStream> input,
                           std::vector<std::string> order_by_columns,
                           std::vector<bool> descending);

    [[nodiscard]] auto is_finished() const -> bool override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    void start() override;
    [[nodiscard]] auto inputs() const -> std::vector<std::shared_ptr<IInputStream>> override;
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override;

private:
    std::shared_ptr<IInputStream> input_;
    std::shared_ptr<NoOpInputStream> in_stream_;
    std::shared_ptr<NoOpOutputStream> out_stream_;
    std::vector<std::string> order_by_columns_;
    std::vector<bool> descending_;
    core::Block header_;
    bool finished_ = false;
    core::Block current_block_;
    core::Block result_block_;
};

// ── LimitProcessor — applies LIMIT/OFFSET ──
class LimitProcessor final : public Processor {
public:
    explicit LimitProcessor(std::shared_ptr<IInputStream> input,
                            size_t limit,
                            size_t offset = 0);

    [[nodiscard]] auto is_finished() const -> bool override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    void start() override;
    [[nodiscard]] auto inputs() const -> std::vector<std::shared_ptr<IInputStream>> override;
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override;

private:
    std::shared_ptr<IInputStream> input_;
    std::shared_ptr<NoOpInputStream> in_stream_;
    std::shared_ptr<NoOpOutputStream> out_stream_;
    size_t limit_;
    size_t offset_;
    size_t skipped_ = 0;
    core::Block header_;
    bool finished_ = false;
    core::Block current_block_;
};

} // namespace mnesso::processors
