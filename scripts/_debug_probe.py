from _mnemo_client import ServerProc

sqls = [
    "CREATE DATABASE IF NOT EXISTS sales_db",
    "USE sales_db",
    "DROP TABLE IF EXISTS customers",
    "CREATE TABLE customers (customer_id Int64, name String, age Int64)",
    "DESCRIBE customers",
    "INSERT INTO customers VALUES (1, 'Alice', 25)",
    "SELECT * FROM customers",
    "SELECT COUNT(*) FROM customers",
]

with ServerProc() as c:
    for sql in sqls:
        r = c.query(sql)
        print("---", sql)
        print("status", r.status)
        print(r.body[:500])
        print()
