#!/usr/bin/env python3
"""End-to-end API tests for VIEW and MATERIALIZED VIEW support.

Usage:
    python scripts/test_views_api.py
    python scripts/test_views_api.py --url http://127.0.0.1:1143 --no-launch
    python scripts/test_views_api.py --server-bin build/bin/Release/mnemosyne_server.exe

Exit code is 0 if all tests pass, 1 otherwise.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from _mnemo_client import (
    Client,
    ServerProc,
    TestRunner,
    GREEN,
    RED,
    YELLOW,
    RESET,
)

REPO_ROOT = Path(__file__).resolve().parent.parent
DB = "views_test_db"


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


def setup(client: Client, t: TestRunner) -> None:
    t.section("Setup")
    for label, sql in [
        ("CREATE DATABASE", f"CREATE DATABASE IF NOT EXISTS {DB}"),
        ("USE", f"USE {DB}"),
        ("DROP customers", "DROP TABLE IF EXISTS customers"),
        ("DROP active_customers", "DROP VIEW IF EXISTS active_customers"),
        ("DROP young_names", "DROP VIEW IF EXISTS young_names"),
        ("DROP age_counts", "DROP MATERIALIZED VIEW IF EXISTS age_counts"),
        (
            "CREATE customers",
            "CREATE TABLE customers (customer_id Int64, name String, age Int64)",
        ),
        (
            "INSERT customers",
            "INSERT INTO customers VALUES "
            "(1, 'Alice', 25), (2, 'Bob', 30), (3, 'Charlie', 35), (4, 'Diana', 22)",
        ),
    ]:
        r = client.query(sql)
        t.check(f"setup: {label}", r.ok(), f"status={r.status} body={r.body[:200]!r}")


def test_views(client: Client, t: TestRunner) -> None:
    t.section("VIEW: CREATE and SELECT")

    r = client.query(
        "CREATE VIEW active_customers AS SELECT * FROM customers WHERE age < 30"
    )
    t.check("CREATE VIEW returns 200", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("SELECT * FROM active_customers")
    t.check("SELECT FROM view returns 200", r.ok(), f"status={r.status}")
    count = _row_count(r.body)
    t.check("view filters to age < 30 (2 rows)", count == 2, f"rows={count}")

    t.section("VIEW: chaining")

    r = client.query(
        "CREATE VIEW young_names AS SELECT name FROM active_customers"
    )
    t.check("CREATE chained view", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("SELECT name FROM young_names ORDER BY name")
    names = [row.get("name", "") for row in _rows(r.body)]
    t.check("chained view returns names", names == ["Alice", "Diana"], f"names={names}")

    t.section("VIEW: SHOW and DESCRIBE")

    r = client.query("SHOW VIEWS")
    t.check("SHOW VIEWS returns 200", r.ok(), f"status={r.status}")
    body_lower = r.body.lower()
    t.check(
        "SHOW VIEWS lists active_customers",
        "active_customers" in body_lower,
        f"body={r.body!r}",
    )
    t.check(
        "SHOW VIEWS lists young_names",
        "young_names" in body_lower,
        f"body={r.body!r}",
    )

    r = client.query("DESCRIBE VIEW active_customers")
    t.check("DESCRIBE VIEW returns 200", r.ok(), f"status={r.status}")
    t.check(
        "DESCRIBE VIEW includes customer_id",
        "customer_id" in r.body,
        f"body={r.body!r}",
    )

    t.section("VIEW: DROP")

    r = client.query("DROP VIEW young_names")
    t.check("DROP VIEW returns 200", r.ok(), f"status={r.status}")

    r = client.query("SHOW VIEWS")
    t.check(
        "young_names removed after DROP",
        "young_names" not in r.body.lower() and "active_customers" in r.body.lower(),
        f"body={r.body!r}",
    )


def test_materialized_views(client: Client, t: TestRunner) -> None:
    t.section("MATERIALIZED VIEW: CREATE and SELECT")

    r = client.query(
        "CREATE MATERIALIZED VIEW age_counts AS "
        "SELECT age, COUNT(*) AS cnt FROM customers GROUP BY age"
    )
    t.check("CREATE MATERIALIZED VIEW", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("SELECT * FROM age_counts ORDER BY age")
    t.check("SELECT FROM MV returns 200", r.ok(), f"status={r.status}")
    rows = _rows(r.body)
    t.check("MV has grouped rows", len(rows) >= 3, f"rows={rows}")

    t.section("MATERIALIZED VIEW: stale until REFRESH")

    client.query("INSERT INTO customers VALUES (5, 'Eve', 22)")
    r_before = client.query("SELECT cnt FROM age_counts WHERE age = 22")
    cnt_before = _rows(r_before.body)
    t.check(
        "MV unchanged after source insert (before refresh)",
        len(cnt_before) == 1 and int(cnt_before[0].get("cnt", 0)) == 1,
        f"cnt_before={cnt_before}",
    )

    t.section("MATERIALIZED VIEW: REFRESH")

    r = client.query("REFRESH MATERIALIZED VIEW age_counts")
    t.check("REFRESH MATERIALIZED VIEW", r.ok(), f"status={r.status} body={r.body!r}")

    r_after = client.query("SELECT cnt FROM age_counts WHERE age = 22")
    cnt_after = _rows(r_after.body)
    t.check(
        "MV updated after REFRESH (age 22 count = 2)",
        len(cnt_after) == 1 and int(cnt_after[0].get("cnt", 0)) == 2,
        f"cnt_after={cnt_after}",
    )

    t.section("MATERIALIZED VIEW: SHOW and DESCRIBE")

    r = client.query("SHOW MATERIALIZED VIEWS")
    t.check("SHOW MATERIALIZED_VIEWS", r.ok(), f"status={r.status}")
    t.check("lists age_counts", "age_counts" in r.body.lower(), f"body={r.body!r}")

    r = client.query("DESCRIBE MATERIALIZED VIEW age_counts")
    t.check("DESCRIBE MATERIALIZED VIEW", r.ok(), f"status={r.status}")
    t.check("includes age column", "age" in r.body.lower(), f"body={r.body!r}")

    t.section("MATERIALIZED VIEW: DROP")

    r = client.query("DROP MATERIALIZED VIEW age_counts")
    t.check("DROP MATERIALIZED VIEW", r.ok(), f"status={r.status}")

    r = client.query("SHOW MATERIALIZED VIEWS")
    t.check(
        "age_counts removed after DROP",
        "age_counts" not in r.body.lower(),
        f"body={r.body!r}",
    )


def test_errors(client: Client, t: TestRunner) -> None:
    t.section("Error handling")

    client.query("DROP VIEW IF EXISTS dup_view")
    r = client.query(
        "CREATE VIEW dup_view AS SELECT * FROM customers WHERE age < 30"
    )
    t.check("first CREATE VIEW dup_view", r.ok())

    r = client.query(
        "CREATE VIEW dup_view AS SELECT * FROM customers WHERE age < 30"
    )
    t.check(
        "duplicate CREATE VIEW without IF NOT EXISTS fails",
        not r.ok(),
        f"status={r.status} body={r.body!r}",
    )

    r = client.query("DROP VIEW definitely_missing_view_xyz")
    t.check(
        "DROP missing VIEW fails",
        not r.ok(),
        f"status={r.status} body={r.body!r}",
    )

    r = client.query("SELECT * FROM no_such_view_xyz")
    t.check(
        "SELECT from unknown view fails",
        not r.ok(),
        f"status={r.status} body={r.body!r}",
    )

    client.query("DROP VIEW dup_view")


def run_tests(client: Client) -> int:
    t = TestRunner()
    setup(client, t)
    test_views(client, t)
    test_materialized_views(client, t)
    test_errors(client, t)
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
