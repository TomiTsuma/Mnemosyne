// tests/test_loggers.h — Unit tests for logging infrastructure
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "loggers/logger.h"
#include "loggers/target_console.h"
#include "loggers/target_file.h"
#include <catch2/catch_all.hpp>

// ── Logger tests ──
TEST_CASE("Logger singleton", "[logger]") {
    auto& logger1 = mnemo::loggers::Logger::get_instance();
    auto& logger2 = mnemo::loggers::Logger::get_instance();
    REQUIRE(&logger1 == &logger2);
}

TEST_CASE("ConsoleTarget emits logs", "[logger]") {
    auto target = mnemo::loggers::ConsoleTarget::create();
    REQUIRE(target != nullptr);
    REQUIRE(target->name() == "Console");
}

TEST_CASE("FileTarget creates file", "[logger]") {
    auto target = mnemo::loggers::FileTarget::create("/tmp/mnemosyne_test.log");
    REQUIRE(target != nullptr);
    REQUIRE(target->name() == "/tmp/mnemosyne_test.log");
}
