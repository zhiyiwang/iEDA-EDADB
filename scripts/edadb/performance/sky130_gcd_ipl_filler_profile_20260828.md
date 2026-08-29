# Sky130 GCD iPL Filler EDADB Profile — 2026-08-28

## Purpose

This report identifies where EDADB DEF persistence spends time without changing database or
adapter behavior. Absolute command time comes from a profiling-OFF Release binary. A separate,
otherwise identical profiling-ON Release binary records aggregate phase, SQLite, Shadow and
traversal measurements.

Input:

```text
scripts/design/sky130_gcd/result/iPL_filler_result.def
```

Command:

```bash
PROFILE_WARMUPS=1 PROFILE_RUNS=5 PROFILE_SETTLE_SECONDS=5 \
PROFILE_OUT_DIR=/tmp/iedadb_profile_filler \
bash scripts/edadb/performance/run_profile.sh \
  scripts/design/sky130_gcd/result/iPL_filler_result.def
```

The benchmark executes samples sequentially. Each sample starts fresh native, EDADB-write and
EDADB-read iEDA processes. LEF load time is outside the four measured Tcl commands. Every sample
must pass an exact `diff` between the native DEF output and the EDADB-restored DEF output.

## Build And Validation

- Parent baseline: `7b661baa25041c1c2c69c27dd5ada6dc8d75121c`.
- EDADB baseline: `90a5fb249c7b2f49f890bf6573fcc5fb05056b18`.
- Both binaries use `CMAKE_BUILD_TYPE=Release` and `-O3`.
- `bin-release/iEDA`: `EDADB_ENABLE_PROFILING=OFF`; contains no profiling marker.
- `bin-profile/iEDA`: `EDADB_ENABLE_PROFILING=ON`; SQL statement trace remains disabled.
- EDADB core tests passed `26/26` with profiling OFF and `26/26` with profiling ON.
- The formal run completed 24 samples: OFF/ON × cold/warm × one warm-up plus five measured runs.
- All 24 native/EDADB DEF comparisons passed exactly.

## Command-Level Observer Check

Median command times and measured ON-versus-OFF differences:

| Cache | Command | Profiling OFF | Profiling ON | Difference |
| --- | --- | ---: | ---: | ---: |
| cold | EDADB read | 19.482 s | 19.157 s | -1.67% |
| cold | EDADB write | 2.335 s | 2.088 s | -10.57% |
| warm | EDADB read | 19.165 s | 19.139 s | -0.14% |
| warm | EDADB write | 2.012 s | 1.872 s | -6.96% |

Profiling cannot make EDADB work faster. Negative values show run-to-run system variance and
temporal drift, not a speedup. The stable warm EDADB read differs by only `0.14%`; no positive
profiling overhead is measurable at command level. Absolute performance claims must still use the
OFF values.

The profiled cold native DEF-write control had unrelated outliers and reported `+206.24%` even
though the native path does not invoke EDADB profiling. This is additional evidence that small
native-write times are dominated by environmental noise in this run.

## Whole-Process Breakdown

The summarizer first adds all phases within each sample and then takes the median. SQLite and Shadow
are mutually exclusive leaf measurements; `core_read/core_write` are inclusive and are not added.

| Cache | Process | Command | SQLite | Core nested Shadow | Non-SQLite/non-Shadow | Outside phases |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| cold | read | 19.157 s | 19.027 s (99.32%) | 0.000767 s (0.0040%) | 0.119 s (0.62%) | 0.0059 s |
| warm | read | 19.139 s | 19.018 s (99.37%) | 0.000762 s (0.0040%) | 0.120 s (0.63%) | 0.0061 s |
| cold | write | 2.088 s | 2.032 s (97.33%) | 0.001139 s (0.0546%) | 0.054 s (2.61%) | 0.0004 s |
| warm | write | 1.872 s | 1.815 s (96.99%) | 0.001139 s (0.0609%) | 0.055 s (2.91%) | 0.0004 s |

Conclusion: core-managed nested Shadow conversion is not the current bottleneck. Adapter-managed
root Shadow conversion is included in the phase residual rather than this Shadow metric; even the
complete residual is small compared with SQLite API time.

## Read Bottleneck

Warm read counters:

| Counter/metric | Value |
| --- | ---: |
| Root read calls / rows | 3,415 / 3,402 |
| Traversed objects | 34,358 |
| Traversed scalar members | 238,340 |
| Traversed vector members | 30,533 |
| Child foreign-key queries | 30,533 |
| Core-managed nested `fromShadow()` calls/time | 17,753 / 0.000762 s |
| SQLite `fetchStep()` calls/time | 64,905 / 18.960 s |

`IdbNet` alone consumes `18.978 s`, or `99.16%` of the complete warm EDADB-read command. Its
SQLite `fetchStep()` time is `18.878 s`, or `98.64%` of that command. Net restoration performs:

- 677 root Net rows;
- 29,699 nested child-FK queries;
- 59,807 SQLite fetch steps;
- only 8 prepares, proving that statements are reused and prepare is not the main cost.

The Net object graph is restored recursively as Net → Wire → Segment → Point/ViaRef. The database
contains 677 Wire rows, 8,997 Segment rows, 14,256 Point rows and 3,716 ViaRef rows.

SQLite query-plan evidence:

- Wire lookup uses the composite primary-key index by Net name.
- Segment lookup uses the composite primary-key index by Net name and Wire key.
- Point and ViaRef tables have no index on their parent foreign-key columns; their child lookup is
  reported as `SCAN` by `EXPLAIN QUERY PLAN`.

Therefore the measured dominant read cost is the combination of an N+1 recursive child-query
pattern and repeated full scans of the Point/ViaRef tables. All non-SQLite work, including adapter
root conversion and object rebuild, is bounded by the much smaller residual.

## Write Bottleneck

Warm write performs 34,358 row steps and 17,753 core-managed nested `toShadow()` callbacks. Those
callbacks cost `0.001139 s`. Adapter-managed root conversion remains in the phase residual.
Aggregated within each sample before taking the median, SQLite `exec` consumes `1.653 s`, or
`88.32%` of the complete write command:

- adapter initialization executes 72 schema/setup statements and costs `1.071 s` (`57.22%` of the
  command);
- each active root family uses two transaction-control `exec` calls;
- actual SQLite row steps cost about `0.107 s` in the cold profile and are not the leading cost.

The first write optimization target is schema/init and transaction execution. Core-managed nested
Shadow conversion is not a useful optimization target for this workload; the complete residual is
also too small to explain the observed write cost.

## Next Optimization Experiments

1. Add indexes for every vector-child parent-FK lookup, especially Net Point and ViaRef tables.
2. Re-run the same OFF/ON benchmark and verify `EXPLAIN QUERY PLAN` changes from `SCAN` to `SEARCH`.
3. If indexed N+1 queries remain expensive, batch/prefetch child rows by table and reconstruct the
   object graph in memory.
4. Separate first-time schema creation from steady-state write, then reduce redundant table/setup
   checks and review the current per-root transaction boundary.

These are follow-up optimization experiments. This profiling branch measures and diagnoses the
current implementation; it does not yet change EDADB query or transaction semantics.
