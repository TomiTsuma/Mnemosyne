#!/usr/bin/env python3
"""End-to-end API tests for Pipeline Layer Phase 1.

Usage:
    python scripts/test_pipelines_api.py
    python scripts/test_pipelines_api.py --url http://127.0.0.1:1143 --no-launch
    python scripts/test_pipelines_api.py --server-bin build/bin/Release/mnemosyne_server.exe

Exit code is 0 if all tests pass, 1 otherwise.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from datetime import datetime
from pathlib import Path

from _mnemo_client import (
    Client,
    ServerProc,
    TestRunner,
    YELLOW,
    RESET,
)

REPO_ROOT = Path(__file__).resolve().parent.parent
DB = "pipeline_test_db"
PIPELINE = "api_revenue_sync"
STAGE_INGEST = "ingestion"
STAGE_TRANSFORM = "transform"
TASK_A = "task_a"
TASK_B = "task_b"
TASK_SQL = "task_sql"
TRIGGER_NIGHTLY = "nightly"
MISSING_PIPELINE = "definitely_missing_pipeline_xyz"

# Multistep ETL pipeline (customers × orders × products → join view → regional MV)
MULTISTEP_PIPELINE = "multistep_revenue_etl"
MS_STAGE_DDL = "ddl"
MS_STAGE_LOAD = "load"
MS_STAGE_TRANSFORM = "transform"
MS_STAGE_VALIDATE = "validate"
MS_CUSTOMERS = "ms_customers"
MS_ORDERS = "ms_orders"
MS_PRODUCTS = "ms_products"


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


def _row_count(body: str) -> int:
    try:
        return int(json.loads(body).get("rows", -1))
    except (json.JSONDecodeError, TypeError, ValueError):
        return -1


def _scalar(body: str, column: str = "") -> str | None:
    rows = _rows(body)
    if not rows:
        return None
    if column:
        if column in rows[0]:
            return str(rows[0][column])
        # Aggregates without aliases often surface as COUNT(*), etc.
        for key, value in rows[0].items():
            if key.upper().startswith(column.upper()):
                return str(value)
        return None
    return str(next(iter(rows[0].values()), ""))


def _create_sql_task(
    client: Client,
    t: TestRunner,
    *,
    name: str,
    stage: str,
    pipeline: str,
    sql: str,
    depends_on: tuple[str, ...] = (),
) -> None:
    dep_clause = f" DEPENDS ON {' '.join(depends_on)}" if depends_on else ""
    r = client.query(
        f"CREATE TASK {name} IN STAGE {stage} IN PIPELINE {pipeline} "
        f"TYPE SQL BODY '{sql}'{dep_clause}"
    )
    t.check(f"CREATE TASK {name}", r.ok(), f"status={r.status} body={r.body!r}")


def setup(client: Client, t: TestRunner) -> None:
    t.section("Setup")
    for label, sql in [
        ("CREATE DATABASE", f"CREATE DATABASE IF NOT EXISTS {DB}"),
        ("USE", f"USE {DB}"),
        ("DROP PIPELINE", f"DROP PIPELINE IF EXISTS {PIPELINE}"),
    ]:
        r = client.query(sql)
        t.check(f"setup: {label}", r.ok(), f"status={r.status} body={r.body[:300]!r}")


def test_catalog_crud(client: Client, t: TestRunner) -> None:
    t.section("Catalog CRUD")

    r = client.query(f"CREATE PIPELINE {PIPELINE} OWNER 'analytics'")
    t.check("CREATE PIPELINE", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(
        f"CREATE STAGE {STAGE_INGEST} IN PIPELINE {PIPELINE} ORDER 1"
    )
    t.check("CREATE STAGE ingestion", r.ok(), f"status={r.status}")

    r = client.query(
        f"CREATE STAGE {STAGE_TRANSFORM} IN PIPELINE {PIPELINE} ORDER 2"
    )
    t.check("CREATE STAGE transform", r.ok(), f"status={r.status}")

    r = client.query(
        f"CREATE TASK {TASK_A} IN STAGE {STAGE_INGEST} IN PIPELINE {PIPELINE} "
        f"TYPE BUILT_IN"
    )
    t.check("CREATE TASK A", r.ok(), f"status={r.status}")

    r = client.query(
        f"CREATE TASK {TASK_B} IN STAGE {STAGE_INGEST} IN PIPELINE {PIPELINE} "
        f"TYPE BUILT_IN DEPENDS ON {TASK_A}"
    )
    t.check("CREATE TASK B", r.ok(), f"status={r.status}")

    r = client.query(
        f"CREATE TASK {TASK_SQL} IN STAGE {STAGE_TRANSFORM} IN PIPELINE {PIPELINE} "
        f"TYPE SQL BODY 'CREATE TABLE IF NOT EXISTS pipeline_out (id Int64) ENGINE=Memory' "
        f"DEPENDS ON {TASK_B}"
    )
    t.check("CREATE TASK SQL", r.ok(), f"status={r.status}")

    r = client.query(
        f"CREATE TRIGGER {TRIGGER_NIGHTLY} ON PIPELINE {PIPELINE} "
        f"SCHEDULE '* * * * *'"
    )
    t.check("CREATE TRIGGER", r.ok(), f"status={r.status}")

    r = client.query("SHOW PIPELINES")
    t.check("SHOW PIPELINES", r.ok(), f"status={r.status}")
    t.check("lists pipeline", PIPELINE in r.body, f"body={r.body!r}")
    t.check("shows ACTIVE", "active" in _body_lower(r.body), f"body={r.body!r}")

    r = client.query(f"SHOW STAGES FROM PIPELINE {PIPELINE}")
    t.check("SHOW STAGES", r.ok(), f"status={r.status}")
    for stage in (STAGE_INGEST, STAGE_TRANSFORM):
        t.check(f"lists stage {stage}", stage in r.body, f"body={r.body!r}")

    r = client.query(f"SHOW TASKS FROM PIPELINE {PIPELINE}")
    t.check("SHOW TASKS", r.ok(), f"status={r.status}")
    for task in (TASK_A, TASK_B, TASK_SQL):
        t.check(f"lists task {task}", task in r.body, f"body={r.body!r}")

    r = client.query(f"SHOW TRIGGERS FROM PIPELINE {PIPELINE}")
    t.check("SHOW TRIGGERS", r.ok(), f"status={r.status}")
    t.check("lists trigger", TRIGGER_NIGHTLY in r.body, f"body={r.body!r}")

    r = client.query(f"DESCRIBE PIPELINE {PIPELINE}")
    t.check("DESCRIBE PIPELINE", r.ok(), f"status={r.status}")
    t.check("includes owner", "analytics" in r.body.lower(), f"body={r.body!r}")
    t.check("includes stage_count", "stage_count" in _body_lower(r.body), f"body={r.body!r}")


def test_dag_execution(client: Client, t: TestRunner) -> None:
    t.section("DAG execution")

    r = client.query(f"RUN PIPELINE {PIPELINE}")
    t.check("RUN PIPELINE", r.ok(), f"status={r.status} body={r.body!r}")
    t.check("run succeeded", "succeeded" in _body_lower(r.body), f"body={r.body!r}")

    r = client.query(f"SHOW PIPELINE_RUNS FOR PIPELINE {PIPELINE}")
    t.check("SHOW PIPELINE_RUNS", r.ok(), f"status={r.status}")
    t.check("has succeeded run", "succeeded" in _body_lower(r.body), f"body={r.body!r}")
    t.check("records manual trigger", "manual" in _body_lower(r.body), f"body={r.body!r}")


def test_pause_resume(client: Client, t: TestRunner) -> None:
    t.section("Pause and resume")

    r = client.query(f"PAUSE PIPELINE {PIPELINE}")
    t.check("PAUSE PIPELINE", r.ok(), f"status={r.status}")

    r = client.query(f"RUN PIPELINE {PIPELINE}")
    t.check("RUN while paused fails", not r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"RESUME PIPELINE {PIPELINE}")
    t.check("RESUME PIPELINE", r.ok(), f"status={r.status}")

    r = client.query(f"RUN PIPELINE {PIPELINE}")
    t.check("RUN after resume", r.ok(), f"status={r.status}")


def test_multistep_data_pipeline(client: Client, t: TestRunner) -> None:
    """ETL-style pipeline: schema → load → join view → regional materialized view."""
    t.section("Multistep data pipeline")

    for sql in (
        f"DROP TABLE IF EXISTS {MS_ORDERS}",
        f"DROP TABLE IF EXISTS {MS_CUSTOMERS}",
        f"DROP TABLE IF EXISTS {MS_PRODUCTS}",
        f"DROP PIPELINE IF EXISTS {MULTISTEP_PIPELINE}",
    ):
        client.query(sql)

    r = client.query(f"CREATE PIPELINE {MULTISTEP_PIPELINE} OWNER 'etl_team'")
    t.check("CREATE multistep pipeline", r.ok(), f"status={r.status}")

    for stage, order in (
        (MS_STAGE_DDL, 1),
        (MS_STAGE_LOAD, 2),
        (MS_STAGE_TRANSFORM, 3),
        (MS_STAGE_VALIDATE, 4),
    ):
        r = client.query(
            f"CREATE STAGE {stage} IN PIPELINE {MULTISTEP_PIPELINE} ORDER {order}"
        )
        t.check(f"CREATE STAGE {stage}", r.ok(), f"status={r.status}")

    # Stage 1 — dimension table DDL (independent tasks, no cross-deps)
    _create_sql_task(
        client,
        t,
        name="ddl_customers",
        stage=MS_STAGE_DDL,
        pipeline=MULTISTEP_PIPELINE,
        sql=(
            f"CREATE TABLE {MS_CUSTOMERS} "
            f"(customer_id Int64, region_id Int64, tier Int64) ENGINE=Memory"
        ),
    )
    _create_sql_task(
        client,
        t,
        name="ddl_orders",
        stage=MS_STAGE_DDL,
        pipeline=MULTISTEP_PIPELINE,
        sql=(
            f"CREATE TABLE {MS_ORDERS} "
            f"(order_id Int64, customer_id Int64, product_id Int64, amount Float64) "
            f"ENGINE=Memory"
        ),
    )
    _create_sql_task(
        client,
        t,
        name="ddl_products",
        stage=MS_STAGE_DDL,
        pipeline=MULTISTEP_PIPELINE,
        sql=(
            f"CREATE TABLE {MS_PRODUCTS} "
            f"(product_id Int64, category_id Int64) ENGINE=Memory"
        ),
    )

    # Stage 2 — load fact/dimension data into separate tables
    _create_sql_task(
        client,
        t,
        name="load_customers",
        stage=MS_STAGE_LOAD,
        pipeline=MULTISTEP_PIPELINE,
        sql=f"INSERT INTO {MS_CUSTOMERS} VALUES (1, 10, 1), (2, 20, 2), (3, 10, 1)",
        depends_on=("ddl_customers",),
    )
    _create_sql_task(
        client,
        t,
        name="load_orders",
        stage=MS_STAGE_LOAD,
        pipeline=MULTISTEP_PIPELINE,
        sql=(
            f"INSERT INTO {MS_ORDERS} VALUES "
            f"(1, 1, 100, 99.5), (2, 2, 200, 150.0), (3, 1, 100, 50.0)"
        ),
        depends_on=("ddl_orders",),
    )
    _create_sql_task(
        client,
        t,
        name="load_products",
        stage=MS_STAGE_LOAD,
        pipeline=MULTISTEP_PIPELINE,
        sql=f"INSERT INTO {MS_PRODUCTS} VALUES (100, 1), (200, 2)",
        depends_on=("ddl_products",),
    )

    # Stage 3 — join and aggregate across tables (alias JOIN; works in SELECT tasks)
    _create_sql_task(
        client,
        t,
        name="probe_three_way_join",
        stage=MS_STAGE_TRANSFORM,
        pipeline=MULTISTEP_PIPELINE,
        sql=(
            f"SELECT COUNT(*) AS join_rows FROM {MS_CUSTOMERS} c "
            f"JOIN {MS_ORDERS} o ON c.customer_id = o.customer_id "
            f"JOIN {MS_PRODUCTS} p ON o.product_id = p.product_id"
        ),
        depends_on=("load_customers", "load_orders", "load_products"),
    )
    _create_sql_task(
        client,
        t,
        name="probe_regional_revenue",
        stage=MS_STAGE_TRANSFORM,
        pipeline=MULTISTEP_PIPELINE,
        sql=(
            f"SELECT c.region_id, SUM(o.amount) AS total_revenue, "
            f"COUNT(*) AS order_count "
            f"FROM {MS_CUSTOMERS} c "
            f"JOIN {MS_ORDERS} o ON c.customer_id = o.customer_id "
            f"GROUP BY c.region_id"
        ),
        depends_on=("load_customers", "load_orders"),
    )

    # Stage 4 — filter joined data (customer ↔ order ↔ product linkage)
    _create_sql_task(
        client,
        t,
        name="validate_order_link",
        stage=MS_STAGE_VALIDATE,
        pipeline=MULTISTEP_PIPELINE,
        sql=(
            f"SELECT c.customer_id, c.region_id, o.order_id, o.amount, p.category_id "
            f"FROM {MS_CUSTOMERS} c "
            f"JOIN {MS_ORDERS} o ON c.customer_id = o.customer_id "
            f"JOIN {MS_PRODUCTS} p ON o.product_id = p.product_id "
            f"WHERE o.order_id = 1"
        ),
        depends_on=("probe_three_way_join", "probe_regional_revenue"),
    )

    r = client.query(f"SHOW TASKS FROM PIPELINE {MULTISTEP_PIPELINE}")
    t.check("SHOW multistep tasks", r.ok(), f"status={r.status}")
    t.check("has 9 tasks", _row_count(r.body) == 9, f"body={r.body!r}")

    r = client.query(f"RUN PIPELINE {MULTISTEP_PIPELINE}")
    t.check("RUN multistep pipeline", r.ok(), f"status={r.status} body={r.body!r}")
    t.check(
        "multistep run succeeded",
        "succeeded" in _body_lower(r.body),
        f"body={r.body!r}",
    )

    # Post-run checks — three-way join exposes one row per order (3 orders)
    r = client.query(
        f"SELECT COUNT(*) AS join_row_count FROM {MS_CUSTOMERS} c "
        f"JOIN {MS_ORDERS} o ON c.customer_id = o.customer_id "
        f"JOIN {MS_PRODUCTS} p ON o.product_id = p.product_id"
    )
    t.check("three-way join query", r.ok(), f"status={r.status}")
    join_count = _scalar(r.body) or _scalar(r.body, "join_row_count")
    t.check(
        "join returns 3 enriched rows",
        join_count == "3",
        f"count={join_count!r} body={r.body!r}",
    )

    r = client.query(
        f"SELECT c.customer_id, c.region_id, o.amount "
        f"FROM {MS_CUSTOMERS} c "
        f"JOIN {MS_ORDERS} o ON c.customer_id = o.customer_id "
        f"WHERE o.order_id = 1"
    )
    t.check("query joined tables", r.ok(), f"status={r.status}")
    rows = _rows(r.body)
    t.check(
        "join links customer and order",
        len(rows) == 1
        and str(rows[0].get("customer_id")) == "1"
        and str(rows[0].get("region_id")) == "10",
        f"rows={rows}",
    )

    # Regional revenue: region 10 → 99.5 + 50.0 = 149.5; region 20 → 150.0
    r = client.query(
        f"SELECT c.region_id, SUM(o.amount) AS total_revenue, COUNT(*) AS order_count "
        f"FROM {MS_CUSTOMERS} c "
        f"JOIN {MS_ORDERS} o ON c.customer_id = o.customer_id "
        f"GROUP BY c.region_id ORDER BY c.region_id"
    )
    t.check("query regional revenue aggregate", r.ok(), f"status={r.status}")
    mv_rows = _rows(r.body)
    t.check("aggregate has 2 regions", len(mv_rows) == 2, f"rows={mv_rows}")

    by_region = {str(row.get("region_id")): row for row in mv_rows}
    t.check(
        "region 10 total revenue",
        abs(float(by_region.get("10", {}).get("total_revenue", 0)) - 149.5) < 1.0,
        f"region_10={by_region.get('10')}",
    )
    t.check(
        "region 20 total revenue",
        abs(float(by_region.get("20", {}).get("total_revenue", 0)) - 150.0) < 1.0,
        f"region_20={by_region.get('20')}",
    )
    t.check(
        "region 10 order count",
        str(by_region.get("10", {}).get("order_count")) == "2",
        f"region_10={by_region.get('10')}",
    )

    r = client.query(f"SHOW PIPELINE_RUNS FOR PIPELINE {MULTISTEP_PIPELINE}")
    t.check("multistep run recorded", r.ok(), f"status={r.status}")
    t.check(
        "multistep run history succeeded",
        "succeeded" in _body_lower(r.body),
        f"body={r.body!r}",
    )

    client.query(f"DROP PIPELINE IF EXISTS {MULTISTEP_PIPELINE}")


def test_metrics(client: Client, t: TestRunner) -> None:
    t.section("Pipeline metrics")

    r = client.query(f"SHOW PIPELINE_METRICS FOR PIPELINE {PIPELINE}")
    t.check("SHOW PIPELINE_METRICS", r.ok(), f"status={r.status}")
    body_lower = _body_lower(r.body)
    t.check("includes run_count", "run_count" in body_lower, f"body={r.body!r}")
    t.check("includes success_rate", "success_rate" in body_lower, f"body={r.body!r}")
    t.check("includes avg_duration", "avg_duration" in body_lower, f"body={r.body!r}")


def test_scheduler_smoke(client: Client, t: TestRunner) -> None:
    t.section("Scheduler smoke")

    sched_pipeline = "api_sched_pipeline"
    client.query(f"DROP PIPELINE IF EXISTS {sched_pipeline}")
    client.query(f"CREATE PIPELINE {sched_pipeline}")
    client.query(
        f"CREATE STAGE s1 IN PIPELINE {sched_pipeline} ORDER 1"
    )
    client.query(
        f"CREATE TASK t1 IN STAGE s1 IN PIPELINE {sched_pipeline} TYPE BUILT_IN"
    )
    now = datetime.now()
    cron = f"{now.minute} {now.hour} * * *"
    r = client.query(
        f"CREATE TRIGGER every_minute ON PIPELINE {sched_pipeline} SCHEDULE '{cron}'"
    )
    t.check("CREATE schedule trigger", r.ok(), f"status={r.status}")

    before = _rows(client.query(f"SHOW PIPELINE_RUNS FOR PIPELINE {sched_pipeline}").body)
    deadline = time.time() + 12
    fired = False
    while time.time() < deadline:
        time.sleep(2)
        after = _rows(client.query(f"SHOW PIPELINE_RUNS FOR PIPELINE {sched_pipeline}").body)
        if len(after) > len(before):
            fired = True
            break
    t.check(
        "scheduler fired pipeline run",
        fired,
        "expected scheduled run within 12s (cron matches current minute)",
    )

    client.query(f"DROP PIPELINE IF EXISTS {sched_pipeline}")


def test_errors(client: Client, t: TestRunner) -> None:
    t.section("Error handling")

    r = client.query(f"CREATE PIPELINE {PIPELINE} OWNER 'dup'")
    t.check("duplicate CREATE PIPELINE fails", not r.ok(), f"status={r.status}")

    r = client.query(f"RUN PIPELINE {MISSING_PIPELINE}")
    t.check("RUN missing pipeline fails", not r.ok(), f"status={r.status}")

    cyclic = "api_cyclic_pipeline"
    client.query(f"DROP PIPELINE IF EXISTS {cyclic}")
    client.query(f"CREATE PIPELINE {cyclic}")
    client.query(f"CREATE STAGE s IN PIPELINE {cyclic}")
    client.query(
        f"CREATE TASK x IN STAGE s IN PIPELINE {cyclic} TYPE BUILT_IN DEPENDS ON y"
    )
    client.query(
        f"CREATE TASK y IN STAGE s IN PIPELINE {cyclic} TYPE BUILT_IN DEPENDS ON x"
    )
    r = client.query(f"RUN PIPELINE {cyclic}")
    t.check("cyclic DAG fails", not r.ok(), f"status={r.status} body={r.body!r}")
    client.query(f"DROP PIPELINE IF EXISTS {cyclic}")

    r = client.query(f"ALTER PIPELINE {PIPELINE} SET OWNER 'data_team'")
    t.check("ALTER PIPELINE owner", r.ok(), f"status={r.status}")


def cleanup(client: Client, t: TestRunner) -> None:
    t.section("Cleanup")
    r = client.query(f"DROP PIPELINE IF EXISTS {PIPELINE}")
    t.check("DROP PIPELINE", r.ok(), f"status={r.status}")


def run_tests(client: Client) -> int:
    t = TestRunner()
    setup(client, t)
    test_catalog_crud(client, t)
    test_dag_execution(client, t)
    test_multistep_data_pipeline(client, t)
    test_pause_resume(client, t)
    test_metrics(client, t)
    test_scheduler_smoke(client, t)
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
