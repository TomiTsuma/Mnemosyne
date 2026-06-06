// src/Connectors/rest_connector.cpp

#include "Connectors/rest_connector.h"
#include "Columns/column_string.h"
#include "Common/exceptions.h"
#include <httplib.h>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <sstream>

namespace mnemo::connectors {

namespace {

auto property(const ConnectorEntry& entry, std::string_view key) -> std::string {
    const auto it = entry.properties.find(std::string{key});
    return it != entry.properties.end() ? it->second : std::string{};
}

auto join_url(std::string base, std::string_view resource) -> std::string {
    if (!base.empty() && base.back() == '/') {
        base.pop_back();
    }
    std::string path{resource};
    if (!path.empty() && path.front() != '/') {
        path = "/" + path;
    }
    return base + path;
}

void skip_ws(std::string_view body, size_t& i) {
    while (i < body.size() && std::isspace(static_cast<unsigned char>(body[i]))) {
        ++i;
    }
}

auto parse_json_string(std::string_view body, size_t& i) -> std::string {
    if (i >= body.size() || body[i] != '"') {
        throw common::Exception{
            "REST connector: expected string in JSON",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    ++i;
    std::string out;
    while (i < body.size()) {
        const char c = body[i++];
        if (c == '"') break;
        if (c == '\\' && i < body.size()) {
            out.push_back(body[i++]);
        } else {
            out.push_back(c);
        }
    }
    return out;
}

auto parse_json_value(std::string_view body, size_t& i) -> std::string {
    skip_ws(body, i);
    if (i >= body.size()) return "";
    if (body[i] == '"') {
        return parse_json_string(body, i);
    }
    size_t start = i;
    while (i < body.size() && body[i] != ',' && body[i] != '}' && body[i] != ']') {
        ++i;
    }
    std::string raw(body.substr(start, i - start));
    while (!raw.empty() && std::isspace(static_cast<unsigned char>(raw.back()))) {
        raw.pop_back();
    }
    return raw;
}

auto parse_json_object_keys(std::string_view body) -> std::vector<std::string> {
    std::vector<std::string> keys;
    size_t i = 0;
    skip_ws(body, i);
    if (i >= body.size() || body[i] != '{') return keys;
    ++i;
    while (i < body.size()) {
        skip_ws(body, i);
        if (i < body.size() && body[i] == '}') break;
        if (body[i] != '"') break;
        const auto key = parse_json_string(body, i);
        skip_ws(body, i);
        if (i < body.size() && body[i] == ':') ++i;
        parse_json_value(body, i);
        keys.push_back(key);
        skip_ws(body, i);
        if (i < body.size() && body[i] == ',') ++i;
    }
    return keys;
}

auto parse_json_array_of_objects(std::string_view body)
    -> std::vector<std::unordered_map<std::string, std::string>> {
    std::vector<std::unordered_map<std::string, std::string>> rows;
    size_t i = 0;
    skip_ws(body, i);
    if (i >= body.size() || body[i] != '[') {
        throw common::Exception{
            "REST connector: expected JSON array response",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    ++i;
    while (i < body.size()) {
        skip_ws(body, i);
        if (i < body.size() && body[i] == ']') break;
        if (body[i] != '{') {
            throw common::Exception{
                "REST connector: expected JSON object in array",
                static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
        }
        ++i;
        std::unordered_map<std::string, std::string> row;
        while (i < body.size()) {
            skip_ws(body, i);
            if (i < body.size() && body[i] == '}') {
                ++i;
                break;
            }
            const auto key = parse_json_string(body, i);
            skip_ws(body, i);
            if (i < body.size() && body[i] == ':') ++i;
            row[key] = parse_json_value(body, i);
            skip_ws(body, i);
            if (i < body.size() && body[i] == ',') ++i;
        }
        rows.push_back(std::move(row));
        skip_ws(body, i);
        if (i < body.size() && body[i] == ',') ++i;
    }
    return rows;
}

auto split_csv(std::string_view value) -> std::vector<std::string> {
    std::vector<std::string> parts;
    std::string current;
    for (const char c : value) {
        if (c == ',') {
            if (!current.empty()) parts.push_back(current);
            current.clear();
        } else if (!std::isspace(static_cast<unsigned char>(c))) {
            current.push_back(c);
        }
    }
    if (!current.empty()) parts.push_back(current);
    return parts;
}

void apply_auth(httplib::Client& client, const ConnectorEntry& entry) {
    if (entry.auth == AuthMethod::ApiKey) {
        const auto key = property(entry, "API_KEY");
        if (!key.empty()) {
            client.set_default_headers({{"X-API-Key", key}});
        }
    }
}

struct UrlParts {
    std::string host;
    int port = 80;
    std::string path = "/";
};

auto parse_http_url(const std::string& url) -> UrlParts {
    UrlParts parts;
    const auto scheme_end = url.find("://");
    if (scheme_end == std::string::npos) {
        throw common::Exception{
            "REST connector: invalid URL",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    parts.port = url.rfind("https://", 0) == 0 ? 443 : 80;
    const auto host_start = scheme_end + 3;
    const auto path_start = url.find('/', host_start);
    std::string authority = path_start == std::string::npos
        ? url.substr(host_start)
        : url.substr(host_start, path_start - host_start);
    if (path_start != std::string::npos) {
        parts.path = url.substr(path_start);
        if (parts.path.empty()) parts.path = "/";
    }
    const auto port_sep = authority.find(':');
    if (port_sep != std::string::npos) {
        parts.host = authority.substr(0, port_sep);
        parts.port = std::stoi(authority.substr(port_sep + 1));
    } else {
        parts.host = std::move(authority);
    }
    return parts;
}

auto http_get(const ConnectorEntry& entry, const std::string& url) -> std::string {
    const auto parts = parse_http_url(url);
    httplib::Client client(parts.host, parts.port);
    client.set_connection_timeout(5, 0);
    client.set_read_timeout(5, 0);
    apply_auth(client, entry);

    const auto res = client.Get(parts.path);
    if (!res) {
        throw common::Exception{
            "REST connector: HTTP request failed",
            static_cast<int>(common::ErrorCode::CANNOT_CONNECT)};
    }
    if (res->status < 200 || res->status >= 300) {
        throw common::Exception{
            "REST connector: HTTP " + std::to_string(res->status),
            static_cast<int>(common::ErrorCode::NET_ERROR)};
    }
    return res->body;
}

auto rows_to_block(const std::vector<std::unordered_map<std::string, std::string>>& rows,
                   size_t limit) -> core::Block {
    core::Block block;
    if (rows.empty()) return block;

    std::vector<std::string> columns;
    for (const auto& [key, _] : rows.front()) {
        columns.push_back(key);
    }
    std::sort(columns.begin(), columns.end());

    std::vector<std::shared_ptr<columns::ColumnString>> col_ptrs;
    col_ptrs.reserve(columns.size());
    for (const auto& name : columns) {
        col_ptrs.push_back(std::make_shared<columns::ColumnString>());
        block.add_column(name, col_ptrs.back());
    }

    const size_t n = limit > 0 ? std::min(limit, rows.size()) : rows.size();
    for (size_t r = 0; r < n; ++r) {
        for (size_t c = 0; c < columns.size(); ++c) {
            const auto it = rows[r].find(columns[c]);
            const std::string value = it != rows[r].end() ? it->second : "";
            col_ptrs[c]->insert_at(r, core::Field(value));
        }
    }
    return block;
}

} // namespace

auto RestConnector::test(ConnectorEntry& entry) -> ConnectorTestResult {
    (void)entry;
    ConnectorTestResult result;
    const auto started = std::chrono::steady_clock::now();
    try {
        (void)http_get(entry, property(entry, "BASE_URL"));
        result.ok = true;
        result.message = "Connection successful";
    } catch (const common::Exception& e) {
        result.ok = false;
        result.message = e.what();
    }
    const auto ended = std::chrono::steady_clock::now();
    result.latency_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(ended - started).count());
    result.tested_at = std::chrono::system_clock::now();
    return result;
}

auto RestConnector::discover_schema(const ConnectorEntry& entry) -> std::vector<std::string> {
    if (const auto schema = property(entry, "SCHEMA"); !schema.empty()) {
        return split_csv(schema);
    }
    if (const auto resources = property(entry, "RESOURCES"); !resources.empty()) {
        return split_csv(resources);
    }
    try {
        const auto body = http_get(entry, property(entry, "BASE_URL"));
        if (!body.empty() && body.front() == '{') {
            return parse_json_object_keys(body);
        }
    } catch (...) {
    }
    throw common::Exception{
        "REST connector: unable to discover schema; set SCHEMA or RESOURCES property",
        static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
}

auto RestConnector::read(const ConnectorEntry& entry, std::string_view resource,
                         size_t limit) -> core::Block {
    const auto url = join_url(property(entry, "BASE_URL"), resource);
    const auto body = http_get(entry, url);
    const auto rows = parse_json_array_of_objects(body);
    return rows_to_block(rows, limit);
}

} // namespace mnemo::connectors
