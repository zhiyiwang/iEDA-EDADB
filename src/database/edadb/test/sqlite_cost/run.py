"""Current-milestone diagnostics; separate timing, counters, perf and syscall runs."""
import argparse
import csv
import hashlib
import json
import os
import pwd
from pathlib import Path
import re
import sqlite3
import statistics
import subprocess
import time

ROOT = Path(__file__).resolve().parents[5]
HERE = Path(__file__).resolve().parent


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fixture(output):
    workspace = ROOT / "scripts/design/sky130_gcd"
    lines = (workspace / "result/iRT_result.def").read_text().splitlines(keepends=True)
    begin = next(index for index, line in enumerate(lines) if line.startswith("NETS "))
    end = next(index for index in range(begin + 1, len(lines)) if lines[index].startswith("END NETS"))
    records = []
    for line in lines[begin + 1:end]:
        if line.startswith("- "):
            records.append([])
        if records:
            records[-1].append(line)
    template = max(records, key=len)
    stress = output / "routed-stress.def"
    text = "".join(lines[:begin] + [f"NETS {len(records) + 1000} ;\n"]
                   + [line for record in records for line in record]
                   + [line for index in range(1000)
                      for line in [f"- __stream_shadow_stress_{index:06d}\n", *template[1:]]]
                   + lines[end:])
    if stress.exists():
        assert stress.read_text() == text
    else:
        stress.write_text(text)
    return {"filler": workspace / "result/iPL_filler_result.def", "routed-stress": stress}


def environment():
    workspace = ROOT / "scripts/design/sky130_gcd"
    result = os.environ.copy()
    result.pop("LD_PRELOAD", None)
    result.pop("EDADB_LEAF_BATCH_READ", None)
    result.update(WORKSPACE=str(workspace), CONFIG_DIR=str(workspace / "iEDA_config"),
                  FOUNDRY_DIR=str(ROOT / "scripts/foundry/sky130"),
                  TCL_SCRIPT_DIR=str(workspace / "script"), DESIGN_TCL_SCRIPT_DIR=str(workspace / "script"),
                  DESIGN_TOP="gcd", NETLIST_FILE="/dev/null", SDC_FILE="/dev/null", SPEF_FILE="/dev/null")
    return result


def invoke(args, datasets, dataset, folder, operation, observation="on", instrument=None):
    folder.mkdir(parents=True, exist_ok=True)
    settings = environment()
    settings.update(INPUT_DEF=str(datasets[dataset]), PERF_MODE=operation,
                    OUTPUT_DEF=str(folder / f"{operation}.def"), EDADB_DB_PATH=str(folder / "edadb.db"),
                    EDADB_STAGE_TIMING="1" if observation == "on" else "0")
    datasets[dataset].read_bytes()
    if operation == "read":
        (folder / "edadb.db").read_bytes()
    command = ["taskset", "-c", "2", str(args.binary), "-script",
               str(ROOT / "scripts/edadb/performance/benchmark.tcl")]
    if instrument in ("counters", "markers", "perf", "io"):
        probe = "counters" if instrument == "counters" else "markers"
        command = ["env", f"LD_PRELOAD={args.output / (probe + '.so')}", *command]
    if instrument == "perf":
        data = folder / f"{operation}.perf.data"
        data.touch()
        exported = [f"{name}={value}" for name, value in settings.items() if name in environment_keys()]
        command = ["sudo", "-n", "perf", "record", "-q", "-m", "8192", "-F", "999", "-e", "cycles:u",
                   "--clockid", "mono", "--call-graph", "dwarf,16384", "-o", str(data), "--", *command]
        split = command.index("--") + 1
        command[split:split] = ["/usr/sbin/runuser", "-u", pwd.getpwuid(os.getuid()).pw_name,
                               "--", "env", *exported]
    elif instrument == "io":
        command = ["strace", "-qq", "-ttt", "-T", "-yy", "-o", str(folder / f"{operation}.strace"),
                   "-e", "trace=pread64,pwrite64,fsync,fdatasync,openat,close,unlink,ftruncate", *command]
    command = ["/usr/bin/time", "-f", "%M\t%U\t%S\t%e", "-o", str(folder / f"{operation}.resources"), *command]
    with (folder / f"{operation}.log").open("w") as log:
        subprocess.run(command, cwd=args.output, env=settings, stdout=log, stderr=subprocess.STDOUT, check=True)
    values = {}
    for line in (folder / f"{operation}.log").read_text().splitlines():
        parts = line.split("\t")
        if parts[0] == "EDADB_PERF":
            values["command_ms"] = int(parts[2]) / 1000
        elif parts[0] == "EDADB_STAGE":
            values["stage." + parts[2] + "_ms"] = int(parts[4]) / 1e6
        elif parts[0] == "EDADB_DETAIL":
            values[parts[2] + "_ms"] = int(parts[4]) / 1e6
    if operation != "native":
        assert "command_ms" in values
        if observation == "on":
            accounted = sum(values[f"stage.{item}_ms"] for item in ("init", "create", operation, "other"))
            assert abs(accounted - values["stage.command_ms"]) < 1e-5
            if operation == "write":
                values["write_without_transaction_ms"] = (values["stage.write_ms"]
                    - values["adapter.transaction.begin_ms"] - values["adapter.transaction.commit_ms"])
                assert values["write_without_transaction_ms"] >= 0
        else:
            assert not any(key.startswith("stage.") for key in values)
    values["peak_rss_kib"] = int((folder / f"{operation}.resources").read_text().split()[0])
    return values


def environment_keys():
    return {"WORKSPACE", "CONFIG_DIR", "FOUNDRY_DIR", "TCL_SCRIPT_DIR", "DESIGN_TCL_SCRIPT_DIR",
            "DESIGN_TOP", "NETLIST_FILE", "SDC_FILE", "SPEF_FILE", "INPUT_DEF", "PERF_MODE",
            "OUTPUT_DEF", "EDADB_DB_PATH", "EDADB_STAGE_TIMING"}


def audit(folder, reference, expected_digest=None):
    assert (folder / "read.def").read_bytes() == reference, folder
    with sqlite3.connect(f"file:{folder / 'edadb.db'}?mode=ro", uri=True) as connection:
        assert connection.execute("PRAGMA integrity_check").fetchall() == [("ok",)]
        assert connection.execute("PRAGMA foreign_key_check").fetchall() == []
        digest = hashlib.sha256("\n".join(connection.iterdump()).encode()).hexdigest()
    if expected_digest:
        assert digest == expected_digest, folder
    return digest


def table(path, rows):
    with path.open("w") as stream:
        writer = csv.DictWriter(stream, fieldnames=rows[0].keys(), delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("phase", choices=["smoke", "timing", "counters", "markers", "perf", "io", "summarize"])
    parser.add_argument("--binary", type=Path, default=ROOT / "bin-release/iEDA")
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output = args.output.resolve()
    args.binary = args.binary.resolve()
    args.build = args.build.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    datasets = fixture(args.output)
    manifest = {"binary": str(args.binary), "binary_sha256": sha(args.binary),
                "inputs": {name: dict(bytes=path.stat().st_size, sha256=sha(path)) for name, path in datasets.items()},
                "parent_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                "core_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT / "src/database/edadb/core", text=True).strip(),
                "cache": "warm", "cpu": 2, "warmups": 1, "runs": 5, "settle_seconds": 5,
                "core_high_frequency_profiling": "OFF", "sql_trace": "OFF", "leaf_batch_read": "NOT_IMPLEMENTED"}
    manifest["production_source_sha256"] = {name: sha(ROOT / name) for name in (
        "src/database/edadb/idb/edadb_stage_timing.h", "src/database/edadb/idb/edadb_idb_init.cpp",
        "src/database/manager/builder/def_builder/def_write_edadb.cpp",
        "src/database/manager/builder/def_builder/def_read_edadb.cpp")}
    core_root = ROOT / "src/database/edadb/core"
    assert not subprocess.check_output(["git", "diff", "HEAD", "--", "include"], cwd=core_root), "Core headers differ from HEAD"
    assert not any("enableLeafBatchRead" in path.read_text() for path in (core_root / "include").rglob("*.h")), "P2 must be absent"
    assert "enableLeafBatchRead" not in (ROOT / "src/database/manager/builder/def_builder/def_read_edadb.cpp").read_text()
    flags_path = args.build / "src/database/manager/builder/def_builder/CMakeFiles/def_builder.dir/flags.make"
    flags = flags_path.read_text()
    assert "-O3" in flags and "-O0" not in flags
    assert "EDADB_ENABLE_PROFILING=1" not in flags and "_EDADB_DEBUG_TRACE_SQL_STMT_=1" not in flags
    manifest["build_flags"] = flags
    manifest["cmake_cache_sha256"] = sha(args.build / "CMakeCache.txt")
    patch = subprocess.check_output(["git", "diff", "HEAD", "--", *manifest["production_source_sha256"]], cwd=ROOT)
    manifest["production_patch_sha256"] = hashlib.sha256(patch).hexdigest()
    patch_path = args.output / "production.patch"
    if patch_path.exists():
        assert patch_path.read_bytes() == patch
    else:
        patch_path.write_bytes(patch)
    manifest["core_header_sha256"] = {
        str(path.relative_to(core_root)): sha(path)
        for path in sorted((core_root / "include").rglob("*.h"))}
    manifest["benchmark_source_sha256"] = {
        path.name: sha(path) for path in sorted(HERE.iterdir())
        if path.suffix in (".py", ".cpp")}
    original_inputs = json.loads((HERE.parent / "stream_shadow/results/manifest.json").read_text())["inputs"]
    assert manifest["inputs"] == original_inputs, "Fixture differs from the approved streaming comparison"
    manifest_path = args.output / "manifest.json"
    if manifest_path.exists():
        assert json.loads(manifest_path.read_text()) == manifest
    else:
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    if args.phase == "summarize":
        rows = list(csv.DictReader((args.output / "samples.tsv").open(), delimiter="\t"))
        groups = {}
        for row in rows:
            key = tuple(row[name] for name in ("dataset", "observation", "operation", "metric"))
            groups.setdefault(key, []).append(float(row["value"]))
        summary = []
        for key, values in groups.items():
            assert len(values) == 5
            summary.append(dict(zip(("dataset", "observation", "operation", "metric"), key)) |
                           dict(count=5, median=statistics.median(values), mean=statistics.mean(values),
                                minimum=min(values), maximum=max(values)))
        table(args.output / "summary.tsv", summary)
        return
    phase_output = args.output / args.phase
    phase_output.mkdir(exist_ok=False)
    rows, checks = [], []
    for dataset in datasets:
        reference_folder = phase_output / dataset / "reference"
        invoke(args, datasets, dataset, reference_folder, "native", "off")
        reference = (reference_folder / "native.def").read_bytes()
        digest = None
        rounds = range(6) if args.phase == "timing" else range(1 if args.phase != "perf" else 3)
        for round_number in rounds:
            observations = ("off", "on") if round_number % 2 == 0 else ("on", "off")
            if args.phase not in ("smoke", "timing"):
                observations = ("on",)
            for observation in observations:
                print(f"{time.strftime('%H:%M:%S')} {args.phase} {dataset} round={round_number} {observation}", flush=True)
                folder = phase_output / dataset / str(round_number) / observation
                if args.phase == "timing":
                    time.sleep(5)
                for operation in ("write", "read"):
                    instrument = args.phase if args.phase in ("counters", "markers", "perf", "io") else None
                    values = invoke(args, datasets, dataset, folder, operation, observation, instrument)
                    if args.phase == "timing" and round_number:
                        rows.extend(dict(dataset=dataset, round=round_number, observation=observation,
                                         operation=operation, metric=key, value=value) for key, value in values.items())
                        table(args.output / "samples.tsv", rows)
                digest = audit(folder, reference, digest)
                checks.append(dict(dataset=dataset, round=round_number, observation=observation,
                                   dump_sha256=digest, status="PASS"))
    (args.output / f"{args.phase}-audit.json").write_text(json.dumps(checks, indent=2) + "\n")
    print(f"PASS {args.phase}: {len(checks)} strict DEF/database roundtrips", flush=True)


if __name__ == "__main__":
    main()
