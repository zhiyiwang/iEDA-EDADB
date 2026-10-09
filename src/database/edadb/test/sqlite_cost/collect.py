"""Audit and retain small evidence files; never copy generated databases or binaries."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import shutil
from review import review

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[4]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--destination", type=Path, required=True)
    arguments = parser.parse_args()
    output = arguments.output.resolve()
    destination = arguments.destination.resolve()
    if destination.exists():
        parser.error("Destination already exists; choose a new batch directory")
    statistics_audit = review(output)
    manifest = json.loads((output / "manifest.json").read_text())
    assert manifest["leaf_batch_read"] == "NOT_IMPLEMENTED"
    assert hashlib.sha256((output / "production.patch").read_bytes()).hexdigest() == manifest["production_patch_sha256"]
    historical = json.loads((HERE.parent / "stream_shadow/results/audit.json").read_text())
    for name, digest in manifest["production_source_sha256"].items():
        assert hashlib.sha256((ROOT / name).read_bytes()).hexdigest() == digest, name
    assert hashlib.sha256(Path(manifest["binary"]).read_bytes()).hexdigest() == manifest["binary_sha256"]
    for name, digest in manifest["core_header_sha256"].items():
        assert hashlib.sha256((ROOT / "src/database/edadb/core" / name).read_bytes()).hexdigest() == digest, name
    for name, digest in manifest["benchmark_source_sha256"].items():
        assert hashlib.sha256((HERE / name).read_bytes()).hexdigest() == digest, name
    files = ["manifest.json", "environment.json", "production.patch", "samples.tsv", "summary.tsv", "sql-statements.tsv",
             "api-counts.tsv", "cache-stages.tsv", "sqlite-config.tsv", "exec-sql.tsv", "query-plans.json",
             "database-layout.json", "cpu-categories.tsv", "cpu-symbols.tsv", "perf-sample-audit.json",
             "io-stages.tsv", "diagnostic-cpu-time.tsv", "probe-test.log", "timer-test.log"]
    counts = {}
    for phase, expected in (("smoke", 4), ("timing", 24), ("counters", 2), ("markers", 2), ("perf", 6), ("io", 2)):
        name = f"{phase}-audit.json"
        checks = json.loads((output / name).read_text())
        assert len(checks) == expected
        for check in checks:
            assert check["status"] == "PASS"
            assert check["dump_sha256"] == historical["database_dump_sha256"][check["dataset"]]
        counts[phase] = len(checks)
        files.append(name)
    samples = list(csv.DictReader((output / "samples.tsv").open(), delimiter="\t"))
    assert len([row for row in samples if row["metric"] == "command_ms"]) == 40
    groups = list(csv.DictReader((output / "summary.tsv").open(), delimiter="\t"))
    assert all(int(row["count"]) == 5 for row in groups)
    perf = json.loads((output / "perf-sample-audit.json").read_text())
    assert len(perf) == 12 and all(row["command_samples"] > 0 and not row["lost_event_messages"] for row in perf)
    validation_files = []
    for name in ("regression", "insert-fault", "conversion-fault"):
        path = output / name / "audit.json"
        assert json.loads(path.read_text())["status"] == "PASS"
        validation_files.append((path, f"{name}-audit.json"))
    for name in files:
        assert (output / name).is_file(), name
    destination.mkdir(parents=True, exist_ok=False)
    for name in files:
        shutil.copyfile(output / name, destination / name)
    for path, name in validation_files:
        shutil.copyfile(path, destination / name)
    audit = dict(status="PASS", roundtrips=counts, formal_command_samples=40, groups=len(groups),
                 database_dumps_equal_to_frozen_streaming_result=True, source_and_binary_hashes_match=True,
                 diagnostic_root=str(output), statistics_review=statistics_audit,
                 production_sources=list(manifest["production_source_sha256"]),
                 benchmark_sources={path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                                    for path in HERE.iterdir() if path.suffix in (".cpp", ".py")})
    (destination / "audit.json").write_text(json.dumps(audit, indent=2) + "\n")
    print(json.dumps(audit, indent=2))


if __name__ == "__main__":
    main()
