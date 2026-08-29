# P2 Net Leaf-Vector Batch Read Result — 2026-08-29

## Goal

Reduce routed-Net N+1 child queries without changing the default EDADB read path. The optimization
must be explicitly enabled by the iEDA Net reader, preserve vector order and ownership, keep failed
reads atomic at the existing vector/root staging boundary, and retain P5's low-memory behavior.

## Input And Baseline

The stress input is generated from the real Sky130 GCD `iRT_result.def` by copying one complete,
parser-proven routed NET record under 1,000 unique names:

```bash
python3 scripts/edadb/performance/make_routed_stress_fixture.py \
  scripts/design/sky130_gcd/result/iRT_result.def \
  /tmp/p2_leaf_batch_routed_1000.def 1000
```

The pre-P2 baseline is documented in `sky130_gcd_p2_reassessment_20260829.md`: `1,677` root Nets,
`447,699` Net child-FK queries, cold/warm EDADB-read medians of `2,304.850/1,966.411 ms`, and a warm
EDADB/native ratio of `3.73x`.

## Implementation

- EDADB keeps the existing complete-FK `QUERY_BY_FK` behavior by default.
- `enableLeafBatchRead()` propagates an explicit read policy from a root operator to descendants.
- Only leaf vectors with at least two ancestor FKs use the batch path.
- For `Net -> Wire -> Segment -> Point/ViaRef/VirtualPoint`, one query binds `(Net, Wire)`, returns
  the Segment FK as a grouping key, and restores each Segment vector by `_vec_idx`, `_order_sd`, or
  `__edadb_vec_idx`.
- Each cache contains one Wire scope and is cleared before every root row, so it never materializes
  a second complete routed graph and cannot reuse stale target addresses across a root cursor.
- Rows are staged before commit. A conversion failure clears all staged pointer elements and leaves
  the pre-existing target graph unchanged.
- Only `DefReadEdadb::readIdbNet()` enables the policy. Other adapter roots and EDADB callers retain
  the original path.

Core files: `DbTableOperator.h`, `DbForeignKeyBinder.h`, `DbTableOpSelect4Sqlite.h`,
`DbTableOpQueryGeneric4Sqlite.h`, `SqlStatement4Sqlite.h` and `DbProfiler.h`. Adapter opt-in:
`src/database/manager/builder/def_builder/def_read_edadb.cpp`.

## Query-Count Result

One profiling-enabled Release sample produced the following stable Net counters:

| Counter | Before P2 | After P2 | Change |
| --- | ---: | ---: | ---: |
| Net child-FK queries | `447,699` | `11,739` | `-435,960` (`-97.38%`) |
| Leaf batch loads | `0` | `5,031` | one load per Net/Wire/leaf relation |
| Leaf parent-vector hits | `0` | `440,991` | three leaf vectors per `146,997` Segments |

The remaining `11,739` queries are exactly:

```text
3 * 1,677 Net child queries
+ 1 * 1,677 Wire child queries
+ 3 * 1,677 Wire-scoped leaf batches
= 11,739
```

`EXPLAIN QUERY PLAN` reports indexed `SEARCH` for Point and ViaRef using the P1 parent-FK index.
Their local vector-order column still requires a temporary B-tree for the right side of `ORDER BY`.
VirtualPoint uses its ancestor-first composite primary-key index without a temporary sort.

## Five-Run Release Performance

Command:

```bash
PERF_WARMUPS=1 PERF_RUNS=5 PERF_SETTLE_SECONDS=5 \
  IEDA_BIN="$PWD/bin-release/iEDA" \
  OUT_DIR=/tmp/iedadb_p2_leaf_batch_release5 \
  bash scripts/edadb/performance/run.sh /tmp/p2_leaf_batch_routed_1000.def
```

Every warm-up and measured sample passed strict native-vs-EDADB DEF diff. Values are medians of five
sequential profiling-disabled Release samples.

| Cache | Native read | EDADB read | Read ratio | EDADB read change | Native write | EDADB write | EDADB write change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| cold | `575.597 ms` | `1,779.656 ms` | `3.09x` | `-22.79%` | `77.941 ms` | `3,004.750 ms` | `+4.59%` |
| warm | `527.350 ms` | `1,473.071 ms` | `2.79x` | `-25.09%` | `124.807 ms` | `2,785.886 ms` | `-5.81%` |

The cold write increase remains below the agreed 5% regression gate; warm write improved. P2 does
not modify the write path, so these differences are run-to-run SQLite/filesystem variation.

## Peak RSS And Database Identity

Five sequential `/usr/bin/time -f '%M\t%e'` samples produced these medians:

| Mode | Median max RSS | Median process elapsed |
| --- | ---: | ---: |
| native | `273,496 KiB` | `10.83 s` |
| EDADB write | `273,644 KiB` | `13.49 s` |
| EDADB read | `273,440 KiB` | `11.78 s` |

EDADB read is `56 KiB` below the native median and `304 KiB` below the pre-P2 EDADB-read median;
both are within process-level measurement noise. The one-Wire cache therefore introduces no
measured peak-RSS regression. All five warm databases remain byte-identical to the pre-P2 result:
`42,295,296` bytes and SHA-256
`dd96004d179cf565296fe1a012b68329eac620d270115a19836067c33e5a5c72`.

## Correctness And Ownership

- EDADB core: `27/27` tests pass with profiling enabled.
- New tests cover value and pointer leaf vectors, multiple immediate parents, two root rows with a
  reused stack address, query/load/hit counts and explicit vector indices.
- A batch whose later parent group fails `fromShadow()` leaves the existing target graph unchanged;
  an object lifetime counter proves all staged pointer elements are released.
- The targeted SELECT test passes under ASan/UBSan. LeakSanitizer itself cannot run in the controlled
  environment because it is incompatible with ptrace, so ownership is asserted directly in-test.
- Complete adapter regression passes with eight fixture processes, including routed and optional
  Net branches. Its log assertions require the Debug functional build; Release correctness is
  independently covered by the strict performance and RSS samples.

## Result And Remaining Work

P2 satisfies its query-count, correctness, ownership, memory and write-regression gates. It is a
deliberately narrow Wire-scoped optimization, not the final generic graph loader. The long-term
N+1 solution remains the bounded root-window design in `TODO.md`: query each descendant relation
once for a root-key window, group by the complete FK suffix, and avoid a sibling-multiplying JOIN.
