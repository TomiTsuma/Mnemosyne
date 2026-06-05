// tests/test_backups.h — Unit tests for backup manager
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "backups/backup.h"
#include <catch2/catch_all.hpp>

// ── Backup tests ──
TEST_CASE("BackupManager creates backup", "[backup]") {
    auto backup = mnemo::backups::Backup::create("test", "/tmp/mnemosyne_backups");
    REQUIRE(backup != nullptr);

    auto list = backup->list();
    REQUIRE(list.empty()); // no backups yet
}
