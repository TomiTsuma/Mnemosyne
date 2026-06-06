# Keywords and Tokens

Keywords are case-insensitive. The lexer maps them to `TokenType` in `src/Parsers/lexer.cpp`.

## Query and DML

| Keyword | Usage |
|---------|--------|
| `SELECT` | Read query |
| `INSERT` | Load data |
| `INTO` | Insert target |
| `VALUES` | Insert literals |
| `FROM` | Table source |
| `WHERE` | Row filter |
| `GROUP` | With `BY` — grouping |
| `BY` | Group/order partition |
| `HAVING` | Group filter |
| `ORDER` | Sort |
| `LIMIT` | Row cap |
| `OFFSET` | Reserved (offset pagination) |
| `AS` | Alias |
| `AND`, `OR`, `NOT` | Logic |
| `IN` | Membership |
| `JOIN`, `LEFT`, `RIGHT`, `INNER`, `OUTER`, `ON` | Joins |
| `DISTINCT`, `ALL` | Select modifiers (planned) |
| `WITH` | CTE (planned) |
| `UNION` | Combine queries (planned) |
| `OVER`, `PARTITION` | Window functions |
| `ARRAY`, `LATERAL`, `ANY`, `ROLLUP` | Reserved / planned |

## DDL

| Keyword | Usage |
|---------|--------|
| `CREATE` | Create object |
| `DROP` | Remove object |
| `ALTER` | Modify object |
| `TRUNCATE` | Clear table |
| `DETACH` | Detach storage |
| `TABLE` | Table object |
| `DATABASE` | Database object |
| `VIEW`, `VIEWS` | View objects |
| `MATERIALIZED` | Materialized view |
| `IF`, `NOT`, `EXISTS` | Conditional DDL |
| `ENGINE` | Table engine |
| `ADD`, `DROP`, `MODIFY`, `COLUMN` | Alter column |
| `REFRESH` | Refresh MV |

## Types and literals

| Keyword | Usage |
|---------|--------|
| `NULL` | Null literal |
| `TRUE`, `FALSE` | Boolean literals |

## Aggregates (also function tokens)

`SUM`, `COUNT`, `AVG`, `MIN`, `MAX`

## Session and meta

| Keyword | Usage |
|---------|--------|
| `USE` | Current database |
| `SHOW` | Introspection |
| `DESCRIBE`, `DESC` | Schema detail |
| `EXPLAIN` | Query plan |

## Storage and topology

`STORAGE`, `UNIT`, `UNITS`, `USAGE`, `PATH`, `BUCKET`, `ENDPOINT`, `REGION`, `TYPE`

## Nodes and clusters

`NODE`, `NODES`, `REGISTER`, `DRAIN`, `REMOVE`, `HOST`, `PORT`, `ROLE`, `SET`, `METRICS`, `CAPABILITIES`, `WORKER`, `COORDINATOR`, `OBSERVER`, `COMPUTE`, `HYBRID`, `GPU`, `CLUSTER`, `CLUSTERS`

## Replication and sharding

`REPLICA`, `REPLICAS`, `REPLICA_GROUP`, `REPLICA_GROUPS`, `REPLICATION`, `STATUS`, `CONSISTENCY`, `QUORUM`, `SYNCHRONOUS`, `ASYNCHRONOUS`, `PLACEMENT`, `NODE_AWARE`, `SHARD`, `SHARDS`, `SHARD_GROUP`, `SHARD_GROUPS`, `KEY`

## Connectors

`CONNECTOR`, `CONNECTORS`, `TEST`, `DISCOVER`, `AUTH`, `SCHEMA`

## Pipelines

`PIPELINE`, `PIPELINES`, `STAGE`, `STAGES`, `TASK`, `TASKS`, `TRIGGER`, `TRIGGERS`, `RUN`, `PAUSE`, `RESUME`, `SCHEDULE`, `DEPENDS`, `BODY`, `OWNER`, `BUILTIN`, `BUILT_IN`, `PIPELINE_RUNS`, `FOR`

## Streams

`STREAM`, `STREAMS`, `TOPIC`, `TOPICS`, `CONSUMER_GROUP`, `CONSUMER_GROUPS`, `PUBLISH`, `SUBSCRIBE`, `RETAIN`, `STREAM_METRICS`, `PARTITIONS`, `DAYS`, `FOREVER`

## Model layer

`DEPLOY`, `PREDICT`, `EVALUATE`, `COMPARE`, `GENERATE`

Multi-word identifiers also recognized as tokens:

`MODEL`, `MODEL_TEMPLATE`, `MODELS`, `FEATURE_SET`, `FEATURE_SETS`, `DATASET`, `DATASETS`, `TRAINING_JOB`, `TRAINING_JOBS`, `TUNING_JOB`, `TUNING_JOBS`, `STORAGE_UNIT`, `STORAGE_UNITS`, `STORAGE_USAGE`, `PIPELINE_METRICS`, `REPLICATION_STATUS`, `SHARD_STATUS`, `NODE_METRICS`, `NODE_CAPABILITIES`, `NODE_PARTITIONS`, `MODEL_VERSIONS`, `MODEL_ENDPOINTS`, `MODEL_METRICS`, `MODEL_DRIFT`, `MODEL_TEMPLATES`

## Operators and punctuation

| Token | Lexeme |
|-------|--------|
| Comparison | `=`, `!=`, `<>`, `<`, `>`, `<=`, `>=` |
| Arithmetic | `+`, `-`, `*`, `/`, `%` |
| Punctuation | `(`, `)`, `,`, `.`, `;`, `:`, `*` |
| Concat (future) | `->` |

## Identifiers vs keywords

Unrecognized words become `Identifier` tokens. Some entity names use `Identifier` with value `CONNECTOR` when not lexed as keyword alias—parser accepts both forms via helper functions (`consume_connector_keyword()`, etc.).

## Not keywords (not parsed as statements)

`UPDATE`, `DELETE`, `SET` (session) — appear in legacy docs or lexer but **no** `parse_query` branch yet.
