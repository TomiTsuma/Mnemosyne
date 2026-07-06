// tests/test_storages.h — Unit tests for storage engines
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Storages/storage_factory.h"
#include "Storages/file_storage.h"
#include "Storages/memory_storage.h"
#include "Storages/dictionary_storage.h"
#include "Disks/disk_local.h"
#include "Core/block.h"
#include "Columns/column_vector.h"
#include "DataTypes/data_type.h"
#include "DataTypes/data_type_factory.h"
#include <catch2/catch_all.hpp>
#include <filesystem>

// ── Storage tests ──
TEST_CASE("StorageFactory singleton", "[storage]") {
    auto& factory = mnemo::storages::StorageFactory::instance();
    auto& factory2 = mnemo::storages::StorageFactory::instance();
    REQUIRE(&factory == &factory2);
}

TEST_CASE("MemoryStorage CRUD", "[storage]") {
    auto storage = mnemo::storages::MemoryStorage::create("test");
    REQUIRE(storage != nullptr);
    REQUIRE(storage->name() == "test");
    REQUIRE(storage->empty());
}

TEST_CASE("DictionaryStorage put/get", "[storage]") {
    auto dict = mnemo::storages::DictionaryStorage::create("test");
    REQUIRE(dict != nullptr);

    auto success = dict->put("key", mnemo::core::Field{123});
    REQUIRE(success);

    auto value = dict->get("key");
    REQUIRE(value.has_value());
}

TEST_CASE("FileStorage write appends rather than overwriting", "[storage]") {
    namespace fs = std::filesystem;
    auto tmp_root = fs::temp_directory_path() / "mnemo_file_storage_append_test";
    std::error_code ec;
    fs::remove_all(tmp_root, ec);
    fs::create_directories(tmp_root, ec);

    auto disk = mnemo::disks::LocalFileDisk::create("local", tmp_root.string());
    auto storage = mnemo::storages::FileStorage::create("appendtest", "data");
    storage->set_disk(disk);
    storage->set_data_path("data");
    storage->set_columns({
        {"id", mnemo::datatypes::get_data_type("Float64")},
        {"val", mnemo::datatypes::get_data_type("Float64")},
    });

    auto make_block = [](std::vector<double> ids, std::vector<double> vals) {
        mnemo::core::Block block;
        auto id_col = std::make_shared<mnemo::columns::ColumnVector<double>>();
        auto val_col = std::make_shared<mnemo::columns::ColumnVector<double>>();
        for (auto v : ids) id_col->insert(mnemo::core::Field{v});
        for (auto v : vals) val_col->insert(mnemo::core::Field{v});
        block.add_column("id", id_col);
        block.add_column("val", val_col);
        return block;
    };

    storage->write(make_block({1.0, 2.0, 3.0}, {10.5, 20.5, 30.5}));
    REQUIRE(storage->row_count() == 3);

    storage->write(make_block({15.0, 12.0, 33.0}, {10.5, 21.5, 310.53}));
    REQUIRE(storage->row_count() == 6);

    auto result = storage->read({"id", "val"});
    REQUIRE(result.row_count() == 6);

    fs::remove_all(tmp_root, ec);
}
