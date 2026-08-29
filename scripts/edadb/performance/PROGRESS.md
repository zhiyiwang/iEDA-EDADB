# EDADB Performance Optimization Progress

Last updated: 2026-08-29

This file is the single current-state summary for EDADB performance work. Detailed methodology and
reproduction commands remain in `README.md`; dated reports retain the raw evidence for each phase.

## Current Baseline

- iEDA branch: `edadb-performance-optimization`
- iEDA commit: `29808d61a` (`perf: stream large root shadow writes`)
- EDADB branch: `performance/optimization`
- EDADB commit: `7066b01` (`docs: explain child FK index rationale`)
- Build mode: Release (`-O3`); profiling is reusable and disabled by default.
- Standard input: `scripts/design/sky130_gcd/result/iPL_filler_result.def`
- Large routed stress input: generated from the real Sky130 GCD `iRT_result.def` by
  `make_routed_stress_fixture.py`.

## Completed Work

| Phase | Problem | Implemented change | Measured result | Status |
| --- | --- | --- | --- | --- |
| P1 | Nested child reads repeatedly scanned child tables | Create a generic non-unique parent-FK index only when the table has no PK and no existing PK/index left-prefix covers the complete parent FK | Warm read `19,164.606 ms -> 169.480 ms` (`113.08x`); query plans changed from `SCAN` to `SEARCH` | Complete, committed and pushed |
| P2 | One child query per parent forms N+1 access | No code change yet; fresh 1,000-net routed profiling measures `447,699` Net child queries and a `3.73x` EDADB/native warm-read ratio | Decision gate passed; implement a Net-only Point/ViaRef batch-read prototype | Recommended, awaiting implementation |
| P3 | Schema creation paid many independent transaction/exec costs | Create all 15 root table trees inside one outer schema transaction | Schema init `1,328.322 ms -> 93.294 ms`; warm write `2,224.625 ms -> 1,131.619 ms` | Complete, committed and pushed |
| P4 | Root families committed independently | Write all 15 root families inside one atomic design transaction | Warm write `1,131.619 ms -> 378.269 ms`; write `sqlite_exec` calls `77 -> 59` | Complete, committed and pushed |
| P5 | Complete vectors of large root Shadows amplified temporary memory | Reuse one insert operator and convert/insert/destroy one Instance, Pin, SpecialNet or Net Shadow at a time | Routed stress EDADB excess RSS reduced by `28.15 MiB`; write time changed `+0.59%`; DB bytes stayed identical | Complete, committed and pushed |

## Current Standard Performance

The following values are medians of five sequential warm Release runs from
`/tmp/iedadb_p2_reassess_filler/baseline/result.tsv`.

| Operation | Median | Relative cost |
| --- | ---: | ---: |
| Native DEF read | `71.176 ms` | `1.00x` |
| EDADB read | `171.322 ms` | `2.41x` native DEF read |
| Native DEF write | `8.769 ms` | `1.00x` |
| EDADB write | `395.792 ms` | `45.14x` native DEF write |

The current standard result must not be interpreted as a P5 write-time regression. The direct P4/P5
stress comparison changed EDADB-write elapsed time only from `13.63 s` to `13.71 s` (`+0.59%`). The
standard filler run has measurable SQLite commit variability; P5's demonstrated result is removal of
temporary root-Shadow memory amplification.

## P5 Routed Stress Evidence

The stress fixture is 6.1 MiB and contains 1,000 copied real routed-net bodies, adding approximately
139,000 segment-level route records.

| Implementation | Native max RSS | EDADB-write max RSS | EDADB excess |
| --- | ---: | ---: | ---: |
| P4 full Shadow vectors | `273,528 KiB` | `302,184 KiB` | `28,656 KiB` |
| P5 one-root streaming | `273,608 KiB` | `273,356 KiB` | `-252 KiB` (noise) |

The pre/post SQLite database is byte-identical: `42,295,296` bytes, SHA-256
`dd96004d179cf565296fe1a012b68329eac620d270115a19836067c33e5a5c72`.

## Validation State

- All 26 EDADB core tests pass.
- The complete adapter regression passes with eight independent fixture jobs.
- The 1,000-net routed stress fixture passes strict native-vs-EDADB DEF diff.
- P3 schema rollback and P4 whole-design rollback are covered by transaction tests.
- Release, Debug and profiling-enabled Release builds pass.
- Before this documentation update, only generated build directories were untracked in the parent repository.

## Next Plan

### P2: Net Child Batch Read

The large routed decision gate passed. Profiling-OFF warm EDADB read is `1,966.411 ms`, `3.73x`
native DEF read. Profiling counts `447,699` Net child-FK queries; instrumented SQLite fetch-step time
is `42.05%` of the instrumented command. Because instrumentation overhead is `38.34%`, the percentage
is used only for bottleneck ordering, while the OFF total and stable call counts drive the decision.

1. Prototype batching only for routed Net Point/ViaRef children.
2. Read each child table once, group by complete parent FK and restore vector index exactly.
3. Avoid a second complete routed-graph copy so P5 memory behavior remains intact.
4. Keep P1 indexes and all existing transaction/failure semantics.
5. Accept only after query count falls materially, strict DEF/DB checks pass, all core and adapter tests
   pass, RSS does not regress and write time stays within 5%.

### Later Work

- P6 adapter/object-rebuild residual remains deferred. Its old below-3% ratio used the obsolete
  19-second scan baseline, while current profiling overhead is too high for a new absolute residual.
- Revisit P6 after P2 removes the known child-query bottleneck and profiling perturbation is measured
  again.

## Detailed Evidence

- Methodology and commands: `scripts/edadb/performance/README.md`
- Optimization plan and acceptance gates: `scripts/edadb/performance/TODO.md`
- Profiling baseline: `scripts/edadb/performance/sky130_gcd_ipl_filler_profile_20260828.md`
- P1: `scripts/edadb/performance/sky130_gcd_ipl_filler_p1_child_fk_index_20260829.md`
- P3: `scripts/edadb/performance/sky130_gcd_ipl_filler_p3_schema_transaction_20260829.md`
- P4: `scripts/edadb/performance/sky130_gcd_ipl_filler_p4_design_transaction_20260829.md`
- P5: `scripts/edadb/performance/sky130_gcd_routed_p5_stream_shadow_20260829.md`
- P2 reassessment: `scripts/edadb/performance/sky130_gcd_p2_reassessment_20260829.md`
