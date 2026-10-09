#!/usr/bin/env python3
"""End-to-end API tests for the default File engine and database-level DDL.

Covers the behaviour added on the web-UI branch:
  * CREATE TABLE without ENGINE now defaults to the File engine (su_default).
  * INSERT appends to existing rows instead of overwriting them.
  * Float64, negative numbers and String columns round-trip through disk.
  * Cross-database qualified names (db.table) in CREATE / INSERT / SELECT.
  * DROP DATABASE [IF EXISTS].

Usage:
    python3.11 scripts/test_file_engine_api.py --no-launch
    python3.11 scripts/test_file_engine_api.py --url http://127.0.0.1:1143 --no-launch

The server must already be running (ServerProc no longer launches it).
Run the server from a scratch directory so ./data is not created in the repo.

Exit code is 0 if all tests pass, 1 otherwise.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from _mnemo_client import (
    Client,
    ServerProc,
    TestRunner,
    YELLOW,
    RESET,
)

DB = "file_engine_test_db"
DB_OTHER = "file_engine_other_db"
MISSING_DB = "definitely_missing_db_xyz"


def _data(body: str) -> list[list]:
    try:
        return json.loads(body).get("data", [])
    except (json.JSONDecodeError, TypeError):
        return []


def _scalar(client: Client, sql: str):
    rows = _data(client.query(sql).body)
    return rows[0][0] if rows and rows[0] else None


def setup(client: Client, t: TestRunner) -> None:
    t.section("Setup")
    for label, sql in [
        ("DROP DATABASE main", f"DROP DATABASE IF EXISTS {DB}"),
        ("DROP DATABASE other", f"DROP DATABASE IF EXISTS {DB_OTHER}"),
        ("CREATE DATABASE main", f"CREATE DATABASE {DB}"),
        ("CREATE DATABASE other", f"CREATE DATABASE {DB_OTHER}"),
        ("USE", f"USE {DB}"),
    ]:
        r = client.query(sql)
        t.check(f"setup: {label}", r.ok(), f"status={r.status} body={r.body[:300]!r}")


def test_default_engine_and_append(client: Client, t: TestRunner) -> None:
    t.section("Default engine + append")
    r = client.query("CREATE TABLE readings (id Int64, val Float64, label String)")
    t.check("CREATE TABLE without ENGINE succeeds", r.ok(), r.body[:300])

    r = client.query("INSERT INTO readings VALUES (1, 1.5, 'a'), (2, 2.5, 'b')")
    t.check("first INSERT ok", r.ok(), r.body[:300])
    r = client.query("INSERT INTO readings VALUES (3, 3.5, 'c'), (4, 4.5, 'd')")
    t.check("second INSERT ok", r.ok(), r.body[:300])

    t.check("INSERTs append (4 rows, not 2)", _scalar(client, "SELECT COUNT(*) FROM readings") == 4,
            f"count={_scalar(client, 'SELECT COUNT(*) FROM readings')}")
    t.check("SUM(val) keeps fractional part (12.0)", _scalar(client, "SELECT SUM(val) FROM readings") == 12.0,
            f"sum={_scalar(client, 'SELECT SUM(val) FROM readings')}")

    rows = _data(client.query("SELECT id, val, label FROM readings").body)
    t.check("row order and values preserved",
            rows == [[1, 1.5, "a"], [2, 2.5, "b"], [3, 3.5, "c"], [4, 4.5, "d"]],
            f"rows={rows}")


def test_value_roundtrip(client: Client, t: TestRunner) -> None:
    t.section("Value round-trip")
    client.query("CREATE TABLE geo (id Int64, lat Float64, lng Float64, name String)")
    r = client.query("INSERT INTO geo VALUES (1, -1.286389, 36.817223, 'Nairobi')")
    t.check("INSERT negative float literal", r.ok(), r.body[:300])
    client.query("INSERT INTO geo VALUES (2, 40.7128, -74.006, 'say \"hi\" it''s')")

    rows = _data(client.query("SELECT lat, lng FROM geo WHERE id = 1").body)
    t.check("negative Float64 round-trips exactly", rows == [[-1.286389, 36.817223]], f"rows={rows}")

    name = _scalar(client, "SELECT name FROM geo WHERE id = 1")
    t.check("String round-trips", name == "Nairobi", f"name={name!r}")


def test_qualified_names(client: Client, t: TestRunner) -> None:
    t.section("Qualified db.table names")
    r = client.query(f"CREATE TABLE {DB_OTHER}.events (id Int64, amount Float64)")
    t.check("CREATE TABLE other_db.events", r.ok(), r.body[:300])
    r = client.query(f"INSERT INTO {DB_OTHER}.events VALUES (1, 9.25), (2, 0.75)")
    t.check("INSERT INTO other_db.events", r.ok(), r.body[:300])

    t.check("SELECT across databases (current db is not other_db)",
            _scalar(client, f"SELECT COUNT(*) FROM {DB_OTHER}.events") == 2)
    t.check("SUM over qualified table", _scalar(client, f"SELECT SUM(amount) FROM {DB_OTHER}.events") == 10.0)


def test_drop_database(client: Client, t: TestRunner) -> None:
    t.section("DROP DATABASE")
    client.query(f"USE {DB}")
    r = client.query(f"DROP DATABASE {DB_OTHER}")
    t.check("DROP DATABASE existing", r.ok(), r.body[:300])
    r = client.query(f"DROP DATABASE IF EXISTS {DB_OTHER}")
    t.check("DROP DATABASE IF EXISTS on already-dropped db", r.ok(), r.body[:300])
    r = client.query(f"DROP DATABASE {MISSING_DB}")
    t.check("DROP DATABASE on missing db fails", not r.ok(), f"status={r.status}")
    r = client.query(f"DROP DATABASE IF EXISTS {MISSING_DB}")
    t.check("DROP DATABASE IF EXISTS on missing db ok", r.ok(), r.body[:300])

    dbs = client.get("/databases").body
    t.check("dropped db no longer listed", DB_OTHER not in dbs, dbs[:300])


def test_drop_removes_data(client: Client, t: TestRunner) -> None:
    t.section("DROP removes on-disk data")
    client.query(f"USE {DB}")
    client.query("CREATE TABLE scratch (id Int64)")
    client.query("INSERT INTO scratch VALUES (1), (2), (3)")
    t.check("scratch has 3 rows", _scalar(client, "SELECT COUNT(*) FROM scratch") == 3)
    r = client.query("DROP TABLE scratch")
    t.check("DROP TABLE ok", r.ok(), r.body[:300])
    client.query("CREATE TABLE scratch (id Int64)")
    t.check("re-created table starts empty (no resurrected rows)",
            _scalar(client, "SELECT COUNT(*) FROM scratch") in (0, None),
            f"count={_scalar(client, 'SELECT COUNT(*) FROM scratch')}")

    client.query(f"CREATE DATABASE {DB_OTHER}")
    client.query(f"CREATE TABLE {DB_OTHER}.t (id Int64)")
    client.query(f"INSERT INTO {DB_OTHER}.t VALUES (1), (2)")
    client.query(f"DROP DATABASE {DB_OTHER}")
    client.query(f"CREATE DATABASE {DB_OTHER}")
    client.query(f"CREATE TABLE {DB_OTHER}.t (id Int64)")
    t.check("re-created database/table starts empty",
            _scalar(client, f"SELECT COUNT(*) FROM {DB_OTHER}.t") in (0, None),
            f"count={_scalar(client, f'SELECT COUNT(*) FROM {DB_OTHER}.t')}")


def cleanup(client: Client, t: TestRunner) -> None:
    t.section("Cleanup")
    for db in (DB, DB_OTHER):
        r = client.query(f"DROP DATABASE IF EXISTS {db}")
        t.check(f"cleanup: drop {db}", r.ok(), r.body[:300])


def run_tests(client: Client) -> int:
    t = TestRunner()
    setup(client, t)
    test_default_engine_and_append(client, t)
    test_value_roundtrip(client, t)
    test_qualified_names(client, t)
    test_drop_database(client, t)
    test_drop_removes_data(client, t)
    cleanup(client, t)
    return t.summary()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:1143")
    parser.add_argument("--no-launch", action="store_true")
    parser.add_argument("--server-bin", default=None)
    args = parser.parse_args()

    if args.no_launch:
        client = Client(args.url)
        print(f"{YELLOW}Using existing server at {args.url}{RESET}")
        return run_tests(client)

    with ServerProc(Path(args.server_bin) if args.server_bin else None, args.url) as client:
        return run_tests(client)


if __name__ == "__main__":
    sys.exit(main())
