#!/usr/bin/env python3
"""End-to-end API tests for pipeline event triggers.

Covers the trigger types added on feature/pipeline-event-triggers:
  - ON INSERT INTO <table>   (TABLE_INSERT)
  - ON STREAM <stream>       (STREAM_EVENT)
  - WEBHOOK [SECRET '...']   (POST /webhooks/<pipeline>/<trigger>)

Assumes a server is already running (the shared ServerProc launch is disabled in
_mnemo_client.py). Start one first, e.g.:

    build/bin/Release/mnemosyne_server.exe        # in an empty working directory

Usage:
    python scripts/test_pipeline_event_triggers_api.py --url http://127.0.0.1:1143

Exit code is 0 if all tests pass, 1 otherwise.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.request

from _mnemo_client import Client, TestRunner

DB = "evt_trig_db"
PIPE = "evt_pipe"


def _rows(body: str) -> list[dict]:
    try:
        data = json.loads(body)
        cols = data.get("columns", [])
        return [dict(zip(cols, row)) for row in data.get("data", [])]
    except (json.JSONDecodeError, TypeError, AttributeError):
        return []


def _runs_for_trigger(client: Client, trigger: str) -> int:
    """How many recorded runs of PIPE were started by `trigger`."""
    r = client.query(f"SHOW PIPELINE_RUNS FOR PIPELINE {PIPE}")
    return sum(1 for row in _rows(r.body) if row.get("trigger") == trigger)


def _wait_for_run(client: Client, trigger: str, baseline: int, timeout_s: float = 12.0) -> bool:
    """Poll until a new run attributed to `trigger` appears, or time out."""
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        if _runs_for_trigger(client, trigger) > baseline:
            return True
        time.sleep(0.4)
    return False


def _webhook_post(base_url: str, pipeline: str, trigger: str, token: str | None):
    url = f"{base_url.rstrip('/')}/webhooks/{pipeline}/{trigger}"
    req = urllib.request.Request(url, data=b"{}", method="POST")
    if token is not None:
        req.add_header("X-Webhook-Token", token)
    try:
        with urllib.request.urlopen(req, timeout=10.0) as resp:
            return resp.status, resp.read().decode("utf-8", "replace")
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode("utf-8", "replace")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default="http://127.0.0.1:1143")
    args = ap.parse_args()

    client = Client(args.url)
    t = TestRunner()

    # ── Setup ──
    t.section("Setup")
    client.query(f"CREATE DATABASE {DB}")
    client.query(f"USE {DB}")
    r = client.query("CREATE TABLE orders (id Int64, amount Float64) ENGINE=Memory")
    t.check("CREATE watched table", r.ok(), f"status={r.status} body={r.body!r}")
    r = client.query("CREATE STREAM clicks")
    t.check("CREATE watched stream", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"CREATE PIPELINE {PIPE} OWNER 'evt'")
    t.check("CREATE PIPELINE", r.ok(), f"status={r.status} body={r.body!r}")
    r = client.query(f"CREATE STAGE s1 IN PIPELINE {PIPE} ORDER 1")
    t.check("CREATE STAGE", r.ok(), f"status={r.status}")
    r = client.query(f"CREATE TASK t1 IN STAGE s1 IN PIPELINE {PIPE} TYPE BUILT_IN")
    t.check("CREATE BUILT_IN task", r.ok(), f"status={r.status} body={r.body!r}")

    # ── Trigger creation (positive + negative) ──
    t.section("CREATE TRIGGER")
    r = client.query(f"CREATE TRIGGER on_orders ON PIPELINE {PIPE} ON INSERT INTO orders")
    t.check("CREATE table-insert trigger", r.ok(), f"status={r.status} body={r.body!r}")
    r = client.query(f"CREATE TRIGGER on_clicks ON PIPELINE {PIPE} ON STREAM clicks")
    t.check("CREATE stream trigger", r.ok(), f"status={r.status} body={r.body!r}")
    r = client.query(f"CREATE TRIGGER hook ON PIPELINE {PIPE} WEBHOOK SECRET 'tok-123'")
    t.check("CREATE webhook trigger", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"CREATE TRIGGER bad ON PIPELINE {PIPE} ON INSERT INTO no_such_table")
    t.check("trigger on unknown table is refused", not r.ok(), f"status={r.status}")
    r = client.query(f"CREATE TRIGGER bad2 ON PIPELINE {PIPE} ON STREAM no_such_stream")
    t.check("trigger on unknown stream is refused", not r.ok(), f"status={r.status}")
    r = client.query(f"CREATE TRIGGER bad3 ON PIPELINE {PIPE} EVERY '5m'")
    t.check("mistyped event keyword is a syntax error", not r.ok(), f"status={r.status}")

    # SHOW TRIGGERS reflects the types and never leaks the secret.
    r = client.query(f"SHOW TRIGGERS FROM PIPELINE {PIPE}")
    body = r.body
    t.check("SHOW TRIGGERS lists TABLE_INSERT", "TABLE_INSERT" in body, body[:200])
    t.check("SHOW TRIGGERS lists STREAM_EVENT", "STREAM_EVENT" in body, body[:200])
    t.check("SHOW TRIGGERS lists WEBHOOK", "WEBHOOK" in body, body[:200])
    t.check("SHOW TRIGGERS hides the secret", "tok-123" not in body, body[:200])

    # ── Table-insert event ──
    t.section("TABLE_INSERT event")
    base = _runs_for_trigger(client, "on_orders")
    r = client.query("INSERT INTO orders VALUES (1, 9.5)")
    t.check("INSERT INTO watched table", r.ok(), f"status={r.status} body={r.body!r}")
    t.check("insert fired the pipeline", _wait_for_run(client, "on_orders", base))

    # ── Stream event ──
    t.section("STREAM_EVENT event")
    base = _runs_for_trigger(client, "on_clicks")
    r = client.query("INSERT INTO clicks VALUES ('{\"x\":1}')")
    t.check("append to watched stream", r.ok(), f"status={r.status} body={r.body!r}")
    t.check("stream append fired the pipeline", _wait_for_run(client, "on_clicks", base))

    # ── Webhook event ──
    t.section("WEBHOOK event")
    status, _ = _webhook_post(args.url, PIPE, "hook", token=None)
    t.check("webhook without token -> 401", status == 401, f"status={status}")
    status, _ = _webhook_post(args.url, PIPE, "hook", token="wrong")
    t.check("webhook with wrong token -> 401", status == 401, f"status={status}")
    status, _ = _webhook_post(args.url, PIPE, "no_such_trigger", token="tok-123")
    t.check("unknown webhook -> 404", status == 404, f"status={status}")

    base = _runs_for_trigger(client, "hook")
    status, body = _webhook_post(args.url, PIPE, "hook", token="tok-123")
    t.check("webhook with valid token -> 202", status == 202, f"status={status} body={body!r}")
    t.check("webhook fired the pipeline", _wait_for_run(client, "hook", base))

    # GET on a webhook path is a 405.
    r = client.get(f"/webhooks/{PIPE}/hook")
    t.check("GET on webhook path -> 405", r.status == 405, f"status={r.status}")

    # ── Manual still works ──
    t.section("Manual trigger")
    base = _runs_for_trigger(client, "manual")
    # A run started by RUN PIPELINE is attributed to 'manual'.
    r = client.query(f"RUN PIPELINE {PIPE}")
    t.check("RUN PIPELINE", r.ok(), f"status={r.status} body={r.body!r}")
    time.sleep(0.5)
    t.check("manual run recorded", _runs_for_trigger(client, "manual") > base)

    return t.summary()


if __name__ == "__main__":
    sys.exit(main())
