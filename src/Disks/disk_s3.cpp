// src/Disks/disk_s3.cpp — S3-compatible disk backend
// Mnemosyne: A column-oriented analytical DBMS
//
// Talks to any S3-compatible object store (AWS S3, MinIO, Ceph RGW, ...)
// over plain HTTP(S) using cpp-httplib, authenticating each request with
// hand-rolled AWS Signature V4 (see s3_signer.h/.cpp).

#include "Disks/disk_s3.h"
#include "Disks/s3_signer.h"
#include "Common/exceptions.h"
#include <httplib.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace mnemo::disks {

namespace {

auto portable_timegm(std::tm* tm) -> std::time_t {
#ifdef _WIN32
    return _mkgmtime(tm);
#else
    return timegm(tm);
#endif
}

// Pulls every value out of same-named XML tags, e.g. all <Key>...</Key>
// contents. Good enough for the flat ListObjectsV2 response shape we need —
// mirrors the minimal hand-rolled parsing style already used in
// src/Connectors/rest_connector.cpp for JSON, just for XML instead.
auto extract_xml_tag_values(const std::string& xml, const std::string& tag)
    -> std::vector<std::string> {
    std::vector<std::string> values;
    const std::string open_tag = "<" + tag + ">";
    const std::string close_tag = "</" + tag + ">";
    size_t pos = 0;
    while (true) {
        const auto start = xml.find(open_tag, pos);
        if (start == std::string::npos) break;
        const auto value_start = start + open_tag.size();
        const auto end = xml.find(close_tag, value_start);
        if (end == std::string::npos) break;
        values.push_back(xml.substr(value_start, end - value_start));
        pos = end + close_tag.size();
    }
    return values;
}

auto build_list_query(const std::string& prefix) -> std::string {
    // Keys must appear sorted alphabetically in the canonical query string:
    // delimiter, list-type, prefix — already alphabetical here.
    return "delimiter=" + s3::uri_encode("/", true) +
           "&list-type=2" +
           "&prefix=" + s3::uri_encode(prefix, true);
}

} // namespace

auto S3Disk::create(std::string name,
                    std::string endpoint,
                    std::string bucket,
                    std::string access_key,
                    std::string secret_key,
                    std::string region) -> std::shared_ptr<S3Disk> {
    // Fall back to environment variables when SQL didn't supply credentials
    // (see Part F of the implementation guide for the full rationale).
    if (access_key.empty()) {
        if (const char* env = std::getenv("AWS_ACCESS_KEY_ID")) access_key = env;
    }
    if (secret_key.empty()) {
        if (const char* env = std::getenv("AWS_SECRET_ACCESS_KEY")) secret_key = env;
    }
    if (region.empty()) {
        if (const char* env = std::getenv("AWS_REGION")) {
            region = env;
        } else if (const char* env2 = std::getenv("AWS_DEFAULT_REGION")) {
            region = env2;
        } else {
            region = "us-east-1";
        }
    }

    auto disk = std::shared_ptr<S3Disk>(new S3Disk());
    disk->name_ = std::move(name);
    disk->endpoint_ = std::move(endpoint);
    disk->bucket_ = std::move(bucket);
    disk->access_key_ = std::move(access_key);
    disk->secret_key_ = std::move(secret_key);
    disk->region_ = std::move(region);
    return disk;
}

S3Disk::S3Disk() = default;

auto S3Disk::name() const -> std::string { return name_; }
auto S3Disk::path() const -> std::string { return bucket_; }
auto S3Disk::type() const -> std::string { return "S3"; }

auto S3Disk::host_header() const -> std::string {
    // We send this exact string as our own "Host" header on every request
    // (cpp-httplib only auto-generates one if the caller didn't set it), so
    // it always matches what we signed — no need to replicate cpp-httplib's
    // internal default-port logic.
    std::string rest = endpoint_;
    if (const auto scheme_pos = rest.find("://"); scheme_pos != std::string::npos) {
        rest = rest.substr(scheme_pos + 3);
    }
    if (const auto slash_pos = rest.find('/'); slash_pos != std::string::npos) {
        rest = rest.substr(0, slash_pos);
    }
    return rest;
}

auto S3Disk::object_path(std::string_view key) const -> std::string {
    return "/" + s3::uri_encode(bucket_, /*encode_slash=*/true) +
           "/" + s3::uri_encode(std::string{key}, /*encode_slash=*/false);
}

auto S3Disk::exists(std::string_view path) -> bool {
    const auto uri = object_path(path);
    const auto signed_headers = s3::sign_request(
        "HEAD", uri, "", host_header(), region_, access_key_, secret_key_, "");

    httplib::Client client(endpoint_);
    client.set_connection_timeout(10, 0);
    client.set_read_timeout(30, 0);
    const httplib::Headers headers = {
        {"Host", host_header()},
        {"x-amz-date", signed_headers.amz_date},
        {"x-amz-content-sha256", signed_headers.payload_hash},
        {"Authorization", signed_headers.authorization},
    };

    const auto res = client.Head(uri, headers);
    if (!res) {
        throw common::Exception{
            "S3Disk::exists: request to " + endpoint_ + " failed",
            static_cast<int>(common::ErrorCode::CANNOT_CONNECT)};
    }
    if (res->status == 404) return false;
    if (res->status != 200) {
        throw common::Exception{
            "S3Disk::exists: HEAD " + uri + " returned HTTP " + std::to_string(res->status),
            static_cast<int>(common::ErrorCode::NET_ERROR)};
    }
    return true;
}

auto S3Disk::size(std::string_view path) -> size_t {
    const auto uri = object_path(path);
    const auto signed_headers = s3::sign_request(
        "HEAD", uri, "", host_header(), region_, access_key_, secret_key_, "");

    httplib::Client client(endpoint_);
    client.set_connection_timeout(10, 0);
    client.set_read_timeout(30, 0);
    const httplib::Headers headers = {
        {"Host", host_header()},
        {"x-amz-date", signed_headers.amz_date},
        {"x-amz-content-sha256", signed_headers.payload_hash},
        {"Authorization", signed_headers.authorization},
    };

    const auto res = client.Head(uri, headers);
    if (!res || res->status != 200) return 0;
    const auto content_length = res->get_header_value("Content-Length");
    return content_length.empty() ? 0 : std::stoull(content_length);
}

auto S3Disk::modified_at(std::string_view path)
    -> std::chrono::system_clock::time_point {
    const auto uri = object_path(path);
    const auto signed_headers = s3::sign_request(
        "HEAD", uri, "", host_header(), region_, access_key_, secret_key_, "");

    httplib::Client client(endpoint_);
    const httplib::Headers headers = {
        {"Host", host_header()},
        {"x-amz-date", signed_headers.amz_date},
        {"x-amz-content-sha256", signed_headers.payload_hash},
        {"Authorization", signed_headers.authorization},
    };

    const auto res = client.Head(uri, headers);
    if (!res || res->status != 200) return std::chrono::system_clock::now();

    const auto last_modified = res->get_header_value("Last-Modified");
    if (last_modified.empty()) return std::chrono::system_clock::now();

    std::tm tm{};
    std::istringstream ss(last_modified);
    ss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
    if (ss.fail()) return std::chrono::system_clock::now();
    return std::chrono::system_clock::from_time_t(portable_timegm(&tm));
}

auto S3Disk::read(std::string_view path, size_t offset,
                  size_t size) -> std::shared_ptr<uint8_t[]> {
    const auto uri = object_path(path);
    std::map<std::string, std::string> extra;
    const bool ranged = !(offset == 0 && size == 0);
    std::string range_value;
    if (ranged) {
        range_value = "bytes=" + std::to_string(offset) + "-" +
                      std::to_string(offset + size - 1);
        extra["range"] = range_value;
    }

    const auto signed_headers = s3::sign_request(
        "GET", uri, "", host_header(), region_, access_key_, secret_key_, "", extra);

    httplib::Client client(endpoint_);
    client.set_connection_timeout(10, 0);
    client.set_read_timeout(60, 0);
    httplib::Headers headers = {
        {"Host", host_header()},
        {"x-amz-date", signed_headers.amz_date},
        {"x-amz-content-sha256", signed_headers.payload_hash},
        {"Authorization", signed_headers.authorization},
    };
    if (ranged) headers.emplace("Range", range_value);

    const auto res = client.Get(uri, headers);
    if (!res) {
        throw common::Exception{
            "S3Disk::read: request to " + endpoint_ + " failed",
            static_cast<int>(common::ErrorCode::CANNOT_CONNECT)};
    }
    if (res->status == 404) return nullptr;
    if (res->status != 200 && res->status != 206) {
        throw common::Exception{
            "S3Disk::read: GET " + uri + " returned HTTP " + std::to_string(res->status),
            static_cast<int>(common::ErrorCode::NET_ERROR)};
    }

    auto data = std::shared_ptr<uint8_t[]>(new uint8_t[res->body.size()],
                                           std::default_delete<uint8_t[]>());
    std::memcpy(data.get(), res->body.data(), res->body.size());
    return data;
}

auto S3Disk::write(std::string_view path, std::span<const uint8_t> data)
    -> bool {
    const auto uri = object_path(path);
    const std::string body(reinterpret_cast<const char*>(data.data()), data.size());
    const auto signed_headers = s3::sign_request(
        "PUT", uri, "", host_header(), region_, access_key_, secret_key_, body);

    httplib::Client client(endpoint_);
    client.set_connection_timeout(10, 0);
    client.set_write_timeout(60, 0);
    const httplib::Headers headers = {
        {"Host", host_header()},
        {"x-amz-date", signed_headers.amz_date},
        {"x-amz-content-sha256", signed_headers.payload_hash},
        {"Authorization", signed_headers.authorization},
    };

    const auto res = client.Put(uri, headers, body, "application/octet-stream");
    if (!res) {
        throw common::Exception{
            "S3Disk::write: request to " + endpoint_ + " failed",
            static_cast<int>(common::ErrorCode::CANNOT_CONNECT)};
    }
    return res->status == 200 || res->status == 201;
}

auto S3Disk::rename(std::string_view from, std::string_view to) -> bool {
    // S3 has no rename API: copy the object under the new key, then delete
    // the old one. Two separately-signed requests.
    const auto from_uri = object_path(from);
    const auto to_uri = object_path(to);
    const std::string copy_source =
        "/" + s3::uri_encode(bucket_, true) + "/" + s3::uri_encode(std::string{from}, false);

    std::map<std::string, std::string> extra{{"x-amz-copy-source", copy_source}};
    const auto copy_signed = s3::sign_request(
        "PUT", to_uri, "", host_header(), region_, access_key_, secret_key_, "", extra);

    httplib::Client client(endpoint_);
    const httplib::Headers copy_headers = {
        {"Host", host_header()},
        {"x-amz-date", copy_signed.amz_date},
        {"x-amz-content-sha256", copy_signed.payload_hash},
        {"x-amz-copy-source", copy_source},
        {"Authorization", copy_signed.authorization},
    };
    const auto copy_res = client.Put(to_uri, copy_headers, "", "application/octet-stream");
    if (!copy_res || copy_res->status != 200) return false;

    const auto delete_signed = s3::sign_request(
        "DELETE", from_uri, "", host_header(), region_, access_key_, secret_key_, "");
    const httplib::Headers delete_headers = {
        {"Host", host_header()},
        {"x-amz-date", delete_signed.amz_date},
        {"x-amz-content-sha256", delete_signed.payload_hash},
        {"Authorization", delete_signed.authorization},
    };
    const auto delete_res = client.Delete(from_uri, delete_headers);
    return delete_res && (delete_res->status == 204 || delete_res->status == 200);
}

auto S3Disk::remove(std::string_view path) -> bool {
    const auto uri = object_path(path);
    const auto signed_headers = s3::sign_request(
        "DELETE", uri, "", host_header(), region_, access_key_, secret_key_, "");

    httplib::Client client(endpoint_);
    const httplib::Headers headers = {
        {"Host", host_header()},
        {"x-amz-date", signed_headers.amz_date},
        {"x-amz-content-sha256", signed_headers.payload_hash},
        {"Authorization", signed_headers.authorization},
    };

    const auto res = client.Delete(uri, headers);
    return res && (res->status == 204 || res->status == 200);
}

auto S3Disk::list_files(std::string_view path) -> std::vector<std::string> {
    std::string prefix{path};
    if (!prefix.empty() && prefix.back() != '/') prefix += "/";

    const std::string query = build_list_query(prefix);
    const std::string uri = "/" + s3::uri_encode(bucket_, true);
    const auto signed_headers = s3::sign_request(
        "GET", uri, query, host_header(), region_, access_key_, secret_key_, "");

    httplib::Client client(endpoint_);
    const httplib::Headers headers = {
        {"Host", host_header()},
        {"x-amz-date", signed_headers.amz_date},
        {"x-amz-content-sha256", signed_headers.payload_hash},
        {"Authorization", signed_headers.authorization},
    };

    const auto res = client.Get(uri + "?" + query, headers);
    if (!res || res->status != 200) return {};

    std::vector<std::string> files;
    for (const auto& key : extract_xml_tag_values(res->body, "Key")) {
        if (key.size() > prefix.size() && key.compare(0, prefix.size(), prefix) == 0) {
            const auto rest = key.substr(prefix.size());
            if (rest.find('/') == std::string::npos) files.push_back(rest);
        }
    }
    return files;
}

auto S3Disk::list_dirs(std::string_view path) -> std::vector<std::string> {
    std::string prefix{path};
    if (!prefix.empty() && prefix.back() != '/') prefix += "/";

    const std::string query = build_list_query(prefix);
    const std::string uri = "/" + s3::uri_encode(bucket_, true);
    const auto signed_headers = s3::sign_request(
        "GET", uri, query, host_header(), region_, access_key_, secret_key_, "");

    httplib::Client client(endpoint_);
    const httplib::Headers headers = {
        {"Host", host_header()},
        {"x-amz-date", signed_headers.amz_date},
        {"x-amz-content-sha256", signed_headers.payload_hash},
        {"Authorization", signed_headers.authorization},
    };

    const auto res = client.Get(uri + "?" + query, headers);
    if (!res || res->status != 200) return {};

    std::vector<std::string> dirs;
    for (const auto& common_prefix : extract_xml_tag_values(res->body, "Prefix")) {
        if (common_prefix.size() > prefix.size() &&
            common_prefix.compare(0, prefix.size(), prefix) == 0) {
            auto rest = common_prefix.substr(prefix.size());
            if (!rest.empty() && rest.back() == '/') rest.pop_back();
            if (!rest.empty()) dirs.push_back(rest);
        }
    }
    return dirs;
}

auto S3Disk::create_dir(std::string_view path) -> bool {
    // S3 has no directories, only keys — nothing to create.
    (void)path;
    return true;
}

auto S3Disk::remove_dir(std::string_view path) -> bool {
    // S3 has no directories to remove; deleting "everything under a prefix"
    // would need a list + bulk-delete loop, which is out of scope here since
    // nothing in FileStorage currently calls remove_dir on S3Disk.
    (void)path;
    return false;
}

auto S3Disk::stats() const -> DiskStats {
    // Unlike a local filesystem, S3 has no fixed capacity to report — this
    // intentionally stays at zero rather than a made-up estimate.
    return DiskStats{};
}

auto S3Disk::set_endpoint(std::string endpoint) -> void {
    endpoint_ = std::move(endpoint);
}

auto S3Disk::set_bucket(std::string bucket) -> void {
    bucket_ = std::move(bucket);
}

} // namespace mnemo::disks
