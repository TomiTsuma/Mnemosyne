# Quantile and Percentile AST

There is not NTILE, QUANTILE or PERCENTILE in the GRAMMAR
## Resolution {#resolution}

Resolved. `quantile(p)(x)`, `median(x)`, `percentile(x, p)` and `NTILE(n) OVER (...)` are supported. `quantile(0.5)(x)` previously returned `0`. See `docs/changelog/101026-020728_select_correctness_changelog.md` and `docs/mnemo_sql_documentation/queries/aggregation.md`.
