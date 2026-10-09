"""Reduce independent diagnostics. CPU samples and syscall timings are not wall-time slices."""
import argparse
from collections import Counter
import csv
import json
from pathlib import Path
import re
import sqlite3
import subprocess


def save(path, rows):
    if not rows:
        return
    with path.open("w") as stream:
        writer = csv.DictWriter(stream, fieldnames=rows[0].keys(), delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def boundaries(log, clock=4):
    pending, spans = {}, []
    for line in log.read_text().splitlines():
        parts = line.split("\t")
        if parts[0] != "BOUNDARY":
            continue
        key = (parts[1], parts[2])
        stamp = int(parts[clock]) / 1e9
        if parts[3] == "1":
            assert key not in pending
            pending[key] = stamp
        else:
            spans.append((parts[2], pending.pop(key), stamp))
    assert not pending
    assert any(name == "command" for name, *_ in spans)
    return spans


def counters(output):
    sql_rows, api_rows, cache_rows, configurations, plans, executions, layouts = [], [], [], [], [], [], []
    for dataset in ("filler", "routed-stress"):
        folder = output / "counters" / dataset / "0/on"
        connection = sqlite3.connect(f"file:{folder / 'edadb.db'}?mode=ro", uri=True)
        connection.execute("PRAGMA foreign_keys=ON")
        for operation in ("read", "write"):
            pending = {}
            for line in (folder / f"{operation}.log").read_text().splitlines():
                parts = line.split("\t")
                base = dict(dataset=dataset, operation=operation)
                if parts[0] == "STMT":
                    row = base | dict(phase=parts[1], runs=int(parts[2]), vm_steps=int(parts[3]),
                                      scan_steps=int(parts[4]), sorts=int(parts[5]), reprepares=int(parts[6]), sql=parts[7])
                    sql_rows.append(row)
                    sql = row["sql"]
                    if sql.startswith(("SELECT", "INSERT")):
                        parameters = (None,) * sql.count("?")
                        plan = connection.execute("EXPLAIN QUERY PLAN " + sql, parameters).fetchall()
                        bytecode = connection.execute("EXPLAIN " + sql, parameters).fetchall()
                        plans.append(base | dict(phase=parts[1], sql=sql, query_plan=plan,
                                                 opcode_counts=dict(Counter(code[1] for code in bytecode)), bytecode=bytecode))
                elif parts[0] == "API":
                    api_rows.append(base | dict(phase=parts[1], api=parts[2], calls=int(parts[3])))
                elif parts[0] == "CONFIG":
                    configurations.append(base | dict(parameter=parts[1], value=parts[2]))
                elif parts[0] == "EXEC":
                    executions.append(base | dict(phase=parts[1], sql="\t".join(parts[2:])))
                elif parts[0] == "DBSTATUS":
                    values = list(map(int, parts[4:]))
                    if parts[3] == "1":
                        pending[parts[2]] = values
                    else:
                        # Command/init begin occurs before sqlite3_open; initial counters are zero.
                        initial = pending.pop(parts[2], [0] * len(values))
                        delta = [last - first for first, last in zip(initial, values)]
                        cache_rows.append(base | dict(phase=parts[2], cache_hits=delta[0], cache_misses=delta[1],
                            page_writes=delta[2], page_spills=delta[3], cache_bytes_at_end=values[4]))
            assert not pending
        definitions = connection.execute("SELECT type, name, tbl_name, sql FROM sqlite_master ORDER BY type, name").fetchall()
        tables = [row[1] for row in definitions if row[0] == "table" and not row[1].startswith("sqlite_")]
        layouts.append(dict(dataset=dataset, database_bytes=(folder / "edadb.db").stat().st_size,
            schema=definitions, table_rows={name: connection.execute(
                'SELECT count(*) FROM "' + name.replace('"', '""') + '"').fetchone()[0] for name in tables}))
        connection.close()
    save(output / "sql-statements.tsv", sql_rows)
    save(output / "api-counts.tsv", api_rows)
    save(output / "cache-stages.tsv", cache_rows)
    save(output / "sqlite-config.tsv", configurations)
    save(output / "exec-sql.tsv", executions)
    (output / "query-plans.json").write_text(json.dumps(plans, indent=2) + "\n")
    (output / "database-layout.json").write_text(json.dumps(layouts, indent=2) + "\n")


def cpu_bucket(frames):
    symbols = "\n".join(frames)
    for bucket, expression in (
        ("sqlite.step", r"\bsqlite3(?:Step|_step|VdbeExec)\b"),
        ("sqlite.bind", r"\bsqlite3_bind_"),
        ("sqlite.column", r"\bsqlite3_column_"),
        ("sqlite.reset_clear", r"\bsqlite3_(?:reset|clear_bindings)\b"),
        ("sqlite.prepare_finalize", r"\bsqlite3_(?:prepare|finalize)"),
        ("adapter.toShadow", r"\btoShadow(?:\(|\s|\.|$)"),
        ("adapter.fromShadow", r"\bfromShadow(?:\(|\s|\.|$)"),
    ):
        if re.search(expression, symbols):
            return bucket
    if "libsqlite3" in frames[0] or "sqlite.debug" in frames[0]:
        return "sqlite.unattributed"
    return "other_framework_adapter_runtime"


def perf(output):
    counts, symbols, audits = Counter(), Counter(), []
    for dataset in ("filler", "routed-stress"):
        for round_number in range(3):
            folder = output / "perf" / dataset / str(round_number) / "on"
            for operation in ("read", "write"):
                spans = boundaries(folder / f"{operation}.log")
                data = folder / f"{operation}.perf.data"
                report = folder / f"{operation}.perf.txt"
                errors = folder / f"{operation}.perf-script.stderr"
                with report.open("w") as stream, errors.open("w") as error_stream:
                    subprocess.run(["perf", "--buildid-dir", str(output / "symbols"), "script", "--ns",
                                    "--no-inline", "--show-lost-events", "-i", str(data), "-F", "comm,pid,tid,time,event,ip,sym,dso"],
                                   stdout=stream, stderr=error_stream, check=True)
                raw = report.read_text()
                lost = [line for line in (raw + errors.read_text() + (folder / f"{operation}.log").read_text()).splitlines()
                        if re.search(r"PERF_RECORD_LOST|LOST \d+ events|lost \d+", line)]
                samples, unknown = 0, 0
                depths = []
                for block in re.split(r"\n\s*\n", raw):
                    lines = block.strip().splitlines()
                    if not lines:
                        continue
                    match = re.search(r"\s(\d+\.\d+):\s+cycles:u:", lines[0])
                    if not match:
                        continue
                    stamp = float(match[1])
                    phases = [name for name, begin, end in spans if begin <= stamp <= end]
                    if "command" not in phases:
                        continue
                    frames = lines[1:] if len(lines) > 1 else [lines[0]]
                    category = cpu_bucket(frames)
                    samples += 1
                    depths.append(len(frames))
                    unknown += int("[unknown]" in frames[0])
                    for phase in phases:
                        counts[(dataset, operation, phase, category)] += 1
                        symbol = re.sub(r"^\s*[0-9a-f]+\s+", "", frames[0]).strip()
                        symbols[(dataset, operation, phase, symbol)] += 1
                audits.append(dict(dataset=dataset, operation=operation, round=round_number,
                                   command_samples=samples, unknown_leaf_samples=unknown,
                                   mean_stack_depth=sum(depths) / max(1, len(depths)), lost_event_messages=lost))
                assert samples > 0, report
                assert not lost, lost
    rows = []
    for (dataset, operation, phase, category), count in counts.items():
        total = sum(value for key, value in counts.items() if key[:3] == (dataset, operation, phase))
        rows.append(dict(dataset=dataset, operation=operation, phase=phase, category=category,
                         samples=count, total_samples=total, percent=100 * count / total))
    save(output / "cpu-categories.tsv", rows)
    save(output / "cpu-symbols.tsv", [dict(dataset=key[0], operation=key[1], phase=key[2], symbol=key[3], samples=value)
                                      for key, value in symbols.most_common()])
    (output / "perf-sample-audit.json").write_text(json.dumps(audits, indent=2) + "\n")


def io(output):
    counts, durations = Counter(), Counter()
    for dataset in ("filler", "routed-stress"):
        folder = output / "io" / dataset / "0/on"
        for operation in ("read", "write"):
            spans = boundaries(folder / f"{operation}.log", clock=5)
            for line in (folder / f"{operation}.strace").read_text().splitlines():
                match = re.match(r"(\d+\.\d+)\s+(\w+)\(.*<([0-9.]+)>$", line)
                if not match:
                    continue
                stamp, syscall, elapsed = float(match[1]), match[2], float(match[3])
                if "edadb.db" in line:
                    kind = "journal" if "edadb.db-journal" in line else "database"
                elif syscall in ("fsync", "fdatasync") and f"<{folder}>" in line:
                    kind = "directory"
                else:
                    continue
                phases = [name for name, begin, end in spans if begin <= stamp <= end]
                for phase in phases:
                    key = (dataset, operation, phase, kind, syscall)
                    counts[key] += 1
                    durations[key] += elapsed * 1000
    save(output / "io-stages.tsv", [dict(zip(("dataset", "operation", "phase", "file", "syscall"), key)) |
                                    dict(calls=value, traced_syscall_ms=durations[key]) for key, value in counts.items()])


def markers(output):
    rows = []
    for dataset in ("filler", "routed-stress"):
        folder = output / "markers" / dataset / "0/on"
        for operation in ("read", "write"):
            pending = {}
            for line in (folder / f"{operation}.log").read_text().splitlines():
                parts = line.split("\t")
                if parts[0] != "BOUNDARY":
                    continue
                values = [int(parts[index]) for index in (4, 6, 7)]
                if parts[3] == "1":
                    pending[parts[2]] = values
                else:
                    delta = [(last - first) / 1e6 for first, last in zip(pending.pop(parts[2]), values)]
                    rows.append(dict(dataset=dataset, operation=operation, phase=parts[2],
                                     diagnostic_wall_ms=delta[0], user_cpu_ms=delta[1], system_cpu_ms=delta[2]))
            assert not pending
    save(output / "diagnostic-cpu-time.tsv", rows)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kind", choices=["counters", "perf", "io", "markers"])
    parser.add_argument("output", type=Path)
    arguments = parser.parse_args()
    globals()[arguments.kind](arguments.output.resolve())
