"""Run the isolated 2x2 SQLite alignment control; no production code changes.

python3 run_alignment.py /tmp/iedadb_alignment_build/alignment /tmp/iedadb_alignment_full
Use --count 1000 --runs 1 --settle 0 for a smoke test. Output must not exist.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import sqlite3
import statistics
import subprocess
import time

VARIANTS = ("raw", "checks", "other", "aligned", "edadb")
FIELDS = ("init_ms", "create_ms", "begin_ms", "data_ms", "commit_ms", "close_ms", "complete_ms", "rows", "digest")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def save_json(path, data):
    path.write_text(json.dumps(data, indent=2, default=str) + "\n")


def run_sample(args, config, variant, sample, count, check):
    stem = args.output / f"{config}-{variant}-{sample}-n{count}"
    database = stem.with_suffix(".db")
    command = [str(args.binary), "edadb" if variant == "edadb" else "sqlite",
               config, str(count), str(database), str(args.output),
               "check" if check else "perf", "first" if config == "A" else "warm", variant]
    env = {key: value for key, value in os.environ.items() if not key.startswith("STREAM_")}
    env["STREAM_SETTLE"] = str(args.settle)
    time.sleep(args.settle if not check else 0)
    with stem.with_suffix(".log").open("w") as log:
        subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
    lines = stem.with_suffix(".log").read_text().splitlines()
    records = []
    for line in lines:
        if line.startswith("STREAM\t"):
            fields = line.split("\t")
            assert len(fields) == 11
            row = dict(config=config, variant=variant, sample=sample, count=count,
                       operation=fields[1], unit="ms", log=str(stem.with_suffix(".log")))
            row.update({key: int(value) if key in ("rows", "digest") else float(value)
                        for key, value in zip(FIELDS, fields[2:])})
            records.append(row)
    assert len(records) == 2 and records[1]["rows"] == count
    if check and count:
        assert "CHECK\tsame-length corruption rejected" in lines
    # Outside all timers: SQL/schema/config evidence and DB integrity.
    config_rows = [line for line in lines if line.startswith("CONFIG\t")]
    sql_rows = [line for line in lines if line.startswith("SQL\t")]
    assert len(sql_rows) == 2
    if config == "B-batch":
        with sqlite3.connect(database) as connection:
            assert connection.execute("PRAGMA integrity_check").fetchone()[0] == "ok"
            assert connection.execute("SELECT count(*) FROM component").fetchone()[0] == count
            assert not connection.execute("PRAGMA index_list(component)").fetchall()
            assert len(connection.execute("PRAGMA table_info(component)").fetchall()) == 8
            stem.with_suffix(".schema.sql").write_text(connection.execute(
                "SELECT sql FROM sqlite_master WHERE name='component'").fetchone()[0] + ";\n")
    database.unlink(missing_ok=True)
    return records, config_rows, sql_rows


def summarize(output, rows):
    with (output / "samples.tsv").open("w") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]), delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)
    groups = []
    for config in ("A", "B-batch"):
        for operation in ("write", "read"):
            for variant in VARIANTS:
                selected = [row for row in rows if (row["config"], row["operation"], row["variant"]) ==
                            (config, operation, variant)]
                if not selected:
                    continue
                group = dict(config=config, operation=operation, variant=variant, samples=len(selected), unit="ms")
                for field in FIELDS[:7]:
                    values = [row[field] for row in selected]
                    for label, function in (("median", statistics.median), ("mean", statistics.mean), ("min", min), ("max", max)):
                        group[label + "_" + field] = function(values) if min(values) >= 0 else None
                groups.append(group)
    save_json(output / "summary.json", groups)
    return groups


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--count", type=int, default=1000000)
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--settle", type=float, default=1)
    args = parser.parse_args()
    assert 0 < args.count <= 1000000 and args.runs > 0 and args.settle >= 0
    args.binary = args.binary.resolve()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    source = Path(__file__).resolve().parent
    flags = args.binary.parent / "CMakeFiles/alignment.dir/flags.make"
    assert "-O3" in flags.read_text() and "-DNDEBUG" in flags.read_text()
    for name in ("flags.make", "link.txt"):
        shutil.copyfile(flags.parent / name, args.output / name)
    files = [source / name for name in ("alignment.cpp", "run_alignment.py", "CMakeLists.txt")]
    files.append(source.parent / "benchmark/benchmark_support.h")
    for path in files:
        shutil.copyfile(path, args.output / path.name)
    root = source.parents[4]
    save_json(args.output / "manifest.json", dict(arguments=vars(args), binary_sha256=sha(args.binary),
        source_sha256={str(path): sha(path) for path in files},
        git=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
        core=subprocess.check_output(["git", "-C", "src/database/edadb/core", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
        hardware=subprocess.check_output(["sh", "-c", "lscpu; free -h"], text=True)))
    rows, checks, configs, sql = [], [], {}, None
    # All variants validate boundary fixtures before the full-size warmup.
    for config in ("A", "B-batch"):
        for count in (0, 1, 8):
            for variant in VARIANTS:
                run_sample(args, config, variant, "check", count, True)
                checks.append(dict(config=config, variant=variant, count=count, status="PASS"))
        for sample in range(args.runs + 1):
            order = VARIANTS[sample % len(VARIANTS):] + VARIANTS[:sample % len(VARIANTS)]
            for variant in order:
                current, actual, current_sql = run_sample(args, config, variant, sample, args.count, sample == 0)
                assert config not in configs or configs[config] == actual
                configs[config] = actual
                assert sql is None or sql == current_sql
                sql = current_sql
                if sample:
                    rows.extend(current)
                    summarize(args.output, rows)
                else:
                    checks.append(dict(config=config, variant=variant, count=args.count, status="PASS"))
                save_json(args.output / "checks.json", checks)
                print(time.strftime("%H:%M:%S"), config, variant, sample, "PASS", flush=True)
    assert len(rows) == 20 * args.runs and len(checks) == 40
    assert len({row["digest"] for row in rows if row["operation"] == "read"}) == 1
    groups = summarize(args.output, rows)
    assert all(group["samples"] == args.runs for group in groups)
    save_json(args.output / "configs.json", configs)
    (args.output / "aligned.sql.txt").write_text("\n".join(sql) + "\n")
    # Differences of same-batch medians, not exclusive function timings.
    deltas = []
    for config in ("A", "B-batch"):
        for operation in ("write", "read"):
            times = {group["variant"]: group["median_data_ms"] for group in groups
                     if (group["config"], group["operation"]) == (config, operation)}
            deltas.append(dict(config=config, operation=operation, unit="ms",
                checks_on_raw=times["checks"]-times["raw"],
                checks_on_other=times["aligned"]-times["other"],
                other_without_checks=times["other"]-times["raw"],
                other_with_checks=times["aligned"]-times["checks"],
                interaction=(times["aligned"]-times["other"])-(times["checks"]-times["raw"]),
                residual=times["edadb"]-times["aligned"],
                total_gap=times["edadb"]-times["raw"]))
    save_json(args.output / "deltas.json", deltas)
    save_json(args.output / "audit.json", dict(status="PASS", checks=len(checks), timing_rows=len(rows),
        groups=len(groups), samples_per_group=args.runs, binary_sha256=sha(args.binary)))
    print("COMPLETE", args.output, flush=True)


if __name__ == "__main__":
    main()
