#!/usr/bin/env python3.11
"""End-to-end API tests for SHOW TASK RUNS and the pipeline run history behind it.

Covers (docs/PRDS/PIPELINE_LAYER_.md, section 22, Monitoring):
  * SHOW TASK RUNS [FOR PIPELINE p] and SHOW TASK_RUNS: one row per task per run, with the stage,
    status, duration, start time, the error of a failed task and a note on a successful one
  * a failed run says which task failed and why, and the tasks it never reached are SKIPPED
  * data quality results: a FAIL rule fails its task, a WARN rule leaves a note on it
  * SHOW PIPELINE RUNS / METRICS in their two-word spelling, and that an unknown word after
    SHOW PIPELINE is a syntax error instead of silently listing pipelines

Usage:
    python3.11 scripts/test_task_runs_api.py --no-launch
    python3.11 scripts/test_task_runs_api.py --url http://127.0.0.1:1143 --no-launch

The server must already be running. Exit code is 0 if all tests pass, 1 otherwise.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from _mnemo_client import Client, ServerProc, TestRunner, YELLOW, RESET

DB = "task_runs_db"
OK, FAILING, SQL_FAIL, WARN, QUALITY_FAIL = "tr_ok", "tr_fail", "tr_sql_fail", "tr_warn", "tr_quality_fail"
RULE_WARN, RULE_FAIL = "tr_rule_warn", "tr_rule_fail"
ALL_PIPELINES = (OK, FAILING, SQL_FAIL, WARN, QUALITY_FAIL)


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

    def run(self, pipeline: str) -> str:
        rows = self.rows(f"RUN PIPELINE {pipeline}")
        return rows[0].get("status", "") if rows else ""

    def task_runs(self, pipeline: str) -> list[dict]:
        return self.rows(f"SHOW TASK RUNS FOR PIPELINE {pipeline}")

    def task(self, pipeline: str, name: str, run_index: int = -1) -> dict:
        runs = [r for r in self.task_runs(pipeline) if r.get("task") == name]
        return runs[run_index] if runs else {}

    def build(self, name: str, tasks: list[tuple[str, str, str, str, str]]) -> None:
        """tasks are (stage, task, type, body, depends_on); stages are created in order."""
        self.ok(f"{name}: pipeline", f"CREATE PIPELINE {name}")
        stages = list(dict.fromkeys(stage for stage, *_ in tasks))
        for order, stage in enumerate(stages, start=1):
            self.ok(f"{name}: stage {stage}", f"CREATE STAGE {stage} IN PIPELINE {name} ORDER {order}")
        for stage, task, kind, body, depends in tasks:
            sql = f"CREATE TASK {task} IN STAGE {stage} IN PIPELINE {name} TYPE {kind}"
            if body:
                sql += f" BODY '{body}'"
            if depends:
                sql += f" DEPENDS ON {depends}"
            self.ok(f"{name}: task {task}", sql)


def setup(x: Ctx) -> None:
    x.t.section("Setup")
    for label, sql in [
        ("drop db", f"DROP DATABASE IF EXISTS {DB}"),
        ("create db", f"CREATE DATABASE {DB}"),
        ("use", f"USE {DB}"),
        ("table", "CREATE TABLE nums (n Int64)"),
    ]:
        x.ok(f"setup: {label}", sql)
    for pipeline in ALL_PIPELINES:
        x.ok(f"setup: drop leftover {pipeline}", f"DROP PIPELINE IF EXISTS {pipeline}")
    for rule in (RULE_WARN, RULE_FAIL):
        x.ok(f"setup: drop leftover rule {rule}", f"DROP DATA_QUALITY_RULE IF EXISTS {rule}")
    x.ok("setup: warning rule", f"CREATE DATA_QUALITY_RULE {RULE_WARN} ON nums RANGE n MAX 0 ACTION WARN")
    x.ok("setup: failing rule", f"CREATE DATA_QUALITY_RULE {RULE_FAIL} ON nums RANGE n MAX 0 ACTION FAIL")


def test_successful_run(x: Ctx) -> None:
    x.t.section("SHOW TASK RUNS: a successful run")
    x.build(OK, [
        ("ingest", "load", "SQL", "INSERT INTO nums VALUES (1)", ""),
        ("serve", "finish", "BUILT_IN", "", "load"),  # not "publish": that is a statement keyword
    ])
    x.t.check("the run succeeds", x.run(OK) == "SUCCEEDED")
    rows = x.task_runs(OK)
    x.t.check("one row per task", [r.get("task") for r in rows] == ["load", "finish"], f"rows={rows}")
    x.t.check("each names its stage", [r.get("stage") for r in rows] == ["ingest", "serve"])
    x.t.check("each succeeded with no error", all(r.get("status") == "SUCCEEDED" and r.get("error") == "" for r in rows))
    x.t.check("each has a start time and a duration",
              all(len(r.get("started_at", "")) == 19 and float(r.get("duration_ms", "-1")) >= 0 for r in rows),
              f"rows={rows}")
    x.t.check("every row carries the run and pipeline",
              len({r.get("run_id") for r in rows}) == 1 and {r.get("pipeline") for r in rows} == {OK})
    x.t.check("the columns are as documented",
              list(rows[0]) == ["run_id", "pipeline", "stage", "task", "status", "duration_ms",
                                "started_at", "error", "detail"], f"columns={list(rows[0])}")

    x.t.check("a second run adds rows, oldest first",
              x.run(OK) == "SUCCEEDED" and len(x.task_runs(OK)) == 4
              and len({r.get("run_id") for r in x.task_runs(OK)}) == 2)


def test_failed_run(x: Ctx) -> None:
    x.t.section("SHOW TASK RUNS: a failed run says which task and why")
    x.build(FAILING, [
        ("etl", "first", "SQL", "INSERT INTO nums VALUES (2)", ""),
        ("etl", "boom", "BUILT_IN", "fail", "first"),
        ("etl", "after_one", "SQL", "INSERT INTO nums VALUES (3)", "boom"),
        ("etl", "after_two", "BUILT_IN", "", "after_one"),
    ])
    x.t.check("the run fails", x.run(FAILING) == "FAILED")
    rows = x.task_runs(FAILING)
    x.t.check("every task is accounted for",
              [r.get("task") for r in rows] == ["first", "boom", "after_one", "after_two"], f"rows={rows}")
    x.t.check("the statuses show where it stopped",
              [r.get("status") for r in rows] == ["SUCCEEDED", "FAILED", "SKIPPED", "SKIPPED"], f"rows={rows}")
    x.t.check("the failed task carries the cause", "forced failure" in rows[1].get("error", "")
              and "boom" in rows[1].get("error", ""), f"row={rows[1]}")
    x.t.check("skipped tasks say why and have no error",
              all("boom" in r.get("detail", "") and r.get("error") == "" for r in rows[2:]), f"rows={rows[2:]}")
    x.t.check("a skipped task has no start time", all(r.get("started_at") == "" for r in rows[2:]))
    # `first` inserted 2 before the failure; `after_one` would have inserted 3.
    x.t.check("the task before the failure ran and the skipped task did not",
              x.rows("SELECT COUNT(*) AS c FROM nums WHERE n = 2")[0]["c"] >= 1
              and x.rows("SELECT COUNT(*) AS c FROM nums WHERE n = 3")[0]["c"] == 0)

    x.build(SQL_FAIL, [("etl", "bad_sql", "SQL", "INSERT INTO tr_no_such_table VALUES (1)", "")])
    x.t.check("a failing SQL task fails the run", x.run(SQL_FAIL) == "FAILED")
    row = x.task(SQL_FAIL, "bad_sql")
    x.t.check("and its error names the table", row.get("status") == "FAILED" and "tr_no_such_table" in row.get("error", ""),
              f"row={row}")


def test_data_quality(x: Ctx) -> None:
    x.t.section("SHOW TASK RUNS: data quality results")
    x.build(WARN, [
        ("checks", "check", "QUALITY", RULE_WARN, ""),
        ("checks", "done", "BUILT_IN", "", "check"),
    ])
    x.t.check("a WARN rule does not fail the run", x.run(WARN) == "SUCCEEDED")
    check = x.task(WARN, "check")
    x.t.check("the task succeeded", check.get("status") == "SUCCEEDED" and check.get("error") == "", f"row={check}")
    x.t.check("and carries the warning", RULE_WARN in check.get("detail", "") and "violations" in check.get("detail", ""),
              f"row={check}")
    x.t.check("later tasks ran", x.task(WARN, "done").get("status") == "SUCCEEDED")

    x.build(QUALITY_FAIL, [
        ("checks", "check", "QUALITY", RULE_FAIL, ""),
        ("checks", "done", "BUILT_IN", "", "check"),
    ])
    x.t.check("a FAIL rule fails the run", x.run(QUALITY_FAIL) == "FAILED")
    check = x.task(QUALITY_FAIL, "check")
    x.t.check("the task error names the rule and the count",
              check.get("status") == "FAILED" and f"Data quality rule failed: {RULE_FAIL}" in check.get("error", "")
              and "violations" in check.get("error", ""), f"row={check}")
    x.t.check("later tasks are skipped", x.task(QUALITY_FAIL, "done").get("status") == "SKIPPED")


def test_filters_and_spellings(x: Ctx) -> None:
    x.t.section("SHOW TASK RUNS: all pipelines, spellings and errors")
    everything = x.rows("SHOW TASK RUNS")
    x.t.check("without FOR PIPELINE it covers every pipeline",
              set(ALL_PIPELINES) <= {r.get("pipeline") for r in everything}, f"pipelines={sorted({r.get('pipeline') for r in everything})}")
    x.t.check("with FOR PIPELINE it covers just that one",
              {r.get("pipeline") for r in x.task_runs(OK)} == {OK})
    x.t.check("SHOW TASK_RUNS means the same", len(x.rows("SHOW TASK_RUNS")) == len(everything))
    x.t.check("so does a lower-case spelling", len(x.rows("show task runs")) == len(everything))
    x.fails("an unknown pipeline", "SHOW TASK RUNS FOR PIPELINE tr_missing", "tr_missing")
    x.fails("TASK needs RUNS", "SHOW TASK", "RUNS")
    x.fails("FOR needs PIPELINE", f"SHOW TASK RUNS FOR {OK}", "PIPELINE")
    x.t.check("SHOW TASKS is unaffected", x.client.query(f"SHOW TASKS FROM PIPELINE {OK}").ok())


def test_show_pipeline_spellings(x: Ctx) -> None:
    x.t.section("SHOW PIPELINE RUNS / METRICS")
    runs = x.rows(f"SHOW PIPELINE RUNS FOR PIPELINE {OK}")
    x.t.check("the two-word spelling lists runs, not pipelines",
              runs and list(runs[0]) == ["run_id", "pipeline", "trigger", "status", "duration_ms"], f"rows={runs}")
    x.t.check("the underscore spelling agrees", x.rows(f"SHOW PIPELINE_RUNS FOR PIPELINE {OK}") == runs)
    x.t.check("without FOR PIPELINE it lists every run",
              len(x.rows("SHOW PIPELINE RUNS")) >= len(runs) and "run_id" in (x.rows("SHOW PIPELINE RUNS") or [{}])[0])
    metrics = x.rows(f"SHOW PIPELINE METRICS FOR PIPELINE {OK}")
    x.t.check("METRICS still works", metrics and metrics[0].get("run_count") == "2", f"rows={metrics}")
    x.t.check("a bare SHOW PIPELINE still lists pipelines", "owner" in (x.rows("SHOW PIPELINE") or [{}])[0])
    x.t.check("SHOW PIPELINES is unaffected", OK in [r.get("name") for r in x.rows("SHOW PIPELINES")])
    x.fails("an unknown word is a syntax error, not a list of pipelines", "SHOW PIPELINE BOGUS",
            "expected RUNS or METRICS")


def cleanup(x: Ctx) -> None:
    x.t.section("Cleanup")
    for pipeline in ALL_PIPELINES:
        x.ok(f"drop pipeline {pipeline}", f"DROP PIPELINE IF EXISTS {pipeline}")
    for rule in (RULE_WARN, RULE_FAIL):
        x.ok(f"drop rule {rule}", f"DROP DATA_QUALITY_RULE IF EXISTS {rule}")
    x.ok("drop database", f"DROP DATABASE IF EXISTS {DB}")


def run_tests(client: Client) -> int:
    t = TestRunner()
    x = Ctx(client, t)
    setup(x)
    test_successful_run(x)
    test_failed_run(x)
    test_data_quality(x)
    test_filters_and_spellings(x)
    test_show_pipeline_spellings(x)
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
