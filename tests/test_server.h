// tests/test_server.h — Unit tests for HTTP/TCP servers
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "server/http_handler.h"
#include "server/http_server.h"
#include <catch2/catch_all.hpp>
#include <memory>

// ── Server tests ──
TEST_CASE("HTTPHandler returns 200 on ping", "[server]") {
    mnemo::interpreters::Context context;
    mnemo::server::HTTPHandler handler(context);

    mnemo::server::Request req;
    req.method = "GET";
    req.path = "/ping";

    auto response = handler.handle(req);
    REQUIRE(response.status_code == 200);
}

TEST_CASE("HTTPHandler returns 200 on status without crashing", "[server]") {
    // Regression test: handle_status dereferences context_.pool(). A Context
    // whose pool_ was never initialised would dereference a null unique_ptr
    // here and segfault. The pool must always be valid.
    mnemo::interpreters::Context context;
    mnemo::server::HTTPHandler handler(context);

    mnemo::server::Request req;
    req.method = "GET";
    req.path = "/status";

    auto response = handler.handle(req);
    REQUIRE(response.status_code == 200);
}

TEST_CASE("HTTPServer starts", "[server]") {
    mnemo::interpreters::Context context;
    auto handler = std::make_shared<mnemo::server::HTTPHandler>(context);
    mnemo::server::HTTPServer server(handler, 0); // port 0 = random

    server.start();
    REQUIRE(server.is_running());

    server.stop();
    REQUIRE(!server.is_running());
}
