# iEDA + EDADB Full Validation — 2026-08-30

## Compared Versions

| Label | iEDA | EDADB core | Meaning |
| --- | --- | --- | --- |
| Pre-optimization EDADB | `62504bd9d` | `be6bbdd` | Profiling baseline before P1-P5/P2 |
| Current EDADB | `9c376467b` | `ad0f820` | P1-P5 plus Wire-scoped P2 leaf batching |
| Native iEDA | Native path inside each Release binary | No EDADB timed operation | `def_init` / `def_save` control |

Both binaries used GCC 10, `Release`, `-O3`, LTO and `EDADB_ENABLE_PROFILING=0`.
The benchmark used the same Sky130 GCD input, Tcl runner and five-second settle interval.

## Functional Result

- EDADB core: `27/27` tests passed.
- Adapter: all `15` roundtrip cases passed.
- DEF normalizer tests passed.
- Generated-via iRT obstacle geometry matched exactly.
- Native/EDADB iRT `env_map.json` input gate matched exactly.
- All `36` performance roundtrips passed exact native-vs-EDADB DEF diff.

Commands:

```bash
ctest --test-dir /tmp/edadb-p2-build --output-on-failure --parallel 20

IEDA_BIN="$PWD/bin/iEDA" EDADB_TEST_JOBS=8 \
  OUT_DIR=/tmp/iedadb_full_validation/current_adapter \
  bash src/database/edadb/test/run_idb_roundtrip_regression.sh

IEDA_BIN="$PWD/bin/iEDA" \
  OUT_ROOT=/tmp/iedadb_full_validation/generated_via \
  bash src/database/edadb/test/stage_validation/run_generated_via_fixture.sh

IEDA_BIN="$PWD/bin/iEDA" IRT_INPUT_GATE_ONLY=1 STAGE_RUN_JOBS=2 \
  OUT_ROOT=/tmp/iedadb_full_validation/irt_gate \
  bash src/database/edadb/test/stage_validation/run_stage_validation.sh irt
```

## Standard Filler Performance

Values are medians of five sequential profiling-disabled Release runs. Warm results are the primary
comparison because cold `posix_fadvise` eviction is advisory and showed more variance.

| Version / warm path | Native | EDADB | EDADB/native |
| --- | ---: | ---: | ---: |
| Pre-optimization read | `73.638 ms` | `18,890.395 ms` | `256.53x` |
| Current read | `71.023 ms` | `152.938 ms` | `2.15x` |
| Pre-optimization write | `8.647 ms` | `2,105.656 ms` | `243.51x` |
| Current write | `8.710 ms` | `410.681 ms` | `47.15x` |

Current versus pre-optimization EDADB:

- Read: `123.52x` faster and `99.19%` less time.
- Write: `5.13x` faster and `80.50%` less time.

Cold medians:

| Version / cold path | Native | EDADB | EDADB/native |
| --- | ---: | ---: | ---: |
| Pre-optimization read | `112.184 ms` | `18,982.414 ms` | `169.21x` |
| Current read | `72.438 ms` | `264.322 ms` | `3.65x` |
| Pre-optimization write | `8.683 ms` | `2,116.884 ms` | `243.80x` |
| Current write | `8.611 ms` | `431.101 ms` | `50.06x` |

Benchmark command, changing only `IEDA_BIN` and `OUT_DIR` between versions:

```bash
PERF_WARMUPS=1 PERF_RUNS=5 PERF_SETTLE_SECONDS=5 \
  IEDA_BIN=/path/to/release/iEDA \
  OUT_DIR=/tmp/iedadb_full_validation/perf_version \
  bash scripts/edadb/performance/run.sh \
    scripts/design/sky130_gcd/result/iPL_filler_result.def
```

## Routed Stress Performance

The current version was also tested on the 1,000 copied routed-Net fixture: `1,677` root Nets and
about `146,997` Segments.

| Warm operation | Native | Current EDADB | EDADB/native |
| --- | ---: | ---: | ---: |
| Read | `526.911 ms` | `1,486.138 ms` | `2.82x` |
| Write | `126.608 ms` | `2,776.661 ms` | `21.93x` |

The routed warm read reproduces the earlier P2 result (`1,473.071 ms`) within `0.89%`. P2 reduced
Net child-FK queries from `447,699` to `11,739` (`-97.38%`).

## Analysis

- P1 provides the dominant read gain by replacing repeated Point/ViaRef full-table scans with
  parent-FK index searches.
- P3/P4 provide the dominant write gain by replacing per-table/per-family transactions with one
  schema transaction and one atomic design transaction.
- P5 removes temporary root-Shadow memory amplification; it is primarily a memory optimization.
- P2 loads Point/ViaRef/VirtualPoint once per Wire scope instead of once per Segment. Its routed read
  gain is about `25%` over the immediate pre-P2 implementation.
- Current EDADB read remains `2.15x-2.82x` native because SQLite row decoding, recursive graph
  traversal, reference lookup/object reconstruction and remaining relation queries have no
  equivalent in a direct text stream.
- Current EDADB write remains `21.93x-47.15x` native because it creates/indexes a relational schema
  and serializes an object graph, while native DEF write streams text. The filler's `8.7 ms` native
  baseline makes the relative ratio especially large even though EDADB write is now `0.411 s`.

Raw TSV and logs were kept under `/tmp/iedadb_full_validation/` for this run. This report preserves
the commands and accepted statistics; generated runtime artifacts are not committed.
