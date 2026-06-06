#!/usr/bin/env python3
"""Distributed NODE tests: multi-node registration and remote execution hooks.

Usage:
    python scripts/test_nodes_distributed_api.py
    python scripts/test_nodes_distributed_api.py --coordinator http://127.0.0.1:1143 --worker http://127.0.0.1:1144

Requires two mnemosyne_server processes (coordinator + worker).
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

from _mnemo_client import (
    Client,
    TestRunner,
    YELLOW,
    RESET,
    find_server_bin,
    REPO_ROOT,
)

WORKER_NODE = "dist_worker"


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


def wait_server(url: str, timeout: float = 30.0) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with urllib.request.urlopen(url + "/ping", timeout=2) as resp:
                if resp.status == 200:
                    return True
        except (urllib.error.URLError, TimeoutError):
            pass
        time.sleep(0.3)
    return False


def run_tests(coord: Client, worker_url: str, t: TestRunner) -> None:
    t.section("Register worker on coordinator")

    code = _post_json(
        f"{coord.base_url}/nodes/register",
        {
            "id": WORKER_NODE,
            "host": "127.0.0.1",
            "port": int(worker_url.rsplit(":", 1)[-1]),
            "type": "COMPUTE",
            "role": "WORKER",
        },
    )
    t.check("worker registered", code == 200, f"code={code}")

    code = _post_json(
        f"{coord.base_url}/nodes/heartbeat",
        {"id": WORKER_NODE, "cpu_pct": 5.0, "memory_bytes": 2048, "status": "ONLINE"},
    )
    t.check("heartbeat accepted", code == 200, f"code={code}")

    r = coord.query("SHOW NODES")
    t.check("coordinator lists worker", WORKER_NODE in r.body.lower(), r.body)

    t.section("Query with EXCHANGE plan (multi-node)")
    r = coord.query("SELECT 1")
    t.check("SELECT on multi-node cluster", r.ok(), f"status={r.status} body={r.body!r}")

    t.section("SHOW NODE METRICS after query")
    r = coord.query("SHOW NODE METRICS")
    t.check("metrics available", r.ok(), r.body)

    t.section("GPU scheduler smoke (capability placement)")
    coord.query(f"CREATE NODE IF NOT EXISTS gpu_01 TYPE GPU ROLE WORKER")
    coord.query("REGISTER NODE gpu_01 HOST '127.0.0.1' PORT 9011")
    r = coord.query("DESCRIBE NODE gpu_01")
    t.check("GPU node created", r.ok() and "gpu" in r.body.lower(), r.body)
    t.check(
        "GPU capabilities",
        "ml_training" in r.body.lower() or "ml_inference" in r.body.lower(),
        r.body,
    )
    coord.query("REMOVE NODE IF EXISTS gpu_01")
    coord.query(f"REMOVE NODE IF EXISTS {WORKER_NODE}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--coordinator", default="http://127.0.0.1:1143")
    parser.add_argument("--worker", default="http://127.0.0.1:1144")
    parser.add_argument("--launch-worker", action="store_true")
    parser.add_argument("--server-bin", default=None)
    args = parser.parse_args()

    t = TestRunner()
    worker_proc = None
    coord_proc = None

    try:
        if args.launch_worker:
            bin_path = find_server_bin(args.server_bin)
            if not bin_path:
                print("mnemosyne_server binary not found", file=sys.stderr)
                return 1
            coord_port = args.coordinator.rsplit(":", 1)[-1]
            worker_port = args.worker.rsplit(":", 1)[-1]
            coord_proc = subprocess.Popen(
                [str(bin_path), coord_port],
                cwd=str(REPO_ROOT),
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            if not wait_server(args.coordinator):
                t.check("coordinator server started", False, args.coordinator)
                return t.summary()
            t.check("coordinator server started", True, args.coordinator)
            worker_proc = subprocess.Popen(
                [str(bin_path), worker_port],
                cwd=str(REPO_ROOT),
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            if not wait_server(args.worker):
                t.check("worker server started", False, args.worker)
                return t.summary()
            t.check("worker server started", True, args.worker)

        if not wait_server(args.coordinator):
            print(f"{YELLOW}Coordinator not reachable at {args.coordinator}{RESET}")
            return 1

        coord = Client(args.coordinator)
        print(f"{YELLOW}Coordinator: {args.coordinator} Worker: {args.worker}{RESET}")
        run_tests(coord, args.worker, t)
        return t.summary()
    finally:
        if worker_proc:
            worker_proc.terminate()
            worker_proc.wait(timeout=5)
        if coord_proc:
            coord_proc.terminate()
            coord_proc.wait(timeout=5)


if __name__ == "__main__":
    sys.exit(main())
