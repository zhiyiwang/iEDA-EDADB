"""Audit phase-gated perf logs and summarize CPU samples, not wall-time percentages."""
import csv
import json
from pathlib import Path
import re
import statistics
import sys

root = Path(sys.argv[1])
rows = []
overheads = []
for config in ("A", "B-batch"):
    for route in ("sqlite", "text"):
        for phase in ("write", "read"):
            controls, profiled = [], []
            for sample in (1, 2, 3):
                stem = root / f"{config}-{route}-{phase}-{sample}"
                report = stem.with_suffix(".report.txt").read_text()
                assert "Total Lost Samples: 0" in report, stem
                sample_count = int(re.search(r"# Samples: ([0-9]+)", report)[1])
                assert sample_count > 0, stem
                for suffix, values in ((".control.log", controls), (".log", profiled)):
                    log = stem.with_suffix(suffix).read_text()
                    assert "CHECK\tdigest\tPASS" in log, stem
                    fields = next(line.split("\t") for line in log.splitlines() if line.startswith("TIME\t" + phase + "\t"))
                    values.append(float(fields[5]))
                for line in report.splitlines():
                    match = re.match(r"\s*([\d.]+)%\s+([\d.]+)%\s+\[.\]\s+(.*?)\s+-\s+-\s*$", line)
                    if match:
                        rows.append(dict(config=config, route=route, phase=phase, sample=sample,
                                         samples=sample_count, symbol=match[3],
                                         children_percent=float(match[1]), self_percent=float(match[2])))
            control = statistics.median(controls)
            profiled_time = statistics.median(profiled)
            overheads.append(dict(config=config, route=route, phase=phase, control_ms=control,
                                  perf_ms=profiled_time, overhead_percent=(profiled_time / control - 1) * 100))
with (root / "symbols.tsv").open("w") as stream:
    writer = csv.DictWriter(stream, fieldnames=list(rows[0]), delimiter="\t")
    writer.writeheader()
    writer.writerows(rows)
(root / "overhead.json").write_text(json.dumps(overheads, indent=2) + "\n")
targets = ("sqlite3_step", "bind_record<false>", "sqlite3_reset", "fetch_record", "Consumer::accept",
           "sqlite3VdbeExec", "sqlite3BtreeInsert", "sqlite3VdbeHalt", "pthread_mutex_lock@@GLIBC_2.2.5",
           "pthread_mutex_unlock@@GLIBC_2.2.5")
summary = []
for config in ("A", "B-batch"):
    for phase in ("write", "read"):
        for symbol in targets:
            selected = [row for row in rows if row["config"] == config and row["route"] == "sqlite"
                        and row["phase"] == phase and row["symbol"] == symbol]
            if len(selected) == 3:
                summary.append(dict(config=config, phase=phase, symbol=symbol,
                                    median_children_percent=statistics.median(row["children_percent"] for row in selected),
                                    min_children_percent=min(row["children_percent"] for row in selected),
                                    max_children_percent=max(row["children_percent"] for row in selected)))
(root / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
(root / "audit.json").write_text(json.dumps(dict(status="PASS", captures=24,
    controls=24, lost_samples=0, meaning="user cycles samples, gated data phase; not absolute wall time"), indent=2) + "\n")
print(json.dumps(overheads, indent=2))
print(json.dumps(summary, indent=2))
