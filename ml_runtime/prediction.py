"""Inference + evaluation against a persisted artifact."""
from __future__ import annotations

from typing import Any, Dict, List

import numpy as np
import pandas as pd

from algorithms import is_classification
from evaluation import compute_metrics
from trainers import load_artifact, load_frame


def _encode(df_raw: pd.DataFrame, payload: Dict[str, Any]) -> pd.DataFrame:
    encoded_names = payload.get("encoded_feature_names") or list(df_raw.columns)
    X = pd.get_dummies(df_raw, dummy_na=False)
    X = X.reindex(columns=encoded_names, fill_value=0)
    return X


def _decode_label(value, payload: Dict[str, Any]) -> str:
    le = payload.get("label_encoder")
    if le is not None:
        try:
            return str(le.inverse_transform([int(round(float(value)))])[0])
        except Exception:  # noqa: BLE001
            return str(value)
    return str(value)


def _predict_rows(payload: Dict[str, Any], df_raw: pd.DataFrame) -> List[Dict[str, Any]]:
    estimator = payload["estimator"]
    model_type = payload.get("model_type") or "CLASSIFICATION"
    X = _encode(df_raw, payload)
    preds = estimator.predict(X)
    proba = None
    if hasattr(estimator, "predict_proba"):
        try:
            proba = np.asarray(estimator.predict_proba(X))
        except Exception:  # noqa: BLE001
            proba = None

    out: List[Dict[str, Any]] = []
    for i in range(len(preds)):
        conf = 0.0
        if proba is not None:
            conf = float(np.max(proba[i]))
        if is_classification(model_type):
            label = _decode_label(preds[i], payload)
        else:
            label = f"{float(preds[i]):.6g}"
            conf = 1.0
        out.append({"prediction": label, "confidence": conf})
    return out


def predict(spec: Dict[str, Any]) -> Dict[str, Any]:
    payload = load_artifact(spec["artifact_in"])
    raw_cols = payload.get("raw_feature_cols") or payload.get("features") or []
    entity_key = payload.get("entity_key") or spec.get("entity_key") or ""
    mode = (spec.get("predict_mode") or "features").lower()

    if mode == "features":
        feats = spec.get("predict_features") or {}
        row = {c: feats.get(c, 0) for c in raw_cols}
        df_raw = pd.DataFrame([row])
        # numeric coercion — keep original values where conversion fails
        # (pandas 3.0 removed errors="ignore", so coerce + fall back per column)
        for c in df_raw.columns:
            converted = pd.to_numeric(df_raw[c], errors="coerce")
            if not converted.isna().any():
                df_raw[c] = converted
        res = _predict_rows(payload, df_raw)[0]
        return {
            "ok": True,
            "predictions": [{
                "entity_key": "",
                "prediction": res["prediction"],
                "confidence": res["confidence"],
            }],
        }

    df = load_frame(spec.get("data_csv"))

    if mode == "entity":
        feats = spec.get("predict_features") or {}
        if entity_key and entity_key in feats and entity_key in df.columns:
            target_val = str(feats[entity_key])
            match = df[df[entity_key].astype(str) == target_val]
            if match.empty:
                return {"ok": False, "error": f"entity {entity_key}={target_val} not found"}
            df_raw = match[raw_cols].head(1)
            res = _predict_rows(payload, df_raw)[0]
            return {
                "ok": True,
                "predictions": [{
                    "entity_key": target_val,
                    "prediction": res["prediction"],
                    "confidence": res["confidence"],
                }],
            }
        return {"ok": False, "error": "entity prediction requires an entity key value"}

    # batch
    df_raw = df[raw_cols] if all(c in df.columns for c in raw_cols) else df
    results = _predict_rows(payload, df_raw)
    keys = df[entity_key].astype(str).tolist() if entity_key in df.columns else [""] * len(results)
    preds = []
    for i, res in enumerate(results):
        preds.append({
            "entity_key": keys[i] if i < len(keys) else "",
            "prediction": res["prediction"],
            "confidence": res["confidence"],
        })
    return {"ok": True, "predictions": preds}


def evaluate(spec: Dict[str, Any]) -> Dict[str, Any]:
    payload = load_artifact(spec["artifact_in"])
    df = load_frame(spec.get("data_csv"))
    target = payload.get("target") or spec.get("target") or ""
    model_type = payload.get("model_type") or "CLASSIFICATION"
    raw_cols = payload.get("raw_feature_cols") or payload.get("features") or []
    if target and target not in df.columns:
        return {"ok": False, "error": f"target column '{target}' not in evaluation data"}

    df_raw = df[raw_cols] if all(c in df.columns for c in raw_cols) else df
    estimator = payload["estimator"]
    X = _encode(df_raw, payload)
    y_pred = estimator.predict(X)

    y_true = df[target] if target else None
    le = payload.get("label_encoder")
    if y_true is not None and le is not None:
        try:
            y_true = le.transform(y_true.astype(str))
        except Exception:  # noqa: BLE001
            pass
    y_proba = None
    if hasattr(estimator, "predict_proba"):
        try:
            y_proba = np.asarray(estimator.predict_proba(X))
        except Exception:  # noqa: BLE001
            y_proba = None

    if y_true is None:
        return {"ok": True, "metrics": {}}
    metrics = compute_metrics(model_type, y_true, y_pred, y_proba)
    return {"ok": True, "metrics": metrics}
