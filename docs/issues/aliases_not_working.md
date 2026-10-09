# Need to add aggregation for aggregation syntax such as SELECT COUNT(name) as total_customers FROM customers

Currently "SELECT name as customer_name FROM customers" syntax works well
## Resolution {#resolution}

Resolved. `SELECT COUNT(name) AS total_customers FROM customers` returns a column named `total_customers`, and aliases apply to the whole select item. See `docs/changelog/101026-020728_select_correctness_changelog.md`.
