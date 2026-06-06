#!/usr/bin/env python3
"""Shared helpers for Mnemosyne API test scripts (stdlib only).

Provides:
  * Client       — thin HTTP client for the /query endpoint and friends.
  * ServerProc   — context manager that launches/​stops mnemosyne_server.
  * TestRunner   — tiny pass/fail harness with a process exit code.

All test scripts in scripts/ import from this module so the launch/teardown
logic and result parsing live in one place.
"""

from __future__ import annotations

import json
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path

try:
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
except (AttributeError, ValueError):
    pass

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_BINS = [
    REPO_ROOT / "build" / "bin" / "Release" / "mnemosyne_server.exe",
    REPO_ROOT / "build" / "bin" / "Debug" / "mnemosyne_server.exe",
    REPO_ROOT / "build" / "bin" / "mnemosyne_server",
]

GREEN, RED, YELLOW, DIM, CYAN, RESET = (
    "\033[32m", "\033[31m", "\033[33m", "\033[2m", "\033[36m", "\033[0m",
)


@dataclass
class HttpResult:
    status: int
    body: str

    def json(self) -> dict:
        return json.loads(self.body)

    def ok(self) -> bool:
        return 200 <= self.status < 300


class Client:
    def __init__(self, base_url: str = "http://127.0.0.1:1143"):
        self.base_url = base_url.rstrip("/")

    def get(self, path: str, params: dict | None = None, timeout: float = 30.0) -> HttpResult:
        url = self.base_url + path
        if params:
            url += "?" + urllib.parse.urlencode(params)
        return self._send(urllib.request.Request(url, method="GET"), timeout)

    def query(self, sql: str, fmt: str = "JSON", timeout: float = 30.0) -> HttpResult:
        return self.get("/query", {"query": sql, "format": fmt}, timeout)

    @staticmethod
    def _send(req: urllib.request.Request, timeout: float) -> HttpResult:
        try:
            with urllib.request.urlopen(req, timeout=timeout) as resp:
                return HttpResult(resp.status, resp.read().decode("utf-8", "replace"))
        except urllib.error.HTTPError as e:
            return HttpResult(e.code, e.read().decode("utf-8", "replace"))


def find_server_bin(explicit: str | None = None) -> Path | None:
    if explicit:
        p = Path(explicit)
        return p if p.exists() else None
    for cand in DEFAULT_BINS:
        if cand.exists():
            return cand
    return None


class ServerProc:
    """Launch mnemosyne_server and wait until /ping responds."""

    def __init__(self, bin_path: Path | None = None, base_url: str = "http://127.0.0.1:1143"):
        self.bin_path = bin_path or find_server_bin()
        self.base_url = base_url
        self.proc: subprocess.Popen | None = None
        self.client = Client(base_url)

    def __enter__(self) -> Client:
        if self.bin_path is None or not self.bin_path.exists():
            raise FileNotFoundError(
                "mnemosyne_server binary not found; build it first "
                f"(looked for {[str(p) for p in DEFAULT_BINS]})"
            )
        print(f"{YELLOW}Launching {self.bin_path}{RESET}")
        self.proc = subprocess.Popen(
            [str(self.bin_path)],
            cwd=str(REPO_ROOT),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        if not self._wait_ready(20.0):
            self.__exit__(None, None, None)
            raise RuntimeError("server did not become ready in time")
        print(f"{GREEN}Server ready at {self.base_url}{RESET}")
        return self.client

    def _wait_ready(self, timeout_s: float) -> bool:
        deadline = time.time() + timeout_s
        while time.time() < deadline:
            if self.proc and self.proc.poll() is not None:
                return False  # process died
            try:
                if self.client.get("/ping", timeout=1.0).status == 200:
                    return True
            except (urllib.error.URLError, ConnectionError, OSError):
                pass
            time.sleep(0.3)
        return False

    def __exit__(self, *exc) -> None:
        if self.proc is not None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.proc.kill()
            self.proc = None


@dataclass
class TestRunner:
    passed: int = 0
    failed: int = 0
    failures: list = field(default_factory=list)

    def check(self, name: str, condition: bool, detail: str = "") -> bool:
        if condition:
            self.passed += 1
            print(f"  {GREEN}PASS{RESET} {name}")
        else:
            self.failed += 1
            self.failures.append(name)
            print(f"  {RED}FAIL{RESET} {name}")
            if detail:
                print(f"       {DIM}{detail}{RESET}")
        return condition

    def section(self, title: str) -> None:
        print(f"\n{CYAN}== {title} =={RESET}")

    def summary(self) -> int:
        total = self.passed + self.failed
        color = GREEN if self.failed == 0 else RED
        print(f"\n{color}{self.passed}/{total} checks passed{RESET}")
        if self.failures:
            print(f"{RED}Failed: {', '.join(self.failures)}{RESET}")
        return 0 if self.failed == 0 else 1
