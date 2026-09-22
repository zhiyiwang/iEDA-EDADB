"""Run serial timing and separate SQLite counters; see docs/test_plan.md."""
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

ROUTES = ("text", "sqlite", "static", "batch10", "batch100", "step-only")
STAGES = ("init_ms", "create_ms", "begin_ms", "data_ms", "commit_ms", "close_ms")


def save(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def invoke(args, config, route, sample, count, mode):
    stem = args.output / f"{config}-{route}-{sample}-n{count}"
    data = stem.with_suffix(".data")
    command = [str(args.binary), route, config, str(count), str(data),
               mode, "0", str(args.settle)]
    if mode == "timing":
        time.sleep(args.settle)
    with stem.with_suffix(".log").open("w") as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
    lines = stem.with_suffix(".log").read_text().splitlines()
    timings, results, counters = {}, {}, []
    for line in lines:
        fields = line.split("\t")
        if fields[0] == "TIME":
            timings[fields[1]] = dict(zip(STAGES, map(float, fields[2:]), strict=True))
        elif fields[0] == "ROWS":
            results[fields[1]] = (int(fields[2]), int(fields[3]))
        elif fields[0] == "COUNTER":
            counters.append(dict(config=config, route=route, phase=fields[1],
                                 counter=fields[2], value=int(fields[3]),
                                 count=count, log=stem.with_suffix(".log").name))
    assert results["read"][0] == results["write"][0] == count
    if route != "step-only" or mode == "check":
        assert results["read"] == results["write"]
    else:
        assert results["read"][1] == 0
    assert "CHECK\t" + ("full fields" if mode == "check" else "digest") + "\tPASS" in lines
    assert not any(line.startswith("PROBE\t") for line in lines)
    if mode == "check" and route != "text" and count:
        assert "CHECK\tcorruption rejected" in lines
    if mode == "counters":
        assert not timings and len(counters) == 32
        values = {(row["phase"], row["counter"]): row["value"] for row in counters}
        batch = {"batch10": 10, "batch100": 100}.get(route, 1)
        assert count % batch == 0
        assert values["write", "run"] == count // batch and values["read", "run"] == 1
        for phase in ("write", "read"):
            for name in ("sort", "autoindex", "reprepare"):
                assert values[phase, name] == 0
        assert values["read", "fullscan_step"] == max(0, count - 1)
        assert values["write", "vm_step"] > 0 or count == 0
        if config == "A":
            assert all(row["value"] == 0 for row in counters
                       if row["counter"] in ("cache_write", "cache_spill"))
        assert any(line.startswith("EQP\t") for line in lines)
    else:
        assert not counters and set(timings) == {"write", "read"}
    rows = []
    for operation, timing in timings.items():
        rows.append(dict(config=config, route=route, sample=sample, count=count,
                         operation=operation, unit="ms", **timing,
                         rows=results[operation][0], digest=results[operation][1],
                         log=stem.with_suffix(".log").name))
    data.unlink(missing_ok=True)
    print(time.strftime("%H:%M:%S"), config, route, sample, "PASS", flush=True)
    return rows, counters


def write_tsv(path, rows):
    with path.open("w") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]), delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def summarize(output, rows):
    write_tsv(output / "samples.tsv", rows)
    groups = []
    for config in ("A", "B-batch"):
        for route in ROUTES:
            for operation in ("write", "read"):
                selected = [row for row in rows if
                            (row["config"], row["route"], row["operation"]) == (config, route, operation)]
                group = dict(config=config, route=route, operation=operation, samples=len(selected))
                for key in STAGES:
                    values = [row[key] for row in selected]
                    for name, function in (("mean", statistics.mean), ("median", statistics.median),
                                           ("min", min), ("max", max)):
                        group[name + "_" + key] = function(values)
                groups.append(group)
    save(output / "summary.json", groups)
    # Paired round differences, not differences across machines or old batches.
    deltas = []
    for config in ("A", "B-batch"):
        for operation in ("write", "read"):
            pairs = [("sqlite", "text")]
            if operation == "write":
                pairs.extend((("sqlite", "static"), ("sqlite", "batch10"), ("sqlite", "batch100")))
            else:
                pairs.append(("sqlite", "step-only"))
            for left, right in pairs:
                paired = []
                for sample in sorted({row["sample"] for row in rows}):
                    values = {row["route"]: row["data_ms"] for row in rows
                              if (row["config"], row["operation"], row["sample"]) == (config, operation, sample)}
                    paired.append(values[left] - values[right])
                median = statistics.median(paired)
                deltas.append(dict(config=config, operation=operation, difference=f"{left}-{right}",
                    paired_ms=paired, median_ms=median, min_ms=min(paired), max_ms=max(paired),
                    median_ns_per_record=median * 1e6 / rows[0]["count"],
                    same_sign=all(value > 0 for value in paired) or all(value < 0 for value in paired)))
    save(output / "deltas.json", deltas)
    lines = ["# SQLite/text results", "", "Units: ms. Five-sample defaults; actual sample counts are in summary.json.",
             "Counters and correctness are separate invocations, excluded from timings.", ""]
    for operation in ("write", "read"):
        lines += [f"## {operation} data", "", "| Config | Route | Median | Min | Max |",
                  "| --- | --- | ---: | ---: | ---: |"]
        for group in groups:
            if group["operation"] == operation:
                lines.append(f"| {group['config']} | {group['route']} | {group['median_data_ms']:.3f} | "
                             f"{group['min_data_ms']:.3f} | {group['max_data_ms']:.3f} |")
    lines += ["", "## Separate write stages", "", "| Config | Route | Init | Create | BEGIN | COMMIT | Close |",
              "| --- | --- | ---: | ---: | ---: | ---: | ---: |"]
    for group in groups:
        if group["operation"] == "write":
            values = [group["median_" + key + "_ms"] for key in ("init", "create", "begin", "commit", "close")]
            lines.append(f"| {group['config']} | {group['route']} | " +
                         " | ".join("—" if value < 0 else f"{value:.3f}" for value in values) + " |")
    lines += ["", "Evidence: [samples](samples.tsv), [summary](summary.json), [paired deltas](deltas.json),",
              "[counters](counters.tsv), [checks](checks.json), [manifest](manifest.json), [audit](audit.json).",
              "Counters measure work, not durations. STATIC delta is a net change, not memcpy exclusive time."]
    (output / "report.md").write_text("\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--count", type=int, default=1000000)
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--settle", type=float, default=1)
    parser.add_argument("--cpu", type=int, default=min(os.sched_getaffinity(0)))
    args = parser.parse_args()
    assert 0 < args.count <= 1000000 and args.runs > 0 and args.settle >= 0
    assert args.cpu in os.sched_getaffinity(0)
    os.sched_setaffinity(0, {args.cpu})
    args.binary = args.binary.resolve()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    source_dir = args.output / "source"
    shutil.copytree(Path(__file__).parent, source_dir, ignore=shutil.ignore_patterns("__pycache__"))
    for name in ("flags.make", "link.txt"):
        shutil.copy2(args.binary.parent / "CMakeFiles/benchmark.dir" / name, args.output / name)
    shutil.copy2(args.binary, args.output / "benchmark")
    save(args.output / "manifest.json", dict(binary=str(args.binary), binary_sha256=sha(args.binary),
        sources={str(path.relative_to(source_dir)): sha(path) for path in source_dir.rglob("*") if path.is_file()},
        count=args.count, runs=args.runs, settle=args.settle, cpu=args.cpu,
        git=subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
        uid=os.getuid(), process_status=Path("/proc/self/status").read_text(),
        hardware=subprocess.check_output(["lscpu"], text=True), memory=Path("/proc/meminfo").read_text(),
        libraries=subprocess.check_output(["ldd", str(args.binary)], text=True)))
    # Execute the archived binary, not a build path that another compilation can replace.
    args.binary = args.output / "benchmark"
    checks, rows, counters = [], [], []
    for config in ("A", "B-batch"):
        for route in ROUTES:
            # The full-size correctness pass also serves as the warm-up.
            for count in dict.fromkeys((0, 1, 8, 101, args.count)):
                invoke(args, config, route, "check", count, "check")
                checks.append(dict(config=config, route=route, count=count, status="PASS"))
        save(args.output / "checks.json", checks)
        for sample in range(args.runs):
            order = ROUTES[sample % len(ROUTES):] + ROUTES[:sample % len(ROUTES)]
            for route in order:
                timings, _ = invoke(args, config, route, f"run-{sample+1}", args.count, "timing")
                rows.extend(timings)
                write_tsv(args.output / "samples.tsv", rows)
    # Diagnostics cannot perturb any later formal sample.
    for config in ("A", "B-batch"):
        for route in ROUTES[1:]:
            _, measured = invoke(args, config, route, "counters", args.count, "counters")
            counters.extend(measured)
    write_tsv(args.output / "counters.tsv", counters)
    summarize(args.output, rows)
    save(args.output / "audit.json", dict(status="PASS", checks=len(checks), timing_rows=len(rows),
         groups=2 * len(ROUTES) * 2, samples_per_group=args.runs,
         counter_runs=2 * (len(ROUTES) - 1), counter_rows=len(counters),
         scope="row/value checks and SQL counter invariants; no counters in timing mode"))
    print("COMPLETE", args.output, flush=True)


if __name__ == "__main__":
    main()
