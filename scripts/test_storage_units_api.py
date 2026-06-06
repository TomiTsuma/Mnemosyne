#!/usr/bin/env python3
"""End-to-end API tests for STORAGE_UNIT support.

Usage:
    python scripts/test_storage_units_api.py
    python scripts/test_storage_units_api.py --url http://127.0.0.1:1143 --no-launch
    python scripts/test_storage_units_api.py --server-bin build/bin/Release/mnemosyne_server.exe

Exit code is 0 if all tests pass, 1 otherwise.
"""

from __future__ import annotations

import argparse
import json
import sys
import tempfile
from pathlib import Path

from _mnemo_client import (
    Client,
    ServerProc,
    TestRunner,
    YELLOW,
    RESET,
)

REPO_ROOT = Path(__file__).resolve().parent.parent
DB = "su_test_db"
LOCAL_UNIT = "su_local"
S3_UNIT = "su_s3"
TABLE = "su_file"


def _rows(body: str) -> list[dict]:
    try:
        data = json.loads(body)
        cols = data.get("columns", [])
        rows = data.get("data", [])
        return [dict(zip(cols, row)) for row in rows]
    except (json.JSONDecodeError, TypeError):
        return []


def _row_count(body: str) -> int:
    try:
        return int(json.loads(body).get("rows", -1))
    except (json.JSONDecodeError, TypeError, ValueError):
        return -1


def setup(client: Client, t: TestRunner, local_path: str) -> None:
    t.section("Setup")
    path_sql = local_path.replace("\\", "/")
    for label, sql in [
        ("CREATE DATABASE", f"CREATE DATABASE IF NOT EXISTS {DB}"),
        ("USE", f"USE {DB}"),
        ("DROP TABLE", f"DROP TABLE IF EXISTS {TABLE}"),
        ("DROP su_s3", f"DROP STORAGE_UNIT IF EXISTS {S3_UNIT}"),
        ("DROP su_local", f"DROP STORAGE_UNIT IF EXISTS {LOCAL_UNIT}"),
        (
            "CREATE LOCAL unit",
            f"CREATE STORAGE_UNIT {LOCAL_UNIT} TYPE LOCAL PATH '{path_sql}'",
        ),
    ]:
        r = client.query(sql)
        t.check(f"setup: {label}", r.ok(), f"status={r.status} body={r.body[:300]!r}")


def test_show_describe(client: Client, t: TestRunner) -> None:
    t.section("SHOW and DESCRIBE storage units")

    r = client.query("SHOW STORAGE_UNITS")
    t.check("SHOW STORAGE_UNITS returns 200", r.ok(), f"status={r.status} body={r.body!r}")
    body_lower = r.body.lower()
    t.check(
        f"lists {LOCAL_UNIT}",
        LOCAL_UNIT in body_lower,
        f"body={r.body!r}",
    )
    t.check("shows LOCAL type", "local" in body_lower, f"body={r.body!r}")
    t.check("shows ONLINE status", "online" in body_lower, f"body={r.body!r}")

    r = client.query(f"DESCRIBE STORAGE_UNIT {LOCAL_UNIT}")
    t.check("DESCRIBE STORAGE_UNIT returns 200", r.ok(), f"status={r.status}")
    t.check("includes name field", "name" in r.body.lower(), f"body={r.body!r}")
    t.check("includes type LOCAL", "local" in r.body.lower(), f"body={r.body!r}")
    t.check(
        "includes BLOCK_STORAGE capability",
        "block_storage" in r.body.lower(),
        f"body={r.body!r}",
    )


def test_table_on_storage_unit(client: Client, t: TestRunner) -> None:
    t.section("CREATE TABLE with STORAGE_UNIT")

    r = client.query(
        f"CREATE TABLE {TABLE} (id Float64, val Float64) "
        f"ENGINE=File STORAGE_UNIT {LOCAL_UNIT}"
    )
    t.check("CREATE TABLE with STORAGE_UNIT", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(
        f"INSERT INTO {TABLE} VALUES (1.0, 10.5), (2.0, 20.5), (3.0, 30.5)"
    )
    t.check("INSERT into File table", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"SELECT * FROM {TABLE} ORDER BY id")
    t.check("SELECT from File table", r.ok(), f"status={r.status}")
    count = _row_count(r.body)
    t.check("SELECT returns 3 rows", count == 3, f"rows={count} body={r.body!r}")


def test_storage_usage(client: Client, t: TestRunner) -> None:
    t.section("SHOW STORAGE_USAGE")

    r = client.query("SHOW STORAGE_USAGE")
    t.check("SHOW STORAGE_USAGE returns 200", r.ok(), f"status={r.status} body={r.body!r}")
    t.check(
        f"usage includes {LOCAL_UNIT}",
        LOCAL_UNIT in r.body.lower(),
        f"body={r.body!r}",
    )
    rows = _rows(r.body)
    t.check("usage has at least one row", len(rows) >= 1, f"rows={rows}")


def test_s3_catalog(client: Client, t: TestRunner) -> None:
    t.section("S3 catalog registration (no I/O)")

    r = client.query(
        f"CREATE STORAGE_UNIT {S3_UNIT} TYPE S3 "
        f"BUCKET 'test-bucket' ENDPOINT 'http://127.0.0.1:9000'"
    )
    t.check("CREATE S3 storage unit", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("SHOW STORAGE_UNITS")
    t.check("SHOW lists S3 unit", S3_UNIT in r.body.lower(), f"body={r.body!r}")

    r = client.query(f"DESCRIBE STORAGE_UNIT {S3_UNIT}")
    t.check("DESCRIBE S3 unit", r.ok(), f"status={r.status}")
    t.check("type is S3", "s3" in r.body.lower(), f"body={r.body!r}")
    t.check(
        "capability OBJECT_STORAGE",
        "object_storage" in r.body.lower(),
        f"body={r.body!r}",
    )


def test_errors(client: Client, t: TestRunner) -> None:
    t.section("Error handling")

    r = client.query(
        f"CREATE STORAGE_UNIT {LOCAL_UNIT} TYPE LOCAL PATH '/tmp/dup'"
    )
    t.check(
        "duplicate CREATE STORAGE_UNIT fails",
        not r.ok(),
        f"status={r.status} body={r.body!r}",
    )

    r = client.query("DROP STORAGE_UNIT definitely_missing_unit_xyz")
    t.check(
        "DROP missing storage unit fails",
        not r.ok(),
        f"status={r.status} body={r.body!r}",
    )

    r = client.query(f"DROP STORAGE_UNIT {LOCAL_UNIT}")
    t.check(
        "DROP storage unit in use fails",
        not r.ok(),
        f"status={r.status} body={r.body!r}",
    )

    r = client.query(
        f"CREATE TABLE bad_s3 (id Float64) ENGINE=File STORAGE_UNIT {S3_UNIT}"
    )
    t.check(
        "CREATE TABLE on S3 unit with File engine fails",
        not r.ok(),
        f"status={r.status} body={r.body!r}",
    )


def cleanup(client: Client, t: TestRunner) -> None:
    t.section("Cleanup")

    client.query(f"DROP TABLE IF EXISTS {TABLE}")
    r = client.query(f"DROP STORAGE_UNIT {LOCAL_UNIT}")
    t.check("DROP LOCAL storage unit after table removed", r.ok(), f"status={r.status}")

    r = client.query(f"DROP STORAGE_UNIT {S3_UNIT}")
    t.check("DROP S3 storage unit", r.ok(), f"status={r.status}")


def run_tests(client: Client) -> int:
    t = TestRunner()
    with tempfile.TemporaryDirectory(prefix="mnemo_su_") as tmp:
        setup(client, t, tmp)
        test_show_describe(client, t)
        test_table_on_storage_unit(client, t)
        test_storage_usage(client, t)
        test_s3_catalog(client, t)
        test_errors(client, t)
        cleanup(client, t)
    return t.summary()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:1143")
    parser.add_argument("--no-launch", action="store_true")
    parser.add_argument("--server-bin", default=None)
    args = parser.parse_args()

    if args.no_launch:
        client = Client(args.url)
        print(f"{YELLOW}Using existing server at {args.url}{RESET}")
        return run_tests(client)

    with ServerProc(
        Path(args.server_bin) if args.server_bin else None,
        args.url,
    ) as client:
        return run_tests(client)


if __name__ == "__main__":
    sys.exit(main())
