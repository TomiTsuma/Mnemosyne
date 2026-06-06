#!/usr/bin/env python3
"""End-to-end API tests for SHARD_GROUP first-class entity support.

Usage:
    python scripts/test_shard_groups_api.py
    python scripts/test_shard_groups_api.py --url http://127.0.0.1:1143 --no-launch
    python scripts/test_shard_groups_api.py --server-bin build/bin/Release/mnemosyne_server.exe

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

DB = "sg_test_db"
GROUP = "customer_distribution"
RG_GROUP = "sg_standard_ha"
TABLE = "sg_customers"
COMBO_TABLE = "sg_sales_combo"
WORKERS = ("sg_worker_01", "sg_worker_02", "sg_worker_03")


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
            {"id": name, "host": "127.0.0.1", "port": 9200 + i, "role": "WORKER", "type": "COMPUTE"},
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
        ("DROP COMBO TABLE", f"DROP TABLE IF EXISTS {COMBO_TABLE}"),
        ("DROP TABLE", f"DROP TABLE IF EXISTS {TABLE}"),
        ("DROP SHARD GROUP", f"DROP SHARD_GROUP IF EXISTS {GROUP}"),
        ("DROP REPLICA GROUP", f"DROP REPLICA_GROUP IF EXISTS {RG_GROUP}"),
    ]:
        r = client.query(sql)
        t.check(f"setup: {label}", r.ok(), f"status={r.status} body={r.body[:300]!r}")


def test_create_show(client: Client, t: TestRunner) -> None:
    t.section("CREATE and SHOW SHARD_GROUPS")

    r = client.query(
        f"CREATE SHARD_GROUP {GROUP} TYPE HASH KEY customer_id SHARDS 16"
    )
    t.check("CREATE SHARD_GROUP", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("SHOW SHARD_GROUPS")
    t.check("SHOW SHARD_GROUPS", r.ok(), f"status={r.status}")
    body_lower = r.body.lower()
    t.check(f"lists {GROUP}", GROUP in body_lower, f"body={r.body!r}")
    t.check("shard_count 16", "16" in r.body, f"body={r.body!r}")
    t.check("type hash", "hash" in body_lower, f"body={r.body!r}")
    t.check("key customer_id", "customer_id" in body_lower, f"body={r.body!r}")


def test_describe(client: Client, t: TestRunner) -> None:
    t.section("DESCRIBE SHARD_GROUP")

    r = client.query(f"DESCRIBE SHARD_GROUP {GROUP}")
    t.check("DESCRIBE returns 200", r.ok(), f"status={r.status}")
    body_lower = r.body.lower()
    t.check("includes name", "name" in body_lower, f"body={r.body!r}")
    t.check("includes shard_count", "shard_count" in body_lower, f"body={r.body!r}")
    t.check("includes shard_key", "shard_key" in body_lower, f"body={r.body!r}")
    t.check("includes type", "type" in body_lower, f"body={r.body!r}")


def test_shard_status(client: Client, t: TestRunner) -> None:
    t.section("SHOW SHARDS and SHOW SHARD_STATUS")

    r = client.query("SHOW SHARDS")
    t.check("SHOW SHARDS", r.ok(), f"status={r.status} body={r.body!r}")
    rows = _rows(r.body)
    group_rows = [row for row in rows if row.get("shard_group", "").lower() == GROUP]
    t.check("has 16 shards", len(group_rows) == 16, f"count={len(group_rows)}")
    nodes = {row.get("node_id", "") for row in group_rows if row.get("node_id")}
    t.check("shards assigned to nodes", len(nodes) >= 1, f"nodes={nodes}")

    r = client.query("SHOW SHARD_STATUS")
    t.check("SHOW SHARD_STATUS", r.ok(), f"status={r.status}")
    status_rows = _rows(r.body)
    status_group = [row for row in status_rows if row.get("shard_group", "").lower() == GROUP]
    t.check("status has 16 shards", len(status_group) == 16, f"count={len(status_group)}")
    t.check("status has row_count column", "row_count" in r.body.lower(), f"body={r.body!r}")


def test_node_partitions(client: Client, t: TestRunner) -> None:
    t.section("SHOW NODE PARTITIONS")

    r = client.query("SHOW NODE PARTITIONS")
    t.check("SHOW NODE PARTITIONS", r.ok(), f"status={r.status}")
    rows = _rows(r.body)
    shard_ids = {row.get("partition_id", "") for row in rows}
    t.check("partition customer_distribution/s0", f"{GROUP}/s0" in shard_ids, f"ids={shard_ids}")


def test_table_attachment(client: Client, t: TestRunner) -> None:
    t.section("CREATE TABLE with SHARD_GROUP")

    r = client.query(
        f"CREATE TABLE {TABLE} (id Float64, customer_id Float64) "
        f"ENGINE=Memory SHARD_GROUP {GROUP}"
    )
    t.check("CREATE TABLE with SHARD_GROUP", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"INSERT INTO {TABLE} VALUES (1.0, 10.0), (2.0, 20.0)")
    t.check("INSERT", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"SELECT * FROM {TABLE} ORDER BY id")
    t.check("SELECT", r.ok(), f"status={r.status}")
    t.check("SELECT 2 rows", _row_count(r.body) == 2, f"body={r.body!r}")


def test_combined_attachment(client: Client, t: TestRunner) -> None:
    t.section("CREATE TABLE with SHARD_GROUP and REPLICA_GROUP")

    r = client.query(
        f"CREATE REPLICA_GROUP {RG_GROUP} REPLICAS 3 CONSISTENCY QUORUM"
    )
    t.check("CREATE REPLICA_GROUP for combo", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(
        f"CREATE TABLE {COMBO_TABLE} (id Float64, val Float64) "
        f"ENGINE=Memory SHARD_GROUP {GROUP} REPLICA_GROUP {RG_GROUP}"
    )
    t.check("CREATE TABLE combo attach", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"INSERT INTO {COMBO_TABLE} VALUES (1.0, 100.0)")
    t.check("INSERT combo", r.ok(), f"status={r.status} body={r.body!r}")


def test_alter(client: Client, t: TestRunner) -> None:
    t.section("ALTER SHARD_GROUP")

    r = client.query(f"ALTER SHARD_GROUP {GROUP} SET SHARDS 32")
    t.check("ALTER SET SHARDS", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("SHOW SHARDS")
    rows = _rows(r.body)
    group_rows = [row for row in rows if row.get("shard_group", "").lower() == GROUP]
    t.check("32 shards after resize", len(group_rows) == 32, f"count={len(group_rows)}")

    r = client.query(f"DESCRIBE SHARD_GROUP {GROUP}")
    t.check("describe shard_count 32", "32" in r.body, f"body={r.body!r}")


def test_errors(client: Client, t: TestRunner) -> None:
    t.section("Error handling")

    r = client.query(
        f"CREATE SHARD_GROUP {GROUP} TYPE HASH KEY customer_id SHARDS 16"
    )
    t.check("duplicate CREATE fails", not r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("DROP SHARD_GROUP definitely_missing_sg_xyz")
    t.check("DROP missing fails", not r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"DROP SHARD_GROUP {GROUP}")
    t.check("DROP in-use fails", not r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(
        "CREATE TABLE bad_sg (id Float64) ENGINE=Memory SHARD_GROUP missing_sg_xyz"
    )
    t.check("CREATE TABLE unknown group fails", not r.ok(), f"status={r.status} body={r.body!r}")


def cleanup(client: Client, t: TestRunner) -> None:
    t.section("Cleanup")

    client.query(f"DROP TABLE IF EXISTS {COMBO_TABLE}")
    client.query(f"DROP TABLE IF EXISTS {TABLE}")
    r = client.query(f"DROP REPLICA_GROUP {RG_GROUP}")
    t.check("DROP REPLICA_GROUP", r.ok(), f"status={r.status}")
    r = client.query(f"DROP SHARD_GROUP {GROUP}")
    t.check("DROP SHARD_GROUP after tables removed", r.ok(), f"status={r.status}")


def run_tests(client: Client) -> int:
    t = TestRunner()
    setup(client, t)
    test_create_show(client, t)
    test_describe(client, t)
    test_shard_status(client, t)
    test_node_partitions(client, t)
    test_table_attachment(client, t)
    test_combined_attachment(client, t)
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
