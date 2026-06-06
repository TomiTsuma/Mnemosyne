"""Training + artifact persistence for the MODEL layer."""
from __future__ import annotations

from typing import Any, Dict, List, Optional, Tuple

import numpy as np
import pandas as pd

from algorithms import is_classification, make_estimator
from evaluation import compute_metrics


def load_frame(data_csv: str) -> pd.DataFrame:
    if not data_csv:
        raise ValueError("no data_csv provided")
    return pd.read_csv(data_csv)


def _prepare_xy(df: pd.DataFrame, features: List[str], target: str):
    cols = features if features else [c for c in df.columns if c != target]
    missing = [c for c in cols + ([target] if target else []) if c not in df.columns]
    if missing:
        raise ValueError(f"columns not found in data: {missing}")
    X = df[cols].copy()
    # One-hot encode any non-numeric feature columns.
    X = pd.get_dummies(X, dummy_na=False)
    feature_names = list(X.columns)
    y = df[target] if target else None
    return X, y, cols, feature_names


def fit_and_eval(
    spec: Dict[str, Any],
    estimator,
    df: pd.DataFrame,
) -> Tuple[Any, Dict[str, float], Dict[str, Any]]:
    """Fit an estimator and return (fitted, metrics, artifact_payload)."""
    from sklearn.model_selection import train_test_split

    features = spec.get("features") or []
    target = spec.get("target") or ""
    model_type = spec.get("model_type") or "CLASSIFICATION"

    X, y, raw_cols, feature_names = _prepare_xy(df, features, target)

    label_encoder = None
    if target and is_classification(model_type) and y is not None:
        if y.dtype == object or str(y.dtype).startswith("string"):
            from sklearn.preprocessing import LabelEncoder
            label_encoder = LabelEncoder()
            y = pd.Series(label_encoder.fit_transform(y.astype(str)), index=y.index)

    if y is None:
        # Unsupervised (clustering / embedding): fit on X only.
        estimator.fit(X)
        labels = getattr(estimator, "labels_", None)
        metrics: Dict[str, float] = {}
        if labels is not None:
            try:
                from sklearn.metrics import silhouette_score
                if len(np.unique(labels)) > 1:
                    metrics["SILHOUETTE"] = float(silhouette_score(X, labels))
            except Exception:  # noqa: BLE001
                pass
    else:
        stratify = None
        if is_classification(model_type):
            counts = y.value_counts()
            if counts.min() >= 2 and len(counts) >= 2:
                stratify = y
        test_size = 0.25 if len(X) >= 8 else 0.0
        if test_size > 0:
            X_tr, X_val, y_tr, y_val = train_test_split(
                X, y, test_size=test_size, random_state=42, stratify=stratify,
            )
        else:
            X_tr, X_val, y_tr, y_val = X, X, y, y
        estimator.fit(X_tr, y_tr)
        y_pred = estimator.predict(X_val)
        y_proba = None
        if hasattr(estimator, "predict_proba"):
            try:
                y_proba = np.asarray(estimator.predict_proba(X_val))
            except Exception:  # noqa: BLE001
                y_proba = None
        metrics = compute_metrics(model_type, y_val, y_pred, y_proba)

    payload = {
        "estimator": estimator,
        "features": features,
        "raw_feature_cols": raw_cols,
        "encoded_feature_names": feature_names,
        "target": target,
        "model_type": model_type,
        "entity_key": spec.get("entity_key") or "",
        "label_encoder": label_encoder,
    }
    return estimator, metrics, payload


def save_artifact(payload: Dict[str, Any], path: str) -> str:
    import joblib
    joblib.dump(payload, path)
    return path


def load_artifact(path: str) -> Dict[str, Any]:
    import joblib
    return joblib.load(path)


def train(spec: Dict[str, Any]) -> Dict[str, Any]:
    df = load_frame(spec.get("data_csv"))
    estimator = make_estimator(
        spec.get("framework"), spec.get("algorithm"),
        spec.get("model_type"), spec.get("hyperparameters") or {},
    )
    _, metrics, payload = fit_and_eval(spec, estimator, df)
    artifact = save_artifact(payload, spec["artifact_out"])
    return {
        "ok": True,
        "metrics": metrics,
        "artifact_location": artifact,
    }
