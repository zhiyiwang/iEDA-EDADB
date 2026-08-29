# P2 N+1 Child-Query Reassessment — 2026-08-29

## Decision

Proceed with a narrowly scoped P2 prototype for routed Net children.

The decision is based on fresh profiling-disabled Release timings plus profiling-enabled structural
evidence. Profiling adds too much overhead to supply exact absolute percentages, so no profiled time
is subtracted from the production result and no profiled percentage is presented as an unperturbed
runtime fraction.

## Baseline

- iEDA: `edadb-performance-optimization @ 29808d61a`
- EDADB: `performance/optimization @ 7066b01`
- Both binaries: Release `-O3`, SQL trace disabled.
- Profiling-OFF binary: `bin-release/iEDA`
- Profiling-ON binary: `bin-profile/iEDA`
- Samples: one warm-up plus five measured cold and warm samples, run sequentially with five seconds
  between samples.
- Correctness gate: every sample must pass strict native-vs-EDADB DEF diff.

Commands:

```bash
PROFILE_WARMUPS=1 PROFILE_RUNS=5 PROFILE_SETTLE_SECONDS=5 \
  PROFILE_OUT_DIR=/tmp/iedadb_p2_reassess_filler \
  bash scripts/edadb/performance/run_profile.sh \
    scripts/design/sky130_gcd/result/iPL_filler_result.def

python3 scripts/edadb/performance/make_routed_stress_fixture.py \
  scripts/design/sky130_gcd/result/iRT_result.def \
  /tmp/p2_reassess_routed_1000.def 1000

PROFILE_WARMUPS=1 PROFILE_RUNS=5 PROFILE_SETTLE_SECONDS=5 \
  PROFILE_OUT_DIR=/tmp/iedadb_p2_reassess_routed \
  bash scripts/edadb/performance/run_profile.sh \
    /tmp/p2_reassess_routed_1000.def
```

## Profiling-OFF Absolute Performance

All values below are five-run medians from the uninstrumented Release binary.

| Dataset | Cache | Native read | EDADB read | Read ratio | Native write | EDADB write | Write ratio |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| iPL filler | cold | `72.896 ms` | `248.939 ms` | `3.41x` | `8.886 ms` | `381.085 ms` | `42.89x` |
| iPL filler | warm | `71.176 ms` | `171.322 ms` | `2.41x` | `8.769 ms` | `395.792 ms` | `45.14x` |
| routed 1000 | cold | `560.250 ms` | `2,304.850 ms` | `4.11x` | `77.396 ms` | `2,873.002 ms` | `37.12x` |
| routed 1000 | warm | `527.761 ms` | `1,966.411 ms` | `3.73x` | `156.173 ms` | `2,957.790 ms` | `18.94x` |

The P2 total-time gate is met: routed warm EDADB read remains greater than `2x` native DEF read.

## Scaling Evidence

| Metric | iPL filler | Routed 1000 | Scale |
| --- | ---: | ---: | ---: |
| Native warm read | `71.176 ms` | `527.761 ms` | `7.41x` |
| EDADB warm read | `171.322 ms` | `1,966.411 ms` | `11.48x` |
| EDADB minus native warm read | `100.146 ms` | `1,438.650 ms` | `14.37x` |
| Net child-FK queries | `29,699` | `447,699` | `15.07x` |
| Net segment rows | `9,090` | `146,997` | `16.17x` |
| Net point rows | `14,386` | `250,256` | `17.40x` |
| Net via-reference rows | `3,751` | `43,716` | `11.65x` |

The routed fixture contains 1,677 root Nets. Current recursive restoration issues three child queries
per Net, one segment query per Wire and three child queries per Segment:

```text
3 * 1,677 Net queries
+ 1 * 1,677 Wire queries
+ 3 * 146,997 Segment queries
= 447,699 child-FK queries
```

P1 makes each query indexed, but does not remove the per-parent query count. The EDADB-specific warm
read excess grows at nearly the same rate as the child-query count. This is evidence that P2 should
address query granularity rather than add more indexes.

## Profiling-ON Structural Evidence

The routed warm profiling run reports:

- command median: `2,720.254 ms`;
- Net phase: `2,369.287 ms`;
- Net child-FK queries: `447,699`;
- SQLite fetch-step calls: `897,761`, totaling `1,143.854 ms`;
- SQLite fetch-column calls: `4,295,399`, totaling `341.818 ms`;
- SQLite bind calls: `1,333,640`, totaling `144.341 ms`;
- SQLite reset calls: `897,092`, totaling `99.476 ms`;
- Shadow `fromShadow()` calls: `252,609`, totaling only `10.563 ms`.

The instrumented SQLite fetch-step time is `42.05%` of the instrumented command, above the proposed
30% decision gate. However, profiling increases routed warm EDADB read by `38.34%` and write by
`14.80%`, both above the 5% precision limit. Therefore these times establish bottleneck ordering,
not production absolute proportions. Stable call counts and profiling-OFF totals are the primary P2
evidence.

## Current Memory State

Fresh five-run routed-stress RSS medians confirm that P5 remains effective:

| Mode | Median max RSS | Median process elapsed |
| --- | ---: | ---: |
| Native | `273,328 KiB` | `10.92 s` |
| EDADB write | `273,556 KiB` | `13.76 s` |
| EDADB read | `273,744 KiB` | `12.42 s` |

The current EDADB-write excess is only `228 KiB`, and the read excess is only `416 KiB`; both are
within process measurement noise. P2 should not reintroduce a complete in-memory copy of all routed
child rows.

## Correctness And Reproducibility

- Standard and routed profiling-OFF/ON runs performed 48 independent strict DEF comparisons. Six
  additional routed read-RSS samples were compared with the native canonical DEF; all 54 passed.
- Every routed database is `42,295,296` bytes.
- All five warm routed databases have the same SHA-256:
  `dd96004d179cf565296fe1a012b68329eac620d270115a19836067c33e5a5c72`.
- P1 indexes remain present, so this result is not caused by a regression to full child-table scans.

Raw results:

- `/tmp/iedadb_p2_reassess_filler/baseline/result.tsv`
- `/tmp/iedadb_p2_reassess_filler/profiled/result.tsv`
- `/tmp/iedadb_p2_reassess_routed/baseline/result.tsv`
- `/tmp/iedadb_p2_reassess_routed/profiled/result.tsv`
- `/tmp/iedadb_p5_rss_stress_streaming/result.tsv`
- `/tmp/iedadb_p2_read_rss_stress/result.tsv`

## Proposed P2 Scope

Implement and measure a Net-only prototype before changing the generic traversal contract:

1. Batch-read Net Point and ViaRef child tables once per root Net read operation rather than once per
   Segment.
2. Group rows by the complete parent-FK tuple and restore each nested vector by its persisted vector
   index.
3. Stream/group rows without retaining a second complete copy of the routed graph.
4. Preserve current P1 indexes, P4 transaction semantics, ownership and failure atomicity.
5. Do not change Instance, Pin, SpecialNet or generic traversal until the prototype proves the benefit.

Acceptance requires materially fewer child queries, strict DEF and database-content equivalence, all
EDADB core tests, complete adapter regression, no peak-RSS regression and no write regression above
5%. Compare profiling-OFF medians for performance; use profiling-ON only for call-count confirmation.
