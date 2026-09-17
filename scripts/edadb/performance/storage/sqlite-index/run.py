#!/usr/bin/env python3
"""Run correctness first, then serial warm-up/timing, then independent diagnostics.

Example (all artifacts stay outside the repository):
  python3 run.py --binary /tmp/iedadb_sqlite_index_build/index_benchmark \
      --out /tmp/iedadb_sqlite_index_full --count 1000000 --runs 5
"""
import argparse
import concurrent.futures
import csv
import hashlib
import itertools
import json
import os
from pathlib import Path
import statistics
import subprocess
import time

LAYOUTS = ["none", "index", "unique", "text-pk", "text-pk-without-rowid",
           "integer-unique", "integer-rowid", "integer-without-rowid"]
CONFIGS = ["A", "B-batch"]
ROOT = Path(__file__).resolve().parent


def save_json(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n")


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def command_output(command):
    return subprocess.check_output(command, text=True).strip()


def parse(path):
    lines = [line.split("\t") for line in path.read_text().splitlines()]
    if not lines or lines[-1] != ["PASS"]:
        raise RuntimeError(f"missing PASS: {path}")
    times = {row[1]: float(row[2]) for row in lines if row[0] == "TIME"}
    if len(times) != 13:
        raise RuntimeError(f"incomplete stages: {path}")
    return {
        "times": times,
        "input": next(row[1:] for row in lines if row[0] == "INPUT"),
        "requests": next(row[1:] for row in lines if row[0] == "REQUESTS"),
        "consumed": next(row[1:] for row in lines if row[0] == "CONSUMED"),
        "space": list(map(int, next(row[1:] for row in lines if row[0] == "SPACE"))),
        "version": next(row[1:] for row in lines if row[0] == "VERSION"),
        "config": [row for row in lines if row[0].startswith("CONFIG")],
        "counters": [row[1:] for row in lines if row[0] == "COUNTER"],
        "schema": [row[1:] for row in lines if row[0] == "SCHEMA"],
        "eqp": [row for row in lines if row[0].startswith("EQP ")],
        "trees": [row[1:] for row in lines if row[0] == "TREE"],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--count", type=int, default=1000000)
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--settle", type=float, default=2)
    parser.add_argument("--check-jobs", type=int, default=4)
    parser.add_argument("--cpu", type=int, default=min(os.sched_getaffinity(0)))
    args = parser.parse_args()
    if args.count < 1 or args.count > 1000000 or args.runs < 1:
        parser.error("count must be 1..1000000; runs must be positive")
    args.binary = args.binary.resolve()
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=False)
    for directory in ("checks", "warmup", "timing", "diagnostics"):
        (args.out / directory).mkdir()
    started = time.monotonic()
    manifest = {
        "arguments": {key: str(value) if isinstance(value, Path) else value for key, value in vars(args).items()},
        "git_commit": command_output(["git", "-C", str(ROOT), "rev-parse", "HEAD"]),
        "source_sha256": {path.name: sha256(path) for path in (ROOT / "benchmark.cpp", ROOT / "run.py", ROOT / "CMakeLists.txt")},
        "binary_sha256": sha256(args.binary),
        "ldd": command_output(["ldd", str(args.binary)]),
        "cpu": command_output(["lscpu"]),
        "memory": command_output(["free", "-b"]),
        "uname": command_output(["uname", "-a"]),
        "input_generator": "eight-field Component; name U%07d, bench_cell, 2,3,1, index%1000*100,index/1000*100,index",
        "timing": "serial; pinned CPU; no row timers or diagnostic counters; ms",
    }
    compile_commands = args.binary.parent / "compile_commands.json"
    if compile_commands.exists():
        manifest["compile_commands"] = json.loads(compile_commands.read_text())
        if not all("-O3" in entry["command"] and "-DNDEBUG" in entry["command"]
                   for entry in manifest["compile_commands"]):
            raise RuntimeError("not verified Release -O3")
    else:
        raise RuntimeError("compile_commands.json required to verify Release")
    save_json(args.out / "manifest.json", manifest)

    def sample(mode, layout, config, count, location, pin=False):
        database = location.with_suffix(".db")
        command = [str(args.binary), mode, layout, config, str(count), str(database)]
        if pin:
            command = ["taskset", "-c", str(args.cpu)] + command
        begin = time.monotonic()
        with location.open("w") as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError(f"failed: {location}")
        output = parse(location)
        output.update(layout=layout, config_name=config, log=str(location), elapsed_s=time.monotonic() - begin)
        # Small generated correctness DBs need not remain after a successful check.
        if mode == "check" and database.exists():
            database.unlink()
        return output

    check_jobs = list(itertools.product(LAYOUTS, CONFIGS, (0, 1, 1000)))
    def check(job):
        layout, config, count = job
        return sample("check", layout, config, count, args.out / "checks" / f"{config}-{layout}-{count}.log")
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.check_jobs) as pool:
        checks = list(pool.map(check, check_jobs))
    save_json(args.out / "checks.json", {"passed": len(checks), "cases": checks})
    print(f"{time.strftime('%H:%M:%S')} correctness PASS: {len(checks)} cases", flush=True)

    timings = []
    completed = 0
    total = len(LAYOUTS) * len(CONFIGS) * (args.runs + 1)
    performance_start = time.monotonic()
    # Rotate the order each round. No other benchmark/diagnostic runs concurrently.
    for config in CONFIGS:
        for repeat in range(args.runs + 1):
            order = LAYOUTS[repeat % len(LAYOUTS):] + LAYOUTS[:repeat % len(LAYOUTS)]
            for layout in order:
                time.sleep(args.settle)
                category = "warmup" if repeat == 0 else "timing"
                output = sample("timing", layout, config, args.count,
                                args.out / category / f"{config}-{layout}-{repeat}.log", pin=True)
                if repeat:
                    output["repeat"] = repeat
                    timings.append(output)
                    save_json(args.out / "samples.json", timings)
                completed += 1
                elapsed = time.monotonic() - performance_start
                eta = elapsed / completed * (total - completed)
                save_json(args.out / "progress.json", {"stage": "timing", "completed": completed, "total": total,
                                                     "elapsed_s": elapsed, "estimated_timing_remaining_s": eta})
                print(f"{time.strftime('%H:%M:%S')} {completed}/{total} {config} {layout} {category}-{repeat}: "
                      f"write={output['times']['write']:.3f} scan={output['times']['scan']:.3f} "
                      f"point={output['times']['point']:.3f} ms; timing ETA={eta/60:.1f} min", flush=True)

    diagnostics = []
    for config, layout in itertools.product(CONFIGS, LAYOUTS):
        output = sample("diagnose", layout, config, args.count,
                        args.out / "diagnostics" / f"{config}-{layout}.log", pin=True)
        diagnostics.append(output)
        print(f"{time.strftime('%H:%M:%S')} diagnostics {len(diagnostics)}/16 PASS {config} {layout}", flush=True)
    save_json(args.out / "diagnostics.json", diagnostics)

    # Validate evidence completeness and same inputs/configurations, not just process exits.
    expected_input = timings[0]["input"]
    expected_requests = timings[0]["requests"]
    expected_consumed = timings[0]["consumed"]
    for output in timings + diagnostics:
        if output["input"] != expected_input or output["requests"] != expected_requests or output["consumed"] != expected_consumed:
            raise RuntimeError("input/request/result mismatch across layouts")
        reference = next(item for item in timings if item["config_name"] == output["config_name"])
        if output["config"] != reference["config"] or output["version"] != reference["version"]:
            raise RuntimeError("runtime configuration drift")
    for output in timings:
        measured = output["times"]
        if abs(measured["write_complete"] - sum(measured[key] for key in ("begin", "write", "commit"))) > 0.000003:
            raise RuntimeError("stage sum mismatch")
        if any(measured[key] < 0 for key in ("write", "scan", "point", "create", "commit")):
            raise RuntimeError("invalid timing")
    for output in diagnostics:
        counters = {(phase, name): int(value) for phase, name, value in output["counters"]}
        if counters["write", "run"] != args.count or counters["scan", "run"] != 1 or counters["point", "run"] != 100:
            raise RuntimeError("statement execution count mismatch")
        if output["layout"] == "none":
            if counters["point", "fullscan_step"] != 100 * (args.count - 1):
                raise RuntimeError("unexpected unindexed scan count")
        elif counters["point", "fullscan_step"] != 0:
            raise RuntimeError("indexed lookup unexpectedly scans")
    with (args.out / "samples.tsv").open("w") as stream:
        writer = csv.writer(stream, delimiter="\t")
        writer.writerow(["config", "layout", "repeat", "phase", "elapsed", "unit"])
        for output in timings:
            for phase, value in output["times"].items():
                writer.writerow([output["config_name"], output["layout"], output["repeat"], phase, value, "ms"])
    summaries = []
    for config, layout in itertools.product(CONFIGS, LAYOUTS):
        members = [item for item in timings if item["config_name"] == config and item["layout"] == layout]
        if len(members) != args.runs:
            raise RuntimeError("missing samples")
        for phase in members[0]["times"]:
            values = [item["times"][phase] for item in members]
            summaries.append([config, layout, phase, len(values), statistics.mean(values),
                              statistics.median(values), min(values), max(values), "ms"])
    with (args.out / "summary.tsv").open("w") as stream:
        writer = csv.writer(stream, delimiter="\t")
        writer.writerow(["config", "layout", "phase", "samples", "mean", "median", "min", "max", "unit"])
        writer.writerows(summaries)
    save_json(args.out / "audit.json", {
        "passed": True, "correctness_cases": len(checks), "formal_samples": len(timings),
        "diagnostic_full_field_checks": len(diagnostics), "groups": len(LAYOUTS) * len(CONFIGS),
        "runs_per_group": args.runs, "input": expected_input,
        "elapsed_s": time.monotonic() - started,
        "checks": ["same-inputs", "same-requests", "same-config-per-storage", "same-consumed-result",
                   "complete-samples", "stage-sums", "statement-counts", "scan-vs-search",
                   "full-fields-and-constraints-in-separate-diagnostics"],
    })
    save_json(args.out / "progress.json", {"stage": "complete", "elapsed_s": time.monotonic() - started})
    print(f"COMPLETE: {args.out}; elapsed={(time.monotonic()-started)/60:.1f} min", flush=True)


if __name__ == "__main__":
    main()
