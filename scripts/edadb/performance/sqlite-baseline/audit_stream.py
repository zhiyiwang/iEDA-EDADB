"""Audit a completed run without rerunning benchmarks.
python3 audit_stream.py /tmp/iedadb_stream_formal_20260910
Writes audit.json alongside the original timing files.
"""
import collections
import csv
import json
from pathlib import Path
import statistics
import sys
from fixtures import sha

def main():
    root = Path(sys.argv[1]).resolve()
    samples = list(csv.DictReader((root/"samples.tsv").open(), delimiter="\t"))
    summary = list(csv.DictReader((root/"summary.tsv").open(), delimiter="\t"))
    manifest = json.loads((root/"manifest.json").read_text())
    runs = manifest["arguments"]["runs"]
    keys = ("count", "route", "config", "cache", "operation")
    groups = collections.defaultdict(list)
    configs = {}
    for row in samples:
        groups[tuple(row[key] for key in keys)].append(row)
        lines = Path(row["log"]).read_text().splitlines()
        raw = next(line.split("\t")[2:] for line in lines if line.startswith("STREAM\t"+row["operation"]+"\t"))
        fields = ("init_ms","create_ms","begin_ms","data_ms","commit_ms","close_ms","complete_ms","rows","digest")
        assert all(float(row[key]) == float(value) for key,value in zip(fields,raw)), row["log"]
        if row["route"] in ("sqlite","edadb"):
            config = dict(line.split("\t",2)[1:] for line in lines
                          if line.startswith("CONFIG\t") and not line.startswith("CONFIG\tcompile_options"))
            assert row["config"] not in configs or configs[row["config"]] == config
            configs[row["config"]] = config
    assert all(len(rows) == runs for rows in groups.values())
    if "B-default" in configs and "B-batch" in configs:
        assert configs["B-default"] == configs["B-batch"]
    for row in summary:
        selected = groups[tuple(row[key] for key in keys)]
        for field in ("init_ms","create_ms","begin_ms","data_ms","commit_ms","close_ms","complete_ms"):
            values = [float(item[field]) for item in selected]
            for label, function in (("mean",statistics.mean),("median",statistics.median),("min",min),("max",max)):
                actual = row[label+"_"+field]
                assert actual == "NA" if min(values) < 0 else float(actual) == function(values)
    assert sha(Path(manifest["arguments"]["binary"])) == manifest["binary_sha256"]
    for dataset in json.loads((root/"datasets.json").read_text()):
        assert sha(Path(dataset["path"])) == dataset["sha256"]
    result = dict(status="PASS", samples=len(samples), groups=len(groups), samples_per_group=runs,
                  correctness_groups=len(json.loads((root/"checks.json").read_text())),
                  configs=configs, timing_source="samples.tsv checked against STREAM lines in individual logs",
                  statistics="mean/median/min/max recalculated", data_hashes="PASS", binary_hash="PASS")
    result["batch_commit_share_median_percent"] = {
        "/".join(key):statistics.median(float(row["commit_ms"])/float(row["complete_ms"])*100 for row in rows)
        for key,rows in groups.items() if key[2] == "B-batch" and key[4] == "write"}
    (root/"audit.json").write_text(json.dumps(result,indent=2))
    print(json.dumps(result,indent=2))

if __name__ == "__main__":
    main()
