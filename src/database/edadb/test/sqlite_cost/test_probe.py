"""Small exact-count test for the diagnostic collector and boundary nesting."""
import os
from pathlib import Path
import subprocess
import sys

here = Path(__file__).resolve().parent
root = here.parents[4]
output = Path(sys.argv[1]).resolve()
output.mkdir(parents=True, exist_ok=True)
source = output / "probe_test.cpp"
source.write_text(r'''
#include "edadb_stage_timing.h"
#include <sqlite3.h>
#include <cassert>
int main() {
    using namespace idb::edadb_adapter::stage_timing;
    Session session("test");
    sqlite3* database = nullptr;
    assert(sqlite3_open(":memory:", &database) == SQLITE_OK);
    assert(sqlite3_exec(database, "CREATE TABLE test(value INTEGER)", nullptr, nullptr, nullptr) == SQLITE_OK);
    {
        DetailTimer timer("insert");
        sqlite3_stmt* statement = nullptr;
        assert(sqlite3_prepare_v2(database, "INSERT INTO test VALUES(?)", -1, &statement, nullptr) == SQLITE_OK);
        for (int value = 0; value < 2001; ++value) {
            assert(sqlite3_bind_int(statement, 1, value) == SQLITE_OK);
            assert(sqlite3_step(statement) == SQLITE_DONE);
            assert(sqlite3_reset(statement) == SQLITE_OK);
        }
        assert(sqlite3_finalize(statement) == SQLITE_OK);
    }
    {
        DetailTimer timer("select");
        sqlite3_stmt* statement = nullptr;
        assert(sqlite3_prepare_v2(database, "SELECT value FROM test", -1, &statement, nullptr) == SQLITE_OK);
        int count = 0;
        while (sqlite3_step(statement) == SQLITE_ROW) {
            assert(sqlite3_column_int(statement, 0) == count++);
        }
        assert(count == 2001);
        assert(sqlite3_finalize(statement) == SQLITE_OK);
    }
    assert(sqlite3_close_v2(database) == SQLITE_OK);
}
''')
for name, definitions in (("counters", []), ("markers", ["-DMARKERS_ONLY"])):
    subprocess.run(["g++-10", "-std=c++17", "-O3", "-shared", "-fPIC", *definitions,
                    str(here / "probe.cpp"), "-ldl", "-lsqlite3", "-o", str(output / f"{name}.so")], check=True)
subprocess.run(["g++-10", "-std=c++17", "-O3", "-I" + str(root / "src/database/edadb/idb"),
                str(source), "-lsqlite3", "-o", str(output / "probe-test")], check=True)
log = subprocess.check_output([str(output / "probe-test")], stderr=subprocess.STDOUT, text=True,
                              env=os.environ | dict(LD_PRELOAD=str(output / "counters.so"), EDADB_STAGE_TIMING="1"))
(output / "probe-test.log").write_text(log)
rows = [line.split("\t") for line in log.splitlines() if line.startswith("STMT\t")]
assert len(rows) == 2, rows
assert next(int(row[2]) for row in rows if row[1] == "insert") == 2001
assert next(int(row[2]) for row in rows if row[1] == "select") == 1
assert "API\tinsert\treset\t2001" in log
assert "API\tselect\tsqlite3_step\t2002" in log
assert "BOUNDARY\ttest\tcommand\t0" in log
print("PASS exact statement RUN counts, reset drains, select steps and boundaries")
