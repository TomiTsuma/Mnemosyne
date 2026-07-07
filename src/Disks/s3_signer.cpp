// src/Disks/s3_signer.cpp — AWS Signature Version 4 implementation

#include "Disks/s3_signer.h"
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>

namespace mnemo::disks::s3 {

namespace {

auto to_hex(const unsigned char* data, unsigned int len) -> std::string {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (unsigned int i = 0; i < len; ++i) {
        out.push_back(digits[(data[i] >> 4) & 0xF]);
        out.push_back(digits[data[i] & 0xF]);
    }
    return out;
}

auto sha256_hex(const std::string& data) -> std::string {
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digest_len = 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, data.data(), data.size());
    EVP_DigestFinal_ex(ctx, digest.data(), &digest_len);
    EVP_MD_CTX_free(ctx);
    return to_hex(digest.data(), digest_len);
}

// Returns the raw (binary, not hex) HMAC-SHA-256 digest.
auto hmac_sha256(const std::string& key, const std::string& data) -> std::string {
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digest_len = 0;
    HMAC(EVP_sha256(),
         key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         digest.data(), &digest_len);
    return std::string(reinterpret_cast<char*>(digest.data()), digest_len);
}

auto hmac_sha256_hex(const std::string& key, const std::string& data) -> std::string {
    const auto raw = hmac_sha256(key, data);
    return to_hex(reinterpret_cast<const unsigned char*>(raw.data()),
                  static_cast<unsigned int>(raw.size()));
}

} // namespace

auto uri_encode(const std::string& input, bool encode_slash) -> std::string {
    std::string out;
    out.reserve(input.size());
    for (unsigned char c : input) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else if (c == '/' && !encode_slash) {
            out.push_back('/');
        } else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", c);
            out += buf;
        }
    }
    return out;
}

auto sign_request(
    const std::string& method,
    const std::string& canonical_uri,
    const std::string& canonical_query,
    const std::string& host_header,
    const std::string& region,
    const std::string& access_key,
    const std::string& secret_key,
    const std::string& payload,
    const std::map<std::string, std::string>& extra_headers) -> SignedHeaders {

    const auto now = std::chrono::system_clock::now();
    const auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc{};
#ifdef _WIN32
    gmtime_s(&tm_utc, &now_time_t);
#else
    gmtime_r(&now_time_t, &tm_utc);
#endif

    char amz_date_buf[32];
    std::strftime(amz_date_buf, sizeof(amz_date_buf), "%Y%m%dT%H%M%SZ", &tm_utc);
    char date_stamp_buf[16];
    std::strftime(date_stamp_buf, sizeof(date_stamp_buf), "%Y%m%d", &tm_utc);
    const std::string amz_date{amz_date_buf};
    const std::string date_stamp{date_stamp_buf};

    const std::string payload_hash = sha256_hex(payload);

    // SigV4 requires canonical headers sorted by lower-case name; std::map
    // keeps its keys sorted, so building it this way sorts them for us.
    std::map<std::string, std::string> headers = extra_headers;
    headers["host"] = host_header;
    headers["x-amz-content-sha256"] = payload_hash;
    headers["x-amz-date"] = amz_date;

    std::string canonical_headers;
    std::string signed_headers;
    for (const auto& [name, value] : headers) {
        canonical_headers += name + ":" + value + "\n";
        signed_headers += (signed_headers.empty() ? "" : ";") + name;
    }

    const std::string canonical_request =
        method + "\n" +
        canonical_uri + "\n" +
        canonical_query + "\n" +
        canonical_headers + "\n" +
        signed_headers + "\n" +
        payload_hash;

    const std::string credential_scope = date_stamp + "/" + region + "/s3/aws4_request";
    const std::string string_to_sign =
        "AWS4-HMAC-SHA256\n" +
        amz_date + "\n" +
        credential_scope + "\n" +
        sha256_hex(canonical_request);

    // Derive the signing key: a fixed chain of HMACs seeded with "AWS4" +
    // the secret key, each step scoping the key to date -> region -> "s3"
    // -> "aws4_request". This is the exact derivation AWS's docs specify.
    const auto k_date    = hmac_sha256("AWS4" + secret_key, date_stamp);
    const auto k_region  = hmac_sha256(k_date, region);
    const auto k_service = hmac_sha256(k_region, "s3");
    const auto k_signing = hmac_sha256(k_service, "aws4_request");

    const std::string signature = hmac_sha256_hex(k_signing, string_to_sign);

    SignedHeaders result;
    result.amz_date = amz_date;
    result.payload_hash = payload_hash;
    result.authorization =
        "AWS4-HMAC-SHA256 Credential=" + access_key + "/" + credential_scope +
        ", SignedHeaders=" + signed_headers +
        ", Signature=" + signature;
    return result;
}

} // namespace mnemo::disks::s3
