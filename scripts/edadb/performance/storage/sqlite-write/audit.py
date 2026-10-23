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
    assert len(rows) == 70
    summaries = json.loads((root / "summary.json").read_text())
    manifest = json.loads((root / "manifest.json").read_text())
    for name, expected in manifest["hashes"].items():
        assert hashlib.sha256(Path(name).read_bytes()).hexdigest() == expected, name
    comparisons = []
    for summary in summaries:
        group = [row for row in rows if row["setting"] == summary["setting"] and row["route"] == summary["route"]]
        assert len(group) == 5 and {row["repeat"] for row in group} == set(map(str, range(5)))
        for phase in ("init", "create", "begin", "data", "commit", "close"):
            assert statistics.median(float(row[phase]) for row in group) == summary[phase]
    for setting in ("memory", "disk"):
        for baseline, variant in (("single", "text"), ("single", "batch10"), ("single", "batch100"), ("switch1", "switch10"), ("switch1", "grouped")):
            reference = {row["repeat"]: float(row["data"]) for row in rows if row["setting"] == setting and row["route"] == baseline}
            changes = [float(row["data"])-reference[row["repeat"]] for row in rows if row["setting"] == setting and row["route"] == variant]
            comparisons.append(dict(setting=setting, baseline=baseline, variant=variant,
                                    delta_ms=statistics.median(changes), min_ms=min(changes), max_ms=max(changes),
                                    delta_percent=100*statistics.median(changes)/statistics.median(reference.values())))
    for logfile in root.glob("*.log"):
        content = logfile.read_text()
        assert "CHECK\tfull fields\tPASS" in content, logfile
        if "CONFIG\t" in content:
            memory = "-memory-" in logfile.name
            assert f"CONFIG\tjournal_mode\t{'memory' if memory else 'delete'}\n" in content
            assert f"CONFIG\tsynchronous\t{0 if memory else 2}\n" in content
    result = dict(status="PASS", formal_samples=70, pairs=comparisons, source_hashes_verified=True,
                  actual_journal_sync_verified=True)
    (root / "review.json").write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
