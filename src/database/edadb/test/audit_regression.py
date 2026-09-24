"""Audit completed 15-case regression, preserving strict versus normalized distinctions."""
import hashlib
import json
from pathlib import Path
import sqlite3
import sys

output = Path(sys.argv[1]).resolve()
binary = Path(sys.argv[2]).resolve()
cases = ("default_ipl design_fields design_fallback die_polygon aux_optional pin_derived "
         "pin_writer pin_branches group_branches special_net_branches grid_branches "
         "via_branches instance_branches routed_irt net_branches").split()
results = []
for case in cases:
    folder = output / case
    log = (output / "case-logs" / (case + ".log")).read_text()
    assert "FAIL:" not in log and f"case output: {folder}" in log, case
    raw_diff = folder / "direct_vs_edadb.diff"
    strict = not raw_diff.read_bytes()
    if not strict:
        assert not (folder / "direct_vs_edadb.normalized.diff").read_bytes(), case
        assert "DEF semantic match with D-level root order differences" in log, case
    with sqlite3.connect(f"file:{folder / 'edadb.db'}?mode=ro", uri=True) as connection:
        assert connection.execute("PRAGMA integrity_check").fetchall() == [("ok",)], case
        assert connection.execute("PRAGMA foreign_key_check").fetchall() == [], case
    results.append(dict(case=case, comparison="strict" if strict else "normalized_root_order",
                        assertions=log.count("PASS:"), integrity="ok", foreign_key_check="ok",
                        restored_def_sha256=hashlib.sha256((folder / "edadb.def").read_bytes()).hexdigest()))
summary = dict(status="PASS", cases=len(results), strict=sum(row["comparison"] == "strict" for row in results),
               normalized=sum(row["comparison"] != "strict" for row in results),
               binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(), results=results)
(output / "audit.json").write_text(json.dumps(summary, indent=2) + "\n")
print(json.dumps(summary, indent=2))
