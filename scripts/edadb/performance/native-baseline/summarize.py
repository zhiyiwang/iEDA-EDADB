"""Audit the native-baseline runner output and summarize microseconds/nanoseconds as ms."""
import csv
import hashlib
import json
import sqlite3
import statistics
import sys
from collections import defaultdict
from pathlib import Path

root = Path(sys.argv[1])
samples = list(csv.DictReader((root / "samples.tsv").open(), delimiter="\t"))
groups = defaultdict(list)
for sample in samples:
    groups[(sample["version"], sample["metric"])].append(float(sample["time_us"]) / 1000)
assert len(groups) == 6
assert all(len(values) == 5 for values in groups.values())

canonical_hashes = set()
database_checks = 0
stage_samples = []
for round_number in range(6):
    original = (root / "original" / f"round-{round_number}" / "native.def").read_bytes()
    canonical_hashes.add(hashlib.sha256(original).hexdigest())
    for version in ("optimized", "stages"):
        folder = root / version / f"round-{round_number}"
        assert (folder / "read.def").read_bytes() == original
        if version == "optimized":
            assert (folder / "native.def").read_bytes() == original
        connection = sqlite3.connect(f"file:{folder / 'edadb.db'}?mode=ro", uri=True)
        try:
            assert connection.execute("PRAGMA integrity_check").fetchall() == [("ok",)]
            assert connection.execute("PRAGMA foreign_key_check").fetchall() == []
        finally:
            connection.close()
        database_checks += 1
    folder = root / "stages" / f"round-{round_number}"
    for operation in ("read", "write"):
        stages = {}
        tcl_ns = None
        for line in (folder / f"{operation}.log").read_text().splitlines():
            fields = line.split("\t")
            if fields[0] == "EDADB_STAGE":
                assert fields[1] == operation and fields[3] == "ns"
                stages[fields[2]] = int(fields[4])
            elif fields[0] == "EDADB_PERF" and fields[1] == f"edadb_{operation}":
                tcl_ns = int(fields[2]) * 1000
        assert set(stages) == {"init", "create", operation, "other", "command"}
        assert min(stages.values()) >= 0
        assert sum(value for key, value in stages.items() if key != "command") == stages["command"]
        assert tcl_ns is not None and tcl_ns >= stages["command"]
        # Derived from the same sample, not a separately measured function:
        # includes work outside the C++ session and its timing-report output.
        stages["outside_session"] = tcl_ns - stages["command"]
        stages["tcl_command"] = tcl_ns
        if round_number:
            for stage, value in stages.items():
                metric = f"{operation}_{stage}"
                groups[("stages", metric)].append(value / 1_000_000)
                stage_samples.append(dict(round=round_number, operation=operation, stage=stage, time_ns=value))
assert len(canonical_hashes) == 1
summary = []
for (version, metric), values in groups.items():
    assert len(values) == 5
    summary.append(dict(version=version, metric=metric, count=len(values),
                        median_ms=statistics.median(values), mean_ms=statistics.mean(values),
                        min_ms=min(values), max_ms=max(values)))
for name, rows in (("summary.tsv", summary), ("stages.tsv", stage_samples)):
    with (root / name).open("w") as output:
        writer = csv.DictWriter(output, fieldnames=rows[0].keys(), delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)
(root / "audit.json").write_text(json.dumps(dict(
    status="PASS", formal_command_records=len(samples), formal_stage_records=len(stage_samples),
    database_checks=database_checks, cross_version_def_checks=6, roundtrip_checks=12,
    canonical_def_sha256=list(canonical_hashes)), indent=2))
for row in summary:
    print(row)
