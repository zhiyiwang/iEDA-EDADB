# iEDA-EDADB Adapter Testing And Validation

Last updated: 2026-08-17
Status: object roundtrip and canonical demo pass; stage acceptance passes its retained gates; full iRT remains `REVIEW`

本文是 iEDA+EDADB adapter 测试的唯一权威说明，统一记录测试模型、数据集、命令、断言、结果、缺陷分类、提交和运行产物。`test/` 下的 README 只索引本文与可执行脚本，不再复制测试规则或结果。

## Global Test Rules

- Regression cases are process-isolated and concurrent by default. Select jobs from logical CPUs, available RAM and measured per-case load; use serial execution only for one selected case or failure diagnosis.
- Each case owns its output directory, SQLite DB and logs. Parallel execution must retain SQL checks, raw/normalized DEF diff and order-perturbation assertions.
- Fixtures must be legal under the current LEF/DEF grammar. Record grammar-unreachable source branches as unreachable instead of fabricating coverage.
- Cover each reachable original `if/else`, optional field and nested loop with an input that distinguishes the branch; the default Sky130 sample alone is insufficient.
- Validate three layers: direct `DefRead/DefWrite` baseline, EDADB source/identity/order fields, and the DEF rebuilt from EDADB. Assert that derived/cache columns are absent when they should be recomputed.
- Reparse EDADB-generated DEF through the original reader/writer for branch fixtures; textual equality alone does not prove that stored source fields or rebuilt references are correct.
- Perturb unordered database fetches for order-sensitive root/nested vectors, then verify `_order_sd` / `_vec_idx`; never treat SQLite's current physical return order as a guarantee.
- Propagate null-child, lookup and `toShadow/fromShadow` failures. Never append or insert a partially rebuilt object.
- Verify parser-only fields through SQL and active read-state assertions when the native writer cannot emit them.
- Before adapter acceptance, run affected focused cases, then the complete object regression. Stage validation is additionally required only when the change can affect point-tool-consumed state.

## Test Portfolio

| Layer | Purpose | Executable entry | Current conclusion |
|---|---|---|---|
| Build | Verify adapter/schema/shadow compatibility with the selected EDADB core. | `build.sh` | `demo/20260814` clean build passed with `-j40`. |
| Canonical demo | Verify the supported mixed path on the maintained Sky130 GCD design. | `scripts/edadb/demo/demo.sh` | Raw input/output DEF diff passed. |
| Object regression | Verify schema, source fields, computed-state rebuild, identity/order and legal DEF roundtrip. | `test/run_idb_roundtrip_regression.sh` | All 15 cases passed with eight concurrent jobs. |
| Minimal causal fixtures | Prove one defect or branch independently from full-flow noise. | `test/stage_validation/run_*_fixture.sh` | Generated-via strict oracle passed; native alias fixture records the original iEDA boundary. |
| Point-tool stages | Verify native and EDADB-restored iDB before and after iPL/iCTS/iTO/iRT transitions. | `test/stage_validation/run_stage_validation.sh` | Retained pre-tool and stage gates passed; full iRT remains `REVIEW`. |

## Current Demo Object Regression

### Baseline And Scope

| Item | Value |
|---|---|
| Branch | `demo/20260814` |
| Demo commit | `4c6aa63c4` |
| Codebase commit | `5fcb67bc7` |
| EDADB core gitlink | `27ecbd2a6dd24d43f05fc7fdda0cf68ece732557` |
| Enabled EDADB roots | Design, Die, Row, TrackGrid, GCellGrid, Via, Instance, Pin, Blockage, Region, Slot, Group, Fill, SpecialNet |
| DEF fallback | regular `NETS` and `SPECIALNETS USE SIGNAL/CLOCK` |
| Host scheduling | 40 logical CPUs, approximately 125 GiB RAM; eight process-isolated regression jobs |
| Run output | `/tmp/iedadb_demo_20260814_roundtrip` |

The demo deliberately has no regular-Net EDADB schema, `writeIdbNet()` or `readIdbNet()`. POWER/GROUND special nets are restored from EDADB; SIGNAL/CLOCK records use the original regular-net parser. This split is asserted in SQL and logs, not inferred only from the final DEF.

### Commands

Run from the repository root:

```bash
bash build.sh -d -n -y
rm -rf bin lib build.out
bash build.sh -j40

cd bin
bash /home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/demo/demo.sh \
  2>&1 | tee run.out

cd ..
OUT_DIR=/tmp/iedadb_demo_20260814_roundtrip \
EDADB_TEST_JOBS=auto \
bash src/database/edadb/test/run_idb_roundtrip_regression.sh
```

Automatic object-regression concurrency is bounded by logical CPUs and current `MemAvailable`. The current host reserves 16 GiB and budgets 8 GiB per active case, producing a cap of eight. Override `EDADB_TEST_JOBS`, `EDADB_TEST_PROCESS_MEMORY_GIB` or `EDADB_TEST_MEMORY_RESERVE_GIB` only after measuring the selected dataset.

### Per-Case Flow And Oracles

Each case owns a separate directory, SQLite DB, iEDA processes and logs, then performs:

1. direct iDB `DEF -> DEF` to establish the current branch's native baseline;
2. `DEF -> EDADB`;
3. a fresh `EDADB -> iDB -> DEF` process;
4. raw DEF diff, followed only when needed by D-level root-record normalization;
5. SQLite schema/value/child-row assertions and selected read/write log assertions;
6. physical table-order perturbation where order recovery is part of the contract;
7. original-parser reparse for generated DEFs used by branch fixtures.

Deep nested vectors are never normalized. A D-level root record may move only as one complete block, carrying all nested content with it.

### Cases And Coverage

| Case | Main coverage | Result |
|---|---|---|
| `default_ipl` | Sky130 GCD baseline, all enabled roots, schema/count/log split. | PASS, raw DEF equal. |
| `design_fields` | VERSION, DESIGN, UNITS and BUSBITCHARS non-default values. | PASS, raw DEF equal. |
| `design_fallback` | canonical Design fallback values and absence of non-DEF Units columns. | PASS, raw DEF equal. |
| `die_polygon` | polygon points, child-vector index and unordered physical fetch. | PASS, raw DEF equal. |
| `grid_branches` | Track nested layer names and non-empty GCellGrid branches. | PASS, normalized D-root equivalent. |
| `via_branches` | generated/fixed Via, offsets, layer grouping and ordered Rect children. | PASS, normalized D-root equivalent. |
| `instance_branches` | weight, region, halo, route halo and reference lookup. | PASS, raw DEF equal. |
| `pin_derived` | no-PORT derived placement/orient/geometry rebuild. | PASS, raw DEF equal. |
| `pin_writer` | writer PORT canonicalization and special-pin branch. | PASS, raw DEF equal. |
| `pin_branches` | multiple ports, placement statuses, duplicate layer names and ordered shapes. | PASS, raw DEF equal and reparsed. |
| `aux_optional` | Blockage, Region, Slot, Group, Fill and SpecialNet optional/parser-only fields. | PASS, normalized D-root equivalent. |
| `group_branches` | regex expansion, duplicate elimination and ordered member vector. | PASS, normalized D-root equivalent and reparsed. |
| `special_net_branches` | explicit refs, STYLE, SHIELD, point/rect/via segment dispatch and nested order. | PASS, normalized D-root equivalent. |
| `net_branches` | complex regular routes and `SPECIALNETS USE SIGNAL` through native fallback. | PASS, raw DEF equal; `iNetSD` absent. |
| `routed_irt` | non-empty routed NETS through native fallback with EDADB-restored supporting roots. | PASS, raw DEF equal; `iNetSD` absent. |

### Current Result

- Clean build completed all `1142/1142` targets.
- Canonical demo restored 2 SpecialNets and 639 SpecialNet segments from EDADB, restored 675 regular Nets from DEF, and reported `Input def and output def are the same.`
- All 15 object-regression cases passed with eight concurrent workers.
- Exact raw DEF equality passed where the native writer preserves root order.
- Via, Grid, Group, SpecialNet and optional fixtures differed only by allowed D-level root order; normalized diff passed while nested-order assertions remained strict.
- SQL confirmed `iSpecNetSD` exists and `iNetSD` does not; logs confirmed SpecialNet EDADB write/read and absence of Net EDADB write/read.

## Point-Tool Stage Validation Record

The following numbered sections preserve the independent 2026-08-11 point-tool campaign. They use a different recorded milestone from the current demo and must not be relabeled as `demo/20260814` evidence.

## 1. Scope And Baseline

This report covers only the stage-validation work beginning at commit `621ae3df6`. It does not
restate the earlier 15-class adapter migration history.

Final milestone policy: original iEDA point-tool, Verilog-builder and iRT diagnostic production
source is restored to the adapter milestone. Commits that temporarily changed native iEDA remain
diagnostic evidence in history, but their source changes are not present in the final tree. The
only retained production correction from this campaign is the generated-via adapter fix.

| Item | Recorded value |
|---|---|
| Branch | `edadb-idb-dev/sort-abc-no-sort-d` |
| Adapter milestone | `milestone/edadb-idb-15class-sort-abc-no-sort-d-20260810` -> `77fbe5c67` |
| Stage-validation milestone | `milestone/edadb-adapter-stage-validation-native-ieda-20260811` |
| Historical diagnostic iEDA head | `ef07f23df8b6ef1b732bac01a79586f23220deeb` |
| EDADB core | `30771329bd5f572fdc871c7ddc865d81095bffce` |
| Host | `cherry13`, 40 logical CPUs, approximately 125 GiB RAM, no swap pressure in the six-control run |
| Main datasets | Sky130 GCD, IHP130 AES, IHP130 PicoRV32A |
| Minimal fixtures | Sky130 generated via; IHP130 Verilog IO alias |

Recorded input/configuration hashes:

| File | SHA-256 |
|---|---|
| `scripts/design/ihp130_gcd/iEDA_config/db_default_config.json` | `9f9ca880d66c4c04f02e76452d9e3b5be4a4dae2b557f5507ab2a8e8928c58cf` |
| `scripts/design/ihp130_gcd/iEDA_config/flow_config.json` | `80cd75d078ded2fb310c218a0d6656d9165e6dc4d175575ccda5115d57725016` |
| `src/database/edadb/test/stage_validation/config/ihp130_cts_no_skew_tree.json` | `98d12003c21b2618af932e4593956b23d366730bdbf116fd2a6a4fa92bdb2f03` |
| `scripts/design/sky130_gcd/iEDA_config/db_default_config.json` | `81edcf78210297fb199fb9a7a5f86e901f0bb7334b9e964fe70e70ace744484f` |
| `scripts/design/sky130_gcd/iEDA_config/flow_config.json` | `80cd75d078ded2fb310c218a0d6656d9165e6dc4d175575ccda5115d57725016` |
| `src/database/edadb/test/stage_validation/fixtures/sky130_generated_via/generated_via.def` | `25027986f50b54649198330a154c43544adf3b6c03d712980c9894a15bc6dbdd` |
| `src/database/edadb/test/stage_validation/fixtures/verilog/io_port_alias.v` | `b4458380f7dc5fcd9747a71584e26541a0f9b3a2496a22a57517b4cdd4352813` |

The historical PicoRV32A iRT run was made at `ef07f23df`, and its manifest records
`git_dirty=true` because process-concurrency and statistical-reporting code was still being
refined. The finalized harness is included in the stage-validation milestone. Historical run
artifacts remain evidence for that exact dirty-state hash; they are not relabeled as milestone
artifacts.

## 2. First-Principles Validation Model

### 2.1 The State Transition Being Tested

For one physical-design state `S` and one point tool `T`, the two paths are:

```text
Native: LEF + DEF(S) -> iDB_native(S) -> T -> result_native
EDADB:  LEF + DEF(S) -> iDB -> EDADB
        fresh process: LEF + EDADB -> iDB_edadb(S) -> T -> result_edadb
```

DEF text equality alone is insufficient. A point tool also consumes pointers, backlinks,
derived geometry, IDs, ownership, connectivity, and nested-vector state that may not be visible
in emitted DEF. Therefore validation has two independent obligations:

1. **Pre-tool adapter gate:** prove `iDB_native(S)` and `iDB_edadb(S)` are semantically equivalent.
2. **Post-tool transition:** prove the same tool consumes and mutates both states equivalently,
   or classify the difference against the tool's native repeatability.

The gate is implemented in
`src/database/edadb/test/stage_validation/run_stage_validation.sh:210`. Native and EDADB loads are
separate processes in `src/database/edadb/test/stage_validation/tcl/run_stage.tcl:58` and
`src/database/edadb/test/stage_validation/tcl/run_stage.tcl:61`.

### 2.2 Why Each Stage Is Needed

| Test | First-principles purpose | Main state exercised |
|---|---|---|
| Pre-tool snapshot | Isolate restoration correctness before a tool can amplify a difference. | Canonical DEF and stable `report_db` state. |
| iPL | Test mutable placement state and order-sensitive numerical consumers. | Rows, regions, instances, pins, nets, coordinates, status, orient. |
| iCTS | Test topology creation and reconnect operations. | Clock pins/nets, inserted clock buffers, placement state. |
| iTO DRV/Hold | Test timing-graph construction and topology mutation. | Instance/net/pin ownership, reconnects, timing summaries. |
| Incremental legalization | Test whether modified instances remain legal and writable. | Row constraints, coordinates, status, orient. |
| iRT input gate | Inspect the internal consumer state that DEF cannot expose. | Layers, axes, via masters, derived geometry, obstacles, ordered nets/pins/shapes. |
| Full iRT | Stress the largest and deepest mutable object graph. | Wires, segments, vias, patches, DRC and routed geometry. |
| Generated-via fixture | Establish a causal oracle for derived via geometry. | Generated cut arrays and routing enclosures. |
| Verilog alias fixture | Establish a causal oracle for one-pin/one-root-net ownership. | IO direction, net membership and back-references. |

Real datasets prove scale and integration. Minimal fixtures prove causality: they remove unrelated
objects until the expected result can be derived directly from the input.

## 3. Dataset Strategy

| Layer | Why it is used | Coverage gained |
|---|---|---|
| Sky130 GCD | Small existing iEDA flow with canonical intermediate DEFs; fastest integration baseline. | All isolated stages and generated-via geometry. |
| IHP130 AES | Approximately 15k instances; larger placement, timing and routing state. | Exposed deterministic iPL pointer-order divergence and native CTS boundary. |
| IHP130 PicoRV32A | Approximately 18k instances and 20k nets; larger connectivity and routing load. | Exposed IO alias ownership defect and supplied the final six-control iRT study. |
| Generated-via fixture | Four real Sky130 generated-via definitions reduced from the GCD flow. | Exact `27`-obstacle oracle and historical `27 -> 65` failure signature. |
| Verilog IO alias fixture | Small legal IHP130-compatible netlist with input/output aliases. | Assignment direction and exactly one root-net membership per IO pin. |

The execution order was `sky130_gcd -> ihp130_aes -> ihp130_picorv32a`. A larger design was not
used to hide a failure found by a smaller one.

## 4. Harness Execution And Oracles

### 4.1 Gate Order

`run_stage_validation.sh` executes these boundaries:

1. Native DEF load and pre-tool snapshot.
2. DEF load followed by EDADB write.
3. Fresh EDADB read and pre-tool snapshot.
4. Pre-tool canonical DEF and DB-report comparison.
5. For iRT, original `env_map.json` comparison before routing; historical deep diagnostics are
   retained only as evidence.
6. Three native controls to establish native repeatability.
7. One EDADB control for a stable native tool, or three controls after variability/mismatch.
8. Post-tool DEF, Verilog, DB report, feature JSON and summary comparison.

The pre-tool boundary begins at
`src/database/edadb/test/stage_validation/run_stage_validation.sh:221`. The iRT internal gate
begins at `src/database/edadb/test/stage_validation/run_stage_validation.sh:233`.

### 4.2 Compared Artifacts

The generic comparator at
`src/database/edadb/test/stage_validation/compare_stage_runs.py:65` checks:

- normalized pre-tool and post-tool DEF;
- timestamp-normalized Verilog;
- runtime/memory-normalized DB reports;
- stable point-tool feature JSON and summary JSON;
- raw artifacts remain in the run directory for audit.

The current iRT-specific comparator uses original iEDA's `env_map.json` to compare die, obstacle
and pin-shape records. A temporary diagnostic version separated a full `semantic_database` from
allocator-dependent `pointer_order_views`; commit `1e00b5940` and the retained artifacts record
that experiment, but the required `DataManager` instrumentation is not in the final milestone.

### 4.3 Resource-Aware Concurrency

The automatic scheduler is defined at
`src/database/edadb/test/stage_validation/run_stage_validation.sh:60`. It reserves host memory,
estimates memory per iEDA process and limits process-level concurrency. The final Pico iRT batch
used three native and three EDADB processes, 12 iRT threads per process, and independent output
directories. The machine reached load above 60 while using about 32 GiB and no swap.

This is a throughput optimization, not a semantic relaxation: native and EDADB processes use the
same input, configuration and thread count. The static OpenMP DR-box loop at
`src/operation/iRT/source/module/detailed_router/DetailedRouter.cpp:369` explains the observed
tail imbalance; changing its schedule is a separate performance experiment.

## 5. Reproduction Commands

Run all commands from the repository root after building `bin/iEDA`:

```bash
bash build.sh -j40
```

### 5.1 Sky130 GCD

```bash
DATASET=sky130_gcd \
OUT_ROOT=/tmp/iedadb_stage_validation_sky130 \
bash src/database/edadb/test/stage_validation/run_stage_validation.sh \
  ipl icts ito_drv ito_hold ipl_lg

DATASET=sky130_gcd \
IRT_INPUT_GATE_ONLY=1 \
OUT_ROOT=/tmp/iedadb_stage_validation_sky130 \
bash src/database/edadb/test/stage_validation/run_stage_validation.sh irt
```

### 5.2 IHP130 AES

Generate isolated stage inputs outside the repository:

```bash
DATASET=ihp130_aes \
PREPARE_THROUGH=ipl_lg \
CTS_CONFIG_FILE=$PWD/src/database/edadb/test/stage_validation/config/ihp130_cts_no_skew_tree.json \
OUT_ROOT=/tmp/iedadb_stage_inputs \
bash src/database/edadb/test/stage_validation/prepare_ihp130_stage_inputs.sh
```

Run isolated comparisons:

```bash
DATASET=ihp130_aes \
DATASET_RESULT_DIR=/tmp/iedadb_stage_inputs/ihp130_aes/result \
CTS_CONFIG_FILE=$PWD/src/database/edadb/test/stage_validation/config/ihp130_cts_no_skew_tree.json \
OUT_ROOT=/tmp/iedadb_stage_validation_aes \
bash src/database/edadb/test/stage_validation/run_stage_validation.sh
```

### 5.3 IHP130 PicoRV32A

Input preparation follows the AES command with `DATASET=ihp130_picorv32a`. The exact completed
six-control iRT command was:

```bash
DATASET=ihp130_picorv32a \
DATASET_RESULT_DIR=/tmp/iedadb_stage_inputs_pico_parallel_20260811/ihp130_picorv32a/result \
OUT_ROOT=/tmp/iedadb_stage_validation_pico_irt_6way_retry_20260811 \
NATIVE_RUNS=3 \
STAGE_RUN_JOBS=3 \
PARALLEL_FIRST_EDADB=1 \
EDADB_RUNS_UPFRONT=3 \
RT_THREAD_NUMBER=12 \
CTS_CONFIG_FILE=$PWD/src/database/edadb/test/stage_validation/config/ihp130_cts_no_skew_tree.json \
bash src/database/edadb/test/stage_validation/run_stage_validation.sh irt
```

Input SHA-256:
`77ca164086e2fddc030714edf91a02e83ba1751c6e3b5efc5121bf82790cd9fd`.

### 5.4 Minimal Fixtures

```bash
FIXTURE_RUN_JOBS=2 \
OUT_ROOT=/tmp/iedadb_generated_via_final_20260811 \
bash src/database/edadb/test/stage_validation/run_generated_via_fixture.sh

OUT_DIR=/tmp/iedadb_alias_final_20260811 \
EXPECT_KNOWN_NATIVE_DEFECT=1 \
bash src/database/edadb/test/stage_validation/test_verilog_alias_roundtrip.sh
```

Generated artifacts are intentionally stored under `/tmp`; they are evidence from this host, not
repository assets.

### 5.5 Final Milestone Acceptance Commands

The final unchanged-native-iEDA tree passed these checks:

```bash
cmake --build build -j40 --target iEDA

python3 -m unittest discover \
  -s src/database/edadb/test/stage_validation -p 'test_*.py'

OUT_DIR=/tmp/iedadb_adapter_milestone_regression_20260811 \
bash src/database/edadb/test/run_idb_roundtrip_regression.sh

FIXTURE_RUN_JOBS=2 \
OUT_ROOT=/tmp/iedadb_generated_via_native_milestone_20260811 \
bash src/database/edadb/test/stage_validation/run_generated_via_fixture.sh

EXPECT_KNOWN_NATIVE_DEFECT=1 \
OUT_DIR=/tmp/iedadb_alias_native_milestone_20260811 \
bash src/database/edadb/test/stage_validation/test_verilog_alias_roundtrip.sh

DATASET=sky130_gcd \
IRT_INPUT_GATE_ONLY=1 \
OUT_ROOT=/tmp/iedadb_sky130_env_gate_milestone_20260811 \
bash src/database/edadb/test/stage_validation/run_stage_validation.sh irt
```

Results:

- build passed;
- six stage-validation unit tests passed;
- all object-level EDADB iDB roundtrip cases passed;
- generated-via fixture passed the exact `27 == 27` geometry oracle;
- Verilog fixture reproduced the unchanged native failure signature exactly;
- Sky130 native/EDADB `env_map.json` input environments matched.

## 6. Historical Diagnostic Stage Results

These tables record the completed diagnostic campaign. iPL and Pico iTO rows that passed after
temporary native fixes are not current final-tree acceptance claims. Current known-native-defect
oracles are specified in [known-native-defects.md](stage-validation/known-native-defects.md).

### 6.1 Sky130 GCD

| Stage | Result | Evidence boundary |
|---|---|---|
| iPL | `PASS` | Three native controls stable; EDADB matches. |
| iCTS | `PASS` | Three native controls stable; EDADB matches. |
| iTO DRV | `PASS` with native-field exclusion | DEF, Verilog, DB report and stable metrics match. |
| iTO Hold | `PASS` | Three native controls stable; EDADB matches. |
| Incremental legalization | `PASS` | Three native controls stable; EDADB matches. |
| iRT semantic input | `PASS` after generated-via fix | Native and EDADB DataManager semantic input matches. |
| Full iRT | `REVIEW` | Native routed results are not point-deterministic. |

### 6.2 IHP130 AES

| Stage | Result | Evidence boundary |
|---|---|---|
| Input preparation | `PASS` | Explicit non-empty iFP/iNO/iPL and downstream DEFs. |
| iPL | `PASS` after native iPL fix | Three native controls and EDADB share one deterministic result. |
| iCTS | `PASS` with stable profile | Default bound-skew-tree path fails natively; no-skew-tree profile matches. |
| iTO DRV/Hold | `PASS` | Stable native controls; EDADB matches. |
| Incremental legalization | `PASS` | Stable native controls; EDADB matches. |
| iRT semantic input | `PASS` | Semantic snapshots exactly equal. |
| Full iRT | `REVIEW` | Both native and EDADB routing vary. |

### 6.3 IHP130 PicoRV32A

| Stage | Result | Evidence boundary |
|---|---|---|
| Input preparation | `PASS` | iFP through incremental legalization produced non-empty isolated outputs. |
| iPL | `PASS` | Pre-tool state matches; native controls stable; EDADB matches. |
| iCTS | `PASS` with stable profile | Three native controls stable; EDADB matches. |
| iTO DRV | `PASS` after Verilog alias fix | Fixture and real design both satisfy single-net IO ownership. |
| iTO Hold | `PASS` | Three native controls stable; EDADB matches. |
| Incremental legalization | `PASS` | Three native controls stable; EDADB matches. |
| iRT semantic input | `PASS` | Semantic input matches; only pointer iteration view differs. |
| Full iRT | `REVIEW` | Six runs complete, but neither group is deterministic. |

### 6.4 PicoRV32A Six-Control Evidence

Artifacts:
`/tmp/iedadb_stage_validation_pico_irt_6way_retry_20260811/ihp130_picorv32a/irt`.

- All six processes exited zero.
- Every run retained `18,157` instances, `411` IO pins, `19,920` nets, `2` PDNs and identical
  layer counts.
- Native and EDADB pre-tool DEFs were byte-identical.
- The iRT `semantic_database` was exactly equal; the pointer-ordered fixed-rectangle view had the
  same content in a different allocator-dependent order.
- Native and EDADB post-tool DEFs were both unstable, so pairwise exact DEF comparison is not a
  valid group-attribution oracle.

| Metric | Native samples | EDADB samples | Mean EDADB-native | Exact permutation `p` |
|---|---:|---:|---:|---:|
| Wire length | `1113899.225, 1113416.740, 1114406.910` | `1113530.325, 1113891.430, 1113522.970` | `-0.0233%` | `0.6` |
| Wire count | `303864, 302273, 302416` | `303254, 302670, 304341` | `+0.1884%` | `0.5` |
| Segment count | `510714, 508788, 508983` | `510252, 509338, 511328` | `+0.1592%` | `0.4` |
| Via count | `184908, 184412, 184561` | `184777, 184624, 184990` | `+0.0921%` | `0.4` |
| Patch count | `21942, 22103, 22006` | `22221, 22044, 21997` | `+0.3195%` | `0.6` |
| Final DRC violations | `7927, 6968, 7462` | `7976, 7168, 8019` | `+3.6051%` | `0.5` |
| iRT runtime, seconds | `9276, 8694, 8960` | `9114, 8933, 9020` | `+0.5087%` | `0.9` |

For three native and three EDADB samples, an exact two-sided permutation test has only 20
partitions and cannot produce `p < 0.1`. No metric proves a group difference. Metal4 wire count
has the largest unresolved separation (`+0.8442%`, `p=0.1`) and remains a targeted repeatability
question, not a demonstrated adapter defect.

## 7. Problem Classification

| Class | Problem | Status | Resolution / evidence | Commit |
|---|---|---|---|---|
| Confirmed adapter bug | Generated-via cut geometry was reconstructed repeatedly. | Fixed | Idempotent replacement reconstruction; strict fixture and full iRT input gate pass. | `e63ebd001` |
| Confirmed iEDA bug | iPL pointer-key map iteration changed order-sensitive vectors. | Documented; native patch not retained | Diagnostic patch proved the cause; final source is original. | `eaba42801` |
| Confirmed iEDA bug | Verilog IO alias could remain in two root nets; output assignment direction was reversed. | Documented; native patch not retained | Fixture preserves the exact native failure signature. | `ef07f23df` |
| Confirmed iEDA bug | iTO summary leaves `HPWL/STWL` uninitialized. | Comparator containment | Exclude only undefined fields from semantic comparison; retain raw JSON. | `621ae3df6` |
| Native tool/config boundary | Default IHP130 bound-skew-tree path fails before EDADB comparison. | Documented profile | Preserve failure; compare with existing no-skew-tree implementation. | `1e00b5940` |
| Unresolved `REVIEW` | iRT pointer-address containers change consumer order and legal routing result. | No partial fix | Strict semantic input gate plus native/EDADB variability evidence. | `df6cae124`, `1e00b5940` |
| EDADB core | No core defect was proven in this campaign. | No core change | Core's scalar/vector shadow lifecycle exposed a non-idempotent adapter implementation. | None |

## 8. Confirmed Bug Cases And Code Changes

### 8.1 Adapter: Generated-Via Reconstruction

**Trigger.** Four Sky130 generated vias contribute cut arrays of `2x5`, `1x4`, `1x4` and `1x1`.
Together with two routing enclosures per via, the expected iRT obstacle count is:

```text
19 cut rectangles + 8 routing enclosures = 27 obstacles
```

**Before the fix.** The minimal fixture produced native `27` versus EDADB `65`. In the full GCD
snapshot, native had `27,299` obstacles and EDADB had `33,861`; 3,281 unique cut rectangles
changed multiplicity from one to three.

**Root cause.** EDADB SELECT calls `fromShadow()` after scalar restoration and again after vector
children at
`src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h:165` and
`src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h:201`. The adapter
appended generated cuts and derived shapes on each call.

**Code change.** Commit `e63ebd001` changes
`src/database/edadb/idb/shadow/shadow_idb_via_master.h`:

- delete and clear the owned cut list before regenerating it at line 108;
- clear bottom/cut/top derived layer-shape lists before `set_via_shape()` at line 208;
- retain standard `toShadow()/fromShadow()` signatures and EDADB's two-phase lifecycle.

**Passing evidence.** Strict fixture reports `27 == 27`; historical defect mode rejects the
fixed output; full GCD semantic input returns to `27,299` obstacles on both paths.

### 8.2 Native iEDA Diagnostic Fixes

Temporary iPL and Verilog-builder changes proved native causes but are not retained in the adapter milestone. Their source evidence, minimal cases and historical diagnostic results are maintained only in `stage-validation/known-native-defects.md`.

## 9. Native iEDA Boundaries

The validation campaign identified native iEDA behavior that is not fixed on the adapter branch:

| Boundary | Adapter conclusion |
|---|---|
| iPL pointer-ordered connectivity | Native determinism defect; not an EDADB field-loss result. |
| Verilog IO alias ownership/direction | Native reader/writer ownership defect; retained as a causal fixture. |
| IHP130 bound-skew-tree | Native iCTS/configuration boundary before EDADB restoration. |
| iTO undefined HPWL/STWL summary fields | Uninitialized report fields are excluded from semantic comparison, while raw output remains. |
| iRT pointer-order nondeterminism | Strict pre-tool adapter gate passes; full routed equivalence remains `REVIEW`. |

The sole detailed owner for reproduction commands, code evidence and historical diagnostic fixes is `stage-validation/known-native-defects.md`.

## 10. EDADB Core Conclusion

No confirmed EDADB core defect was found.

The generated-via failure occurred because the adapter assumed `fromShadow()` would execute only
once. Current EDADB SELECT explicitly completes scalar shadow restoration and later completes
vector-child restoration. The correct local contract is therefore replacement-based,
repeat-safe adapter reconstruction. The fix changed only the adapter and passed both minimal and
full-design oracles; changing EDADB core was neither necessary nor justified by the evidence.

## 11. Finalized Harness And Remaining Research

The milestone includes these test-framework changes:

- resource-aware concurrent native/EDADB controls through `PARALLEL_FIRST_EDADB`,
  `EDADB_RUNS_UPFRONT` and `STAGE_RUN_JOBS`;
- dirty-state manifests containing `git_status_short` and `git_diff_sha256`;
- iRT raw samples, runtime, means, sample standard deviation, standardized difference and exact
  permutation value, with a unit test;
- the tracked generated-via DEF fixture required by a fresh clone.

Two research questions remain outside adapter acceptance:

- three-plus-three iRT runs have minimum exact `p=0.1`; add targeted repeats only for unresolved
  metrics such as Metal4 wire count, without inventing a tolerance;
- parallel controls expose tail imbalance at `DetailedRouter.cpp:369`; changing iRT scheduling is
  a separate performance study and must not alter this correctness baseline.

## 12. Commit Index

| Commit | Purpose | Principal implementation files |
|---|---|---|
| `621ae3df6` | Establish staged native/EDADB equivalence harness and artifact comparators. | `test/stage_validation/run_stage_validation.sh`, `compare_stage_runs.py`, `create_manifest.py`, `tcl/run_stage.tcl` |
| `e63ebd001` | Make generated-via restoration idempotent. | `idb/shadow/shadow_idb_via_master.h`, generated-via fixture/checker docs |
| `df6cae124` | Record proven iRT pointer-order determinism boundary. | Stage-validation documentation and agent rules |
| `eaba42801` | Prove iPL pointer-order divergence; native source change later reverted. | Historical `NesterovPlace.cc` patch, input preparation, harness/docs |
| `1e00b5940` | Add semantic iRT input snapshots, variability diagnostics and stable IHP CTS profile. | `DataManager.cpp`, iRT comparators/summarizer, CTS config |
| `36f38d6c6` | Record the initial Pico native iTO boundary before root cause was fixed. | Documentation only; superseded by `ef07f23df` for resolution |
| `ef07f23df` | Prove Verilog IO alias causes; native source changes later reverted. | Historical Verilog patch plus retained alias fixture/test |

## 13. Artifact Locations

| Evidence | Location | Persistence |
|---|---|---|
| Pico stage inputs | `/tmp/iedadb_stage_inputs_pico_parallel_20260811/ihp130_picorv32a/result` | Host-local, ephemeral |
| Pico 3+3 iRT run | `/tmp/iedadb_stage_validation_pico_irt_6way_retry_20260811/ihp130_picorv32a/irt` | Host-local, ephemeral |
| Generated-via final run | `/tmp/iedadb_generated_via_final_20260811` | Host-local, ephemeral |
| Verilog alias final run | `/tmp/iedadb_alias_final_20260811` | Host-local, ephemeral |
| Final object regression | `/tmp/iedadb_adapter_milestone_regression_20260811` | Host-local, ephemeral |
| Final generated-via fixture | `/tmp/iedadb_generated_via_native_milestone_20260811` | Host-local, ephemeral |
| Final native-alias oracle | `/tmp/iedadb_alias_native_milestone_20260811` | Host-local, ephemeral |
| Final Sky130 iRT input gate | `/tmp/iedadb_sky130_env_gate_milestone_20260811` | Host-local, ephemeral |
| Methodology and report | `src/database/edadb/docs/adapter-testing.md` | Repository |
| Executable harness | `src/database/edadb/test/stage_validation` | Repository |

## 14. Final Assessment

- The adapter passes strict pre-tool equivalence for the tested stages and datasets.
- Historical diagnostic runs showed stable transitions after temporary native fixes, but those
  iEDA source changes are not retained in the final adapter milestone.
- No EDADB core defect is proven by this campaign.
- Full iRT cannot yet be declared statistically equivalent or defective. Historical deep
  diagnostics found equal semantic content, while the retained baseline provides the original
  `env_map.json` gate and records routed output as `REVIEW`.
- Current acceptance is the object-level adapter regression, strict generated-via fixture and
  original iEDA `env_map.json` gate; native point-tool defects remain separate upstream work.
