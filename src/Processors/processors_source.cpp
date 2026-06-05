// src/Processors/processors_source.cpp — Source processor implementations
// Mnemosyne: A column-oriented analytical DBMS

#include "processors_source.h"
#include "processors.h"
#include "processor.h"
#include "Common/exceptions.h"
#include "Columns/column_vector.h"
#include "Interpreters/context.h"
#include "Databases/database_memory.h"
#include "DataTypes/data_type_factory.h"
#include <algorithm>
#include <map>
#include <numeric>

namespace mnemo::processors {

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

// ── Pipe ──

auto Pipe::connect(std::shared_ptr<Processor> producer,
                   std::shared_ptr<Processor> consumer) -> Pipe {
    Pipe pipe;
    pipe.outputs_ = producer->outputs();
    pipe.inputs_  = consumer->inputs();
    pipe.n_pipes  = 1;
    return pipe;
}

auto Pipe::outputs() const -> std::vector<std::shared_ptr<IOutputStream>> {
    return outputs_;
}

auto Pipe::inputs() const -> std::vector<std::shared_ptr<IInputStream>> {
    return inputs_;
}

auto Pipe::size() const -> size_t {
    return n_pipes;
}

// ── MultiPipe ──

auto MultiPipe::connect(std::vector<std::shared_ptr<Processor>> producers,
                        std::vector<std::shared_ptr<Processor>> consumers)
    -> MultiPipe {
    MultiPipe pipe;
    for (auto& p : producers) {
        auto outs = p->outputs();
        pipe.outputs_.insert(pipe.outputs_.end(), outs.begin(), outs.end());
    }
    for (auto& c : consumers) {
        auto ins = c->inputs();
        pipe.inputs_.insert(pipe.inputs_.end(), ins.begin(), ins.end());
    }
    pipe.n_pipes = producers.size() + consumers.size();
    return pipe;
}

auto MultiPipe::outputs() const -> std::vector<std::shared_ptr<IOutputStream>> {
    return outputs_;
}

auto MultiPipe::inputs() const -> std::vector<std::shared_ptr<IInputStream>> {
    return inputs_;
}

auto MultiPipe::size() const -> size_t {
    return n_pipes;
}

// ── ScanProcessor ──

ScanProcessor::ScanProcessor(std::shared_ptr<storages::IStorage> storage,
                             std::vector<std::string> column_names,
                             size_t max_block_size)
    : storage_(std::move(storage)),
      column_names_(std::move(column_names)),
      max_block_size_(max_block_size) {
    out_streams_.push_back(std::make_shared<NoOpOutputStream>());
}

auto ScanProcessor::is_finished() const -> bool { return finished_; }
auto ScanProcessor::getHeader() const -> core::Block { return header_; }

void ScanProcessor::start() {
    if (finished_) return;

    // Build header from storage schema
    auto col_types = storage_->column_types();
    for (auto& col_name : column_names_) {
        auto it = col_types.find(col_name);
        if (it != col_types.end()) {
            auto* raw = it->second->create_column();
            auto col = std::shared_ptr<core::IColumn>(
                static_cast<core::IColumn*>(raw),
                [](void* p) { delete static_cast<core::IColumn*>(p); });
            header_.add_column(col_name, col);
        }
    }
    if (!out_streams_.empty()) {
        out_streams_[0]->write(header_);
    }

    // Read first block
    auto cols = column_names_.empty() ? storage_->columns() : column_names_;
    current_block_ = storage_->read(cols, max_block_size_);
    if (current_block_.empty()) {
        finished_ = true;
    }
}

auto ScanProcessor::inputs() const -> std::vector<std::shared_ptr<IInputStream>> {
    return {};
}
auto ScanProcessor::outputs() const -> std::vector<std::shared_ptr<IOutputStream>> {
    std::vector<std::shared_ptr<IOutputStream>> result;
    result.reserve(out_streams_.size());
    for (const auto& stream : out_streams_) {
        result.push_back(stream);
    }
    return result;
}

// ── FilterProcessor ──

FilterProcessor::FilterProcessor(std::shared_ptr<IInputStream> input,
                                 std::function<bool(const core::Field&)> predicate)
    : input_(std::move(input)),
      predicate_(std::move(predicate)) {
    in_stream_ = std::make_shared<NoOpInputStream>();
    out_stream_ = std::make_shared<NoOpOutputStream>();
}

auto FilterProcessor::is_finished() const -> bool { return finished_; }
auto FilterProcessor::getHeader() const -> core::Block { return header_; }

void FilterProcessor::start() {
    if (finished_) return;

    header_ = input_->getHeader();
    in_stream_->set_header(header_);
    out_stream_->write(header_);

    current_block_ = input_->read();
    current_row_ = 0;

    if (!current_block_.empty()) {
        size_t original_rows = current_block_.row_count();
        core::Block filtered;

        for (size_t col_idx = 0; col_idx < current_block_.column_count(); ++col_idx) {
            auto col_name = current_block_.column_names()[col_idx];
            auto col = current_block_.get_column(col_name);
            if (!col) continue;

            auto new_col = col->clone();
            new_col->clear();

            for (size_t i = 0; i < original_rows; ++i) {
                auto field = current_block_.get_row_value(col_idx, i);
                if (predicate_(field)) {
                    new_col->insert(field);
                }
            }
            filtered.add_column(col_name, new_col);
        }
        current_block_ = std::move(filtered);
        if (current_block_.empty()) {
            finished_ = true;
        }
    }
}

auto FilterProcessor::inputs() const -> std::vector<std::shared_ptr<IInputStream>> {
    return {in_stream_};
}
auto FilterProcessor::outputs() const -> std::vector<std::shared_ptr<IOutputStream>> {
    return {std::static_pointer_cast<IOutputStream>(out_stream_)};
}

// ── ProjectProcessor ──

ProjectProcessor::ProjectProcessor(std::shared_ptr<IInputStream> input,
                                   std::vector<std::string> column_names)
    : input_(std::move(input)),
      column_names_(std::move(column_names)) {
    in_stream_ = std::make_shared<NoOpInputStream>();
    out_stream_ = std::make_shared<NoOpOutputStream>();
}

auto ProjectProcessor::is_finished() const -> bool { return finished_; }
auto ProjectProcessor::getHeader() const -> core::Block { return header_; }

void ProjectProcessor::start() {
    if (finished_) return;

    header_ = input_->getHeader();
    in_stream_->set_header(header_);
    out_stream_->write(header_);

    current_block_ = input_->read();
    current_row_ = 0;

    core::Block projected;
    for (auto& col_name : column_names_) {
        auto col = current_block_.get_column(col_name);
        if (col) {
            projected.add_column(col_name, col->clone());
        }
    }
    current_block_ = std::move(projected);
}

auto ProjectProcessor::inputs() const -> std::vector<std::shared_ptr<IInputStream>> {
    return {in_stream_};
}
auto ProjectProcessor::outputs() const -> std::vector<std::shared_ptr<IOutputStream>> {
    return {std::static_pointer_cast<IOutputStream>(out_stream_)};
}

// ── GroupByProcessor ──

GroupByProcessor::GroupByProcessor(std::shared_ptr<IInputStream> input,
                                   std::vector<std::string> group_by_columns,
                                   std::vector<std::pair<std::string, std::vector<std::string>>> aggregates)
    : input_(std::move(input)),
      group_by_columns_(std::move(group_by_columns)),
      aggregates_(std::move(aggregates)) {
    in_stream_ = std::make_shared<NoOpInputStream>();
    out_stream_ = std::make_shared<NoOpOutputStream>();
}

auto GroupByProcessor::is_finished() const -> bool { return finished_; }
auto GroupByProcessor::getHeader() const -> core::Block { return header_; }

void GroupByProcessor::start() {
    if (finished_) return;

    header_ = input_->getHeader();
    in_stream_->set_header(header_);

    core::Block all_data;
    while (!input_->is_finished()) {
        auto block = input_->read();
        if (!block.empty()) {
            if (all_data.empty()) {
                all_data = block.clone();
            } else {
                for (auto& col_name : block.column_names()) {
                    auto col = block.get_column(col_name);
                    if (col) {
                        auto existing_col = all_data.get_column(col_name);
                        if (existing_col) {
                            for (size_t i = 0; i < col->size(); ++i) {
                                existing_col->insert(col->get(i));
                            }
                        }
                    }
                }
            }
        }
    }

    if (all_data.empty()) {
        finished_ = true;
        return;
    }

    // Group by columns
    struct GroupKey {
        std::vector<core::Field> fields;
        bool operator<(const GroupKey& other) const {
            return fields < other.fields;
        }
    };
    std::map<GroupKey, std::vector<size_t>> groups;

    for (size_t row = 0; row < all_data.row_count(); ++row) {
        GroupKey key;
        for (auto& col_name : group_by_columns_) {
            auto col_idx = all_data.column_indices()[col_name];
            key.fields.push_back(all_data.get_row_value(col_idx, row));
        }
        groups[key].push_back(row);
    }

    // Build result block
    core::Block result;
    for (auto& col_name : group_by_columns_) {
        auto col = all_data.get_column(col_name)->clone_empty();
        result.add_column(col_name, col);
    }
    for (auto& [agg_name, agg_cols] : aggregates_) {
        auto col = std::make_shared<columns::ColumnVector<int64_t>>();
        result.add_column(agg_name, col);
    }

    // Compute aggregates
    size_t result_row = 0;
    for (auto& [key, rows] : groups) {
        for (size_t col_idx = 0; col_idx < group_by_columns_.size(); ++col_idx) {
            result.get_column_by_index(col_idx)->insert(key.fields[col_idx]);
        }
        for (size_t agg_idx = 0; agg_idx < aggregates_.size(); ++agg_idx) {
            auto& [agg_name, agg_cols] = aggregates_[agg_idx];
            auto col = result.get_column_by_index(group_by_columns_.size() + agg_idx);
            if (agg_cols.empty()) {
                col->insert(core::Field(static_cast<int64_t>(rows.size())));
            } else {
                auto sum_col = all_data.get_column(agg_cols[0]);
                if (sum_col) {
                    int64_t sum = 0;
                    for (auto row : rows) {
                        auto field = sum_col->get(row);
                        std::visit([&sum](auto&& v) {
                            using T = std::decay_t<decltype(v)>;
                            if constexpr (std::is_integral_v<T>) sum += v;
                            else if constexpr (std::is_floating_point_v<T>) sum += static_cast<int64_t>(v);
                        }, field.variant());
                    }
                    col->insert(core::Field(sum));
                }
            }
        }
        ++result_row;
    }

    current_block_ = std::move(result);
    finished_ = true;
    out_stream_->write(header_);
}

auto GroupByProcessor::inputs() const -> std::vector<std::shared_ptr<IInputStream>> {
    return {in_stream_};
}
auto GroupByProcessor::outputs() const -> std::vector<std::shared_ptr<IOutputStream>> {
    return {std::static_pointer_cast<IOutputStream>(out_stream_)};
}

// ── SortProcessor ──

SortProcessor::SortProcessor(std::shared_ptr<IInputStream> input,
                             std::vector<std::string> order_by_columns,
                             std::vector<bool> descending)
    : input_(std::move(input)),
      order_by_columns_(std::move(order_by_columns)),
      descending_(std::move(descending)) {
    in_stream_ = std::make_shared<NoOpInputStream>();
    out_stream_ = std::make_shared<NoOpOutputStream>();
}

auto SortProcessor::is_finished() const -> bool { return finished_; }
auto SortProcessor::getHeader() const -> core::Block { return header_; }

void SortProcessor::start() {
    if (finished_) return;

    header_ = input_->getHeader();
    in_stream_->set_header(header_);

    core::Block all_data;
    while (!input_->is_finished()) {
        auto block = input_->read();
        if (!block.empty()) {
            if (all_data.empty()) {
                all_data = block.clone();
            } else {
                for (auto& col_name : block.column_names()) {
                    auto col = block.get_column(col_name);
                    if (col) {
                        auto existing_col = all_data.get_column(col_name);
                        if (existing_col) {
                            for (size_t i = 0; i < col->size(); ++i) {
                                existing_col->insert(col->get(i));
                            }
                        }
                    }
                }
            }
        }
    }

    if (all_data.empty()) {
        finished_ = true;
        return;
    }

    std::vector<size_t> indices(all_data.row_count());
    std::iota(indices.begin(), indices.end(), 0);

    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        for (size_t i = 0; i < order_by_columns_.size(); ++i) {
            auto col = all_data.get_column(order_by_columns_[i]);
            if (!col) continue;

            auto val_a = col->get(a);
            auto val_b = col->get(b);

            bool cmp = val_a < val_b;

            if (descending_[i]) cmp = !cmp;
            if (cmp) return true;
            if (!cmp) return false;
        }
        return a < b;
    });

    result_block_ = all_data.clone();
    for (auto& col_name : all_data.column_names()) {
        auto col = all_data.get_column(col_name);
        auto sorted_col = col->clone_empty();
        for (auto idx : indices) {
            sorted_col->insert(col->get(idx));
        }
        result_block_.erase_column(col_name);
        result_block_.add_column(col_name, sorted_col);
    }

    current_block_ = std::move(result_block_);
    finished_ = true;
    out_stream_->write(header_);
}

auto SortProcessor::inputs() const -> std::vector<std::shared_ptr<IInputStream>> {
    return {in_stream_};
}
auto SortProcessor::outputs() const -> std::vector<std::shared_ptr<IOutputStream>> {
    return {std::static_pointer_cast<IOutputStream>(out_stream_)};
}

// ── LimitProcessor ──

LimitProcessor::LimitProcessor(std::shared_ptr<IInputStream> input,
                               size_t limit,
                               size_t offset)
    : input_(std::move(input)),
      limit_(limit),
      offset_(offset) {
    in_stream_ = std::make_shared<NoOpInputStream>();
    out_stream_ = std::make_shared<NoOpOutputStream>();
}

auto LimitProcessor::is_finished() const -> bool { return finished_; }
auto LimitProcessor::getHeader() const -> core::Block { return header_; }

void LimitProcessor::start() {
    if (finished_) return;

    header_ = input_->getHeader();
    in_stream_->set_header(header_);

    core::Block all_data;
    while (!input_->is_finished()) {
        auto block = input_->read();
        if (!block.empty()) {
            if (all_data.empty()) {
                all_data = block.clone();
            } else {
                for (auto& col_name : block.column_names()) {
                    auto col = block.get_column(col_name);
                    if (col) {
                        auto existing_col = all_data.get_column(col_name);
                        if (existing_col) {
                            for (size_t i = 0; i < col->size(); ++i) {
                                existing_col->insert(col->get(i));
                            }
                        }
                    }
                }
            }
        }
    }

    if (all_data.empty()) {
        finished_ = true;
        return;
    }

    size_t start_row = std::min(offset_, all_data.row_count());
    size_t end_row = std::min(start_row + limit_, all_data.row_count());

    if (start_row >= end_row) {
        finished_ = true;
        return;
    }

    current_block_ = all_data.clone();
    for (auto& col_name : all_data.column_names()) {
        auto col = all_data.get_column(col_name);
        auto limited_col = col->clone_empty();
        for (size_t i = start_row; i < end_row; ++i) {
            limited_col->insert(col->get(i));
        }
        current_block_.erase_column(col_name);
        current_block_.add_column(col_name, limited_col);
    }

    finished_ = true;
    out_stream_->write(header_);
}

auto LimitProcessor::inputs() const -> std::vector<std::shared_ptr<IInputStream>> {
    return {in_stream_};
}
auto LimitProcessor::outputs() const -> std::vector<std::shared_ptr<IOutputStream>> {
    return {std::static_pointer_cast<IOutputStream>(out_stream_)};
}

// ── ShowProcessor ──

ShowProcessor::ShowProcessor(std::string show_type, interpreters::Context& context)
    : show_type_(std::move(show_type)), context_(context) {}

auto ShowProcessor::getHeader() const -> core::Block { return result_data_; }

void ShowProcessor::start() {
    if (finished_) return;

    // Build result block based on show_type
    if (show_type_ == "DATABASES") {
        // Get list of databases from context
        auto db_names = context_.databases();

        // Create a column for database names
        auto string_type = datatypes::get_data_type("String");
        auto* raw_col = string_type->create_column();
        auto col = std::shared_ptr<core::IColumn>(
            static_cast<core::IColumn*>(raw_col),
            [](void* p) { delete static_cast<core::IColumn*>(p); });

        // Add database names to the column
        for (const auto& db_name : db_names) {
            col->insert(core::Field(std::string(db_name)));
        }

        // Build result block
        result_data_.add_column("name", col);

    } else if (show_type_ == "TABLES") {
        // Get current database
        auto current_db = context_.current_database();
        auto db = context_.get_database(current_db);

        if (db) {
            // Get list of tables from database
            auto table_names = db->tables();

            // Create a column for table names
            auto string_type = datatypes::get_data_type("String");
            auto* raw_col = string_type->create_column();
            auto col = std::shared_ptr<core::IColumn>(
                static_cast<core::IColumn*>(raw_col),
                [](void* p) { delete static_cast<core::IColumn*>(p); });

            // Add table names to the column
            for (const auto& table_name : table_names) {
                col->insert(core::Field(std::string(table_name)));
            }

            // Build result block
            result_data_.add_column("name", col);
        }
    }

    finished_ = true;
}

auto ShowProcessor::result() const -> std::optional<core::Block> {
    if (finished_) {
        return result_data_;
    }
    return std::nullopt;
}

// ── CreateProcessor ──

CreateProcessor::CreateProcessor(std::string name, std::vector<std::string> columns, bool is_database, interpreters::Context& context)
    : name_(std::move(name)), columns_(std::move(columns)), is_database_(is_database), context_(context) {}

auto CreateProcessor::getHeader() const -> core::Block { return result_data_; }

void CreateProcessor::start() {
    if (finished_) return;

    if (is_database_) {
        // CREATE DATABASE
        auto db = databases::DatabaseMemory::create(name_, "");
        context_.register_database(name_, db);

        // Build success result
        auto string_type = datatypes::get_data_type("String");
        auto* raw_col = string_type->create_column();
        auto col = std::shared_ptr<core::IColumn>(
            static_cast<core::IColumn*>(raw_col),
            [](void* p) { delete static_cast<core::IColumn*>(p); });

        col->insert(core::Field(std::string{"OK"}));
        result_data_.add_column("result", col);

    } else {
        // CREATE TABLE (not implemented yet for this use case)
        auto string_type = datatypes::get_data_type("String");
        auto* raw_col = string_type->create_column();
        auto col = std::shared_ptr<core::IColumn>(
            static_cast<core::IColumn*>(raw_col),
            [](void* p) { delete static_cast<core::IColumn*>(p); });

        col->insert(core::Field(std::string{"OK"}));
        result_data_.add_column("result", col);
    }

    finished_ = true;
}

auto CreateProcessor::result() const -> std::optional<core::Block> {
    if (finished_) {
        return result_data_;
    }
    return std::nullopt;
}

// ── UseProcessor ──

UseProcessor::UseProcessor(std::string database_name, interpreters::Context& context)
    : database_name_(std::move(database_name)), context_(context) {}

auto UseProcessor::getHeader() const -> core::Block { return result_data_; }

void UseProcessor::start() {
    if (finished_) return;

    // Fail loudly if the target database is unknown rather than silently
    // switching to a non-existent context (no-fallback principle).
    if (!context_.get_database(database_name_)) {
        throw common::Exception{
            "Unknown database: " + database_name_,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    context_.set_current_database(database_name_);

    // USE produces no tabular output — return a single-cell acknowledgement
    // so callers that inspect the result block see a success marker.
    auto string_type = datatypes::get_data_type("String");
    auto* raw_col = string_type->create_column();
    auto col = std::shared_ptr<core::IColumn>(
        static_cast<core::IColumn*>(raw_col),
        [](void* p) { delete static_cast<core::IColumn*>(p); });

    col->insert(core::Field(std::string{"OK"}));
    result_data_.add_column("result", col);

    finished_ = true;
}

auto UseProcessor::result() const -> std::optional<core::Block> {
    if (finished_) {
        return result_data_;
    }
    return std::nullopt;
}

// ── InsertProcessor ──

InsertProcessor::InsertProcessor(std::string table,
                                 std::vector<std::string> columns,
                                 std::vector<std::vector<std::string>> values,
                                 interpreters::Context& context)
    : table_(std::move(table)), columns_(std::move(columns)), values_(std::move(values)), context_(context) {}

auto InsertProcessor::getHeader() const -> core::Block { return result_data_; }

void InsertProcessor::start() {
    if (finished_) return;

    // Get storage for the target table
    auto storage = context_.get_storage(table_);
    if (!storage) {
        finished_ = true;
        return;
    }

    // Build a block from the values
    core::Block block;
    for (size_t i = 0; i < columns_.size(); ++i) {
        const auto& col_name = columns_[i];
        auto col_type = storage->column_types().at(col_name);
        auto* raw_col = col_type->create_column();
        auto col = std::shared_ptr<core::IColumn>(
            static_cast<core::IColumn*>(raw_col),
            [](void* p) { delete static_cast<core::IColumn*>(p); });

        for (const auto& row : values_) {
            if (i < row.size()) {
                // Parse the string value based on column type
                auto val_str = row[i];
                // Remove quotes if present
                if (val_str.size() >= 2 && val_str.front() == '\'' && val_str.back() == '\'') {
                    val_str = val_str.substr(1, val_str.size() - 2);
                }
                col->insert(core::Field(val_str));
            } else {
                col->insert_default();
            }
        }

        block.add_column(col_name, col);
    }

    // Write to storage
    if (block.column_count() > 0) {
        storage->write(block);
    }

    // Build result
    auto string_type = datatypes::get_data_type("String");
    auto* raw_col = string_type->create_column();
    auto col = std::shared_ptr<core::IColumn>(
        static_cast<core::IColumn*>(raw_col),
        [](void* p) { delete static_cast<core::IColumn*>(p); });

    col->insert(core::Field(std::string{"OK"}));
    result_data_.add_column("result", col);

    finished_ = true;
}

auto InsertProcessor::result() const -> std::optional<core::Block> {
    if (finished_) {
        return result_data_;
    }
    return std::nullopt;
}

// ── DropProcessor ──

DropProcessor::DropProcessor(std::string table_name, interpreters::Context& context)
    : table_name_(std::move(table_name)), context_(context) {}

auto DropProcessor::getHeader() const -> core::Block { return header_; }

void DropProcessor::start() {
    if (finished_) return;

    // Get current database
    auto current_db = context_.current_database();
    auto db = context_.get_database(current_db);

    if (db) {
        db->drop_table(table_name_);
    }

    // Build result
    auto string_type = datatypes::get_data_type("String");
    auto* raw_col = string_type->create_column();
    auto col = std::shared_ptr<core::IColumn>(
        static_cast<core::IColumn*>(raw_col),
        [](void* p) { delete static_cast<core::IColumn*>(p); });

    col->insert(core::Field(std::string{"OK"}));
    result_data_.add_column("result", col);

    header_ = result_data_.clone();
    header_.reset();
    finished_ = true;
}

auto DropProcessor::result() const -> std::optional<core::Block> {
    if (finished_) {
        return result_data_;
    }
    return std::nullopt;
}

// ── DescribeProcessor ──

DescribeProcessor::DescribeProcessor(std::string table_name, interpreters::Context& context)
    : table_name_(std::move(table_name)), context_(context) {}

auto DescribeProcessor::getHeader() const -> core::Block { return header_; }

void DescribeProcessor::start() {
    if (finished_) return;

    // Get storage for the table
    auto storage = context_.get_storage(table_name_);
    if (!storage) {
        finished_ = true;
        return;
    }

    // Build result block with column names and types
    auto col_names = storage->columns();
    auto col_types = storage->column_types();

    auto string_type = datatypes::get_data_type("String");
    auto* raw_col_name = string_type->create_column();
    auto* raw_col_type = string_type->create_column();
    auto col_name = std::shared_ptr<core::IColumn>(
        static_cast<core::IColumn*>(raw_col_name),
        [](void* p) { delete static_cast<core::IColumn*>(p); });
    auto col_type = std::shared_ptr<core::IColumn>(
        static_cast<core::IColumn*>(raw_col_type),
        [](void* p) { delete static_cast<core::IColumn*>(p); });

    for (const auto& col : col_names) {
        col_name->insert(core::Field(col));
        col_type->insert(core::Field(col_types[col]->name()));
    }

    result_data_.add_column("name", col_name);
    result_data_.add_column("type", col_type);

    header_ = result_data_.clone();
    header_.reset();
    finished_ = true;
}

auto DescribeProcessor::result() const -> std::optional<core::Block> {
    if (finished_) {
        return result_data_;
    }
    return std::nullopt;
}

// ── ExplainProcessor ──

ExplainProcessor::ExplainProcessor(std::string explain_plan, interpreters::Context& context)
    : explain_plan_(std::move(explain_plan)), context_(context) {}

auto ExplainProcessor::getHeader() const -> core::Block { return header_; }

void ExplainProcessor::start() {
    if (finished_) return;

    // Build result block with the plan as a string
    auto string_type = datatypes::get_data_type("String");
    auto* raw_col = string_type->create_column();
    auto col = std::shared_ptr<core::IColumn>(
        static_cast<core::IColumn*>(raw_col),
        [](void* p) { delete static_cast<core::IColumn*>(p); });

    col->insert(core::Field(explain_plan_));
    result_data_.add_column("plan", col);

    header_ = result_data_.clone();
    header_.reset();
    finished_ = true;
}

auto ExplainProcessor::result() const -> std::optional<core::Block> {
    if (finished_) {
        return result_data_;
    }
    return std::nullopt;
}

// ── Simpler processors for Interpreter (block-based, no streaming) ──

// ── SimpleScanProcessor (simpler version) ──

SimpleScanProcessor::SimpleScanProcessor(std::string table, interpreters::Context& context)
    : table_(std::move(table)), context_(context) {}

auto SimpleScanProcessor::getHeader() const -> core::Block { return header_; }

void SimpleScanProcessor::start() {
    if (finished_) return;

    // Get storage for the table
    auto storage = context_.get_storage(table_);
    if (!storage) {
        finished_ = true;
        return;
    }

    // Read all data from storage — read all columns in one call
    auto all_cols = storage->columns();
    result_data_ = storage->read(all_cols);

    header_ = result_data_.clone();
    header_.reset();
    finished_ = true;
}

auto SimpleScanProcessor::result() const -> std::optional<core::Block> {
    if (finished_) {
        return result_data_;
    }
    return std::nullopt;
}

// ── SimpleFilterProcessor (simpler version) ──

SimpleFilterProcessor::SimpleFilterProcessor(core::Block header, std::string expression)
    : header_(std::move(header)), expression_(std::move(expression)) {}

auto SimpleFilterProcessor::getHeader() const -> core::Block { return header_; }

void SimpleFilterProcessor::start() {
    if (finished_) return;

    // For now, just pass through the header
    // Full implementation would apply the predicate to filter rows
    header_ = header_.clone();
    header_.reset();
    finished_ = true;
}

auto SimpleFilterProcessor::result() const -> std::optional<core::Block> {
    if (finished_) {
        return result_data_;
    }
    return std::nullopt;
}

// ── SimpleProjectProcessor (simpler version) ──

SimpleProjectProcessor::SimpleProjectProcessor(core::Block header, std::vector<std::string> columns)
    : header_(std::move(header)), columns_(std::move(columns)) {}

auto SimpleProjectProcessor::getHeader() const -> core::Block { return header_; }

void SimpleProjectProcessor::start() {
    if (finished_) return;

    // For now, just pass through the header
    // Full implementation would project specific columns
    header_ = header_.clone();
    header_.reset();
    finished_ = true;
}

auto SimpleProjectProcessor::result() const -> std::optional<core::Block> {
    if (finished_) {
        return result_data_;
    }
    return std::nullopt;
}

// ── SimpleGroupByProcessor (simpler version) ──

SimpleGroupByProcessor::SimpleGroupByProcessor(core::Block header)
    : header_(std::move(header)) {}

auto SimpleGroupByProcessor::getHeader() const -> core::Block { return header_; }

void SimpleGroupByProcessor::start() {
    if (finished_) return;

    // For now, just pass through the header
    // Full implementation would perform grouping and aggregation
    header_ = header_.clone();
    header_.reset();
    finished_ = true;
}

auto SimpleGroupByProcessor::result() const -> std::optional<core::Block> {
    if (finished_) {
        return result_data_;
    }
    return std::nullopt;
}

// ── SimpleSortProcessor (simpler version) ──

SimpleSortProcessor::SimpleSortProcessor(core::Block header, std::vector<std::string> order_by)
    : header_(std::move(header)), order_by_(std::move(order_by)) {}

auto SimpleSortProcessor::getHeader() const -> core::Block { return header_; }

void SimpleSortProcessor::start() {
    if (finished_) return;

    // For now, just pass through the header
    // Full implementation would sort the rows
    header_ = header_.clone();
    header_.reset();
    finished_ = true;
}

auto SimpleSortProcessor::result() const -> std::optional<core::Block> {
    if (finished_) {
        return result_data_;
    }
    return std::nullopt;
}

// ── SimpleLimitProcessor (simpler version) ──

SimpleLimitProcessor::SimpleLimitProcessor(core::Block header, size_t offset, size_t limit)
    : header_(std::move(header)), offset_(offset), limit_(limit) {}

auto SimpleLimitProcessor::getHeader() const -> core::Block { return header_; }

void SimpleLimitProcessor::start() {
    if (finished_) return;

    // For now, just pass through the header
    // Full implementation would apply limit and offset
    header_ = header_.clone();
    header_.reset();
    finished_ = true;
}

auto SimpleLimitProcessor::result() const -> std::optional<core::Block> {
    if (finished_) {
        return result_data_;
    }
    return std::nullopt;
}

} // namespace mnemo::processors
