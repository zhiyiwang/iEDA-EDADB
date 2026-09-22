"""Serial SELECT */explicit-column comparison; correctness and counters untimed.

python3 run_select.py /tmp/select_build/select_projection /tmp/select_results
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("binary", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--count", type=int, default=1000000)
parser.add_argument("--runs", type=int, default=5)
parser.add_argument("--prepare-repeats", type=int, default=10000)
parser.add_argument("--settle", type=float, default=1)
args = parser.parse_args()
assert 0 < args.count <= 1000000 and args.runs > 0 and args.prepare_repeats > 0 and args.settle >= 0
os.sched_setaffinity(0, {min(os.sched_getaffinity(0))})
args.output.mkdir(parents=True, exist_ok=False)
args.output = args.output.resolve()
binary = args.output / "select_projection"
shutil.copy2(args.binary.resolve(), binary)
source = Path(__file__).parent
shutil.copytree(source, args.output / "source", ignore=shutil.ignore_patterns("__pycache__"))
for name in ("flags.make", "link.txt"):
    shutil.copy2(args.binary.resolve().parent / "CMakeFiles/select_projection.dir" / name, args.output / name)
(args.output / "manifest.json").write_text(json.dumps(dict(
    binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(), count=args.count,
    runs=args.runs, prepare_repeats=args.prepare_repeats,
    process_status=Path("/proc/self/status").read_text(),
    libraries=subprocess.check_output(["ldd", str(binary)], text=True),
    source_hashes={name: hashlib.sha256((source/name).read_bytes()).hexdigest() for name in
                   ("benchmark.cpp", "select_projection.cpp", "run_select.py", "CMakeLists.txt")}), indent=2))
rows, evidence = [], {}
checks = 0


def run(config, route, sample, count, mode):
    stem = args.output / f"{config}-{route}-{sample}-{count}"
    data = stem.with_suffix(".db")
    if mode == "timing":
        time.sleep(args.settle)
    with stem.with_suffix(".log").open("w") as log:
        subprocess.run([str(binary), route, config, str(count), str(data), mode,
                        str(args.prepare_repeats)], stdout=log, stderr=subprocess.STDOUT,
                       check=True, timeout=180)
    lines = stem.with_suffix(".log").read_text().splitlines()
    checked = next(line.split("\t") for line in lines if line.startswith("CHECK\tPASS"))
    assert int(checked[2]) == count
    plan = [line for line in lines if line.startswith("EXPLAIN\t")]
    assert plan
    key = (config, count)
    if key in evidence:
        assert evidence[key] == plan, "bytecode differs"
    evidence[key] = plan
    if mode == "timing":
        result = next(line.split("\t") for line in lines if line.startswith("RESULT\t"))
        assert int(result[3]) == args.prepare_repeats
        rows.append(dict(config=config, route=route, sample=sample, count=count,
                         read_ms=float(result[1]), prepare_finalize_ms=float(result[2]),
                         prepare_repeats=args.prepare_repeats, log=stem.with_suffix(".log").name))
        with (args.output / "samples.tsv").open("w") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(rows[0]), delimiter="\t")
            writer.writeheader()
            writer.writerows(rows)
    if mode == "counters":
        counters = dict((line.split("\t")[1], int(line.split("\t")[2]))
                        for line in lines if line.startswith("COUNTER\t"))
        assert counters == dict(vm_step=count * 10 + 6, run=1, reprepare=0,
                                fullscan_step=count-1, sort=0, autoindex=0)
    data.unlink(missing_ok=True)
    print(config, route, sample, "PASS", flush=True)


for config in ("A", "B-batch"):
    for route in ("explicit", "star"):
        for count in dict.fromkeys((0, 1, 8, args.count)):
            run(config, route, "check", count, "check")
            checks += 1
    for sample in range(args.runs):
        for route in (("explicit", "star") if sample % 2 == 0 else ("star", "explicit")):
            run(config, route, sample+1, args.count, "timing")
for config in ("A", "B-batch"):
    for route in ("explicit", "star"):
        run(config, route, "counters", args.count, "counters")

summary, deltas = [], []
for config in ("A", "B-batch"):
    for metric in ("read_ms", "prepare_finalize_ms"):
        for route in ("explicit", "star"):
            values = [row[metric] for row in rows if row["config"] == config and row["route"] == route]
            summary.append(dict(config=config, route=route, metric=metric, mean=statistics.mean(values),
                                median=statistics.median(values), min=min(values), max=max(values)))
        paired = []
        for sample in range(1, args.runs+1):
            selected = {row["route"]: row[metric] for row in rows if
                        row["config"] == config and row["sample"] == sample}
            paired.append(selected["star"]-selected["explicit"])
        deltas.append(dict(config=config, metric=metric, difference="star-explicit", paired=paired,
                           median=statistics.median(paired), min=min(paired), max=max(paired)))
for name, value in (("summary", summary), ("deltas", deltas), ("audit", dict(
        status="PASS", checks=checks, timing_samples=len(rows), bytecode_equal=True, counter_runs=4))):
    (args.output / (name+".json")).write_text(json.dumps(value, indent=2)+"\n")
print("COMPLETE", args.output)
