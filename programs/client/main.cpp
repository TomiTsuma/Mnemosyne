// programs/client/main.cpp — Mnemosyne REPL client
// Mnemosyne: A column-oriented analytical DBMS

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <format>
#include <fstream>
#include <chrono>
#include <thread>
#include <csignal>

#include "parsers/lexer.h"
#include "parsers/parser_query.h"
#include "Core/block.h"
#include "Core/series.h"
#include "common/settings.h"
#include "loggers/logger.h"
#include "interpreters/interpreter.h"
#include "server/server.h"

// ── REPL client ──
class MnemosyneClient {
public:
    MnemosyneClient(std::string host = "127.0.0.1",
                    uint16_t port = 8123)
        : host_(host), port_(port) {}

    auto connect() -> bool {
        // TODO: HTTP client implementation
        return true;
    }

    auto execute(std::string_view query) -> mnesso::core::Block {
        // TODO: send query to server
        return mnesso::core::Block{};
    }

    auto show_help() const {
        std::cout << "Mnemosyne CLI\n";
        std::cout << "Commands:\n";
        std::cout << "  \\\\help     - Show help\n";
        std::cout << "  \\\\quit     - Quit\n";
        std::cout << "  \\\\tables   - Show tables\n";
        std::cout << "  \\\\databases - Show databases\n";
        std::cout << "  \\\\settings - Show settings\n";
        std::cout << "  \\\\kill     - Kill query\n";
    }

    auto run() {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;
            if (line == "\\help") {
                show_help();
            } else if (line == "\\quit") {
                break;
            } else {
                execute(line);
            }
        }
    }

private:
    std::string host_;
    uint16_t    port_;
};

auto main(int argc, char** argv) -> int {
    try {
        std::string host = "127.0.0.1";
        uint16_t port = 8123;

        if (argc > 1) host = argv[1];
        if (argc > 2) port = static_cast<uint16_t>(std::stoi(argv[2]));

        MnemosyneClient client(host, port);
        client.connect();
        client.run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
