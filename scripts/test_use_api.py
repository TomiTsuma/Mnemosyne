"""End-to-end API tests for the Mnemosyne server, focused on the USE statement.

This script exercises the HTTP API of ``mnemosyne_server`` (default port 1143).
It can either launch the server itself (default) or talk to an already-running
instance via ``--url``.

The USE statement sets the *session's current database* on the server's
persistent ``Context``. Because the HTTP server keeps a single long-lived
context shared across requests, ``USE`` is observable across requests through
the ``/metrics`` endpoint, which reports ``"current_database": "<name>"``.

Usage:
    python scripts/test_use_api.py                 # launch server, run tests
    python scripts/test_use_api.py --url http://127.0.0.1:1143 --no-launch
    python scripts/test_use_api.py --server-bin build/bin/Debug/mnemosyne_server.exe

Exit code is 0 if all tests pass, 1 otherwise.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path

import requests
from requests.exceptions import RequestException

# Force UTF-8 output so ANSI/section markers render on Windows consoles (cp1252).
try:
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
except (AttributeError, ValueError):
    pass

REPO_ROOT = Path(__file__).resolve().parent.parent

DEFAULT_BINS = [
    REPO_ROOT / "build" / "bin" / "Release" / "mnemosyne_server.exe",
    REPO_ROOT / "build" / "bin" / "Debug" / "mnemosyne_server.exe",
    REPO_ROOT / "build" / "bin" / "mnemosyne_server",  # non-Windows
]

# == Tiny ANSI helpers (no external deps) ==
GREEN = "\033[32m"
RED = "\033[31m"
YELLOW = "\033[33m"
DIM = "\033[2m"
RESET = "\033[0m"


@dataclass
class HttpResult:
    status: int
    body: str

    def json(self) -> dict:
        return json.loads(self.body)


# == HTTP client (requests) ==
class Client:
    def __init__(self, base_url: str):
        self.base_url = base_url.rstrip("/")
        self.session = requests.Session()

    def get(
        self,
        path: str,
        params: dict | None = None,
        timeout: float = 10.0,
    ) -> HttpResult:
        response = self.session.get(
            f"{self.base_url}{path}",
            params=params,
            timeout=timeout,
        )

        return HttpResult(
            status=response.status_code,
            body=response.text,
        )

    def query(
        self,
        sql: str,
        fmt: str = "JSON",
        timeout: float = 10.0,
    ) -> HttpResult:
        """Run a SQL statement through the /query endpoint."""
        return self.get(
            "/query",
            {"query": sql, "format": fmt},
            timeout,
        )


# == Test harness ==
class TestRunner:
    def __init__(self) -> None:
        self.passed = 0
        self.failed = 0

    def check(self, name: str, condition: bool, detail: str = "") -> None:
        if condition:
            self.passed += 1
            print(f"  {GREEN}PASS{RESET} {name}")
        else:
            self.failed += 1
            print(f"  {RED}FAIL{RESET} {name}")
            if detail:
                print(f"       {DIM}{detail}{RESET}")

    def summary(self) -> int:
        total = self.passed + self.failed
        color = GREEN if self.failed == 0 else RED
        print(f"\n{color}{self.passed}/{total} checks passed{RESET}")
        return 0 if self.failed == 0 else 1


def wait_for_server(client: Client, timeout_s: float = 20.0) -> bool:
    deadline = time.time() + timeout_s

    while time.time() < deadline:
        try:
            r = client.get("/ping", timeout=1.0)
            if r.status == 200:
                return True

        except (RequestException, ConnectionError, OSError):
            pass

        time.sleep(0.3)

    return False


def current_database(client: Client) -> str:
    """Read the session's current database from the /metrics JSON endpoint."""
    r = client.get("/metrics")

    try:
        return r.json().get("current_database", "<missing>")
    except (json.JSONDecodeError, ValueError):
        return "<unparseable: " + r.body[:80] + ">"


def run_tests(client: Client) -> int:
    t = TestRunner()
    db = "analytics_demo"

    print("\n== Connectivity ==")

    ping = client.get("/ping")
    t.check(
        "GET /ping returns 200",
        ping.status == 200,
        f"status={ping.status}",
    )

    t.check(
        "GET /ping body is 'Ok.'",
        ping.body.strip() == "Ok.",
        f"body={ping.body!r}",
    )

    metrics = client.get("/metrics")

    t.check(
        "GET /metrics returns 200",
        metrics.status == 200,
        f"status={metrics.status}",
    )

    t.check(
        "GET /metrics reports a current_database field",
        "current_database" in metrics.body,
        f"body={metrics.body!r}",
    )

    print("\n== Setup: create a target database ==")

    create = client.query(f"CREATE DATABASE {db}")

    t.check(
        "CREATE DATABASE returns 200",
        create.status == 200,
        f"status={create.status} body={create.body!r}",
    )

    dbs = client.get("/databases")

    t.check(
        f"/databases now lists '{db}'",
        db in dbs.body,
        f"body={dbs.body!r}",
    )

    print("\n== USE <database> ==")

    use = client.query(f"USE {db}")

    t.check(
        "USE <db> returns 200",
        use.status == 200,
        f"status={use.status} body={use.body!r}",
    )

    current = current_database(client)

    t.check(
        f"current database is '{db}' after USE",
        current == db,
        f"current={current!r}",
    )

    print("\n== USE DATABASE <database> (keyword variant) ==")

    client.query("CREATE DATABASE other_db")
    client.query("USE other_db")

    t.check(
        "switched away to 'other_db'",
        current_database(client) == "other_db",
    )

    use_kw = client.query(f"USE DATABASE {db}")

    t.check(
        "USE DATABASE <db> returns 200",
        use_kw.status == 200,
        f"status={use_kw.status} body={use_kw.body!r}",
    )

    current = current_database(client)

    t.check(
        f"current database is '{db}' after USE DATABASE",
        current == db,
        f"current={current!r}",
    )

    print("\n== Session persistence across requests ==")

    current = current_database(client)

    t.check(
        "current database persists across separate HTTP requests",
        current == db,
        f"current={current!r}",
    )

    print("\n== Error handling: USE of a non-existent database ==")

    bad = client.query("USE definitely_not_a_database")

    t.check(
        "USE <missing db> returns an error status (>=400)",
        bad.status >= 400,
        f"status={bad.status} body={bad.body!r}",
    )

    t.check(
        "error message mentions the unknown database",
        "Unknown database" in bad.body
        or "definitely_not_a_database" in bad.body,
        f"body={bad.body!r}",
    )

    t.check(
        "failed USE does not change the current database",
        current_database(client) == db,
    )

    return t.summary()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument(
        "--url",
        default="http://127.0.0.1:1143",
        help="Base URL of the server",
    )

    parser.add_argument(
        "--no-launch",
        action="store_true",
        help="Do not start the server; use a running one",
    )

    parser.add_argument(
        "--server-bin",
        default=None,
        help="Path to mnemosyne_server executable",
    )

    parser.add_argument(
        "--startup-timeout",
        type=float,
        default=20.0,
        help="Seconds to wait for /ping",
    )

    args = parser.parse_args()

    client = Client(args.url)
    proc: subprocess.Popen | None = None

    try:
        if not args.no_launch:
            bin_path = Path(args.server_bin) if args.server_bin else None

            if bin_path is None:
                for candidate in DEFAULT_BINS:
                    if candidate.exists():
                        bin_path = candidate
                        break

            if bin_path is None or not bin_path.exists():
                print(
                    f"{RED}Server binary not found.{RESET} "
                    "Build it first or pass --server-bin."
                )
                print(
                    f"  Looked for: {[str(p) for p in DEFAULT_BINS]}"
                )
                return 2

            print(f"{YELLOW}Launching server:{RESET} {bin_path}")

            proc = subprocess.Popen(
                [str(bin_path)],
                cwd=str(REPO_ROOT),
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )

        print(f"{YELLOW}Waiting for server at {args.url} ...{RESET}")

        if not wait_for_server(client, args.startup_timeout):
            print(
                f"{RED}Server did not become ready within "
                f"{args.startup_timeout}s.{RESET}"
            )
            return 2

        print(f"{GREEN}Server is ready.{RESET}")

        return run_tests(client)

    finally:
        if proc is not None:
            print(
                f"\n{YELLOW}Shutting down server "
                f"(pid {proc.pid})...{RESET}"
            )

            proc.terminate()

            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()


if __name__ == "__main__":
    sys.exit(main())