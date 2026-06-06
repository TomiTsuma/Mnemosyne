#!/usr/bin/env python3
"""End-to-end API tests for Streaming Layer Phase 1.

Usage:
    python scripts/test_streams_api.py
    python scripts/test_streams_api.py --url http://127.0.0.1:1143 --no-launch
    python scripts/test_streams_api.py --server-bin build/bin/Release/mnemosyne_server.exe

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
    YELLOW,
    RESET,
)

REPO_ROOT = Path(__file__).resolve().parent.parent
DB = "streams_test_db"
TOPIC = "api_user_activity"
STREAM = "api_user_events"
STREAM_ALT = "api_orders"
GROUP_A = "api_analytics"
GROUP_B = "api_dashboard"
MISSING_STREAM = "definitely_missing_stream_xyz"


def _rows(body: str) -> list[dict]:
    try:
        data = json.loads(body)
        cols = data.get("columns", [])
        rows = data.get("data", [])
        return [dict(zip(cols, row)) for row in rows]
    except (json.JSONDecodeError, TypeError):
        return []


def _body_lower(body: str) -> str:
    return body.lower()


def _describe_value(body: str, field: str) -> str | None:
    for row in _rows(body):
        if str(row.get("field", "")).lower() == field.lower():
            return str(row.get("value", ""))
    return None


def setup(client: Client, t: TestRunner) -> None:
    t.section("Setup")
    for label, sql in [
        ("CREATE DATABASE", f"CREATE DATABASE IF NOT EXISTS {DB}"),
        ("USE", f"USE {DB}"),
        ("DROP STREAM orders", f"DROP STREAM IF EXISTS {STREAM_ALT}"),
        ("DROP STREAM events", f"DROP STREAM IF EXISTS {STREAM}"),
        ("DROP TOPIC", f"DROP TOPIC IF EXISTS {TOPIC}"),
        ("DROP GROUP A", f"DROP CONSUMER_GROUP IF EXISTS {GROUP_A}"),
        ("DROP GROUP B", f"DROP CONSUMER_GROUP IF EXISTS {GROUP_B}"),
    ]:
        r = client.query(sql)
        t.check(f"setup: {label}", r.ok(), f"status={r.status} body={r.body[:300]!r}")


def test_catalog_crud(client: Client, t: TestRunner) -> None:
    t.section("Catalog CRUD")

    r = client.query(f"CREATE TOPIC {TOPIC} PARTITIONS 4 RETAIN 30 DAYS")
    t.check("CREATE TOPIC", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"CREATE STREAM {STREAM} TOPIC {TOPIC} RETAIN 7 DAYS")
    t.check("CREATE STREAM with TOPIC", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"CREATE CONSUMER_GROUP {GROUP_A}")
    t.check("CREATE CONSUMER_GROUP", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("SHOW TOPICS")
    t.check("SHOW TOPICS", r.ok(), f"status={r.status}")
    t.check("topic listed", TOPIC in _body_lower(r.body), f"body={r.body!r}")

    r = client.query("SHOW STREAMS")
    t.check("SHOW STREAMS", r.ok(), f"status={r.status}")
    t.check("stream listed", STREAM in _body_lower(r.body), f"body={r.body!r}")

    r = client.query("SHOW CONSUMER_GROUPS")
    t.check("SHOW CONSUMER_GROUPS", r.ok(), f"status={r.status}")
    t.check("group listed", GROUP_A in _body_lower(r.body), f"body={r.body!r}")

    r = client.query(f"DESCRIBE TOPIC {TOPIC}")
    t.check("DESCRIBE TOPIC", r.ok(), f"status={r.status}")
    t.check(
        "topic partition_count",
        _describe_value(r.body, "partition_count") == "4",
        f"body={r.body!r}",
    )

    r = client.query(f"DESCRIBE STREAM {STREAM}")
    t.check("DESCRIBE STREAM", r.ok(), f"status={r.status}")
    t.check(
        "stream bound topic",
        _describe_value(r.body, "topic") == TOPIC,
        f"body={r.body!r}",
    )
    t.check(
        "stream retention",
        "7" in (_describe_value(r.body, "retention") or ""),
        f"body={r.body!r}",
    )

    r = client.query(f"DESCRIBE CONSUMER_GROUP {GROUP_A}")
    t.check("DESCRIBE CONSUMER_GROUP", r.ok(), f"status={r.status}")
    t.check(
        "group name in describe",
        _describe_value(r.body, "name") == GROUP_A,
        f"body={r.body!r}",
    )


def test_insert_and_subscribe(client: Client, t: TestRunner) -> None:
    t.section("INSERT and SUBSCRIBE")

    r = client.query(
        f"INSERT INTO {STREAM} VALUES "
        "('{\"user_id\":1,\"action\":\"click\"}'), "
        "('{\"user_id\":2,\"action\":\"view\"}')"
    )
    t.check("INSERT INTO stream", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"SUBSCRIBE {STREAM} LIMIT 10")
    t.check("SUBSCRIBE without group", r.ok(), f"status={r.status}")
    rows = _rows(r.body)
    t.check("returns 2 events", len(rows) == 2, f"rows={rows}")
    payloads = [row.get("payload", "") for row in rows]
    t.check("first payload", "click" in payloads[0], f"payloads={payloads}")
    t.check("second payload", "view" in payloads[1], f"payloads={payloads}")

    r = client.query(f"SUBSCRIBE {STREAM} LIMIT 10")
    t.check("SUBSCRIBE replay without group", r.ok(), f"status={r.status}")
    t.check(
        "replay returns same count",
        len(_rows(r.body)) == 2,
        f"body={r.body!r}",
    )


def test_publish_flow(client: Client, t: TestRunner) -> None:
    t.section("PUBLISH to topic")

    r = client.query(
        f"PUBLISH {TOPIC} VALUES ('{{\"user_id\":3,\"action\":\"purchase\"}}')"
    )
    t.check("PUBLISH to topic", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"SUBSCRIBE {STREAM} LIMIT 10")
    t.check("SUBSCRIBE after publish", r.ok(), f"status={r.status}")
    t.check(
        "stream has 3 events after publish",
        len(_rows(r.body)) == 3,
        f"rows={_rows(r.body)}",
    )
    payloads = [row.get("payload", "") for row in _rows(r.body)]
    t.check("publish payload present", any("purchase" in p for p in payloads), f"payloads={payloads}")


def test_consumer_groups(client: Client, t: TestRunner) -> None:
    t.section("Consumer group offsets")

    client.query(f"CREATE CONSUMER_GROUP {GROUP_B}")

    r = client.query(f"SUBSCRIBE {STREAM} CONSUMER_GROUP {GROUP_A} LIMIT 2")
    t.check("group A first read", r.ok(), f"status={r.status}")
    t.check("group A got 2 events", len(_rows(r.body)) == 2, f"rows={_rows(r.body)}")

    r = client.query(f"SUBSCRIBE {STREAM} CONSUMER_GROUP {GROUP_B} LIMIT 2")
    t.check("group B first read", r.ok(), f"status={r.status}")
    t.check("group B got 2 events", len(_rows(r.body)) == 2, f"rows={_rows(r.body)}")

    client.query(
        f"INSERT INTO {STREAM} VALUES ('{{\"user_id\":4,\"action\":\"logout\"}}')"
    )

    r = client.query(f"SUBSCRIBE {STREAM} CONSUMER_GROUP {GROUP_A} LIMIT 10")
    t.check("group A incremental", r.ok(), f"status={r.status}")
    rows_a = _rows(r.body)
    t.check("group A gets unpublished events", len(rows_a) == 2, f"rows={rows_a}")
    payloads_a = [row.get("payload", "") for row in rows_a]
    t.check("group A purchase event", any("purchase" in p for p in payloads_a), f"payloads={payloads_a}")
    t.check("group A logout event", any("logout" in p for p in payloads_a), f"payloads={payloads_a}")

    r = client.query(f"SUBSCRIBE {STREAM} CONSUMER_GROUP {GROUP_B} LIMIT 10")
    t.check("group B incremental", r.ok(), f"status={r.status}")
    rows_b = _rows(r.body)
    t.check("group B gets unpublished events", len(rows_b) == 2, f"rows={rows_b}")


def test_metrics_and_alter(client: Client, t: TestRunner) -> None:
    t.section("Metrics and ALTER")

    r = client.query(f"SHOW STREAM_METRICS FOR STREAM {STREAM}")
    t.check("SHOW STREAM_METRICS", r.ok(), f"status={r.status}")
    body_lower = _body_lower(r.body)
    t.check("includes event_count", "event_count" in body_lower, f"body={r.body!r}")
    t.check("includes consumer_lag", "consumer_lag" in body_lower, f"body={r.body!r}")

    metrics_rows = _rows(r.body)
    if metrics_rows:
        t.check(
            "event_count >= 4",
            int(metrics_rows[0].get("event_count", 0)) >= 4,
            f"row={metrics_rows[0]}",
        )

    r = client.query(f"ALTER STREAM {STREAM} SET RETENTION 14 DAYS")
    t.check("ALTER STREAM retention", r.ok(), f"status={r.status}")

    r = client.query(f"DESCRIBE STREAM {STREAM}")
    t.check(
        "retention updated",
        "14" in (_describe_value(r.body, "retention") or ""),
        f"body={r.body!r}",
    )


def test_errors(client: Client, t: TestRunner) -> None:
    t.section("Error handling")

    r = client.query(f"CREATE STREAM {STREAM} TOPIC {TOPIC}")
    t.check("duplicate CREATE STREAM fails", not r.ok(), f"status={r.status}")

    r = client.query(f"SUBSCRIBE {MISSING_STREAM}")
    t.check("subscribe missing stream fails", not r.ok(), f"status={r.status}")

    r = client.query(f"PUBLISH {TOPIC} VALUES ('{{\"x\":1}}')")
    t.check("publish still works", r.ok(), f"status={r.status}")

    orphan_topic = "api_orphan_topic"
    client.query(f"DROP TOPIC IF EXISTS {orphan_topic}")
    client.query(f"CREATE TOPIC {orphan_topic}")
    r = client.query(f"PUBLISH {orphan_topic} VALUES ('{{\"x\":1}}')")
    t.check("publish without bound stream fails", not r.ok(), f"status={r.status} body={r.body!r}")
    client.query(f"DROP TOPIC IF EXISTS {orphan_topic}")


def cleanup(client: Client, t: TestRunner) -> None:
    t.section("Cleanup")
    for label, sql in [
        ("DROP GROUP B", f"DROP CONSUMER_GROUP IF EXISTS {GROUP_B}"),
        ("DROP GROUP A", f"DROP CONSUMER_GROUP IF EXISTS {GROUP_A}"),
        ("DROP STREAM", f"DROP STREAM IF EXISTS {STREAM}"),
        ("DROP TOPIC", f"DROP TOPIC IF EXISTS {TOPIC}"),
    ]:
        r = client.query(sql)
        t.check(f"cleanup: {label}", r.ok(), f"status={r.status}")


def run_tests(client: Client) -> int:
    t = TestRunner()
    setup(client, t)
    test_catalog_crud(client, t)
    test_insert_and_subscribe(client, t)
    test_publish_flow(client, t)
    test_consumer_groups(client, t)
    test_metrics_and_alter(client, t)
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
