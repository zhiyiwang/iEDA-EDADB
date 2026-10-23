import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import statistics
import subprocess
import time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.binary = args.binary.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    args.settle = 0.5
    source_dir = Path(__file__).parent.parent / "sqlite-vs-text"
    spec = importlib.util.spec_from_file_location("frozen_runner", source_dir / "run.py")
    original = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(original)
    cpu = min(os.sched_getaffinity(0))
    os.sched_setaffinity(0, {cpu})
    files = [Path(__file__), source_dir / "run.py", source_dir / "benchmark.cpp", args.binary,
             Path(__file__).with_name("CMakeLists.txt")]
    manifest = dict(cpu=cpu, parent_commit=subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
                    hashes={str(path.resolve()): sha(path) for path in files},
                    library=subprocess.check_output(["ldd", str(args.binary)], text=True),
                    compiler=subprocess.check_output(["c++", "--version"], text=True).splitlines()[0],
                    count=1000000, repeats=5, cache="A same connection first read; B/text file pre-read outside timer",
                    correctness="full-field warmup and small checks; timing reads use equal digest, not full-field comparisons")
    original.save(args.output / "manifest.json", manifest)
    jobs = [(setting, route) for setting in ("A", "B-batch") for route in ("text", "sqlite", "batch10", "batch100")]
    checks = 0
    started = time.monotonic()
    for setting, route in jobs:
        for count in (0, 1, 8, 101, 1000000):
            original.invoke(args, setting, route, f"check{count}", count, "check")
            checks += 1
    print("Full-field checks and full-size warmup: 40 PASS", flush=True)
    rows, counters = [], []
    for repeat in range(5):
        ordered = jobs[repeat:] + jobs[:repeat]
        if repeat % 2:
            ordered.reverse()
        for setting, route in ordered:
            measured, _ = original.invoke(args, setting, route, f"formal{repeat}", 1000000, "timing")
            rows.extend(measured)
            original.write_tsv(args.output / "samples.tsv", rows)
            print(f"Heartbeat: {len(rows)//2}/40 paired read/write samples, elapsed {time.monotonic()-started:.0f}s", flush=True)
    for setting, route in jobs:
        if route != "text":
            _, counted = original.invoke(args, setting, route, "counters", 1000000, "counters")
            counters.extend(counted)
    original.write_tsv(args.output / "counters.tsv", counters)
    summary, comparisons = [], []
    for setting, route in jobs:
        for operation in ("write", "read"):
            group = [row for row in rows if row["config"] == setting and row["route"] == route and row["operation"] == operation]
            assert len(group) == 5
            summary.append(dict(config=setting, route=route, operation=operation,
                                **{phase:statistics.median(row[phase] for row in group) for phase in original.STAGES},
                                min_ms=min(row["data_ms"] for row in group), max_ms=max(row["data_ms"] for row in group)))
            if route != "text":
                reference = {row["sample"]:row["data_ms"] for row in rows if row["config"] == setting and row["route"] == "text" and row["operation"] == operation}
                delta = [row["data_ms"] - reference[row["sample"]] for row in group]
                comparisons.append(dict(config=setting, route=route, operation=operation,
                                        ratio_to_text=statistics.median(row["data_ms"] for row in group)/statistics.median(reference.values()),
                                        paired_delta_ms=statistics.median(delta), min_delta_ms=min(delta), max_delta_ms=max(delta)))
    original.save(args.output / "summary.json", summary)
    original.save(args.output / "comparisons.json", comparisons)
    assert all(sha(path) == value for path, value in manifest["hashes"].items())
    actual_config = {}
    for setting in ("A", "B-batch"):
        log = (args.output / f"{setting}-sqlite-formal0-n1000000.log").read_text()
        assert f"CONFIG\tjournal_mode\t{'memory' if setting == 'A' else 'delete'}\n" in log
        assert f"CONFIG\tsynchronous\t{0 if setting == 'A' else 2}\n" in log
        actual_config[setting] = [line.split("\t")[1:] for line in log.splitlines() if line.startswith("CONFIG\t")]
    original.save(args.output / "config.json", actual_config)
    original.save(args.output / "audit.json", dict(status="PASS", full_field_checks=checks, formal_processes=40,
                                                    timing_rows=80, counter_processes=6, hashes_unchanged=True,
                                                    actual_config_verified=True))
    print("Completed: 40 checks, 80 timing rows, 6 counter runs PASS", flush=True)


if __name__ == "__main__":
    main()
