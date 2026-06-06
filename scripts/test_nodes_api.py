#!/usr/bin/env python3
"""End-to-end API tests for NODE first-class entity support.

Usage:
    python scripts/test_nodes_api.py
    python scripts/test_nodes_api.py --url http://127.0.0.1:1143 --no-launch
    python scripts/test_nodes_api.py --server-bin build/bin/Release/mnemosyne_server.exe
    python scripts/test_nodes_api.py --worker-url http://127.0.0.1:1144 --heartbeat-ttl-test

Exit code is 0 if all tests pass, 1 otherwise.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

from _mnemo_client import (
    Client,
    ServerProc,
    TestRunner,
    YELLOW,
    RESET,
)

REPO_ROOT = Path(__file__).resolve().parent.parent
WORKER_01 = "worker_01"
WORKER_02 = "worker_02"
WORKER_03 = "worker_03"


def _rows(body: str) -> list[dict]:
    try:
        data = json.loads(body)
        cols = data.get("columns", [])
        rows = data.get("data", [])
        return [dict(zip(cols, row)) for row in rows]
    except (json.JSONDecodeError, TypeError):
        return []


def _post_json(url: str, payload: dict, timeout: float = 10.0) -> tuple[int, str]:
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=data,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return resp.status, resp.read().decode("utf-8", "replace")
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode("utf-8", "replace")


def test_show_local(client: Client, t: TestRunner) -> None:
    t.section("SHOW NODES (self node)")

    r = client.query("SHOW NODES")
    t.check("SHOW NODES returns 200", r.ok(), f"status={r.status} body={r.body!r}")
    body_lower = r.body.lower()
    t.check("lists local self node", "local" in body_lower, f"body={r.body!r}")
    t.check("shows HYBRID type", "hybrid" in body_lower, f"body={r.body!r}")
    t.check("shows COORDINATOR role", "coordinator" in body_lower, f"body={r.body!r}")
    t.check("shows ONLINE status", "online" in body_lower, f"body={r.body!r}")


def test_create_register(client: Client, t: TestRunner) -> None:
    t.section("CREATE and REGISTER nodes")

    for name in (WORKER_01, WORKER_02):
        client.query(f"REMOVE NODE IF EXISTS {name}")

    r = client.query(f"CREATE NODE {WORKER_01} TYPE COMPUTE ROLE WORKER")
    t.check("CREATE NODE worker_01", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(
        f"REGISTER NODE {WORKER_02} HOST '127.0.0.1' PORT 9001"
    )
    t.check("REGISTER NODE worker_02", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("SHOW NODES")
    t.check("SHOW lists worker_01", WORKER_01 in r.body.lower(), f"body={r.body!r}")
    t.check("SHOW lists worker_02", WORKER_02 in r.body.lower(), f"body={r.body!r}")


def test_describe(client: Client, t: TestRunner) -> None:
    t.section("DESCRIBE NODE")

    r = client.query(f"DESCRIBE NODE {WORKER_02}")
    t.check("DESCRIBE NODE returns 200", r.ok(), f"status={r.status}")
    body_lower = r.body.lower()
    t.check("includes host", "127.0.0.1" in body_lower, f"body={r.body!r}")
    t.check("includes port", "9001" in body_lower, f"body={r.body!r}")
    t.check("includes cpu_cores", "cpu_cores" in body_lower, f"body={r.body!r}")
    t.check("includes memory_bytes", "memory_bytes" in body_lower, f"body={r.body!r}")
    t.check("includes capabilities", "capabilities" in body_lower, f"body={r.body!r}")


def test_alter(client: Client, t: TestRunner) -> None:
    t.section("ALTER NODE")

    r = client.query(f"ALTER NODE {WORKER_02} SET ROLE WORKER")
    t.check("ALTER NODE SET ROLE", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("CREATE CLUSTER IF NOT EXISTS production")
    t.check("CREATE CLUSTER", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"ALTER NODE {WORKER_02} SET CLUSTER production")
    t.check("ALTER NODE SET CLUSTER", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"DESCRIBE NODE {WORKER_02}")
    t.check("cluster_id set", "production" in r.body.lower(), f"body={r.body!r}")


def test_metrics_capabilities(client: Client, t: TestRunner) -> None:
    t.section("SHOW NODE METRICS / CAPABILITIES")

    r = client.query("SHOW NODE METRICS")
    t.check("SHOW NODE METRICS", r.ok(), f"status={r.status} body={r.body!r}")
    t.check("metrics has rows", len(_rows(r.body)) >= 1, f"body={r.body!r}")

    r = client.query("SHOW NODE CAPABILITIES")
    t.check("SHOW NODE CAPABILITIES", r.ok(), f"status={r.status} body={r.body!r}")
    t.check(
        "capabilities include QUERY_ENGINE or hybrid caps",
        "query_engine" in r.body.lower() or "columnar_storage" in r.body.lower(),
        f"body={r.body!r}",
    )

    r = client.query("SHOW CLUSTERS")
    t.check("SHOW CLUSTERS", r.ok(), f"status={r.status}")
    t.check("lists production", "production" in r.body.lower(), f"body={r.body!r}")


def test_drain_remove(client: Client, t: TestRunner) -> None:
    t.section("DRAIN and REMOVE")

    client.query(f"CREATE NODE IF NOT EXISTS {WORKER_03} TYPE STORAGE ROLE WORKER")
    client.query(f"REGISTER NODE {WORKER_03} HOST '127.0.0.1' PORT 9003")

    r = client.query(f"DRAIN NODE {WORKER_03}")
    t.check("DRAIN NODE", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("SHOW NODES")
    t.check("status DRAINING", "draining" in r.body.lower(), f"body={r.body!r}")

    r = client.query(f"REMOVE NODE {WORKER_03}")
    t.check("REMOVE drained node", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("SHOW NODES")
    t.check("worker_03 gone", WORKER_03 not in r.body.lower(), f"body={r.body!r}")


def test_errors(client: Client, t: TestRunner) -> None:
    t.section("Error handling")

    r = client.query(f"CREATE NODE {WORKER_01}")
    t.check("duplicate CREATE NODE fails", not r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("REMOVE NODE definitely_missing_node_xyz")
    t.check("REMOVE missing node fails", not r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("ALTER NODE missing_node SET ROLE WORKER")
    t.check("ALTER unknown node fails", not r.ok(), f"status={r.status} body={r.body!r}")


def test_http_register_heartbeat(
    client: Client, t: TestRunner, worker_url: str | None
) -> None:
    t.section("HTTP /nodes/register and /nodes/heartbeat")

    base = client.base_url
    r = client.get("/nodes")
    t.check("GET /nodes", r.ok(), f"status={r.status} body={r.body!r}")

    status = client.get("/status")
    t.check("/status includes node_id", "node_id" in status.body.lower(), status.body)

    reg_name = "http_worker"
    client.query(f"REMOVE NODE IF EXISTS {reg_name}")
    code, body = _post_json(
        f"{base}/nodes/register",
        {"id": reg_name, "host": "127.0.0.1", "port": 9010, "role": "WORKER", "type": "COMPUTE"},
    )
    t.check("POST /nodes/register", code == 200, f"code={code} body={body!r}")

    code, body = _post_json(
        f"{base}/nodes/heartbeat",
        {"id": reg_name, "cpu_pct": 12.5, "memory_bytes": 1024, "status": "ONLINE"},
    )
    t.check("POST /nodes/heartbeat", code == 200, f"code={code} body={body!r}")

    r = client.query(f"DESCRIBE NODE {reg_name}")
    t.check("registered node visible via SQL", r.ok() and reg_name in r.body.lower(), r.body)

    if worker_url:
        t.section("Worker heartbeat via HTTP to coordinator")
        code, _ = _post_json(
            f"{base}/nodes/register",
            {"id": "remote_worker", "host": "127.0.0.1", "port": 1144, "role": "WORKER"},
        )
        t.check("register remote worker on coordinator", code == 200, f"code={code}")


def test_heartbeat_offline(client: Client, t: TestRunner) -> None:
    t.section("Heartbeat TTL → OFFLINE")

    stale = "stale_worker"
    client.query(f"REMOVE NODE IF EXISTS {stale}")
    client.query(f"REGISTER NODE {stale} HOST '127.0.0.1' PORT 9020")

    # No heartbeat sent — wait for membership sweep (TTL ~15s, sweep ~5s)
    time.sleep(22)

    r = client.query("SHOW NODES")
    t.check(
        "stale worker marked OFFLINE",
        "offline" in r.body.lower() and stale in r.body.lower(),
        f"body={r.body!r}",
    )
    client.query(f"REMOVE NODE IF EXISTS {stale}")


def test_partitions_replicas(client: Client, t: TestRunner) -> None:
    t.section("SHOW NODE PARTITIONS / REPLICAS (metadata hooks)")

    r = client.query("SHOW NODE PARTITIONS")
    t.check("SHOW NODE PARTITIONS", r.ok(), f"status={r.status}")

    r = client.query("SHOW NODE REPLICAS")
    t.check("SHOW NODE REPLICAS", r.ok(), f"status={r.status}")


def cleanup(client: Client, t: TestRunner) -> None:
    t.section("Cleanup")
    for name in (WORKER_01, WORKER_02, "http_worker", "remote_worker"):
        client.query(f"REMOVE NODE IF EXISTS {name}")
    r = client.query(f"REMOVE NODE IF EXISTS {WORKER_01}")
    t.check("cleanup worker nodes", r.ok() or "unknown" in r.body.lower(), r.body)


def run_tests(
    client: Client,
    worker_url: str | None = None,
    heartbeat_ttl_test: bool = False,
) -> int:
    t = TestRunner()
    test_show_local(client, t)
    test_create_register(client, t)
    test_describe(client, t)
    test_alter(client, t)
    test_metrics_capabilities(client, t)
    test_drain_remove(client, t)
    test_errors(client, t)
    test_http_register_heartbeat(client, t, worker_url)
    test_partitions_replicas(client, t)
    if heartbeat_ttl_test:
        test_heartbeat_offline(client, t)
    cleanup(client, t)
    return t.summary()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:1143")
    parser.add_argument("--worker-url", default=None)
    parser.add_argument("--heartbeat-ttl-test", action="store_true")
    parser.add_argument("--no-launch", action="store_true")
    parser.add_argument("--server-bin", default=None)
    args = parser.parse_args()

    if args.no_launch:
        client = Client(args.url)
        print(f"{YELLOW}Using existing server at {args.url}{RESET}")
        return run_tests(client, args.worker_url, args.heartbeat_ttl_test)

    with ServerProc(
        Path(args.server_bin) if args.server_bin else None,
        args.url,
    ) as client:
        return run_tests(client, args.worker_url, args.heartbeat_ttl_test)


if __name__ == "__main__":
    sys.exit(main())
