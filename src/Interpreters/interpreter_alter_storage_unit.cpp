// src/Interpreters/interpreter_alter_storage_unit.cpp — ALTER STORAGE_UNIT implementation

#include "Interpreters/interpreter_alter_storage_unit.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "StorageUnits/storage_unit_manager.h"
#include "Common/exceptions.h"
#include <charconv>
#include <cstdint>
#include <optional>

namespace mnemo::interpreters {

namespace {

[[noreturn]] void fail(const std::string& message) {
    throw common::Exception{message, static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
}

// "FOREVER" -> nullopt; otherwise a whole number of days of at least 1.
auto parse_retention_days(const std::string& value) -> std::optional<uint32_t> {
    if (value == "FOREVER") {
        return std::nullopt;
    }
    uint32_t days = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), days);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        fail("RETENTION must be a whole number of days or FOREVER, got '" + value + "'");
    }
    if (days == 0) {
        fail("RETENTION must be at least 1 day; use FOREVER to keep data indefinitely");
    }
    return days;
}

} // namespace

auto InterpreterAlterStorageUnit::execute(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    (void)context;
    const auto& alter = query.alter;

    // Parse every value before changing anything, so a bad statement changes nothing.
    std::optional<std::optional<uint32_t>> retention;
    for (const auto& cmd : alter.storage_unit_sets) {
        if (cmd.property == "RETENTION") {
            retention = parse_retention_days(cmd.value);
        } else {
            fail("Unknown ALTER STORAGE_UNIT property: " + cmd.property);
        }
    }

    auto& manager = storage_units::StorageUnitManager::instance();
    if (!manager.has_unit(alter.table)) {
        throw common::Exception{
            "Unknown storage unit: " + alter.table,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }
    if (retention) {
        manager.set_retention(alter.table, *retention);
    }
    return ddl_utils::make_ok_block();
}

} // namespace mnemo::interpreters
