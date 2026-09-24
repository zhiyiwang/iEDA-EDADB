"""Paired warm-cache regression/performance test; run only after compilation stops.

Usage: python3 run.py BASELINE_BINARY STREAM_BINARY NEW_OUTPUT_DIRECTORY
Stage timing is enabled in both binaries. Counters run separately without timing claims.
"""
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import sqlite3
import statistics
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[5]
binaries = {"baseline": Path(sys.argv[1]).resolve(), "stream": Path(sys.argv[2]).resolve()}
output = Path(sys.argv[3]).resolve()
finish_only = "--finish" in sys.argv[4:]
output.mkdir(parents=True, exist_ok=finish_only)
workspace = root / "scripts/design/sky130_gcd"
source = workspace / "result/iRT_result.def"
lines = source.read_text().splitlines(keepends=True)
start = next(index for index, line in enumerate(lines) if line.startswith("NETS "))
end = next(index for index in range(start + 1, len(lines)) if lines[index].startswith("END NETS"))
records = []
for line in lines[start + 1:end]:
    if line.startswith("- "):
        records.append([])
    if records:
        records[-1].append(line)
template = max(records, key=len)
# Synthetic stress fixture: copy one route body, not a valid placement/routing benchmark.
stress = output / "routed-stress.def"
stress.write_text("".join(lines[:start] + [f"NETS {len(records) + 1000} ;\n"]
                         + [line for record in records for line in record]
                         + [line for index in range(1000)
                            for line in [f"- __stream_shadow_stress_{index:06d}\n", *template[1:]]]
                         + lines[end:]))
datasets = {"filler": workspace / "result/iPL_filler_result.def", "routed-stress": stress}
environment = os.environ.copy()
environment.pop("LD_PRELOAD", None)
environment.update(WORKSPACE=str(workspace), CONFIG_DIR=str(workspace / "iEDA_config"),
                   FOUNDRY_DIR=str(root / "scripts/foundry/sky130"),
                   TCL_SCRIPT_DIR=str(workspace / "script"), DESIGN_TCL_SCRIPT_DIR=str(workspace / "script"),
                   DESIGN_TOP="gcd", NETLIST_FILE="/dev/null", SDC_FILE="/dev/null", SPEF_FILE="/dev/null",
                   EDADB_STAGE_TIMING="1")
benchmark = root / "scripts/edadb/performance/benchmark.tcl"
manifest = {"binaries": {key: hashlib.sha256(path.read_bytes()).hexdigest() for key, path in binaries.items()},
            "inputs": {key: dict(bytes=path.stat().st_size, sha256=hashlib.sha256(path.read_bytes()).hexdigest())
                       for key, path in datasets.items()},
            "cache": "warm", "stage_timing": "ON", "cpu": 2, "warmups": 1, "runs": 5,
            "settle_seconds": 5, "additional_stress_nets": 1000}
manifest["parent_head"] = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
manifest["core_gitlink"] = subprocess.check_output(
    ["git", "rev-parse", "HEAD:src/database/edadb/core"], cwd=root, text=True).strip()
manifest["writer_sha256"] = hashlib.sha256(
    (root / "src/database/manager/builder/def_builder/def_write_edadb.cpp").read_bytes()).hexdigest()
if finish_only:
    assert json.loads((output / "manifest.json").read_text()) == manifest
else:
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
rows = []
if finish_only:
    rows = list(csv.DictReader((output / "samples.tsv").open(), delimiter="\t"))
    for row in rows:
        row["value"] = float(row["value"])
reference = {}
db_reference = {}
shim = output / "count_sql.so"
subprocess.run(["g++-10", "-O3", "-shared", "-fPIC", str(Path(__file__).with_name("count_sql.cpp")),
                "-ldl", "-lsqlite3", "-o", str(shim)], check=True)


def run(dataset, version, folder, mode, counts=False):
    settings = environment.copy()
    settings.update(INPUT_DEF=str(datasets[dataset]), PERF_MODE=mode,
                    OUTPUT_DEF=str(folder / f"{mode}.def"), EDADB_DB_PATH=str(folder / "edadb.db"))
    datasets[dataset].read_bytes()
    if mode == "read":
        (folder / "edadb.db").read_bytes()
    # Preload only the child, not GNU time (which also has a process destructor).
    prefix = ["env", f"LD_PRELOAD={shim}"] if counts else []
    with (folder / f"{mode}.log").open("w") as log:
        subprocess.run(["/usr/bin/time", "-f", "%M", "-o", str(folder / f"{mode}.rss"),
                        *prefix, "taskset", "-c", "2", str(binaries[version]), "-script", str(benchmark)],
                       cwd=output, env=settings, stdout=log, stderr=subprocess.STDOUT, check=True)


def audit(dataset, folder):
    assert (folder / "read.def").read_bytes() == reference[dataset], folder
    with sqlite3.connect(f"file:{folder / 'edadb.db'}?mode=ro", uri=True) as connection:
        assert connection.execute("PRAGMA integrity_check").fetchall() == [("ok",)]
        assert connection.execute("PRAGMA foreign_key_check").fetchall() == []
        digest = hashlib.sha256("\n".join(connection.iterdump()).encode()).hexdigest()
    if dataset not in db_reference:
        db_reference[dataset] = digest
    assert db_reference[dataset] == digest, folder


for dataset in (() if finish_only else datasets):
    for round_number in range(6):
        order = ("baseline", "stream") if round_number % 2 == 0 else ("stream", "baseline")
        for version in order:
            print(f"{time.strftime('%H:%M:%S')} {dataset} round={round_number} {version}", flush=True)
            folder = output / dataset / str(round_number) / version
            folder.mkdir(parents=True)
            time.sleep(5)
            if round_number == 0:
                run(dataset, version, folder, "native")
                data = (folder / "native.def").read_bytes()
                if dataset not in reference:
                    reference[dataset] = data
                assert reference[dataset] == data
            for mode in ("write", "read"):
                run(dataset, version, folder, mode)
                values = {}
                for line in (folder / f"{mode}.log").read_text().splitlines():
                    parts = line.split("\t")
                    if parts[0] == "EDADB_PERF":
                        values["command_ms"] = int(parts[2]) / 1000
                    elif parts[0] == "EDADB_STAGE":
                        values["stage_" + parts[2] + "_ms"] = int(parts[4]) / 1e6
                assert "command_ms" in values and f"stage_{mode}_ms" in values
                accounted = sum(values["stage_" + stage + "_ms"] for stage in ("init", "create", mode, "other"))
                assert abs(accounted - values["stage_command_ms"]) < 0.00001
                values["process_peak_rss_kib"] = int((folder / f"{mode}.rss").read_text())
                if round_number:
                    rows.extend(dict(dataset=dataset, round=round_number, version=version, operation=mode,
                                     metric=metric, value=value) for metric, value in values.items())
                    with (output / "samples.tsv").open("w") as stream:
                        writer = csv.DictWriter(stream, fieldnames=rows[0].keys(), delimiter="\t")
                        writer.writeheader()
                        writer.writerows(rows)
            audit(dataset, folder)

# --finish retries only diagnostics/statistics after verifying all saved timed samples.
if finish_only:
    assert len(rows) == 280
    for dataset in datasets:
        reference[dataset] = (output / dataset / "0" / "baseline" / "native.def").read_bytes()
        assert reference[dataset] == (output / dataset / "0" / "stream" / "native.def").read_bytes()
        for round_number in range(6):
            for version in binaries:
                audit(dataset, output / dataset / str(round_number) / version)

# No performance conclusion is drawn from these instrumented counter runs.
counts = {}
for dataset in datasets:
    counts[dataset] = {}
    for version in binaries:
        folder = output / "counts" / dataset / version
        folder.mkdir(parents=True, exist_ok=finish_only)
        run(dataset, version, folder, "write", counts=True)
        log = (folder / "write.log").read_text()
        matches = re.findall(r"SQL_COUNTS prepare=(\d+) insert=(\d+) begin=(\d+) commit=(\d+) rollback=(\d+)", log)
        assert len(matches) == 1
        counts[dataset][version] = dict(zip(("prepare", "insert", "begin", "commit", "rollback"), map(int, matches[0])))
        assert counts[dataset][version]["insert"] > 0
        assert counts[dataset][version]["begin"] == counts[dataset][version]["commit"] == 2
        assert counts[dataset][version]["rollback"] == 0
    assert counts[dataset]["baseline"] == counts[dataset]["stream"], counts[dataset]
groups = {}
for row in rows:
    key = (row["dataset"], row["version"], row["operation"], row["metric"])
    groups.setdefault(key, []).append(row["value"])
summary = []
for key, values in groups.items():
    assert len(values) == 5
    summary.append(dict(zip(("dataset", "version", "operation", "metric"), key)) |
                   dict(count=5, median=statistics.median(values), mean=statistics.mean(values),
                        minimum=min(values), maximum=max(values)))
with (output / "summary.tsv").open("w") as stream:
    writer = csv.DictWriter(stream, fieldnames=summary[0].keys(), delimiter="\t")
    writer.writeheader()
    writer.writerows(summary)
(output / "audit.json").write_text(json.dumps(dict(status="PASS", strict_roundtrips=24,
    equal_database_dumps=24, counts=counts, database_dump_sha256=db_reference), indent=2) + "\n")
print("PASS: paired timings, strict DEF, database dumps and SQL counters", flush=True)
