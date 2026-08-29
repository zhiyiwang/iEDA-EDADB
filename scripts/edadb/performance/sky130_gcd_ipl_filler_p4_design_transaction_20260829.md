# P4 Design Transaction Result — 2026-08-29

## Scope

- Parent branch: `edadb-performance-optimization`
- EDADB core branch: `performance/optimization`
- P3 baseline: `/tmp/iedadb_p3_profile_filler`
- P4 result: `/tmp/iedadb_p4_profile_filler`
- Input: `scripts/design/sky130_gcd/result/iPL_filler_result.def`
- Build: Release `-O3`; profiling OFF measures absolute time and profiling ON provides attribution.

P4 changes only the design-write transaction boundary. Schema, table definitions, indexes, object
traversal and Shadow conversion remain unchanged.

## Implementation

`DefWriteEdadb::writeDb2Edadb()` now opens one explicit transaction after schema initialization,
runs the selected writer, and commits only after every enabled root family succeeds. Every root
`insertObject()` / `insertVector()` call uses `self_txn=false`. A root conversion or insert failure
rolls back all design rows, and the previously ignored writer return value is now propagated.

The EDADB transaction test covers two independent root tables in one explicit transaction. A
duplicate key in the second table must roll back rows already inserted into both tables; the success
path must commit both tables.

## Correctness

- EDADB core Release suite passed: 26/26 tests.
- Targeted Debug and profiling-ON transaction tests passed.
- Complete iEDA+EDADB adapter regression passed with `EDADB_TEST_JOBS=8`.
- Every Release performance sample passed strict native/EDADB DEF comparison.
- Database size remains `2,936,832` bytes.

The first Release adapter run used `EDADB_OUTPUT_DEBUG=OFF`; its DEF and SQL assertions passed, while
debug-log assertions correctly did not run. The canonical Debug regression was then run and passed
all cases, including those log assertions.

## Absolute Release Result

Times are profiling-OFF five-run medians.

| Cache | Metric | P3 | P4 | Change |
| --- | --- | ---: | ---: | ---: |
| cold | EDADB write | 1,119.777 ms | 401.164 ms | 64.17% faster |
| cold | EDADB read | 199.610 ms | 312.935 ms | 56.77% slower |
| warm | EDADB write | 1,131.619 ms | 378.269 ms | 66.57% faster |
| warm | EDADB read | 167.552 ms | 172.396 ms | 2.89% slower |

The cold-read samples have substantially higher variance and P4 does not modify read code. The warm
read median, which is the stable no-I/O-noise comparison, remains within the 5% no-regression gate.

## Profiling Attribution

| Warm write item | P3 | P4 | Change |
| --- | ---: | ---: | ---: |
| SQLite `exec` calls | 77 | 59 | 18 fewer calls |
| SQLite `exec` time | 849.969 ms | 220.784 ms | 74.02% faster |
| adapter schema init | 93.294 ms | 88.359 ms | unchanged within noise |
| profiling-ON write command | 1,138.702 ms | 439.128 ms | 61.44% faster |
| one design `COMMIT` | not isolated | 128.714 ms | one durable boundary |

The input has ten non-empty root families. P3 therefore used ten `BEGIN` and ten `COMMIT` calls for
design rows; P4 uses one `BEGIN` and one `COMMIT`, exactly explaining the reduction of 18 SQLite
`exec` calls. Empty root families still return without inserting rows, but they remain inside the
same outer transaction.

Profiling instrumentation adds 16.09% to the warm write command after P4 because the optimized
command is much shorter. Absolute performance conclusions therefore use profiling-OFF medians;
profiling-ON data is used only for call counts and phase attribution.

## Conclusion

P4 is complete. One atomic design transaction removes nine durable commit boundaries and improves
warm EDADB write by 66.57% without changing database content or materially regressing warm read.
Failure semantics are intentionally stronger: a failed root family no longer leaves earlier root
families committed.
