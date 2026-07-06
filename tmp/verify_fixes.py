#!/usr/bin/env python3
"""Standalone verification for the three bug fixes (alias, LIMIT, File append)."""
import json
import subprocess
import sys
import tempfile
import time
import urllib.request
import urllib.parse
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
SERVER_BIN = REPO_ROOT / "build" / "bin" / "Debug" / "mnemosyne_server.exe"
BASE_URL = "http://127.0.0.1:1143"


def query(sql, timeout=10.0):
    url = BASE_URL + "/query?" + urllib.parse.urlencode({"query": sql, "format": "JSON"})
    try:
        with urllib.request.urlopen(url, timeout=timeout) as resp:
            return resp.status, resp.read().decode("utf-8", "replace")
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode("utf-8", "replace")


def wait_ready(proc, timeout_s=20.0):
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        if proc.poll() is not None:
            return False
        try:
            with urllib.request.urlopen(BASE_URL + "/ping", timeout=1.0) as resp:
                if resp.status == 200:
                    return True
        except Exception:
            pass
        time.sleep(0.3)
    return False


def main():
    passed, failed = 0, 0

    def check(name, cond, detail=""):
        nonlocal passed, failed
        if cond:
            passed += 1
            print(f"PASS {name}")
        else:
            failed += 1
            print(f"FAIL {name} {detail}")

    proc = subprocess.Popen([str(SERVER_BIN)], cwd=str(REPO_ROOT),
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        if not wait_ready(proc):
            print("server did not become ready")
            sys.exit(1)

        # Setup database + table
        print("create db:", query("CREATE DATABASE IF NOT EXISTS verify_db"))
        print("use db:", query("USE verify_db"))
        status, body = query(
            "CREATE TABLE customers (customer_id Int64, name String, age Int64) "
            "ENGINE=Memory"
        )
        print("create table:", status, body[:200])

        query("INSERT INTO customers VALUES (1, 'Alice', 28)")
        query("INSERT INTO customers VALUES (2, 'Bob', 22)")
        query("INSERT INTO customers VALUES (3, 'Carl', 27)")
        query("INSERT INTO customers VALUES (4, 'Dana', 21)")
        query("INSERT INTO customers VALUES (5, 'Eve', 35)")

        # --- Bug 1: alias on aggregate ---
        status, body = query("SELECT COUNT(name) as total_customers FROM customers")
        print("alias query:", status, body)
        try:
            data = json.loads(body)
            cols = data.get("columns", [])
            check("alias applied to COUNT(...) AS total_customers", cols == ["total_customers"], f"columns={cols}")
        except Exception as e:
            check("alias applied to COUNT(...) AS total_customers", False, str(e))

        # --- Bug 2: LIMIT ---
        status, body = query("SELECT customer_id, age FROM customers LIMIT 3")
        print("limit query:", status, body)
        try:
            data = json.loads(body)
            check("LIMIT 3 returns 3 rows", data.get("rows") == 3 and len(data.get("data", [])) == 3,
                  f"rows={data.get('rows')}")
        except Exception as e:
            check("LIMIT 3 returns 3 rows", False, str(e))

        # --- Bug 4: File engine append ---
        tmp_dir = tempfile.mkdtemp(prefix="mnemo_local_")
        tmp_path = tmp_dir.replace("\\", "/")
        query(f"CREATE STORAGE_UNIT local_unit TYPE LOCAL PATH '{tmp_path}'")
        status, body = query(
            "CREATE TABLE appendtest (id Float64, val Float64) Engine=File STORAGE_UNIT local_unit"
        )
        print("create appendtest:", status, body[:200])
        query("INSERT INTO appendtest VALUES (1.0, 10.5), (2.0, 20.5), (3.0, 30.5)")
        query("INSERT INTO appendtest VALUES (15.0, 10.5), (12.0, 21.5), (33.0, 310.53)")
        status, body = query("SELECT * FROM appendtest")
        print("select appendtest:", status, body)
        try:
            data = json.loads(body)
            check("File engine appends rather than overwrites", data.get("rows") == 6, f"rows={data.get('rows')}")
        except Exception as e:
            check("File engine appends rather than overwrites", False, str(e))

    finally:
        proc.terminate()
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()

    print(f"\n{passed}/{passed+failed} checks passed")
    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()
