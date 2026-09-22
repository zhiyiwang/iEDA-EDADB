"""Recompute the causal experiment summaries and inspect batch SQL independently.

Usage: python3 audit_causal.py /tmp/iedadb_sqlite_text_causal_full
No benchmark timing is collected here. EXPLAIN uses Python's SQLite only after
checking its source ID against the measured C++ library.
"""
import csv
import hashlib
import json
from pathlib import Path
import sqlite3
import statistics
import sys

output = Path(sys.argv[1])
samples = list(csv.DictReader((output / "samples.tsv").open(), delimiter="\t"))
summary = json.loads((output / "summary.json").read_text())
deltas = json.loads((output / "deltas.json").read_text())
counters = list(csv.DictReader((output / "counters.tsv").open(), delimiter="\t"))
manifest = json.loads((output / "manifest.json").read_text())
assert len(samples) == 24 * manifest["runs"]
assert len(summary) == 24
assert hashlib.sha256((output / "benchmark").read_bytes()).hexdigest() == manifest["binary_sha256"]
for group in summary:
    values = [float(row["data_ms"]) for row in samples if
              (row["config"], row["route"], row["operation"]) ==
              (group["config"], group["route"], group["operation"])]
    assert len(values) == manifest["runs"]
    assert statistics.median(values) == group["median_data_ms"]
for delta in deltas:
    left, right = delta["difference"].split("-", 1)
    differences = []
    for sample in sorted({row["sample"] for row in samples}):
        values = {row["route"]: float(row["data_ms"]) for row in samples if
                  (row["config"], row["operation"], row["sample"]) ==
                  (delta["config"], delta["operation"], sample)}
        differences.append(values[left] - values[right])
    assert differences == delta["paired_ms"]

database = sqlite3.connect(":memory:")
source_id = database.execute("SELECT sqlite_source_id()").fetchone()[0]
schema = "CREATE TABLE component(name TEXT,master_name TEXT,source INTEGER,status INTEGER,orient INTEGER,x INTEGER,y INTEGER,record_order BIGINT)"
database.execute(schema)
plans = {}
for batch in (1, 10, 100):
    sql = "INSERT INTO component VALUES" + ",".join(["(?,?,?,?,?,?,?,?)"] * batch)
    plans[str(batch)] = {"sql": sql, "explain": database.execute(
        "EXPLAIN " + sql, [None] * (batch * 8)).fetchall()}
for row in samples:
    if row["route"] == "text":
        continue
    log = (output / row["log"]).read_text()
    assert "CONFIG\tsource_id\t" + source_id in log
    assert "CHECK\tdigest\tPASS" in log
    batch = {"batch10": 10, "batch100": 100}.get(row["route"], 1)
    assert "SQL\twrite\t" + plans[str(batch)]["sql"] + "\n" in log
for config in ("A", "B-batch"):
    def values(route, phase):
        return {row["counter"]: int(row["value"]) for row in counters if
                (row["config"], row["route"], row["phase"]) == (config, route, phase)}
    assert values("sqlite", "read") == values("step-only", "read")
    for route, batch in (("sqlite", 1), ("batch10", 10), ("batch100", 100)):
        assert values(route, "write")["run"] == manifest["count"] // batch
        assert values(route, "read")["fullscan_step"] == manifest["count"] - 1
database.close()
checks = json.loads((output / "checks.json").read_text())
assert len(checks) == 60 and all(row["status"] == "PASS" for row in checks)
(output / "batch_explain.json").write_text(json.dumps({
    "source_id": source_id, "origin": "independent EXPLAIN, not runtime trace",
    "plans": plans}, indent=2) + "\n")
(output / "causal_audit.json").write_text(json.dumps({
    "status": "PASS", "timing_rows": len(samples), "groups": len(summary),
    "checks": len(checks), "read_vm_identical": True,
    "auditor_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    "scope": "recomputed medians/deltas; binary hash, emitted SQL, source ID, RUN and read counter invariants; batch EXPLAIN"}, indent=2) + "\n")
print("PASS", output)
