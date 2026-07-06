### Happens when the table engine is File and storage unit is Local


Need to ensure that INSERT query appends rather than overwrites data in these tables.

## Steps to test:

- json.loads(client.query(f"CREATE STORAGE_UNIT {LOCAL_UNIT} TYPE LOCAL PATH '{tmp_path}'").body)
- json.loads(client.query(f"CREATE TABLE {TABLE} (id Float64, val Float64) Engine=File STORAGE_UNIT {LOCAL_UNIT} ").body)
- json.loads(client.query(f"INSERT INTO {TABLE} VALUES (1.0, 10.5), (2.0, 20.5), (3.0, 30.5)").body)
- json.loads(client.query(f"INSERT INTO {TABLE} VALUES (15.0, 10.5), (12.0, 21.5), (33.0, 310.53)").body)
- json.loads(client.query(f"SELECT * FROM {TABLE}").body)
