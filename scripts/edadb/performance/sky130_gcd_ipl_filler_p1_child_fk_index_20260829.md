# P1 Child-FK Index Result — 2026-08-29

## Scope

- Parent branch: `edadb-performance-optimization`
- EDADB core branch: `performance/optimization`
- Input: `scripts/design/sky130_gcd/result/iPL_filler_result.def`
- Baseline result: `/tmp/iedadb_profile_filler`
- P1 result: `/tmp/iedadb_p1_profile_filler`
- Build: Release `-O3`; profiling OFF measures absolute time and profiling ON provides attribution.

P1 adds a generic SQLite DDL rule in EDADB core. A generated child table receives one non-unique
index on its complete parent-FK column chain only when it has a parent FK and has no generated
primary key. Existing `FOREIGN KEY ... REFERENCES ...` constraints and adapter schemas are unchanged.

## Implementation

- `SqlStatement4Sqlite.h` generates
  `CREATE INDEX IF NOT EXISTS "<table>__edadb_parent_fk_idx" ON "<table>" (<parent-FK columns>)`.
- `DbTableOpCreate4Sqlite.h` executes that statement immediately after the corresponding
  `CREATE TABLE`, in the same schema-tree traversal and transaction.
- Tables whose generated composite PK already starts with the complete parent FK do not receive a
  duplicate secondary index.
- Vector index columns are not added to the secondary index; vector reconstruction still uses the
  stored vector index independently of SQLite result order.

## Correctness And Query-Plan Evidence

- EDADB core Release tests with profiling OFF: `27/27` passed.
- EDADB core Release tests with profiling ON: `27/27` passed.
- iEDA+EDADB full Debug regression: all object/schema/DEF roundtrip cases passed.
- Every Release benchmark sample passed strict `diff -u native.def edadb.def`.
- A real routed database contains 13 generated `__edadb_parent_fk_idx` indexes.
- Net Point and ViaRef plans changed from `SCAN` to `SEARCH ... USING INDEX` on the complete
  three-column parent-FK chain.
- Net child-FK query count remained `29,699`; P1 changes the access path, not traversal semantics.

The full core CTest suite must run serially because existing tests reuse SQLite filenames. An
initial parallel run produced cross-test `disk I/O error`; rerunning the same OFF/ON suites with
`ctest -j1` passed completely. Independent iEDA regression cases use separate databases and ran with
`EDADB_TEST_JOBS=8`.

## Absolute Release Result

Times are five-run medians from profiling-OFF binaries.

| Cache | Metric | Before P1 | After P1 | Change |
| --- | --- | ---: | ---: | ---: |
| cold | EDADB write | 2,334.579 ms | 1,973.443 ms | 15.47% faster |
| cold | EDADB read | 19,481.822 ms | 242.327 ms | 80.39x faster |
| warm | EDADB write | 2,011.603 ms | 2,224.625 ms | 10.59% slower |
| warm | EDADB read | 19,164.606 ms | 169.480 ms | 113.08x faster |

The agreed gate was at least `2x` warm-read speedup and no more than `20%` warm-write regression.
P1 passes both conditions. The database grew from `2,203,648` to `2,936,832` bytes, an increase of
`33.27%`.

## Profiling Attribution After P1

Profiling ON has observer overhead, so the times below explain proportions and call counts rather
than replacing the OFF absolute result.

| Warm read item | Before P1 | After P1 | Meaning |
| --- | ---: | ---: | --- |
| EDADB read command | 19,138.732 ms | 233.928 ms | Profiling-ON command median |
| Net adapter phase | 18,977.921 ms | 150.185 ms | Remaining largest read family |
| Net SQLite time | 18,922.144 ms | 105.399 ms | Indexed child lookup removes repeated table scans |
| Net SQLite fetch-step | 18,878.006 ms | 73.706 ms | Direct evidence that `SCAN` was the dominant cost |
| Net child-FK queries | 29,699 | 29,699 | N+1 traversal remains but is no longer scan-bound |

Warm profiling-ON overhead is `38.03%` for read and `1.62%` for write. Therefore absolute claims use
the profiling-OFF medians above. P1 also leaves write initialization dominated by SQLite DDL and
transaction `exec`; the new indexes explain the measured write/storage tradeoff.

## Conclusion And Next Decision

P1 is complete. Missing child-side FK indexes, not Shadow conversion, were the primary read
bottleneck for this dataset. P2 would remove the remaining `29,699` N+1 child queries, but the warm
absolute read is now `169.480 ms`; P2 should remain deferred until a larger routed design proves that
the additional traversal complexity is justified. The next lower-risk measured target is write-side
schema/transaction batching, reviewed separately as P3/P4.
