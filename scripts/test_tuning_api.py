#!/usr/bin/env python3
"""End-to-end API tests for MODEL layer hyperparameter tuning (PRD §13/§14).

Flow:
    seed table -> CREATE FEATURE_SET -> CREATE MODEL -> CREATE TRAINING_JOB
    -> CREATE TUNING_JOB (STRATEGY RANDOM, TRIALS n) -> RUN TUNING_JOB
    -> assert N trials recorded + a best version registered.

STRATEGY RANDOM/GRID work with only scikit-learn (optuna is optional and the
runtime falls back automatically). If scikit-learn is absent the execution
section is skipped; catalog CRUD still runs.

Usage:
    python scripts/test_tuning_api.py
    python scripts/test_tuning_api.py --no-launch

Exit code is 0 if all tests pass, 1 otherwise.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

from _mnemo_client import Client, ServerProc, TestRunner, YELLOW, RESET

DB = "tuning_test_db"
SOURCE = "tune_customers"
FS = "tune_features"
MODEL = "tune_model"
TRAIN_JOB = "tune_training"
TUNE_JOB = "tune_sweep"
TRIALS = 6


def _rows(body: str) -> list[dict]:
    try:
        data = json.loads(body)
        cols = data.get("columns", [])
        rows = data.get("data", [])
        return [dict(zip(cols, row)) for row in rows]
    except (json.JSONDecodeError, TypeError):
        return []


def _ml_runtime_available() -> bool:
    py = os.environ.get("MNEMO_PYTHON", sys.executable)
    try:
        return subprocess.run(
            [py, "-c", "import sklearn, pandas, numpy, joblib"],
            capture_output=True,
        ).returncode == 0
    except OSError:
        return False


def _seed_values(n: int = 80) -> str:
    import random
    rng = random.Random(29)
    parts = []
    for i in range(1, n + 1):
        label = 1 if rng.random() < 0.5 else 0
        f1 = round(rng.gauss(10 + 6 * label, 2), 3)
        f2 = round(rng.gauss(100 - 30 * label, 12), 3)
        f3 = int(max(0, rng.gauss(2 + 4 * label, 1)))
        parts.append(f"({i}, {f1}, {f2}, {f3}, {label})")
    return ", ".join(parts)


def setup(client: Client, t: TestRunner) -> None:
    t.section("Setup")
    for sql in [
        f"CREATE DATABASE IF NOT EXISTS {DB}",
        f"USE {DB}",
        f"DROP TUNING_JOB IF EXISTS {TUNE_JOB}",
        f"DROP TRAINING_JOB IF EXISTS {TRAIN_JOB}",
        f"DROP MODEL IF EXISTS {MODEL}",
        f"DROP FEATURE_SET IF EXISTS {FS}",
        f"DROP TABLE IF EXISTS {SOURCE}",
    ]:
        r = client.query(sql)
        t.check(f"setup: {sql.split()[0]} {sql.split()[1]}", r.ok(),
                f"status={r.status} body={r.body[:200]!r}")

    r = client.query(
        f"CREATE TABLE {SOURCE} "
        f"(uid Int64, f1 Float64, f2 Float64, f3 Int64, label Int64) ENGINE=Memory"
    )
    t.check("CREATE source table", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"INSERT INTO {SOURCE} VALUES {_seed_values()}")
    t.check("INSERT seed rows", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(
        f"CREATE FEATURE_SET {FS} FROM {SOURCE} "
        f"ENTITY_KEY(uid) FEATURES(f1, f2, f3) TARGET label"
    )
    t.check("CREATE FEATURE_SET", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"CREATE MODEL {MODEL} TYPE CLASSIFICATION")
    t.check("CREATE MODEL", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(
        f"CREATE TRAINING_JOB {TRAIN_JOB} MODEL {MODEL} FEATURE_SET {FS} "
        f"FRAMEWORK SKLEARN ALGORITHM RandomForestClassifier OBJECTIVE ACCURACY"
    )
    t.check("CREATE TRAINING_JOB", r.ok(), f"status={r.status} body={r.body!r}")


def test_tuning_catalog(client: Client, t: TestRunner) -> None:
    t.section("Tuning job catalog")

    r = client.query(
        f"CREATE TUNING_JOB {TUNE_JOB} TRAINING_JOB {TRAIN_JOB} "
        f"STRATEGY RANDOM TRIALS {TRIALS} OBJECTIVE ACCURACY "
        f"SEARCH_SPACE(n_estimators='20..120', max_depth='2..12')"
    )
    t.check("CREATE TUNING_JOB", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("SHOW TUNING_JOBS")
    t.check("SHOW TUNING_JOBS", r.ok(), f"status={r.status}")
    t.check("lists tuning job", TUNE_JOB in r.body, f"body={r.body!r}")
    rows = _rows(r.body)
    match = next((row for row in rows if row.get("name") == TUNE_JOB), None)
    if match:
        t.check("strategy RANDOM", "RANDOM" in str(match.get("strategy")), f"row={match}")
        t.check("records trial count", str(match.get("trials")) == str(TRIALS), f"row={match}")

    r = client.query(f"DESCRIBE TUNING_JOB {TUNE_JOB}")
    t.check("DESCRIBE TUNING_JOB", r.ok(), f"status={r.status}")


def test_tuning_run(client: Client, t: TestRunner, *, ml: bool) -> None:
    t.section("Tuning execution")
    if not ml:
        r = client.query(f"RUN TUNING_JOB {TUNE_JOB}")
        t.check(
            "RUN TUNING_JOB reports missing ML deps",
            not r.ok() and ("library" in r.body.lower() or "missing" in r.body.lower()),
            f"status={r.status} body={r.body!r}",
        )
        print(f"{YELLOW}  (skipping tuning-result assertions — ML runtime deps absent){RESET}")
        return

    r = client.query(f"RUN TUNING_JOB {TUNE_JOB}")
    t.check("RUN TUNING_JOB", r.ok(), f"status={r.status} body={r.body!r}")
    all_rows = _rows(r.body)
    # The result lists one row per trial plus a trailing "best -> vN" summary row.
    trial_rows = [row for row in all_rows if not str(row.get("trial", "")).startswith("best")]
    summary_rows = [row for row in all_rows if str(row.get("trial", "")).startswith("best")]
    t.check(
        f"tuning recorded {TRIALS} trials",
        len(trial_rows) == TRIALS,
        f"got {len(trial_rows)} body={r.body[:400]!r}",
    )
    t.check("tuning reports a best summary row", len(summary_rows) == 1, f"rows={all_rows}")
    t.check(
        "trials report params + objective",
        bool(trial_rows) and "params" in trial_rows[0] and "objective_value" in trial_rows[0],
        f"rows={trial_rows[:1]}",
    )

    # A best version must have been registered from the sweep.
    r = client.query(f"SHOW MODEL VERSIONS {MODEL}")
    t.check("SHOW MODEL VERSIONS", r.ok(), f"status={r.status}")
    t.check("best version registered (v1)", "v1" in r.body, f"body={r.body!r}")

    # The tuned best version is usable for inference.
    r = client.query(f"PREDICT MODEL {MODEL}:1 WITH (f1=16, f2=70, f3=6)")
    t.check("PREDICT with tuned model", r.ok(), f"status={r.status} body={r.body!r}")


def test_errors(client: Client, t: TestRunner) -> None:
    t.section("Error handling")
    r = client.query("RUN TUNING_JOB definitely_missing_tuning_job_xyz")
    t.check("RUN missing tuning job fails", not r.ok(), f"status={r.status}")


def cleanup(client: Client, t: TestRunner) -> None:
    t.section("Cleanup")
    for sql in [
        f"DROP TUNING_JOB IF EXISTS {TUNE_JOB}",
        f"DROP TRAINING_JOB IF EXISTS {TRAIN_JOB}",
        f"DROP MODEL IF EXISTS {MODEL}",
        f"DROP FEATURE_SET IF EXISTS {FS}",
        f"DROP TABLE IF EXISTS {SOURCE}",
    ]:
        client.query(sql)
    t.check("cleanup done", True)


def run_tests(client: Client) -> int:
    t = TestRunner()
    ml = _ml_runtime_available()
    if not ml:
        print(f"{YELLOW}NOTE: scikit-learn not importable — tuning execution "
              f"will be skipped (catalog CRUD still tested).{RESET}")
    setup(client, t)
    test_tuning_catalog(client, t)
    test_tuning_run(client, t, ml=ml)
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
        print(f"{YELLOW}Using existing server at {args.url}{RESET}")
        return run_tests(Client(args.url))

    with ServerProc(
        Path(args.server_bin) if args.server_bin else None,
        args.url,
    ) as client:
        return run_tests(client)


if __name__ == "__main__":
    sys.exit(main())
