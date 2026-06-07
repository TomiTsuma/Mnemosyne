# Mnemo SQL Documentation

Mnemo SQL is the query and control language for **Mnemosyne** (Mnemo): a column-oriented analytical database with extensions for storage, distributed execution, connectors, pipelines, streaming, and machine learning. This documentation is written for developers who integrate with Mnemo via HTTP/TCP, client libraries, or embedded interpreters.

## What Mnemo SQL covers

Mnemo SQL is not a generic ANSI SQL clone. It combines:

- **Analytical SQL** — `SELECT`, `INSERT`, aggregations, joins, window functions (partial)
- **Catalog DDL** — databases, tables, views, materialized views
- **First-class entities** — storage units, nodes, replication, sharding, connectors, pipelines, streams, model catalog
- **Model layer** — feature sets, training jobs, tuning, deployment, prediction, evaluation
- **Control verbs** — `RUN`, `DEPLOY`, `PREDICT`, `PUBLISH`, `REGISTER`, and more

Statements are parsed by a recursive-descent parser (`src/Parsers/parser.cpp`). The canonical grammar sketch lives in `src/Parsers/grammar.y` and the complete reference in [grammar/full-grammar.md](grammar/full-grammar.md).

## Documentation map

| Section | Path | Topics |
|---------|------|--------|
| Getting started | [getting-started/](getting-started/) | Running queries, conventions, session context |
| Data types | [data-types/](data-types/) | Column types, literals, compatibility |
| Queries (DQL) | [queries/](queries/) | `SELECT`, joins, `GROUP BY`, `ORDER BY`, `LIMIT` |
| Data manipulation | [dml/](dml/) | `INSERT` |
| Schema & DDL | [ddl/](ddl/) | `CREATE` / `DROP` / `ALTER` for tables and catalog objects |
| Metadata | [metadata/](metadata/) | `SHOW`, `DESCRIBE`, `EXPLAIN`, `USE` |
| Expressions | [expressions/](expressions/) | Operators, functions, precedence |
| **First-class entities** | [entities/](entities/) | **All catalog objects with properties and examples** |
| **Model layer** | [model-layer/](model-layer/) | **ML workflows, training, inference, visualizations** |
| Grammar reference | [grammar/](grammar/) | Full EBNF, lexer, keywords, statement catalog |

### Highlighted guides (new / expanded)

| Guide | Description |
|-------|-------------|
| [entities/README.md](entities/README.md) | Entity domain map, capability matrix, relationships |
| [entities/data-layer.md](entities/data-layer.md) | `DATABASE`, `TABLE`, `VIEW`, `MATERIALIZED_VIEW` |
| [model-layer/workflows.md](model-layer/workflows.md) | End-to-end ML: features → train → tune → deploy → predict |
| [model-layer/visualizations.md](model-layer/visualizations.md) | Charts, dashboards, monitoring from SQL results |

## Quick example

```sql
CREATE DATABASE IF NOT EXISTS analytics;
USE analytics;

CREATE TABLE orders (
    id     Int64,
    region String,
    amount Float64
) ENGINE = Memory;

INSERT INTO orders VALUES
    (1, 'US-East', 99.50),
    (2, 'US-West', 42.00);

SELECT region, sum(amount) AS total
FROM orders
GROUP BY region
ORDER BY total DESC
LIMIT 10;
```

Send the query to the server (default HTTP port `1143`):

```bash
curl -G "http://127.0.0.1:1143/query" \
  --data-urlencode "query=SELECT 1" \
  --data-urlencode "format=JSON"
```

```python
from scripts._mnemo_client import Client
client = Client("http://127.0.0.1:1143")
r = client.query("SELECT 1")
assert r.ok()
```

## First-class entities at a glance

| Domain | Entities |
|--------|----------|
| Data | `DATABASE`, `TABLE`, `VIEW`, `MATERIALIZED_VIEW` |
| Infrastructure | `CLUSTER`, `NODE`, `STORAGE_UNIT`, `REPLICA_GROUP`, `SHARD_GROUP` |
| Integration | `CONNECTOR` |
| Pipeline | `PIPELINE`, `STAGE`, `TASK`, `TRIGGER` |
| Streaming | `TOPIC`, `STREAM`, `CONSUMER_GROUP` |
| AI | `FEATURE_SET`, `DATASET`, `MODEL`, `TRAINING_JOB`, `TUNING_JOB`, `MODEL_ENDPOINT` |

Each entity supports catalog operations (`CREATE`, `SHOW`, `DESCRIBE`, `DROP`) and documented properties. See [entities/README.md](entities/README.md).

## ML workflow at a glance

```sql
CREATE FEATURE_SET features FROM t ENTITY_KEY(id) FEATURES(f1, f2) TARGET label;
CREATE MODEL m TYPE CLASSIFICATION;
CREATE TRAINING_JOB job MODEL m FEATURE_SET features
    FRAMEWORK SKLEARN ALGORITHM RandomForestClassifier OBJECTIVE ACCURACY;
RUN TRAINING_JOB job;
EVALUATE MODEL m:1;
DEPLOY MODEL m:1 AS endpoint;
PREDICT MODEL m:1 FROM t;
```

Full walkthrough: [model-layer/workflows.md](model-layer/workflows.md).

## Statement overview

Top-level statements recognized by the parser:

| Category | Statements |
|----------|------------|
| Query | `SELECT` |
| DML | `INSERT` |
| DDL | `CREATE`, `DROP`, `TRUNCATE`, `DETACH`, `ALTER`, `REFRESH` |
| Session | `USE` |
| Introspection | `SHOW`, `DESCRIBE`, `EXPLAIN` |
| Infrastructure | `REGISTER`, `DRAIN`, `REMOVE`, `TEST`, `DISCOVER` |
| Pipelines | `RUN`, `PAUSE`, `RESUME` (pipelines); `RUN` (training/tuning jobs) |
| Streaming | `PUBLISH`, `SUBSCRIBE` |
| Model layer | `DEPLOY`, `PREDICT`, `EVALUATE`, `COMPARE`, `GENERATE` |

## Integration tests

Runnable SQL examples live in `scripts/`:

| Script | Coverage |
|--------|----------|
| `test_models_api.py` | Full ML lifecycle |
| `test_tuning_api.py` | Hyperparameter tuning |
| `test_pipelines_api.py` | ETL pipelines |
| `test_streams_api.py` | Topics, publish, subscribe |
| `test_connectors_api.py` | REST/Postgres/S3 connectors |
| `test_nodes_api.py` | Nodes and clusters |
| `test_storage_units_api.py` | Storage units |
| `test_replica_groups_api.py` | Replication |
| `test_shard_groups_api.py` | Sharding |

## Not yet implemented (parser)

The following appear in PRDs or common SQL dialects but are **not** parsed today:

- `UPDATE`, `DELETE`
- `DROP DATABASE`
- `SET` session variables (keyword reserved in lexer)
- `WITH` (CTE), `UNION`, `DISTINCT` select modifier (grammar stub only)
- `BETWEEN`, `CASE`, `CAST`, `LIKE`, `IS NULL` in expressions (grammar stub; verify before use)
- Governance entities (`USER`, `ROLE`, `POLICY`) — PRD only

When in doubt, consult [grammar/statement-catalog.md](grammar/statement-catalog.md) and the parser source.

## Related resources

- Integration tests with runnable SQL: `scripts/test_*_api.py`
- Legacy grammar summary: `docs/GRAMMAR.md`
- Architecture: `docs/ARCHITECTURE.md`
- Model layer PRD: `docs/PRDS/MODEL_LAYER_.md`
- First-class entities PRD: `docs/PRDS/FIRST_CLASS_ENTITIES.md`
