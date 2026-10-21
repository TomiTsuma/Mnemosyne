#!/usr/bin/env python3.11
"""End-to-end API tests for EXPORT TABLE ... TO CONNECTOR (docs/PRDS/CONNECTOR_.md, section 11).

Covers:
  * APPEND (POST) and OVERWRITE (PUT) to a REST connector, the default mode and resource
  * exporting a table, a view, a materialized view and a database-qualified table
  * the request actually sent: method, path, content type, API key, and the JSON body (typed
    values, quote escaping)
  * a round trip: exported rows read back through INSERT ... SELECT FROM CONNECTOR
  * empty tables, remote errors, unreachable servers, unsupported modes and connector types,
    and syntax errors

Usage:
    python3.11 scripts/test_export_api.py --no-launch
    python3.11 scripts/test_export_api.py --url http://127.0.0.1:1143 --no-launch

The server must already be running. Exit code is 0 if all tests pass, 1 otherwise.
"""

from __future__ import annotations

import argparse
import json
import sys
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

from _mnemo_client import Client, ServerProc, TestRunner, YELLOW, RESET

DB = "export_db"
API = "export_api"
KEYED = "export_keyed"
DEAD = "export_dead"
PG = "export_pg"
S3 = "export_s3"
API_KEY = "k-123"

ORDERS = [
    (1, 'Ann "A" Lee', 42.5, 1),
    (2, "Bob", 18.0, 0),
    (3, "Cy", -5.25, 1),
    (4, "Di", 7.0, 0),
]


class MockRest:
    """A REST resource store: POST appends, PUT replaces, GET returns the stored rows."""

    def __init__(self) -> None:
        self.store: dict[str, list] = {}
        self.requests: list[dict] = []
        self.lock = threading.Lock()
        self._httpd: HTTPServer | None = None
        mock = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, format: str, *args) -> None:  # noqa: A003
                return

            def _reply(self, status: int, payload) -> None:
                body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
                self.send_response(status)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def do_GET(self) -> None:  # noqa: N802
                with mock.lock:
                    self._reply(200, mock.store.get(self.path, []))

            def _write(self, method: str) -> None:
                length = int(self.headers.get("Content-Length", "0"))
                raw = self.rfile.read(length).decode("utf-8")
                with mock.lock:
                    mock.requests.append({
                        "method": method, "path": self.path, "raw": raw,
                        "content_type": self.headers.get("Content-Type", ""),
                        "api_key": self.headers.get("X-API-Key", ""),
                    })
                    if self.path == "/rejects":
                        self._reply(503, {"error": "unavailable"})
                        return
                    rows = json.loads(raw)
                    if method == "POST":
                        mock.store.setdefault(self.path, []).extend(rows)
                    else:
                        mock.store[self.path] = rows
                    self._reply(200, {"ok": True})

            def do_POST(self) -> None:  # noqa: N802
                self._write("POST")

            def do_PUT(self) -> None:  # noqa: N802
                self._write("PUT")

        self._handler = Handler

    def start(self) -> str:
        self._httpd = HTTPServer(("127.0.0.1", 0), self._handler)
        threading.Thread(target=self._httpd.serve_forever, daemon=True).start()
        return f"http://127.0.0.1:{self._httpd.server_address[1]}"

    def stop(self) -> None:
        if self._httpd is not None:
            self._httpd.shutdown()
            self._httpd.server_close()

    def count(self) -> int:
        with self.lock:
            return len(self.requests)

    def last(self) -> dict:
        with self.lock:
            return self.requests[-1] if self.requests else {}

    def body(self) -> list:
        return json.loads(self.last().get("raw", "[]"))


def _rows(body: str) -> list[dict]:
    try:
        parsed = json.loads(body)
        return [dict(zip(parsed.get("columns", []), row)) for row in parsed.get("data", [])]
    except (json.JSONDecodeError, TypeError):
        return []


def by_id(rows: list[dict]) -> list[dict]:
    return sorted(rows, key=lambda row: int(row["order_id"]))


class Ctx:
    def __init__(self, client: Client, t: TestRunner, mock: MockRest) -> None:
        self.client, self.t, self.mock = client, t, mock

    def ok(self, name: str, sql: str) -> bool:
        r = self.client.query(sql)
        return self.t.check(name, r.ok(), f"status={r.status} body={r.body[:300]!r}")

    def fails(self, name: str, sql: str, needle: str = "") -> bool:
        r = self.client.query(sql)
        good = (not r.ok()) and needle.lower() in r.body.lower()
        return self.t.check(name, good, f"status={r.status} body={r.body[:300]!r}")

    def rows(self, sql: str) -> list[dict]:
        return _rows(self.client.query(sql).body)

    def export(self, sql: str) -> dict:
        rows = self.rows(sql)
        return rows[0] if rows else {}


def expected_rows() -> list[dict]:
    return [{"order_id": o, "customer": c, "total": t, "paid": p} for o, c, t, p in ORDERS]


def setup(x: Ctx, base_url: str) -> None:
    x.t.section("Setup")
    for label, sql in [
        ("drop db", f"DROP DATABASE IF EXISTS {DB}"),
        ("create db", f"CREATE DATABASE {DB}"),
        ("use", f"USE {DB}"),
        ("orders", "CREATE TABLE orders (order_id Int64, customer String, total Float64, paid Int64)"),
        ("nothing", "CREATE TABLE nothing (order_id Int64, customer String, total Float64, paid Int64)"),
        ("big view", "CREATE VIEW big_orders AS SELECT order_id, total FROM orders WHERE total > 10"),
        ("summary view", "CREATE MATERIALIZED VIEW order_summary AS SELECT COUNT(*) AS n FROM orders"),
    ]:
        x.ok(f"setup: {label}", sql)
    values = ", ".join(f"({o}, '{c}', {t}, {p})" for o, c, t, p in ORDERS)
    x.ok("setup: insert orders", f"INSERT INTO orders VALUES {values}")
    for conn in (API, KEYED, DEAD, PG, S3):
        x.ok(f"drop leftover connector {conn}", f"DROP CONNECTOR IF EXISTS {conn}")
    x.ok("setup: REST connector", f"CREATE CONNECTOR {API} TYPE REST BASE_URL '{base_url}'")
    x.ok("setup: REST connector with an API key",
         f"CREATE CONNECTOR {KEYED} TYPE REST BASE_URL '{base_url}' AUTH API_KEY API_KEY '{API_KEY}'")
    x.ok("setup: unreachable REST connector", f"CREATE CONNECTOR {DEAD} TYPE REST BASE_URL 'http://127.0.0.1:1'")
    x.ok("setup: POSTGRES connector",
         f"CREATE CONNECTOR {PG} TYPE POSTGRES HOST 'localhost' PORT 5432 DATABASE 'd' USER 'u' PASSWORD 'p'")
    x.ok("setup: S3 connector", f"CREATE CONNECTOR {S3} TYPE S3 BUCKET 'b' ENDPOINT 'http://127.0.0.1:9000'")


def test_append_and_overwrite(x: Ctx) -> None:
    x.t.section("EXPORT: APPEND and OVERWRITE")
    x.t.check("REST connectors now advertise WRITE",
              "WRITE" in x.client.query(f"SHOW CONNECTOR CAPABILITIES {API}").body)

    before = x.mock.count()
    result = x.export(f"EXPORT TABLE orders TO CONNECTOR {API}")
    x.t.check("the default is APPEND to a resource named after the table",
              result == {"connector": API, "resource": "orders", "mode": "APPEND", "rows": 4}, f"result={result}")
    x.t.check("exactly one request was sent", x.mock.count() == before + 1)
    sent = x.mock.last()
    x.t.check("APPEND is a POST to /orders as JSON",
              sent.get("method") == "POST" and sent.get("path") == "/orders"
              and sent.get("content_type", "").startswith("application/json"), f"sent={sent}")
    x.t.check("the body holds every row with its types, quotes intact",
              by_id(x.mock.body()) == expected_rows(), f"body={x.mock.body()}")
    x.t.check("integers are JSON numbers and strings are JSON strings",
              isinstance(x.mock.body()[0]["order_id"], int) and isinstance(x.mock.body()[0]["customer"], str))

    x.export(f"EXPORT TABLE orders TO CONNECTOR {API}")
    x.t.check("APPENDing again adds to the resource", len(x.mock.store["/orders"]) == 8)

    result = x.export(f"EXPORT TABLE orders TO CONNECTOR {API} MODE OVERWRITE")
    x.t.check("OVERWRITE reports its mode", result.get("mode") == "OVERWRITE" and result.get("rows") == 4)
    sent = x.mock.last()
    x.t.check("OVERWRITE is a PUT to the same resource", sent.get("method") == "PUT" and sent.get("path") == "/orders")
    x.t.check("and replaces what was there", len(x.mock.store["/orders"]) == 4)

    result = x.export(f"EXPORT TABLE orders TO CONNECTOR {API} MODE append")
    x.t.check("the mode is case-insensitive", result.get("mode") == "APPEND" and len(x.mock.store["/orders"]) == 8)

    result = x.export(f"EXPORT TABLE orders TO CONNECTOR {API}.archive")
    x.t.check("TO CONNECTOR name.resource picks the resource",
              result.get("resource") == "archive" and x.mock.last().get("path") == "/archive")


def test_sources(x: Ctx) -> None:
    x.t.section("EXPORT: views, materialized views and qualified names")
    result = x.export(f"EXPORT TABLE big_orders TO CONNECTOR {API}")
    x.t.check("a view can be exported", result.get("rows") == 2 and result.get("resource") == "big_orders", f"result={result}")
    x.t.check("with only its own columns",
              sorted(x.mock.body(), key=lambda r: r["order_id"]) == [{"order_id": 1, "total": 42.5}, {"order_id": 2, "total": 18}],
              f"body={x.mock.body()}")

    # The view was created before the rows were inserted and has not been refreshed, so what it
    # holds, and therefore what is exported, is its stale contents: EXPORT reads the stored data,
    # it does not re-run the view's query.
    result = x.export(f"EXPORT TABLE order_summary TO CONNECTOR {API}")
    x.t.check("a materialized view exports what it currently holds",
              result.get("rows") == 1 and x.mock.body() == [{"n": 0}],
              f"result={result} body={x.mock.body()}")
    x.ok("refresh it", "REFRESH MATERIALIZED VIEW order_summary")
    x.export(f"EXPORT TABLE order_summary TO CONNECTOR {API}")
    x.t.check("and the refreshed contents once refreshed", x.mock.body() == [{"n": 4}],
              f"body={x.mock.body()}")

    result = x.export(f"EXPORT TABLE {DB}.orders TO CONNECTOR {API}.qualified")
    x.t.check("a database-qualified table can be exported", result.get("rows") == 4, f"result={result}")
    result = x.export(f"EXPORT TABLE {DB}.orders TO CONNECTOR {API}")
    x.t.check("and is named after the table alone by default", result.get("resource") == "orders", f"result={result}")


def test_empty_table(x: Ctx) -> None:
    x.t.section("EXPORT: an empty table")
    before = x.mock.count()
    result = x.export(f"EXPORT TABLE nothing TO CONNECTOR {API}")
    x.t.check("APPENDing nothing exports 0 rows", result.get("rows") == 0, f"result={result}")
    x.t.check("and sends no request", x.mock.count() == before)

    result = x.export(f"EXPORT TABLE nothing TO CONNECTOR {API} MODE OVERWRITE")
    x.t.check("OVERWRITE with nothing still replaces the resource", result.get("rows") == 0)
    x.t.check("with an empty array", x.mock.last().get("method") == "PUT" and x.mock.body() == []
              and x.mock.store.get("/nothing") == [])


def test_auth(x: Ctx) -> None:
    x.t.section("EXPORT: authentication")
    x.export(f"EXPORT TABLE orders TO CONNECTOR {KEYED}.keyed")
    x.t.check("the API key is sent", x.mock.last().get("api_key") == API_KEY, f"sent={x.mock.last()}")
    x.export(f"EXPORT TABLE orders TO CONNECTOR {API}.unkeyed")
    x.t.check("and only when the connector has one", x.mock.last().get("api_key") == "")


def test_round_trip(x: Ctx) -> None:
    x.t.section("EXPORT: round trip through INSERT ... SELECT FROM CONNECTOR")
    x.export(f"EXPORT TABLE orders TO CONNECTOR {API}.rt MODE OVERWRITE")
    x.export(f"EXPORT TABLE orders TO CONNECTOR {API}.rt")
    x.t.check("the remote resource holds both exports", len(x.mock.store["/rt"]) == 8)
    x.export(f"EXPORT TABLE orders TO CONNECTOR {API}.rt MODE OVERWRITE")
    x.t.check("OVERWRITE leaves exactly one copy", len(x.mock.store["/rt"]) == 4)

    x.ok("a table to load into", "CREATE TABLE orders_copy (order_id Int64, customer String, total Float64, paid Int64)")
    x.ok("read the export back",
         f"INSERT INTO orders_copy SELECT order_id, customer, total, paid FROM CONNECTOR {API}.rt")
    back = by_id(x.rows("SELECT order_id, customer, total, paid FROM orders_copy ORDER BY order_id"))
    x.t.check("every row and value survives, including the quotes", back == expected_rows(), f"back={back}")


def test_errors(x: Ctx) -> None:
    x.t.section("EXPORT: errors leave nothing behind")
    before = x.mock.count()
    x.fails("unknown connector", "EXPORT TABLE orders TO CONNECTOR export_no_such", "export_no_such")
    x.fails("unknown table", f"EXPORT TABLE no_such_table TO CONNECTOR {API}", "no_such_table")
    x.fails("MERGE is not supported yet", f"EXPORT TABLE orders TO CONNECTOR {API} MODE MERGE", "not supported yet")
    x.fails("UPSERT is not supported yet", f"EXPORT TABLE orders TO CONNECTOR {API} MODE UPSERT", "not supported yet")
    x.fails("an unknown mode", f"EXPORT TABLE orders TO CONNECTOR {API} MODE BOGUS", "Unknown EXPORT MODE")
    x.t.check("none of those sent a request", x.mock.count() == before)

    x.fails("a remote error is reported with its status",
            f"EXPORT TABLE orders TO CONNECTOR {API}.rejects", "HTTP 503")
    x.t.check("and the remote store was not changed", "/rejects" not in x.mock.store)
    x.fails("an unreachable server", f"EXPORT TABLE orders TO CONNECTOR {DEAD}", "request failed")
    x.fails("POSTGRES is not implemented", f"EXPORT TABLE orders TO CONNECTOR {PG}", "not implemented")
    x.fails("S3 is not implemented", f"EXPORT TABLE orders TO CONNECTOR {S3}", "not implemented")

    x.fails("TABLE is required", f"EXPORT orders TO CONNECTOR {API}", "TABLE")
    x.fails("TO is required", f"EXPORT TABLE orders CONNECTOR {API}", "TO")
    x.fails("CONNECTOR is required", f"EXPORT TABLE orders TO {API}", "CONNECTOR")
    x.fails("MODE needs a word", f"EXPORT TABLE orders TO CONNECTOR {API} MODE", "after MODE")
    x.fails("trailing tokens", f"EXPORT TABLE orders TO CONNECTOR {API} MODE APPEND extra", "unexpected")
    x.t.check("export is not a reserved word",
              x.client.query("CREATE TABLE export_words (export Int64)").ok()
              and x.client.query("INSERT INTO export_words VALUES (1)").ok()
              and x.rows("SELECT export FROM export_words") == [{"export": 1}])


def cleanup(x: Ctx) -> None:
    x.t.section("Cleanup")
    for conn in (API, KEYED, DEAD, PG, S3):
        x.ok(f"drop connector {conn}", f"DROP CONNECTOR IF EXISTS {conn}")
    x.ok("drop database", f"DROP DATABASE IF EXISTS {DB}")


def run_tests(client: Client, mock: MockRest, base_url: str) -> int:
    t = TestRunner()
    x = Ctx(client, t, mock)
    setup(x, base_url)
    test_append_and_overwrite(x)
    test_sources(x)
    test_empty_table(x)
    test_auth(x)
    test_round_trip(x)
    test_errors(x)
    cleanup(x)
    return t.summary()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:1143")
    parser.add_argument("--no-launch", action="store_true")
    parser.add_argument("--server-bin", default=None)
    args = parser.parse_args()

    mock = MockRest()
    base_url = mock.start()
    try:
        if args.no_launch:
            print(f"{YELLOW}Using existing server at {args.url}{RESET}")
            return run_tests(Client(args.url), mock, base_url)
        with ServerProc(Path(args.server_bin) if args.server_bin else None, args.url) as client:
            return run_tests(client, mock, base_url)
    finally:
        mock.stop()


if __name__ == "__main__":
    sys.exit(main())
