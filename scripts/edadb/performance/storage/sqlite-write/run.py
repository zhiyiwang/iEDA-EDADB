import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import statistics
import subprocess
import time


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    cpu = min(os.sched_getaffinity(0))
    sources = [Path(__file__), Path(__file__).with_name("benchmark.cpp"),
               Path(__file__).parent.parent / "sqlite-vs-text/benchmark.cpp"]
    hashes = {str(path.resolve()): digest(path) for path in sources + [args.binary]}
    manifest = {"hashes": hashes, "cpu": cpu, "uname": list(os.uname()),
                "parent_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
                "library": subprocess.check_output(["ldd", str(args.binary)], text=True),
                "repetitions": 5, "validation": "all fields outside timers, including formal samples"}
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2))
    jobs = [(setting, route, 100000 if route.startswith("switch") or route == "grouped" else 1000000)
            for setting in ("memory", "disk")
            for route in ("text", "single", "batch10", "batch100", "switch1", "switch10", "grouped")]
    samples, checks, diagnostics = [], [], []
    started = time.monotonic()

    def run(setting, route, count, mode, label):
        path = args.output / (label + ".data")
        command = ["taskset", "-c", str(cpu), str(args.binary), route, setting, str(count), str(path), mode]
        completed = subprocess.run(command, text=True, capture_output=True, timeout=180)
        (args.output / (label + ".log")).write_text(completed.stdout + completed.stderr)
        if completed.returncode or "CHECK\tfull fields\tPASS" not in completed.stdout:
            raise RuntimeError(f"Failed {label}: {completed.stderr}")
        path.unlink(missing_ok=True)
        checks.append({"label": label, "rows_per_table": count, "status": "PASS"})
        return completed.stdout

    for setting, route, _ in jobs:
        for count in (0, 1, 8, 101):
            run(setting, route, count, "check", f"check-{setting}-{route}-{count}")
    print("Small correctness: 56 PASS", flush=True)
    for setting, route, count in jobs:
        run(setting, route, count, "check", f"warm-{setting}-{route}")
    print("Full-size warmup: 14 PASS; formal 70 samples starting", flush=True)
    for repeat in range(5):
        ordered = jobs[repeat:] + jobs[:repeat]
        if repeat % 2:
            ordered = list(reversed(ordered))
        for setting, route, count in ordered:
            time.sleep(0.5)
            output = run(setting, route, count, "timing", f"sample-{repeat}-{setting}-{route}")
            timing = next(line.split("\t")[2:] for line in output.splitlines() if line.startswith("TIME\twrite"))
            row = dict(zip(("init", "create", "begin", "data", "commit", "close"), map(float, timing)))
            row.update(setting=setting, route=route, count=count, repeat=repeat)
            samples.append(row)
            print(f"Heartbeat {len(samples)}/70: {setting}/{route} data={row['data']:.3f} ms; elapsed={time.monotonic()-started:.0f}s", flush=True)
    for setting, route, count in jobs:
        if route == "text":
            continue
        output = run(setting, route, count, "counters", f"counter-{setting}-{route}")
        for line in output.splitlines():
            if line.startswith("COUNTER\t"):
                _, phase, name, value = line.split("\t")
                diagnostics.append(dict(setting=setting, route=route, phase=phase, name=name, value=int(value)))
        expected = count * 3 if route.startswith("switch") or route == "grouped" else count // {"single": 1, "batch10": 10, "batch100": 100}[route]
        actual = sum(item["value"] for item in diagnostics if item["setting"] == setting and item["route"] == route and item["phase"] == "write" and item["name"] == "run")
        assert actual == expected, (route, actual, expected)
    for name, rows in (("samples", samples), ("counters", diagnostics)):
        with (args.output / (name + ".tsv")).open("w") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(rows[0]), delimiter="\t")
            writer.writeheader()
            writer.writerows(rows)
    summary = []
    for setting, route, count in jobs:
        group = [row for row in samples if row["setting"] == setting and row["route"] == route]
        summary.append(dict(setting=setting, route=route, count=count,
                            **{phase: statistics.median(row[phase] for row in group) for phase in ("init", "create", "begin", "data", "commit", "close")},
                            data_min=min(row["data"] for row in group), data_max=max(row["data"] for row in group)))
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2))
    assert hashes == {path: digest(path) for path in hashes}
    (args.output / "audit.json").write_text(json.dumps({"status": "PASS", "checks": len(checks), "formal_samples": len(samples), "counter_runs": 12, "source_unchanged": True, "checks_detail": checks}, indent=2))
    print("Completed: full-field validation, RUN counts and source hashes PASS", flush=True)


if __name__ == "__main__":
    main()
