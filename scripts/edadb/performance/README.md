# iEDA And EDADB Performance Test

This test compares native DEF read/write with EDADB write/read. It does not modify iEDA, the
adapter, or EDADB core.

The two groups refer to the Linux OS page cache, not whether iDB objects exist in memory:

- `cold`: call `posix_fadvise(..., POSIX_FADV_DONTNEED)` to evict the input DEF/DB file pages before
  each read;
- `warm`: read the complete input DEF/DB files into the page cache before each read.

`run.sh` automatically executes both groups. Cold eviction is file-specific, does not clear the
machine-wide page cache, and does not require root. It is a Linux advisory request rather than a
guarantee from the kernel.

All samples run strictly sequentially: native, EDADB write and EDADB read complete one at a time,
and the next sample starts only after the current sample passes its DEF comparison. Do not run
multiple performance runners concurrently; CPU, memory-bandwidth and storage contention would make
the measured times incomparable.

An iDB must already exist in memory before either native DEF write or EDADB write. Both writers
always create fresh output files, so cache mode mainly distinguishes the read paths.

## Test Flow

Each sample starts three fresh iEDA processes with the same LEF and input DEF:

```text
native: DEF -> iDB -> native.def
write:  DEF -> iDB -> edadb.db
read:   edadb.db -> iDB -> edadb.def
check:  diff -u native.def edadb.def
```

Only four commands are timed and kept separate:

| Metric | Meaning |
| --- | --- |
| `native_def_read_us` | `def_init` reads DEF into iDB. |
| `native_def_write_us` | `def_save` writes iDB to DEF. |
| `edadb_write_us` | `edadb_write` writes an existing iDB to EDADB. |
| `edadb_read_us` | `edadb_read` restores iDB from EDADB. |

LEF loading, process startup, EDADB write setup `def_init`, and the final correctness `def_save`
are not included in these four command times.

## LEF Timing Boundary

Every native/write/read process first executes:

```tcl
source $DESIGN_TCL_SCRIPT_DIR/DB_script/db_path_setting.tcl
source $DESIGN_TCL_SCRIPT_DIR/DB_script/db_init_lef.tcl
```

`db_init_lef.tcl` calls `tech_lef_init` and `lef_init`. These calls finish before `time_command`
records its start time, so LEF file reading and parsing are excluded. This benchmark does not write
LEF files.

After LEF is loaded, DEF/EDADB restoration must still resolve Site, Layer, Cell Master and ViaRule
references against the in-memory LEF objects. Those lookups are necessary parts of the corresponding
read operation and remain inside `native_def_read_us` or `edadb_read_us`.

The current production `edadb_read` command also scans its reference DEF after restoring EDADB
objects. Therefore `edadb_read_us` measures the complete production EDADB-read command, not a pure
SQLite-only SELECT. Excluding that internal reference-DEF scan would require a separate C++ timing
boundary or API and is outside this no-production-code-change benchmark.

Compare only corresponding operations:

```text
native_def_read_us  <-> edadb_read_us
native_def_write_us <-> edadb_write_us
```

Do not add read and write time together when claiming a read or write speed difference.

## Run

Quick smoke test:

```bash
cd /home/zhiyiwang/cs/arch/eda/iEDA-EDADB
PERF_WARMUPS=0 PERF_RUNS=1 \
OUT_DIR=/tmp/iedadb_perf_smoke \
bash scripts/edadb/performance/run.sh \
  scripts/design/sky130_gcd/result/iPL_filler_result.def
```

Initial performance test:

```bash
cd /home/zhiyiwang/cs/arch/eda/iEDA-EDADB
PERF_WARMUPS=1 PERF_RUNS=5 \
OUT_DIR=/tmp/iedadb_perf_filler \
bash scripts/edadb/performance/run.sh \
  scripts/design/sky130_gcd/result/iPL_filler_result.def
```

Use `iRT_result.def` to emphasize routed Net/Wire/Segment/Via data.

Both commands run `cold` first and `warm` second. No additional cache-mode argument is required.

## Output

`result.tsv` contains the raw command times for both cache groups:

```text
cache_mode  sample  time_unit  native_def_read_us  native_def_write_us  edadb_write_us  edadb_read_us
```

Each sample directory also keeps the database, native/EDADB DEF files, and logs. A sample is
accepted only when the two output DEF files match exactly.

```text
/tmp/iedadb_perf_filler/
├── result.tsv
├── cold/run-1/
│   ├── native.def
│   ├── edadb.db
│   ├── edadb.def
│   ├── native.log
│   ├── write.log
│   └── read.log
└── warm/run-1/
    └── ...
```

Display the result as an aligned table:

```bash
column -t -s $'\t' /tmp/iedadb_perf_filler/result.tsv
```

The default is one discarded warm-up plus five measured runs for each group. `time_unit` is `us`
(microseconds). Compare medians
within the same cache group when reporting a result; use one run only to check that the benchmark
works.

The runner waits five seconds before every warm-up or measured sample. This short quiet interval
reduces carry-over from process teardown, dirty-page writeback and CPU temperature/frequency changes.
Override it with `PERF_SETTLE_SECONDS=N`. It does not prove that the whole server is idle, so formal
measurements should run without other builds or tests; inspect outliers and report the median of the
five runs instead of trusting one sample. A dynamic load-average gate is intentionally avoided
because Linux load average decays slowly and does not distinguish CPU work from I/O wait.

Calculate ratios only within the same row/cache group:

```text
read cost ratio  = edadb_read_us  / native_def_read_us
write cost ratio = edadb_write_us / native_def_write_us
```

A ratio greater than `1` means EDADB is slower for that operation. Do not compare native read with
EDADB write, and do not add read and write times into one ratio.

## EDADB Performance Breakdown

Fine-grained profiling uses two separate Release binaries:

- `bin-release/iEDA`: `EDADB_ENABLE_PROFILING=OFF`; authoritative absolute-performance baseline;
- `bin-profile/iEDA`: `EDADB_ENABLE_PROFILING=ON`; aggregate phase/API measurements.

Profiling never skips SQLite calls and never changes the traversal path. It records elapsed time
around the real operations and prints one aggregate record per metric at the end of each adapter
phase. Records use this format:

```text
EDADB_PROFILE  operation  kind  name  calls  total_us
```

The measured levels are:

| Level | Measurements |
| --- | --- |
| Tcl command | Complete `edadb_write` or `edadb_read`. |
| Adapter phase | Init, each root family, and reference DEF scan. |
| EDADB core | Inclusive root insert/read operator time. |
| SQLite API | Open, close, exec, prepare, bind, step, column fetch, reset and finalize. |
| Shadow | Nested `toShadow()` and `fromShadow()` callbacks invoked by EDADB core. |
| Traversal | Object/scalar/vector and child-FK-query counts. |

Measured reports:

- Baseline breakdown: `sky130_gcd_ipl_filler_profile_20260828.md`.
- P1 child-FK index result: `sky130_gcd_ipl_filler_p1_child_fk_index_20260829.md`.
- P3 schema transaction result: `sky130_gcd_ipl_filler_p3_schema_transaction_20260829.md`.

For one independently reported phase:

```text
sqlite_total = sum(sqlite_* metrics)
non_sqlite_nonshadow = phase_total - sqlite_total - shadow_total
```

`non_sqlite_nonshadow` contains adapter conversion/rebuild, EDADB metadata traversal, C++ allocation
and uninstrumented glue. `core_read/core_write` helps narrow that residual but overlaps SQLite and
Shadow, so it must not be added to them. Timers aggregate in memory and emit only one record per
metric after a phase; profiling does not print per SQL call or per object.

Software timers have observer overhead. Absolute performance always comes from the profiling-OFF
binary. `run_profile.sh` executes the same strict-diff benchmark with OFF and ON binaries and writes
the median overhead to `overhead.tsv`. If command-level overhead exceeds 5%, reduce high-frequency
timer granularity before using the timing proportions; call counts remain useful.

Build the profiling binary:

```bash
cd /home/zhiyiwang/cs/arch/eda/iEDA-EDADB
cmake -S . -B build-profile \
  -DCMAKE_CXX_COMPILER=g++-10 \
  -DCMAKE_C_COMPILER=gcc-10 \
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$PWD/bin-profile" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOMPATIBILITY_MODE=OFF \
  -DBUILD_STATIC_LIB=ON \
  -DEDADB_DEBUG_TRACE_SQL_STMT=OFF \
  -DEDADB_ENABLE_PROFILING=ON
cmake --build build-profile -j40 --target iEDA
```

Run the OFF/ON comparison sequentially:

```bash
PROFILE_WARMUPS=1 PROFILE_RUNS=5 \
PROFILE_OUT_DIR=/tmp/iedadb_profile_filler \
bash scripts/edadb/performance/run_profile.sh \
  scripts/design/sky130_gcd/result/iPL_filler_result.def
```

Outputs:

- `overhead.tsv`: command-level profiling observer overhead;
- `profile.tsv`: every raw aggregate record with cache mode and sample;
- `metric_summary.tsv`: median calls and time for each phase metric;
- `phase_summary.tsv`: per-operation median phase, SQLite, Shadow and residual time;
- `process_summary.tsv`: whole read/write process totals, computed per sample before taking medians;
- `api_summary.tsv`: process-wide SQLite/Shadow API calls and time, also aggregated per sample before
  taking medians.

Use `process_summary.tsv` for percentages that must add up to a command. Do not sum rows from
`phase_summary.tsv`: a sum of independently computed medians is not a valid process median.

The first measured sky130 GCD `iPL_filler_result.def` breakdown is recorded in
`sky130_gcd_ipl_filler_profile_20260828.md`.

## Measurement Implementation And Accounting

### Code changes

| Layer | Modified code | Instrumentation |
| --- | --- | --- |
| Build switch | `src/database/edadb/core/CMakeLists.txt`, `src/database/edadb/core/src/core/CMakeLists.txt` | Adds `EDADB_ENABLE_PROFILING`, default `OFF`, and exports `0/1` to EDADB consumers. |
| Aggregator | `src/database/edadb/core/include/edadb/DbProfiler.h` | `steady_clock` scoped timers, fixed counters, in-memory aggregation and one report per adapter phase. |
| Adapter | `def_write_edadb.cpp`, `def_read_edadb.cpp` | Wraps init, each root family and reference DEF scan with `reset -> phase timer -> original call -> report`. |
| Traversal | `DbObjectTraverser.h` | Counts traversed objects, scalar members and vector members; it does not start one timer per member. |
| EDADB operators | `DbTableOpInsert4Sqlite.h`, `DbTableOpSelect4Sqlite.h`, `DbTableOpQueryGeneric4Sqlite.h` | Inclusive root-operation timers plus root-row/read-call and child-FK-query counters. |
| SQLite manager | `DbManager4Sqlite.h` | Times open, close, raw `exec` and manager-owned prepare/bind/step/column/finalize calls. |
| SQLite statement | `DbStatement4Sqlite.h` | Times every normal-path prepare, bind, write step, fetch step, column access, clear/reset and finalize call. |
| Core Shadow | SQLite insert/select operators | Times EDADB-core-managed nested `toShadow()` and `fromShadow()` callbacks. |
| Command timer | `scripts/edadb/performance/benchmark.tcl` | Uses Tcl `clock microseconds` immediately before and after the synchronous iEDA command. |
| Runner | `scripts/edadb/performance/run.sh`, `run_profile.sh` | Runs fresh sequential processes, controls cache state, records five samples and requires exact DEF diff. |
| Summarizer | `summarize_profile.py` | Aggregates within each sample first, then computes medians and writes overhead/process/API/phase summaries. |

The instrumentation calls the original function exactly once. It does not skip SQL, replace the
backend, or use a dry-run path.

### Timing boundaries

The Tcl command is the authoritative outer envelope:

```text
command_total
  = outside_profiled_phases
  + sum(adapter phase_total)

one adapter phase_total
  = SQLite leaf time
  + core-managed nested Shadow callback time
  + profiled_phase_residual
```

These equalities apply to each individual sample. Reported columns are five-run medians computed
after per-sample aggregation, so independently displayed medians need not add exactly.

- `SQLite leaf time` is additive because each metric wraps one direct SQLite C API call. Bind,
  step, column and reset calls execute sequentially; their timers do not nest. `sqlite_reset`
  intentionally combines `sqlite3_clear_bindings()` and `sqlite3_reset()`.
- `core_read/core_write` are inclusive: they contain traversal, SQLite and nested Shadow time. They
  are diagnostic bounds and must never be added to the leaf totals.
- `shadow_to/shadow_from` currently cover only callbacks automatically invoked by EDADB core for
  nested members/vector elements. Adapter-managed root Shadow conversion is not lost: it remains in
  `profiled_phase_residual`, together with object allocation, metadata traversal, LEF lookup and iDB
  rebuild work. The current data cannot separate those residual components individually.
- Traversal is represented by call counts rather than another high-frequency timer. Its elapsed
  cost is part of `profiled_phase_residual`.
- `outside_profiled_phases` contains adapter reset/report work and small command glue before,
  between or after phases. Therefore phase-report output cannot disappear from the command total.

Intentional benchmark exclusions are not EDADB-command undercount:

- LEF loading and process startup finish before `time_command()` starts;
- EDADB-write setup `def_init` is outside `edadb_write_us`, matching native write-to-write comparison;
- correctness-only `def_save` is outside `edadb_read_us`;
- the production reference DEF scan occurs inside `edadb_read` and is included;
- process teardown after the Tcl command returns is excluded from all command metrics.

### Why profiling does not silently lose time

1. The outer Tcl timer surrounds the complete synchronous `edadb_write` or `edadb_read` command.
   Anything not assigned to a leaf metric remains visible in phase residual or outside-phase time.
2. Each adapter phase resets its own aggregate, so Design, Net, init and reference-scan records
   cannot leak into one another.
3. API calls and callback counts are recorded even when their elapsed time rounds below one
   microsecond, allowing the measured call graph to be checked against rows and traversal counts.
4. Every accepted sample writes native and EDADB-restored DEF files and requires exact text equality.
   The formal run rechecked 24 pairs successfully.
5. EDADB core passed `26/26` tests with profiling both OFF and ON.

### Profiling disturbance

Profiling ON is not zero-overhead. Every timed call executes two `steady_clock` reads and updates an
in-memory aggregate. Counter increments and aggregate updates not included inside a leaf timer are
still included by the surrounding phase/command timer and therefore appear in residual time.

Disturbance is controlled as follows:

- profiling OFF compiles timers/counters to inline no-ops; adapter wrappers directly invoke the
  original function, and the Release binary contains no `EDADB_PROFILE` marker;
- profiling ON prints no per-object or per-SQL-call logs; it emits one aggregate report after each
  phase, outside that phase timer but inside the Tcl command timer;
- OFF and ON use the same Release `-O3` configuration, input, cache modes and strict-diff oracle;
- samples run sequentially after a quiet interval, never concurrently;
- absolute performance uses OFF only; ON is used for proportions and call counts.

The measured warm EDADB-read ON/OFF difference is `-0.14%`. Warm write is `-6.96%`; a negative value
cannot be profiler speedup and demonstrates run-to-run system noise. Therefore no timer cost is
subtracted from the results, and very small microsecond phases must not be overinterpreted. The
19-second Net/SQLite bottleneck is orders of magnitude larger than the unclassified residual and is
stable across cold and warm runs.

### Accounted warm-cache cost

| Process component | Read | Read share | Write | Write share |
| --- | ---: | ---: | ---: | ---: |
| Complete command | 19,138.732 ms | 100% | 1,871.545 ms | 100% |
| Sum of profiled adapter phases | 19,132.670 ms | 99.9683% | 1,871.102 ms | 99.9763% |
| Inclusive EDADB core operators | 19,071.453 ms | 99.6484% | 203.179 ms | 10.8562% |
| SQLite leaf APIs | 19,018.252 ms | 99.3705% | 1,815.205 ms | 96.9897% |
| Core-managed nested Shadow callbacks | 0.762 ms | 0.0040% | 1.139 ms | 0.0609% |
| Profiled phase residual | 113.637 ms | 0.5938% | 54.104 ms | 2.8909% |
| Outside profiled phases | 6.084 ms | 0.0318% | 0.442 ms | 0.0236% |

The residual is an upper bound for all non-SQLite/non-core-managed-Shadow work inside phases. It
does not justify assigning a fabricated time to adapter root conversion, allocation or traversal;
separating those would require an additional explicitly measured subphase.

The inclusive core row is not additive. Read operators enclose root stepping plus recursive child
restoration, so their time nearly equals the command. Write operators enclose row binding/stepping
but not schema initialization or the facade's outer `BEGIN/COMMIT`, explaining why inclusive core
write is only `203.179 ms` while SQLite leaf time is `1,815.205 ms`.

## Measured Phase Result — 2026-08-28

The following tables use the profiling-ON warm-cache median from five measured runs. The complete
warm command medians are `19,138.732 ms` for EDADB read and `1,871.545 ms` for EDADB write. Absolute
performance remains the profiling-OFF result; these tables attribute time within the command.

The values are measured, not estimated:

- command samples: `/tmp/iedadb_profile_filler/profiled/result.tsv`;
- raw profiler records extracted from each process log: `/tmp/iedadb_profile_filler/profile.tsv`;
- per-sample component medians: `/tmp/iedadb_profile_filler/process_summary.tsv`;
- per-sample SQLite/Shadow API medians and calls: `/tmp/iedadb_profile_filler/api_summary.tsv`;
- exact-diff databases and DEF outputs: `/tmp/iedadb_profile_filler/profiled/{cold,warm}/run-*`.

The `SCAN`/`SEARCH` conclusion was verified with `EXPLAIN QUERY PLAN` against the real database
`/tmp/iedadb_profile_filler/profiled/warm/run-3/edadb.db`: Wire and Segment parent-FK queries use
their composite primary-key indexes, while Point and ViaRef parent-FK queries report full-table
`SCAN` because those child tables have no corresponding FK index.

### Complete time distribution

| Cache | Process | Command | SQLite | Core nested Shadow | Remaining command time |
| --- | --- | ---: | ---: | ---: | ---: |
| cold | read | 19,156.527 ms | 19,026.666 ms (99.32%) | 0.767 ms (0.0040%) | 118.739 ms (0.62%) |
| warm | read | 19,138.732 ms | 19,018.252 ms (99.37%) | 0.762 ms (0.0040%) | 119.699 ms (0.63%) |
| cold | write | 2,087.791 ms | 2,032.129 ms (97.33%) | 1.139 ms (0.0546%) | 54.462 ms (2.61%) |
| warm | write | 1,871.545 ms | 1,815.205 ms (96.99%) | 1.139 ms (0.0609%) | 54.546 ms (2.91%) |

Each component is summed within one sample before its five-run median is calculated. Consequently,
displayed medians need not add exactly. `Remaining command time` is the median of
`command - SQLite - Shadow` for each sample; it includes adapter rebuild/conversion, EDADB metadata
traversal, allocation, reference DEF scan and the small command region outside profiled phases.

Warm-cache SQLite/Shadow API distribution:

| Process | API | Calls | Time | Command share | Interpretation |
| --- | --- | ---: | ---: | ---: | --- |
| read | `sqlite_fetch_step` | 64,905 | 18,963.612 ms | 99.0850% | Dominant cost: execute/advance many nested SELECTs. |
| read | `sqlite_fetch_column` | 400,151 | 33.564 ms | 0.1754% | Converting returned columns is small. |
| read | `sqlite_bind` | 86,640 | 11.294 ms | 0.0590% | FK parameter binding is small. |
| read | `sqlite_reset` | 61,092 | 8.058 ms | 0.0421% | Reusing prepared statements is inexpensive. |
| read | `sqlite_prepare` | 35 | 1.331 ms | 0.0070% | Low count proves statement preparation is not the bottleneck. |
| read | core nested `shadow_from` | 17,753 | 0.762 ms | 0.0040% | Core-managed nested conversion is negligible. |
| write | `sqlite_exec` | 92 | 1,652.985 ms | 88.3219% | Schema/setup and transaction begin/commit dominate. |
| write | `sqlite_write_step` | 34,358 | 107.737 ms | 5.7566% | Actual row insertion is secondary. |
| write | `sqlite_bind` | 298,800 | 40.571 ms | 2.1678% | Serializing/binding columns is measurable but not dominant. |
| write | `sqlite_reset` | 68,716 | 12.550 ms | 0.6706% | Statement reuse overhead is small. |
| write | `sqlite_prepare` | 26 | 0.900 ms | 0.0481% | Preparation is negligible. |
| write | core nested `shadow_to` | 17,753 | 1.139 ms | 0.0609% | Core-managed nested conversion is not a useful first target. |

### Read phases

| Adapter phase | Time | Command share | SQLite | Residual | Main work and measured cause |
| --- | ---: | ---: | ---: | ---: | --- |
| helper init | 0.000 ms | 0.0000% | 0.000 ms | 0.000 ms | Stores the existing iDB service pointer; below microsecond resolution. |
| database init | 0.444 ms | 0.0023% | 0.194 ms | 0.253 ms | Opens SQLite and maps the 15 existing table trees. |
| reference DEF scan | 30.546 ms | 0.1596% | 0.000 ms | 30.546 ms | Runs the production reference-DEF parser lifecycle with persisted root callbacks disabled. |
| Design | 0.624 ms | 0.0033% | 0.605 ms | 0.019 ms | Reads one scalar root row. |
| Die | 0.090 ms | 0.0005% | 0.059 ms | 0.032 ms | Reads one root plus one point-vector child query and rebuilds the bounding box. |
| Row | 0.243 ms | 0.0013% | 0.162 ms | 0.080 ms | Reads 39 roots and rebuilds LEF Site references and row bounding boxes. |
| TrackGrid | 0.162 ms | 0.0008% | 0.107 ms | 0.055 ms | Reads 12 roots and 12 ordered layer-name child vectors. |
| GCellGrid | 0.050 ms | 0.0003% | 0.037 ms | 0.013 ms | Reads six direct scalar roots. |
| Via | 0.196 ms | 0.0010% | 0.124 ms | 0.060 ms | Reads four roots/four child queries and rebuilds layer/via references; core nested Shadow costs 0.013 ms. |
| Instance | 33.283 ms | 0.1739% | 11.060 ms | 21.967 ms | Reads 2,604 roots; most residual is adapter master/reference lookup and object rebuild. |
| Pin | 1.776 ms | 0.0093% | 1.297 ms | 0.474 ms | Reads 56 roots, 168 child queries and 224 traversed objects. |
| Blockage | 0.063 ms | 0.0003% | 0.050 ms | 0.012 ms | Input table is empty; only prepares and confirms exhaustion. |
| Region | 0.039 ms | 0.0002% | 0.032 ms | 0.007 ms | Input table is empty. |
| Slot | 0.043 ms | 0.0002% | 0.036 ms | 0.007 ms | Input table is empty. |
| Group | 0.032 ms | 0.0002% | 0.026 ms | 0.006 ms | Input table is empty. |
| Fill | 0.037 ms | 0.0002% | 0.030 ms | 0.007 ms | Input table is empty. |
| SpecialNet | 83.792 ms | 0.4378% | 78.724 ms | 4.970 ms | Reads two roots, 649 child queries and 1,346 nested objects. |
| Net | 18,977.921 ms | 99.1598% | 18,922.144 ms | 55.152 ms | Reads 677 roots through 29,699 nested child queries; Point/ViaRef FK lookups scan whole child tables. |

`Residual` is `phase_total - SQLite - core-managed nested Shadow`; nested Shadow time is omitted
from the table because it is only `0.762 ms` across the complete read. Empty phases prove the
empty-table path only; this input does not measure non-empty Blockage/Region/Slot/Group/Fill
performance.

### Write phases

| Adapter phase | Time | Command share | SQLite | Residual | Main work and measured cause |
| --- | ---: | ---: | ---: | ---: | --- |
| database init | 1,072.374 ms | 57.2989% | 1,071.019 ms | 1.371 ms | Executes 72 schema/setup statements while creating the 15 table trees. |
| Design | 44.145 ms | 2.3587% | 44.100 ms | 0.036 ms | One row; fixed per-root transaction cost dominates. |
| Die | 50.168 ms | 2.6806% | 50.140 ms | 0.028 ms | Three traversed rows; fixed transaction cost dominates. |
| Row | 50.119 ms | 2.6779% | 50.022 ms | 0.097 ms | 39 root rows; fixed transaction cost dominates. |
| TrackGrid | 41.926 ms | 2.2402% | 41.861 ms | 0.065 ms | 12 roots/24 total rows; fixed transaction cost dominates. |
| GCellGrid | 41.741 ms | 2.2303% | 41.721 ms | 0.021 ms | Six direct rows; fixed transaction cost dominates. |
| Via | 46.181 ms | 2.4675% | 46.100 ms | 0.078 ms | Four roots; fixed transaction cost dominates, Shadow costs 0.003 ms. |
| Instance | 128.526 ms | 6.8674% | 116.158 ms | 12.061 ms | Converts and writes 2,604 roots; row steps plus one root-family transaction. |
| Pin | 53.524 ms | 2.8599% | 52.927 ms | 0.582 ms | Writes 56 roots/224 total rows; transaction cost remains dominant. |
| Blockage | 0.000 ms | 0.0000% | 0.000 ms | 0.000 ms | Empty input; adapter returns before opening a write transaction. |
| Region | 0.000 ms | 0.0000% | 0.000 ms | 0.000 ms | Empty input. |
| Slot | 0.000 ms | 0.0000% | 0.000 ms | 0.000 ms | Empty input. |
| Group | 0.000 ms | 0.0000% | 0.000 ms | 0.000 ms | Empty input. |
| Fill | 0.000 ms | 0.0000% | 0.000 ms | 0.000 ms | Empty input. |
| SpecialNet | 106.709 ms | 5.7017% | 103.697 ms | 2.932 ms | Writes two roots/1,346 total rows in one root-family transaction. |
| Net | 261.123 ms | 13.9523% | 223.432 ms | 36.894 ms | Converts 677 roots and writes 30,107 rows; row stepping is visible after transaction cost. |

Every non-empty root family uses `insertObject()` or `insertVector()` with its default self-managed
transaction, producing one `BEGIN` and one `COMMIT`. Across each sample before taking the median,
SQLite `exec` takes `1,652.985 ms` (`88.32%` of the warm command), while all `toShadow()` callbacks
automatically invoked by EDADB core take only `1.139 ms`; adapter root conversion remains in the
phase residual.

Cold and warm call counts are identical. The major cold/warm phase differences are first-access and
system noise rather than a different traversal path; the full cold table and raw samples remain in
the dated report and `/tmp/iedadb_profile_filler`.

## Optimization Priority From The Measurements

The order below follows measured recoverable time, dependency between changes, and implementation
risk. Apply and benchmark one item at a time; otherwise the source of improvement or regression is
not identifiable.

### P1 — Missing child-FK indexes — Complete

**Evidence:** Net read takes `18,977.921 ms`; SQLite `fetchStep` takes `18,878.006 ms` inside that
phase. The real Point and ViaRef child tables have no index on their three-column parent FK, and
`EXPLAIN QUERY PLAN` reports `SCAN`. Wire and Segment use composite-PK prefix indexes and report
`SEARCH`.

**Why it is slow:** EDADB asks for Point/ViaRef children of one Segment at a time. Without a child-FK
index, SQLite scans 14,256 Point rows or 3,716 ViaRef rows repeatedly.

**Implementation:**

1. Extend SQLite schema creation in `SqlStatement4Sqlite.h` / `DbTableOpCreate4Sqlite.h` to create a
   composite index on every child table's complete parent-FK column chain.
2. Do not create a duplicate index when an existing composite primary-key index already begins with
   the same FK columns, as in Wire and Segment.
3. Keep vector reconstruction based on stored vector indices; do not depend on index scan order.

**Acceptance:** query plans change from `SCAN` to indexed `SEARCH`; row/callback/query counts remain
unchanged; all core tests and 24 strict DEF comparisons pass. Re-run the same five-sample benchmark
and report read improvement, write-index maintenance cost and database-size growth.

**Measured result:** Point/ViaRef now use `SEARCH`; the warm profiling-OFF read median changed from
`19,164.606 ms` to `169.480 ms` (`113.08x` faster), while write changed from `2,011.603 ms` to
`2,224.625 ms` (`10.59%` slower). Child-query count remains `29,699`, and database size increased
`33.27%`. Core OFF/ON tests and the complete adapter regression passed. See
`sky130_gcd_ipl_filler_p1_child_fk_index_20260829.md` for raw evidence and accounting.

### P2 — N+1 recursive child queries — Deferred

**Evidence:** Net restore performs 29,699 child-FK queries, 59,807 Net fetch steps and only eight
prepares. Prepared statements are reused; repeated query execution is the issue.

**Why it is slow:** the current recursive path in `DbTableOpSelect4Sqlite.h::readByForeignKey()`
executes one child query for each parent object. Indexes remove table scans but do not remove the
29,699 bind/step/reset query cycles.

**Implementation after P1 is measured:**

1. Read each large child table once, ordered/grouped by the complete ancestor FK chain and its
   stored vector index.
2. Stream rows into a map/current-parent group and publish one completed child vector to its parent.
3. Preserve sparse/order indices, pointer ownership and the current failure-atomic staging contract.
4. Start with Net Point/ViaRef; do not rewrite every child-table path until this proves useful.

**Acceptance:** child query count drops materially while object/scalar/vector counts, restored graph,
strict DEF and failure tests remain unchanged. If P1 already makes read sufficiently fast, defer P2
because it is a larger traversal change.

P1 reduced warm absolute EDADB read to `169.480 ms`, so P2 is deferred until a larger routed design
shows that the unchanged N+1 count is again a material bottleneck.

### P3 — Schema creation uses many committed operations — Complete

**Evidence:** write init costs `1,072.374 ms` (`57.30%` of write) and executes 72 SQLite `exec` calls.
`initAllTables()` registers 15 table trees, and each `createTable()` currently uses its own
self-managed transaction in addition to the child-table DDL.

**Implementation:**

1. Open one explicit schema transaction before `initAllTables(true)`.
2. Call `createTable<T>(self_txn=false)` for every root table tree.
3. Commit once after all DDL succeeds; rollback the complete schema on failure.
4. For an existing database, use an explicit schema-version record to skip only proven-compatible
   creation work. Never skip compatibility validation based only on table existence.

**Acceptance:** schema is byte/DDL equivalent, foreign-key checks pass, failure rolls back all schema
changes, and `sqlite_exec` count/time decreases. Current development validates newly created
databases; old-database migration remains out of scope.

**Measured result:** one outer schema transaction changed warm init from `1,328.322 ms` to
`93.294 ms` (`14.24x`), reduced init `sqlite_exec` calls from `85` to `57`, and improved the warm
profiling-OFF write median from `2,224.625 ms` to `1,131.619 ms` (`49.13%`). Database size and read
behavior are unchanged. See `sky130_gcd_ipl_filler_p3_schema_transaction_20260829.md`.

### P4 — One transaction per non-empty root family — Next Review

**Evidence:** every non-empty `insertObject/insertVector` performs `BEGIN` and `COMMIT`. The current
input has ten non-empty families, adding 20 transaction `exec` calls. Small families still cost
roughly `40–50 ms`, even when writing only one to 39 rows.

**Implementation:**

1. Begin one data transaction around `writeChip2Edadb()` after schema creation.
2. Pass `self_txn=false` through each per-family insert call.
3. Commit only after all root families succeed; rollback the complete design on failure.
4. If lock duration or database size makes one transaction unacceptable, use a documented small
   number of stage transactions, not one transaction per class.

**Acceptance:** verify whole-design rollback semantics, duplicate-PK/error recovery, unchanged DEF/DB
content and reduced `sqlite_exec` time. This intentionally changes failure behavior from partially
committed families to atomic design write, so the API contract must document the change.

### P5 — Root Shadow materialization can amplify memory

**Status:** potential large-design scalability issue, not the current measured time bottleneck.
Instance, Pin, SpecialNet and Net writers first materialize vectors of root Shadow objects before
calling `insertVector()`. Routed Net shadows may duplicate a large nested routing graph temporarily.

**Implementation after transaction batching:** reuse one prepared insert operator inside the outer
transaction, convert one root object to one temporary Shadow, insert it, then destroy/reuse it before
converting the next root. Preserve root/nested order fields and rollback behavior.

**Acceptance:** measure peak RSS as well as time on iRT/PicoRV32A-sized routed DEFs. Object/row counts
and strict DEF must remain unchanged. Do not claim a benefit from the small GCD case alone.

### P6 — Adapter/object rebuild residual

**Evidence:** warm phase residual is `113.637 ms` for read and `54.104 ms` for write, below `3%` of
either command. It contains adapter root Shadow conversion, allocation, metadata traversal, LEF
lookup and iDB rebuilding; the current profiler does not split those components.

**Implementation only if later needed:** add coarse adapter subphase timers first, then optimize the
largest measured subphase through vector `reserve`, reusable temporary objects or batched name
lookup. Do not optimize based on guesses.

### Deferred/non-problems in the current profile

- `sqlite_prepare`: `1.331 ms` read and `0.900 ms` write; statement preparation is not the bottleneck.
- core-managed nested Shadow callbacks: `0.762 ms` read and `1.139 ms` write.
- reference DEF scan: `30.546 ms`, only `0.16%` of read.
- cold and warm read times are nearly equal; OS page-cache misses do not explain the 19-second read.
- adding EDADB/iDB multithreading before fixing scans/N+1 would add synchronization and SQLite
  contention around an inefficient algorithm. Revisit parallelism only after P1–P4.

Do not change SQLite durability pragmas merely to obtain a faster number. Journal/synchronous mode
is a separate durability contract and must be compared under explicitly documented guarantees.

## Release Build

Performance measurements must use an optimized Release build, not the repository's default Debug
build in `bin/iEDA`. Build a separate binary so normal development artifacts are not overwritten:

```bash
cd /home/zhiyiwang/cs/arch/eda/iEDA-EDADB

# Build the Rust iIR archive when the shared ExternalProject stamp exists but
# its Release archive is absent.
cd src/operation/iIR/source/iir-rust/iir
cargo build --release
cd /home/zhiyiwang/cs/arch/eda/iEDA-EDADB

cmake -S . -B build-release \
  -DCMAKE_CXX_COMPILER=g++-10 \
  -DCMAKE_C_COMPILER=gcc-10 \
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$PWD/bin-release" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCOMPATIBILITY_MODE=OFF \
  -DCMD_BUILD=ON \
  -DBUILD_STATIC_LIB=ON
cmake --build build-release -j40 --target iEDA
```

Verify the selected configuration and optimization flags:

```bash
grep '^CMAKE_BUILD_TYPE:' build-release/CMakeCache.txt
grep '^COMPATIBILITY_MODE:' build-release/CMakeCache.txt
grep -- '-O3' build-release/src/apps/CMakeFiles/iEDA.dir/flags.make
```

`run.sh` uses `bin-release/iEDA` by default. Set `IEDA_BIN` explicitly only when another verified
Release binary should be measured.
