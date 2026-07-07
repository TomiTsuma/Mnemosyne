// src/Disks/s3_signer.h — AWS Signature Version 4 request signing
// Mnemosyne: A column-oriented analytical DBMS
//
// Implements header-based AWS SigV4 signing (see AWS's "Signature Version 4
// signing process" docs). This is a protocol every S3-compatible store
// (AWS S3, MinIO, Ceph RGW, ...) validates identically, so implementing it
// directly here — instead of depending on the AWS SDK — works against all
// of them.

#pragma once

#include <map>
#include <string>

namespace mnemo::disks::s3 {

struct SignedHeaders {
    std::string authorization;   // value for the "Authorization" header
    std::string amz_date;        // value for the "x-amz-date" header
    std::string payload_hash;    // value for the "x-amz-content-sha256" header
};

// Computes the SigV4 Authorization header for a single HTTP request.
//
// method           — "GET", "PUT", "DELETE", "HEAD"
// canonical_uri    — URL-encoded request path, e.g. "/my-bucket/my/object.bin"
// canonical_query  — URL-encoded query string with keys already sorted
//                     alphabetically, e.g. "list-type=2&prefix=a%2F", or ""
// host_header      — the exact value that will be sent as the "Host" header
// region           — e.g. "us-east-1" (any consistent string works for MinIO)
// access_key       — S3 access key
// secret_key       — S3 secret key
// payload          — the raw request body (empty string for GET/HEAD/DELETE)
// extra_headers    — additional headers that must be signed, with
//                     lower-case names, e.g. {{"range", "bytes=0-99"}}
[[nodiscard]] auto sign_request(
    const std::string& method,
    const std::string& canonical_uri,
    const std::string& canonical_query,
    const std::string& host_header,
    const std::string& region,
    const std::string& access_key,
    const std::string& secret_key,
    const std::string& payload,
    const std::map<std::string, std::string>& extra_headers = {})
    -> SignedHeaders;

// URL-encodes one path/query component per RFC 3986.
// Pass encode_slash=false when encoding an S3 object key used directly as a
// URI path (AWS leaves '/' between "directory" segments unescaped there);
// pass true everywhere else (query string values, x-amz-copy-source).
[[nodiscard]] auto uri_encode(const std::string& input, bool encode_slash) -> std::string;

} // namespace mnemo::disks::s3
