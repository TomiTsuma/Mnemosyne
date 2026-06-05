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

namespace mnemo::interpreters {
class Context;
}

namespace mnemo::processors {

// ── SimpleScanProcessor — reads all rows from a table (block-based) ──
class SimpleScanProcessor final : public Processor {
public:
    explicit SimpleScanProcessor(std::string table, interpreters::Context& context);

    [[nodiscard]] auto inputs()  const -> std::vector<std::shared_ptr<IInputStream>>  override { return {}; }
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override { return {}; }
    [[nodiscard]] auto is_finished() const -> bool override { return finished_; }
    void start() override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    [[nodiscard]] auto result() const -> std::optional<core::Block> override;

private:
    std::string table_;
    interpreters::Context& context_;
    core::Block header_;
    core::Block result_data_;
    bool finished_ = false;
};

// ── SimpleFilterProcessor — applies a WHERE predicate to rows (block-based) ──
class SimpleFilterProcessor final : public Processor {
public:
    SimpleFilterProcessor(core::Block header, std::string expression);

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

// ── SimpleProjectProcessor — selects specific columns (block-based) ──
class SimpleProjectProcessor final : public Processor {
public:
    SimpleProjectProcessor(core::Block header, std::vector<std::string> columns);

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

// ── SimpleGroupByProcessor — groups rows by column values (block-based) ──
class SimpleGroupByProcessor final : public Processor {
public:
    explicit SimpleGroupByProcessor(core::Block header);

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

// ── SimpleSortProcessor — orders rows by column(s) (block-based) ──
class SimpleSortProcessor final : public Processor {
public:
    SimpleSortProcessor(core::Block header, std::vector<std::string> order_by);

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

// ── SimpleLimitProcessor — restricts the number of rows returned (block-based) ──
class SimpleLimitProcessor final : public Processor {
public:
    SimpleLimitProcessor(core::Block header, size_t offset, size_t limit);

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
                    std::vector<std::vector<std::string>> values,
                    interpreters::Context& context);

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
    interpreters::Context& context_;
    core::Block result_data_;
    bool finished_ = false;
};

// ── CreateProcessor — creates a new database or table ──
class CreateProcessor final : public Processor {
public:
    CreateProcessor(std::string name, std::vector<std::string> columns, bool is_database, interpreters::Context& context);

    [[nodiscard]] auto inputs()  const -> std::vector<std::shared_ptr<IInputStream>>  override { return {}; }
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override { return {}; }
    [[nodiscard]] auto is_finished() const -> bool override { return finished_; }
    void start() override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    [[nodiscard]] auto result() const -> std::optional<core::Block> override;

private:
    std::string name_;
    std::vector<std::string> columns_;
    bool is_database_;
    interpreters::Context& context_;
    core::Block result_data_;
    bool finished_ = false;
};

// ── UseProcessor — sets the session's current database ──
class UseProcessor final : public Processor {
public:
    UseProcessor(std::string database_name, interpreters::Context& context);

    [[nodiscard]] auto inputs()  const -> std::vector<std::shared_ptr<IInputStream>>  override { return {}; }
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override { return {}; }
    [[nodiscard]] auto is_finished() const -> bool override { return finished_; }
    void start() override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    [[nodiscard]] auto result() const -> std::optional<core::Block> override;

private:
    std::string database_name_;
    interpreters::Context& context_;
    core::Block result_data_;
    bool finished_ = false;
};

// ── DropProcessor — drops an existing table ──
class DropProcessor final : public Processor {
public:
    explicit DropProcessor(std::string table_name, interpreters::Context& context);

    [[nodiscard]] auto inputs()  const -> std::vector<std::shared_ptr<IInputStream>>  override { return {}; }
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override { return {}; }
    [[nodiscard]] auto is_finished() const -> bool override { return finished_; }
    void start() override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    [[nodiscard]] auto result() const -> std::optional<core::Block> override;

private:
    std::string table_name_;
    interpreters::Context& context_;
    core::Block result_data_;
    core::Block header_;
    bool finished_ = false;
};

// ── ShowProcessor — lists databases, tables, or columns ──
class ShowProcessor final : public Processor {
public:
    ShowProcessor(std::string show_type, interpreters::Context& context);

    [[nodiscard]] auto inputs()  const -> std::vector<std::shared_ptr<IInputStream>>  override { return {}; }
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override { return {}; }
    [[nodiscard]] auto is_finished() const -> bool override { return finished_; }
    void start() override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    [[nodiscard]] auto result() const -> std::optional<core::Block> override;

private:
    std::string show_type_;
    interpreters::Context& context_;
    core::Block result_data_;
    bool finished_ = false;
};

// ── DescribeProcessor — returns column metadata for a table ──
class DescribeProcessor final : public Processor {
public:
    explicit DescribeProcessor(std::string table_name, interpreters::Context& context);

    [[nodiscard]] auto inputs()  const -> std::vector<std::shared_ptr<IInputStream>>  override { return {}; }
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override { return {}; }
    [[nodiscard]] auto is_finished() const -> bool override { return finished_; }
    void start() override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    [[nodiscard]] auto result() const -> std::optional<core::Block> override;

private:
    std::string table_name_;
    interpreters::Context& context_;
    core::Block result_data_;
    core::Block header_;
    bool finished_ = false;
};

// ── ExplainProcessor — returns the execution plan as a string ──
class ExplainProcessor final : public Processor {
public:
    explicit ExplainProcessor(std::string explain_plan, interpreters::Context& context);

    [[nodiscard]] auto inputs()  const -> std::vector<std::shared_ptr<IInputStream>>  override { return {}; }
    [[nodiscard]] auto outputs() const -> std::vector<std::shared_ptr<IOutputStream>> override { return {}; }
    [[nodiscard]] auto is_finished() const -> bool override { return finished_; }
    void start() override;
    [[nodiscard]] auto getHeader() const -> core::Block override;
    [[nodiscard]] auto result() const -> std::optional<core::Block> override;

private:
    std::string explain_plan_;
    interpreters::Context& context_;
    core::Block result_data_;
    core::Block header_;
    bool finished_ = false;
};

} // namespace mnemo::processors
