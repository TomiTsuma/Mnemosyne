// src/Databases/database.h — Database: collection of tables (implements IDatabase)
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Storages/table.h"
#include "Storages/memory_storage.h"
#include "view_catalog.h"
#include "i_database.h"
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mnemo::databases {

struct ColumnDef {
    std::string name;
    std::string data_type;
};

// Database extends IDatabase so shared_ptr<Database> converts to shared_ptr<IDatabase>
class Database final : public IDatabase {
public:
    explicit Database(std::string name);

    // IDatabase interface
    [[nodiscard]] auto name()   const -> std::string override;
    [[nodiscard]] auto path()   const -> std::string override;
    [[nodiscard]] auto engine() const -> std::string override;

    [[nodiscard]] auto tables() const -> std::vector<std::string> override;
    auto attach_table(std::string name, std::shared_ptr<storages::IStorage> table) -> void override;
    auto detach_table(std::string name) -> std::shared_ptr<storages::IStorage> override;
    auto rename_table(std::string from, std::string to) -> bool override;
    [[nodiscard]] auto table_exists(std::string_view name) const -> bool override;
    [[nodiscard]] auto table(std::string name) -> std::shared_ptr<storages::IStorage> override;
    auto drop_table(std::string name) -> bool override;
    auto create_table(
        std::string name,
        std::unordered_map<std::string, datatypes::DataTypePtr> columns,
        std::string engine = "Memory") -> std::shared_ptr<storages::IStorage> override;

    // Database-specific methods
    auto get_table(std::string_view name) -> std::shared_ptr<storages::Table>;
    auto get_table(std::string_view name) const -> std::shared_ptr<storages::Table>;
    void create_table(std::string name, std::vector<ColumnDef> columns);
    [[nodiscard]] auto table_names() const -> std::vector<std::string>;
    [[nodiscard]] auto table_count() const -> size_t;
    [[nodiscard]] auto has_table(std::string_view name) const -> bool;

    // View catalog
    [[nodiscard]] auto has_view(std::string_view name) const -> bool;
    [[nodiscard]] auto has_materialized_view(std::string_view name) const -> bool;
    [[nodiscard]] auto get_view(std::string_view name) const -> std::optional<ViewEntry>;
    [[nodiscard]] auto get_materialized_view(std::string_view name) const
        -> std::optional<MaterializedViewEntry>;
    [[nodiscard]] auto view_names() const -> std::vector<std::string>;
    [[nodiscard]] auto materialized_view_names() const -> std::vector<std::string>;
    auto create_view(ViewEntry entry) -> void;
    auto drop_view(std::string name) -> bool;
    auto create_materialized_view(MaterializedViewEntry entry) -> void;
    auto drop_materialized_view(std::string name) -> bool;
    [[nodiscard]] auto relation_exists(std::string_view name) const -> bool;

private:
    std::string name_;
    std::string path_ = "";
    std::string engine_ = "Memory";
    std::unordered_map<std::string, std::shared_ptr<storages::Table>> tables_;
    std::unordered_map<std::string, ViewEntry> views_;
    std::unordered_map<std::string, MaterializedViewEntry> materialized_views_;
};

} // namespace mnemo::databases
