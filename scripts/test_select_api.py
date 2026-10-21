#!/usr/bin/env python3
"""End-to-end API tests for SELECT semantics.

Expected results are computed in Python from the same rows that are inserted, so
a failing check means the server disagrees with a plain reference calculation.

Covers: aliases (AS, bare, on aggregates/expressions/comparisons), typed aggregate
results, GROUP BY on string/float/expression keys, HAVING on aliases, WHERE with
strings/floats/precedence/arithmetic, ORDER BY (strings, multi-key, unselected
columns), LIMIT/OFFSET, quantile/median/percentile/NTILE, and that unsupported
constructs fail loudly instead of returning an empty or wrong result.

Usage:
    python3.11 scripts/test_select_api.py --no-launch
    python3.11 scripts/test_select_api.py --url http://127.0.0.1:1143 --no-launch

The server must already be running (start it from a scratch directory).
Exit code is 0 if all tests pass, 1 otherwise.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

from _mnemo_client import Client, ServerProc, TestRunner, YELLOW, RESET

DB = "select_test_db"

# (customer_id, name, age, spend)
CUSTOMERS = [
    (1, "ann", 28, 10.5), (2, "bob", 22, 20.0), (3, "cy", 27, 30.25),
    (4, "di", 35, 40.0), (5, "ed", 23, 50.75), (6, "fay", 11, 60.0),
    (7, "gus", 41, 5.0), (8, "hal", 19, 15.5), (9, "ivy", 33, 25.0),
]
# (k, s, v): float keys that only differ after the decimal point, repeated strings
GROUPS = [(1.2, "a", 1), (1.7, "b", 2), (1.2, "a", 3), (1.7, "a", 4)]


def quantile(values: list[float], level: float) -> float:
    vals = sorted(values)
    pos = level * (len(vals) - 1)
    lo, hi = math.floor(pos), math.ceil(pos)
    return vals[lo] + (vals[hi] - vals[lo]) * (pos - lo)


def ntile_buckets(rows: list[tuple], tiles: int) -> dict[int, int]:
    ordered = sorted(rows, key=lambda r: r[2])
    base, extra = divmod(len(ordered), tiles)
    out, idx = {}, 0
    for bucket in range(1, tiles + 1):
        for _ in range(base + (1 if bucket <= extra else 0)):
            out[ordered[idx][0]] = bucket
            idx += 1
    return out


def cell_equal(got, exp) -> bool:
    if isinstance(exp, (int, float)) and isinstance(got, (int, float)) \
            and not isinstance(exp, bool):
        return math.isclose(got, exp, rel_tol=1e-9, abs_tol=1e-9)
    return got == exp


def rows_equal(got: list, exp: list) -> bool:
    return len(got) == len(exp) and all(
        len(g) == len(e) and all(cell_equal(a, b) for a, b in zip(g, e))
        for g, e in zip(got, exp))


class Runner:
    def __init__(self, client: Client, t: TestRunner) -> None:
        self.client, self.t = client, t

    def select(self, sql: str):
        r = self.client.query(sql)
        if not r.ok():
            return None, None, r
        body = json.loads(r.body)
        return body.get("columns", []), body.get("data", []), r

    def expect(self, name: str, sql: str, exp: list, columns: list | None = None) -> None:
        cols, data, r = self.select(sql)
        if data is None:
            self.t.check(name, False, f"status={r.status} body={r.body[:200]!r}")
            return
        ok = rows_equal(data, exp) and (columns is None or cols == columns)
        self.t.check(name, ok, f"sql={sql}\n       cols={cols}\n       got={data}\n       exp={exp}")

    def expect_error(self, name: str, sql: str) -> None:
        r = self.client.query(sql)
        self.t.check(name, not r.ok(), f"expected an error, got status={r.status} body={r.body[:160]!r}")


def setup(client: Client, t: TestRunner) -> None:
    t.section("Setup")
    stmts = [
        f"DROP DATABASE IF EXISTS {DB}",
        f"CREATE DATABASE {DB}",
        f"USE {DB}",
        "CREATE TABLE customers (customer_id Int64, name String, age Int64, spend Float64)",
        "CREATE TABLE g (k Float64, s String, v Int64)",
    ]
    rows = ", ".join(f"({i}, '{n}', {a}, {s})" for i, n, a, s in CUSTOMERS)
    stmts.append(f"INSERT INTO customers VALUES {rows}")
    rows = ", ".join(f"({k}, '{s}', {v})" for k, s, v in GROUPS)
    stmts.append(f"INSERT INTO g VALUES {rows}")
    for sql in stmts:
        r = client.query(sql)
        t.check(f"setup: {sql[:60]}", r.ok(), f"status={r.status} body={r.body[:200]!r}")


def test_aliases(x: Runner) -> None:
    x.t.section("Aliases")
    x.expect("bare alias", "SELECT name n FROM customers ORDER BY customer_id LIMIT 2",
             [["ann"], ["bob"]], ["n"])
    x.expect("AS alias on columns", "SELECT name AS n, age AS a FROM customers WHERE customer_id = 1",
             [["ann", 28]], ["n", "a"])
    x.expect("alias on comparison covers the whole comparison",
             "SELECT age > 25 AS older FROM customers ORDER BY customer_id LIMIT 3",
             [[1], [0], [1]], ["older"])
    x.expect("alias on arithmetic", "SELECT spend * 2 AS dbl FROM customers ORDER BY customer_id LIMIT 2",
             [[21.0], [40.0]], ["dbl"])
    x.expect("alias on COUNT", "SELECT COUNT(name) AS total_customers FROM customers",
             [[9]], ["total_customers"])
    x.expect("alias on expression of aggregates",
             "SELECT SUM(spend) / COUNT(*) AS avg_spend FROM customers",
             [[sum(c[3] for c in CUSTOMERS) / 9]], ["avg_spend"])
    x.expect("two aliased aggregates", "SELECT MIN(age) AS youngest, MAX(age) AS oldest FROM customers",
             [[11, 41]], ["youngest", "oldest"])


def test_aggregates(x: Runner) -> None:
    x.t.section("Aggregates keep their type")
    spends = [c[3] for c in CUSTOMERS]
    x.expect("AVG keeps fraction", "SELECT AVG(spend) AS a FROM customers", [[sum(spends) / 9]])
    older = [c[3] for c in CUSTOMERS if c[2] > 25]
    x.expect("SUM(float) with WHERE", "SELECT SUM(spend) AS s FROM customers WHERE age > 25", [[sum(older)]])
    young = [c[3] for c in CUSTOMERS if c[2] < 25]
    x.expect("MIN/MAX(float)", "SELECT MIN(spend) AS lo, MAX(spend) AS hi FROM customers WHERE age < 25",
             [[min(young), max(young)]])
    x.expect("MIN/MAX(string)", "SELECT MIN(name) AS first, MAX(name) AS last FROM customers",
             [["ann", "ivy"]])
    x.expect("aggregate over an expression", "SELECT SUM(spend * 2) AS s FROM customers",
             [[sum(spends) * 2]])
    x.expect("aggregates over zero rows", "SELECT COUNT(*) AS n, SUM(age) AS s FROM customers WHERE age > 100",
             [[0, 0]])
    x.expect("COUNT(*) default column name", "SELECT COUNT(*) FROM customers", [[9]], ["COUNT(*)"])


def test_group_by(x: Runner) -> None:
    x.t.section("GROUP BY")
    x.expect("string key groups by value", "SELECT name, COUNT(*) AS n FROM customers GROUP BY name ORDER BY name",
             [[c[1], 1] for c in sorted(CUSTOMERS, key=lambda c: c[1])])
    x.expect("string key with SUM", "SELECT s, COUNT(*) AS n, SUM(v) AS total FROM g GROUP BY s ORDER BY s",
             [["a", 3, 8], ["b", 1, 2]])
    x.expect("float keys are not truncated", "SELECT k, COUNT(*) AS n FROM g GROUP BY k ORDER BY k",
             [[1.2, 2], [1.7, 2]])
    x.expect("output follows select-list order", "SELECT COUNT(*) AS n, s FROM g GROUP BY s ORDER BY s",
             [[3, "a"], [1, "b"]], ["n", "s"])
    low = [c[3] for c in CUSTOMERS if c[2] <= 25]
    high = [c[3] for c in CUSTOMERS if c[2] > 25]
    x.expect("expression key with aliased aggregates",
             "SELECT age > 25 AS older, COUNT(*) AS n, AVG(spend) AS avg_spend "
             "FROM customers GROUP BY age > 25 ORDER BY older",
             [[0, len(low), sum(low) / len(low)], [1, len(high), sum(high) / len(high)]],
             ["older", "n", "avg_spend"])
    x.expect("HAVING on an alias", "SELECT s, COUNT(*) AS n FROM g GROUP BY s HAVING n > 1", [["a", 3]])
    x.expect("HAVING on an aggregate", "SELECT s, SUM(v) AS total FROM g GROUP BY s HAVING SUM(v) > 5",
             [["a", 8]])
    x.expect("ORDER BY aggregate alias + LIMIT",
             "SELECT s, SUM(v) AS total FROM g GROUP BY s ORDER BY total DESC LIMIT 1", [["a", 8]])
    x.expect_error("column outside GROUP BY/aggregate (no GROUP BY)", "SELECT name, COUNT(*) FROM customers")
    x.expect_error("column outside GROUP BY/aggregate", "SELECT name, COUNT(*) FROM customers GROUP BY age")


def test_where(x: Runner) -> None:
    x.t.section("WHERE")
    x.expect("string equality", "SELECT name FROM customers WHERE name = 'bob'", [["bob"]])
    x.expect("string inequality", "SELECT COUNT(*) AS n FROM customers WHERE name != 'bob'", [[8]])
    x.expect("no string match", "SELECT name FROM customers WHERE name = 'zzz'", [])
    x.expect("float equality is exact", "SELECT name FROM customers WHERE spend = 20.5", [])
    x.expect("integer-valued float equality", "SELECT name FROM customers WHERE spend = 20", [["bob"]])
    x.expect("AND binds tighter than OR",
             "SELECT name FROM customers WHERE age = 11 OR age = 19 AND spend > 100", [["fay"]])
    x.expect("parentheses override precedence",
             "SELECT name FROM customers WHERE (age = 11 OR age = 19) AND spend > 50", [["fay"]])
    x.expect("arithmetic in WHERE",
             "SELECT name FROM customers WHERE spend * 2 > 50 ORDER BY customer_id",
             [[c[1]] for c in CUSTOMERS if c[3] * 2 > 50])


def test_arithmetic(x: Runner) -> None:
    x.t.section("Arithmetic")
    x.expect("multiplication before addition", "SELECT 2 + 3 * 4 AS v FROM customers LIMIT 1", [[14]])
    x.expect("parentheses", "SELECT (2 + 3) * 4 AS v FROM customers LIMIT 1", [[20]])
    x.expect("division is fractional", "SELECT 7 / 2 AS v FROM customers LIMIT 1", [[3.5]])
    x.expect("modulo", "SELECT age % 5 AS m FROM customers ORDER BY customer_id LIMIT 3",
             [[c[2] % 5] for c in CUSTOMERS[:3]])
    x.expect("unary minus", "SELECT -age AS v FROM customers ORDER BY customer_id LIMIT 1", [[-28]])


def test_order_by(x: Runner) -> None:
    x.t.section("ORDER BY")
    x.expect("strings descending", "SELECT name FROM customers ORDER BY name DESC",
             [[c[1]] for c in sorted(CUSTOMERS, key=lambda c: c[1], reverse=True)])
    exp = sorted(CUSTOMERS, key=lambda c: (-(c[2] > 25), c[2]))
    x.expect("two keys", "SELECT name, age > 25 AS older FROM customers ORDER BY older DESC, age ASC",
             [[c[1], int(c[2] > 25)] for c in exp])
    x.expect("column that is not selected", "SELECT name FROM customers ORDER BY age DESC LIMIT 3",
             [[c[1]] for c in sorted(CUSTOMERS, key=lambda c: -c[2])[:3]])
    x.expect("alias", "SELECT name, spend AS s FROM customers ORDER BY s DESC LIMIT 2",
             [[c[1], c[3]] for c in sorted(CUSTOMERS, key=lambda c: -c[3])[:2]])
    x.expect_error("unknown column", "SELECT name FROM customers ORDER BY nope")


def test_limit(x: Runner) -> None:
    x.t.section("LIMIT / OFFSET")
    ids = [[c[0]] for c in CUSTOMERS]
    base = "SELECT customer_id FROM customers ORDER BY customer_id"
    x.expect("LIMIT n", f"{base} LIMIT 3", ids[:3])
    x.expect("LIMIT n OFFSET m", f"{base} LIMIT 3 OFFSET 2", ids[2:5])
    x.expect("LIMIT offset, count", f"{base} LIMIT 2, 3", ids[2:5])
    x.expect("LIMIT 0 returns no rows", f"{base} LIMIT 0", [])
    x.expect("OFFSET near the end", f"{base} LIMIT 5 OFFSET 7", ids[7:])
    x.expect("OFFSET past the end", f"{base} LIMIT 5 OFFSET 20", [])
    x.expect_error("trailing tokens are rejected", "SELECT name FROM customers WHERE age > 20 BANANA")


def test_quantiles(x: Runner) -> None:
    x.t.section("Quantiles and windows")
    ages = [c[2] for c in CUSTOMERS]
    spends = [c[3] for c in CUSTOMERS]
    x.expect("quantile(p)(x) on int", "SELECT quantile(0.5)(age) AS q FROM customers", [[quantile(ages, 0.5)]])
    x.expect("quantile(p)(x) on float", "SELECT quantile(0.9)(spend) AS q FROM customers",
             [[quantile(spends, 0.9)]])
    x.expect("median", "SELECT median(age) AS m FROM customers", [[quantile(ages, 0.5)]])
    x.expect("PERCENTILE(x, p)", "SELECT PERCENTILE(age, 90) AS p FROM customers", [[quantile(ages, 0.9)]])
    x.expect("quantile per group", "SELECT s, quantile(0.5)(v) AS q FROM g GROUP BY s ORDER BY s",
             [["a", quantile([1, 3, 4], 0.5)], ["b", 2.0]])
    buckets = ntile_buckets(CUSTOMERS, 4)
    x.expect("NTILE with alias",
             "SELECT customer_id, NTILE(4) OVER (ORDER BY age) AS bucket FROM customers ORDER BY customer_id",
             [[c[0], buckets[c[0]]] for c in CUSTOMERS], ["customer_id", "bucket"])


def test_join(x: Runner) -> None:
    x.t.section("JOIN with qualified names")
    for sql in (
        "CREATE TABLE regions (customer_id Int64, region_id Int64)",
        "CREATE TABLE orders (order_id Int64, customer_id Int64, amount Float64)",
        "INSERT INTO regions VALUES (1, 10), (2, 20), (3, 10)",
        "INSERT INTO orders VALUES (1, 1, 99.5), (2, 2, 150.0), (3, 1, 50.0), (4, 3, 5.25)",
    ):
        x.t.check(f"join setup: {sql[:40]}", x.client.query(sql).ok())
    x.expect("qualified GROUP BY and ORDER BY",
             "SELECT r.region_id, SUM(o.amount) AS total, COUNT(*) AS n "
             "FROM regions r JOIN orders o ON r.customer_id = o.customer_id "
             "GROUP BY r.region_id ORDER BY r.region_id",
             [[10, 99.5 + 50.0 + 5.25, 3], [20, 150.0, 1]], ["region_id", "total", "n"])
    x.expect("qualified ORDER BY on a non-aggregate join, descending",
             "SELECT o.order_id FROM regions r JOIN orders o ON r.customer_id = o.customer_id "
             "ORDER BY o.amount DESC LIMIT 2",
             [[2], [1]])
    x.expect_error("ORDER BY an expression is rejected, not ignored",
                   "SELECT name FROM customers ORDER BY age * 2")


def test_errors_and_views(x: Runner) -> None:
    x.t.section("Failures are loud; views keep types")
    x.expect_error("unknown function", "SELECT FOO(age) FROM customers")
    x.expect_error("aggregate in WHERE", "SELECT name FROM customers WHERE SUM(age) > 1")
    r = x.client.query("CREATE VIEW v_avg AS SELECT s, AVG(v) AS a FROM g GROUP BY s")
    x.t.check("CREATE VIEW with aggregate", r.ok(), r.body[:200])
    x.expect("view returns fractional AVG", "SELECT s, a FROM v_avg ORDER BY s",
             [["a", 8 / 3], ["b", 2.0]])


def cleanup(client: Client, t: TestRunner) -> None:
    t.section("Cleanup")
    r = client.query(f"DROP DATABASE IF EXISTS {DB}")
    t.check("drop test database", r.ok(), r.body[:200])


def run_tests(client: Client) -> int:
    t = TestRunner()
    setup(client, t)
    x = Runner(client, t)
    for fn in (test_aliases, test_aggregates, test_group_by, test_where, test_arithmetic,
               test_order_by, test_limit, test_quantiles, test_join, test_errors_and_views):
        fn(x)
    cleanup(client, t)
    return t.summary()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:1143")
    parser.add_argument("--no-launch", action="store_true")
    parser.add_argument("--server-bin", default=None)
    args = parser.parse_args()
    if args.no_launch:
        print(f"{YELLOW}Using existing server at {args.url}{RESET}")
        return run_tests(Client(args.url))
    with ServerProc(Path(args.server_bin) if args.server_bin else None, args.url) as client:
        return run_tests(client)


if __name__ == "__main__":
    sys.exit(main())
