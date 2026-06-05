// tests/test_storages.h — Unit tests for storage engines
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Storages/storage_factory.h"
#include "Storages/file_storage.h"
#include "Storages/memory_storage.h"
#include "Storages/dictionary_storage.h"
#include "Core/block.h"
#include "DataTypes/data_type.h"
#include <catch2/catch_all.hpp>

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
