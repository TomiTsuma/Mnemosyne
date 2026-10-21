---
description: "SQL reference pages brought up to date with the statements added in October 2026, plus corrections to claims that did not match the code."
sidebar_label: "Document New Statements"
sidebar_position: 10
slug: /changelog/221026-document-new-statements
title: "Document the Statements Added in October 2026"
doc_type: changelog
---

# Document the Statements Added in October 2026 {#document-new-statements}

**Branch:** `docs/document-new-statements`
**Date:** 2026-10-22

## Summary {#summary}

Five features had been added with only a changelog entry each. This brings
`docs/mnemo_sql_documentation/` up to date so the statements can be found in the reference,
and corrects statements in those pages that did not match the code.

## Documented {#documented}

- `ALTER CLUSTER`, `DROP CLUSTER`, derived cluster status, `SHOW CLUSTER STATUS` and
  `SHOW CLUSTER METRICS` (`entities/nodes-and-clusters.md`), and `ALTER NODE ... SET STATUS`.
- `ALTER STORAGE_UNIT ... SET RETENTION`, stated plainly as recorded and not enforced
  (`entities/storage-units.md`).
- `ALTER VIEW`, `ALTER MATERIALIZED VIEW ... SET REFRESH`, scheduled refresh and the new
  `SHOW MATERIALIZED VIEWS` columns (`ddl/views.md`, `ddl/alter.md`).
- `INSERT ... SELECT` (`dml/README.md`), `EXPORT TABLE ... TO CONNECTOR` and loading connector
  data into a table (`entities/connectors.md`).
- `DATA_QUALITY_RULE`, `QUALITY` tasks and `RUN DATA_QUALITY_RULE` (`entities/pipelines.md`).
- The grammar, statement catalog, keyword list, `SHOW`/`DESCRIBE` index, entity index and test
  script lists.

## Corrected {#corrected}

- `grammar/statement-catalog.md` and `ddl/drop-and-truncate.md` said `DROP DATABASE` was not
  implemented. It is, and it deletes the database's data from disk; both pages now say so.
- `dml/README.md` called `INSERT ... SELECT` an unverified grammar stub.
- `entities/storage-units.md` listed columns for `SHOW STORAGE_UNITS` that it never returned.
- `entities/pipelines.md` described a `task_runs` field on `SHOW PIPELINE RUNS`. No statement
  returns it: a failed run does not say which task failed or why. The page now says so.

## Changelog conventions {#changelog-conventions}

The six changelogs written earlier in this series had no frontmatter and unanchored headings.
They now carry the frontmatter block and an explicit anchor on every heading that the other
recent changelogs use, and write function names without parentheses.
