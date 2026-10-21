#!/usr/bin/env python3
"""End-to-end API tests for Pipeline Layer Phase 2.

Covers:
  * INSERT INTO t [(cols)] SELECT ...   (typed, all-or-nothing loading, incl. FROM CONNECTOR)
  * DATA_QUALITY_RULE: CREATE / SHOW / DESCRIBE / DROP / RUN, kinds UNIQUE, RANGE,
    EXPRESSION, MIN_ROWS and actions WARN, FAIL
  * Pipeline tasks: connector ingest through a SQL task, and TYPE QUALITY tasks that
    fail the run (FAIL) or only warn (WARN)

Usage:
    python3.11 scripts/test_pipeline_phase2_api.py --no-launch
    python3.11 scripts/test_pipeline_phase2_api.py --url http://127.0.0.1:1143 --no-launch

The server must already be running (start it from a scratch directory).
Exit code is 0 if all tests pass, 1 otherwise.
"""

from __future__ import annotations

import argparse
import json
import sys
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

from _mnemo_client import Client, ServerProc, TestRunner, YELLOW, RESET

DB = "phase2_db"
CONN = "phase2_api"
PIPELINE = "phase2_pipeline"

ORDERS = [
    {"order_id": "100", "customer_id": "1", "total": "42.50"},
    {"order_id": "101", "customer_id": "2", "total": "18.00"},
    {"order_id": "102", "customer_id": "2", "total": "-5.00"},
    {"order_id": "103", "customer_id": "3", "total": "7.25"},
]
BAD_ORDERS = [
    {"order_id": "200", "customer_id": "1", "total": "1.5"},
    {"order_id": "abc", "customer_id": "2", "total": "2.5"},
]
ROUTES = {"/orders": ORDERS, "/bad_orders": BAD_ORDERS}


class MockApiHandler(BaseHTTPRequestHandler):
    def log_message(self, format: str, *args) -> None:  # noqa: A003
        return

    def do_GET(self) -> None:  # noqa: N802
        if self.path not in ROUTES:
            self.send_response(404)
            self.end_headers()
            return
        body = json.dumps(ROUTES[self.path]).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


class MockApiServer:
    def __init__(self) -> None:
        self._httpd: HTTPServer | None = None

    def start(self) -> str:
        self._httpd = HTTPServer(("127.0.0.1", 0), MockApiHandler)
        threading.Thread(target=self._httpd.serve_forever, daemon=True).start()
        return f"http://127.0.0.1:{self._httpd.server_address[1]}"

    def stop(self) -> None:
        if self._httpd is not None:
            self._httpd.shutdown()
            self._httpd.server_close()
            self._httpd = None


def _data(body: str) -> list[list]:
    try:
        return json.loads(body).get("data", [])
    except (json.JSONDecodeError, TypeError):
        return []


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

    def rows(self, sql: str) -> list[list]:
        return _data(self.client.query(sql).body)

    def scalar(self, sql: str):
        data = self.rows(sql)
        return data[0][0] if data and data[0] else None

    def rule(self, name: str) -> dict:
        for row in _rows(self.client.query("SHOW DATA_QUALITY_RULES").body):
            if row.get("name") == name:
                return row
        return {}

    def run_rule(self, name: str) -> dict:
        rows = _rows(self.client.query(f"RUN DATA_QUALITY_RULE {name}").body)
        return rows[0] if rows else {}


def setup(x: Ctx, base_url: str) -> None:
    x.t.section("Setup")
    for label, sql in [
        ("DROP DATABASE", f"DROP DATABASE IF EXISTS {DB}"),
        ("DROP PIPELINE", f"DROP PIPELINE IF EXISTS {PIPELINE}"),
        ("DROP CONNECTOR", f"DROP CONNECTOR IF EXISTS {CONN}"),
        ("CREATE DATABASE", f"CREATE DATABASE {DB}"),
        ("USE", f"USE {DB}"),
        ("CREATE CONNECTOR", f"CREATE CONNECTOR {CONN} TYPE REST BASE_URL '{base_url}' "
                             f"SCHEMA 'orders,bad_orders'"),
        ("CREATE src", "CREATE TABLE src (id Int64, name String, score Float64)"),
        ("CREATE dst", "CREATE TABLE dst (id Int64, name String, score Float64)"),
        ("INSERT src", "INSERT INTO src VALUES (1, 'ann', 1.5), (2, 'bob', 2.5), (3, 'cy', 3.5)"),
    ]:
        x.ok(f"setup: {label}", sql)


def test_insert_select(x: Ctx) -> None:
    x.t.section("INSERT ... SELECT")
    x.ok("copy all columns", "INSERT INTO dst SELECT id, name, score FROM src")
    x.t.check("copied rows match", x.rows("SELECT id, name, score FROM dst ORDER BY id") ==
              [[1, "ann", 1.5], [2, "bob", 2.5], [3, "cy", 3.5]])

    x.ok("SELECT * form appends", "INSERT INTO dst SELECT * FROM src WHERE id > 1")
    x.t.check("appended, not replaced", x.scalar("SELECT COUNT(*) FROM dst") == 5)

    x.ok("create totals", "CREATE TABLE totals (label String, n Int64, total Float64)")
    x.ok("aggregate source with expressions",
         "INSERT INTO totals SELECT name, COUNT(*), SUM(score) * 2 FROM src GROUP BY name")
    x.t.check("aggregate values loaded", x.rows("SELECT label, n, total FROM totals ORDER BY label") ==
              [["ann", 1, 3.0], ["bob", 1, 5.0], ["cy", 1, 7.0]])

    x.ok("explicit column list fills the rest with defaults",
         "INSERT INTO dst (id, name) SELECT id + 10, name FROM src WHERE id = 1")
    x.t.check("omitted column defaulted and rows aligned",
              x.rows("SELECT id, name, score FROM dst WHERE id = 11") == [[11, "ann", 0.0]])

    before = x.scalar("SELECT COUNT(*) FROM src")
    x.ok("table can feed itself", "INSERT INTO src SELECT id + 100, name, score FROM src")
    x.t.check("self insert doubled the rows", x.scalar("SELECT COUNT(*) FROM src") == before * 2)

    x.fails("column count mismatch is rejected", "INSERT INTO dst SELECT id, name FROM src", "column")
    x.fails("unknown source column is rejected", "INSERT INTO dst SELECT id, nope, score FROM src", "nope")
    x.fails("unknown target table is rejected", "INSERT INTO missing_table SELECT id FROM src", "missing_table")
    x.fails("unknown target column is rejected", "INSERT INTO dst (id, nope) SELECT id, name FROM src", "nope")

    x.ok("create typed", "CREATE TABLE typed (n Int64, f Float64, s String)")
    x.fails("text that is not a number is rejected, nothing written",
            "INSERT INTO typed SELECT name, score, name FROM src", "convert")
    x.fails("a fractional value does not fit an integer column",
            "INSERT INTO typed SELECT score, score, name FROM src", "convert")
    x.t.check("failed inserts left the table empty", x.scalar("SELECT COUNT(*) FROM typed") == 0)


def test_connector_ingest(x: Ctx) -> None:
    x.t.section("Loading from a connector")
    x.ok("create raw", "CREATE TABLE raw_orders (order_id Int64, customer_id Int64, total Float64)")
    x.ok("ingest converts text to the column types",
         f"INSERT INTO raw_orders SELECT order_id, customer_id, total FROM CONNECTOR {CONN}.orders")
    x.t.check("typed rows loaded", x.rows("SELECT order_id, customer_id, total FROM raw_orders ORDER BY order_id") ==
              [[100, 1, 42.5], [101, 2, 18.0], [102, 2, -5.0], [103, 3, 7.25]])
    x.t.check("numeric aggregates work on the loaded data",
              x.scalar("SELECT SUM(total) FROM raw_orders") == 42.5 + 18.0 - 5.0 + 7.25)

    x.ok("create strict", "CREATE TABLE strict_orders (order_id Int64, customer_id Int64, total Float64)")
    x.fails("one bad value aborts the whole load",
            f"INSERT INTO strict_orders SELECT order_id, customer_id, total FROM CONNECTOR {CONN}.bad_orders",
            "convert")
    x.t.check("no partial rows were written", x.scalar("SELECT COUNT(*) FROM strict_orders") == 0)


def test_rule_ddl(x: Ctx) -> None:
    x.t.section("DATA_QUALITY_RULE DDL")
    x.ok("UNIQUE rule", "CREATE DATA_QUALITY_RULE uniq_customer ON raw_orders UNIQUE customer_id ACTION WARN")
    x.ok("RANGE rule", "CREATE DATA_QUALITY_RULE total_nonneg ON raw_orders RANGE total MIN 0 ACTION FAIL")
    x.ok("RANGE with both bounds", "CREATE DATA_QUALITY_RULE total_cap ON raw_orders RANGE total MIN -10 MAX 100")
    x.ok("EXPRESSION rule", "CREATE DATA_QUALITY_RULE ids_positive ON raw_orders EXPRESSION 'order_id > 0'")
    x.ok("MIN_ROWS rule", "CREATE DATA_QUALITY_RULE has_rows ON raw_orders MIN_ROWS 4 ACTION FAIL")

    rows = {r.get("name"): r for r in _rows(x.client.query("SHOW DATA_QUALITY_RULES").body)}
    x.t.check("SHOW lists the rules", {"uniq_customer", "total_nonneg", "total_cap", "ids_positive",
                                      "has_rows"} <= set(rows), f"names={sorted(rows)}")
    x.t.check("SHOW reports action and table",
              rows.get("uniq_customer", {}).get("action", "").upper() == "WARN"
              and rows.get("uniq_customer", {}).get("table") == "raw_orders", f"row={rows.get('uniq_customer')}")
    x.t.check("action defaults to FAIL", rows.get("ids_positive", {}).get("action", "").upper() == "FAIL")
    x.t.check("rules start as not run", rows.get("has_rows", {}).get("last_status", "").upper() == "NOT_RUN")

    r = x.client.query("DESCRIBE DATA_QUALITY_RULE total_cap")
    body = r.body.lower()
    x.t.check("DESCRIBE shows the bounds", r.ok() and "total" in body and "100" in body and "-10" in body, body[:300])

    x.fails("duplicate name is rejected", "CREATE DATA_QUALITY_RULE has_rows ON raw_orders MIN_ROWS 1", "exists")
    x.ok("IF NOT EXISTS is idempotent", "CREATE DATA_QUALITY_RULE IF NOT EXISTS has_rows ON raw_orders MIN_ROWS 1")
    x.fails("unknown check type", "CREATE DATA_QUALITY_RULE r1 ON raw_orders BOGUS total", "")
    x.fails("RANGE needs a bound", "CREATE DATA_QUALITY_RULE r2 ON raw_orders RANGE total", "")
    x.fails("MIN above MAX", "CREATE DATA_QUALITY_RULE r3 ON raw_orders RANGE total MIN 5 MAX 1", "")
    x.fails("bad action", "CREATE DATA_QUALITY_RULE r4 ON raw_orders UNIQUE order_id ACTION EXPLODE", "action")
    x.fails("QUARANTINE is not available yet",
            "CREATE DATA_QUALITY_RULE r5 ON raw_orders UNIQUE order_id ACTION QUARANTINE", "quarantine")
    x.fails("expression that does not parse", "CREATE DATA_QUALITY_RULE r6 ON raw_orders EXPRESSION 'total >'", "")
    x.fails("expression cannot smuggle a second clause",
            "CREATE DATA_QUALITY_RULE r7 ON raw_orders EXPRESSION 'order_id > 0) OR (1 = 1'", "")
    x.fails("column names must be plain identifiers",
            "CREATE DATA_QUALITY_RULE r8 ON raw_orders UNIQUE 'order id'", "")
    x.fails("DESCRIBE of an unknown rule", "DESCRIBE DATA_QUALITY_RULE nope", "nope")
    x.fails("RUN of an unknown rule", "RUN DATA_QUALITY_RULE nope", "nope")
    x.fails("DROP of an unknown rule", "DROP DATA_QUALITY_RULE nope", "nope")
    x.ok("DROP IF EXISTS of an unknown rule", "DROP DATA_QUALITY_RULE IF EXISTS nope")


def test_rule_evaluation(x: Ctx) -> None:
    x.t.section("Rule evaluation")
    r = x.run_rule("uniq_customer")
    x.t.check("UNIQUE finds the two rows that share customer 2",
              r.get("status", "").upper() == "WARNED" and int(r.get("violations", -1)) == 2, f"result={r}")
    r = x.run_rule("total_nonneg")
    x.t.check("RANGE counts the negative total", r.get("status", "").upper() == "FAILED"
              and int(r.get("violations", -1)) == 1, f"result={r}")
    r = x.run_rule("total_cap")
    x.t.check("RANGE with both bounds passes", r.get("status", "").upper() == "PASSED"
              and int(r.get("violations", -1)) == 0, f"result={r}")
    r = x.run_rule("ids_positive")
    x.t.check("EXPRESSION passes when every row satisfies it", r.get("status", "").upper() == "PASSED", f"result={r}")
    r = x.run_rule("has_rows")
    x.t.check("MIN_ROWS passes with enough rows", r.get("status", "").upper() == "PASSED", f"result={r}")

    x.ok("add an expression that three of four rows break",
         "CREATE DATA_QUALITY_RULE big_orders ON raw_orders EXPRESSION 'total > 20' ACTION WARN")
    r = x.run_rule("big_orders")
    x.t.check("EXPRESSION counts the rows that do not satisfy it",
              r.get("status", "").upper() == "WARNED" and int(r.get("violations", -1)) == 3, f"result={r}")

    x.ok("empty table", "CREATE TABLE empty_t (id Int64)")
    x.ok("MIN_ROWS on an empty table", "CREATE DATA_QUALITY_RULE empty_needs_rows ON empty_t MIN_ROWS 1")
    r = x.run_rule("empty_needs_rows")
    x.t.check("MIN_ROWS fails on an empty table", r.get("status", "").upper() == "FAILED", f"result={r}")
    x.ok("UNIQUE on an empty table", "CREATE DATA_QUALITY_RULE empty_unique ON empty_t UNIQUE id")
    x.t.check("UNIQUE passes on an empty table", x.run_rule("empty_unique").get("status", "").upper() == "PASSED")

    shown = x.rule("total_nonneg")
    x.t.check("SHOW remembers the last result",
              shown.get("last_status", "").upper() == "FAILED" and int(shown.get("last_violations", -1)) == 1,
              f"row={shown}")

    x.ok("rule on a table that does not exist", "CREATE DATA_QUALITY_RULE ghost ON no_such_table MIN_ROWS 1")
    x.fails("evaluating it is an error, not a pass", "RUN DATA_QUALITY_RULE ghost", "no_such_table")
    x.ok("rule on a column that does not exist", "CREATE DATA_QUALITY_RULE ghost_col ON raw_orders UNIQUE nope")
    x.fails("evaluating it is an error, not a pass", "RUN DATA_QUALITY_RULE ghost_col", "nope")


def build_pipeline(x: Ctx, name: str, quality_rules: str, load_table: str) -> None:
    x.ok(f"{name}: pipeline", f"CREATE PIPELINE {name}")
    x.ok(f"{name}: stage", f"CREATE STAGE s IN PIPELINE {name} ORDER 1")
    x.ok(f"{name}: extract", f"CREATE TASK extract IN STAGE s IN PIPELINE {name} TYPE SQL BODY "
                             f"'INSERT INTO staging SELECT order_id, customer_id, total FROM CONNECTOR {CONN}.orders'")
    x.ok(f"{name}: validate", f"CREATE TASK validate IN STAGE s IN PIPELINE {name} TYPE QUALITY "
                              f"BODY '{quality_rules}' DEPENDS ON extract")
    x.ok(f"{name}: load", f"CREATE TASK load IN STAGE s IN PIPELINE {name} TYPE SQL BODY "
                          f"'INSERT INTO {load_table} SELECT customer_id, SUM(total) AS spend, COUNT(*) AS n "
                          f"FROM staging GROUP BY customer_id' DEPENDS ON validate")


def run_status(x: Ctx, name: str) -> str:
    rows = _rows(x.client.query(f"RUN PIPELINE {name}").body)
    return rows[0].get("status", "").upper() if rows else ""


def test_pipelines(x: Ctx) -> None:
    x.t.section("Pipelines: ingest, validate, load")
    x.ok("staging", "CREATE TABLE staging (order_id Int64, customer_id Int64, total Float64)")
    x.ok("warehouse for the passing run", "CREATE TABLE spend_ok (customer_id Int64, spend Float64, n Int64)")
    x.ok("warehouse for the failing run", "CREATE TABLE spend_blocked (customer_id Int64, spend Float64, n Int64)")
    x.ok("warehouse for the warn run", "CREATE TABLE spend_warned (customer_id Int64, spend Float64, n Int64)")

    x.ok("rule: orders are not empty", "CREATE DATA_QUALITY_RULE staged_rows ON staging MIN_ROWS 1 ACTION FAIL")
    x.ok("rule: totals are not negative (blocks)",
         "CREATE DATA_QUALITY_RULE staged_nonneg ON staging RANGE total MIN 0 ACTION FAIL")
    x.ok("rule: customer ids repeat (only warns)",
         "CREATE DATA_QUALITY_RULE staged_unique ON staging UNIQUE customer_id ACTION WARN")
    x.ok("rule: totals within the cap", "CREATE DATA_QUALITY_RULE staged_cap ON staging RANGE total MAX 100")

    x.fails("QUALITY task naming an unknown rule is rejected",
            "CREATE TASK bad IN STAGE s IN PIPELINE nope TYPE QUALITY BODY 'no_such_rule'", "")
    x.ok("pipeline for the unknown-rule check", f"CREATE PIPELINE p_rules")
    x.ok("stage", "CREATE STAGE s IN PIPELINE p_rules ORDER 1")
    x.fails("QUALITY task naming an unknown rule is rejected",
            "CREATE TASK bad IN STAGE s IN PIPELINE p_rules TYPE QUALITY BODY 'no_such_rule'", "no_such_rule")
    x.fails("QUALITY task needs at least one rule",
            "CREATE TASK bad IN STAGE s IN PIPELINE p_rules TYPE QUALITY BODY ''", "")
    x.ok("drop helper pipeline", "DROP PIPELINE p_rules")

    build_pipeline(x, PIPELINE, "staged_rows, staged_unique, staged_cap", "spend_ok")
    status = run_status(x, PIPELINE)
    x.t.check("a WARN-only violation does not stop the pipeline", status == "SUCCEEDED", f"status={status}")
    x.t.check("the load after validation ran",
              x.rows("SELECT customer_id, spend, n FROM spend_ok ORDER BY customer_id") ==
              [[1, 42.5, 1], [2, 13.0, 2], [3, 7.25, 1]])
    shown = x.rule("staged_unique")
    x.t.check("the warning is visible on the rule", shown.get("last_status", "").upper() == "WARNED"
              and int(shown.get("last_violations", -1)) == 2, f"row={shown}")

    x.ok("clear staging", "DROP TABLE staging")
    x.ok("recreate staging", "CREATE TABLE staging (order_id Int64, customer_id Int64, total Float64)")
    build_pipeline(x, "phase2_blocked", "staged_rows, staged_nonneg", "spend_blocked")
    status = run_status(x, "phase2_blocked")
    x.t.check("a FAIL violation fails the run", status == "FAILED", f"status={status}")
    x.t.check("the load after the failed check never ran", x.scalar("SELECT COUNT(*) FROM spend_blocked") == 0)
    x.t.check("the ingest before the check did run", x.scalar("SELECT COUNT(*) FROM staging") == 4)
    shown = x.rule("staged_nonneg")
    x.t.check("the failure is visible on the rule", shown.get("last_status", "").upper() == "FAILED"
              and int(shown.get("last_violations", -1)) == 1, f"row={shown}")

    x.ok("clear staging again", "DROP TABLE staging")
    x.ok("recreate staging again", "CREATE TABLE staging (order_id Int64, customer_id Int64, total Float64)")
    build_pipeline(x, "phase2_warned", "staged_unique", "spend_warned")
    status = run_status(x, "phase2_warned")
    x.t.check("a pipeline that only warns succeeds", status == "SUCCEEDED", f"status={status}")


def cleanup(x: Ctx) -> None:
    x.t.section("Cleanup")
    for name in (PIPELINE, "phase2_blocked", "phase2_warned"):
        x.ok(f"drop pipeline {name}", f"DROP PIPELINE IF EXISTS {name}")
    for rule in ("uniq_customer", "total_nonneg", "total_cap", "ids_positive", "has_rows", "big_orders",
                 "empty_needs_rows", "empty_unique", "ghost", "ghost_col", "staged_rows", "staged_nonneg",
                 "staged_unique", "staged_cap"):
        x.ok(f"drop rule {rule}", f"DROP DATA_QUALITY_RULE IF EXISTS {rule}")
    x.ok("drop connector", f"DROP CONNECTOR IF EXISTS {CONN}")
    x.ok("drop database", f"DROP DATABASE IF EXISTS {DB}")


def run_tests(client: Client, base_url: str) -> int:
    t = TestRunner()
    x = Ctx(client, t)
    setup(x, base_url)
    test_insert_select(x)
    test_connector_ingest(x)
    test_rule_ddl(x)
    test_rule_evaluation(x)
    test_pipelines(x)
    cleanup(x)
    return t.summary()


def main() -> int:
    import argparse as _argparse
    parser = _argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:1143")
    parser.add_argument("--no-launch", action="store_true")
    parser.add_argument("--server-bin", default=None)
    args = parser.parse_args()

    mock = MockApiServer()
    base_url = mock.start()
    try:
        if args.no_launch:
            print(f"{YELLOW}Using existing server at {args.url}{RESET}")
            return run_tests(Client(args.url), base_url)
        with ServerProc(Path(args.server_bin) if args.server_bin else None, args.url) as client:
            return run_tests(client, base_url)
    finally:
        mock.stop()


if __name__ == "__main__":
    sys.exit(main())
