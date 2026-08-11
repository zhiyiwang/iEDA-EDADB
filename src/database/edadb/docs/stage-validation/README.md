# EDADB Point-Tool Stage Validation

## Reports

- [2026-08-11 independent validation report](test-report-20260811.md): executed tests,
  first-principles rationale, discovered defects, code fixes, commits, and remaining review
  boundaries.
- [Known native iEDA defects](known-native-defects.md): reproducible cases retained as text/tests
  while original point-tool and Verilog-builder source remains unchanged.

## Goal

Prove that loading a physical design through EDADB produces an iDB that iPL, iCTS/iTO, and iRT consume equivalently to the native DEF-loaded iDB. DEF roundtrip alone is necessary but insufficient: point tools exercise name references, backlinks, derived geometry, connectivity, IDs, and mutable state.

Final milestone policy: fix confirmed EDADB adapter defects, but do not modify original iEDA
point-tool, Verilog-builder, or diagnostic production source. Native defects found during this
campaign are preserved as reproducible evidence in `known-native-defects.md`. Result sections
that used temporary native diagnostic patches are explicitly historical.

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
4. For iRT, compares original iEDA's wrapped die, obstacle and pin-shape `env_map.json` before routing.
5. Runs three native controls to measure native determinism.
6. Runs one EDADB control when native output is stable; expands to three after variability or mismatch.
7. Compares structure, connectivity-facing outputs, geometry, reports, feature metrics, and final DEF.
8. Preserves every command, hash, log, database, and output under an isolated run directory.

Process concurrency is resource-aware rather than uniform. On the current 40-logical-CPU, 125-GiB host, the scheduler reserves 16 GiB for the host and budgets 16 GiB per iEDA process from current `MemAvailable`. Expensive controls may run concurrently only when native and EDADB paths use the same internal thread count, fixed input/configuration, and independent output directories. For iRT, `PARALLEL_FIRST_EDADB=1 EDADB_RUNS_UPFRONT=3 STAGE_RUN_JOBS=3 RT_THREAD_NUMBER=12` runs three native and three EDADB controls in one batch. The modest thread oversubscription compensates for observed OpenMP barrier tail imbalance; it does not change the equal-thread comparison contract. Interrupted fixture tests terminate child processes so an orphaned iEDA process cannot silently invalidate later measurements.

The PicoRV32A six-control run reached a peak load above 60 while using about 32 GiB and no swap. Later DetailedRouter tails use fewer cores because `routeDRBoxMap()` has a static OpenMP loop at `src/operation/iRT/source/module/detailed_router/DetailedRouter.cpp:369`; adding threads during a run cannot rebalance already assigned boxes. Changing that schedule is a separate iRT performance experiment and must not be mixed into an adapter-correctness baseline.

`RT_ENABLE_NOTIFICATION=1 RT_SNAPSHOT_ONLY=1` asks original iEDA to emit `env_map.json` and stop
before routing. The deeper semantic/pointer-order snapshot used during diagnosis required a
temporary `DataManager` patch and is not retained in the final milestone.

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

IHP130 stage inputs are generated outside the repository so existing design results remain
read-only. The preparation script passes every output path explicitly and rejects a missing or
empty DEF even if iEDA returns zero:

```bash
DATASET=ihp130_aes PREPARE_THROUGH=ipl \
bash src/database/edadb/test/stage_validation/prepare_ihp130_stage_inputs.sh
```

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

### Bug-Proof Protocol

Before changing production code, the validation uses the grilling workflow:

1. Establish whether native controls are stable under the same input, configuration, threads, and revision.
2. Require native and EDADB pre-tool state to match before attributing any post-tool difference to the adapter.
3. Locate the first divergent consumer or derived state, then reduce it to a legal minimal fixture when possible.
4. Trace the full object lifecycle and derive the expected count/value change from source instead of guessing from the final DEF.
5. Commit only a complete local adapter fix with a failing-before/passing-after oracle. Native
   iEDA defects remain documented/reproducible boundaries and are fixed upstream, not here.

## Historical Sky130 Diagnostic Results

These results describe the diagnostic campaign, including temporary native fixes. The final
milestone retains only adapter changes; current native-defect expectations are documented in
`known-native-defects.md`.

| Stage | Result | Evidence |
|---|---|---|
| iPL | Pass | Three native controls are stable; EDADB matches. |
| iCTS | Pass | Three native controls are stable; EDADB matches. |
| iTO DRV | Pass with native-field exclusion | DEF, Verilog, DB report, and stable metrics match. Native `TimingOptSummary::HPWL/STWL` are uninitialized and excluded while raw JSON is retained. |
| iTO Hold | Pass | Three native controls are stable; EDADB matches. |
| Incremental legalization | Pass | Three native controls are stable; EDADB matches. |
| iRT input gate | Pass after generated-via fix | Canonical DEF/report and the semantic DataManager source database match; pointer-order differences are reported separately. |
| iRT full routing | Review | Three native controls produce different legal routing geometries and QoR values. EDADB attribution therefore requires a native variability envelope rather than pairwise exact DEF equality. |

## Historical IHP130 AES Diagnostic Results

| Stage | Result | Evidence |
|---|---|---|
| Input preparation through iPL | Pass | Explicit iFP, iNO, and iPL outputs were created under `/tmp/iedadb_stage_inputs/ihp130_aes/result`. |
| iPL | Pass after native iPL fix | Native pre-tool and EDADB pre-tool DEFs are byte-identical; three native controls are stable; EDADB matches the native DEF, feature JSON, reports, and QoR. |
| iCTS | Pass with stable algorithm profile | The default IHP skew-tree algorithm fails natively before EDADB attribution. With the documented no-skew-tree profile, three native controls are stable and EDADB matches. |
| iTO DRV | Pass | Three native controls are stable and EDADB matches. |
| iTO Hold | Pass | Three native controls are stable and EDADB matches. |
| Incremental legalization | Pass | Three native controls are stable and EDADB matches. |
| iRT semantic input gate | Pass | Native and EDADB `semantic_database` snapshots are exactly equal; pointer-ordered fixed-rectangle iteration differs with an identical value multiset. |
| iRT full routing | Review | Three native and three EDADB runs complete, but native routing is not deterministic; observed QoR ranges are retained as evidence, not promoted to a tolerance. |

## Historical IHP130 PicoRV32A Diagnostic Results

| Stage | Result | Evidence |
|---|---|---|
| Input preparation through iPL | Pass | iFP, iNO, and iPL produce non-empty isolated outputs. |
| iPL | Pass | Native/EDADB pre-tool state matches; three native controls are stable; EDADB matches. |
| iCTS | Pass with stable algorithm profile | Three native controls are stable and EDADB matches under the documented no-skew-tree profile. |
| iTO DRV | Pass after native Verilog alias fix | Pre-tool state matches; three native controls are stable; EDADB matches the native result. |
| iTO Hold | Pass | Three native controls are stable and EDADB matches. |
| Incremental legalization | Pass | Three native controls are stable and EDADB matches. |
| iRT semantic input gate | Pass | Native and EDADB semantic DataManager snapshots match; only the pointer-ordered fixed-rectangle view differs while its content is equal. |
| iRT full routing | Review | Three native and three EDADB runs all exit zero and preserve fixed structure. Both groups produce different legal routing results, so the report retains raw samples and exploratory statistics rather than inventing a pass tolerance. |

### PicoRV32A Six-Control iRT Evidence

The completed run uses iEDA commit `ef07f23df`, input SHA-256
`77ca164086e2fddc030714edf91a02e83ba1751c6e3b5efc5121bf82790cd9fd`, six concurrent
processes, and 12 iRT threads per process. Artifacts are under
`/tmp/iedadb_stage_validation_pico_irt_6way_retry_20260811/ihp130_picorv32a/irt`.

- All six processes exit zero and retain `18,157` instances, `411` IO pins, `19,920` nets,
  `2` PDNs, and the same layer counts.
- Native and EDADB pre-tool DEFs are byte-identical. The iRT semantic input database is equal;
  only an allocator-dependent pointer iteration view differs.
- Mean EDADB-minus-native changes are `-0.0233%` wire length, `+0.1884%` wires,
  `+0.1592%` segments, `+0.0921%` vias, `+0.3195%` patches, and `+3.6051%` final DRC
  violations. Mean iRT runtime changes by `+0.5087%`; native and EDADB runtime ranges overlap.
- With three runs per group, the exact two-sided permutation test has only 20 partitions and
  cannot produce `p < 0.1`. No metric proves a group difference. Metal4 wire count has the
  largest unresolved separation (`+0.8442%`, `p=0.1`) and remains a targeted repeatability
  question, not a demonstrated adapter defect.
- `variability_summary.json` stores every raw sample, mean, sample standard deviation,
  standardized mean difference, exact permutation value, and descriptive native range. None of
  these values is used as an automatic acceptance tolerance; strict wrapped-input equality is
  still the adapter gate.

### Diagnosed PicoRV32A Net-Alias / RC-Tree Defect

The Pico failure was upstream of EDADB and stable across three native runs. The bug-proof audit
reduced it to two native Verilog builder defects:

1. `build_nets()` initially attaches each IO pin to its same-name net. The assign branches in
   `RustVerilogRead::build_assign()` then added the pin to the assigned net and changed its single
   `_net` pointer without removing it from the old net container. One pin therefore appeared in
   two `IdbNet::_io_pin_list` vectors even though its back-reference named only one net.
2. `VerilogWriter::writeAssign()` emitted both input and output aliases as `assign net = port`.
   For an output port this reverses Verilog semantics; it must emit `assign output_port = net`.

The temporary diagnostic correction enforced one invariant: moving an IO pin to a new logical net removes it
from the previous net, inserts it only once in the target net, and then synchronizes its `_net`
and `_net_name` back-references. Commit `ef07f23df` proved the cause, but those native Verilog
builder changes are not retained in the final adapter milestone.

The minimal fixture `test/stage_validation/fixtures/verilog/io_port_alias.v` covers input-to-net,
net-to-output, and output-to-output assignments. On the final unchanged-native tree,
`EXPECT_KNOWN_NATIVE_DEFECT=1` requires the exact reversed-assignment and duplicate-root-membership
signature. Strict mode describes the desired upstream behavior and intentionally fails. During
diagnosis, the temporary patch made `trace_data[5]` occur only under `_17496_`, made iNO emit
`assign trace_data[5] = fanout_net_56`, and allowed native iTO DRV to complete; this proved the
cause without retaining the native patch.

### IHP130 CTS Native Boundary

The default IHP130 configuration enables the bound-skew-tree algorithm. Three native AES runs
fail at `src/operation/iCTS/source/solver/tools/tree_builder/bound_skew_tree/BoundSkewTree.cc:1600`
because an edge has stored length zero while its endpoint Manhattan distance is `55.239`. This is
not a floating-point tolerance issue and occurs before an EDADB comparison. The stage harness
therefore keeps the default failure as native evidence and uses
`test/stage_validation/config/ihp130_cts_no_skew_tree.json` for the stable comparison profile.
That profile selects the existing non-skew-tree implementation; it does not patch CTS geometry.

### Diagnosed iPL Pointer-Ordered Connectivity

The initial AES iPL comparison satisfied the strict adapter gate but failed after placement:

- native and EDADB pre-tool DEFs were byte-identical;
- three native controls produced one stable result and three EDADB controls produced a second
  stable result;
- reducing iPL to one internal thread did not remove the difference;
- the first numerical divergence occurred at Nesterov iteration 320, where HPWL differed by two
  DBU before later branch decisions amplified it.

The source path identifies a native iPL determinism defect rather than a missing EDADB field:

1. `IDBWrapper::wrapNetlists()` traverses the logical iDB net and pin relationships at
   `src/operation/iPL/source/module/wrapper/IDBWrapper.cc:572-629`; `wrapPin()` appends each new
   placement pin to `Design::_pin_list` at
   `src/operation/iPL/source/module/wrapper/IDBWrapper.cc:631-697`.
2. `NesterovPlace::wrapNesPinList()` preserves that logical order in `_nPin_list`, while also
   creating pointer-keyed lookup maps, at
   `src/operation/iPL/source/module/global_placer/electrostatic_placer/NesterovPlace.cc:192-204`.
3. The old `completeConnection()` iterated `std::map<Pin*, NesPin*>`, so allocation addresses
   selected the append order of `NesInstance::_nPin_list` and `NesNet::_loader_list`. Those vectors
   feed placement calculations, including per-instance wirelength preconditioning.
4. The temporary correction in commit `eaba42801` traversed `_nPin_list` and used `_pin_map` only
   for reverse lookup, proving the cause without changing identity, connectivity or formulas.
   The final adapter milestone restores original `NesterovPlace.cc`.
5. `initNodes()` still uses a pointer-keyed lookup map because it immediately sorts the resulting
   node vector by stable `pin_id` at
   `src/operation/iPL/source/module/global_placer/electrostatic_placer/NesterovPlace.cc:326-353`;
   it does not create the order-sensitive vectors involved in this failure.

Passing-after evidence:

- IHP130 AES: three native DEFs and the EDADB DEF share SHA-256
  `c8458e6342d2e8c041c63ad05df24f76fb9f47c26a3990a536100eb66802e336`;
- sky130 GCD: three native DEFs and the EDADB DEF share SHA-256
  `26fc1e9ff16bf660c403cb9d3f0a8714be0ffd7a945d20fae76027e6c6d5dd85`.

### Resolved iRT Generated-Via Cut Duplication

Before the fix, the single-thread diagnostic was deterministic: all three native runs matched one another, all three EDADB runs matched one another, but the two groups differed. The raw pre-tool DEF was byte-identical, so this was hidden iDB state rather than DEF text order.

The iRT wrapper snapshot localizes the difference to `env_shape.obs`:

- native: 27,299 obstacle shapes;
- EDADB: 33,861 obstacle shapes;
- 3,281 unique cut-layer rectangles change multiplicity from one to three, adding 6,562 shapes;
- additions by cut layer: `via=3600`, `via2=1440`, `via3=1440`, `via4=82`;
- all net pin-shape entries and the die are identical.

The source-level lifecycle explains the exact `N -> 3N` result:

1. The input defines four generated DEF vias at `scripts/design/sky130_gcd/result/iPL_lg_result.def:71`.
2. EDADB SELECT calls `fromShadow()` after scalar restoration at `src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h:165` and again after vector-child restoration at `src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h:201` for `TABLE4SHADOW_WVEC` objects.
3. The old `Shadow<IdbViaMasterGenerate>::fromShadow()` appended `N` generated cuts at the reconstruction loop now located at `src/database/edadb/idb/shadow/shadow_idb_via_master.h:120-132`.
4. `Shadow<IdbViaMaster>::fromShadow()` then calls `set_via_shape()` at `src/database/edadb/idb/shadow/shadow_idb_via_master.h:211`. The first pass created `N` derived cut shapes. The second pass appended another `N` source cuts, then appended all `2N` cuts to the existing `N` derived shapes, producing `3N`.
5. iRT consumes those derived cut shapes as special-net obstacles at `src/operation/iRT/interface/RTInterface.cpp:1017` and `src/operation/iRT/interface/RTInterface.cpp:1030`.

### Minimal Generated-Via Confirmation

`src/database/edadb/test/stage_validation/fixtures/sky130_generated_via/generated_via.def`
reduces the real failure to four generated vias and one special net that uses each via once.
The oracle follows directly from the DEF `ROWCOL` values:

- native cut shapes: `2x5 + 1x4 + 1x4 + 1x1 = 19`;
- routing-layer enclosures: two per via, so `8`;
- expected native and correct EDADB total: `19 + 8 = 27` obstacles;
- pre-fix EDADB total: `3x19 + 8 = 65`, with exactly 38 added cut shapes and unchanged enclosures;
- fixed EDADB total: `19 + 8 = 27`, with the same rectangle multiset as native.

Both serial and two-process fixture scheduling reproduce the same exact signature. Diagnostic
mode passes only for that signature:

```bash
EXPECT_KNOWN_DEFECT=1 \
bash src/database/edadb/test/stage_validation/run_generated_via_fixture.sh
```

Strict mode omits `EXPECT_KNOWN_DEFECT` and is the acceptance test. The historical diagnostic
mode remains available to prove that a pre-fix binary has the exact `27 -> 65` signature; it
must fail for a fixed binary. The fixture README records why one row and the routing tracks are
the minimum valid iRT physical context.

### Adapter Correction And Acceptance

The smallest sufficient production change is limited to generated-via reconstruction:

1. At the start of `Shadow<IdbViaMasterGenerate>::fromShadow()`, delete and clear the
   target `_cut_rect_list` before regenerating cuts from `ROWCOL`, `CUTSIZE`, and
   `CUTSPACING`.
2. In the generated branch of `Shadow<IdbViaMaster>::fromShadow()`, clear the target
   bottom/cut/top `IdbLayerShape` rectangle lists before `set_via_shape()` derives them.
3. Keep the fixed-via branch and EDADB's scalar/vector two-phase SELECT lifecycle unchanged;
   fixed child rows are populated only during the vector phase and do not cause this defect.

This makes both generated-via calls replacement-based and idempotent without adding phase
flags, changing schema, or changing the standard `toShadow/fromShadow` signatures. The fix was
accepted by all required checks:

- strict minimal fixture: `27 == 27`;
- historical known-defect checker rejects the fixed result;
- full sky130 iRT wrapper gate: native and EDADB both contain `27,299` obstacle shapes;
- fixed/generated Via regression and the complete object-level regression remain green.

Final unchanged-native milestone reruns additionally passed the six script unit tests, the full
object-level regression, the exact native Verilog-alias known-defect oracle and the original
Sky130 `env_map.json` gate. Exact commands and artifact roots are recorded in
`test-report-20260811.md`.

### Current Full-Routing Review Boundary

After the input gate passed, three single-process native iRT controls produced different routed
DEFs, wire lengths, segment/via counts, and patch counts. For example, native controls changed
total wire length from `13,083,995` to `13,087,655` DBU and patch count from `18` to `24`.
Because the native oracle is not point-deterministic, an EDADB run cannot be rejected merely for
not matching `native-1` byte-for-byte. The next comparator must first model the native result
distribution and then test whether EDADB structural/DRC/QoR results fall within that envelope.
The identical pre-tool iRT environment remains a strict, deterministic adapter gate.

A single-thread follow-up separates thread scheduling from object-allocation order. All three
native controls are mutually identical, and all three EDADB controls are mutually identical,
but the two groups converge to different legal routes. Enhanced diagnostic snapshots confirmed
that the ordered nets, pin grouping/driver flags, routing and cut shapes, GCell axes, via masters,
and geometry multisets are identical before routing. The first observed difference is iteration
order inside `DataManager::getTypeLayerNetFixedRectMap()` (`src/operation/iRT/source/data_manager/DataManager.cpp:345`):
the same rectangle pointers are returned by `std::set<EXTLayerRect*>` in address order. iRT stores
these and other routing objects in pointer-ordered sets (`src/operation/iRT/source/data_manager/advance/GCell.hpp:85-107`),
and algorithmic consumers iterate that order, for example obstacle clipping in
`src/operation/iRT/source/module/pin_accessor/PinAccessor.cpp:287-317`.

This is a real iRT determinism defect: semantically identical inputs with different allocation
histories can select different physical implementations. It is not evidence of a missing EDADB
field. A local sort in PinAccessor is insufficient because pointer-ordered fixed rectangles,
access points, segments, patches, and violations are consumed throughout later routing stages.
Replacing the containers also requires care: a value-only `std::set` comparator could collapse
distinct objects with equal geometry. No incomplete production fix is retained; the strict input
gate remains authoritative while an iRT-wide stable identity/value-order design is deferred.

The complete AES input snapshot strengthens that boundary. It compares ordered layers, track and
GCell axes, via masters, derived layer geometry, obstacles, ordered nets, ordered pins, driver
flags, access points, routing/cut shapes, bounding boxes, and scalar database values. Native and
EDADB semantic payloads are exactly equal. The separately recorded
`DataManager::getTypeLayerNetFixedRectMap()` view differs only in pointer iteration order; sorting
each group by rectangle value makes the views equal. A repository audit finds pointer-address
sets in GCell state and throughout downstream router modules, so changing one return path would
not remove the architectural source of nondeterminism. Under the bug-proof protocol, this is a
proven native iRT determinism defect but not a safe local adapter fix.

Test-framework defects may be fixed directly with a self-test. Native iEDA behavior and adapter storage semantics are not changed opportunistically during stage validation.

### Known Native Metric Limitation

`ToApi::outputSummary()` returns a local `TimingOptSummary` without assigning its `HPWL` or `STWL` members (`src/operation/iTO/api/ToApi.cpp:114-163`). `feature_tool` therefore emits subnormal process-memory values such as `3.7e-322`; native and EDADB processes can differ even when DEF, Verilog, timing fields, DB reports, and feature summary are identical. The harness retains the raw values but excludes only `optDrv/optHold/optSetup.HPWL` and `.STWL` from semantic comparison. This is a documented native iEDA defect, not an adapter field or a valid QoR tolerance.

## Final Milestone Acceptance

- Original iEDA production files match
  `milestone/edadb-idb-15class-sort-abc-no-sort-d-20260810`.
- `Shadow<IdbViaMasterGenerate>` and `Shadow<IdbViaMaster>` retain the generated-via
  idempotence fix from `e63ebd001`.
- Object-level EDADB regression, strict generated-via `27 == 27`, original iRT `env_map.json`,
  and test-script unit checks are the adapter acceptance gates.
- iPL pointer-order, Verilog IO alias, default IHP130 bound-skew-tree, iTO undefined metrics and
  iRT pointer-order remain documented native boundaries; post-tool differences caused by them
  are not adapter regressions.

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

Each process directory contains its log, inputs/outputs, reports, feature JSON, and `manifest.json`. A dirty worktree records both `git_status_short` and `git_diff_sha256`, rather than only an ambiguous dirty flag. Existing files under `scripts/design/*/result/` are read-only test inputs and are never overwritten.
