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
