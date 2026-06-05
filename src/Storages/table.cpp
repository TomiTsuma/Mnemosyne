// src/Storages/table.cpp — Table implementation

#include "Storages/table.h"

namespace mnemo::storages {

Table::Table(std::string name)
    : name_{std::move(name)}
    , storage_{MemoryStorage::create(name_)} {}

void Table::add_column(std::string col_name, datatypes::DataTypePtr type) {
    if (auto mem = std::dynamic_pointer_cast<MemoryStorage>(storage_)) {
        mem->add_column(std::move(col_name), std::move(type));
    }
}

} // namespace mnemo::storages
