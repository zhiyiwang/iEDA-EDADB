import csv
import json
from pathlib import Path
import statistics
import sys


def main():
    directory = Path(sys.argv[1])
    rows = list(csv.DictReader((directory / "samples.tsv").open(), delimiter="\t"))
    counters = list(csv.DictReader((directory / "counters.tsv").open(), delimiter="\t"))
    summary = json.loads((directory / "summary.json").read_text())
    assert len(rows) == 200 and len(summary) == 40
    keys = ("backend", "setting", "write_block", "read_block", "operation")
    groups = {}
    for row in rows:
        assert int(row["count_per_table"]) == 100000
        group = groups.setdefault(tuple(row[key] for key in keys), {})
        assert row["repeat"] not in group
        group[row["repeat"]] = float(row["data"])
    assert len(groups) == 40 and len({row["digest"] for row in rows}) == 1
    for item in summary:
        group = groups[tuple(item[key] for key in keys)]
        assert set(group) == set("01234")
        assert statistics.median(group.values()) == item["data"]
        assert min(group.values()) == item["min_ms"]
        assert max(group.values()) == item["max_ms"]
    for setting in ("memory", "disk"):
        for writing, reading in (("1", "all"), ("10", "all"), ("all", "all"), ("all", "1"), ("all", "10")):
            group = [row for row in counters if row["setting"] == setting and row["write_block"] == writing and row["read_block"] == reading]
            for phase, runs in (("write", 300000), ("read", 3)):
                values = lambda name: sum(int(row["value"]) for row in group if row["phase"] == phase and row["counter"] == name)
                assert values("run") == runs
                assert values("sort") == values("reprepare") == 0
    comparisons = []
    def compare(label, baseline, candidate):
        before, after = groups[baseline], groups[candidate]
        delta = [after[repeat] - before[repeat] for repeat in before]
        comparisons.append(dict(label=label, baseline=baseline, candidate=candidate,
            median_delta_ms=statistics.median(delta), min_delta_ms=min(delta), max_delta_ms=max(delta),
            median_time_ratio=statistics.median(after.values()) / statistics.median(before.values())))
    for key in groups:
        backend, setting, writing, reading, operation = key
        if backend == "sqlite":
            compare("SQLite minus matching text", ("text", *key[1:]), key)
        if operation == "write" and reading == "all" and writing != "1":
            compare("write switching versus each row", (backend, setting, "1", "all", operation), key)
        if operation == "read" and writing == "all" and reading != "1":
            compare("read switching versus each row", (backend, setting, "all", "1", operation), key)
    result = dict(status="PASS", timing_rows=len(rows), groups=len(groups), comparisons=comparisons)
    (directory / "review.json").write_text(json.dumps(result, indent=2))
    print("PASS: 200 timing rows, 40 groups, 10 counter controls; review.json written")


if __name__ == "__main__":
    main()
