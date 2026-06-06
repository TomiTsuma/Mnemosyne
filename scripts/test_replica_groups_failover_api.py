#!/usr/bin/env python3
"""Failover tests for REPLICA_GROUP primary-replica placement.

Usage:
    python scripts/test_replica_groups_failover_api.py
    python scripts/test_replica_groups_failover_api.py --server-bin build/bin/Release/mnemosyne_server.exe
    python scripts/test_replica_groups_failover_api.py --no-launch --url http://127.0.0.1:1143

Exit code is 0 if all tests pass, 1 otherwise.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

from _mnemo_client import Client, ServerProc, TestRunner, YELLOW, RESET

GROUP = "failover_ha"
WORKERS = ("fo_worker_01", "fo_worker_02", "fo_worker_03")
SWEEP_WAIT_S = 25.0


def _rows(body: str) -> list[dict]:
    try:
        data = json.loads(body)
        cols = data.get("columns", [])
        rows = data.get("data", [])
        return [dict(zip(cols, row)) for row in rows]
    except (json.JSONDecodeError, TypeError):
        return []


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


def _primary_node(client: Client, group: str) -> str | None:
    r = client.query("SHOW REPLICATION_STATUS")
    if not r.ok():
        return None
    for row in _rows(r.body):
        if row.get("replica_group", "").lower() != group.lower():
            continue
        if row.get("role", "").lower() == "primary":
            return row.get("node_id", "")
    return None


def _heartbeat_workers(client: Client, exclude: set[str] | None = None) -> None:
    exclude = exclude or set()
    base = client.base_url
    for name in WORKERS:
        if name in exclude:
            continue
        _post_json(
            f"{base}/nodes/heartbeat",
            {"id": name, "cpu_pct": 2.0, "memory_bytes": 2048, "status": "ONLINE"},
        )


def run_tests(client: Client) -> int:
    t = TestRunner()
    base = client.base_url

    t.section("Setup workers and replica group")
    for i, name in enumerate(WORKERS, start=1):
        client.query(f"REMOVE NODE IF EXISTS {name}")
        code = _post_json(
            f"{base}/nodes/register",
            {"id": name, "host": "127.0.0.1", "port": 9200 + i, "role": "WORKER"},
        )
        t.check(f"register {name}", code == 200, f"code={code}")

    _heartbeat_workers(client)
    client.query(f"DROP REPLICA_GROUP IF EXISTS {GROUP}")

    r = client.query(f"CREATE REPLICA_GROUP {GROUP} REPLICAS 3 CONSISTENCY QUORUM")
    t.check("CREATE replica group", r.ok(), f"status={r.status} body={r.body!r}")

    primary_before = _primary_node(client, GROUP)
    t.check("initial primary assigned", primary_before is not None, f"primary={primary_before}")
    t.check(
        "primary is a worker (not local)",
        primary_before in WORKERS if primary_before else False,
        f"primary={primary_before}",
    )

    t.section("Stop heartbeats on primary → failover")
    stale_primary = primary_before or WORKERS[0]
    deadline = time.time() + SWEEP_WAIT_S
    while time.time() < deadline:
        _heartbeat_workers(client, exclude={stale_primary})
        time.sleep(2.0)

    primary_after = _primary_node(client, GROUP)
    t.check("primary still exists", primary_after is not None, f"primary={primary_after}")
    t.check(
        "primary moved after stale TTL",
        primary_after != stale_primary,
        f"before={stale_primary} after={primary_after}",
    )

    r = client.query(f"DESCRIBE REPLICA_GROUP {GROUP}")
    t.check("DESCRIBE after failover", r.ok(), r.body)
    describe = {row.get("field", ""): row.get("value", "") for row in _rows(r.body)}
    t.check("failover_events field present", "failover_events" in describe, f"fields={describe}")
    try:
        failover_count = int(describe.get("failover_events", "0"))
    except ValueError:
        failover_count = 0
    t.check("failover count > 0", failover_count > 0, f"failover_events={failover_count}")

    t.section("Resume heartbeats on recovered node")
    _heartbeat_workers(client)
    time.sleep(2.0)
    r = client.query("SHOW REPLICATION_STATUS")
    t.check("status after recovery", r.ok(), r.body)
    rows = _rows(r.body)
    recovered = [row for row in rows if row.get("node_id") == stale_primary]
    t.check("recovered node visible", len(recovered) >= 1, f"rows={recovered}")
    if recovered:
        t.check(
            "recovered member online",
            recovered[0].get("state", "").upper() == "ONLINE",
            f"state={recovered[0]}",
        )

    t.section("Cleanup")
    client.query(f"DROP REPLICA_GROUP IF EXISTS {GROUP}")
    for name in WORKERS:
        client.query(f"REMOVE NODE IF EXISTS {name}")

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
