# Mnemosyne MODEL Layer PRD

## Product Requirements Document

**Product:** Mnemosyne
**Component:** MODEL Layer
**Version:** 1.0
**Status:** Implemented (sklearn/XGBoost runtime; FEATURE_SET full, DATASET minimal)

---

# 1. Overview

The MODEL Layer is Mnemosyne's native machine learning and AI lifecycle management subsystem.

The purpose of the MODEL Layer is to allow Mnemosyne to manage intelligence assets with the same rigor and governance used for data assets.

Models become first-class entities that participate in:

* Data ingestion
* Feature engineering
* Training
* Hyperparameter optimization
* Experiment tracking
* Model registry
* Deployment
* Inference
* Monitoring
* Explainability

The MODEL Layer is framework-agnostic and supports traditional machine learning, deep learning, reinforcement learning, recommendation systems, forecasting, and large language models.

---

# 2. Design Principles

## Principle 1: MODEL ≠ ALGORITHM

A model represents a business intelligence asset.

An algorithm represents one implementation of that model.

Example:

Business Asset:

```text
Customer Churn Prediction
```

Possible Implementations:

```text
LogisticRegression
RandomForestClassifier
XGBClassifier
Neural Network
```

Mnemosyne must separate these concepts.

---

## Principle 2: No Implicit Feature Discovery

Mnemosyne must never infer:

* Features
* Targets
* Entity Keys
* Time Keys

These must be explicitly declared.

---

## Principle 3: Complete Reproducibility

Every prediction must be traceable to:

* Model Version
* Model Run
* Training Job
* Feature Set Version
* Dataset Version
* Hyperparameters
* Training Code

---

## Principle 4: Framework Independence

Mnemosyne orchestrates training.

Mnemosyne does not replace:

* PyTorch
* TensorFlow
* Scikit-Learn
* XGBoost
* LightGBM
* CatBoost

Training logic remains inside those frameworks.

---

# 3. Architecture

```text
DATASET
FEATURE_SET
      ↓
TRAINING_JOB
      ↓
MODEL_RUN
      ↓
MODEL_VERSION
      ↓
MODEL_REGISTRY
      ↓
MODEL_ENDPOINT
      ↓
PREDICTIONS
```

---

# 4. First-Class Entities

## Core Entities

```text
MODEL
MODEL_VERSION
MODEL_RUN
TRAINING_JOB
TUNING_JOB
MODEL_TEMPLATE
MODEL_PACKAGE
MODEL_ENDPOINT
PREDICTION_TABLE
```

## Dependencies

```text
FEATURE_SET
DATASET
PIPELINE
STORAGE_UNIT
CLUSTER
NODE
```

---

# 5. MODEL

## Purpose

Represents a logical business intelligence asset.

Examples:

```sql
CREATE MODEL customer_churn
TYPE CLASSIFICATION;

CREATE MODEL revenue_forecast
TYPE FORECASTING;

CREATE MODEL product_recommendation
TYPE RECOMMENDATION;

CREATE MODEL Mnemosyne_llm
TYPE LLM;
```

---

## Metadata

| Field       | Description       |
| ----------- | ----------------- |
| model_id    | Unique identifier |
| name        | Model name        |
| type        | Model type        |
| description | Description       |
| owner       | Owner             |
| status      | Lifecycle status  |
| created_at  | Creation time     |
| updated_at  | Last update       |

---

## Supported Types

```text
CLASSIFICATION
REGRESSION
FORECASTING
RECOMMENDATION
CLUSTERING
EMBEDDING
LLM
RL
CUSTOM
```

---

# 6. FEATURE_SET Integration

A MODEL may consume exactly one FEATURE_SET version during training.

Example:

```sql
CREATE FEATURE_SET customer_features

ENTITY_KEY(customer_id)

FEATURES (
    purchase_frequency,
    avg_order_value,
    days_since_last_purchase
)

TARGET churn;
```

Mnemosyne stores:

```text
Entity Key:
customer_id

Features:
purchase_frequency
avg_order_value
days_since_last_purchase

Target:
churn
```

No inference is allowed.

---

# 7. MODEL_TEMPLATE

## Purpose

Reusable algorithm definitions.

Example:

```sql
CREATE MODEL_TEMPLATE random_forest_classifier

FRAMEWORK SKLEARN

ALGORITHM RandomForestClassifier;
```

Templates simplify model creation and AutoML.

---

# 8. TRAINING_JOB

## Purpose

Defines how training is executed.

---

## Sklearn Example

```sql
CREATE TRAINING_JOB train_customer_churn

MODEL customer_churn

FEATURE_SET customer_features

FRAMEWORK SKLEARN

ALGORITHM RandomForestClassifier;
```

---

## XGBoost Example

```sql
CREATE TRAINING_JOB train_customer_churn

MODEL customer_churn

FEATURE_SET customer_features

FRAMEWORK XGBOOST

ALGORITHM XGBClassifier;
```

---

## PyTorch Example

```sql
CREATE TRAINING_JOB train_customer_churn

MODEL customer_churn

FEATURE_SET customer_features

FRAMEWORK PYTORCH

ENTRYPOINT 'train.py';
```

---

## Metadata

| Field           | Description          |
| --------------- | -------------------- |
| training_job_id | Unique identifier    |
| model_id        | Associated model     |
| framework       | Training framework   |
| algorithm       | Algorithm used       |
| entrypoint      | Python script        |
| feature_set     | Feature set          |
| dataset         | Dataset              |
| resources       | Compute requirements |

---

# 9. DATASET Support

Datasets are primarily used for:

* Deep Learning
* LLM Training
* Reinforcement Learning

Example:

```sql
CREATE DATASET wikipedia_dataset;
```

Unlike FEATURE_SETs, datasets contain raw training data.

---

# 10. LLM Training

LLMs do not require explicit target columns.

Instead they declare objectives.

Example:

```sql
CREATE TRAINING_JOB train_llm

MODEL Mnemosyne_llm

DATASET wikipedia_dataset

FRAMEWORK PYTORCH

ENTRYPOINT 'train.py'

OBJECTIVE CAUSAL_LANGUAGE_MODELING;
```

---

## Supported Objectives

```text
CAUSAL_LANGUAGE_MODELING
MASKED_LANGUAGE_MODELING
SEQ2SEQ
EMBEDDING
INSTRUCTION_TUNING
RLHF
DPO
```

---

# 11. MODEL_RUN

## Purpose

Represents a single training execution.

Every training invocation creates a MODEL_RUN.

---

## Metadata

| Field               | Description       |
| ------------------- | ----------------- |
| run_id              | Unique identifier |
| model_id            | Associated model  |
| training_job_id     | Source job        |
| feature_set_version | Feature version   |
| dataset_version     | Dataset version   |
| hyperparameters     | Hyperparameters   |
| metrics             | Metrics           |
| artifact_location   | Model artifact    |
| start_time          | Start             |
| end_time            | End               |
| status              | Run status        |

---

## States

```text
QUEUED
RUNNING
SUCCEEDED
FAILED
CANCELLED
```

---

# 12. MODEL_VERSION

## Purpose

Represents a deployable model artifact.

Examples:

```text
customer_churn:v1
customer_churn:v2
customer_churn:v3
```

---

## Metadata

| Field             | Description        |
| ----------------- | ------------------ |
| version_id        | Version identifier |
| model_id          | Parent model       |
| run_id            | Source run         |
| artifact_location | Artifact location  |
| metrics           | Evaluation metrics |
| created_at        | Creation time      |

---

## Supported Artifact Formats

```text
.pt
.pth
.pkl
.joblib
.onnx
.safetensors
.gguf
```

---

# 13. TUNING_JOB

## Purpose

Hyperparameter optimization.

---

Example:

```sql
CREATE TUNING_JOB churn_tuning

MODEL customer_churn

TRAINING_JOB train_customer_churn

STRATEGY OPTUNA

OBJECTIVE ROC_AUC

TRIALS 100;
```

---

## Supported Strategies

```text
GRID
RANDOM
OPTUNA
HYPEROPT
BAYESIAN
EVOLUTIONARY
```

---

Execution:

```sql
RUN TUNING_JOB churn_tuning;
```

---

Output:

```text
100 MODEL_RUNS
       ↓
Best Run
       ↓
MODEL_VERSION
```

---

# 14. MODEL Registry

## Purpose

Central catalog of all model assets.

---

Registry Example

```text
customer_churn
 ├── v1
 ├── v2
 └── v3

revenue_forecast
 ├── v1
 └── v2
```

---

## Registry Commands

```sql
SHOW MODELS;
```

```sql
SHOW MODEL VERSIONS customer_churn;
```

```sql
DESCRIBE MODEL customer_churn;
```

```sql
DESCRIBE MODEL VERSION customer_churn:v3;
```

---

# 15. Model Evaluation

## Purpose

Evaluate trained models.

---

### Classification Metrics

```text
ACCURACY
PRECISION
RECALL
F1
ROC_AUC
PR_AUC
```

---

### Regression Metrics

```text
MAE
MSE
RMSE
MAPE
R2
```

---

### Recommendation Metrics

```text
PRECISION_AT_K
RECALL_AT_K
NDCG
MAP
```

---

### LLM Metrics

```text
PERPLEXITY
BLEU
ROUGE
BERTSCORE
LATENCY
TOKENS_PER_SECOND
```

---

## Evaluation Query

```sql
EVALUATE MODEL customer_churn:v4;
```

---

## Comparison Query

```sql
COMPARE MODELS
customer_churn:v3,
customer_churn:v4;
```

---

# 16. MODEL_PACKAGE

## Purpose

Stores everything required to reproduce a model.

Contains:

```text
Training Code
Dependencies
Container Image
Tokenizer
Preprocessing Logic
Postprocessing Logic
Configuration
```

---

# 17. MODEL_ENDPOINT

## Purpose

Serves model predictions.

---

Deployment:

```sql
DEPLOY MODEL customer_churn:v4;
```

---

Named Endpoint:

```sql
DEPLOY MODEL customer_churn:v4
AS churn_api;
```

---

States:

```text
STARTING
ACTIVE
SCALING
FAILED
STOPPED
```

---

# 18. Prediction Grammar

## Entity-Based Prediction

```sql
PREDICT MODEL customer_churn

FOR (
    customer_id = 1234
);
```

Execution:

```text
Entity Key
      ↓
Feature Lookup
      ↓
Model
      ↓
Prediction
```

---

## Direct Feature Prediction

```sql
PREDICT MODEL customer_churn

WITH (
    purchase_frequency = 2,
    avg_order_value = 75,
    days_since_last_purchase = 40
);
```

Used for:

* Simulations
* What-if analysis
* Testing

---

## Batch Prediction

```sql
PREDICT MODEL customer_churn

FROM customers;
```

Produces:

```text
PREDICTION_TABLE
```

---

# 19. PREDICTION_TABLE

## Purpose

Stores generated predictions.

---

Schema

```text
entity_key
prediction
confidence
timestamp
model_version
```

---

# 20. LLM Inference

Example:

```sql
GENERATE
USING MODEL Mnemosyne_llm

PROMPT '
Explain customer churn
';
```

Alternative:

```sql
SELECT GENERATE(
    MODEL Mnemosyne_llm,
    'Explain customer churn'
);
```

---

# 21. Monitoring

Mnemosyne continuously tracks:

```text
Latency
Throughput
Failures
Prediction Volume
Feature Drift
Prediction Drift
Concept Drift
```

---

Queries:

```sql
SHOW MODEL ENDPOINTS;
```

```sql
SHOW MODEL METRICS;
```

```sql
SHOW MODEL DRIFT;
```

---

# 22. Explainability

Future Capability

Example:

```sql
EXPLAIN PREDICTION

MODEL customer_churn

FOR (
    customer_id = 1234
);
```

Returns:

```text
Top Features
Feature Contributions
Confidence
```

---

# 23. Security

Permissions:

```text
CREATE_MODEL
ALTER_MODEL
DROP_MODEL

TRAIN_MODEL
DEPLOY_MODEL
PREDICT_MODEL

VIEW_MODEL
VIEW_MODEL_RUN
VIEW_MODEL_VERSION
```

---

# 24. End-to-End Example

```sql
CREATE FEATURE_SET customer_features

ENTITY_KEY(customer_id)

FEATURES (
    purchase_frequency,
    avg_order_value,
    days_since_last_purchase
)

TARGET churn;

CREATE MODEL customer_churn
TYPE CLASSIFICATION;

CREATE TRAINING_JOB train_customer_churn

MODEL customer_churn

FEATURE_SET customer_features

FRAMEWORK SKLEARN

ALGORITHM RandomForestClassifier;

RUN TRAINING_JOB train_customer_churn;

DEPLOY MODEL customer_churn:v1;

PREDICT MODEL customer_churn
FOR (
    customer_id = 1234
);
```

---

# 25. Integration Test Queries

## Test 1: Create Model

```sql
CREATE MODEL customer_churn
TYPE CLASSIFICATION;
```

Expected:

```text
MODEL created
```

---

## Test 2: Create Training Job

```sql
CREATE TRAINING_JOB train_customer_churn

MODEL customer_churn

FEATURE_SET customer_features

FRAMEWORK SKLEARN

ALGORITHM RandomForestClassifier;
```

Expected:

```text
TRAINING_JOB created
```

---

## Test 3: Train

```sql
RUN TRAINING_JOB train_customer_churn;
```

Expected:

```text
MODEL_RUN created
MODEL_VERSION v1 registered
```

---

## Test 4: Registry

```sql
SHOW MODEL VERSIONS customer_churn;
```

Expected:

```text
customer_churn:v1
```

---

## Test 5: Deployment

```sql
DEPLOY MODEL customer_churn:v1;
```

Expected:

```text
MODEL_ENDPOINT created
```

---

## Test 6: Prediction

```sql
PREDICT MODEL customer_churn
FOR (
    customer_id = 1234
);
```

Expected:

```json
{
  "prediction": "CHURN",
  "probability": 0.92
}
```

---

## Test 7: Evaluation

```sql
EVALUATE MODEL customer_churn:v1;
```

Expected:

```text
ROC_AUC
PRECISION
RECALL
F1
```

---

# 26. Long-Term Vision

The MODEL Layer establishes Mnemosyne as an AI-native platform where machine learning assets are managed with the same governance, lineage, versioning, observability, and reproducibility as data assets.

Just as TABLE is Mnemosyne's fundamental representation of data, MODEL is Mnemosyne's fundamental representation of machine intelligence.
