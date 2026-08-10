# EDADB Point-Tool Stage Validation

## Goal

Prove that loading a physical design through EDADB produces an iDB that iPL, iCTS/iTO, and iRT consume equivalently to the native DEF-loaded iDB. DEF roundtrip alone is necessary but insufficient: point tools exercise name references, backlinks, derived geometry, connectivity, IDs, and mutable state.

## First-Principles Model

### What Is Being Tested

For one fixed physical-design state `S`:

- Native reconstruction: `LEF + DEF(S) -> iDB_native(S)`.
- EDADB reconstruction: `LEF + DEF(S) -> iDB -> EDADB`, followed in a new process by `LEF + EDADB -> iDB_edadb(S)`.
- Point-tool transition: `T(iDB_native(S))` versus `T(iDB_edadb(S))`.

The adapter is correct for tool `T` only when the restored pre-tool state is equivalent and the resulting physical transition remains within the tool's native repeatability envelope.

### What The Harness Does

1. Uses identical LEF, DEF, liberty, SDC, tool configuration, thread count, and fixed code revision.
2. Creates native and EDADB pre-tool snapshots in separate processes.
3. Compares canonical DEF and stable `report_db` content before invoking the point tool.
4. For iRT, compares the wrapped die, obstacle, and pin-shape environment before routing starts.
5. Runs three native controls to measure native determinism.
6. Runs one EDADB control when native output is stable; expands to three after variability or mismatch.
7. Compares structure, connectivity-facing outputs, geometry, reports, feature metrics, and final DEF.
8. Preserves every command, hash, log, database, and output under an isolated run directory.

Process concurrency is resource-aware rather than uniform. On the current 40-logical-CPU, 125-GiB host, iPL controls have a CPU cap of three, but the scheduler also reserves 16 GiB for the host and budgets 16 GiB per iEDA process from current `MemAvailable`. iCTS/iTO/iRT already use large internal thread pools, so their controls run serially to avoid CPU oversubscription and a distorted repeatability baseline. Interrupted fixture tests terminate child processes so an orphaned iEDA process cannot silently invalidate later measurements.

`RT_ENABLE_NOTIFICATION=1 RT_SNAPSHOT_ONLY=1` is a diagnostic-only mode that emits iRT's wrapped environment JSON and stops before routing. It is disabled by default and does not modify the production adapter.

### Why Independent Stages Come First

Each stage starts from an existing canonical stage input:

| Stage | Fixed input | Main state consumed or modified |
|---|---|---|
| iPL | `iTO_fix_fanout_result.def` | Rows, regions, instances, pins, nets; writes placement coordinates/status/orient. |
| iCTS | `iPL_result.def` | Clock pins/nets and placed instances; inserts clock buffers and reconnects topology. |
| iTO DRV | `iCTS_result.def` | Timing graph connectivity; inserts buffers/nets and reconnects pins. |
| iTO Hold | `iTO_drv_result.def` | Timing graph connectivity; performs hold repair and topology mutation. |
| Incremental legalization | `iTO_hold_result.def` | Modified instances and rows; writes legal coordinates/status. |
| iRT | `iPL_lg_result.def` | Layers, vias, blockages, nets and pins; rebuilds regular routing wires/segments/vias. |

Independent inputs prevent one upstream mismatch from contaminating every downstream result. After all isolated stages pass, the same checks are applied to a complete chained flow.

## Dataset Layers

1. `sky130_gcd`: first executable baseline with existing stage DEFs and reports.
2. IHP130 AES: approximately 15k instances; larger real-flow validation.
3. IHP130 PicoRV32A: approximately 18k instances; larger connectivity and routing validation.
4. Minimal fixtures: targeted valid derivatives of real DEF input for placement, CTS/TO, and routing branches.
5. ISPD18 routing data: later iRT-specific extension after download and format integration.

The first three datasets are introduced sequentially. A larger dataset is not used to hide a failure in a smaller one.

## Minimal Fixture Contract

Every fixture must document:

- what DEF records and iDB class/member relationships it contains;
- which native parser/writer and point-tool code consumes those fields;
- why the fixture isolates a specific correctness hypothesis;
- required LEF masters, layers, vias, coordinates, clocks, and connectivity;
- pre-tool invariants that native and EDADB reconstruction must share;
- fields the point tool is allowed to modify and fields it must preserve;
- structural, SQL, DEF, QoR, and DRC assertions;
- commit, input/config hashes, thread count, seed, and exact reproduction command.

Fixtures are small for causal isolation, not as substitutes for real designs. Each fixture must pass the native path before its EDADB result is interpreted.

## Failure Classification

| Failure boundary | Classification | Action |
|---|---|---|
| Native path also fails | Native iEDA, data, PDK, or configuration | Preserve artifacts; do not modify adapter. |
| Native/EDADB differ before tool execution | Adapter/schema/toShadow/fromShadow | Stop the stage; minimize to class/member evidence. |
| Pre-tool state matches but post-tool result differs | Hidden derived state, backlink, ID/order dependency, or tool nondeterminism | Compare three native controls before attribution. |
| Only runtime/memory differs | Volatile measurement | Exclude from semantic equality; retain for later performance work. |
| Schema/ownership/order semantics need change | Architectural adapter issue | Present evidence and obtain review before modification. |

## Sky130 Isolated Results

| Stage | Result | Evidence |
|---|---|---|
| iPL | Pass | Three native controls are stable; EDADB matches. |
| iCTS | Pass | Three native controls are stable; EDADB matches. |
| iTO DRV | Pass with native-field exclusion | DEF, Verilog, DB report, and stable metrics match. Native `TimingOptSummary::HPWL/STWL` are uninitialized and excluded while raw JSON is retained. |
| iTO Hold | Pass | Three native controls are stable; EDADB matches. |
| Incremental legalization | Pass | Three native controls are stable; EDADB matches. |
| iRT | Open adapter defect | Canonical pre-tool DEF/report match, but the iRT-wrapped obstacle environment differs before routing. |

### iRT Generated-Via Cut Duplication

The single-thread diagnostic is deterministic: all three native runs match one another, all three EDADB runs match one another, but the two groups differ. The raw pre-tool DEF is byte-identical, so this is hidden iDB state rather than DEF text order.

The iRT wrapper snapshot localizes the difference to `env_shape.obs`:

- native: 27,299 obstacle shapes;
- EDADB: 33,861 obstacle shapes;
- 3,281 unique cut-layer rectangles change multiplicity from one to three, adding 6,562 shapes;
- additions by cut layer: `via=3600`, `via2=1440`, `via3=1440`, `via4=82`;
- all net pin-shape entries and the die are identical.

The source-level lifecycle explains the exact `N -> 3N` result:

1. The input defines four generated DEF vias at `scripts/design/sky130_gcd/result/iPL_lg_result.def:71`.
2. EDADB SELECT calls `fromShadow()` after scalar restoration at `src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h:165` and again after vector-child restoration at `src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h:201` for `TABLE4SHADOW_WVEC` objects.
3. `Shadow<IdbViaMasterGenerate>::fromShadow()` appends `N` generated cuts at `src/database/edadb/idb/shadow/shadow_idb_via_master.h:108`.
4. `Shadow<IdbViaMaster>::fromShadow()` then calls `set_via_shape()` at `src/database/edadb/idb/shadow/shadow_idb_via_master.h:202`. The first pass creates `N` derived cut shapes. The second pass appends another `N` source cuts, then appends all `2N` cuts to the existing `N` derived shapes, producing `3N`.
5. iRT consumes those derived cut shapes as special-net obstacles at `src/operation/iRT/interface/RTInterface.cpp:1017` and `src/operation/iRT/interface/RTInterface.cpp:1030`.

### Minimal Generated-Via Confirmation

`src/database/edadb/test/stage_validation/fixtures/sky130_generated_via/generated_via.def`
reduces the real failure to four generated vias and one special net that uses each via once.
The oracle follows directly from the DEF `ROWCOL` values:

- native cut shapes: `2x5 + 1x4 + 1x4 + 1x1 = 19`;
- routing-layer enclosures: two per via, so `8`;
- expected native and correct EDADB total: `19 + 8 = 27` obstacles;
- current EDADB total: `3x19 + 8 = 65`, with exactly 38 added cut shapes and unchanged enclosures.

Both serial and two-process fixture scheduling reproduce the same exact signature. Diagnostic
mode passes only for that signature:

```bash
EXPECT_KNOWN_DEFECT=1 \
bash src/database/edadb/test/stage_validation/run_generated_via_fixture.sh
```

Strict mode omits `EXPECT_KNOWN_DEFECT`; it returns failure until native and EDADB obstacle
multisets are equal. The fixture README records why one row and the routing tracks are the
minimum valid iRT physical context.

### Reviewable Adapter Correction

The smallest sufficient production change is limited to generated-via reconstruction:

1. At the start of `Shadow<IdbViaMasterGenerate>::fromShadow()`, delete and clear the
   target `_cut_rect_list` before regenerating cuts from `ROWCOL`, `CUTSIZE`, and
   `CUTSPACING`.
2. In the generated branch of `Shadow<IdbViaMaster>::fromShadow()`, clear the target
   bottom/cut/top `IdbLayerShape` rectangle lists before `set_via_shape()` derives them.
3. Keep the fixed-via branch and EDADB's scalar/vector two-phase SELECT lifecycle unchanged;
   fixed child rows are populated only during the vector phase and do not cause this defect.

This makes both generated-via calls replacement-based and idempotent without adding phase
flags, changing schema, or changing the standard `toShadow/fromShadow` signatures. Acceptance
requires the strict minimal fixture to change from `27 != 65` to `27 == 27`, the full sky130
iRT wrapper gate to change from `27299 != 33861` to equality, and existing fixed/generated via
roundtrip tests to remain green. Production adapter code remains unchanged until review.

Test-framework defects may be fixed directly with a self-test. Native iEDA behavior and adapter storage semantics are not changed opportunistically during stage validation.

### Known Native Metric Limitation

`ToApi::outputSummary()` returns a local `TimingOptSummary` without assigning its `HPWL` or `STWL` members (`src/operation/iTO/api/ToApi.cpp:114-163`). `feature_tool` therefore emits subnormal process-memory values such as `3.7e-322`; native and EDADB processes can differ even when DEF, Verilog, timing fields, DB reports, and feature summary are identical. The harness retains the raw values but excludes only `optDrv/optHold/optSetup.HPWL` and `.STWL` from semantic comparison. This is a documented native iEDA defect, not an adapter field or a valid QoR tolerance.

## Artifact Layout

Tests and documentation remain in the repository. Generated data defaults to:

```text
/tmp/iedadb_stage_validation/<dataset>/<stage>/
  input/
  precheck/native/
  precheck/edadb/
  native-1/
  native-2/
  native-3/
  edadb-1/
  edadb-2/
  edadb-3/
```

Each process directory contains its log, inputs/outputs, reports, feature JSON, and `manifest.json`. Existing files under `scripts/design/*/result/` are read-only test inputs and are never overwritten.
