# S3 storage unit — `ENGINE=File` support — changelog

**Date:** 2026-07-07

## Summary

Implemented real S3-compatible I/O for `S3Disk` (per the implementation guide in
[docs/issues/s3_file_engine_support.md](../issues/s3_file_engine_support.md)),
so `CREATE TABLE ... ENGINE=File STORAGE_UNIT <s3_unit>` — previously blocked
by an explicit guard — now works end-to-end against any S3-compatible store
(AWS S3, MinIO, Ceph RGW, ...). Requests are authenticated with a hand-rolled
AWS Signature V4 signer, reusing the project's existing vendored `httplib`
instead of adding an AWS SDK dependency.

## Changes by file

### Two pre-existing bugs fixed first (Part A)

- `interpreter_create_query.cpp` (`do_create_storage_unit`) — `ACCESS_KEY` /
  `SECRET_KEY` properties from `CREATE STORAGE_UNIT` were parsed correctly but
  silently dropped when copying into `StorageUnitEntry`. Added the two missing
  `else if` branches.
- `disk_s3.h` / `disk_s3.cpp` — `S3Disk::create` had no `region` parameter,
  even though AWS Signature V4 requires one to sign requests. Added
  `std::string region = ""`, dropped the dead `use_ssl` flag (HTTPS vs HTTP is
  already fully determined by the `endpoint` URL scheme).

### New module: SigV4 signer

- `src/Disks/s3_signer.h` / `s3_signer.cpp` — `sign_request(...)` builds the
  canonical request, string-to-sign, and derived signing key exactly per
  AWS's documented algorithm (SHA-256 / HMAC-SHA-256 via OpenSSL's
  `libcrypto`), returning the `Authorization`, `x-amz-date`, and
  `x-amz-content-sha256` header values. Also exports `uri_encode` (RFC 3986,
  with a slash-encoding toggle for object keys vs. query values).

### `src/Disks/disk_s3.cpp` — full rewrite

Every method now makes a real signed HTTP call via `httplib::Client`:
`exists`/`size`/`modified_at` (HEAD), `read` (GET, with byte-range support),
`write` (PUT), `remove` (DELETE), `rename` (PUT COPY + DELETE, since S3 has no
rename API), `list_files`/`list_dirs` (GET `?list-type=2` + minimal hand-rolled
XML tag extraction, mirroring the JSON-parsing style already used in
`rest_connector.cpp`). `create_dir`/`remove_dir` are no-ops/unsupported since S3
has no real directories. Credentials fall back to `AWS_ACCESS_KEY_ID` /
`AWS_SECRET_ACCESS_KEY` / `AWS_REGION` (or `AWS_DEFAULT_REGION`) when not
supplied via SQL, so secrets never need to appear in `CREATE STORAGE_UNIT`.

### `src/StorageUnits/disk_factory.cpp`

Passes `entry.region` through to `S3Disk::create` instead of the old
hard-coded `true` (`use_ssl`) argument.

### `src/Interpreters/interpreter_create_query.cpp` (`do_create_table`)

Removed the guard that rejected `ENGINE=File` on S3 storage units — nothing
needed to replace it, since `FileStorage` already talks only to the abstract
`IDisk` interface and required no changes itself.

### Build system

- Root `CMakeLists.txt` — `HTTPLIB_REQUIRE_OPENSSL` `OFF` → `ON`, so a missing
  OpenSSL install now fails loudly at CMake configure time instead of silently
  building `httplib` without HTTPS support.
- `src/Disks/CMakeLists.txt` — added `find_package(OpenSSL REQUIRED)` and
  linked `httplib` + `OpenSSL::Crypto` into `mnemosyne_disks`.

### Tests

- `tests/test_disk_s3.h` (new) — Catch2 write/read/exists/remove round-trip
  against a live endpoint, gated behind `MNEMOSYNE_TEST_S3_ENDPOINT` (via
  `SKIP(...)` when unset, so it never fails CI machines without an S3
  endpoint configured). Registered in `tests/test_all.cpp`.

## Environment note

This machine's toolchain is MSVC (Visual Studio 17 2022); the OpenSSL that
ships with Git for Windows is MinGW-built and ABI-incompatible with it. A
matching MSVC OpenSSL was installed via `choco install openssl` (Shining Light
Productions' Win64 build) before `find_package(OpenSSL)` could succeed.

## Verification

Built `mnemosyne_disks`, `mnemosyne_server`, and `mnemosyne_tests` in Debug
after reconfiguring CMake. Ran the new Catch2 test and a full SQL round-trip
against a real, already-provisioned MinIO instance (bucket `mnemo`):

```bash
MNEMOSYNE_TEST_S3_ENDPOINT=http://100.127.65.29:9000 \
MNEMOSYNE_TEST_S3_BUCKET=mnemo \
MNEMOSYNE_TEST_S3_ACCESS_KEY=<redacted> \
MNEMOSYNE_TEST_S3_SECRET_KEY=<redacted> \
  ./tests/Debug/mnemosyne_tests.exe "[s3]" -s
# All tests passed (7 assertions in 1 test case)
```

```sql
CREATE STORAGE_UNIT su_s3_e2e TYPE S3 BUCKET 'mnemo'
    ENDPOINT 'http://100.127.65.29:9000' ACCESS_KEY '...' SECRET_KEY '...';
CREATE TABLE s3_tbl (id Float64, val Float64) Engine=File STORAGE_UNIT su_s3_e2e;
INSERT INTO s3_tbl VALUES (1, 1.5), (2, 2.5);
INSERT INTO s3_tbl VALUES (3, 3.5), (4, 4.5);
SELECT * FROM s3_tbl;   -- 4 rows: confirms both INSERTs appended, not overwrote
```

Confirmed via `aws s3 ls --recursive` that `id.bin`/`val.bin` landed at
`mnemo/s3_e2e_db/s3_tbl/` with the expected byte counts (32 bytes = 4 rows ×
8-byte `Float64` each), then dropped the table/storage unit and removed the
test objects.

Full `mnemosyne_tests.exe` suite re-run afterward: no new failures (the two
pre-existing `[interpreter]` failures — an unrelated, already-broken
planner-based SELECT path — are unchanged from before this work).

## Out of scope

- IAM roles / instance profiles (IMDSv2) — the env-var fallback already
  covers the common self-hosted/MinIO/local-dev case; revisit if deploying
  against real AWS with instance roles.
- `remove_dir` on `S3Disk` — would need a list + bulk-delete loop; nothing in
  `FileStorage` currently calls it on S3, so left unimplemented (returns
  `false`).
