# Implementation Guide: `ENGINE=File` on S3 Storage Units

This is a step-by-step guide for implementing this feature yourself. It assumes
no prior C++ experience — every step names the exact file, the exact function,
and the exact lines to change, with the full code to put there and an
explanation of *why*.

**What you're building:** today, `CREATE TABLE ... ENGINE=File STORAGE_UNIT su_s3`
is explicitly blocked in code. You will (1) fix two real bugs that silently
break S3 storage units even before you touch any S3 code, (2) implement a real
S3-compatible HTTP client (works with AWS S3, MinIO, Ceph RGW, or any other
S3-compatible object store), and (3) make credentials safer to configure.

**Investigation findings** (read this before starting — it changes what you
need to do):

1. There is a **guard check** in
   [interpreter_create_query.cpp](file:///c:/Users/tsuma.thomas/Documents/Mnemosyne/src/Interpreters/interpreter_create_query.cpp)
   that explicitly rejects `ENGINE=File` on S3 storage units. This must be removed.
2. `S3Disk` (in [disk_s3.cpp](file:///c:/Users/tsuma.thomas/Documents/Mnemosyne/src/Disks/disk_s3.cpp))
   is a **placeholder** — every method just returns `false`/`0`/`nullptr` without
   ever making a network call. This must become a real S3 client.
3. **Bug found:** even when you type `ACCESS_KEY` / `SECRET_KEY` in
   `CREATE STORAGE_UNIT`, the interpreter silently drops them — they never
   reach `S3Disk`. This means today, S3 credentials configured via SQL do
   nothing at all. You must fix this before real S3 access can work.
4. **Bug found:** `S3Disk::create` has no `region` parameter, even though
   `StorageUnitEntry` already has a `region` field and AWS Signature V4
   (the auth scheme every S3-compatible store implements) requires a region
   to sign requests. You must add it.
5. **Good news:** `SHOW STORAGE_UNITS` and `DESCRIBE STORAGE_UNIT` already do
   **not** print `access_key`/`secret_key` (verified in
   [block_interpreter.cpp](file:///c:/Users/tsuma.thomas/Documents/Mnemosyne/src/Interpreters/block_interpreter.cpp)
   lines 411-426 and 1527-1559) — no fix needed there.
6. **Good news:** the SQL parser already accepts arbitrary `KEY 'value'`
   properties on `CREATE STORAGE_UNIT` (see
   [parser.cpp:2368-2400](file:///c:/Users/tsuma.thomas/Documents/Mnemosyne/src/Parsers/parser.cpp)) —
   `ACCESS_KEY` and `SECRET_KEY` already parse correctly into
   `create.storage_properties["ACCESS_KEY"]` etc. via the generic `Identifier`
   fallback branch. **You do not need to touch the parser or lexer at all.**
   The bug is entirely in the interpreter (point 3 above).

---

## 0. Architecture recap

```
┌─────────────────┐
│   FileStorage   │ (table engine — src/Storages/file_storage.cpp)
└────────┬────────┘
         │ disk_->write(path, bytes) / disk_->read(path, offset, size)
         ▼
┌─────────────────┐
│     IDisk       │ (abstract interface — src/Disks/disk.h)
└────────┬────────┘
         ├──────────────────────────┐
         ▼                          ▼
┌─────────────────┐        ┌─────────────────┐
│  LocalFileDisk  │        │     S3Disk      │
└─────────────────┘        └─────────────────┘
```

`FileStorage` never touches the filesystem or the network directly — it only
calls methods on whatever `IDisk` it was handed. Which concrete disk it gets
is decided in
[disk_factory.cpp](file:///c:/Users/tsuma.thomas/Documents/Mnemosyne/src/StorageUnits/disk_factory.cpp),
based on the storage unit's `type`. This means once `S3Disk`'s methods
actually talk to S3, `FileStorage` starts working on S3 **without any change
to `FileStorage` itself.**

The project already vendors [cpp-httplib](https://github.com/yhirose/cpp-httplib)
(see `httplib` target, fetched in the root `CMakeLists.txt`) as its HTTP
client/server library — it's already used this way in
[rest_connector.cpp](file:///c:/Users/tsuma.thomas/Documents/Mnemosyne/src/Connectors/rest_connector.cpp).
We'll reuse it instead of adding a new dependency like the AWS SDK for C++.
Combined with hand-rolled **AWS Signature Version 4** request signing (the
same signing scheme AWS S3, MinIO, Ceph RGW, and every other S3-compatible
store expects), this gives us a lightweight client that works against any of
them — with no dependency heavier than OpenSSL (needed for `SHA-256`/`HMAC-SHA-256`).

---

## Part A — Fix the two bugs first

Do these two fixes first. They're small, isolated, and without them nothing
else in this guide will work end-to-end even after `S3Disk` is fully implemented.

### A.1 — Credentials are silently dropped

**File:** [src/Interpreters/interpreter_create_query.cpp](file:///c:/Users/tsuma.thomas/Documents/Mnemosyne/src/Interpreters/interpreter_create_query.cpp)
**Function:** `InterpreterCreateQuery::do_create_storage_unit` (around line 334)

Find this loop (around line 350):

```cpp
    for (const auto& [key, value] : create.storage_properties) {
        if (key == "PATH") {
            entry.path = value;
        } else if (key == "BUCKET") {
            entry.bucket = value;
        } else if (key == "ENDPOINT") {
            entry.endpoint = value;
        } else if (key == "REGION") {
            entry.region = value;
        }
    }
```

Replace it with:

```cpp
    for (const auto& [key, value] : create.storage_properties) {
        if (key == "PATH") {
            entry.path = value;
        } else if (key == "BUCKET") {
            entry.bucket = value;
        } else if (key == "ENDPOINT") {
            entry.endpoint = value;
        } else if (key == "REGION") {
            entry.region = value;
        } else if (key == "ACCESS_KEY") {
            entry.access_key = value;
        } else if (key == "SECRET_KEY") {
            entry.secret_key = value;
        }
    }
```

**What changed:** two new `else if` branches. **Why:** `create.storage_properties`
is a `std::unordered_map<std::string, std::string>` already populated correctly
by the parser (confirmed in `parser.cpp`). `entry` is a `storage_units::StorageUnitEntry`
(defined in [storage_unit_catalog.h](file:///c:/Users/tsuma.thomas/Documents/Mnemosyne/src/StorageUnits/storage_unit_catalog.h))
which already has `access_key` and `secret_key` fields — they just weren't
being copied over. Without this fix, `entry.access_key`/`entry.secret_key`
are always empty, no matter what you type in SQL.

### A.2 — `S3Disk::create` needs a `region` parameter

AWS Signature V4 (the request-signing scheme, explained in Part C) requires a
region string to compute the signature — even against MinIO or other
non-AWS endpoints, which usually accept any string (e.g. `us-east-1`) as long
as it's consistent. Right now `S3Disk::create` has no way to receive one.

**File:** [src/Disks/disk_s3.h](file:///c:/Users/tsuma.thomas/Documents/Mnemosyne/src/Disks/disk_s3.h)

Find:

```cpp
    static auto create(std::string name,
                       std::string endpoint,
                       std::string bucket,
                       std::string access_key,
                       std::string secret_key,
                       bool use_ssl = true)
        -> std::shared_ptr<S3Disk>;
```

Replace with:

```cpp
    static auto create(std::string name,
                       std::string endpoint,
                       std::string bucket,
                       std::string access_key,
                       std::string secret_key,
                       std::string region = "")
        -> std::shared_ptr<S3Disk>;
```

**Why we dropped `use_ssl`:** it was a dead parameter — the placeholder
implementation never read it, and it doesn't need to be a separate flag at
all: whether a request uses HTTPS is already fully determined by whether
`endpoint` starts with `https://` or `http://` (e.g. `https://s3.amazonaws.com`
vs. `http://localhost:9000` for a local MinIO instance running without TLS).
Keeping both would let them silently disagree with each other.

Now find the private member list at the bottom of the same file:

```cpp
private:
    S3Disk();
    std::string name_;
    std::string endpoint_;
    std::string bucket_;
    std::string access_key_;
    std::string secret_key_;
    bool use_ssl_ = true;
};
```

Replace with:

```cpp
private:
    S3Disk();

    // Builds the exact request-target string (path + optional query string)
    // used both to sign a request and to send it, so the two always match.
    [[nodiscard]] auto object_path(std::string_view key) const -> std::string;
    // The exact value sent (and signed) as the "Host" header.
    [[nodiscard]] auto host_header() const -> std::string;

    std::string name_;
    std::string endpoint_;
    std::string bucket_;
    std::string access_key_;
    std::string secret_key_;
    std::string region_ = "us-east-1";
};
```

We'll implement `object_path` and `host_header` in Part D.

---

## Part B — Build system changes (CMake)

### B.1 — Make httplib build with HTTPS support

**File:** [CMakeLists.txt](file:///c:/Users/tsuma.thomas/Documents/Mnemosyne/CMakeLists.txt) (repo root)

Find this block (around line 55, right before `FetchContent_MakeAvailable(httplib)`):

```cmake
set(HTTPLIB_REQUIRE_SSL OFF CACHE BOOL "" FORCE)
set(HTTPLIB_REQUIRE_OPENSSL OFF CACHE BOOL "" FORCE)
```

Change the second line's value from `OFF` to `ON`:

```cmake
set(HTTPLIB_REQUIRE_SSL OFF CACHE BOOL "" FORCE)
set(HTTPLIB_REQUIRE_OPENSSL ON CACHE BOOL "" FORCE)
```

**Why:** `httplib::Client` only supports `https://` URLs if the vendored
copy of cpp-httplib was itself compiled with OpenSSL support
(`CPPHTTPLIB_OPENSSL_SUPPORT`). Today that's optional and silent — if OpenSSL
isn't found on your machine when CMake configures, httplib quietly builds
*without* HTTPS support, and the first time your code tries to reach real AWS
S3 (which requires `https://`), it throws `std::invalid_argument("'https'
scheme is not supported.")` at runtime — a confusing failure far from its
cause. Setting `HTTPLIB_REQUIRE_OPENSSL ON` makes this fail loudly at CMake
*configure* time instead, with a clear "OpenSSL not found" error, which is
much easier to diagnose. (Talking to a local MinIO over plain `http://` will
still work regardless of this setting — this only matters for `https://`.)

### B.2 — Link OpenSSL and httplib into the Disks library

**File:** [src/Disks/CMakeLists.txt](file:///c:/Users/tsuma.thomas/Documents/Mnemosyne/src/Disks/CMakeLists.txt)

Current content:

```cmake
# src/Disks/CMakeLists.txt
file(GLOB MNEMOSYNE_DISKS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(mnemosyne_disks ${MNEMOSYNE_DISKS_SOURCES})

target_include_directories(mnemosyne_disks PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_disks PUBLIC
    mnemosyne_common
)

set_target_properties(mnemosyne_disks PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
```

Add a `find_package` call and a new `target_link_libraries` block (insert
right after the `file(GLOB ...)` / `add_library(...)` lines):

```cmake
# src/Disks/CMakeLists.txt
file(GLOB MNEMOSYNE_DISKS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(mnemosyne_disks ${MNEMOSYNE_DISKS_SOURCES})

find_package(OpenSSL REQUIRED)

target_include_directories(mnemosyne_disks PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mnemosyne_disks PUBLIC
    mnemosyne_common
)

target_link_libraries(mnemosyne_disks PRIVATE
    httplib
    OpenSSL::Crypto
)

set_target_properties(mnemosyne_disks PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
```

**Why:** `mnemosyne_disks` didn't link `httplib` at all before (only
`mnemosyne_connectors` did) — `disk_s3.cpp` is about to `#include <httplib.h>`
so it needs the target linked, same as
[src/Connectors/CMakeLists.txt](file:///c:/Users/tsuma.thomas/Documents/Mnemosyne/src/Connectors/CMakeLists.txt)
already does. `OpenSSL::Crypto` is the imported CMake target providing
`libcrypto` (SHA-256, HMAC) — this is separate from `OpenSSL::SSL`, which
`httplib` links internally for the HTTPS transport itself; we only need
`Crypto` directly, for the request-signing code you'll write in Part C.

### B.3 — Installing OpenSSL (Windows)

Since you're on Windows, `find_package(OpenSSL)` needs OpenSSL's development
headers/libs available. The simplest route is [vcpkg](https://vcpkg.io):

```bash
git clone https://github.com/microsoft/vcpkg
./vcpkg/bootstrap-vcpkg.bat
./vcpkg/vcpkg install openssl:x64-windows
```

Then configure CMake with the vcpkg toolchain file added to your existing
configure command:

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake
```

If you already have OpenSSL installed some other way (e.g. via an installer),
point CMake at it directly instead: `-DOPENSSL_ROOT_DIR=<install-path>`.

---

## Part C — The AWS Signature V4 signer

This is the core piece that makes the client work against **any**
S3-compatible store. AWS Signature V4 (SigV4) is a public, documented
protocol — it's not an AWS-only mechanism. MinIO, Ceph RGW, Wasabi, Backblaze
B2 (S3-compatible mode), etc. all validate requests against the exact same
algorithm, so implementing the protocol directly gives you universal
compatibility "for free," instead of only supporting whatever one SDK
happens to hard-code.

At a high level, SigV4 asks you to build a canonical, deterministic
description of your request (method, path, query string, a fixed set of
headers, and a hash of the body), hash *that*, then run it through a chain of
`HMAC-SHA-256` operations seeded with your secret key. The final hex string
goes into an `Authorization` header. If your description of the request
doesn't byte-for-byte match what actually goes over the wire, the signature
won't validate — that's why every helper below is written to be reused
identically for both "what we sign" and "what we send."

### C.1 — New file: `src/Disks/s3_signer.h`

Create this new file:

```cpp
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
```

### C.2 — New file: `src/Disks/s3_signer.cpp`

```cpp
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
```

You don't need to add this file to `CMakeLists.txt` — `src/Disks/CMakeLists.txt`
uses `file(GLOB ... CONFIGURE_DEPENDS *.cpp)`, which picks up new `.cpp`
files in that folder automatically the next time CMake configures (most
build setups re-run configure automatically; if yours doesn't pick it up,
just re-run your `cmake -B build` command once).

---

## Part D — Rewrite `S3Disk`

### D.1 — `src/Disks/disk_s3.cpp` (full replacement)

Replace the entire file with:

```cpp
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
```

### D.2 — Update `disk_factory.cpp` to pass the region through

**File:** [src/StorageUnits/disk_factory.cpp](file:///c:/Users/tsuma.thomas/Documents/Mnemosyne/src/StorageUnits/disk_factory.cpp)

Find:

```cpp
        case StorageUnitType::S3:
            return disks::S3Disk::create(
                entry.name,
                entry.endpoint,
                entry.bucket,
                entry.access_key,
                entry.secret_key,
                true);
```

Replace with:

```cpp
        case StorageUnitType::S3:
            return disks::S3Disk::create(
                entry.name,
                entry.endpoint,
                entry.bucket,
                entry.access_key,
                entry.secret_key,
                entry.region);
```

(`entry.region` may be an empty string here if `REGION` wasn't specified in
`CREATE STORAGE_UNIT` — that's fine, `S3Disk::create` already falls back to
environment variables and finally `"us-east-1"`, from Part D.1.)

---

## Part E — Remove the guard check

**File:** [src/Interpreters/interpreter_create_query.cpp](file:///c:/Users/tsuma.thomas/Documents/Mnemosyne/src/Interpreters/interpreter_create_query.cpp)
**Function:** `do_create_table`, around line 199

Find:

```cpp
        if (unit->type == storage_units::StorageUnitType::S3) {
            throw common::Exception{
                "S3 storage units do not support File engine I/O yet",
                static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
        }
```

Delete these four lines entirely. Nothing needs to replace them — this block
existed solely to reject S3, and now that `S3Disk` is real, there's nothing
left to guard against.

---

## Part F — Credential handling

Three things were asked for here; here's the state of each after the changes
above:

1. **Env var fallback (implemented in Part D.1):** `S3Disk::create` now reads
   `AWS_ACCESS_KEY_ID` / `AWS_SECRET_ACCESS_KEY` / `AWS_REGION` (or
   `AWS_DEFAULT_REGION`) whenever the SQL statement didn't provide them. This
   means you can now run:
   ```sql
   CREATE STORAGE_UNIT su_s3 TYPE S3 BUCKET 'my-bucket' ENDPOINT 'https://s3.amazonaws.com';
   ```
   with credentials supplied only via environment variables when starting
   the Mnemosyne server — nothing secret needs to appear in SQL, logs, or
   query history.

2. **No secret leakage in `SHOW`/`DESCRIBE` (already true, verified, no
   change needed):** confirmed in `block_interpreter.cpp` that
   `SHOW STORAGE_UNITS` and `DESCRIBE STORAGE_UNIT` only ever print
   `name`/`type`/`status`/`path`/`endpoint`/`bucket`/`region`/stats — never
   `access_key`/`secret_key`. If a `access_key`/`secret_key` row is ever added
   to those code paths in the future, mask it the same way
   `connector_catalog.cpp`'s `mask_property_value` already does for
   connectors (`is_secret_property` / `"***"`).

3. **IAM roles / instance profiles — explicitly out of scope for this pass.**
   The original issue notes running on an EC2/EKS/ECS instance role instead
   of static keys. That requires querying the AWS Instance Metadata Service
   (IMDSv2) at runtime for short-lived temporary credentials — a separate,
   larger feature (token refresh, retry-on-expiry, etc.) that isn't needed to
   unblock `ENGINE=File` on S3 today, since the env-var fallback in point 1
   already covers the common self-hosted/MinIO/local-dev case. Leave this as
   a follow-up if/when you deploy against real AWS with instance roles.

**Intentional behavior if credentials are missing everywhere** (no SQL
values, no env vars): `access_key_`/`secret_key_` stay empty strings, SigV4
signs with an empty key, and S3/MinIO will reject the actual request with a
`403 Forbidden` at call time — which becomes a real, visible
`common::Exception` (`NET_ERROR`) from e.g. `S3Disk::write`. This is
deliberate: it's better to fail loudly with a real HTTP error than to
silently substitute placeholder credentials.

---

## Part G — Testing

### G.1 — Manual test with a local MinIO

MinIO is the easiest way to test against a real S3-compatible server without
an AWS account. Run it via Docker:

```bash
docker run -d --name mnemosyne-test-minio \
  -p 9000:9000 -p 9001:9001 \
  -e MINIO_ROOT_USER=minioadmin \
  -e MINIO_ROOT_PASSWORD=minioadmin \
  minio/minio server /data --console-address ":9001"
```

Create the bucket (via the MinIO web console at `http://localhost:9001`,
login `minioadmin`/`minioadmin`, or via the `mc` CLI if you have it
installed):

```bash
mc alias set local http://localhost:9000 minioadmin minioadmin
mc mb local/mnemosyne-test
```

Then, from the Mnemosyne SQL shell:

```sql
CREATE STORAGE_UNIT su_s3 TYPE S3
    BUCKET 'mnemosyne-test'
    ENDPOINT 'http://localhost:9000'
    ACCESS_KEY 'minioadmin'
    SECRET_KEY 'minioadmin';

CREATE TABLE s3_tbl (id Float64, val Float64) Engine=File STORAGE_UNIT su_s3;
INSERT INTO s3_tbl VALUES (1, 1.5), (2, 2.5);
SELECT * FROM s3_tbl;
```

If this round-trips correctly, you can confirm the objects landed in S3 via
`mc ls local/mnemosyne-test/<db_name>/s3_tbl/`.

### G.2 — Automated test (Catch2)

**New file:** `tests/test_disk_s3.h`

```cpp
// tests/test_disk_s3.h — Integration tests for the S3 disk backend
// These perform real HTTP calls, so they only run when a live S3-compatible
// endpoint (e.g. local MinIO) is configured via environment variables.
#pragma once

#include "Disks/disk_s3.h"
#include <catch2/catch_all.hpp>
#include <algorithm>
#include <cstdlib>

namespace {
auto env_or(const char* name, const char* fallback) -> std::string {
    const char* value = std::getenv(name);
    return value ? value : fallback;
}
} // namespace

TEST_CASE("S3Disk write/read/exists/remove roundtrip", "[disk][s3]") {
    const char* endpoint = std::getenv("MNEMOSYNE_TEST_S3_ENDPOINT");
    if (!endpoint) {
        SKIP("Set MNEMOSYNE_TEST_S3_ENDPOINT (e.g. http://localhost:9000) to run S3 tests");
    }

    auto disk = mnemo::disks::S3Disk::create(
        "test-s3",
        endpoint,
        env_or("MNEMOSYNE_TEST_S3_BUCKET", "mnemosyne-test"),
        env_or("MNEMOSYNE_TEST_S3_ACCESS_KEY", "minioadmin"),
        env_or("MNEMOSYNE_TEST_S3_SECRET_KEY", "minioadmin"),
        "us-east-1");

    const std::string key = "catch2-roundtrip-test.bin";
    const std::vector<uint8_t> payload = {1, 2, 3, 4, 5};

    REQUIRE(disk->write(key, std::span(payload.data(), payload.size())));
    REQUIRE(disk->exists(key));
    REQUIRE(disk->size(key) == payload.size());

    auto data = disk->read(key, 0, payload.size());
    REQUIRE(data != nullptr);
    REQUIRE(std::equal(payload.begin(), payload.end(), data.get()));

    REQUIRE(disk->remove(key));
    REQUIRE_FALSE(disk->exists(key));
}
```

Register it in the test runner — **file:**
[tests/test_all.cpp](file:///c:/Users/tsuma.thomas/Documents/Mnemosyne/tests/test_all.cpp),
next to the existing `#include "test_disks.h"` (around line 16):

```cpp
#include "test_disks.h"
#include "test_disk_s3.h"
```

Run it (with MinIO already running from G.1):

```bash
MNEMOSYNE_TEST_S3_ENDPOINT=http://localhost:9000 \
MNEMOSYNE_TEST_S3_BUCKET=mnemosyne-test \
MNEMOSYNE_TEST_S3_ACCESS_KEY=minioadmin \
MNEMOSYNE_TEST_S3_SECRET_KEY=minioadmin \
  ctest --test-dir build -R disk
```

Without those environment variables set, the test reports `SKIP` rather than
failing — so it won't break CI machines that don't have MinIO available.

---

## Part H — Build & verify

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build > build/build.log 2>&1
```

Check `build/build.log` for errors. Common first-time issues:
- `Could NOT find OpenSSL` → see Part B.3.
- Link errors mentioning `HMAC` / `EVP_...` → `OpenSSL::Crypto` isn't linked;
  re-check Part B.2.
- `'httplib.h' file not found` in `disk_s3.cpp` → the `httplib` link line
  from Part B.2 is missing or CMake wasn't reconfigured after adding it.

---

## Part I — Changelog

Once this is implemented and tested, add a changelog entry describing what
changed, per this project's convention:
`docs/changelog/{ddmmyy-hhmmss}_s3_file_engine_changelog.md` (see existing
entries in `docs/changelog/` for the expected format/tone).

---

## Summary of every file touched

| File | Change |
|---|---|
| `src/Interpreters/interpreter_create_query.cpp` | Fix dropped `ACCESS_KEY`/`SECRET_KEY` (Part A.1); remove S3 guard check (Part E) |
| `src/Disks/disk_s3.h` | Add `region` param, drop dead `use_ssl` param, add two private helpers (Part A.2) |
| `src/Disks/disk_s3.cpp` | Full rewrite: real HTTP calls via httplib + SigV4 (Part D.1) |
| `src/Disks/s3_signer.h` (new) | SigV4 signing interface (Part C.1) |
| `src/Disks/s3_signer.cpp` (new) | SigV4 signing implementation (Part C.2) |
| `src/StorageUnits/disk_factory.cpp` | Pass `entry.region` through instead of a hard-coded `true` (Part D.2) |
| `CMakeLists.txt` (root) | `HTTPLIB_REQUIRE_OPENSSL` OFF → ON (Part B.1) |
| `src/Disks/CMakeLists.txt` | Link `httplib` + `OpenSSL::Crypto` (Part B.2) |
| `tests/test_disk_s3.h` (new) | Catch2 roundtrip test against a live endpoint (Part G.2) |
| `tests/test_all.cpp` | Include the new test header (Part G.2) |
