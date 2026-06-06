#!/usr/bin/env python3
"""End-to-end API tests for REPLICA_GROUP first-class entity support.

Usage:
    python scripts/test_replica_groups_api.py
    python scripts/test_replica_groups_api.py --url http://127.0.0.1:1143 --no-launch
    python scripts/test_replica_groups_api.py --server-bin build/bin/Release/mnemosyne_server.exe

Exit code is 0 if all tests pass, 1 otherwise.
"""

from __future__ import annotations

import argparse
import json
import sys
import urllib.error
import urllib.request
from pathlib import Path

from _mnemo_client import (
    Client,
    ServerProc,
    TestRunner,
    YELLOW,
    RESET,
)

DB = "rg_test_db"
GROUP = "standard_ha"
TABLE = "rg_sales"
WORKERS = ("rg_worker_01", "rg_worker_02", "rg_worker_03")


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


def _post_json(url: str, payload: dict) -> int:
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=data,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            return resp.status
    except urllib.error.HTTPError as e:
        return e.code


def register_workers(client: Client, t: TestRunner) -> None:
    t.section("Register worker nodes")
    base = client.base_url
    for i, name in enumerate(WORKERS, start=1):
        client.query(f"REMOVE NODE IF EXISTS {name}")
        code = _post_json(
            f"{base}/nodes/register",
            {"id": name, "host": "127.0.0.1", "port": 9100 + i, "role": "WORKER", "type": "COMPUTE"},
        )
        t.check(f"register {name}", code == 200, f"code={code}")
        code = _post_json(
            f"{base}/nodes/heartbeat",
            {"id": name, "cpu_pct": 1.0, "memory_bytes": 1024, "status": "ONLINE"},
        )
        t.check(f"heartbeat {name}", code == 200, f"code={code}")


def setup(client: Client, t: TestRunner) -> None:
    t.section("Setup")
    register_workers(client, t)
    for label, sql in [
        ("CREATE DATABASE", f"CREATE DATABASE IF NOT EXISTS {DB}"),
        ("USE", f"USE {DB}"),
        ("DROP TABLE", f"DROP TABLE IF EXISTS {TABLE}"),
        ("DROP GROUP", f"DROP REPLICA_GROUP IF EXISTS {GROUP}"),
    ]:
        r = client.query(sql)
        t.check(f"setup: {label}", r.ok(), f"status={r.status} body={r.body[:300]!r}")


def test_create_show(client: Client, t: TestRunner) -> None:
    t.section("CREATE and SHOW REPLICA_GROUPS")

    r = client.query(
        f"CREATE REPLICA_GROUP {GROUP} REPLICAS 3 CONSISTENCY QUORUM"
    )
    t.check("CREATE REPLICA_GROUP", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("SHOW REPLICA_GROUPS")
    t.check("SHOW REPLICA_GROUPS", r.ok(), f"status={r.status}")
    body_lower = r.body.lower()
    t.check(f"lists {GROUP}", GROUP in body_lower, f"body={r.body!r}")
    t.check("factor 3", "3" in r.body, f"body={r.body!r}")
    t.check("consistency quorum", "quorum" in body_lower, f"body={r.body!r}")


def test_describe(client: Client, t: TestRunner) -> None:
    t.section("DESCRIBE REPLICA_GROUP")

    r = client.query(f"DESCRIBE REPLICA_GROUP {GROUP}")
    t.check("DESCRIBE returns 200", r.ok(), f"status={r.status}")
    body_lower = r.body.lower()
    t.check("includes name", "name" in body_lower, f"body={r.body!r}")
    t.check("includes replication_factor", "replication_factor" in body_lower, f"body={r.body!r}")
    t.check("includes consistency", "consistency" in body_lower, f"body={r.body!r}")
    t.check("includes strategy", "strategy" in body_lower, f"body={r.body!r}")


def test_replication_status(client: Client, t: TestRunner) -> None:
    t.section("SHOW REPLICATION_STATUS")

    r = client.query("SHOW REPLICATION_STATUS")
    t.check("SHOW REPLICATION_STATUS", r.ok(), f"status={r.status} body={r.body!r}")
    rows = _rows(r.body)
    group_rows = [row for row in rows if row.get("replica_group", "").lower() == GROUP]
    t.check("has 3 members", len(group_rows) == 3, f"rows={group_rows}")
    roles = {row.get("role", "").lower() for row in group_rows}
    t.check("one primary", "primary" in roles, f"roles={roles}")
    t.check("has replicas", "replica" in roles, f"roles={roles}")
    nodes = {row.get("node_id", "") for row in group_rows}
    t.check("distinct nodes", len(nodes) == 3, f"nodes={nodes}")


def test_table_attachment(client: Client, t: TestRunner) -> None:
    t.section("CREATE TABLE with REPLICA_GROUP")

    r = client.query(
        f"CREATE TABLE {TABLE} (id Float64, val Float64) "
        f"ENGINE=Memory REPLICA_GROUP {GROUP}"
    )
    t.check("CREATE TABLE with REPLICA_GROUP", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"INSERT INTO {TABLE} VALUES (1.0, 10.0), (2.0, 20.0)")
    t.check("INSERT", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"SELECT * FROM {TABLE} ORDER BY id")
    t.check("SELECT", r.ok(), f"status={r.status}")
    t.check("SELECT 2 rows", _row_count(r.body) == 2, f"body={r.body!r}")


def test_alter(client: Client, t: TestRunner) -> None:
    t.section("ALTER REPLICA_GROUP")

    r = client.query(f"ALTER REPLICA_GROUP {GROUP} SET REPLICAS 5")
    t.check("ALTER SET REPLICAS", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("SHOW REPLICATION_STATUS")
    rows = _rows(r.body)
    group_rows = [row for row in rows if row.get("replica_group", "").lower() == GROUP]
    t.check("5 members after resize", len(group_rows) == 5, f"count={len(group_rows)}")

    r = client.query(f"ALTER REPLICA_GROUP {GROUP} SET CONSISTENCY SYNCHRONOUS")
    t.check("ALTER SET CONSISTENCY", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"DESCRIBE REPLICA_GROUP {GROUP}")
    t.check("consistency synchronous", "synchronous" in r.body.lower(), f"body={r.body!r}")


def test_errors(client: Client, t: TestRunner) -> None:
    t.section("Error handling")

    r = client.query(f"CREATE REPLICA_GROUP {GROUP} REPLICAS 3")
    t.check("duplicate CREATE fails", not r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("DROP REPLICA_GROUP definitely_missing_rg_xyz")
    t.check("DROP missing fails", not r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"DROP REPLICA_GROUP {GROUP}")
    t.check("DROP in-use fails", not r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(
        "CREATE TABLE bad_rg (id Float64) ENGINE=Memory REPLICA_GROUP missing_rg_xyz"
    )
    t.check("CREATE TABLE unknown group fails", not r.ok(), f"status={r.status} body={r.body!r}")


def cleanup(client: Client, t: TestRunner) -> None:
    t.section("Cleanup")

    client.query(f"DROP TABLE IF EXISTS {TABLE}")
    r = client.query(f"DROP REPLICA_GROUP {GROUP}")
    t.check("DROP REPLICA_GROUP after table removed", r.ok(), f"status={r.status}")


def run_tests(client: Client) -> int:
    t = TestRunner()
    setup(client, t)
    test_create_show(client, t)
    test_describe(client, t)
    test_replication_status(client, t)
    test_table_attachment(client, t)
    test_alter(client, t)
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
