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

SQLite internal time is intentionally not measured yet. Add that only after this command-level
test shows an EDADB bottleneck.

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
