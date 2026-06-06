#!/usr/bin/env python3
"""End-to-end API tests for CONNECTOR support.

Usage:
    python scripts/test_connectors_api.py
    python scripts/test_connectors_api.py --url http://127.0.0.1:1143 --no-launch
    python scripts/test_connectors_api.py --server-bin build/bin/Release/mnemosyne_server.exe

Exit code is 0 if all tests pass, 1 otherwise.
"""

from __future__ import annotations

import argparse
import json
import sys
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

from _mnemo_client import (
    Client,
    ServerProc,
    TestRunner,
    YELLOW,
    RESET,
)

REPO_ROOT = Path(__file__).resolve().parent.parent
REST_CONN = "api_rest"
PG_CONN = "api_pg"
S3_CONN = "api_s3"
MISSING_CONN = "definitely_missing_connector_xyz"

CUSTOMERS_JSON = json.dumps([
    {"id": "1", "name": "Alice", "age": "25"},
    {"id": "2", "name": "Bob", "age": "30"},
    {"id": "3", "name": "Charlie", "age": "35"},
])

ORDERS_JSON = json.dumps([
    {"order_id": "100", "customer_id": "1", "total": "42.50"},
    {"order_id": "101", "customer_id": "2", "total": "18.00"},
])


class MockApiHandler(BaseHTTPRequestHandler):
    def log_message(self, format: str, *args) -> None:  # noqa: A003
        return

    def do_GET(self) -> None:  # noqa: N802
        if self.path in ("/", ""):
            body = b'{"ok": true}'
        elif self.path == "/customers":
            body = CUSTOMERS_JSON.encode("utf-8")
        elif self.path == "/orders":
            body = ORDERS_JSON.encode("utf-8")
        else:
            self.send_response(404)
            self.end_headers()
            return
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


class MockApiServer:
    def __init__(self) -> None:
        self._httpd: HTTPServer | None = None
        self._thread: threading.Thread | None = None
        self.port = 0
        self.base_url = ""

    def start(self) -> str:
        self._httpd = HTTPServer(("127.0.0.1", 0), MockApiHandler)
        self.port = self._httpd.server_address[1]
        self.base_url = f"http://127.0.0.1:{self.port}"
        self._thread = threading.Thread(target=self._httpd.serve_forever, daemon=True)
        self._thread.start()
        return self.base_url

    def stop(self) -> None:
        if self._httpd is not None:
            self._httpd.shutdown()
            self._httpd.server_close()
            self._httpd = None


def _rows(body: str) -> list[dict]:
    try:
        data = json.loads(body)
        cols = data.get("columns", [])
        rows = data.get("data", [])
        return [dict(zip(cols, row)) for row in rows]
    except (json.JSONDecodeError, TypeError):
        return []


def _row_count(body: str) -> int:
    try:
        return int(json.loads(body).get("rows", -1))
    except (json.JSONDecodeError, TypeError, ValueError):
        return -1


def setup(client: Client, t: TestRunner, base_url: str) -> None:
    t.section("Setup")
    for label, sql in [
        ("DROP REST", f"DROP CONNECTOR IF EXISTS {REST_CONN}"),
        ("DROP PG", f"DROP CONNECTOR IF EXISTS {PG_CONN}"),
        ("DROP S3", f"DROP CONNECTOR IF EXISTS {S3_CONN}"),
        (
            "CREATE REST",
            f"CREATE CONNECTOR {REST_CONN} TYPE REST "
            f"BASE_URL '{base_url}' SCHEMA 'customers,orders'",
        ),
        (
            "CREATE POSTGRES",
            f"CREATE CONNECTOR {PG_CONN} TYPE POSTGRES "
            f"HOST 'localhost' PORT 5432 DATABASE 'sales' USER 'u' PASSWORD 'secret'",
        ),
        (
            "CREATE S3",
            f"CREATE CONNECTOR {S3_CONN} TYPE S3 "
            f"BUCKET 'test-bucket' ENDPOINT 'http://127.0.0.1:9000'",
        ),
    ]:
        r = client.query(sql)
        t.check(f"setup: {label}", r.ok(), f"status={r.status} body={r.body[:300]!r}")


def test_show_describe(client: Client, t: TestRunner) -> None:
    t.section("SHOW and DESCRIBE connectors")

    r = client.query("SHOW CONNECTORS")
    t.check("SHOW CONNECTORS returns 200", r.ok(), f"status={r.status} body={r.body!r}")
    body_lower = r.body.lower()
    for name in (REST_CONN, PG_CONN, S3_CONN):
        t.check(f"lists {name}", name in body_lower, f"body={r.body!r}")
    t.check("shows REST type", "rest" in body_lower, f"body={r.body!r}")
    t.check("shows ACTIVE status", "active" in body_lower, f"body={r.body!r}")

    r = client.query(f"DESCRIBE CONNECTOR {REST_CONN}")
    t.check("DESCRIBE CONNECTOR returns 200", r.ok(), f"status={r.status}")
    t.check("includes name field", "name" in r.body.lower(), f"body={r.body!r}")
    t.check("includes type REST", "rest" in r.body.lower(), f"body={r.body!r}")
    t.check(
        "includes SCHEMA_DISCOVERY capability",
        "schema_discovery" in r.body.lower() or "read" in r.body.lower(),
        f"body={r.body!r}",
    )
    t.check("masks secrets if present", "secret" not in r.body.lower(), f"body={r.body!r}")


def test_capabilities(client: Client, t: TestRunner) -> None:
    t.section("SHOW CONNECTOR CAPABILITIES")

    r = client.query(f"SHOW CONNECTOR CAPABILITIES {REST_CONN}")
    t.check("SHOW CONNECTOR CAPABILITIES returns 200", r.ok(), f"status={r.status}")
    body_lower = r.body.lower()
    t.check("includes READ", "read" in body_lower, f"body={r.body!r}")
    t.check(
        "includes SCHEMA_DISCOVERY",
        "schema_discovery" in body_lower,
        f"body={r.body!r}",
    )


def test_connector_test(client: Client, t: TestRunner) -> None:
    t.section("TEST CONNECTOR")

    r = client.query(f"TEST CONNECTOR {REST_CONN}")
    t.check("TEST CONNECTOR returns 200", r.ok(), f"status={r.status} body={r.body!r}")
    t.check("reports ok", "true" in r.body.lower(), f"body={r.body!r}")

    r = client.query(f"SHOW CONNECTOR STATUS {REST_CONN}")
    t.check("SHOW CONNECTOR STATUS returns 200", r.ok(), f"status={r.status}")
    t.check("status shows active", "active" in r.body.lower(), f"body={r.body!r}")


def test_discover_schema(client: Client, t: TestRunner) -> None:
    t.section("DISCOVER SCHEMA FROM CONNECTOR")

    r = client.query(f"DISCOVER SCHEMA FROM CONNECTOR {REST_CONN}")
    t.check("DISCOVER returns 200", r.ok(), f"status={r.status} body={r.body!r}")
    resources = [row.get("resource", "") for row in _rows(r.body)]
    t.check("discovers customers", "customers" in resources, f"resources={resources}")
    t.check("discovers orders", "orders" in resources, f"resources={resources}")


def test_select_from_connector(client: Client, t: TestRunner) -> None:
    t.section("SELECT FROM CONNECTOR")

    r = client.query(f"SELECT * FROM CONNECTOR {REST_CONN}.customers")
    t.check("SELECT FROM CONNECTOR returns 200", r.ok(), f"status={r.status}")
    count = _row_count(r.body)
    t.check("SELECT returns 3 rows", count == 3, f"rows={count} body={r.body!r}")


def test_alter(client: Client, t: TestRunner, base_url: str) -> None:
    t.section("ALTER CONNECTOR")

    r = client.query(
        f"ALTER CONNECTOR {REST_CONN} SET SCHEMA 'customers,orders,updated'"
    )
    t.check("ALTER CONNECTOR returns 200", r.ok(), f"status={r.status} body={r.body!r}")

    r = client.query(f"TEST CONNECTOR {REST_CONN}")
    t.check("re-TEST after ALTER", r.ok(), f"status={r.status}")

    r = client.query(
        f"ALTER CONNECTOR {REST_CONN} SET BASE_URL '{base_url}'"
    )
    t.check("ALTER BASE_URL", r.ok(), f"status={r.status}")


def test_errors(client: Client, t: TestRunner, base_url: str) -> None:
    t.section("Error handling")

    r = client.query(
        f"CREATE CONNECTOR {REST_CONN} TYPE REST BASE_URL '{base_url}'"
    )
    t.check(
        "duplicate CREATE CONNECTOR fails",
        not r.ok(),
        f"status={r.status} body={r.body!r}",
    )

    r = client.query(f"DROP CONNECTOR {MISSING_CONN}")
    t.check(
        "DROP missing connector fails",
        not r.ok(),
        f"status={r.status} body={r.body!r}",
    )

    r = client.query(f"SELECT * FROM CONNECTOR {PG_CONN}.customers")
    t.check(
        "SELECT from POSTGRES connector fails",
        not r.ok(),
        f"status={r.status} body={r.body!r}",
    )


def cleanup(client: Client, t: TestRunner) -> None:
    t.section("Cleanup")

    for name in (REST_CONN, PG_CONN, S3_CONN):
        r = client.query(f"DROP CONNECTOR IF EXISTS {name}")
        t.check(f"DROP {name}", r.ok(), f"status={r.status}")


def run_tests(client: Client, mock: MockApiServer) -> int:
    t = TestRunner()
    base_url = mock.start()
    try:
        setup(client, t, base_url)
        test_show_describe(client, t)
        test_capabilities(client, t)
        test_connector_test(client, t)
        test_discover_schema(client, t)
        test_select_from_connector(client, t)
        test_alter(client, t, base_url)
        test_errors(client, t, base_url)
        cleanup(client, t)
    finally:
        mock.stop()
    return t.summary()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:1143")
    parser.add_argument("--no-launch", action="store_true")
    parser.add_argument("--server-bin", default=None)
    args = parser.parse_args()

    mock = MockApiServer()

    if args.no_launch:
        client = Client(args.url)
        print(f"{YELLOW}Using existing server at {args.url}{RESET}")
        return run_tests(client, mock)

    with ServerProc(
        Path(args.server_bin) if args.server_bin else None,
        args.url,
    ) as client:
        return run_tests(client, mock)


if __name__ == "__main__":
    sys.exit(main())
