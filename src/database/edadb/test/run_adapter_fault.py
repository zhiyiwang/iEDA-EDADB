"""Exercise real adapter rollback and same-process retry; all checks are untimed."""
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import sys


def audit_database(path, empty=False):
    with sqlite3.connect(f"file:{path}?mode=ro", uri=True) as connection:
        assert connection.execute("PRAGMA integrity_check").fetchall() == [("ok",)]
        assert connection.execute("PRAGMA foreign_key_check").fetchall() == []
        tables = [row[0] for row in connection.execute(
            "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'")]
        assert len(tables) == 41, len(tables)
        counts = {name: connection.execute('SELECT count(*) FROM "' + name.replace('"', '""') + '"').fetchone()[0]
                  for name in tables}
        if empty:
            assert not any(counts.values()), counts
        else:
            assert counts["iInstSD"] > 0 and counts["iNetSD"] > 1
        indexes = connection.execute("SELECT count(*) FROM sqlite_master WHERE type='index'").fetchone()[0]
        assert indexes == 40, indexes
    return dict(tables=len(tables), indexes=indexes, counts=counts)


if sys.argv[1] == "check-empty":
    audit_database(Path(sys.argv[2]), empty=True)
    print("PASS: schema retained, all 41 tables empty, integrity/FK valid")
    sys.exit(0)

root = Path(__file__).resolve().parents[4]
binary = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2]).resolve()
output.mkdir(parents=True, exist_ok=False)
conversion_fault = "--conversion-fault" in sys.argv[3:]
test = Path(__file__).resolve().parent
shim = output / "adapter_fault.so"
subprocess.run(["g++-10", "-std=c++17", "-O3", "-shared", "-fPIC", str(test / "adapter_fault.cpp"),
                "-ldl", "-lsqlite3", "-o", str(shim)], check=True)
workspace = root / "scripts/design/sky130_gcd"
environment = os.environ.copy()
environment.update(WORKSPACE=str(workspace), CONFIG_DIR=str(workspace / "iEDA_config"),
                   FOUNDRY_DIR=str(root / "scripts/foundry/sky130"),
                   TCL_SCRIPT_DIR=str(workspace / "script"), DESIGN_TCL_SCRIPT_DIR=str(workspace / "script"),
                   DESIGN_TOP="gcd", NETLIST_FILE="/dev/null", SDC_FILE="/dev/null", SPEF_FILE="/dev/null",
                   INPUT_DEF=str(workspace / "result/iPL_filler_result.def"),
                   EDADB_DB_PATH=str(output / "edadb.db"), FAULT_RUNNER=str(Path(__file__).resolve()))
if conversion_fault:
    environment["ADAPTER_CONVERSION_FAULT"] = "1"
else:
    environment.pop("ADAPTER_CONVERSION_FAULT", None)


def run(script, name, preload=False):
    settings = environment.copy()
    settings.pop("LD_PRELOAD", None)
    if preload:
        settings["LD_PRELOAD"] = str(shim)
    with (output / (name + ".log")).open("w") as log:
        subprocess.run([str(binary), "-script", str(script)], cwd=output, env=settings,
                       stdout=log, stderr=subprocess.STDOUT, check=True)


run(test / "tcl/adapter_fault.tcl", "fault", preload=True)
log = (output / "fault.log").read_text()
fault_marker = "FAULT conversion rollback pending" if conversion_fault else "FAULT fired"
for marker in (fault_marker, "FAULT rollback result=0 autocommit=1 instances=0 nets=0",
               "PASS: adapter error reached Tcl", "PASS: schema retained", "PASS: same-process retry succeeded"):
    assert marker in log, marker
database = audit_database(output / "edadb.db")
environment["OUTPUT_DEF"] = str(output / "restored.def")
run(test / "tcl/edadb2def_generic.tcl", "read")
environment["OUTPUT_DEF"] = str(output / "native.def")
run(test / "tcl/direct_def_roundtrip.tcl", "native")
assert (output / "native.def").read_bytes() == (output / "restored.def").read_bytes()
result = dict(status="PASS", fault_mode="conversion" if conversion_fault else "insert",
              binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
              input_sha256=hashlib.sha256(Path(environment["INPUT_DEF"]).read_bytes()).hexdigest(),
              rollback_all_tables_empty=True, same_process_retry=True, strict_def_equal=True, database=database)
(output / "audit.json").write_text(json.dumps(result, indent=2) + "\n")
print("PASS: adapter failure, rollback, same-process retry and strict DEF roundtrip")
