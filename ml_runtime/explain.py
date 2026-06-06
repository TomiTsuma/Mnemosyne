"""Explainability: SHAP when available, else model feature importances."""
from __future__ import annotations

from typing import Any, Dict, List

import numpy as np
import pandas as pd

from trainers import load_artifact, load_frame


def _feature_importances(payload: Dict[str, Any]) -> List[Dict[str, Any]]:
    estimator = payload["estimator"]
    names = payload.get("encoded_feature_names") or []
    importances = getattr(estimator, "feature_importances_", None)
    if importances is None:
        coef = getattr(estimator, "coef_", None)
        if coef is not None:
            importances = np.abs(np.asarray(coef)).ravel()
    if importances is None:
        return []
    pairs = sorted(zip(names, importances), key=lambda kv: float(kv[1]), reverse=True)
    return [{"feature": n, "contribution": float(v)} for n, v in pairs]


def explain(spec: Dict[str, Any]) -> Dict[str, Any]:
    payload = load_artifact(spec["artifact_in"])
    raw_cols = payload.get("raw_feature_cols") or payload.get("features") or []
    entity_key = payload.get("entity_key") or spec.get("entity_key") or ""

    contributions: List[Dict[str, Any]] = []
    confidence = 0.0

    # Try SHAP on a single row if we have a data context.
    feats = spec.get("predict_features") or {}
    df_raw = None
    if feats and entity_key in feats and spec.get("data_csv"):
        df = load_frame(spec.get("data_csv"))
        if entity_key in df.columns:
            match = df[df[entity_key].astype(str) == str(feats[entity_key])]
            if not match.empty:
                df_raw = match[raw_cols].head(1)

    if df_raw is not None:
        try:
            import shap  # noqa: WPS433
            from prediction import _encode  # reuse encoding
            X = _encode(df_raw, payload)
            explainer = shap.Explainer(payload["estimator"])
            sv = explainer(X)
            vals = np.asarray(sv.values)
            row = vals[0]
            if row.ndim > 1:
                row = row[:, -1]
            names = payload.get("encoded_feature_names") or list(X.columns)
            pairs = sorted(zip(names, row), key=lambda kv: abs(float(kv[1])), reverse=True)
            contributions = [{"feature": n, "contribution": float(v)} for n, v in pairs]
        except Exception:  # noqa: BLE001
            contributions = _feature_importances(payload)
    else:
        contributions = _feature_importances(payload)

    return {
        "ok": True,
        "explanation": {
            "top_features": [c["feature"] for c in contributions[:10]],
            "contributions": contributions[:20],
            "confidence": confidence,
        },
    }
