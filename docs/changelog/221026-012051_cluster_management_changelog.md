---
description: "CLUSTER membership with `ALTER CLUSTER`, `DROP CLUSTER`, derived cluster status, and `SHOW CLUSTER STATUS` and `SHOW CLUSTER METRICS`."
sidebar_label: "Cluster Management"
sidebar_position: 5
slug: /changelog/221026-cluster-management
title: "CLUSTER — membership, drop, status and metrics"
doc_type: changelog
---
# CLUSTER — membership, drop, status and metrics {#cluster-membership-drop-status-and-metrics}

**Branch:** `feature/cluster-management`
**Date:** 2026-10-22

## Summary {#summary}

`docs/PRDS/CLUSTER_.md` Phase 1 asks for single-node cluster support, node membership and
cluster metadata. Before this change a cluster could be created, listed and described but not
changed or removed: `ALTER CLUSTER ... ADD/REMOVE NODE` and `DROP CLUSTER` were not parsed, the
PRD's `SHOW CLUSTER STATUS` / `SHOW CLUSTER METRICS` did not exist, `SHOW CLUSTERS` returned
names only, and there was no test suite for the entity at all.

This change adds those statements and `scripts/test_clusters_api.py` (97 checks). All 17 other
API suites still pass.

## What was added {#what-was-added}

- **`DROP CLUSTER [IF EXISTS] <name>`.** Member nodes are detached (their `cluster_id` is
  cleared) so none is left pointing at a cluster that no longer exists. The nodes themselves are
  not removed.
- **`ALTER CLUSTER <name> ADD NODE <n> | REMOVE NODE <n> [, ...]`.** Several steps may appear in
  one statement and are applied **all-or-nothing** by `NodeManager::alter_cluster_membership`:
  the previous `cluster_id` of every touched node is recorded under the manager lock and
  restored if any step throws, so `ALTER CLUSTER c ADD NODE a, ADD NODE ghost` leaves `a`
  untouched.
- **One cluster per node.** `ADD NODE` of a node that already belongs to a *registered* cluster
  is refused ("remove it from that cluster first") rather than silently moving it. Adding a
  current member again is a no-op; `REMOVE NODE` of a non-member is an error.
- **Cluster state**, derived from member health in one locked pass (`ClusterSummary`):
  no members `CREATING`; every member `OFFLINE` `OFFLINE`; every member `ONLINE`/`BUSY`
  `ONLINE`; anything else (offline, draining, registering, ...) `DEGRADED`. These are the PRD
  §8 names. `UPGRADING` and `MAINTENANCE` are operator-set states and are not modelled yet.
- **`SHOW CLUSTERS`** now returns `name, status, node_count`; **`DESCRIBE CLUSTER`** returns
  `name, status, node_count, nodes` (members sorted by name).
- **`SHOW CLUSTER STATUS`**: `name, status, node_count, online, offline`.
- **`SHOW CLUSTER METRICS`**: capacity (CPU cores, memory, storage, GPUs) summed over members,
  average CPU utilisation, memory/storage used, and query/pipeline/stream throughput.

## Decisions worth knowing about {#decisions-worth-knowing-about}

- **The self node's seeded `cluster_id` is not membership.** The server starts the self node
  with `cluster_id = "default"` and no cluster of that name exists, and `ALTER NODE ... SET
  CLUSTER` does not validate. The one-cluster rule therefore only refuses when the node's
  current cluster is a registered one; otherwise `ALTER CLUSTER prod ADD NODE local` — the
  single-node case the PRD names — would be rejected.
- **Membership stays a node attribute** (`NodeEntry::cluster_id`), as it already was. A cluster
  is still just a name and creation time; richer metadata (type, owner, version) is not added.

## Two latent bugs fixed on the way {#two-latent-bugs-fixed-on-the-way}

- **`ALTER NODE ... SET STATUS <state>` was unreachable from SQL.** The interpreter handles the
  `STATUS` property, but `Parser::parse_name_or_keyword` did not accept `KeywordStatus`, so the
  statement failed with `Parser: expected name`. No existing test covered it. `KeywordStatus`
  is now accepted. This changes behaviour outside CLUSTER, and is what makes node states
  settable for the state-derivation tests.
- **`DESCRIBE CLUSTER` returned an empty field name.** It built rows with
  `core::Field("name")`; a `const char*` converts to `bool` ahead of `std::string`, so the
  field column stored an empty string. Rows now go through an `add_row(std::string,
  std::string)` helper, like the other `DESCRIBE` handlers.

## Not done (still open in CLUSTER_.md) {#not-done-still-open-in-cluster-md}

`CREATE RESOURCE_POOL`; operator-set `UPGRADING`/`MAINTENANCE` states; cluster type, owner and
version metadata; model throughput in the metrics; workload scheduling, HA/failover, consensus
and multi-region (PRD phases 2-5).

## Files changed {#files-changed}

- `src/Nodes/cluster_catalog.h`, `node_manager.{h,cpp}` — `ClusterState`, `ClusterSummary`,
  `ClusterMembershipOp`, `drop_cluster`, `alter_cluster_membership`, `nodes_in_cluster`,
  `cluster_summary`, `cluster_summaries`.
- `src/Parsers/ast.h`, `parser.cpp` — `ALTER CLUSTER`, `DROP CLUSTER`, `SHOW CLUSTER
  STATUS|METRICS`; `KeywordStatus` accepted as a property name.
- `src/Analyzer/query_tree.h`, `analyzer.cpp`, `src/Planner/planner.cpp` — plumbing for the two
  new `SHOW` forms.
- `src/Interpreters/interpreter_alter_cluster.{h,cpp}` (new), `interpreter_drop_query.cpp`,
  `block_interpreter.cpp` — execution and output.
- `src/Server/http_handler.cpp` — `ALTER CLUSTER` dispatch.
- `scripts/test_clusters_api.py` (new).

## Verification {#verification}

- Build: `cmake --build build --config Release --target mnemosyne_server` — 0 errors.
- `python3.11 scripts/test_clusters_api.py --no-launch` — **97/97**.
- Regression sweep, all green: `pipeline_phase2` 136, `select` 74, `pipelines` 74, `streams` 58,
  `models` 54, `shard_groups` 47, `nodes` 40, `connectors` 38, `replica_groups` 38, `views` 36,
  `storage_units` 32, `file_engine` 29, `feature_sets` 28, `tuning` 27, `use` 15,
  `replica_groups_failover` 14, `ml_runtime` 12.
