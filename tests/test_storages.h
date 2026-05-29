// tests/test_storages.h — Unit tests for storage engines
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "storages/storage_factory.h"
#include "storages/file_storage.h"
#include "storages/memory_storage.h"
#include "storages/dictionary_storage.h"
#include "core/block.h"
#include "data_types/data_type.h"
#include <catch2/catch_all.hpp>

// ── Storage tests ──
TEST_CASE("StorageFactory singleton", "[storage]") {
    auto& factory = mnesso::storages::StorageFactory::instance();
    auto& factory2 = mnesso::storages::StorageFactory::instance();
    REQUIRE(&factory == &factory2);
}

TEST_CASE("MemoryStorage CRUD", "[storage]") {
    auto storage = mnesso::storages::MemoryStorage::create("test");
    REQUIRE(storage != nullptr);
    REQUIRE(storage->name() == "test");
    REQUIRE(storage->empty());
}

TEST_CASE("DictionaryStorage put/get", "[storage]") {
    auto dict = mnesso::storages::DictionaryStorage::create("test");
    REQUIRE(dict != nullptr);

    auto success = dict->put("key", mnesso::core::Field{123});
    REQUIRE(success);

    auto value = dict->get("key");
    REQUIRE(value.has_value());
}
