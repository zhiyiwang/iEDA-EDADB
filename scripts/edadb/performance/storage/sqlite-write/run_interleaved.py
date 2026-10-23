import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import statistics
import subprocess
import time


def save(path, value):
    path.write_text(json.dumps(value, indent=2))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_tsv(path, rows):
    with path.open("w") as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]), delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.binary = args.binary.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    cpu = min(os.sched_getaffinity(0))
    files = [args.binary, Path(__file__), Path(__file__).with_name("interleaved.cpp"),
             Path(__file__).parent.parent / "sqlite-vs-text/benchmark.cpp"]
    hashes = {str(path.resolve()): sha(path) for path in files}
    save(args.output / "manifest.json", dict(hashes=hashes, cpu=cpu,
        parent_commit=subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
        library=subprocess.check_output(["ldd", str(args.binary)], text=True),
        compiler=subprocess.check_output(["c++", "--version"], text=True).splitlines()[0],
        count_per_table=100000, repeats=5, formal_full_field_validation="after timed reading"))
    orders = [("1", "all"), ("10", "all"), ("all", "all"), ("all", "1"), ("all", "10")]
    jobs = [(backend, setting, writing, reading) for setting in ("memory", "disk")
            for backend in ("text", "sqlite") for writing, reading in orders]
    samples, diagnostics, checks, configurations = [], [], [], {}
    stages = ("init", "create", "begin", "data", "commit", "close")

    def invoke(job, count, mode, repeat):
        backend, setting, writing, reading = job
        label = f"{backend}-{setting}-w{writing}-r{reading}-{repeat}-n{count}"
        data = args.output / (label + ".data")
        command = ["taskset", "-c", str(cpu), str(args.binary), backend, setting,
                   str(count), str(data), writing, reading, mode]
        result = subprocess.run(command, text=True, capture_output=True, timeout=180)
        (args.output / (label + ".log")).write_text(result.stdout + result.stderr)
        assert result.returncode == 0, (label, result.stderr)
        assert "CHECK\tfull fields\tPASS" in result.stdout
        checks.append(label)
        timings, totals = {}, {}
        base = dict(backend=backend, setting=setting, write_block=writing, read_block=reading)
        for line in result.stdout.splitlines():
            fields = line.split("\t")
            if fields[0] == "TIME":
                timings[fields[1]] = dict(zip(stages, map(float, fields[2:])))
            elif fields[0] == "ROWS":
                totals[fields[1]] = list(map(int, fields[2:]))
            elif fields[0] == "COUNTER":
                diagnostics.append(dict(**base, phase=fields[1], counter=fields[2], value=int(fields[3])))
        assert totals["write"] == totals["read"] and totals["read"][0] == count * 3
        if backend == "sqlite":
            assert f"CONFIG\tjournal_mode\t{'memory' if setting == 'memory' else 'delete'}\n" in result.stdout
            assert f"CONFIG\tsynchronous\t{0 if setting == 'memory' else 2}\n" in result.stdout
            configurations[setting] = [line.split("\t")[1:] for line in result.stdout.splitlines() if line.startswith("CONFIG\t")]
        if mode == "timing":
            assert set(timings) == {"write", "read"}
            for phase in ("write", "read"):
                samples.append(dict(**base, operation=phase, repeat=repeat, count_per_table=count,
                                    **timings[phase], digest=totals[phase][1]))
            write_tsv(args.output / "samples.tsv", samples)
        for path in [data] + [Path(str(data) + f".{table}") for table in range(3)]:
            path.unlink(missing_ok=True)

    for job in jobs:
        for count in (0, 1, 8, 101, 100000):
            invoke(job, count, "check", f"check{count}")
    print("100 small/full-size checks PASS; formal 100 processes starting", flush=True)
    start = time.monotonic()
    for repeat in range(5):
        ordered = jobs[repeat:] + jobs[:repeat]
        if repeat % 2:
            ordered.reverse()
        for job in ordered:
            time.sleep(0.5)
            invoke(job, 100000, "timing", str(repeat))
            print(f"Heartbeat {len(samples)//2}/100: {'/'.join(job)}; elapsed {time.monotonic()-start:.0f}s", flush=True)
    for job in jobs:
        if job[0] == "sqlite":
            invoke(job, 100000, "counters", "diagnostic")
            group = [row for row in diagnostics if all(row[key] == value for key, value in zip(("backend", "setting", "write_block", "read_block"), job))]
            for phase, expected in (("write", 300000), ("read", 3)):
                assert sum(row["value"] for row in group if row["phase"] == phase and row["counter"] == "run") == expected
                for counter in ("sort", "reprepare"):
                    assert sum(row["value"] for row in group if row["phase"] == phase and row["counter"] == counter) == 0
    write_tsv(args.output / "counters.tsv", diagnostics)
    summary = []
    for job in jobs:
        base = dict(zip(("backend", "setting", "write_block", "read_block"), job))
        for operation in ("write", "read"):
            group = [row for row in samples if all(row[key] == value for key,value in base.items()) and row["operation"] == operation]
            assert len(group) == 5
            summary.append(dict(**base, operation=operation,
                                **{stage:statistics.median(row[stage] for row in group) for stage in stages},
                                min_ms=min(row["data"] for row in group), max_ms=max(row["data"] for row in group)))
    assert all(sha(path) == expected for path, expected in hashes.items())
    save(args.output / "summary.json", summary)
    save(args.output / "config.json", configurations)
    save(args.output / "audit.json", dict(status="PASS", checks=len(checks), full_warmup_checks=100,
        formal_processes=100, timing_rows=200, counter_processes=10, source_unchanged=True,
        expected_read_runs=3, expected_write_runs=300000))
    print("Completed: 200 timing rows, full-field verification, statement counts PASS", flush=True)


if __name__ == "__main__":
    main()
