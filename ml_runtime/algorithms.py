"""Estimator factory: maps (framework, algorithm, model_type) -> estimator.

Training logic lives inside the underlying libraries (MODEL_LAYER PRD Principle 4);
this module only constructs the right estimator with the requested hyperparameters.
"""
from __future__ import annotations

from typing import Any, Dict


def _coerce(value: str) -> Any:
    """Coerce a hyperparameter string into int/float/bool/None where sensible."""
    if isinstance(value, (int, float, bool)):
        return value
    s = str(value).strip()
    low = s.lower()
    if low in ("true", "false"):
        return low == "true"
    if low in ("none", "null"):
        return None
    try:
        return int(s)
    except ValueError:
        pass
    try:
        return float(s)
    except ValueError:
        pass
    return s


def coerce_params(params: Dict[str, Any]) -> Dict[str, Any]:
    return {k: _coerce(v) for k, v in (params or {}).items()}


def is_classification(model_type: str) -> bool:
    return (model_type or "").upper() in ("CLASSIFICATION", "RECOMMENDATION")


def is_regression(model_type: str) -> bool:
    return (model_type or "").upper() in ("REGRESSION", "FORECASTING")


def make_estimator(framework: str, algorithm: str, model_type: str,
                   hyperparameters: Dict[str, Any]):
    """Construct an unfitted estimator.

    Falls back to sensible defaults when the algorithm is omitted.
    """
    fw = (framework or "SKLEARN").upper()
    algo = (algorithm or "").strip()
    params = coerce_params(hyperparameters)
    clf = is_classification(model_type)

    if fw in ("XGBOOST",):
        import xgboost as xgb  # noqa: WPS433
        if clf:
            params.setdefault("use_label_encoder", False)
            params.setdefault("eval_metric", "logloss")
            return xgb.XGBClassifier(**params)
        return xgb.XGBRegressor(**params)

    if fw in ("LIGHTGBM",):
        import lightgbm as lgb  # noqa: WPS433
        return lgb.LGBMClassifier(**params) if clf else lgb.LGBMRegressor(**params)

    if fw in ("CATBOOST",):
        from catboost import CatBoostClassifier, CatBoostRegressor  # noqa: WPS433
        params.setdefault("verbose", False)
        return CatBoostClassifier(**params) if clf else CatBoostRegressor(**params)

    # Default: scikit-learn.
    from sklearn.ensemble import (  # noqa: WPS433
        RandomForestClassifier, RandomForestRegressor,
        GradientBoostingClassifier, GradientBoostingRegressor,
    )
    from sklearn.linear_model import (  # noqa: WPS433
        LogisticRegression, LinearRegression, Ridge,
    )
    from sklearn.tree import DecisionTreeClassifier, DecisionTreeRegressor  # noqa: WPS433
    from sklearn.svm import SVC, SVR  # noqa: WPS433
    from sklearn.cluster import KMeans  # noqa: WPS433

    table = {
        "RandomForestClassifier": RandomForestClassifier,
        "RandomForestRegressor": RandomForestRegressor,
        "GradientBoostingClassifier": GradientBoostingClassifier,
        "GradientBoostingRegressor": GradientBoostingRegressor,
        "LogisticRegression": LogisticRegression,
        "LinearRegression": LinearRegression,
        "Ridge": Ridge,
        "DecisionTreeClassifier": DecisionTreeClassifier,
        "DecisionTreeRegressor": DecisionTreeRegressor,
        "SVC": SVC,
        "SVR": SVR,
        "KMeans": KMeans,
    }

    if algo in table:
        cls = table[algo]
        if cls is SVC:
            params.setdefault("probability", True)
        return cls(**params)

    # Unknown / unspecified algorithm: pick a robust default per task.
    if (model_type or "").upper() == "CLUSTERING":
        return KMeans(**params)
    if clf:
        return RandomForestClassifier(**params)
    return RandomForestRegressor(**params)
