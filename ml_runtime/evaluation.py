"""Metric computation for classification, regression, and recommendation tasks."""
from __future__ import annotations

from typing import Any, Dict

import numpy as np


def classification_metrics(y_true, y_pred, y_proba=None) -> Dict[str, float]:
    from sklearn.metrics import (
        accuracy_score, precision_score, recall_score, f1_score,
        roc_auc_score, average_precision_score,
    )
    metrics: Dict[str, float] = {}
    metrics["ACCURACY"] = float(accuracy_score(y_true, y_pred))
    metrics["PRECISION"] = float(precision_score(y_true, y_pred, average="weighted", zero_division=0))
    metrics["RECALL"] = float(recall_score(y_true, y_pred, average="weighted", zero_division=0))
    metrics["F1"] = float(f1_score(y_true, y_pred, average="weighted", zero_division=0))

    classes = np.unique(y_true)
    if y_proba is not None and len(classes) == 2:
        try:
            pos = y_proba[:, 1] if y_proba.ndim == 2 else y_proba
            metrics["ROC_AUC"] = float(roc_auc_score(y_true, pos))
            metrics["PR_AUC"] = float(average_precision_score(y_true, pos))
        except Exception:  # noqa: BLE001
            pass
    elif y_proba is not None and len(classes) > 2:
        try:
            metrics["ROC_AUC"] = float(roc_auc_score(y_true, y_proba, multi_class="ovr"))
        except Exception:  # noqa: BLE001
            pass
    return metrics


def regression_metrics(y_true, y_pred) -> Dict[str, float]:
    from sklearn.metrics import (
        mean_absolute_error, mean_squared_error, r2_score,
    )
    y_true = np.asarray(y_true, dtype=float)
    y_pred = np.asarray(y_pred, dtype=float)
    mse = float(mean_squared_error(y_true, y_pred))
    metrics = {
        "MAE": float(mean_absolute_error(y_true, y_pred)),
        "MSE": mse,
        "RMSE": float(np.sqrt(mse)),
        "R2": float(r2_score(y_true, y_pred)),
    }
    denom = np.where(np.abs(y_true) < 1e-9, np.nan, y_true)
    mape = np.nanmean(np.abs((y_true - y_pred) / denom)) * 100.0
    if np.isfinite(mape):
        metrics["MAPE"] = float(mape)
    return metrics


def compute_metrics(model_type: str, y_true, y_pred, y_proba=None) -> Dict[str, float]:
    from algorithms import is_classification
    if is_classification(model_type):
        return classification_metrics(y_true, y_pred, y_proba)
    return regression_metrics(y_true, y_pred)


def objective_value(metrics: Dict[str, float], objective: str) -> float:
    """Return the value of the requested objective metric (defaulting sensibly)."""
    obj = (objective or "").upper()
    if obj and obj in metrics:
        return metrics[obj]
    for default in ("ROC_AUC", "F1", "ACCURACY", "R2"):
        if default in metrics:
            return metrics[default]
    # Regression error metrics: lower is better, but objective_value is maximized,
    # so callers should negate. Here we just surface the first available.
    for k in ("RMSE", "MAE", "MSE", "MAPE"):
        if k in metrics:
            return metrics[k]
    return 0.0


def is_minimized(objective: str) -> bool:
    return (objective or "").upper() in ("MAE", "MSE", "RMSE", "MAPE", "LOSS", "PERPLEXITY")
