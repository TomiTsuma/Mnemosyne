"""Hyperparameter optimization: OPTUNA when available, GRID/RANDOM fallbacks."""
from __future__ import annotations

import itertools
import random
from typing import Any, Dict, List, Tuple

from algorithms import make_estimator
from evaluation import is_minimized, objective_value
from trainers import fit_and_eval, load_frame, save_artifact


def _parse_space(spec_str: str):
    """Parse a search-space spec string.

    Supported forms:
      "10..200"        integer range (inclusive)
      "0.01..0.3"      float range
      "a,b,c"          categorical choices (ints/floats coerced)
    Returns a dict describing the dimension.
    """
    s = str(spec_str).strip()
    if ".." in s:
        lo_s, hi_s = s.split("..", 1)
        lo_s, hi_s = lo_s.strip(), hi_s.strip()
        if "." in lo_s or "." in hi_s or "e" in lo_s.lower():
            return {"kind": "float", "low": float(lo_s), "high": float(hi_s)}
        return {"kind": "int", "low": int(lo_s), "high": int(hi_s)}
    if "," in s:
        choices: List[Any] = []
        for tok in s.split(","):
            tok = tok.strip()
            try:
                choices.append(int(tok))
            except ValueError:
                try:
                    choices.append(float(tok))
                except ValueError:
                    choices.append(tok)
        return {"kind": "choice", "choices": choices}
    # single value
    return {"kind": "choice", "choices": [s]}


def _sample_random(space: Dict[str, dict], rng: random.Random) -> Dict[str, Any]:
    params: Dict[str, Any] = {}
    for name, dim in space.items():
        if dim["kind"] == "int":
            params[name] = rng.randint(dim["low"], dim["high"])
        elif dim["kind"] == "float":
            params[name] = rng.uniform(dim["low"], dim["high"])
        else:
            params[name] = rng.choice(dim["choices"])
    return params


def _grid(space: Dict[str, dict], trials: int) -> List[Dict[str, Any]]:
    axes = []
    names = []
    for name, dim in space.items():
        names.append(name)
        if dim["kind"] == "choice":
            axes.append(dim["choices"])
        elif dim["kind"] == "int":
            lo, hi = dim["low"], dim["high"]
            step = max(1, (hi - lo) // 4)
            axes.append(list(range(lo, hi + 1, step)))
        else:
            lo, hi = dim["low"], dim["high"]
            axes.append([lo + (hi - lo) * i / 4.0 for i in range(5)])
    combos = [dict(zip(names, vals)) for vals in itertools.product(*axes)]
    if trials and len(combos) > trials:
        combos = combos[:trials]
    return combos


def _evaluate_params(spec: Dict[str, Any], df, params: Dict[str, Any]) -> Tuple[Dict[str, float], float]:
    merged = dict(spec.get("hyperparameters") or {})
    merged.update({k: str(v) for k, v in params.items()})
    estimator = make_estimator(
        spec.get("framework"), spec.get("algorithm"),
        spec.get("model_type"), merged,
    )
    _, metrics, _ = fit_and_eval(spec, estimator, df)
    obj = objective_value(metrics, spec.get("objective"))
    return metrics, obj


def tune(spec: Dict[str, Any]) -> Dict[str, Any]:
    df = load_frame(spec.get("data_csv"))
    raw_space = spec.get("search_space") or {}
    if not raw_space:
        # No declared space: tune a couple of common knobs by default.
        raw_space = {"n_estimators": "50..300", "max_depth": "2,4,8,16"}
    space = {name: _parse_space(val) for name, val in raw_space.items()}
    strategy = (spec.get("strategy") or "OPTUNA").upper()
    trials = int(spec.get("trials") or 20)
    objective = spec.get("objective") or ""
    minimize = is_minimized(objective)

    records: List[Dict[str, Any]] = []
    best = None  # (objective_value, params, metrics)

    def consider(params, metrics, obj):
        nonlocal best
        signed = -obj if minimize else obj
        records.append({
            "params": {k: str(v) for k, v in params.items()},
            "metrics": metrics,
            "objective_value": obj,
        })
        if best is None or signed > best[0]:
            best = (signed, params, metrics)

    used_optuna = False
    if strategy == "OPTUNA":
        try:
            import optuna  # noqa: WPS433
            optuna.logging.set_verbosity(optuna.logging.WARNING)
            used_optuna = True

            def objective_fn(trial):
                params = {}
                for name, dim in space.items():
                    if dim["kind"] == "int":
                        params[name] = trial.suggest_int(name, dim["low"], dim["high"])
                    elif dim["kind"] == "float":
                        params[name] = trial.suggest_float(name, dim["low"], dim["high"])
                    else:
                        params[name] = trial.suggest_categorical(name, dim["choices"])
                metrics, obj = _evaluate_params(spec, df, params)
                consider(params, metrics, obj)
                return -obj if minimize else obj

            study = optuna.create_study(direction="maximize")
            study.optimize(objective_fn, n_trials=trials)
        except ImportError:
            used_optuna = False

    if not used_optuna:
        rng = random.Random(42)
        if strategy == "GRID":
            combos = _grid(space, trials)
        else:
            combos = [_sample_random(space, rng) for _ in range(trials)]
        for params in combos:
            metrics, obj = _evaluate_params(spec, df, params)
            consider(params, metrics, obj)

    if best is None:
        return {"ok": False, "error": "tuning produced no trials"}

    _, best_params, best_metrics = best
    # Refit best params on the full data and persist the artifact.
    merged = dict(spec.get("hyperparameters") or {})
    merged.update({k: str(v) for k, v in best_params.items()})
    estimator = make_estimator(
        spec.get("framework"), spec.get("algorithm"),
        spec.get("model_type"), merged,
    )
    _, final_metrics, payload = fit_and_eval(spec, estimator, df)
    artifact = save_artifact(payload, spec["artifact_out"])

    return {
        "ok": True,
        "metrics": final_metrics or best_metrics,
        "artifact_location": artifact,
        "best_params": {k: str(v) for k, v in best_params.items()},
        "trials": records,
    }
