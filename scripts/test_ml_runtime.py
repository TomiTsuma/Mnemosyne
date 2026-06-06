#!/usr/bin/env python3
"""Standalone tests for the Python ML runtime (ml_runtime/run.py).

Drives the spec.json / result.json protocol directly — no server, no C++.
Exercises train -> evaluate -> predict, and tune (GRID/RANDOM fallback works
without optuna). Generates a synthetic classification CSV on the fly.

Usage:
    python scripts/test_ml_runtime.py

Exit code is 0 if all tests pass (or the runtime deps are absent and the
suite skips cleanly), 1 otherwise.
"""
from __future__ import annotations

import csv
import json
import os
import random
import subprocess
import sys
import tempfile
from pathlib import Path

from _mnemo_client import TestRunner, GREEN, YELLOW, RESET

REPO_ROOT = Path(__file__).resolve().parent.parent
RUNTIME = REPO_ROOT / "ml_runtime" / "run.py"
PYTHON = os.environ.get("MNEMO_PYTHON", sys.executable)


def _deps_available() -> tuple[bool, str]:
    try:
        import numpy  # noqa: F401
        import pandas  # noqa: F401
        import sklearn  # noqa: F401
        import joblib  # noqa: F401
        return True, ""
    except ImportError as exc:  # noqa: BLE001
        return False, str(exc)


def _make_dataset(path: Path, n: int = 200) -> None:
    """Synthetic, linearly separable-ish binary classification data."""
    rng = random.Random(7)
    with path.open("w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["user_id", "age", "income", "visits", "label"])
        for i in range(n):
            label = 1 if rng.random() < 0.5 else 0
            # Features correlate with the label so the model can learn.
            age = rng.gauss(40 + 8 * label, 5)
            income = rng.gauss(50000 + 15000 * label, 8000)
            visits = rng.gauss(5 + 4 * label, 2)
            w.writerow([i, round(age, 2), round(income, 2), round(visits, 2), label])


def _run_spec(spec: dict, run_dir: Path) -> dict:
    spec_path = run_dir / "spec.json"
    result_path = run_dir / "result.json"
    spec["result_path"] = str(result_path)
    spec_path.write_text(json.dumps(spec, indent=2), encoding="utf-8")
    proc = subprocess.run(
        [PYTHON, str(RUNTIME), "--spec", str(spec_path)],
        capture_output=True, text=True, cwd=str(REPO_ROOT),
    )
    if not result_path.exists():
        return {"ok": False, "error": f"no result.json; stderr={proc.stderr[:400]}"}
    return json.loads(result_path.read_text(encoding="utf-8"))


def run_tests() -> int:
    t = TestRunner()
    ok, why = _deps_available()
    if not ok:
        print(f"{YELLOW}SKIP: ML runtime deps missing ({why}). "
              f"Install with: pip install -r ml_runtime/requirements.txt{RESET}")
        return 0

    features = ["age", "income", "visits"]
    target = "label"

    with tempfile.TemporaryDirectory(prefix="mnemo_ml_") as tmp:
        tmp_dir = Path(tmp)
        data_csv = tmp_dir / "data.csv"
        _make_dataset(data_csv)
        artifact = tmp_dir / "model.joblib"

        # ---- train ----
        t.section("train")
        train_dir = tmp_dir / "train"
        train_dir.mkdir()
        res = _run_spec({
            "action": "train",
            "run_id": "rt_train",
            "model": "rt_model",
            "framework": "SKLEARN",
            "algorithm": "RandomForestClassifier",
            "model_type": "CLASSIFICATION",
            "features": features,
            "target": target,
            "entity_key": "user_id",
            "hyperparameters": {"n_estimators": "50", "max_depth": "6"},
            "data_csv": str(data_csv),
            "artifact_out": str(artifact),
        }, train_dir)
        t.check("train ok", res.get("ok"), f"res={res}")
        t.check("train wrote artifact", artifact.exists(), f"res={res}")
        metrics = res.get("metrics") or {}
        t.check("train produced ACCURACY", "ACCURACY" in metrics, f"metrics={metrics}")
        t.check(
            "ACCURACY is reasonable",
            float(metrics.get("ACCURACY", 0)) >= 0.6,
            f"metrics={metrics}",
        )

        # ---- evaluate ----
        t.section("evaluate")
        eval_dir = tmp_dir / "eval"
        eval_dir.mkdir()
        res = _run_spec({
            "action": "evaluate",
            "run_id": "rt_eval",
            "model": "rt_model",
            "framework": "SKLEARN",
            "model_type": "CLASSIFICATION",
            "features": features,
            "target": target,
            "data_csv": str(data_csv),
            "artifact_in": str(artifact),
            "artifact_out": str(artifact),
        }, eval_dir)
        t.check("evaluate ok", res.get("ok"), f"res={res}")
        em = res.get("metrics") or {}
        t.check("evaluate produced metrics", bool(em), f"res={res}")

        # ---- predict (direct features) ----
        t.section("predict")
        pred_dir = tmp_dir / "pred"
        pred_dir.mkdir()
        res = _run_spec({
            "action": "predict",
            "run_id": "rt_pred",
            "model": "rt_model",
            "framework": "SKLEARN",
            "model_type": "CLASSIFICATION",
            "predict_mode": "features",
            "predict_features": {"age": "55", "income": "75000", "visits": "12"},
            "features": features,
            "artifact_in": str(artifact),
            "artifact_out": str(artifact),
        }, pred_dir)
        t.check("predict ok", res.get("ok"), f"res={res}")
        preds = res.get("predictions") or []
        t.check("predict returned a prediction", len(preds) >= 1, f"res={res}")

        # ---- tune (RANDOM strategy — no optuna needed) ----
        t.section("tune")
        tune_dir = tmp_dir / "tune"
        tune_dir.mkdir()
        tune_artifact = tmp_dir / "tuned.joblib"
        res = _run_spec({
            "action": "tune",
            "run_id": "rt_tune",
            "model": "rt_model",
            "framework": "SKLEARN",
            "algorithm": "RandomForestClassifier",
            "model_type": "CLASSIFICATION",
            "features": features,
            "target": target,
            "objective": "ACCURACY",
            "strategy": "RANDOM",
            "trials": 5,
            "search_space": {"n_estimators": "20..120", "max_depth": "2,4,8"},
            "data_csv": str(data_csv),
            "artifact_out": str(tune_artifact),
        }, tune_dir)
        t.check("tune ok", res.get("ok"), f"res={res}")
        t.check("tune found best_params", bool(res.get("best_params")), f"res={res}")
        trials = res.get("trials") or []
        t.check("tune recorded 5 trials", len(trials) == 5, f"trials={len(trials)}")
        t.check("tune wrote artifact", tune_artifact.exists(), f"res={res}")

    return t.summary()


if __name__ == "__main__":
    sys.exit(run_tests())
