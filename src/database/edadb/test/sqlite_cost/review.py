"""Recompute archived statistics without executing or overwriting benchmarks."""
import csv
import argparse
import hashlib
import json
import math
import statistics
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[4]
RESULTS = HERE / "results"


def rows(name):
    with (RESULTS / name).open() as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


def review(results):
    global RESULTS
    RESULTS = Path(results).resolve()
    groups = defaultdict(list)
    samples = defaultdict(dict)
    for row in rows("samples.tsv"):
        key = tuple(row[name] for name in ("dataset", "observation", "operation", "metric"))
        groups[key].append(float(row["value"]))
        sample = tuple(row[name] for name in ("dataset", "observation", "operation", "round"))
        assert row["metric"] not in samples[sample]
        samples[sample][row["metric"]] = float(row["value"])
    summaries = rows("summary.tsv")
    assert len(summaries) == len(groups) == 114
    for row in summaries:
        key = tuple(row[name] for name in ("dataset", "observation", "operation", "metric"))
        values = groups[key]
        assert len(values) == int(row["count"]) == 5
        for field, compute in (("median", statistics.median), ("mean", statistics.mean),
                               ("minimum", min), ("maximum", max)):
            assert math.isclose(float(row[field]), compute(values), abs_tol=1e-8), (key, field)
    for sample, metrics in samples.items():
        if sample[1] != "on":
            continue
        operation = sample[2]
        total = sum(metrics[f"stage.{phase}_ms"] for phase in ("init", "create", operation, "other"))
        assert math.isclose(total, metrics["stage.command_ms"], abs_tol=1e-6)
        assert metrics["adapter.root.Net_ms"] <= metrics[f"stage.{operation}_ms"]
        if operation == "write":
            data = metrics["stage.write_ms"]
            transaction = sum(metrics[f"adapter.transaction.{phase}_ms"] for phase in ("begin", "commit"))
            assert math.isclose(data - transaction, metrics["write_without_transaction_ms"], abs_tol=1e-6)
    for phase, count in (("smoke", 4), ("timing", 24), ("counters", 2), ("markers", 2), ("perf", 6), ("io", 2)):
        checks = json.loads((RESULTS / f"{phase}-audit.json").read_text())
        assert len(checks) == count and all(check["status"] == "PASS" for check in checks)
    perf = json.loads((RESULTS / "perf-sample-audit.json").read_text())
    assert len(perf) == 12 and all(not sample["lost_event_messages"] for sample in perf)
    sql = rows("sql-statements.tsv")
    selects = {}
    for dataset in ("filler", "routed-stress"):
        selects[dataset] = sum(int(row["runs"]) for row in sql
                              if row["dataset"] == dataset and row["operation"] == "read"
                              and row["sql"].lstrip().upper().startswith("SELECT"))
    assert selects == {"filler": 30548, "routed-stress": 448548}, selects
    manifest = json.loads((RESULTS / "manifest.json").read_text())
    comparisons = {}
    for name, expected in manifest["production_source_sha256"].items():
        comparisons[name] = hashlib.sha256((ROOT / name).read_bytes()).hexdigest() == expected
    binary = Path(manifest["binary"])
    binary_matches = binary.exists() and hashlib.sha256(binary.read_bytes()).hexdigest() == manifest["binary_sha256"]
    return dict(archived_statistics="PASS", groups=len(groups),
                         command_samples=len(samples), stage_checks="PASS", select_runs=selects,
                         current_sources_match_archive=comparisons,
                         current_binary_matches_archive=binary_matches)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path)
    arguments = parser.parse_args()
    print(json.dumps(review(arguments.results), indent=2))


if __name__ == "__main__":
    main()
