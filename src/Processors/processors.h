// src/Processors/processors.h — Concrete processor implementations for each plan node type
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "processor.h"
#include "Core/block.h"
#include "Processors/i_input_stream.h"
#include "Processors/i_output_stream.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mnesso::processors {

// ── ScanProcessor — reads all rows from a table ──
class ScanProcessor final : public Processor {
public:
    explicit ScanProcessor(std::string table);

    [[nodiscard]] auto inputs()  const -> std::vector<std::shared_ptr<IInputStream>>  override { return {}; }
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override { return {}; }
    [[nodiscard]] auto is_finished() const -> bool override { return finished_; }
    void start() override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    [[nodiscard]] auto result() const -> std::optional<core::Block> override;

private:
    std::string table_;
    core::Block header_;
    core::Block result_data_;
    bool finished_ = false;
};

// ── FilterProcessor — applies a WHERE predicate to rows ──
class FilterProcessor final : public Processor {
public:
    FilterProcessor(core::Block header, std::string expression);

    [[nodiscard]] auto inputs()  const -> std::vector<std::shared_ptr<IInputStream>>  override { return {}; }
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override { return {}; }
    [[nodiscard]] auto is_finished() const -> bool override { return finished_; }
    void start() override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    [[nodiscard]] auto result() const -> std::optional<core::Block> override;

private:
    std::string expression_;
    core::Block header_;
    core::Block result_data_;
    bool finished_ = false;
};

// ── ProjectProcessor — selects specific columns ──
class ProjectProcessor final : public Processor {
public:
    ProjectProcessor(core::Block header, std::vector<std::string> columns);

    [[nodiscard]] auto inputs()  const -> std::vector<std::shared_ptr<IInputStream>>  override { return {}; }
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override { return {}; }
    [[nodiscard]] auto is_finished() const -> bool override { return finished_; }
    void start() override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    [[nodiscard]] auto result() const -> std::optional<core::Block> override;

private:
    std::vector<std::string> columns_;
    core::Block header_;
    core::Block result_data_;
    bool finished_ = false;
};

// ── GroupByProcessor — groups rows by column values ──
class GroupByProcessor final : public Processor {
public:
    explicit GroupByProcessor(core::Block header);

    [[nodiscard]] auto inputs()  const -> std::vector<std::shared_ptr<IInputStream>>  override { return {}; }
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override { return {}; }
    [[nodiscard]] auto is_finished() const -> bool override { return finished_; }
    void start() override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    [[nodiscard]] auto result() const -> std::optional<core::Block> override;

private:
    core::Block header_;
    core::Block result_data_;
    bool finished_ = false;
};

// ── SortProcessor — orders rows by column(s) ──
class SortProcessor final : public Processor {
public:
    SortProcessor(core::Block header, std::vector<std::string> order_by);

    [[nodiscard]] auto inputs()  const -> std::vector<std::shared_ptr<IInputStream>>  override { return {}; }
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override { return {}; }
    [[nodiscard]] auto is_finished() const -> bool override { return finished_; }
    void start() override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    [[nodiscard]] auto result() const -> std::optional<core::Block> override;

private:
    std::vector<std::string> order_by_;
    core::Block header_;
    core::Block result_data_;
    bool finished_ = false;
};

// ── LimitProcessor — restricts the number of rows returned ──
class LimitProcessor final : public Processor {
public:
    LimitProcessor(core::Block header, size_t offset, size_t limit);

    [[nodiscard]] auto inputs()  const -> std::vector<std::shared_ptr<IInputStream>>  override { return {}; }
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override { return {}; }
    [[nodiscard]] auto is_finished() const -> bool override { return finished_; }
    void start() override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    [[nodiscard]] auto result() const -> std::optional<core::Block> override;

private:
    size_t offset_;
    size_t limit_;
    core::Block header_;
    core::Block result_data_;
    bool finished_ = false;
};

// ── InsertProcessor — inserts rows into a table ──
class InsertProcessor final : public Processor {
public:
    InsertProcessor(std::string table,
                    std::vector<std::string> columns,
                    std::vector<std::vector<std::string>> values);

    [[nodiscard]] auto inputs()  const -> std::vector<std::shared_ptr<IInputStream>>  override { return {}; }
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override { return {}; }
    [[nodiscard]] auto is_finished() const -> bool override { return finished_; }
    void start() override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    [[nodiscard]] auto result() const -> std::optional<core::Block> override;

private:
    std::string table_;
    std::vector<std::string> columns_;
    std::vector<std::vector<std::string>> values_;
    core::Block result_data_;
    bool finished_ = false;
};

// ── CreateProcessor — creates a new table ──
class CreateProcessor final : public Processor {
public:
    CreateProcessor(std::string table_name, std::vector<std::string> columns);

    [[nodiscard]] auto inputs()  const -> std::vector<std::shared_ptr<IInputStream>>  override { return {}; }
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override { return {}; }
    [[nodiscard]] auto is_finished() const -> bool override { return finished_; }
    void start() override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    [[nodiscard]] auto result() const -> std::optional<core::Block> override;

private:
    std::string table_name_;
    std::vector<std::string> columns_;
    core::Block result_data_;
    bool finished_ = false;
};

// ── DropProcessor — drops an existing table ──
class DropProcessor final : public Processor {
public:
    explicit DropProcessor(std::string table_name);

    [[nodiscard]] auto inputs()  const -> std::vector<std::shared_ptr<IInputStream>>  override { return {}; }
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override { return {}; }
    [[nodiscard]] auto is_finished() const -> bool override { return finished_; }
    void start() override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    [[nodiscard]] auto result() const -> std::optional<core::Block> override;

private:
    std::string table_name_;
    core::Block result_data_;
    bool finished_ = false;
};

// ── ShowProcessor — lists databases, tables, or columns ──
class ShowProcessor final : public Processor {
public:
    explicit ShowProcessor(std::string show_type);

    [[nodiscard]] auto inputs()  const -> std::vector<std::shared_ptr<IInputStream>>  override { return {}; }
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override { return {}; }
    [[nodiscard]] auto is_finished() const -> bool override { return finished_; }
    void start() override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    [[nodiscard]] auto result() const -> std::optional<core::Block> override;

private:
    std::string show_type_;
    core::Block result_data_;
    bool finished_ = false;
};

// ── DescribeProcessor — returns column metadata for a table ──
class DescribeProcessor final : public Processor {
public:
    explicit DescribeProcessor(std::string table_name);

    [[nodiscard]] auto inputs()  const -> std::vector<std::shared_ptr<IInputStream>>  override { return {}; }
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override { return {}; }
    [[nodiscard]] auto is_finished() const -> bool override { return finished_; }
    void start() override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    [[nodiscard]] auto result() const -> std::optional<core::Block> override;

private:
    std::string table_name_;
    core::Block result_data_;
    bool finished_ = false;
};

// ── ExplainProcessor — returns the execution plan as a string ──
class ExplainProcessor final : public Processor {
public:
    explicit ExplainProcessor(std::string explain_plan);

    [[nodiscard]] auto inputs()  const -> std::vector<std::shared_ptr<IInputStream>>  override { return {}; }
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override { return {}; }
    [[nodiscard]] auto is_finished() const -> bool override { return finished_; }
    void start() override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    [[nodiscard]] auto result() const -> std::optional<core::Block> override;

private:
    std::string explain_plan_;
    core::Block result_data_;
    bool finished_ = false;
};

} // namespace mnesso::processors
