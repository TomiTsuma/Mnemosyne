#!/usr/bin/env python3.11
"""End-to-end API tests for ALTER STORAGE_UNIT, ALTER VIEW and ALTER MATERIALIZED VIEW.

Covers (docs/PRDS/STORAGE_UNIT.md, VIEW_.md, MATERIALIZED_VIEW_.md):
  * ALTER STORAGE_UNIT <name> SET RETENTION <n> DAYS | FOREVER   (recorded, not enforced)
  * ALTER VIEW <name> AS SELECT ...   (validated, never circular, never breaks dependent views)
  * ALTER MATERIALIZED VIEW <name> SET REFRESH EVERY <n> <unit> | MANUAL, and the scheduler that
    acts on it: scheduled refresh, not refreshing early, MANUAL, a failing refresh, refreshing a
    view in another database without disturbing the session's current database

Usage:
    python3.11 scripts/test_alter_api.py --no-launch
    python3.11 scripts/test_alter_api.py --url http://127.0.0.1:1143 --no-launch

The server must already be running. Exit code is 0 if all tests pass, 1 otherwise.
"""

from __future__ import annotations

import argparse
import json
import sys
import tempfile
import time
from pathlib import Path
from typing import Callable

from _mnemo_client import Client, ServerProc, TestRunner, YELLOW, RESET

DB = "alter_db"
OTHER_DB = "alter_other_db"
UNIT = "alter_unit"

SCHEDULER_WAIT_S = 15.0  # generous: the scheduler ticks once a second


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
        return self.t.check(name, r.ok(), f"status={r.status} body={r.body[:300]!r}")

    def fails(self, name: str, sql: str, needle: str = "") -> bool:
        r = self.client.query(sql)
        good = (not r.ok()) and needle.lower() in r.body.lower()
        return self.t.check(name, good, f"status={r.status} body={r.body[:300]!r}")

    def rows(self, sql: str) -> list[dict]:
        return _rows(self.client.query(sql).body)

    def column(self, sql: str, column: str) -> list:
        return [row.get(column) for row in self.rows(sql)]

    def row_named(self, sql: str, name: str) -> dict:
        for row in self.rows(sql):
            if row.get("name") == name:
                return row
        return {}

    def describe(self, kind: str, name: str) -> dict:
        return {row.get("field"): row.get("value") for row in self.rows(f"DESCRIBE {kind} {name}")}

    def wait_until(self, predicate: Callable[[], bool], timeout: float = SCHEDULER_WAIT_S) -> bool:
        deadline = time.time() + timeout
        while time.time() < deadline:
            if predicate():
                return True
            time.sleep(0.25)
        return predicate()


def setup(x: Ctx) -> None:
    x.t.section("Setup")
    for label, sql in [
        ("drop db", f"DROP DATABASE IF EXISTS {DB}"),
        ("drop other db", f"DROP DATABASE IF EXISTS {OTHER_DB}"),
        ("create db", f"CREATE DATABASE {DB}"),
        ("create other db", f"CREATE DATABASE {OTHER_DB}"),
        ("use", f"USE {DB}"),
        ("items", "CREATE TABLE items (id Int64, name String, active Int64, deleted Int64)"),
        ("insert items", "INSERT INTO items VALUES (1, 'a', 1, 0), (2, 'b', 1, 1), "
                         "(3, 'c', 0, 0), (4, 'd', 1, 0)"),
    ]:
        x.ok(f"setup: {label}", sql)


def test_storage_unit_retention(x: Ctx, unit_path: str) -> None:
    x.t.section("ALTER STORAGE_UNIT ... SET RETENTION")
    x.ok("drop leftover unit", f"DROP STORAGE_UNIT IF EXISTS {UNIT}")
    x.ok("create unit", f"CREATE STORAGE_UNIT {UNIT} TYPE LOCAL PATH '{unit_path}'")

    row = x.row_named("SHOW STORAGE_UNITS", UNIT)
    x.t.check("retention defaults to FOREVER", row.get("retention") == "FOREVER", f"row={row}")
    described = x.describe("STORAGE_UNIT", UNIT)
    x.t.check("DESCRIBE agrees and says it is not enforced",
              described.get("retention") == "FOREVER" and described.get("retention_enforced") == "false",
              f"described={described}")

    x.ok("SET RETENTION 365 DAYS", f"ALTER STORAGE_UNIT {UNIT} SET RETENTION 365 DAYS")
    x.t.check("SHOW reports it", x.row_named("SHOW STORAGE_UNITS", UNIT).get("retention") == "365 DAYS")
    x.t.check("DESCRIBE reports it", x.describe("STORAGE_UNIT", UNIT).get("retention") == "365 DAYS")
    x.t.check("it is still reported as not enforced",
              x.describe("STORAGE_UNIT", UNIT).get("retention_enforced") == "false")

    x.ok("singular DAY is accepted", f"ALTER STORAGE_UNIT {UNIT} SET RETENTION 1 DAY")
    x.t.check("and is reported in the round-trippable form",
              x.row_named("SHOW STORAGE_UNITS", UNIT).get("retention") == "1 DAYS")

    x.ok("back to 365", f"ALTER STORAGE_UNIT {UNIT} SET RETENTION 365 DAYS")
    x.fails("zero days is rejected", f"ALTER STORAGE_UNIT {UNIT} SET RETENTION 0 DAYS", "at least 1")
    x.fails("a count without a unit is rejected", f"ALTER STORAGE_UNIT {UNIT} SET RETENTION 30", "DAYS")
    x.fails("a negative count is rejected", f"ALTER STORAGE_UNIT {UNIT} SET RETENTION -5 DAYS", "")
    x.fails("a count that does not fit is rejected",
            f"ALTER STORAGE_UNIT {UNIT} SET RETENTION 99999999999 DAYS", "whole number")
    x.fails("an unknown property is rejected", f"ALTER STORAGE_UNIT {UNIT} SET COLOR red", "RETENTION")
    x.fails("SET is required", f"ALTER STORAGE_UNIT {UNIT} RETENTION 5 DAYS", "SET")
    x.fails("trailing tokens are rejected",
            f"ALTER STORAGE_UNIT {UNIT} SET RETENTION 5 DAYS extra", "unexpected")
    x.fails("an unknown unit", "ALTER STORAGE_UNIT alter_no_such_unit SET RETENTION 5 DAYS",
            "alter_no_such_unit")
    x.t.check("rejected statements changed nothing",
              x.row_named("SHOW STORAGE_UNITS", UNIT).get("retention") == "365 DAYS")

    x.ok("SET RETENTION FOREVER", f"ALTER STORAGE_UNIT {UNIT} SET RETENTION FOREVER")
    x.t.check("back to FOREVER", x.row_named("SHOW STORAGE_UNITS", UNIT).get("retention") == "FOREVER")
    x.ok("drop unit", f"DROP STORAGE_UNIT {UNIT}")


def test_alter_view(x: Ctx) -> None:
    x.t.section("ALTER VIEW ... AS SELECT")
    x.ok("create view", "CREATE VIEW v_active AS SELECT id, name FROM items WHERE active = 1")
    x.t.check("original definition", x.column("SELECT id FROM v_active ORDER BY id", "id") == [1, 2, 4])

    x.ok("ALTER VIEW", "ALTER VIEW v_active AS SELECT id, name FROM items "
                       "WHERE active = 1 AND deleted = 0")
    x.t.check("new definition takes effect", x.column("SELECT id FROM v_active ORDER BY id", "id") == [1, 4])
    x.t.check("the view is still listed once", x.column("SHOW VIEWS", "name").count("v_active") == 1)

    x.ok("ALTER VIEW can change the columns", "ALTER VIEW v_active AS SELECT id FROM items WHERE active = 1")
    x.t.check("DESCRIBE shows the new columns", x.column("DESCRIBE VIEW v_active", "name") == ["id"],
              f"columns={x.column('DESCRIBE VIEW v_active', 'name')}")
    x.ok("restore", "ALTER VIEW v_active AS SELECT id, name FROM items WHERE active = 1")

    x.t.section("ALTER VIEW is refused when it should be")
    x.ok("a materialized view to aim ALTER VIEW at", "CREATE MATERIALIZED VIEW mv_kind AS SELECT id FROM items")
    x.fails("unknown view", "ALTER VIEW v_no_such AS SELECT id FROM items", "Unknown view")
    x.fails("a table is not a view", "ALTER VIEW items AS SELECT id FROM items", "is a table")
    x.fails("a materialized view is not a view", "ALTER VIEW mv_kind AS SELECT id FROM items",
            "is a materialized view")
    x.fails("AS is required", "ALTER VIEW v_active SELECT id FROM items", "AS")
    x.fails("unknown source table", "ALTER VIEW v_active AS SELECT id FROM v_no_table", "v_no_table")
    x.fails("unknown column", "ALTER VIEW v_active AS SELECT no_such_col FROM items", "Invalid view definition")
    x.ok("a view that outputs only id", "CREATE VIEW v_id_only AS SELECT id FROM items")
    x.fails("a column the source view does not output (the analyzer lets this through)",
            "ALTER VIEW v_active AS SELECT name FROM v_id_only", "Invalid view definition")
    x.fails("a view cannot read itself", "ALTER VIEW v_active AS SELECT id FROM v_active", "circular")
    x.t.check("every refusal left the definition alone",
              x.column("SELECT id FROM v_active ORDER BY id", "id") == [1, 2, 4])

    x.t.section("ALTER VIEW cannot create a cycle or break a dependent view")
    x.ok("base view", "CREATE VIEW v_base AS SELECT id, name FROM items")
    x.ok("view on the base view", "CREATE VIEW v_top AS SELECT id FROM v_base")
    x.fails("a cycle through another view", "ALTER VIEW v_base AS SELECT id FROM v_top", "circular")
    x.ok("a view that reads the name column", "CREATE VIEW v_names AS SELECT name FROM v_base")
    x.fails("dropping a column a dependent view reads",
            "ALTER VIEW v_base AS SELECT id FROM items", "v_names")
    x.t.check("the base view kept its columns",
              x.column("DESCRIBE VIEW v_base", "name") == ["id", "name"],
              f"columns={x.column('DESCRIBE VIEW v_base', 'name')}")
    x.t.check("and the dependent view still works",
              sorted(x.column("SELECT name FROM v_names", "name")) == ["a", "b", "c", "d"])
    x.ok("a compatible change is allowed", "ALTER VIEW v_base AS SELECT id, name FROM items WHERE active = 1")
    x.t.check("dependent views see it",
              sorted(x.column("SELECT name FROM v_names", "name")) == ["a", "b", "d"])


def test_alter_materialized_view(x: Ctx) -> None:
    x.t.section("ALTER MATERIALIZED VIEW ... SET REFRESH")
    x.ok("events", "CREATE TABLE events (id Int64, amount Int64)")
    x.ok("insert events", "INSERT INTO events VALUES (1, 10), (2, 20)")
    x.ok("create view", "CREATE MATERIALIZED VIEW mv_totals AS "
                        "SELECT COUNT(*) AS n, SUM(amount) AS total FROM events")

    def total_rows() -> list:
        return x.column("SELECT n FROM mv_totals", "n")

    row = x.row_named("SHOW MATERIALIZED VIEWS", "mv_totals")
    x.t.check("a new view is MANUAL and ACTIVE",
              row.get("refresh") == "MANUAL" and row.get("refresh_interval_seconds") == "0"
              and row.get("status") == "ACTIVE", f"row={row}")
    x.t.check("it records when it was populated", len(row.get("last_refreshed_at", "")) == 19, f"row={row}")
    created_at = row.get("last_refreshed_at")

    x.ok("insert a third event", "INSERT INTO events VALUES (3, 5)")
    time.sleep(2.5)
    x.t.check("a MANUAL view does not refresh by itself", total_rows() == [2], f"n={total_rows()}")

    x.ok("SET REFRESH EVERY 1 SECOND", "ALTER MATERIALIZED VIEW mv_totals SET REFRESH EVERY 1 SECOND")
    row = x.row_named("SHOW MATERIALIZED VIEWS", "mv_totals")
    x.t.check("it is now SCHEDULED",
              row.get("refresh") == "SCHEDULED" and row.get("refresh_interval_seconds") == "1", f"row={row}")
    x.t.check("the scheduler refreshes it", x.wait_until(lambda: total_rows() == [3]), f"n={total_rows()}")
    x.t.check("and records the refresh",
              x.row_named("SHOW MATERIALIZED VIEWS", "mv_totals").get("last_refreshed_at") != created_at)

    x.t.section("Scheduled refresh respects the interval and MANUAL")
    x.ok("SET REFRESH EVERY 1 HOUR", "ALTER MATERIALIZED VIEW mv_totals SET REFRESH EVERY 1 HOUR")
    x.t.check("an hour is 3600 seconds",
              x.row_named("SHOW MATERIALIZED VIEWS", "mv_totals").get("refresh_interval_seconds") == "3600")
    x.ok("insert a fourth event", "INSERT INTO events VALUES (4, 1)")
    time.sleep(3.0)
    x.t.check("it does not refresh before the interval has passed", total_rows() == [3], f"n={total_rows()}")

    for text, seconds in [("2 MINUTES", "120"), ("1 DAY", "86400"), ("3 DAYS", "259200"),
                          ("1 HOURS", "3600"), ("30 SECONDS", "30")]:
        x.ok(f"EVERY {text}", f"ALTER MATERIALIZED VIEW mv_totals SET REFRESH EVERY {text}")
        x.t.check(f"EVERY {text} is {seconds} seconds",
                  x.row_named("SHOW MATERIALIZED VIEWS", "mv_totals").get("refresh_interval_seconds") == seconds)

    x.ok("SET REFRESH EVERY 1 SECOND again", "ALTER MATERIALIZED VIEW mv_totals SET REFRESH EVERY 1 SECOND")
    x.t.check("picks up the fourth event", x.wait_until(lambda: total_rows() == [4]), f"n={total_rows()}")
    x.ok("SET REFRESH MANUAL", "ALTER MATERIALIZED VIEW mv_totals SET REFRESH MANUAL")
    row = x.row_named("SHOW MATERIALIZED VIEWS", "mv_totals")
    x.t.check("MANUAL again", row.get("refresh") == "MANUAL" and row.get("refresh_interval_seconds") == "0")
    x.ok("insert a fifth event", "INSERT INTO events VALUES (5, 1)")
    time.sleep(3.0)
    x.t.check("MANUAL stops the scheduler refreshing it", total_rows() == [4], f"n={total_rows()}")
    x.ok("REFRESH by hand", "REFRESH MATERIALIZED VIEW mv_totals")
    x.t.check("a manual refresh still works", total_rows() == [5], f"n={total_rows()}")

    x.t.section("ALTER MATERIALIZED VIEW is refused when it should be")
    x.fails("EVERY 0", "ALTER MATERIALIZED VIEW mv_totals SET REFRESH EVERY 0 SECONDS", "at least 1")
    x.fails("EVERY needs a number", "ALTER MATERIALIZED VIEW mv_totals SET REFRESH EVERY abc HOURS", "whole number")
    x.fails("an unknown unit", "ALTER MATERIALIZED VIEW mv_totals SET REFRESH EVERY 5 FORTNIGHTS", "SECONDS")
    x.fails("a missing unit", "ALTER MATERIALIZED VIEW mv_totals SET REFRESH EVERY 5", "SECONDS")
    x.fails("an interval that does not fit",
            "ALTER MATERIALIZED VIEW mv_totals SET REFRESH EVERY 99999999999999999999 HOURS", "too large")
    x.fails("an interval that overflows once multiplied",
            "ALTER MATERIALIZED VIEW mv_totals SET REFRESH EVERY 9999999999999999999 DAYS", "too large")
    x.fails("REFRESH needs EVERY or MANUAL", "ALTER MATERIALIZED VIEW mv_totals SET REFRESH", "EVERY")
    x.fails("an unknown property", "ALTER MATERIALIZED VIEW mv_totals SET COLOR red", "REFRESH")
    x.fails("trailing tokens", "ALTER MATERIALIZED VIEW mv_totals SET REFRESH MANUAL extra", "unexpected")
    x.fails("unknown materialized view", "ALTER MATERIALIZED VIEW mv_no_such SET REFRESH MANUAL",
            "Unknown materialized view")
    x.fails("a view is not a materialized view", "ALTER MATERIALIZED VIEW v_active SET REFRESH MANUAL", "is a view")
    x.fails("a table is not a materialized view", "ALTER MATERIALIZED VIEW items SET REFRESH MANUAL", "is a table")
    x.t.check("rejected statements changed nothing",
              x.row_named("SHOW MATERIALIZED VIEWS", "mv_totals").get("refresh") == "MANUAL")


def test_failing_refresh(x: Ctx) -> None:
    x.t.section("A failing scheduled refresh")
    x.ok("source table", "CREATE TABLE flaky (id Int64)")
    x.ok("insert", "INSERT INTO flaky VALUES (1), (2)")
    x.ok("create view", "CREATE MATERIALIZED VIEW mv_flaky AS SELECT COUNT(*) AS n FROM flaky")
    x.ok("schedule it", "ALTER MATERIALIZED VIEW mv_flaky SET REFRESH EVERY 1 SECOND")
    x.ok("drop the source table", "DROP TABLE flaky")

    def flaky() -> dict:
        return x.row_named("SHOW MATERIALIZED VIEWS", "mv_flaky")

    x.t.check("the view becomes DEGRADED", x.wait_until(lambda: flaky().get("status") == "DEGRADED"),
              f"row={flaky()}")
    x.t.check("and says why", "flaky" in flaky().get("last_error", ""), f"row={flaky()}")
    x.t.check("it keeps the last good data", x.column("SELECT n FROM mv_flaky", "n") == [2])
    x.t.check("the server keeps answering queries", x.client.query("SELECT 1").ok())

    x.ok("recreate the source table", "CREATE TABLE flaky (id Int64)")
    x.ok("with one row", "INSERT INTO flaky VALUES (7)")
    x.t.check("the scheduled retry recovers the view",
              x.wait_until(lambda: flaky().get("status") == "ACTIVE" and x.column(
                  "SELECT n FROM mv_flaky", "n") == [1]), f"row={flaky()}")
    x.t.check("and clears the error", flaky().get("last_error") == "", f"row={flaky()}")
    x.ok("stop scheduling it", "ALTER MATERIALIZED VIEW mv_flaky SET REFRESH MANUAL")


def test_other_database(x: Ctx) -> None:
    x.t.section("Scheduled refresh in another database")
    x.ok("use the other database", f"USE {OTHER_DB}")
    x.ok("table", "CREATE TABLE xsrc (id Int64)")
    x.ok("insert", "INSERT INTO xsrc VALUES (1)")
    x.ok("view", "CREATE MATERIALIZED VIEW mv_x AS SELECT COUNT(*) AS n FROM xsrc")
    x.ok("schedule it", "ALTER MATERIALIZED VIEW mv_x SET REFRESH EVERY 1 SECOND")
    x.ok("back to the first database", f"USE {DB}")
    x.ok("insert from here", f"INSERT INTO {OTHER_DB}.xsrc VALUES (2), (3)")

    def other_count() -> list:
        return x.column(f"SELECT n FROM {OTHER_DB}.mv_x", "n")

    x.t.check("it is refreshed while another database is current",
              x.wait_until(lambda: other_count() == [3]), f"n={other_count()}")
    tables = x.column("SHOW TABLES", "name")
    x.t.check("the session's current database was not changed",
              "items" in tables and "xsrc" not in tables, f"tables={tables}")

    x.ok("drop the database that holds a scheduled view", f"DROP DATABASE {OTHER_DB}")
    time.sleep(3.0)
    x.t.check("the scheduler survives it", x.client.query("SELECT 1").ok())
    x.t.check("and the first database is untouched", "items" in x.column("SHOW TABLES", "name"))


def cleanup(x: Ctx) -> None:
    x.t.section("Cleanup")
    x.ok("drop the unit", f"DROP STORAGE_UNIT IF EXISTS {UNIT}")
    x.ok("drop the other database", f"DROP DATABASE IF EXISTS {OTHER_DB}")
    x.ok("drop the database", f"DROP DATABASE IF EXISTS {DB}")


def run_tests(client: Client) -> int:
    t = TestRunner()
    x = Ctx(client, t)
    with tempfile.TemporaryDirectory(prefix="mnemo_alter_unit_") as unit_dir:
        setup(x)
        test_storage_unit_retention(x, unit_dir.replace("\\", "/"))
        test_alter_view(x)
        test_alter_materialized_view(x)
        test_failing_refresh(x)
        test_other_database(x)
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
