#!/usr/bin/env python3
"""End-to-end API tests for the MODEL layer (PRD MODEL_LAYER_ §24 + §25).

Drives the full lifecycle over HTTP:
    seed table -> CREATE FEATURE_SET -> CREATE MODEL -> CREATE TRAINING_JOB
    -> RUN TRAINING_JOB -> SHOW MODEL VERSIONS -> EVALUATE -> DEPLOY
    -> PREDICT (WITH / FOR / FROM) -> COMPARE -> SHOW MODEL ENDPOINTS / METRICS.

The training/evaluation/prediction steps spawn the Python ML runtime, which
requires scikit-learn (+ pandas/numpy/joblib). If those libraries are not
importable the ML-execution sections are skipped (catalog CRUD still runs).

Usage:
    python scripts/test_models_api.py
    python scripts/test_models_api.py --no-launch

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

DB = "model_test_db"
SOURCE = "ml_customers"
FS = "churn_features"
MODEL = "churn_model"
MODEL2 = "churn_model_b"
TRAIN_JOB = "churn_training"
TRAIN_JOB2 = "churn_training_b"
ENDPOINT = "churn_endpoint"


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
    probe = "import sklearn, pandas, numpy, joblib"
    try:
        return subprocess.run([py, "-c", probe], capture_output=True).returncode == 0
    except OSError:
        return False


def _seed_values(n: int = 60) -> str:
    """Deterministic, learnable churn data (high charges + low tenure -> churn)."""
    import random
    rng = random.Random(13)
    parts = []
    for i in range(1, n + 1):
        churn = 1 if rng.random() < 0.5 else 0
        tenure = int(max(1, rng.gauss(24 - 14 * churn, 4)))
        charges = round(rng.gauss(55 + 35 * churn, 8), 2)
        support = int(max(0, rng.gauss(1 + 3 * churn, 1)))
        parts.append(f"({i}, {tenure}, {charges}, {support}, {churn})")
    return ", ".join(parts)


def setup(client: Client, t: TestRunner) -> None:
    t.section("Setup")
    for label, sql in [
        ("CREATE DATABASE", f"CREATE DATABASE IF NOT EXISTS {DB}"),
        ("USE", f"USE {DB}"),
        ("DROP TRAINING_JOB", f"DROP TRAINING_JOB IF EXISTS {TRAIN_JOB}"),
        ("DROP TRAINING_JOB b", f"DROP TRAINING_JOB IF EXISTS {TRAIN_JOB2}"),
        ("DROP MODEL", f"DROP MODEL IF EXISTS {MODEL}"),
        ("DROP MODEL b", f"DROP MODEL IF EXISTS {MODEL2}"),
        ("DROP FEATURE_SET", f"DROP FEATURE_SET IF EXISTS {FS}"),
        ("DROP TABLE", f"DROP TABLE IF EXISTS {SOURCE}"),
    ]:
        r = client.query(sql)
        t.check(f"setup: {label}", r.ok(), f"status={r.status} body={r.body[:300]!r}")

    r = client.query(
        f"CREATE TABLE {SOURCE} "
        f"(customer_id Int64, tenure Int64, monthly_charges Float64, "
        f"support_calls Int64, churn Int64) ENGINE=Memory"
    )
    t.check("CREATE source table", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"INSERT INTO {SOURCE} VALUES {_seed_values()}")
    t.check("INSERT seed rows", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(
        f"CREATE FEATURE_SET {FS} FROM {SOURCE} "
        f"ENTITY_KEY(customer_id) "
        f"FEATURES(tenure, monthly_charges, support_calls) TARGET churn"
    )
    t.check("CREATE FEATURE_SET", r.ok(), f"status={r.status} body={r.body!r}")


def test_catalog(client: Client, t: TestRunner) -> None:
    t.section("Model catalog CRUD")

    r = client.query(f"CREATE MODEL {MODEL} TYPE CLASSIFICATION")
    t.check("CREATE MODEL", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("SHOW MODELS")
    t.check("SHOW MODELS", r.ok(), f"status={r.status}")
    t.check("lists model", MODEL in r.body, f"body={r.body!r}")

    r = client.query(
        f"CREATE TRAINING_JOB {TRAIN_JOB} MODEL {MODEL} FEATURE_SET {FS} "
        f"FRAMEWORK SKLEARN ALGORITHM RandomForestClassifier OBJECTIVE ACCURACY "
        f"HYPERPARAMS(n_estimators=60, max_depth=6)"
    )
    t.check("CREATE TRAINING_JOB", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query("SHOW TRAINING_JOBS")
    t.check("SHOW TRAINING_JOBS", r.ok(), f"status={r.status}")
    t.check("lists training job", TRAIN_JOB in r.body, f"body={r.body!r}")
    rows = _rows(r.body)
    match = next((row for row in rows if row.get("name") == TRAIN_JOB), None)
    if match:
        t.check("job references model", match.get("model") == MODEL, f"row={match}")
        t.check("job references feature_set", match.get("feature_set") == FS, f"row={match}")
        t.check("job framework SKLEARN", "SKLEARN" in str(match.get("framework")), f"row={match}")

    r = client.query(f"DESCRIBE MODEL {MODEL}")
    t.check("DESCRIBE MODEL", r.ok(), f"status={r.status}")
    t.check("describe mentions CLASSIFICATION", "classification" in r.body.lower(), f"body={r.body!r}")


def test_training_and_inference(client: Client, t: TestRunner, *, ml: bool) -> None:
    t.section("Training + inference")
    if not ml:
        # Catalog is in place; verify RUN surfaces the missing-deps error cleanly.
        r = client.query(f"RUN TRAINING_JOB {TRAIN_JOB}")
        t.check(
            "RUN TRAINING_JOB reports missing ML deps",
            not r.ok() and ("library" in r.body.lower() or "missing" in r.body.lower()),
            f"status={r.status} body={r.body!r}",
        )
        print(f"{YELLOW}  (skipping inference assertions — ML runtime deps absent){RESET}")
        return

    r = client.query(f"RUN TRAINING_JOB {TRAIN_JOB}")
    t.check("RUN TRAINING_JOB", r.ok(), f"status={r.status} body={r.body!r}")
    t.check("training SUCCEEDED", "succeeded" in r.body.lower(), f"body={r.body!r}")
    t.check("training produced v1", "v1" in r.body, f"body={r.body!r}")
    t.check("training reports ACCURACY", "ACCURACY" in r.body, f"body={r.body!r}")

    # PRD §12 — model version registered after a successful run.
    r = client.query(f"SHOW MODEL VERSIONS {MODEL}")
    t.check("SHOW MODEL VERSIONS", r.ok(), f"status={r.status}")
    t.check("version v1 listed", "v1" in r.body, f"body={r.body!r}")
    vrows = _rows(r.body)
    t.check("exactly one version", len(vrows) == 1, f"rows={vrows}")
    t.check(
        "version has artifact",
        bool(vrows and vrows[0].get("artifact")),
        f"rows={vrows}",
    )

    # PRD §16 — EVALUATE.
    r = client.query(f"EVALUATE MODEL {MODEL}:1")
    t.check("EVALUATE MODEL", r.ok(), f"status={r.status} body={r.body!r}")
    t.check("evaluate returns metrics", "ACCURACY" in r.body or "metric" in r.body.lower(),
            f"body={r.body!r}")

    # PRD §17 — DEPLOY.
    r = client.query(f"DEPLOY MODEL {MODEL}:1 AS {ENDPOINT}")
    t.check("DEPLOY MODEL", r.ok(), f"status={r.status} body={r.body!r}")
    t.check("endpoint named", ENDPOINT in r.body, f"body={r.body!r}")

    # PRD §18 — PREDICT WITH (direct features).
    r = client.query(
        f"PREDICT MODEL {MODEL}:1 WITH "
        f"(tenure=2, monthly_charges=95.0, support_calls=5)"
    )
    t.check("PREDICT WITH features", r.ok(), f"status={r.status} body={r.body!r}")
    prows = _rows(r.body)
    t.check("WITH returns a prediction", len(prows) == 1, f"rows={prows}")
    t.check(
        "prediction has confidence",
        bool(prows and prows[0].get("confidence")),
        f"rows={prows}",
    )

    # PRD §18 — PREDICT FOR (entity lookup against the training feature set).
    r = client.query(f"PREDICT MODEL {MODEL}:1 FOR (customer_id=2)")
    t.check("PREDICT FOR entity", r.ok(), f"status={r.status} body={r.body!r}")
    erows = _rows(r.body)
    t.check(
        "entity prediction returned",
        bool(erows) and str(erows[0].get("entity_key")) == "2",
        f"rows={erows}",
    )

    # PRD §18/§19 — PREDICT FROM (batch) materializes a PREDICTION_TABLE.
    r = client.query(f"PREDICT MODEL {MODEL}:1 FROM {SOURCE}")
    t.check("PREDICT FROM batch", r.ok(), f"status={r.status} body={r.body!r}")
    brows = _rows(r.body)
    t.check("batch produced many predictions", len(brows) >= 50, f"n={len(brows)}")

    pred_table = f"{MODEL}_predictions"
    r = client.query(f"SELECT COUNT(*) AS n FROM {pred_table}")
    t.check("PREDICTION_TABLE materialized", r.ok(), f"status={r.status} body={r.body!r}")
    cnt_rows = _rows(r.body)
    t.check(
        "prediction table populated",
        bool(cnt_rows) and int(next(iter(cnt_rows[0].values()))) >= 50,
        f"rows={cnt_rows}",
    )

    # PRD §21 — endpoint monitoring counters.
    r = client.query(f"SHOW MODEL ENDPOINTS {MODEL}")
    t.check("SHOW MODEL ENDPOINTS", r.ok(), f"status={r.status}")
    t.check("endpoint listed", ENDPOINT in r.body, f"body={r.body!r}")

    r = client.query(f"SHOW MODEL METRICS {MODEL}")
    t.check("SHOW MODEL METRICS", r.ok(), f"status={r.status}")
    t.check("metrics include prediction_count", "prediction_count" in r.body.lower(),
            f"body={r.body!r}")


def test_compare(client: Client, t: TestRunner, *, ml: bool) -> None:
    if not ml:
        return
    t.section("Compare models (PRD §22)")

    client.query(f"CREATE MODEL {MODEL2} TYPE CLASSIFICATION")
    client.query(
        f"CREATE TRAINING_JOB {TRAIN_JOB2} MODEL {MODEL2} FEATURE_SET {FS} "
        f"FRAMEWORK SKLEARN ALGORITHM LogisticRegression OBJECTIVE ACCURACY"
    )
    r = client.query(f"RUN TRAINING_JOB {TRAIN_JOB2}")
    t.check("RUN second training job", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"COMPARE MODELS {MODEL}:1, {MODEL2}:1")
    t.check("COMPARE MODELS", r.ok(), f"status={r.status} body={r.body!r}")
    t.check("comparison mentions both models",
            MODEL in r.body and MODEL2 in r.body, f"body={r.body!r}")


def test_errors(client: Client, t: TestRunner) -> None:
    t.section("Error handling")

    r = client.query("RUN TRAINING_JOB definitely_missing_job_xyz")
    t.check("RUN missing training job fails", not r.ok(), f"status={r.status}")

    r = client.query("PREDICT MODEL definitely_missing_model_xyz WITH (a=1)")
    t.check("PREDICT missing model fails", not r.ok(), f"status={r.status}")

    r = client.query(f"CREATE MODEL {MODEL} TYPE CLASSIFICATION")
    t.check("duplicate CREATE MODEL fails", not r.ok(), f"status={r.status}")


def cleanup(client: Client, t: TestRunner) -> None:
    t.section("Cleanup")
    for sql in [
        f"DROP TRAINING_JOB IF EXISTS {TRAIN_JOB}",
        f"DROP TRAINING_JOB IF EXISTS {TRAIN_JOB2}",
        f"DROP MODEL IF EXISTS {MODEL}",
        f"DROP MODEL IF EXISTS {MODEL2}",
        f"DROP FEATURE_SET IF EXISTS {FS}",
        f"DROP TABLE IF EXISTS {SOURCE}",
    ]:
        client.query(sql)
    t.check("cleanup done", True)


def run_tests(client: Client) -> int:
    t = TestRunner()
    ml = _ml_runtime_available()
    if not ml:
        print(f"{YELLOW}NOTE: scikit-learn not importable — ML execution steps "
              f"will be skipped (catalog CRUD still tested).{RESET}")
    setup(client, t)
    test_catalog(client, t)
    test_training_and_inference(client, t, ml=ml)
    test_compare(client, t, ml=ml)
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
