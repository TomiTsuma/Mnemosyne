#!/usr/bin/env python3
"""Baseline probe: run representative queries from TEST_QUERIES.md levels.

Writes full server responses to scripts/baseline_probe.txt (the canonical
pass/fail artifact). Exit code is 0 only when every probe returns HTTP 2xx.
"""

from __future__ import annotations

import sys
from datetime import datetime, timezone
from pathlib import Path

from _mnemo_client import ServerProc, GREEN, RED, YELLOW, DIM, CYAN, RESET

SCRIPT_DIR = Path(__file__).resolve().parent
OUTPUT_FILE = SCRIPT_DIR / "baseline_probe.txt"

# Idempotent setup — (re)creates schema + seed data before feature probes.
SETUP = [
    ("setup CREATE DATABASE", "CREATE DATABASE IF NOT EXISTS sales_db"),
    ("setup USE", "USE sales_db"),
    ("setup DROP orders", "DROP TABLE IF EXISTS orders"),
    ("setup DROP customers", "DROP TABLE IF EXISTS customers"),
    (
        "setup CREATE customers",
        "CREATE TABLE customers (customer_id Int64, name String, age Int64)",
    ),
    (
        "setup CREATE orders",
        "CREATE TABLE orders (order_id Int64, customer_id Int64, product_id Int64, amount Float64)",
    ),
    (
        "setup INSERT customers",
        "INSERT INTO customers VALUES (1, 'Alice', 25), (2, 'Bob', 30), (3, 'Charlie', 35)",
    ),
    (
        "setup INSERT orders",
        "INSERT INTO orders VALUES (1, 1, 10, 99.5), (2, 2, 20, 150.0), (3, 3, 30, 75.25)",
    ),
]

PROBES = [
    ("L1 CREATE DATABASE", "CREATE DATABASE IF NOT EXISTS sales_db"),
    ("L1 USE", "USE sales_db"),
    ("L1 SHOW DATABASES", "SHOW DATABASES"),
    ("L1 CREATE TABLE", "CREATE TABLE IF NOT EXISTS customers (customer_id Int64, name String, age Int64)"),
    ("L1 SHOW TABLES", "SHOW TABLES"),
    ("L1 DESCRIBE", "DESCRIBE customers"),
    ("L2 INSERT", "INSERT INTO customers VALUES (4, 'Diana', 28)"),
    ("L2 SELECT *", "SELECT * FROM customers"),
    ("L2 projection", "SELECT name FROM customers"),
    ("L2 multi-col", "SELECT customer_id, age FROM customers"),
    ("L3 WHERE >", "SELECT * FROM customers WHERE age > 30"),
    ("L3 WHERE =", "SELECT * FROM customers WHERE customer_id = 2"),
    ("L3 AND", "SELECT * FROM customers WHERE age > 25 AND age < 40"),
    ("L3 OR", "SELECT * FROM customers WHERE age < 25 OR age > 30"),
    ("L4 ORDER BY", "SELECT * FROM customers ORDER BY age"),
    ("L4 ORDER BY DESC", "SELECT * FROM customers ORDER BY age DESC"),
    ("L5 COUNT", "SELECT COUNT(*) FROM customers"),
    ("L5 SUM", "SELECT SUM(age) FROM customers"),
    ("L5 AVG", "SELECT AVG(age) FROM customers"),
    ("L5 MIN", "SELECT MIN(age) FROM customers"),
    ("L5 MAX", "SELECT MAX(age) FROM customers"),
    ("L6 GROUP BY", "SELECT age, COUNT(*) FROM customers GROUP BY age"),
    ("L7 HAVING", "SELECT age, COUNT(*) FROM customers GROUP BY age HAVING COUNT(*) > 0"),
    ("L8 JOIN", "SELECT c.name, o.amount FROM customers c JOIN orders o ON c.customer_id = o.customer_id"),
    ("L9 scalar subquery", "SELECT * FROM customers WHERE age > (SELECT AVG(age) FROM customers)"),
    ("L9 IN subquery", "SELECT * FROM customers WHERE customer_id IN (SELECT customer_id FROM customers)"),
    ("L10 top-N", "SELECT customer_id, age FROM customers ORDER BY age DESC LIMIT 10"),
    (
        "L11 window",
        "SELECT customer_id, age, SUM(age) OVER (PARTITION BY age ORDER BY customer_id) FROM customers",
    ),
]


def _run_probe(client, label: str, sql: str, lines: list[str]) -> bool:
    r = client.query(sql)
    ok = r.ok()
    status_color = GREEN if ok else RED
    header = f"\n{label}  {sql}"
    print(f"\n{CYAN}{label}{RESET}  {DIM}{sql}{RESET}")
    print(f"  status={status_color}{r.status}{RESET}")
    body = r.body.strip()
    display = body if len(body) <= 600 else body[:600] + " ...(truncated)"
    print(f"  {DIM}{display}{RESET}")
    lines.append(header)
    lines.append(f"  status={r.status}")
    lines.append(f"  {body}")
    return ok


def main() -> int:
    lines: list[str] = [
        f"# baseline_probe run at {datetime.now(timezone.utc).isoformat()}",
        "",
    ]
    failures: list[str] = []

    with ServerProc() as client:
        print(f"{YELLOW}Setup{RESET}")
        lines.append("== SETUP ==")
        for label, sql in SETUP:
            if not _run_probe(client, label, sql, lines):
                failures.append(label)

        print(f"\n{YELLOW}Probes{RESET}")
        lines.append("\n== PROBES ==")
        for label, sql in PROBES:
            if not _run_probe(client, label, sql, lines):
                failures.append(label)

    passed = (len(SETUP) + len(PROBES)) - len(failures)
    total = len(SETUP) + len(PROBES)
    summary_color = GREEN if not failures else RED
    summary = f"\n{passed}/{total} probes passed"
    if failures:
        summary += f"\nFailed: {', '.join(failures)}"
    print(f"\n{summary_color}{summary}{RESET}")
    lines.append("\n== SUMMARY ==")
    lines.append(summary.strip())

    OUTPUT_FILE.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"\n{CYAN}Wrote {OUTPUT_FILE}{RESET}")
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
