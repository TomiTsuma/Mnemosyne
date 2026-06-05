// src/Storages/table_metadata.cpp — TableMetadata implementation

#include "Storages/table_metadata.h"

namespace mnemo::storages {

auto TableMetadata::clone() const -> TableMetadata {
    TableMetadata copy;
    copy.version = version;
    copy.engine  = engine;
    copy.columns = columns;
    return copy;
}

auto TableMetadata::has_column(std::string_view name) const -> bool {
    return columns.find(std::string{name}) != columns.end();
}

} // namespace mnemo::storages
