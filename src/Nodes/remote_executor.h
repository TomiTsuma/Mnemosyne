// src/Nodes/remote_executor.h — Execute SQL on a remote node via HTTP

#pragma once

#include "Core/block.h"
#include <optional>
#include <string>
#include <string_view>

namespace mnemo::nodes {

class RemoteExecutor {
public:
    static auto execute_on_node(std::string_view node_name, std::string_view sql)
        -> std::optional<core::Block>;

    static auto build_node_url(std::string_view host, uint16_t port) -> std::string;
};

} // namespace mnemo::nodes
