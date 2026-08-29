# EDADB Performance Optimization Plan

This is the single source of truth for pending EDADB performance changes. Performance findings stay
in `README.md` and dated reports; design decisions, implementation order, acceptance criteria and
unresolved questions stay only in this file.

## Baseline

- iEDA+EDADB branch: `edadb-performance-optimization`
- Parent baseline: `62504bd9d` (`perf: add EDADB performance breakdown workflow`)
- EDADB core branch: `performance/optimization`
- EDADB core baseline: `be6bbdd` (`perf: add reusable EDADB profiling instrumentation`)
- Measured input: `scripts/design/sky130_gcd/result/iPL_filler_result.def`
- Measurements and accounting: `scripts/edadb/performance/README.md`
- Raw-result report: `scripts/edadb/performance/sky130_gcd_ipl_filler_profile_20260828.md`

## Branch Isolation

- All parent-repository performance work is restricted to `edadb-performance-optimization`.
- All EDADB core performance work is restricted to `performance/optimization`.
- The parent branch currently records the exact EDADB gitlink `be6bbdd`, and that commit is the tip of
  `performance/optimization`. A Git superproject records a submodule commit, not a branch name.
- Normal `git submodule update --init --recursive` is reproducible because it checks out the recorded
  gitlink. On this parent performance branch, `.gitmodules` declares
  `branch = performance/optimization`, so `git submodule update --remote` also follows the isolated
  EDADB performance branch instead of `main`.
- Proposed workflow for every EDADB change: commit and push EDADB `performance/optimization` first;
  then update the parent gitlink, commit and push `edadb-performance-optimization`.
- Decision: use `branch = performance/optimization` only on the parent performance branch. The exact
  gitlink remains authoritative for reproducible normal submodule updates; the branch setting only
  controls explicit `--remote` updates.

Do not implement an optimization until its design and acceptance criteria are agreed. Apply one
optimization per commit and remeasure before starting the next one. P1 through P5 are complete;
P6 remains conditional.

## Confirmed Bottlenecks

| Priority | Status | Confirmed observation | Baseline/result |
| --- | --- | --- | ---: |
| P1 | complete | Net Point/ViaRef child-FK queries used full-table `SCAN` | warm read `19,164.606 -> 169.480 ms` |
| P2 | complete | Wire-scoped leaf batching removes repeated Segment leaf queries | queries `447,699 -> 11,739`; warm read `1,966.411 -> 1,473.071 ms` |
| P3 | complete | Schema creation used one self-managed transaction per root tree | warm init `1,328.322 -> 93.294 ms` |
| P4 | complete | One atomic transaction now covers all design root families | warm write `1,131.619 -> 378.269 ms` |
| P5 | complete | Full root-Shadow vectors amplified routed-write memory | peak RSS `302,184 -> 273,356 KiB` |

P6 adapter/object rebuild remains deferred until P2 removes the known query-granularity bottleneck.

## Proposed Execution Order

### P1: Add Child-FK Indexes — Complete

Goal: change Point/ViaRef parent-FK lookup from table `SCAN` to indexed `SEARCH` without changing
schema relationships or vector reconstruction semantics.

Proposed implementation:

1. Generate a deterministic non-unique composite index for the complete child-side parent-FK chain.
2. Skip the index when an existing primary-key/index prefix already covers the same FK chain.
3. Keep `FOREIGN KEY ... REFERENCES ...` unchanged; the new index is an access path, not a constraint.
4. Keep nested-vector reconstruction based on the stored vector index, never on SQLite row order.

Decision: implement this as a generic EDADB SQLite DDL rule. The general rule is to add the index only
when no existing PK/index has the complete parent-FK chain as its leftmost prefix. Under the current
EDADB schema generator, every child with `hasPrimKey=true` receives
`PRIMARY KEY(parent-FK columns..., child PK)`, and EDADB generates no other secondary indexes.
Therefore the P1 implementation condition is exactly:

```text
parentFkc().valid() && Cpp2SqlTypeTrait<T>::hasPrimKey == false
```

Point/ViaRef are measured examples, not an adapter-specific allowlist. P1 does not change any PK
definition. If a vector index is already part of the generated composite PK, that PK index covers the
parent-FK query and no secondary index is created. If it is not part of a PK, P1 indexes only the
parent-FK columns required by `QUERY_BY_FK`; it does not promote the vector index into a PK or add it
to the secondary index.

Resolved decisions:

- Index generation belongs in generic EDADB SQLite DDL, not the iDB adapter.
- The deterministic name is `<full-child-table-name>__edadb_parent_fk_idx`.
- Existing-database migration is out of scope while the schema is still under development. P1 is
  required only for a newly created EDADB database.

Acceptance:

- Point/ViaRef plans report indexed `SEARCH`; Wire/Segment do not receive duplicate indexes.
- EDADB core tests pass with profiling OFF and ON.
- All strict native/EDADB DEF comparisons pass.
- Row counts, traversal counts and child-query counts are unchanged.
- Five sequential Release samples report read median, write median and database-size delta.
- Quantitative gate: warm EDADB-read five-run median improves by at least 50% (`>=2x` speedup), while
  warm EDADB-write median regresses by no more than 20%. Database-size growth is reported but has no
  P1 hard limit.
- If the plans use `SEARCH` but the read gate is missed, P1 is not complete; profile the remaining
  cost before deciding whether P2 is justified.

Stop and review after P1. Do not begin P2 until the new profile shows whether N+1 query execution is
still the dominant read cost.

Measured result: P1 passed every acceptance gate. Warm profiling-OFF EDADB read improved from
`19,164.606 ms` to `169.480 ms` (`113.08x`), warm write changed from `2,011.603 ms` to
`2,224.625 ms` (`+10.59%`), and database size increased `33.27%`. Point/ViaRef plans use indexed
`SEARCH`; the Net child-query count remains `29,699`. Full evidence is in
`sky130_gcd_ipl_filler_p1_child_fk_index_20260829.md`.

#### P1 design analysis

The existing generated DDL already contains the correct referential-integrity constraint:

```sql
FOREIGN KEY (<child parent-key columns>)
REFERENCES <parent table> (<parent primary-key columns>)
ON DELETE CASCADE ON UPDATE CASCADE
```

That constraint validates parent/child relationships but does not create an SQLite index on the
child columns. EDADB's `QUERY_BY_FK` path independently executes:

```sql
SELECT ... FROM <child table>
WHERE <child parent-key column 0> = ?
  AND <child parent-key column 1> = ?
  ...;
```

The generated schema has two relevant shapes:

1. A child store with `hasPrimKey=true` receives the composite primary key
   `(all parent-FK columns, child primary key)`. Its SQLite primary-key index already covers the
   parent-FK query as a leftmost prefix. Wire and Segment have this shape and report `SEARCH`.
2. A child store with `hasPrimKey=false` receives the FK constraint but no primary key or secondary
   index. Point and ViaRef have this shape and report full-table `SCAN`.

The proposed generic P1 rule is therefore narrower than "index every foreign key":

```text
child table has a parent FK
AND child store has no primary key whose generated prefix covers that FK
→ create one non-unique composite index over the complete child-side parent-FK chain
```

For the current generated schema, `hasPrimKey=true` is sufficient to prove prefix coverage because
`createTableStatement()` always places every parent-FK column before the child primary-key column.
No adapter-specific type list is required.

Recommended generated SQL:

```sql
CREATE INDEX IF NOT EXISTS
  "<full-child-table-name>__edadb_parent_fk_idx"
ON "<full-child-table-name>" (<complete child-side parent-FK columns>);
```

Decision: the deterministic index name is
`<full-child-table-name>__edadb_parent_fk_idx`. The complete generated table name keeps the index
schema-readable and schema-wide unique; quoted SQLite identifiers handle the generated name safely.

Rationale:

- The index is non-unique because many child rows may belong to the same parent.
- The complete FK chain is required because nested tables are identified by all ancestor keys.
- The table name is already unique in the SQLite schema, so appending one reserved EDADB suffix gives
  a deterministic schema-wide index name without a hash or adapter knowledge.
- The stored vector index is not added to P1. Current reads do not use SQL order; they validate the
  stored index and place each child at that position. Adding it would change index width without
  reducing the measured FK lookup scan.

Implementation boundary:

- Add one SQL generator for the child-FK index in `SqlStatement4Sqlite.h`.
- Invoke it immediately after successful child-table creation in `DbTableOpCreate4Sqlite.h`.
- Do not change `ForeignKeyConstraint`, table macros, adapter schema, `QUERY_BY_FK`, vector recovery,
  root order or transaction behavior in the P1 commit.
- Keep table creation and index creation as separate checked statements so an index failure cannot be
  mistaken for successful schema initialization. P3 may later batch their transaction boundary.

Decision: execute `CREATE INDEX` immediately after the corresponding `CREATE TABLE` succeeds, inside
the same existing `createTable<T>()` transaction and the same single schema-tree traversal. Do not
defer index creation until after data insertion. An index error fails the table-tree operation and is
rolled back with that transaction.

#### P1 traversal and SQL order

P1 does not require a second traversal of the EDADB schema tree. The current CREATE operator visits
the root and recursively visits each vector-child table once. At each visited table node it already
has the typed `DbTableDef`, so the proposed local operation is:

```text
visit one table node
  → generate and execute CREATE TABLE
  → inspect this node's parent-FK/PK metadata
  → if its FK is uncovered, generate and execute CREATE INDEX
  → continue the existing traversal to child table nodes
```

Generating the index statement may collect this table node's FK-column names again, but that is a
small local metadata pass, not another recursive schema-tree traversal. Avoiding that local pass by
refactoring all CREATE statement generation into a new intermediate object would enlarge P1 without
addressing the measured runtime bottleneck.

SQLite still parses two SQL statements because SQLite does not define a secondary child-key index
inside `CREATE TABLE`. Their required order is:

```text
CREATE parent table
→ CREATE child table with FOREIGN KEY
→ CREATE child-FK index
→ INSERT child rows later
```

The index may technically be built after data insertion, but P1 proposes immediate creation because
it keeps schema initialization complete before any read/write operation. This adds index-maintenance
cost to inserts and one extra DDL statement per uncovered child table; both are intentional measured
tradeoffs. Deferring index creation until after bulk load would couple schema and design-write
lifecycle and is a separate optimization, not part of P1.

For the P1 database the index is created while the child table is empty. Old non-empty database
migration cost is intentionally not measured. Under the current self-managed `createTable<T>()`
transaction, an index creation failure returns failure and rolls back that root table tree. Under
future P3 batching, the outer schema transaction must provide the same rollback guarantee.

Database compatibility scope:

- P1 targets a newly created EDADB SQLite database: the file does not exist yet, or it contains no
  EDADB schema/data before `initWriteDb()` creates the table trees and indexes.
- Old milestone databases do not need migration or compatibility validation during current
  development. Direct `initReadDb()` of an old database is outside P1 acceptance.
- Keep `CREATE INDEX IF NOT EXISTS` for idempotent repeated schema initialization of the same current-
  version development database, not as a promise to migrate arbitrary old schemas.
- Tests inspect `PRAGMA index_list` and `PRAGMA index_info` on the newly generated database to prove the
  expected complete FK column sequence.
- Do not introduce schema-version or migration code in P1.

P1 failure cases that must be tested:

- root table: no parent FK, therefore no parent-FK index;
- child with generated composite PK: no duplicate secondary index;
- child without PK: exactly one non-unique complete-FK index;
- nested child with multiple ancestor FK columns: preserve their generated order;
- repeated initialization: no duplicate object and the same verified index definition;
- injected index-creation failure: `createTable<T>()` reports failure;
- query result order changes: vector reconstruction remains correct because stored vector indices are
  authoritative.

P1 is complete only when correctness, query-plan and performance evidence all agree. A faster time
without `SEARCH`, or `SEARCH` with changed object/vector results, is not acceptance.

#### P1 validation sequence

1. **EDADB schema unit tests:** cover root, PK child, no-PK child and multi-level no-PK child shapes;
   verify `foreign_key_list`, `index_list` and ordered `index_info` results.
2. **EDADB query-plan test:** populate enough parents/children to prevent a trivial planner choice and
   require the no-PK child FK query to report indexed `SEARCH` while the PK child has no duplicate
   secondary index.
3. **EDADB object roundtrip tests:** verify row counts, pointer cleanup, sparse vector indices,
   duplicate-index rejection and exact nested-vector reconstruction with profiling OFF and ON.
4. **iEDA+EDADB regression:** rebuild Release, run canonical, optional and routed fixtures, and require
   strict DEF equality plus existing SQLite assertions.
5. **Absolute benchmark:** profiling OFF, one warm-up plus five sequential cold and warm samples using
   the same `iPL_filler_result.def`; report read/write/init medians and raw samples.
6. **Breakdown benchmark:** profiling ON with the same input; require unchanged child-query/traversal
   counts, `SCAN` to `SEARCH`, reduced `fetchStep` time and an explicit profiling-overhead check.
7. **Storage tradeoff:** report database byte-size growth and fresh-schema index-creation cost. Old
   non-empty database migration is outside P1.

Correctness builds/tests may run in parallel. Performance samples remain sequential and use the same
Release compiler flags, inputs, cache protocol and settle interval as the baseline.

### P2: Reduce N+1 Child Queries — Complete

Goal: reduce repeated bind/step/reset cycles after indexed lookup has been measured.

Implemented design:

1. Add a default-off leaf-vector FK-prefix path and enable it explicitly only in the iEDA Net reader.
2. For `Net -> Wire -> Segment -> leaf`, bind the `Net + Wire` FK prefix once, group rows by Segment
   key, then restore Point/ViaRef/VirtualPoint by their stored vector-order column.
3. Cache only one Wire scope and clear the cache at every root-row boundary; do not materialize a
   second full database graph.
4. Retain the existing complete-FK query when batching is disabled, the vector is not a leaf, or its
   FK depth is less than two.

The rule is depth-independent. For a leaf table with `d` ancestor FK columns, the normal lookup has
`d` equality predicates. Prefix batching removes the immediate-parent key from the predicate, so it
uses `d - 1` equality predicates, reads that final FK as the group key, and orders by two columns:
the immediate-parent FK and the leaf's stored vector-order/local-key column. Example: Point has three
ancestor keys `(Net, Wire, Segment)`, therefore the Wire batch uses two predicates `(Net, Wire)` and
`ORDER BY Segment, _vec_idx`. A deeper leaf applies the same formula; query depth grows, while the
two ordering roles do not.

Acceptance:

- Child-query count drops materially while restored object counts and graph ownership remain equal.
- Sparse/duplicate vector-index validation and failure-atomic staging remain covered by core tests.
- Strict DEF comparisons pass and P1 indexes remain used where applicable.

Measured result: all acceptance gates passed. Net child-FK queries fell from `447,699` to `11,739`
(`-97.38%`), and the five-run warm profiling-OFF EDADB-read median improved from `1,966.411 ms` to
`1,473.071 ms` (`-25.09%`). The cold write median changed `+4.59%`, below the 5% gate; warm write
improved. Peak read RSS did not regress, and database bytes/SHA-256 remained identical. Core 27/27,
ASan/UBSan SELECT, complete adapter regression and all strict performance/RSS diffs passed. See
`sky130_gcd_routed_p2_leaf_batch_20260829.md`.

The earlier routed decision evidence remains in `sky130_gcd_p2_reassessment_20260829.md`; the final
implementation and measurements are in `sky130_gcd_routed_p2_leaf_batch_20260829.md`.

#### P2+ TODO: Root-window graph loading

The long-term generic N+1 solution is a bounded root-window loader. Read a fixed number of roots,
query each descendant table once for that root-key set, group by the complete ancestor FK chain, and
attach rows by stored vector index. This reduces query count toward
`root_window_count * relation_count` while bounding memory. Do not implement it as one giant JOIN,
which would multiply sibling vectors. This remains future work after the completed Wire-level P2;
it requires a buffered root cursor and a broader traversal contract than the current explicit leaf
optimization.

For an arbitrary-depth leaf with `d` ancestor FK columns, let `p` be the number of leading ancestor
keys fixed by one batch query. The SQL then has `p` equality predicates, while the remaining
`d - p` FK columns identify the parent group inside the returned window. To restore an ordered
vector directly from the result stream, use `ORDER BY` on those `d - p` grouping columns followed by
the leaf vector-order column, for `d - p + 1` ordering columns in total. The current Wire-scoped P2
is the special case `p = d - 1`: query depth can increase, but SQL still orders by only the immediate
parent key and the leaf vector index. A wider root window reduces query count further, but adds
grouping/order columns and memory proportional to the number of rows in the selected window.

### P3: Batch Schema Creation — Complete

Goal: create all 15 root table trees in one schema transaction.

Proposed implementation:

1. Begin one transaction around `initAllTables(true)`.
2. Call every `createTable<T>(false)` inside it.
3. Commit once, or rollback the complete schema after any failure.
4. Keep old-database migration and schema-version handling out of scope during current development.

Acceptance:

- Fresh schema DDL, PK, FK and indexes match the expected schema.
- Injected failure leaves no partial schema.
- Explicit schema commit and rollback behavior is tested.
- Five sequential Release samples show init/`sqlite_exec` change.

Measured result: warm schema init changed from `1,328.322 ms` to `93.294 ms` (`14.24x`), init
`sqlite_exec` calls changed from `85` to `57`, and warm profiling-OFF EDADB write changed from
`2,224.625 ms` to `1,131.619 ms` (`49.13%` faster). Read and database size did not regress. Full
evidence is in `sky130_gcd_ipl_filler_p3_schema_transaction_20260829.md`.

### P4: Batch Design Writes — Complete

Goal: replace one transaction per non-empty root family with one atomic design transaction, unless a
measured lock/journal limit requires a small documented number of stage transactions.

Proposed implementation:

1. Begin the transaction after schema initialization and before `writeChip2Edadb()` root writes.
2. Pass `self_txn=false` to root `insertObject()`/`insertVector()` calls.
3. Commit only after every root family succeeds; otherwise rollback the whole design.
4. Document the intentional change from partial root-family commits to atomic design persistence.

Acceptance:

- Success produces identical DB rows and strict-equal DEF output.
- Duplicate-PK and injected mid-family failures rollback the complete design.
- Lock duration, journal/WAL growth and five-sample write median are reported.

Measured result: one outer design transaction changed warm write from `1,131.619 ms` to
`378.269 ms`, reduced write `sqlite_exec` calls from `77` to `59`, preserved database content, and
passed whole-design rollback, all 26 core tests and the complete adapter regression. See
`sky130_gcd_ipl_filler_p4_design_transaction_20260829.md`.

### P5: Stream Root Shadow Writes — Complete

A reproducible 1,000-net routed stress fixture proved `28,656 KiB` of EDADB-specific peak RSS above
the native path. Instance, Pin, SpecialNet and Net now reuse one insert operator and
convert/insert/release one stack Shadow at a time inside the P4 transaction. Peak RSS fell by
`28.15 MiB`; stress write elapsed changed by `+0.59%`; the SQLite database is byte-identical; strict
DEF, all 26 core tests and the full adapter regression pass. See
`sky130_gcd_routed_p5_stream_shadow_20260829.md`.

### P6: Split Adapter Residual Only If Needed

The measured residual is `113.637 ms` for read and `54.104 ms` for write. Add only coarse timers for
root conversion, allocation/rebuild and reference lookup if this residual becomes material after
P1-P4. Do not optimize an unmeasured subcomponent.

## Validation Rules

- Performance samples are sequential; parallel samples measure contention rather than operation cost.
- Independent builds and correctness tests may run in parallel to use server resources efficiently.
- Use Release `-O3`; profiling OFF gives absolute time, profiling ON gives attribution and counts.
- Use identical LEF/DEF/configuration and require strict DEF equality for every accepted sample.
- Compare five-run medians and retain raw samples; also report database size and peak RSS when relevant.
- One optimization per commit. If an acceptance check fails, stop and discuss rather than stacking
  another optimization on top of an unresolved regression.

## Discussion Order

1. Database scope decided: newly created development databases only; old milestone migration and
   schema-version compatibility are outside P1.
2. Index scope decided: generic EDADB SQLite DDL; every generated child with a parent FK and no
   generated PK receives a complete non-unique parent-FK index. This is the current generated-schema
   form of the general "existing PK/index left-prefix coverage" rule.
3. Index naming decided: `<full-child-table-name>__edadb_parent_fk_idx`.
4. Performance gate decided: at least 50% warm-read median reduction, no more than 20% warm-write
   median regression, with five sequential Release samples and all correctness gates passing.
5. P3 and P4 are separate commits: P3 changes schema atomicity; P4 will change design-write atomicity.
6. Select the larger routed design used to decide P5.
