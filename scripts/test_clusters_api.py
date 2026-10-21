#!/usr/bin/env python3.11
"""End-to-end API tests for the CLUSTER first-class entity (docs/PRDS/CLUSTER_.md, Phase 1).

Covers:
  * CREATE / SHOW / DESCRIBE / DROP CLUSTER
  * ALTER CLUSTER ... ADD NODE / REMOVE NODE, including all-or-nothing multi-step statements,
    the one-cluster-per-node rule, and the single-node case (the self node)
  * cluster state derived from member health (CREATING / ONLINE / DEGRADED / OFFLINE)
  * SHOW CLUSTER STATUS and SHOW CLUSTER METRICS

Usage:
    python3.11 scripts/test_clusters_api.py --no-launch
    python3.11 scripts/test_clusters_api.py --url http://127.0.0.1:1143 --no-launch

The server must already be running. Exit code is 0 if all tests pass, 1 otherwise.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from _mnemo_client import Client, ServerProc, TestRunner, YELLOW, RESET

PROD = "cl_prod"
STAGE = "cl_stage"
SINGLE = "cl_single"
W1, W2, W3 = "cl_w1", "cl_w2", "cl_w3"
SELF_NODE = "local"
SELF_DEFAULT_CLUSTER = "default"  # what the server seeds the self node with at startup


def _rows(body: str) -> list[dict]:
    try:
        parsed = json.loads(body)
        return [dict(zip(parsed.get("columns", []), row)) for row in parsed.get("data", [])]
    except (json.JSONDecodeError, TypeError):
        return []


class Ctx:
    def __init__(self, client: Client, t: TestRunner) -> None:
        self.client, self.t = client, t

    def ok(self, name: str, sql: str) -> bool:
        r = self.client.query(sql)
        return self.t.check(name, r.ok(), f"status={r.status} body={r.body[:240]!r}")

    def fails(self, name: str, sql: str, needle: str = "") -> bool:
        r = self.client.query(sql)
        good = (not r.ok()) and needle.lower() in r.body.lower()
        return self.t.check(name, good, f"status={r.status} body={r.body[:240]!r}")

    def rows(self, sql: str) -> list[dict]:
        return _rows(self.client.query(sql).body)

    def cluster_row(self, sql: str, name: str) -> dict:
        for row in self.rows(sql):
            if row.get("name") == name:
                return row
        return {}

    def describe(self, kind: str, name: str) -> dict:
        return {row.get("field"): row.get("value") for row in self.rows(f"DESCRIBE {kind} {name}")}

    def node_cluster(self, node: str) -> str:
        return self.describe("NODE", node).get("cluster_id", "")

    def set_status(self, node: str, status: str) -> None:
        self.ok(f"{node} -> {status}", f"ALTER NODE {node} SET STATUS {status}")


def setup(x: Ctx) -> None:
    x.t.section("Setup")
    for cluster in (PROD, STAGE, SINGLE):
        x.ok(f"drop leftover cluster {cluster}", f"DROP CLUSTER IF EXISTS {cluster}")
    for node in (W1, W2, W3):
        x.ok(f"drop leftover node {node}", f"REMOVE NODE IF EXISTS {node}")
        x.ok(f"create node {node}", f"CREATE NODE {node} TYPE COMPUTE ROLE WORKER")
        x.set_status(node, "ONLINE")


def test_create_show_describe(x: Ctx) -> None:
    x.t.section("CREATE / SHOW / DESCRIBE CLUSTER")
    x.ok("CREATE CLUSTER", f"CREATE CLUSTER {PROD}")
    x.fails("duplicate cluster is rejected", f"CREATE CLUSTER {PROD}", "exists")
    x.ok("IF NOT EXISTS is idempotent", f"CREATE CLUSTER IF NOT EXISTS {PROD}")
    x.ok("second cluster", f"CREATE CLUSTER {STAGE}")

    shown = x.rows("SHOW CLUSTERS")
    names = {r.get("name") for r in shown}
    x.t.check("SHOW CLUSTERS lists both", {PROD, STAGE} <= names, f"names={sorted(names)}")
    row = x.cluster_row("SHOW CLUSTERS", PROD)
    x.t.check("an empty cluster has no members", row.get("node_count") == "0", f"row={row}")
    x.t.check("an empty cluster is CREATING", row.get("status") == "CREATING", f"row={row}")

    described = x.describe("CLUSTER", PROD)
    x.t.check("DESCRIBE shows the name", described.get("name") == PROD, f"described={described}")
    x.t.check("DESCRIBE shows status and member count",
              described.get("status") == "CREATING" and described.get("node_count") == "0",
              f"described={described}")
    x.fails("DESCRIBE of an unknown cluster", "DESCRIBE CLUSTER cl_nope", "cl_nope")


def test_membership(x: Ctx) -> None:
    x.t.section("ALTER CLUSTER ADD / REMOVE NODE")
    x.ok("ADD NODE", f"ALTER CLUSTER {PROD} ADD NODE {W1}")
    described = x.describe("CLUSTER", PROD)
    x.t.check("member is listed", described.get("nodes") == W1 and described.get("node_count") == "1",
              f"described={described}")
    x.t.check("the node records its cluster", x.node_cluster(W1) == PROD, f"cluster_id={x.node_cluster(W1)!r}")

    x.ok("adding a current member again is a no-op", f"ALTER CLUSTER {PROD} ADD NODE {W1}")
    x.t.check("no duplicate member", x.describe("CLUSTER", PROD).get("node_count") == "1")

    x.ok("several steps in one statement", f"ALTER CLUSTER {PROD} ADD NODE {W3}, ADD NODE {W2}")
    described = x.describe("CLUSTER", PROD)
    x.t.check("members are listed sorted by name",
              described.get("nodes") == f"{W1}, {W2}, {W3}" and described.get("node_count") == "3",
              f"described={described}")
    x.t.check("SHOW CLUSTERS agrees", x.cluster_row("SHOW CLUSTERS", PROD).get("node_count") == "3")

    x.ok("REMOVE NODE", f"ALTER CLUSTER {PROD} REMOVE NODE {W3}")
    x.t.check("member is gone", x.describe("CLUSTER", PROD).get("nodes") == f"{W1}, {W2}")
    x.t.check("the node forgets its cluster", x.node_cluster(W3) == "", f"cluster_id={x.node_cluster(W3)!r}")

    x.ok("mixed ADD and REMOVE in one statement",
         f"ALTER CLUSTER {PROD} REMOVE NODE {W2}, ADD NODE {W3}")
    x.t.check("both steps applied", x.describe("CLUSTER", PROD).get("nodes") == f"{W1}, {W3}")
    x.ok("restore", f"ALTER CLUSTER {PROD} REMOVE NODE {W3}, ADD NODE {W2}")


def test_membership_errors(x: Ctx) -> None:
    x.t.section("Membership errors leave nothing half-done")
    before = x.describe("CLUSTER", PROD).get("nodes")

    x.fails("unknown node", f"ALTER CLUSTER {PROD} ADD NODE cl_ghost", "cl_ghost")
    x.fails("unknown cluster", f"ALTER CLUSTER cl_nope ADD NODE {W3}", "cl_nope")
    x.fails("removing a non-member", f"ALTER CLUSTER {PROD} REMOVE NODE {W3}", "not a member")

    x.fails("a failing second step undoes the first",
            f"ALTER CLUSTER {PROD} ADD NODE {W3}, ADD NODE cl_ghost", "cl_ghost")
    x.t.check("the first step was rolled back", x.node_cluster(W3) == "", f"cluster_id={x.node_cluster(W3)!r}")
    x.t.check("membership is unchanged", x.describe("CLUSTER", PROD).get("nodes") == before)

    x.fails("a failing REMOVE undoes an earlier ADD",
            f"ALTER CLUSTER {PROD} ADD NODE {W3}, REMOVE NODE {W3}, REMOVE NODE {W3}", "not a member")
    x.t.check("nothing leaked through", x.node_cluster(W3) == "")

    x.fails("a node cannot belong to two clusters", f"ALTER CLUSTER {STAGE} ADD NODE {W1}", "already belongs")
    x.t.check("the refusal named the current cluster",
              PROD in x.client.query(f"ALTER CLUSTER {STAGE} ADD NODE {W1}").body)
    x.t.check("the node stayed where it was", x.node_cluster(W1) == PROD)
    x.ok("it can move once removed", f"ALTER CLUSTER {PROD} REMOVE NODE {W1}")
    x.ok("... and added elsewhere", f"ALTER CLUSTER {STAGE} ADD NODE {W1}")
    x.t.check("now a member of the other cluster", x.node_cluster(W1) == STAGE)
    x.ok("move it back: leave the other cluster", f"ALTER CLUSTER {STAGE} REMOVE NODE {W1}")
    x.ok("... and rejoin the original", f"ALTER CLUSTER {PROD} ADD NODE {W1}")

    x.fails("ALTER CLUSTER needs ADD or REMOVE", f"ALTER CLUSTER {PROD} SET x y", "ADD or REMOVE")
    x.fails("ADD needs the NODE keyword", f"ALTER CLUSTER {PROD} ADD {W3}", "NODE")


def test_single_node(x: Ctx) -> None:
    x.t.section("Single-node cluster (the self node)")
    x.t.check("the self node starts with the seeded cluster_id",
              x.node_cluster(SELF_NODE) == SELF_DEFAULT_CLUSTER, f"cluster_id={x.node_cluster(SELF_NODE)!r}")
    x.ok("CREATE CLUSTER", f"CREATE CLUSTER {SINGLE}")
    x.ok("the self node can join despite its seeded cluster_id", f"ALTER CLUSTER {SINGLE} ADD NODE {SELF_NODE}")
    row = x.cluster_row("SHOW CLUSTERS", SINGLE)
    x.t.check("a one-node cluster of a live node is ONLINE",
              row.get("status") == "ONLINE" and row.get("node_count") == "1", f"row={row}")
    x.ok("leave", f"ALTER CLUSTER {SINGLE} REMOVE NODE {SELF_NODE}")
    x.ok("restore the self node's seeded cluster_id", f"ALTER NODE {SELF_NODE} SET CLUSTER {SELF_DEFAULT_CLUSTER}")
    x.ok("drop", f"DROP CLUSTER {SINGLE}")


def test_state_and_status(x: Ctx) -> None:
    x.t.section("Cluster state and SHOW CLUSTER STATUS")
    x.t.check("all members up is ONLINE", x.cluster_row("SHOW CLUSTERS", PROD).get("status") == "ONLINE")
    status = x.cluster_row("SHOW CLUSTER STATUS", PROD)
    x.t.check("STATUS counts online members",
              status.get("status") == "ONLINE" and status.get("node_count") == "2"
              and status.get("online") == "2" and status.get("offline") == "0", f"row={status}")

    x.set_status(W2, "OFFLINE")
    status = x.cluster_row("SHOW CLUSTER STATUS", PROD)
    x.t.check("one member offline is DEGRADED",
              status.get("status") == "DEGRADED" and status.get("online") == "1"
              and status.get("offline") == "1", f"row={status}")
    x.t.check("DESCRIBE reports the same state", x.describe("CLUSTER", PROD).get("status") == "DEGRADED")

    x.set_status(W1, "OFFLINE")
    x.t.check("every member offline is OFFLINE",
              x.cluster_row("SHOW CLUSTER STATUS", PROD).get("status") == "OFFLINE")

    x.set_status(W1, "ONLINE")
    x.set_status(W2, "DRAINING")
    status = x.cluster_row("SHOW CLUSTER STATUS", PROD)
    x.t.check("a draining member also degrades the cluster",
              status.get("status") == "DEGRADED" and status.get("offline") == "0", f"row={status}")

    x.set_status(W2, "ONLINE")
    x.t.check("recovers to ONLINE", x.cluster_row("SHOW CLUSTER STATUS", PROD).get("status") == "ONLINE")
    x.t.check("an empty cluster stays CREATING",
              x.cluster_row("SHOW CLUSTER STATUS", STAGE).get("status") == "CREATING")

    x.fails("SHOW CLUSTER needs STATUS or METRICS", "SHOW CLUSTER", "STATUS or METRICS")


def test_metrics(x: Ctx) -> None:
    x.t.section("SHOW CLUSTER METRICS")
    per_node = x.describe("NODE", W1)
    cores = int(per_node.get("cpu_cores", "0"))
    memory = int(per_node.get("memory_bytes", "0"))
    x.t.check("the member node reports capacity", cores > 0 and memory > 0, f"node={per_node}")

    row = x.cluster_row("SHOW CLUSTER METRICS", PROD)
    expected = {"name", "node_count", "cpu_cores", "cpu_utilization_pct", "memory_bytes", "memory_used_bytes",
                "storage_bytes", "storage_used_bytes", "gpu_count", "query_throughput", "pipeline_throughput",
                "stream_throughput"}
    x.t.check("every metric column is present", expected <= set(row), f"columns={sorted(row)}")
    x.t.check("capacity is summed over the two members",
              row.get("node_count") == "2" and row.get("cpu_cores") == str(2 * cores)
              and row.get("memory_bytes") == str(2 * memory), f"row={row}")

    empty = x.cluster_row("SHOW CLUSTER METRICS", STAGE)
    x.t.check("an empty cluster has no capacity",
              empty.get("node_count") == "0" and empty.get("cpu_cores") == "0"
              and empty.get("memory_bytes") == "0", f"row={empty}")


def test_drop(x: Ctx) -> None:
    x.t.section("DROP CLUSTER")
    x.ok("DROP an empty cluster", f"DROP CLUSTER {STAGE}")
    names = {r.get("name") for r in x.rows("SHOW CLUSTERS")}
    x.t.check("it is gone from SHOW", STAGE not in names, f"names={sorted(names)}")
    x.fails("DESCRIBE after DROP", f"DESCRIBE CLUSTER {STAGE}", STAGE)
    x.fails("DROP of an unknown cluster", "DROP CLUSTER cl_nope", "cl_nope")
    x.ok("DROP IF EXISTS of an unknown cluster", "DROP CLUSTER IF EXISTS cl_nope")
    x.fails("ALTER of a dropped cluster", f"ALTER CLUSTER {STAGE} ADD NODE {W3}", STAGE)

    x.t.check("members still point at the cluster before it is dropped", x.node_cluster(W1) == PROD)
    x.ok("DROP a cluster that has members", f"DROP CLUSTER {PROD}")
    x.t.check("its members are detached, not left dangling",
              x.node_cluster(W1) == "" and x.node_cluster(W2) == "",
              f"w1={x.node_cluster(W1)!r} w2={x.node_cluster(W2)!r}")
    x.t.check("the detached nodes still exist",
              {W1, W2} <= {r.get("name") or r.get("node_name") for r in x.rows("SHOW NODES")})
    x.ok("a detached node can join a new cluster", f"CREATE CLUSTER {STAGE}")
    x.ok("... like this one", f"ALTER CLUSTER {STAGE} ADD NODE {W1}")


def cleanup(x: Ctx) -> None:
    x.t.section("Cleanup")
    for cluster in (PROD, STAGE, SINGLE):
        x.ok(f"drop cluster {cluster}", f"DROP CLUSTER IF EXISTS {cluster}")
    for node in (W1, W2, W3):
        x.ok(f"remove node {node}", f"REMOVE NODE IF EXISTS {node}")


def run_tests(client: Client) -> int:
    t = TestRunner()
    x = Ctx(client, t)
    setup(x)
    test_create_show_describe(x)
    test_membership(x)
    test_membership_errors(x)
    test_single_node(x)
    test_state_and_status(x)
    test_metrics(x)
    test_drop(x)
    cleanup(x)
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
    with ServerProc(Path(args.server_bin) if args.server_bin else None, args.url) as client:
        return run_tests(client)


if __name__ == "__main__":
    sys.exit(main())
