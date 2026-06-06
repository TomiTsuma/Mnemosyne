# Full Grammar (EBNF)

Derived from `src/Parsers/grammar.y` and `Parser` implementation. **Bold** rules are fully parsed today; _italic_ rules are stubbed or partial.

## Top level

```ebnf
query           ::= statement [ ';' ] EOF

statement       ::= select_stmt
                  | insert_stmt
                  | create_stmt
                  | drop_stmt
                  | alter_stmt
                  | show_stmt
                  | describe_stmt
                  | explain_stmt
                  | use_stmt
                  | refresh_stmt
                  | register_node_stmt
                  | drain_node_stmt
                  | remove_node_stmt
                  | test_stmt
                  | discover_stmt
                  | run_stmt
                  | pause_stmt
                  | resume_stmt
                  | publish_stmt
                  | subscribe_stmt
                  | deploy_stmt
                  | predict_stmt
                  | evaluate_stmt
                  | compare_stmt
                  | generate_stmt
```

## SELECT

```ebnf
select_stmt     ::= ** SELECT expr_list
                      [ FROM from_source [ join_clause ... ] ]
                      [ WHERE boolean_expr ]
                      [ GROUP BY expr_list ]
                      [ HAVING boolean_expr ]
                      [ ORDER BY order_list ]
                      [ LIMIT limit_clause ] **

from_source     ::= table_ref
                  | CONNECTOR identifier '.' identifier

join_clause     ::= ( INNER | LEFT [ OUTER ] | RIGHT [ OUTER ] ) JOIN table_ref ON boolean_expr

table_ref       ::= identifier [ identifier ]   /* table [ alias ] */

limit_clause    ::= integer [ ',' integer ]     /* offset, count if two integers */
```

### Planned (not in parse_select today)

```ebnf
_cte            ::= _ WITH cte ( ',' cte )* _
_cte            ::= _ identifier AS '(' select_stmt ')' _
_union          ::= _ select_stmt UNION ( ALL | DISTINCT ) select_stmt _
_distinct       ::= _ SELECT ( ALL | DISTINCT ) ... _
```

## INSERT

```ebnf
insert_stmt     ::= ** INSERT INTO identifier [ column_list ] VALUES value_rows **

column_list     ::= '(' identifier ( ',' identifier )* ')'
value_rows      ::= '(' expr ( ',' expr )* ')' ( ',' '(' expr ( ',' expr )* ')' )*
```

## CREATE

```ebnf
create_stmt     ::= CREATE ( if_not_exists )? create_target create_body

if_not_exists   ::= IF NOT EXISTS

create_target   ::= DATABASE identifier
                  | TABLE identifier
                  | VIEW identifier
                  | MATERIALIZED VIEW identifier
                  | STORAGE_UNIT identifier
                  | STORAGE UNIT identifier
                  | NODE identifier
                  | CLUSTER identifier
                  | REPLICA_GROUP identifier | REPLICA GROUP identifier
                  | SHARD_GROUP identifier | SHARD GROUP identifier
                  | CONNECTOR identifier
                  | PIPELINE identifier
                  | STAGE identifier IN PIPELINE identifier
                  | TASK identifier IN STAGE identifier IN PIPELINE identifier
                  | TRIGGER identifier ON PIPELINE identifier
                  | STREAM identifier [ TOPIC identifier ]
                  | TOPIC identifier
                  | CONSUMER_GROUP identifier | CONSUMER GROUP identifier
                  | MODEL identifier
                  | MODEL_TEMPLATE identifier
                  | FEATURE_SET identifier
                  | DATASET identifier
                  | TRAINING_JOB identifier
                  | TUNING_JOB identifier

create_body     ::= /* target-specific clauses; see statement-catalog.md */
```

### CREATE TABLE body

```ebnf
table_body      ::= '(' column_def ( ',' column_def )* ')'
                    [ ENGINE '=' identifier ]
                    [ STORAGE_UNIT identifier ]
                    [ SHARD_GROUP identifier ]
                    [ REPLICA_GROUP identifier ]

column_def      ::= identifier type_identifier
```

### CREATE VIEW body

```ebnf
view_body       ::= AS select_stmt
```

## DROP / TRUNCATE / DETACH

```ebnf
drop_stmt       ::= ( DROP | DETACH | TRUNCATE ) ( if_exists )? drop_target

drop_target     ::= TABLE identifier
                  | VIEW identifier
                  | MATERIALIZED VIEW identifier
                  | STORAGE_UNIT identifier
                  | NODE identifier
                  | CONNECTOR identifier
                  | PIPELINE identifier
                  | STAGE identifier FROM PIPELINE identifier
                  | TASK identifier FROM STAGE identifier IN PIPELINE identifier
                  | TRIGGER identifier [ FROM PIPELINE identifier ]
                  | STREAM | TOPIC | CONSUMER_GROUP | REPLICA_GROUP | SHARD_GROUP
                  | MODEL | MODEL_TEMPLATE | FEATURE_SET | DATASET
                  | TRAINING_JOB | TUNING_JOB
                  | TABLE identifier   /* TRUNCATE TABLE only */
```

## ALTER

```ebnf
alter_stmt      ::= ALTER alter_target alter_commands

alter_target    ::= TABLE identifier alter_table_cmds
                  | NODE identifier SET prop_list
                  | REPLICA_GROUP identifier SET prop_list
                  | SHARD_GROUP identifier SET prop_list
                  | CONNECTOR identifier SET prop_list
                  | STREAM identifier SET prop_list
                  | PIPELINE identifier SET prop_list

alter_table_cmds ::= alter_table_cmd ( ',' alter_table_cmd )*
alter_table_cmd ::= ADD [ COLUMN ] identifier type_identifier
                  | DROP [ COLUMN ] identifier
                  | MODIFY [ COLUMN ] identifier type_identifier
```

## USE

```ebnf
use_stmt        ::= USE [ DATABASE ] identifier
```

## SHOW / DESCRIBE / EXPLAIN

```ebnf
show_stmt       ::= SHOW show_object [ FOR ... ] [ identifier ]
describe_stmt   ::= ( DESCRIBE | DESC ) describe_object identifier
explain_stmt    ::= EXPLAIN statement
```

## Expressions

```ebnf
boolean_expr    ::= comparison_expr ( ( AND | OR ) comparison_expr )*

comparison_expr ::= term ( comp_op term )*
comp_op         ::= '=' | '!=' | '<>' | '<' | '>' | '<=' | '>=' | IN '(' subquery ')'

term            ::= factor ( ( '+' | '-' | '*' | '/' | '%' ) factor )*
                  [ AS identifier ]

factor          ::= literal
                  | identifier [ '(' arg_list ')' [ OVER window_spec ] ]
                  | identifier '.' identifier
                  | '*'
                  | '(' boolean_expr ')'
                  | '(' subquery ')'

arg_list        ::= [ expr ( ',' expr )* ]

window_spec     ::= '(' [ PARTITION BY expr_list ] [ ORDER BY order_list ] ')'

literal         ::= integer | float | string | TRUE | FALSE | NULL
```

### Planned expression forms

```ebnf
_planned        ::= _ expr BETWEEN expr AND expr _
_planned        ::= _ CASE [ expr ] WHEN expr THEN expr ... END _
_planned        ::= _ CAST '(' expr AS type ')' _
_planned        ::= _ expr IS [ NOT ] NULL _
_planned        ::= _ expr LIKE expr _
_planned        ::= _ expr IN '(' expr_list ')' _
```

## Model control

```ebnf
deploy_stmt     ::= DEPLOY [ MODEL ] model_version [ AS identifier ]

predict_stmt    ::= PREDICT [ MODEL ] model_version
                      ( FOR '(' kv_pairs ')' | WITH '(' kv_pairs ')' | FROM identifier )

evaluate_stmt   ::= EVALUATE [ MODEL ] model_version

compare_stmt    ::= COMPARE ( MODELS | MODEL ) model_version ( ',' model_version )*

generate_stmt   ::= GENERATE [ USING ] [ MODEL ] identifier [ PROMPT string ]

model_version   ::= identifier [ ':' version ]

run_stmt        ::= RUN ( TRAINING_JOB identifier
                       | TUNING_JOB identifier
                       | PIPELINE identifier )
```

## Stream control

```ebnf
publish_stmt    ::= PUBLISH identifier [ VALUES value_rows ]
subscribe_stmt  ::= SUBSCRIBE identifier [ CONSUMER_GROUP identifier ] [ LIMIT integer ]
```

## Node control

```ebnf
register_node_stmt ::= REGISTER NODE identifier HOST string PORT integer
drain_node_stmt    ::= DRAIN NODE identifier
remove_node_stmt   ::= REMOVE NODE ( if_exists )? identifier
test_stmt          ::= TEST CONNECTOR identifier
discover_stmt      ::= DISCOVER SCHEMA FROM CONNECTOR identifier
```

## refresh

```ebnf
refresh_stmt    ::= REFRESH MATERIALIZED VIEW identifier
```

## Lexical

See [keywords.md](keywords.md) for tokens and comments.

```ebnf
identifier      ::= [A-Za-z_][A-Za-z0-9_]*
string          ::= '\'' [^']* '\''
integer         ::= [0-9]+
float           ::= [0-9]+ '.' [0-9]* | '.' [0-9]+
comment         ::= '--' [^\n]* | '/*' ... '*/' | '//' [^\n]*
```
