#!/usr/bin/env python3
"""Mnemosyne ML runtime dispatcher.

The C++ MODEL layer writes a spec.json describing an action, spawns this script,
and reads the result.json it writes. Protocol:

    spec.json  -> { action, run_id, model, framework, algorithm, features,
                    target, hyperparameters, data_csv, artifact_out, ... }
    result.json <- { ok, error?, metrics?, artifact_location?, best_params?,
                     trials?, predictions?, generated_text?, explanation? }

This keeps all real ML logic (sklearn / xgboost / optuna / transformers) inside
the Python ecosystem (MODEL_LAYER PRD Principle 4: Framework Independence).
"""
from __future__ import annotations

import argparse
import json
import os
import sys
import traceback
from typing import Any, Dict

# Allow importing sibling modules regardless of CWD.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


def _write_result(spec: Dict[str, Any], result: Dict[str, Any]) -> None:
    path = spec.get("result_path")
    if not path:
        run_dir = os.path.dirname(spec.get("artifact_out", "")) or "."
        path = os.path.join(run_dir, "result.json")
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(result, fh, indent=2, default=str)


def dispatch(spec: Dict[str, Any]) -> Dict[str, Any]:
    action = (spec.get("action") or "").lower()

    if action == "train":
        from trainers import train
        return train(spec)
    if action == "tune":
        from tuning import tune
        return tune(spec)
    if action == "evaluate":
        from prediction import evaluate
        return evaluate(spec)
    if action == "predict":
        from prediction import predict
        return predict(spec)
    if action == "generate":
        from llm import generate
        return generate(spec)
    if action == "explain":
        from explain import explain
        return explain(spec)

    return {"ok": False, "error": f"unknown action: {action}"}


def main() -> int:
    parser = argparse.ArgumentParser(description="Mnemosyne ML runtime")
    parser.add_argument("--spec", required=True, help="path to spec.json")
    args = parser.parse_args()

    try:
        with open(args.spec, "r", encoding="utf-8") as fh:
            spec = json.load(fh)
    except Exception as exc:  # noqa: BLE001
        sys.stderr.write(f"failed to read spec: {exc}\n")
        return 2

    try:
        result = dispatch(spec)
    except ModuleNotFoundError as exc:
        result = {
            "ok": False,
            "error": (f"required ML library missing: {exc.name}. Install runtime deps: "
                      "pip install -r ml_runtime/requirements.txt"),
        }
    except Exception as exc:  # noqa: BLE001
        result = {
            "ok": False,
            "error": f"{type(exc).__name__}: {exc}",
            "traceback": traceback.format_exc(),
        }

    try:
        _write_result(spec, result)
    except Exception as exc:  # noqa: BLE001
        sys.stderr.write(f"failed to write result: {exc}\n")
        return 3

    return 0 if result.get("ok") else 1


if __name__ == "__main__":
    sys.exit(main())
