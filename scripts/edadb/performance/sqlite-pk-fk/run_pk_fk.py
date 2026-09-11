"""Run independent checks, then SERIAL Release performance samples.

Example: python3 run_pk_fk.py --binary /tmp/iedadb_pk_fk_build/pk_fk_benchmark \
    --out /tmp/iedadb_pk_fk_main --stage main
main: 10000x10, three schemas, two APIs/storages, FK-OFF and SQLite flat controls.
ratios: 1000x100 and 100000x1, graph reads/FK-ON only. Never silently shorten none scans.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import time

TIME_FIELDS = ["init_ms", "create_ms", "begin_ms", "data_ms", "commit_ms", "close_ms", "complete_ms"]
FIELDS = TIME_FIELDS + ["parents", "children", "digest"]
KEYS = ["parent_count", "children_per_parent", "route", "schema", "storage", "foreign_keys", "read_mode", "operation"]


def sha(path):
    with Path(path).open("rb") as source:
        result = hashlib.sha256()
        for block in iter(lambda: source.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def configurations():
    groups = [dict(route=route, schema=schema, storage=storage, foreign_keys="on", read_mode="graph")
              for route in ("sqlite", "edadb") for schema in ("pk", "index", "none")
              for storage in ("memory", "disk")]
    groups += [dict(route=route, schema="pk", storage=storage, foreign_keys="off", read_mode="graph")
               for route in ("sqlite", "edadb") for storage in ("memory", "disk")]
    groups += [dict(route="sqlite", schema=schema, storage=storage, foreign_keys="on", read_mode="flat")
               for schema in ("pk", "index", "none") for storage in ("memory", "disk")]
    return groups


def parse_log(path):
    lines = path.read_text().splitlines()
    assert lines[-1] == "PASS", path
    rows, evidence, sql, checks = [], {}, {}, []
    expected = None
    for line in lines:
        parts = line.split("\t")
        if parts[0] == "INPUT":
            expected = list(map(int, parts[1:]))
        elif parts[0] == "RESULT":
            assert len(parts) == 12, parts
            row = dict(operation=parts[1])
            row.update({key: float(value) if key in TIME_FIELDS else int(value)
                        for key, value in zip(FIELDS, parts[2:])})
            assert [row[key] for key in ("parents", "children", "digest")] == expected, path
            assert abs(row["complete_ms"] - (row["data_ms"] + max(0, row["begin_ms"]) + max(0, row["commit_ms"]))) < 0.000004
            rows.append(row)
        elif parts[0] == "EVIDENCE":
            evidence.setdefault(parts[1], []).append(parts[2:])
        elif parts[0] == "SQL":
            sql[parts[1]] = parts[2]
        elif parts[0] == "CHECK":
            checks.append(parts[1:])
    return rows, evidence, sql, checks


def run(binary, out, group, parents, children, label, check=False, fixture="regular", settle=0):
    stem = out / (label + "-" + "-".join(str(value) for value in (parents, children, *group.values(), fixture)))
    command = [str(binary), "--route", group["route"], "--schema", group["schema"],
               "--storage", group["storage"], "--foreign-keys", group["foreign_keys"],
               "--read", group["read_mode"], "--parents", str(parents), "--children-per-parent", str(children),
               "--mode", "check" if check else "perf", "--fixture", fixture, "--db", str(stem) + ".db"]
    time.sleep(settle)
    started = time.strftime("%Y-%m-%dT%H:%M:%S%z")
    load = os.getloadavg()[0]
    before = time.monotonic()
    log_path = Path(str(stem) + ".log")
    with log_path.open("w") as output:
        subprocess.run(command, stdout=output, stderr=subprocess.STDOUT, check=True,
                       env={**os.environ, "PK_FK_SETTLE": str(settle)})
    rows, evidence, sql, checks = parse_log(log_path)
    assert len(rows) == (3 if group["storage"] == "memory" else 2)
    for row in rows:
        row.update(parent_count=parents, children_per_parent=children, **group,
                   sample=label, unit="ms", started=started, load1=load, log=str(log_path))
    assert evidence["config_foreign_keys"] == [["1" if group["foreign_keys"] == "on" else "0"]]
    if check:
        assert ["constraints_and_rollback", "PASS"] in checks
        child_columns = evidence["child_columns"]
        assert len(child_columns) == 5 and child_columns[0][1:4] == ["pin_name", "TEXT", "1"]
        assert len(evidence["foreign_key"]) == 1
        indexes = evidence.get("child_indexes", [])
        if group["schema"] == "none":
            assert not indexes and not any(int(column[5]) for column in child_columns)
        else:
            assert len(indexes) == 1 and indexes[0][2] == ("1" if group["schema"] == "pk" else "0")
            assert [entry[2] for entry in evidence["index_columns"]] == [child_columns[-1][1], "pin_name"]
        assert len([entry for entry in evidence["schema"] if entry[0] == "table"]) == 2
    return rows, dict(group=group, parents=parents, children=children, fixture=fixture, log=str(log_path),
                      seconds=time.monotonic() - before, evidence=evidence, sql=sql, checks=checks, status="PASS")


def summarize(out, samples):
    with (out / "samples.tsv").open("w") as output:
        writer = csv.DictWriter(output, fieldnames=list(samples[0]), delimiter="\t")
        writer.writeheader(); writer.writerows(samples)
    summary = []
    for key in sorted({tuple(row[field] for field in KEYS) for row in samples}):
        rows = [row for row in samples if tuple(row[field] for field in KEYS) == key]
        result = dict(zip(KEYS, key), samples=len(rows), unit="ms")
        for field in TIME_FIELDS:
            values = [row[field] for row in rows]
            for name, function in (("mean", statistics.mean), ("median", statistics.median), ("min", min), ("max", max)):
                result[name + "_" + field] = function(values) if min(values) >= 0 else "NA"
        summary.append(result)
    with (out / "summary.tsv").open("w") as output:
        writer = csv.DictWriter(output, fieldnames=list(summary[0]), delimiter="\t")
        writer.writeheader(); writer.writerows(summary)
    with (out / "report.md").open("w") as output:
        output.write("# PK/FK measured results\n\nUnit: ms; -1/NA means not applicable. Median phases must not be added as if from one sample.\n"
                     "Sources: [samples](samples.tsv), [summary](summary.tsv), [checks](checks.json), [manifest](manifest.json).\n"
                     "Graph routes reconstruct one parent and children. flat only consumes fields. Create and COMMIT are separate.\n\n")
        columns = KEYS + ["samples"] + ["median_" + field for field in TIME_FIELDS]
        output.write("| " + " | ".join(columns) + " |\n|" + "---|" * len(columns) + "\n")
        for row in summary:
            output.write("| " + " | ".join(str(row[column]) for column in columns) + " |\n")
        output.write("\n## Raw timing source\n\n```tsv\n" + (out / "samples.tsv").read_text() + "```\n")
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--stage", choices=("main", "ratios", "all"), default="main")
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--settle", type=float, default=5)
    parser.add_argument("--check-jobs", type=int, default=4)
    args = parser.parse_args()
    assert args.runs > 0 and args.settle >= 0 and args.check_jobs > 0
    binary, out = args.binary.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    root = Path(__file__).resolve().parents[4]
    source = Path(__file__).resolve().parent
    shutil.copytree(source, out / "source", ignore=shutil.ignore_patterns("__pycache__"))
    flags = binary.parent / "CMakeFiles/pk_fk_benchmark.dir/flags.make"
    assert "-O3" in flags.read_text() and "-DNDEBUG" in flags.read_text()
    shutil.copy2(flags, out / "compile_flags.txt")
    shutil.copy2(binary.parent / "CMakeFiles/pk_fk_benchmark.dir/link.txt", out / "link_command.txt")
    manifest = dict(arguments={key: str(value) if isinstance(value, Path) else value for key, value in vars(args).items()},
                    started=time.strftime("%Y-%m-%dT%H:%M:%S%z"), binary_sha256=sha(binary),
                    source_sha256={path.name: sha(path) for path in source.iterdir() if path.is_file()},
                    git=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
                    core=subprocess.check_output(["git", "-C", "src/database/edadb/core", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
                    hardware=subprocess.check_output(["sh", "-c", "lscpu; free -h; df -h /tmp"], text=True))
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2))
    groups = configurations()
    checks = []
    jobs = [(group, parents, children, fixture) for group in groups
            for parents, children, fixture in ((0, 0, "regular"), (1, 1, "regular"), (8, 5, "mixed"), (100, 10, "regular"))]
    with ThreadPoolExecutor(max_workers=args.check_jobs) as pool:
        futures = [pool.submit(run, binary, out, group, parents, children, "check", True, fixture)
                   for group, parents, children, fixture in jobs]
        for future in futures:
            _, check = future.result(); checks.append(check)
    # Same schema and PRAGMA values across routes; no near-equivalent hand-written DDL.
    lookup = {(item["parents"], item["children"], item["fixture"], *item["group"].values()): item for item in checks}
    for item in checks:
        group = item["group"]
        if group["route"] != "edadb": continue
        direct = lookup[(item["parents"], item["children"], item["fixture"], "sqlite", group["schema"], group["storage"], group["foreign_keys"], group["read_mode"])]
        assert item["evidence"] == direct["evidence"], (item["log"], direct["log"])
        assert item["sql"] == direct["sql"]
    (out / "checks.json").write_text(json.dumps(checks, indent=2))
    print(f"{time.strftime('%H:%M:%S')} CHECK PASS: {len(checks)} cases; cross-route DDL/config identical", flush=True)
    sizes = ([(10000, 10)] if args.stage in ("main", "all") else []) + ([(1000, 100), (100000, 1)] if args.stage in ("ratios", "all") else [])
    samples = []
    for parents, children in sizes:
        selected = groups if (parents, children) == (10000, 10) else [group for group in groups if group["foreign_keys"] == "on" and group["read_mode"] == "graph"]
        for sample in range(args.runs + 1):
            for group in (selected if sample % 2 == 0 else list(reversed(selected))):
                print(f"{time.strftime('%H:%M:%S')} START {parents}x{children} run-{sample} {group}", flush=True)
                rows, result = run(binary, out, group, parents, children, f"run-{sample}", settle=args.settle)
                if sample:
                    samples.extend(rows); summarize(out, samples)
                print(f"{time.strftime('%H:%M:%S')} PASS {result['seconds']:.2f}s; " + ", ".join(f"{row['operation']}={row['data_ms']:.3f}ms" for row in rows), flush=True)
    summary = summarize(out, samples)
    # Reparse every raw log and independently validate aggregate statistics before declaring completion.
    for row in samples:
        original = next(item for item in parse_log(Path(row["log"]))[0] if item["operation"] == row["operation"])
        assert all(row[field] == original[field] for field in FIELDS)
    assert all(row["samples"] == args.runs for row in summary)
    assert sha(binary) == manifest["binary_sha256"]
    (out / "audit.json").write_text(json.dumps(dict(status="PASS", samples=len(samples), groups=len(summary),
        correctness_cases=len(checks), samples_per_group=args.runs, finished=time.strftime("%Y-%m-%dT%H:%M:%S%z")), indent=2))
    print(f"COMPLETE {out / 'report.md'}", flush=True)


if __name__ == "__main__":
    main()
