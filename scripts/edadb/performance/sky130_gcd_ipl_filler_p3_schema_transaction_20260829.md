# P3 Schema Transaction Result — 2026-08-29

## Scope

- Parent branch: `edadb-performance-optimization`
- EDADB core branch: `performance/optimization`
- P1 baseline: `/tmp/iedadb_p1_profile_filler`
- P3 result: `/tmp/iedadb_p3_profile_filler`
- Input: `scripts/design/sky130_gcd/result/iPL_filler_result.def`
- Build: Release `-O3`; profiling OFF measures absolute time and profiling ON provides attribution.

P3 changes only the schema transaction boundary. The 15 adapter root table trees still use the same
EDADB `createTable<T>()` implementation, table order, DDL, PK/FK definitions and P1 indexes.

## Implementation

`initWriteDb()` now starts one explicit transaction, calls every root `createTable<T>(false)`, and
commits after all table trees succeed. Any table/index failure rolls back the complete schema. The
implementation reuses the existing EDADB facade API; no new production core API was added.

The EDADB transaction test now also verifies the exact schema contract used by the adapter:

1. begin a transaction;
2. run `createTable<T>(false)`;
3. inject an invalid SQL statement;
4. rollback and prove root/child tables are absent;
5. repeat, commit and prove both tables exist.

## Correctness

- Targeted Debug build passed.
- Complete iEDA+EDADB adapter regression passed with `EDADB_TEST_JOBS=8`.
- EDADB explicit schema transaction test passed with profiling OFF and ON.
- Every Release performance sample passed strict `diff -u native.def edadb.def`.
- P1 schema/index definitions and database size are unchanged.

## Absolute Release Result

Times are profiling-OFF five-run medians.

| Cache | Metric | P1 | P3 | Change |
| --- | --- | ---: | ---: | ---: |
| cold | EDADB write | 1,973.443 ms | 1,119.777 ms | 43.26% faster |
| cold | EDADB read | 242.327 ms | 199.610 ms | 17.63% faster |
| warm | EDADB write | 2,224.625 ms | 1,131.619 ms | 49.13% faster |
| warm | EDADB read | 169.480 ms | 167.552 ms | 1.14% faster |

The database remains `2,936,832` bytes. P3 passes the agreed gates: schema init improves by more than
50%, total warm write improves by more than 20%, and warm read does not regress.

## Profiling Attribution

| Warm write item | P1 | P3 | Change |
| --- | ---: | ---: | ---: |
| adapter schema init | 1,328.322 ms | 93.294 ms | 92.98% faster / 14.24x |
| init SQLite exec time | 1,326.736 ms | 92.456 ms | 93.03% faster |
| init SQLite exec calls | 85 | 57 | 28 fewer calls |
| profiling-ON write command | 2,260.632 ms | 1,138.702 ms | 49.63% faster |

The call-count reduction is exact: 15 self-managed root schema transactions required 30
`BEGIN/COMMIT` calls; one outer transaction requires two, so P3 removes 28 calls. More importantly,
it removes 14 durable commit boundaries, which explains the much larger time reduction than the
call-count reduction alone.

## Conclusion

P3 is complete. Schema initialization is no longer the dominant write cost. The remaining warm write
time is concentrated in one transaction per non-empty root family; P4 can batch those design writes
into one atomic transaction. P4 changes design failure semantics, so it must remain a separate review,
commit and benchmark rather than being combined with P3.
