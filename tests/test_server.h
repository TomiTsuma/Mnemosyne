// tests/test_server.h — Unit tests for HTTP/TCP servers
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "server/http_handler.h"
#include "server/http_server.h"
#include <catch2/catch_all.hpp>
#include <memory>

// ── Server tests ──
TEST_CASE("HTTPHandler returns 200 on ping", "[server]") {
    mnesso::interpreters::Context context;
    mnesso::server::HTTPHandler handler(context);

    mnesso::server::Request req;
    req.method = "GET";
    req.path = "/ping";

    auto response = handler.handle(req);
    REQUIRE(response.status_code == 200);
}

TEST_CASE("HTTPServer starts", "[server]") {
    mnesso::interpreters::Context context;
    auto handler = std::make_shared<mnesso::server::HTTPHandler>(context);
    mnesso::server::HTTPServer server(handler, 0); // port 0 = random

    server.start();
    REQUIRE(server.is_running());

    server.stop();
    REQUIRE(!server.is_running());
}
