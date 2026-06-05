// tests/test_disks.h — Unit tests for disk backends
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "disks/disk.h"
#include "disks/disk_local.h"
#include "disks/disk_s3.h"
#include <catch2/catch_all.hpp>

// ── Disk tests ──
TEST_CASE("LocalFileDisk stats", "[disk]") {
    auto disk = mnemo::disks::LocalFileDisk::create("test", "/tmp/mnemosyne_test");
    REQUIRE(disk != nullptr);

    auto stats = disk->stats();
    REQUIRE(stats.free_space >= 0);
    REQUIRE(stats.total_space >= 0);
}

TEST_CASE("Disk path operations", "[disk]") {
    auto disk = mnemo::disks::LocalFileDisk::create("test", "/tmp/mnemosyne_test");
    auto result = disk->create_dir("subdir");
    REQUIRE(result);

    auto exists = disk->exists("subdir");
    REQUIRE(exists);
}
