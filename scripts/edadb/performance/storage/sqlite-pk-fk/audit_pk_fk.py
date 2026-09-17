"""Recheck saved timing logs/statistics and every field of formal disk databases.
Run after performance sampling, never concurrently with measured operations.
Usage: python3 audit_pk_fk.py /tmp/iedadb_pk_fk_main_20260911
"""
import csv
import hashlib
import json
from pathlib import Path
import sqlite3
import statistics
import sys


def audit(directory):
    samples = list(csv.DictReader((directory / "samples.tsv").open(), delimiter="\t"))
    summary = list(csv.DictReader((directory / "summary.tsv").open(), delimiter="\t"))
    keys = ["parent_count", "children_per_parent", "route", "schema", "storage", "foreign_keys", "read_mode", "operation"]
    fields = ["init_ms", "create_ms", "begin_ms", "data_ms", "commit_ms", "close_ms", "complete_ms"]
    for row in samples:
        lines = Path(row["log"]).read_text().splitlines()
        result = next(line.split("\t") for line in lines if line.startswith("RESULT\t" + row["operation"] + "\t"))
        assert [float(row[field]) for field in fields] == list(map(float, result[2:9]))
        assert [int(row[field]) for field in ("parents", "children", "digest")] == list(map(int, result[9:12]))
    for row in summary:
        selected = [sample for sample in samples if all(sample[key] == row[key] for key in keys)]
        assert len(selected) == int(row["samples"])
        for field in fields:
            values = [float(sample[field]) for sample in selected]
            for name, function in (("mean", statistics.mean), ("median", statistics.median), ("min", min), ("max", max)):
                actual = row[name + "_" + field]
                assert actual == "NA" if min(values) < 0 else abs(float(actual) - function(values)) < 1e-9
    databases = []
    for row in samples:
        if row["storage"] != "disk" or row["operation"] != "write": continue
        path = Path(row["log"]).with_suffix(".db")
        parents, children = int(row["parent_count"]), int(row["children_per_parent"])
        with sqlite3.connect(path.as_uri() + "?mode=ro", uri=True) as database:
            assert database.execute("PRAGMA integrity_check").fetchall() == [("ok",)]
            assert not database.execute("PRAGMA foreign_key_check").fetchall()
            tables = [record[0] for record in database.execute("SELECT name FROM sqlite_schema WHERE type='table' AND name NOT LIKE 'sqlite_%'")]
            assert len(tables) == 2 and "component" in tables
            child_table = next(table for table in tables if table != "component")
            seen = set()
            for record in database.execute("SELECT name,master_name,source,status,orient,x,y,record_order FROM component"):
                index = record[7]
                assert 0 <= index < parents and index not in seen
                seen.add(index)
                assert record == (f"U{index:07d}", "bench_cell", 2, 3, 1, index % 1000 * 100, index // 1000 * 100, index)
            assert len(seen) == parents
            fk = database.execute(f'PRAGMA foreign_key_list("{child_table}")').fetchall()
            assert len(fk) == 1
            owner_column = fk[0][3]
            seen = set()
            for pin, direction, xpos, ypos, owner in database.execute(f'SELECT pin_name,direction,x,y,"{owner_column}" FROM "{child_table}"'):
                parent_index, pin_index = int(owner[1:]), int(pin[1:])
                assert owner == f"U{parent_index:07d}" and pin == f"P{pin_index:04d}"
                assert 0 <= parent_index < parents and 0 <= pin_index < children
                assert (parent_index, pin_index) not in seen
                seen.add((parent_index, pin_index))
                assert (direction, xpos, ypos) == (1, parent_index % 1000 * 100 + 10 * (pin_index + 1), parent_index // 1000 * 100)
            assert len(seen) == parents * children
            indexes = database.execute(f'PRAGMA index_list("{child_table}")').fetchall()
            assert len(indexes) == (0 if row["schema"] == "none" else 1)
            if indexes:
                assert indexes[0][2] == (1 if row["schema"] == "pk" else 0)
                assert [entry[2] for entry in database.execute(f'PRAGMA index_info("{indexes[0][1]}")')] == [owner_column, "pin_name"]
        databases.append(dict(path=str(path), bytes=path.stat().st_size, sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    (directory / "database_audit.json").write_text(json.dumps(dict(status="PASS", databases=databases,
        timing_rows=len(samples), statistics_groups=len(summary), note="All formal disk DB fields verified after measurements; memory checked by independent check processes and per-sample digest."), indent=2))
    print(f"PASS: {len(samples)} timing rows, {len(summary)} summary groups, {len(databases)} full-field disk databases")


if __name__ == "__main__":
    audit(Path(sys.argv[1]).resolve())
