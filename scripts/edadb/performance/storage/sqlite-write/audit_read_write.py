import argparse
import csv
import hashlib
import json
from pathlib import Path
import statistics


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    root = args.directory
    rows = list(csv.DictReader((root / "samples.tsv").open(), delimiter="\t"))
    assert len(rows) == 80
    summaries = json.loads((root / "summary.json").read_text())
    comparisons = json.loads((root / "comparisons.json").read_text())
    manifest = json.loads((root / "manifest.json").read_text())
    for path, value in manifest["hashes"].items():
        assert hashlib.sha256(Path(path).read_bytes()).hexdigest() == value
    assert len(summaries) == 16 and len(comparisons) == 12
    for summary in summaries:
        group = [row for row in rows if all(row[key] == summary[key] for key in ("config", "route", "operation"))]
        assert len(group) == 5
        assert {row["sample"] for row in group} == {f"formal{index}" for index in range(5)}
        for phase in ("init_ms", "create_ms", "begin_ms", "data_ms", "commit_ms", "close_ms"):
            assert statistics.median(float(row[phase]) for row in group) == summary[phase]
        assert min(float(row["data_ms"]) for row in group) == summary["min_ms"]
        assert max(float(row["data_ms"]) for row in group) == summary["max_ms"]
    for pair in comparisons:
        selected = [row for row in rows if row["config"] == pair["config"] and row["operation"] == pair["operation"]]
        reference = {row["sample"]:float(row["data_ms"]) for row in selected if row["route"] == "text"}
        values = [float(row["data_ms"]) for row in selected if row["route"] == pair["route"]]
        differences = [float(row["data_ms"])-reference[row["sample"]] for row in selected if row["route"] == pair["route"]]
        assert statistics.median(differences) == pair["paired_delta_ms"]
        assert statistics.median(values)/statistics.median(reference.values()) == pair["ratio_to_text"]
    for row in rows:
        assert int(row["rows"]) == 1000000 and int(row["count"]) == 1000000
        partner = [other for other in rows if all(other[key] == row[key] for key in ("config", "route", "sample")) and other["operation"] != row["operation"]]
        assert len(partner) == 1 and partner[0]["digest"] == row["digest"]
        log = (root / row["log"]).read_text()
        assert "CHECK\tdigest\tPASS" in log and "COUNTER\t" not in log
    checks = list(root.glob("*-check*-n*.log"))
    assert len(checks) == 40
    for check in checks:
        log = check.read_text()
        assert "CHECK\tfull fields\tPASS" in log
        if "-text-" not in check.name and not check.name.endswith("-n0.log"):
            assert "CHECK\tcorruption rejected" in log
    counters = list(csv.DictReader((root / "counters.tsv").open(), delimiter="\t"))
    for setting in ("A", "B-batch"):
        for route, batch in (("sqlite", 1), ("batch10", 10), ("batch100", 100)):
            values = {(row["phase"], row["counter"]):int(row["value"]) for row in counters if row["config"] == setting and row["route"] == route}
            assert values["read", "run"] == 1 and values["read", "vm_step"] == 10000006
            assert values["write", "run"] == 1000000 // batch
            for phase in ("read", "write"):
                assert values[phase, "sort"] == values[phase, "reprepare"] == values[phase, "autoindex"] == 0
    report = dict(status="PASS", timing_rows=80, paired_comparisons=12, full_field_checks=40,
                  counter_runs=6, same_read_vm=True, current_source_hashes_verified=True)
    (root / "review.json").write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
