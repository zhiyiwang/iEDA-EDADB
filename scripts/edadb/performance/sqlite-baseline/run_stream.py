"""Generate fixtures, check correctness, then sample serially. All artifacts go to --out.
Example: python3 run_stream.py --binary /tmp/iedadb_stream_clean_build/stream_benchmark --out /tmp/iedadb_stream --counts 1000 10000 100000 1000000
Defaults: one warmup, five samples, five-second settling, B-default capped at 1000.
"""
import argparse
import concurrent.futures
import csv
import json
import shutil
import os
from pathlib import Path
import statistics
import sqlite3
import subprocess
import time
from fixtures import prepare, sha

FIELDS = ["init_ms", "create_ms", "begin_ms", "data_ms", "commit_ms", "close_ms", "complete_ms", "rows", "digest"]
GROUPS = [("text", "none", "warm"), ("native", "none", "warm"), ("adapter", "original", "warm")]
GROUPS += [(route, config, cache) for route in ("sqlite", "edadb")
           for config in ("A", "A-no-journal", "B-default", "B-batch")
           for cache in (("first", "repeat") if config.startswith("A") else ("warm",))]

def run(binary, directory, count, group, label, check, settle):
    route, config, cache = group
    stem = directory / (label + "-" + "-".join(group))
    command = [str(binary), route, config, str(count), str(stem)+".data", str(directory),
               "check" if check else "perf", "none" if count == 0 else cache]
    time.sleep(settle)
    started = time.strftime("%Y-%m-%dT%H:%M:%S%z")
    load = os.getloadavg()[0]
    with Path(str(stem)+".log").open("w") as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True,
                       env={**os.environ, "STREAM_SETTLE":str(settle)})
    lines = Path(str(stem)+".log").read_text().splitlines()
    rows = []
    for line in lines:
        if line.startswith("STREAM\t"):
            fields = line.split("\t")
            row = dict(count=count, route=route, config=config, cache=cache, sample=label,
                       operation=fields[1], unit="ms", started=started, load1=load,
                       log=str(stem)+".log")
            row.update({key: float(value) if key not in ("rows", "digest") else int(value)
                        for key, value in zip(FIELDS, fields[2:])})
            rows.append(row)
    assert len(rows) == 2 and rows[1]["rows"] == count, stem
    if route in ("sqlite", "edadb") and config.startswith("B"):
        # Schema inspection is outside the measured process. No production DB changes.
        with sqlite3.connect(str(stem)+".data") as database:
            info = database.execute("PRAGMA table_info(component)").fetchall()
            assert len(info) == 8 and not any(column[5] for column in info), info
            assert not database.execute("PRAGMA index_list(component)").fetchall()
            assert database.execute("select count(*) from component").fetchone()[0] == count
            Path(str(stem)+".schema.sql").write_text(database.execute(
                "select sql from sqlite_master where name='component'").fetchone()[0]+";\n")
    if check and route in ("native", "adapter"):
        # Both outputs pass through the original DEF writer before byte comparison.
        assert sha(Path(str(stem)+".data.restored.def")) == sha(directory/"canonical.def"), stem
    return rows

def summarize(out, samples):
    with (out/"samples.tsv").open("w") as output:
        writer = csv.DictWriter(output, fieldnames=list(samples[0]), delimiter="\t")
        writer.writeheader(); writer.writerows(samples)
    summary = []
    keys = ("count", "route", "config", "cache", "operation")
    for group in sorted(set(tuple(row[key] for key in keys) for row in samples)):
        rows = [row for row in samples if tuple(row[key] for key in keys) == group]
        result = dict(zip(keys, group)); result.update(unit="ms", samples=len(rows))
        for field in FIELDS[:7]:
            values = [row[field] for row in rows]
            for name, function in (("mean", statistics.mean), ("median", statistics.median), ("min", min), ("max", max)):
                result[name+"_"+field] = function(values) if min(values) >= 0 else "NA"
        summary.append(result)
    with (out/"summary.tsv").open("w") as output:
        writer = csv.DictWriter(output, fieldnames=list(summary[0]), delimiter="\t")
        writer.writeheader(); writer.writerows(summary)
    # One human-readable file contains both raw timing and aggregate results.
    with (out/"report.md").open("w") as output:
        output.write("# Stream benchmark results\n\nTime unit: ms. Negative raw values mean N/A.\n"
                     "Sources: [raw samples](samples.tsv), [statistics](summary.tsv), "
                     "[manifest](manifest.json), [correctness](checks.json), [datasets](datasets.json).\n"
                     "B-default includes implicit commits; batch data excludes explicit commit. Compare complete_ms for transaction effects.\n"
                     "Adapter init_ms combines open/schema. Native APIs include internal open/close.\n\n")
        output.write("| N | route | config | cache | operation | data median ms | complete median ms | create median ms | init median ms |\n|---|---|---|---|---|---:|---:|---:|---:|\n")
        for row in summary:
            output.write("| " + " | ".join(str(row[key]) for key in (*keys, "median_data_ms", "median_complete_ms", "median_create_ms", "median_init_ms")) + " |\n")
        output.write("\n## Comparisons\n\nRatios use medians; deltas are net differences, not internal profiling.\n")
        lookup = {tuple(row[key] for key in keys):row for row in summary}
        for row in summary:
            if row["route"] == "edadb":
                baseline = lookup.get((row["count"],"sqlite",row["config"],row["cache"],row["operation"]))
                if baseline:
                    field = "median_complete_ms" if row["operation"] == "write" else "median_data_ms"
                    value, reference = row[field], baseline[field]
                    output.write(f"- N={row['count']} {row['config']}/{row['cache']} {row['operation']}: EDADB/SQLite={value/reference:.3f}x; delta={value-reference:.6f} ms.\n")
            if row["config"] == "B-default" and row["operation"] == "write":
                baseline = lookup.get((row["count"],row["route"],"B-batch",row["cache"],"write"))
                if baseline:
                    output.write(f"- N={row['count']} {row['route']} B-default/B-batch complete ratio={row['median_complete_ms']/baseline['median_complete_ms']:.3f}x.\n")
        output.write("\n## Raw timing source\n\n```tsv\n" + (out/"samples.tsv").read_text() + "```\n")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--counts", type=int, nargs="+", default=[1000,10000,100000,1000000])
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--settle", type=float, default=5)
    parser.add_argument("--default-max", type=int, default=1000)
    parser.add_argument("--routes", nargs="+", choices=("text", "native", "sqlite", "edadb", "adapter"),
                        default=["text", "native", "sqlite", "edadb", "adapter"], help="Only check and sample these routes; native fixture preparation remains untimed")
    args = parser.parse_args()
    selected_groups = [group for group in GROUPS if group[0] in args.routes]
    assert args.runs > 0 and args.settle >= 0 and all(0 < count <= 1000000 for count in args.counts)
    args.binary = args.binary.resolve(); args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=False)
    source_copy = args.out/"source"
    source_copy.mkdir()
    for path in Path(__file__).parent.iterdir():
        if path.is_file(): shutil.copy2(path,source_copy/path.name)
    root = Path(__file__).resolve().parents[4]
    manifest = dict(arguments={key:str(value) if isinstance(value, Path) else value for key,value in vars(args).items()},
                    binary_sha256=sha(args.binary),
                    source_sha256={path.name:sha(path) for path in Path(__file__).parent.iterdir() if path.is_file()},
                    git=subprocess.check_output(["git","rev-parse","HEAD"],cwd=root,text=True).strip(),
                    core=subprocess.check_output(["git","-C","src/database/edadb/core","rev-parse","HEAD"],cwd=root,text=True).strip(),
                    hardware=subprocess.check_output(["sh","-c","lscpu; free -h; df -h /tmp; ldd " + str(args.binary)],text=True))
    (args.out/"manifest.json").write_text(json.dumps(manifest, indent=2))
    flags = args.binary.parent/"CMakeFiles/stream_benchmark.dir/flags.make"
    link = args.binary.parent/"CMakeFiles/stream_benchmark.dir/link.txt"
    assert "-O3" in flags.read_text() and "-DNDEBUG" in flags.read_text(), "Release flags required"
    shutil.copy2(flags,args.out/"compile_flags.txt")
    shutil.copy2(link,args.out/"link_command.txt")
    checks = []
    for count in sorted(set([0,1,8]+args.counts)):
        fixture = args.out/f"n{count}"
        prepare(fixture, count)
        # Normalize fixture using native DEF writer, without any measured sample.
        run(args.binary, fixture, count, ("native","none","warm"), "canonical", False, 0)
        (fixture/"canonical-native-none-warm.data").replace(fixture/"canonical.def")
        check_groups = [group for group in selected_groups if group[1] != "B-default" or count <= args.default_max]
        with concurrent.futures.ThreadPoolExecutor(max_workers=5) as pool:
            futures = [pool.submit(run,args.binary,fixture,count,group,"check",True,0) for group in check_groups]
            for group,future in zip(check_groups,futures):
                future.result(); checks.append(dict(count=count,group=group,status="PASS"))
        print(f"{time.strftime('%H:%M:%S')} fixture {count} ready",flush=True)
    (args.out/"checks.json").write_text(json.dumps(checks,indent=2))
    (args.out/"datasets.json").write_text(json.dumps([
        dict(path=str(path),bytes=path.stat().st_size,sha256=sha(path))
        for path in args.out.glob("n*/*") if path.name in ("bench.lef","canonical.def")],indent=2))
    samples = []
    for count in args.counts:
        groups = [group for group in selected_groups if group[1] != "B-default" or count <= args.default_max]
        for sample in range(args.runs+1):
            for group in (groups if sample % 2 == 0 else list(reversed(groups))):
                rows = run(args.binary,args.out/f"n{count}",count,group,f"run-{sample}",False,args.settle)
                if sample:
                    samples.extend(rows); summarize(args.out,samples)
                print(f"{time.strftime('%H:%M:%S')} N={count} sample={sample} {group}: PASS",flush=True)
    print(f"COMPLETE {args.out}/report.md",flush=True)

if __name__ == "__main__":
    main()
