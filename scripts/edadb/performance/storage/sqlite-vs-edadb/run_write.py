"""Paired write controls, sequential samples; no changes to EDADB or adapter.

python3 run_write.py /tmp/iedadb_stream_clean_build/stream_benchmark /tmp/iedadb_write_control
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
for name in ["stream_benchmark.cpp", "benchmark_support.h"]:
    shutil.copyfile(source / name, output / name)
shutil.copyfile(__file__, output / Path(__file__).name)
for name in ["flags.make", "link.txt"]:
    shutil.copyfile(binary.parent / "CMakeFiles/stream_benchmark.dir" / name, output / name)
(output / "binary.sha256").write_text(hashlib.sha256(binary.read_bytes()).hexdigest() + "\n")
variants = ["sqlite", "sqlite-clear", "sqlite-match", "sqlite-match-clear", "edadb"]
rows = []
checks = []
for config, cache in [("A", "first"), ("B-batch", "warm")]:
    for sample in range(args.runs + 1):
        # Full-field validation is a separate warmup; performance only consumes
        # fields and checks the resulting row count/digest outside the timer.
        order = variants[sample % 5:] + variants[:sample % 5]
        for variant in order:
            label = f"{config}-{sample}-{variant}"
            database = output / (label + ".db")
            env = os.environ.copy()
            for name in ["STREAM_NULL_CHECK", "STREAM_MATCH_BINDINGS", "STREAM_CLEAR_BINDINGS"]:
                env.pop(name, None)
            env["STREAM_SETTLE"] = str(args.settle)
            if "match" in variant:
                env["STREAM_MATCH_BINDINGS"] = "1"
            if "clear" in variant:
                env["STREAM_CLEAR_BINDINGS"] = "1"
            command = [str(binary), "edadb" if variant == "edadb" else "sqlite", config,
                       str(args.count), str(database), str(output), "check" if sample == 0 else "perf", cache]
            print(label, flush=True)
            with (output / (label + ".log")).open("w") as log:
                subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
            lines = (output / (label + ".log")).read_text().splitlines()
            write = next(line.split("\t") for line in lines if line.startswith("STREAM\twrite\t"))
            read = next(line.split("\t") for line in lines if line.startswith("STREAM\tread\t"))
            assert int(read[9]) == args.count
            if sample == 0:
                assert "CHECK\tsame-length corruption rejected" in lines
                checks.append(label)
            else:
                rows.append(dict(config=config, sample=sample, route=variant,
                                 init_ms=float(write[2]), create_ms=float(write[3]), begin_ms=float(write[4]),
                                 write_ms=float(write[5]), commit_ms=float(write[6]), complete_ms=float(write[8]),
                                 read_ms=float(read[5]), count=int(read[9]), digest=read[10]))
                with (output / "samples.tsv").open("w") as stream:
                    writer = csv.DictWriter(stream, fieldnames=list(rows[0]), delimiter="\t")
                    writer.writeheader()
                    writer.writerows(rows)
            database.unlink(missing_ok=True)
assert len(checks) == 10 and len(rows) == 10 * args.runs
assert len({row["digest"] for row in rows}) == 1
(output / "arguments.json").write_text(json.dumps(vars(args), default=str, indent=2) + "\n")
summary = []
for config in ["A", "B-batch"]:
    for variant in variants:
        selected = [row for row in rows if row["config"] == config and row["route"] == variant]
        values = [row["write_ms"] for row in selected]
        summary.append(dict(config=config, route=variant, median_ms=statistics.median(values),
                            mean_ms=statistics.mean(values), min_ms=min(values), max_ms=max(values),
                            create_ms=statistics.median(row["create_ms"] for row in selected),
                            commit_ms=statistics.median(row["commit_ms"] for row in selected)))
(output / "checks.json").write_text(json.dumps(checks, indent=2) + "\n")
(output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
print(json.dumps(summary, indent=2), flush=True)
