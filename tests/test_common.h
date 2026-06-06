// tests/test_common.h — Unit tests for common utilities
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "Common/settings.h"
#include "Common/types.h"
#include "Common/thread_pool.h"
#include <catch2/catch_all.hpp>

// ── Common tests ──
TEST_CASE("Settings defaults", "[common]") {
    auto settings = mnemo::common::default_settings();
    REQUIRE(settings.names().size() > 0);
}

TEST_CASE("ThreadPool creates threads", "[common]") {
    mnemo::common::ThreadPool pool(4);
    REQUIRE(pool.size() == 4);
    REQUIRE(pool.active_count() == 0);

    auto future = pool.submit([]() {
        // do nothing
    });

    future.get();
    pool.shutdown();
}
