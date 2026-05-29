// tests/test_server.h — Unit tests for HTTP/TCP servers
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "server/server.h"
#include "server/http_handler.h"
#include "server/http_server.h"
#include "server/tcp_server.h"
#include <catch2/catch_all.hpp>

// ── Server tests ──
TEST_CASE("HTTPHandler returns 200 on ping", "[server]") {
    mnesso::interpreters::Context context;
    mnesso::server::HTTPHandler handler(context);

    auto response = handler.handle("GET", "/ping", "");
    REQUIRE(response.status_code == 200);
}

TEST_CASE("HTTPServer starts", "[server]") {
    mnesso::interpreters::Context context;
    mnesso::server::HTTPServer server(context, "127.0.0.1", 0); // port 0 = random

    auto started = server.start();
    REQUIRE(started);

    auto stopped = server.stop();
    REQUIRE(stopped);
}
