import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import statistics
import subprocess
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--duckdb-threads", type=int, choices=[1])
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("runs must be positive")
    root = Path(__file__).resolve().parents[4]
    binary = args.binary.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    workspace = root / "scripts/design/sky130_gcd"
    source = workspace / "result/iPL_filler_result.def"
    benchmark = root / "scripts/edadb/performance/benchmark.tcl"
    environment = os.environ.copy()
    environment.pop("LD_PRELOAD", None)
    environment.pop("EDADB_BENCH_DUCKDB_THREADS", None)
    if args.duckdb_threads is not None:
        environment["EDADB_BENCH_DUCKDB_THREADS"] = str(args.duckdb_threads)
    environment.update(WORKSPACE=str(workspace), CONFIG_DIR=str(workspace / "iEDA_config"),
                       FOUNDRY_DIR=str(root / "scripts/foundry/sky130"),
                       TCL_SCRIPT_DIR=str(workspace / "script"),
                       DESIGN_TCL_SCRIPT_DIR=str(workspace / "script"), DESIGN_TOP="gcd",
                       NETLIST_FILE="/dev/null", SDC_FILE="/dev/null", SPEF_FILE="/dev/null",
                       INPUT_DEF=str(source))
    manifest = {
        "parent_head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
        "core_head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root / "src/database/edadb/core", text=True).strip(),
        "binary": str(binary), "binary_sha256": digest(binary),
        "input": str(source), "input_bytes": source.stat().st_size, "input_sha256": digest(source),
        "benchmark_sha256": digest(benchmark), "runner_sha256": digest(Path(__file__)),
        "lef_sha256": {str(path.relative_to(root)): digest(path)
                       for path in sorted((root / "scripts/foundry/sky130/lef").glob("*")) if path.is_file()},
        "cpu": 2, "cache": "warm", "warmups": 1, "runs": args.runs,
        "formal_stage_timing": "OFF", "diagnostic_stage_timing": "ON",
        "native": "same integrated binary DEF control, not pre-EDADB binary",
        "duckdb_threads_override": args.duckdb_threads,
        "duckdb_settings": "defaults except explicit thread override; no memory/durability overrides",
        "uname": subprocess.check_output(["uname", "-a"], text=True).strip(),
        "lscpu": subprocess.check_output(["lscpu"], text=True),
        "linked_libraries": subprocess.check_output(["ldd", str(binary)], text=True),
    }
    for relative in ("CMakeCache.txt", "src/database/edadb/idb/CMakeFiles/db_edadb.dir/flags.make"):
        path = args.build.resolve() / relative
        (output / path.name).write_text(path.read_text())
    library = Path("/home/zhiyiwang/cs/db/duckdb/sources/v1.5.6/libduckdb.so")
    manifest["duckdb_library_sha256"] = digest(library)
    manifest["adapter_sha256"] = {
        relative: digest(root / relative) for relative in (
            "src/database/manager/builder/builder.cpp",
            "src/database/manager/builder/def_builder/def_read_edadb.cpp",
            "src/database/manager/builder/def_builder/def_write_edadb.cpp",
            "src/database/edadb/idb/edadb_idb_init.cpp")}
    (output / "production-diff.txt").write_text(subprocess.check_output(
        ["git", "diff", "--", "src/database/manager", "src/database/edadb/core",
         "src/database/edadb/idb/edadb_idb_init.cpp"], cwd=root, text=True))
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    rows = []
    reference = None
    audit = {"status": "RUNNING", "roundtrips": 0, "formal_rounds": 0}

    def run(mode, folder, timing):
        settings = environment.copy()
        settings.update(PERF_MODE=mode, EDADB_STAGE_TIMING=str(timing),
                        EDADB_DB_PATH=str(folder / "design.duckdb"), OUTPUT_DEF=str(folder / f"{mode}.def"))
        source.read_bytes()
        if mode == "read":
            (folder / "design.duckdb").read_bytes()
        with (folder / f"{mode}.log").open("w") as log:
            subprocess.run(["/usr/bin/time", "-v", "-o", str(folder / f"{mode}.resources"),
                            "taskset", "-c", "2", str(binary), "-script", str(benchmark)],
                           cwd=folder, env=settings, stdout=log, stderr=subprocess.STDOUT,
                           check=True, timeout=900)
        content = (folder / f"{mode}.log").read_text(errors="replace")
        if mode != "native" and args.duckdb_threads is not None:
            expected_config = f"EDADB_BENCH_CONFIG\tduckdb\tthreads\t{args.duckdb_threads}"
            if content.splitlines().count(expected_config) != 1:
                raise RuntimeError(f"Missing/duplicate thread verification: {folder}/{mode}.log")
        measurements = re.findall(r"^EDADB_PERF\t([^\t]+)\t(\d+)$", content, re.MULTILINE)
        expected = {"native": {"native_def_read", "native_def_write"},
                    "write": {"edadb_write"}, "read": {"edadb_read"}}[mode]
        if len(measurements) != len(expected) or {name for name, _ in measurements} != expected:
            raise RuntimeError(f"Missing/duplicate command markers: {folder}/{mode}.log")
        return measurements

    try:
        for round_number in range(args.runs + 1):
            print(f"{time.strftime('%H:%M:%S')} round={round_number}/{args.runs}", flush=True)
            folder = output / f"round-{round_number}"
            folder.mkdir()
            measurements = []
            modes = ("native", "write", "read") if round_number % 2 == 0 else ("write", "read", "native")
            for mode in modes:
                print(f"  {mode}", flush=True)
                measurements.extend(run(mode, folder, 0))
            native = (folder / "native.def").read_bytes()
            restored = (folder / "read.def").read_bytes()
            if native != restored or (reference is not None and native != reference):
                raise RuntimeError(f"Strict DEF roundtrip mismatch: {folder}")
            reference = native
            audit["roundtrips"] += 1
            if round_number:
                rows.extend((round_number, name, float(value) / 1000) for name, value in measurements)
                audit["formal_rounds"] += 1
            with (output / "samples.tsv").open("w") as stream:
                writer = csv.writer(stream, delimiter="\t")
                writer.writerow(("round", "phase", "ms"))
                writer.writerows(rows)
        folder = output / "diagnostic-on"
        folder.mkdir()
        run("write", folder, 1)
        run("read", folder, 1)
        if (folder / "read.def").read_bytes() != reference:
            raise RuntimeError("Diagnostic roundtrip mismatch")
        with (output / "summary.tsv").open("w") as stream:
            writer = csv.writer(stream, delimiter="\t")
            writer.writerow(("phase", "n", "median_ms", "min_ms", "max_ms"))
            for phase in sorted({row[1] for row in rows}):
                values = [row[2] for row in rows if row[1] == phase]
                writer.writerow((phase, len(values), statistics.median(values), min(values), max(values)))
        audit.update(status="PASS", canonical_def_sha256=hashlib.sha256(reference).hexdigest())
    except Exception as error:
        audit.update(status="FAIL", error=str(error))
        raise
    finally:
        (output / "audit.json").write_text(json.dumps(audit, indent=2) + "\n")


if __name__ == "__main__":
    main()
