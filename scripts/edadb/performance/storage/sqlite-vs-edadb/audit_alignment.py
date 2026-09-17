"""Recompute alignment statistics and verify logs/snapshots without rerunning samples.

python3 audit_alignment.py /tmp/iedadb_alignment_full
"""
import csv
import hashlib
import json
from pathlib import Path
import statistics
import sys

root = Path(sys.argv[1]).resolve()
rows = list(csv.DictReader((root / "samples.tsv").open(), delimiter="\t"))
groups = json.loads((root / "summary.json").read_text())
deltas = json.loads((root / "deltas.json").read_text())
manifest = json.loads((root / "manifest.json").read_text())
checks = json.loads((root / "checks.json").read_text())
runs = manifest["arguments"]["runs"]
assert len(rows) == 20 * runs and len(groups) == 20 and len(checks) == 40
fields = ("init_ms", "create_ms", "begin_ms", "data_ms", "commit_ms", "close_ms", "complete_ms", "rows", "digest")
for row in rows:
    lines = Path(row["log"]).read_text().splitlines()
    # The verification trace must never run in performance processes.
    assert not any(line.startswith("CHECK\t") for line in lines)
    source = next(line.split("\t")[2:] for line in lines if line.startswith("STREAM\t" + row["operation"] + "\t"))
    assert all(float(row[key]) == float(value) for key, value in zip(fields, source))
    if row["operation"] == "read":
        assert int(row["rows"]) == int(row["count"])
    expected = float(row["data_ms"]) + max(0, float(row["begin_ms"])) + max(0, float(row["commit_ms"]))
    assert abs(float(row["complete_ms"]) - expected) < 0.000003
for check in checks:
    label = "0" if check["count"] == manifest["arguments"]["count"] else "check"
    path = root / f'{check["config"]}-{check["variant"]}-{label}-n{check["count"]}.log'
    lines = path.read_text().splitlines()
    counts = [int(line.split("\t")[2]) for line in lines if line.startswith("CHECK\tSQL statements\t")]
    assert counts == [check["count"], 1]
    if check["count"]:
        assert "CHECK\tsame-length corruption rejected" in lines
for group in groups:
    selected = [row for row in rows if all(row[key] == group[key] for key in ("config", "operation", "variant"))]
    assert len(selected) == runs
    for field in fields[:7]:
        values = [float(row[field]) for row in selected]
        for label, function in (("median", statistics.median), ("mean", statistics.mean), ("min", min), ("max", max)):
            actual = group[label + "_" + field]
            assert actual is None if min(values) < 0 else abs(actual - function(values)) < 1e-8
for delta in deltas:
    times = {group["variant"]: group["median_data_ms"] for group in groups
             if (group["config"], group["operation"]) == (delta["config"], delta["operation"])}
    expected = dict(checks_on_raw=times["checks"]-times["raw"],
        checks_on_other=times["aligned"]-times["other"],
        other_without_checks=times["other"]-times["raw"],
        other_with_checks=times["aligned"]-times["checks"],
        interaction=(times["aligned"]-times["other"])-(times["checks"]-times["raw"]),
        residual=times["edadb"]-times["aligned"], total_gap=times["edadb"]-times["raw"])
    assert all(abs(delta[key] - value) < 1e-8 for key, value in expected.items())
for path, expected in manifest["source_sha256"].items():
    assert hashlib.sha256((root / Path(path).name).read_bytes()).hexdigest() == expected
binary = Path(manifest["arguments"]["binary"])
assert hashlib.sha256(binary.read_bytes()).hexdigest() == manifest["binary_sha256"]
config_lines = json.loads((root / "configs.json").read_text())
for row in rows:
    actual = [line for line in Path(row["log"]).read_text().splitlines() if line.startswith("CONFIG\t")]
    assert actual == config_lines[row["config"]]

def fmt(value):
    return "—" if value is None else f"{value:.3f}"

report = ["# SQL/API alignment results", "",
    f'N={manifest["arguments"]["count"]:,} records; {runs} samples/group; units: ms. Median unless specified.',
    "", "40 correctness cases, including actual SQL/count checks; 20 groups. Trace disabled in performance.",
    "", "## Data-only comparison",
    "", "| Config | Variant | Write data | Read data |",
    "| --- | --- | ---: | ---: |"]
for config in ("A", "B-batch"):
    for variant in ("raw", "checks", "other", "aligned", "edadb"):
        selected = {group["operation"]:group["median_data_ms"] for group in groups
                    if (group["config"], group["variant"]) == (config, variant)}
        report.append(f'| {config} | {variant} | {fmt(selected["write"])} | {fmt(selected["read"])} |')
report += ["", "## Same-batch deltas", "",
    "| Config | Operation | Checks−raw | Aligned−other | Other−raw | Aligned−checks | Interaction | EDADB−aligned |",
    "| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |"]
for delta in deltas:
    values = [delta[key] for key in ("checks_on_raw", "checks_on_other", "other_without_checks",
                                    "other_with_checks", "interaction", "residual")]
    report.append("| " + " | ".join([delta["config"], delta["operation"]] + [fmt(value) for value in values]) + " |")
report += ["", "Differences are net effects, not exclusive function times; negative differences are retained.",
    "Create and COMMIT are excluded from data. Read/write variants are from the same batch.",
    "", "## All stage medians", "",
    "| Config | Variant | Operation | Init | Create | BEGIN | Data | COMMIT | Close | Complete | Data min–max |",
    "| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |"]
for group in groups:
    values = [fmt(group["median_" + field]) for field in fields[:7]]
    report.append("| " + " | ".join([group["config"], group["variant"], group["operation"]] + values +
        [fmt(group["min_data_ms"]) + "–" + fmt(group["max_data_ms"])]) + " |")
report += ["", "## Evidence", "",
    "[Samples](samples.tsv), [summary](summary.json), [deltas](deltas.json), [checks](checks.json),",
    "[manifest and source hashes](manifest.json), [actual configuration](configs.json), [SQL](aligned.sql.txt).",
    "Each sample references its original log. Complete is computed per sample; medians need not add."]
(root / "report.md").write_text("\n".join(report) + "\n")
(root / "audit.json").write_text(json.dumps(dict(status="PASS", checks=len(checks), timing_rows=len(rows),
    groups=len(groups), samples_per_group=runs, raw_log_comparison="PASS", statistics="PASS",
    sql_checks="PASS", trace_off_in_performance="PASS", snapshots="PASS", config="PASS"), indent=2) + "\n")
print((root / "audit.json").read_text())
