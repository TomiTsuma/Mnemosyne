#!/usr/bin/env python3
"""End-to-end API tests for the MODEL layer FEATURE_SET / DATASET entities.

Seeds a table, then exercises CREATE / SHOW / DESCRIBE / DROP for both
FEATURE_SET and DATASET over HTTP. No ML libraries required — these are pure
catalog operations.

Usage:
    python scripts/test_feature_sets_api.py
    python scripts/test_feature_sets_api.py --no-launch
    python scripts/test_feature_sets_api.py --server-bin build/bin/Release/mnemosyne_server.exe

Exit code is 0 if all tests pass, 1 otherwise.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from _mnemo_client import Client, ServerProc, TestRunner, YELLOW, RESET

DB = "feature_set_test_db"
SOURCE = "fs_customers"
FS = "customer_churn_features"
DS = "churn_dataset"


def _rows(body: str) -> list[dict]:
    try:
        data = json.loads(body)
        cols = data.get("columns", [])
        rows = data.get("data", [])
        return [dict(zip(cols, row)) for row in rows]
    except (json.JSONDecodeError, TypeError):
        return []


def setup(client: Client, t: TestRunner) -> None:
    t.section("Setup")
    for label, sql in [
        ("CREATE DATABASE", f"CREATE DATABASE IF NOT EXISTS {DB}"),
        ("USE", f"USE {DB}"),
        ("DROP FEATURE_SET", f"DROP FEATURE_SET IF EXISTS {FS}"),
        ("DROP DATASET", f"DROP DATASET IF EXISTS {DS}"),
        ("DROP TABLE", f"DROP TABLE IF EXISTS {SOURCE}"),
    ]:
        r = client.query(sql)
        t.check(f"setup: {label}", r.ok(), f"status={r.status} body={r.body[:300]!r}")

    r = client.query(
        f"CREATE TABLE {SOURCE} "
        f"(customer_id Int64, tenure Int64, monthly_charges Float64, churn Int64) "
        f"ENGINE=Memory"
    )
    t.check("CREATE source table", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(
        f"INSERT INTO {SOURCE} VALUES "
        f"(1, 12, 70.5, 0), (2, 1, 99.9, 1), (3, 24, 55.0, 0), "
        f"(4, 2, 88.0, 1), (5, 36, 45.0, 0)"
    )
    t.check("INSERT seed rows", r.ok(), f"status={r.status} body={r.body!r}")


def test_feature_set_crud(client: Client, t: TestRunner) -> None:
    t.section("FEATURE_SET CRUD")

    r = client.query(
        f"CREATE FEATURE_SET {FS} FROM {SOURCE} "
        f"ENTITY_KEY(customer_id) FEATURES(tenure, monthly_charges) TARGET churn"
    )
    t.check("CREATE FEATURE_SET", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("SHOW FEATURE_SETS")
    t.check("SHOW FEATURE_SETS", r.ok(), f"status={r.status}")
    t.check("lists feature set", FS in r.body, f"body={r.body!r}")
    rows = _rows(r.body)
    match = next((row for row in rows if row.get("name") == FS), None)
    t.check("feature set row present", match is not None, f"rows={rows}")
    if match:
        t.check("source_table recorded", match.get("source_table") == SOURCE, f"row={match}")
        t.check("entity_key recorded", match.get("entity_key") == "customer_id", f"row={match}")
        t.check("target recorded", match.get("target") == "churn", f"row={match}")
        t.check(
            "features recorded",
            "tenure" in str(match.get("features")) and "monthly_charges" in str(match.get("features")),
            f"row={match}",
        )

    r = client.query(f"DESCRIBE FEATURE_SET {FS}")
    t.check("DESCRIBE FEATURE_SET", r.ok(), f"status={r.status}")
    body_lower = r.body.lower()
    t.check("describe mentions source", SOURCE.lower() in body_lower, f"body={r.body!r}")
    t.check("describe mentions entity_key", "customer_id" in body_lower, f"body={r.body!r}")


def test_dataset_crud(client: Client, t: TestRunner) -> None:
    t.section("DATASET CRUD")

    r = client.query(f"CREATE DATASET {DS} FROM {SOURCE}")
    t.check("CREATE DATASET", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("SHOW DATASETS")
    t.check("SHOW DATASETS", r.ok(), f"status={r.status}")
    t.check("lists dataset", DS in r.body, f"body={r.body!r}")

    r = client.query(f"DESCRIBE DATASET {DS}")
    t.check("DESCRIBE DATASET", r.ok(), f"status={r.status}")


def test_errors(client: Client, t: TestRunner) -> None:
    t.section("Error handling")

    r = client.query(
        f"CREATE FEATURE_SET {FS} FROM {SOURCE} "
        f"ENTITY_KEY(customer_id) FEATURES(tenure) TARGET churn"
    )
    t.check("duplicate CREATE FEATURE_SET fails", not r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("DESCRIBE FEATURE_SET definitely_missing_fs_xyz")
    t.check("DESCRIBE missing feature set fails", not r.ok(), f"status={r.status}")


def cleanup(client: Client, t: TestRunner) -> None:
    t.section("Cleanup")
    for label, sql in [
        ("DROP FEATURE_SET", f"DROP FEATURE_SET IF EXISTS {FS}"),
        ("DROP DATASET", f"DROP DATASET IF EXISTS {DS}"),
        ("DROP TABLE", f"DROP TABLE IF EXISTS {SOURCE}"),
    ]:
        r = client.query(sql)
        t.check(f"cleanup: {label}", r.ok(), f"status={r.status}")

    r = client.query("SHOW FEATURE_SETS")
    t.check("feature set removed", FS not in r.body, f"body={r.body!r}")


def run_tests(client: Client) -> int:
    t = TestRunner()
    setup(client, t)
    test_feature_set_crud(client, t)
    test_dataset_crud(client, t)
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
        print(f"{YELLOW}Using existing server at {args.url}{RESET}")
        return run_tests(Client(args.url))

    with ServerProc(
        Path(args.server_bin) if args.server_bin else None,
        args.url,
    ) as client:
        return run_tests(client)


if __name__ == "__main__":
    sys.exit(main())
