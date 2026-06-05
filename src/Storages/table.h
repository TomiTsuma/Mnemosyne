// src/Storages/table.h — Table: named collection of columns backed by storage
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Storages/i_storage.h"
#include "Storages/memory_storage.h"
#include "DataTypes/data_type.h"
#include <memory>
#include <string>

namespace mnemo::storages {

class Table {
public:
    explicit Table(std::string name);

    [[nodiscard]] auto name() const -> const std::string& { return name_; }
    [[nodiscard]] auto storage() const -> std::shared_ptr<IStorage> { return storage_; }

    void add_column(std::string col_name, datatypes::DataTypePtr type);

private:
    std::string               name_;
    std::shared_ptr<IStorage> storage_;
};

} // namespace mnemo::storages
