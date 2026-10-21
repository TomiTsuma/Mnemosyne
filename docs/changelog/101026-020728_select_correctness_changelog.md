---
description: "SELECT correctness: aliases, operator precedence, typed aggregates, string and float grouping, ORDER BY, LIMIT/OFFSET, quantiles, NTILE, and errors instead of silent wrong answers."
sidebar_label: "SELECT Correctness"
sidebar_position: 3
slug: /changelog/101026-select-correctness
title: "SELECT Correctness: Aliases, Aggregates, GROUP BY, ORDER BY, LIMIT, Quantiles"
doc_type: changelog
---

# SELECT Correctness: Aliases, Aggregates, GROUP BY, ORDER BY, LIMIT, Quantiles {#select-correctness}

**Date:** 2026-10-10
**Branch:** `fix/sql-aliases-limit-quantiles`
**Issues:** `docs/issues/aliases_not_working.md`, `limit_grammar_not_working.md`, `no_quartile_percentile_grammar.md`

## Summary {#summary}

The four reports in `docs/issues/` turned out to be symptoms of a handful of root causes in the parser and in `InterpreterSelectQuery`, and investigating them found more queries that returned wrong results without any error. The evaluator now follows standard SQL semantics for the constructs it supports, and rejects the ones it does not, instead of returning an empty or wrong answer.

The work was done test-first: `scripts/test_select_api.py` was written against the old behaviour (18 of 67 checks passed) and drives the fixes.

---

## Root causes {#root-causes}

| Symptom | Root cause | Fix |
|---------|------------|-----|
| `AS` bound to the wrong operand (`age > 25 AS older` aliased only `25`), `SUM(x) / COUNT(*) AS y` returned nothing | The alias was parsed inside `parse_term`, below the comparison level | Aliases are parsed once per select item (`parse_select_item`), so they cover the whole expression; `AS` is optional |
| `2 + 3 * 4`, `a OR b AND c`, `x % 5` wrong | `parse_term` mixed `+ - * / %` at one level, `AND`/`OR` shared one level, and `%` was mapped to `Div` | Separate levels for `OR`, `AND`, comparison, `+ -`, `* / %`, and unary minus; new `ASTBinaryOp::Op::Mod` |
| `AVG` and `SUM` of floats truncated to integers | Every aggregate output column was created as `Int64` | Output columns are typed from their values (`column_from_fields`): `String`, `Float64` or `Int64` |
| `GROUP BY name` returned one group; `WHERE name = 'bob'` matched every row; `WHERE spend = 20.5` matched `20` | Group keys and `=`/`!=` went through `field_to_int64`, so every string became `0` and floats were truncated | Values are compared by type (`compare_fields`); group keys use a canonical text of the value (`field_key`) |
| `WHERE spend * 2 > 50`, `SELECT spend * 2` returned nothing | `eval_expr` had no arithmetic and silently returned an empty `Field` for anything it did not know | Arithmetic and unary operators are evaluated; unknown expressions raise instead of returning an empty value |
| `LIMIT 3 OFFSET 2` and `LIMIT 2, 3` ignored the offset | `OFFSET` was not parsed, and the interpreter used only the count | `parse_limit` reads all three forms; `limit_block` applies offset then count |
| `LIMIT 0` returned every row | A count of `0` was used as "no limit" | `has_limit` is stored on the AST, so `0` means zero rows |
| `quantile(0.5)(age)` returned `0` | The parser read `quantile(0.5)` as a call and left `(age)` behind; the leftover was never checked | Parametric calls are parsed (`ASTFunction::parameters`); `median` was added |
| `HAVING n > 3`, `GROUP BY big` failed with `Unknown column` | The analyzer only knew table columns | Select aliases are collected (`AnalyzeResult::select_aliases`) and accepted in `WHERE`, `GROUP BY`, `HAVING` |
| `ORDER BY c.region_id` did not sort | `parse_order_by` consumed one token (`c`) and left `.region_id` behind | Qualified keys are parsed and resolved against the result, then the source rows |
| `ORDER BY` used only the first key, numerically | `sort_block` looked at `order_by[0]` and compared with `field_to_double` | Multi-key, stable, typed comparison; `NULL < numbers < strings` (`total_compare`) |
| Typos like `... WHERE a > 1 BANANA` returned rows | Nothing checked that the whole statement had been consumed | `SELECT` must end at the end of input or `;` |
| `SELECT name, COUNT(*) FROM t` returned the first name | No validation of ungrouped columns | `validate_grouped` raises `Column '...' must appear in GROUP BY or inside an aggregate function` |
| Views with aliases or aggregates could not be queried | The analyzer registered only the base table's columns for a view | A view's output names (aliases, expressions) are registered with a type |

## Behaviour changes to be aware of {#behaviour-changes}

- Statements that previously "worked" by returning wrong or empty data now fail: leftover tokens, unknown functions, `ORDER BY` an expression, ungrouped columns beside aggregates, `SELECT *` with aggregation, window functions combined with `GROUP BY`.
- Default column names changed for non-aliased expressions from `col` to the expression text (for example `(spend * 2)`). Plain columns and aggregates such as `COUNT(*)` and `SUM(spend)` keep their names.
- `SUM` of integers returns an integer; `AVG` and quantiles return floating-point numbers.
- Groups are returned in order of first appearance (previously ordered by an integer key that collapsed every string to `0`). Use `ORDER BY` for a defined order.
- A scalar or `IN` subquery runs once per statement instead of once per row.
- When reading from a connector, the limit is pushed down only if there is no `WHERE`, `ORDER BY`, aggregation or window, and it now includes the offset. Previously the count alone was pushed down even with a filter, which could drop matching rows.

## Files {#files}

- `src/Parsers/ast.h`, `parser.h`, `parser.cpp`, `format.cpp` — precedence levels, select-item aliases, parametric calls, `LIMIT`/`OFFSET`, `has_limit`, qualified `ORDER BY`, trailing-token check, unary minus folding.
- `src/Analyzer/analyzer.h`, `analyzer.cpp` — select aliases in `WHERE`/`GROUP BY`/`HAVING`; view output columns.
- `src/Interpreters/interpreter_select_query.cpp` — rewritten evaluator: typed comparison and arithmetic, group-aware aggregate evaluation, `validate_grouped`, typed output columns, stable multi-key sort, `limit_block`, `NTILE`, quantiles.

## Tests {#tests}

- New `scripts/test_select_api.py` — 74 checks; expected values are computed in Python from the inserted rows. Needs a running server.
- New parser cases in `tests/test_parsers.h` — precedence, aliases, keyword alias after `AS`, unary minus, `LIMIT` forms, parametric calls, qualified `ORDER BY`, rejected leftovers (9 test cases, no server needed).

All server-backed suites pass against a Release build: `test_select_api` 74/74, `test_file_engine_api` 29/29, `test_use_api` 15/15, `test_views_api` 36/36, `test_storage_units_api` 32/32, `test_shard_groups_api` 47/47, `test_replica_groups_api` 38/38, `test_streams_api` 58/58, `test_pipelines_api` 74/74, `test_connectors_api` 38/38, `test_feature_sets_api` 28/28, `test_models_api` 54/54, `test_tuning_api` 27/27.

`mnemosyne_tests` (Catch2) is unchanged apart from the new parser cases: 53 test cases, of which 4 fail and 1 is skipped. The failures are not caused by this change:

- `Interpreter executes SELECT` and `Interpreter handles errors` call the planner-based interpreter with an empty plan (known since the S3 work).
- `SUM function registered` and `COUNT function registered` fail because `AggregateFunctionFactory::instance` never registers the built-ins; registration only happens inside `get_aggregate_function`, so a fresh `instance` lookup returns `nullptr`.
- `Coordination creates cluster` terminates the process with exit code 127, which hides the final summary. Run the suite with `'~Coordination creates cluster'` to see it.

## Known limitations {#known-limitations}

- `ORDER BY` accepts column names and aliases only. Order by an expression through an alias.
- Window functions compute over the whole partition; frames and running totals are not supported, and windows cannot be mixed with `GROUP BY`.
- No null bitmap on columns: `AVG`, `MIN` and `MAX` over zero rows return `0` instead of `NULL`, and `NULL` cannot be stored.
- Scalar functions (`toYear`, `upper`, ...), `LIKE`, `DISTINCT` and `UNION` are not implemented. Each now fails with an error instead of returning nothing.
- The planner path (`Analyzer::buildQueryTree` and `Planner::plan_limit`) still reads `limit` as `{count, offset}` rather than `{offset, count}` and is not used for HTTP `SELECT`; it needs aligning before it is.
- Evaluation is row-at-a-time and `JOIN` is a nested loop, which is fine for the current data sizes but not for large tables.
