"""Sequential NULL-guard control; generated DBs/logs stay outside the repository.

python3 run_read.py /tmp/iedadb_stream_clean_build/stream_benchmark /tmp/iedadb_null_check
"""
import argparse
import csv
import json
import os
import hashlib
import shutil
from pathlib import Path
import statistics
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("binary", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--count", type=int, default=1000000)
parser.add_argument("--runs", type=int, default=5)
parser.add_argument("--settle", type=float, default=1)
args = parser.parse_args()
assert 0 < args.count <= 1000000 and args.runs > 0 and args.settle >= 0
binary = args.binary.resolve()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=False)
source = Path(__file__).resolve().parent.parent / "benchmark"
for name in ("stream_benchmark.cpp", "benchmark_support.h", "CMakeLists.txt"):
    shutil.copyfile(source / name, output / name)
shutil.copyfile(__file__, output / Path(__file__).name)
for name in ("flags.make", "link.txt"):
    shutil.copyfile(binary.parent / "CMakeFiles/stream_benchmark.dir" / name, output / name)
(output / "binary.sha256").write_text(hashlib.sha256(binary.read_bytes()).hexdigest() + "\n")
checks = []
rows = []
for config, cache in [("A", "first"), ("B-batch", "warm")]:
    for sample in range(args.runs + 1):
        # Rotate order to reduce systematic time/order bias. Check mode is the
        # independent full-field validation/warmup, excluded from statistics.
        variants = ["sqlite", "sqlite-null", "edadb"]
        variants = variants[sample % 3:] + variants[:sample % 3]
        for variant in variants:
            label = f"{config}-{sample}-{variant}"
            database = output / (label + ".db")
            env = os.environ.copy()
            for name in ("STREAM_NULL_CHECK", "STREAM_MATCH_BINDINGS", "STREAM_CLEAR_BINDINGS"):
                env.pop(name, None)
            env["STREAM_SETTLE"] = str(args.settle)
            if variant == "sqlite-null":
                env["STREAM_NULL_CHECK"] = "1"
            command = [str(binary), "edadb" if variant == "edadb" else "sqlite",
                       config, str(args.count), str(database), str(output),
                       "check" if sample == 0 else "perf", cache]
            print(label, flush=True)
            with (output / (label + ".log")).open("w") as log:
                subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
            lines = (output / (label + ".log")).read_text().splitlines()
            read = next(line.split("\t") for line in lines if line.startswith("STREAM\tread\t"))
            if sample == 0:
                assert "CHECK\tsame-length corruption rejected" in lines
                checks.append(label)
            if sample:
                rows.append(dict(config=config, sample=sample, route=variant,
                                 read_ms=float(read[5]), count=int(read[9]), digest=read[10]))
                with (output / "samples.tsv").open("w") as stream:
                    writer = csv.DictWriter(stream, fieldnames=list(rows[0]), delimiter="\t")
                    writer.writeheader()
                    writer.writerows(rows)
            database.unlink(missing_ok=True)
assert len(checks) == 6 and len(rows) == 6 * args.runs
(output / "checks.json").write_text(json.dumps(checks, indent=2) + "\n")
assert len({(row["count"], row["digest"]) for row in rows}) == 1
assert all(row["count"] == args.count for row in rows)
(output / "arguments.json").write_text(json.dumps(vars(args), default=str, indent=2) + "\n")
summary = []
for config in ["A", "B-batch"]:
    for variant in ["sqlite", "sqlite-null", "edadb"]:
        values = [row["read_ms"] for row in rows if row["config"] == config and row["route"] == variant]
        summary.append(dict(config=config, route=variant, median_ms=statistics.median(values),
                            mean_ms=statistics.mean(values), min_ms=min(values), max_ms=max(values)))
(output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
print(json.dumps(summary, indent=2), flush=True)
