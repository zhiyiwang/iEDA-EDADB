# EDADB Stage Validation Tests

This directory validates that a point tool consumes an EDADB-restored iDB in the same way as a native DEF-loaded iDB. It is separate from the object-level roundtrip regression in `../run_idb_roundtrip_regression.sh`.

## Current Scope

- Branch baseline: `edadb-idb-dev/sort-abc-no-sort-d` milestone implementation.
- Dataset profiles: `sky130_gcd`, `ihp130_aes`, and `ihp130_picorv32a`.
- Isolated stages: `ipl`, `icts`, `ito_drv`, `ito_hold`, `ipl_lg`, `irt`.
- Native controls: three runs by default.
- EDADB controls: one run when native results are stable and equal; three runs after variability or mismatch.
- Resource-aware scheduling: process concurrency is bounded by both CPU capacity and `MemAvailable`; iCTS, iTO, and iRT remain serial because each tool already uses many worker threads.
- iRT wrapper gate: native and EDADB paths must produce identical semantic DataManager inputs before routing starts. A separate pointer-order view reports allocator-dependent iteration without confusing it with missing adapter data.

## Run

From the repository root:

```bash
bash src/database/edadb/test/stage_validation/run_stage_validation.sh ipl
bash src/database/edadb/test/stage_validation/run_stage_validation.sh icts ito_drv ito_hold
bash src/database/edadb/test/stage_validation/run_stage_validation.sh irt
```

Generate isolated IHP130 inputs outside the repository, then validate a stage:

```bash
DATASET=ihp130_aes PREPARE_THROUGH=ipl \
bash src/database/edadb/test/stage_validation/prepare_ihp130_stage_inputs.sh

DATASET=ihp130_aes \
DATASET_RESULT_DIR=/tmp/iedadb_stage_inputs/ihp130_aes/result \
bash src/database/edadb/test/stage_validation/run_stage_validation.sh ipl
```

`PREPARE_THROUGH` accepts `ifp`, `ino`, `ipl`, `icts`, `ito_drv`, `ito_hold`, or
`ipl_lg`. Use `DATASET=ihp130_picorv32a` for the PicoRV32A profile. The preparation
script passes explicit output/report paths and fails when the expected DEF is absent,
empty, or accompanied by a DEF-save failure message.

The default IHP130 bound-skew-tree profile fails natively on AES before any EDADB
comparison. Use the existing non-skew-tree CTS implementation for the stable stage profile:

```bash
CTS_CONFIG_FILE=$PWD/src/database/edadb/test/stage_validation/config/ihp130_cts_no_skew_tree.json \
DATASET=ihp130_aes PREPARE_THROUGH=ipl_lg \
bash src/database/edadb/test/stage_validation/prepare_ihp130_stage_inputs.sh
```

Run every isolated stage:

```bash
bash src/database/edadb/test/stage_validation/run_stage_validation.sh
```

Override outputs or binaries:

```bash
OUT_ROOT=/tmp/iedadb_stage_validation \
IEDA_BIN=/path/to/iEDA \
bash src/database/edadb/test/stage_validation/run_stage_validation.sh ipl
```

Override process-level concurrency when the machine is shared or has different resources:

```bash
STAGE_RUN_JOBS=1 bash src/database/edadb/test/stage_validation/run_stage_validation.sh ipl
```

On the current 40-logical-CPU, 125-GiB host, the CPU cap is three concurrent iPL controls. The actual count is reduced when available memory cannot preserve the default 16-GiB host reserve plus 16 GiB per iEDA process. CTS/TO/RT controls stay at one process to avoid oversubscribing their internal 50-80/64-thread execution. Override the estimates with `IEDA_PROCESS_MEMORY_GIB` and `IEDA_MEMORY_RESERVE_GIB` only after measuring the dataset.

The [generated-via fixture](fixtures/sky130_generated_via/README.md) provides a separate,
first-principles iRT input test:

```bash
bash src/database/edadb/test/stage_validation/run_generated_via_fixture.sh
```

Strict mode is the current regression and requires native/EDADB equality. Set
`EXPECT_KNOWN_DEFECT=1` only with a pre-fix binary to reproduce the historical `27 -> 65`
signature; that mode must fail for the fixed implementation.

For iRT input-state diagnosis, set `RT_ENABLE_NOTIFICATION=1 RT_SNAPSHOT_ONLY=1`. Normal runs leave both disabled. The options ask iRT to emit `input_snapshot.json` under each run's `rt/data_manager/` directory and stop before routing. The snapshot separates the ordered semantic database from pointer-ordered consumer views; notification delivery itself may remain disabled.

Run only the strict native-vs-EDADB iRT wrapper gate without starting full routing:

```bash
IRT_INPUT_GATE_ONLY=1 \
bash src/database/edadb/test/stage_validation/run_stage_validation.sh irt
```

The gate fails on any semantic field or ordered-vector difference. If only the
`std::set<EXTLayerRect*>` iteration view differs while its rectangle multiset is equal, the
comparator reports `REVIEW` and keeps the semantic adapter gate passing. This distinguishes a
native pointer-order determinism defect from missing EDADB data.

Generated artifacts stay outside the repository by default. Every process writes a `manifest.json` containing the commit, branch, input/config hashes, command, host, thread setting, and exit status.

## Gate Order

1. Load the same LEF and input DEF through native `def_init` and EDADB `edadb_read` in separate processes.
2. Save canonical pre-tool DEF and `report_db` output.
3. Stop before the point tool if pre-tool semantic state differs.
4. For iRT, compare the internal environment produced by iDB-to-iRT wrapping and stop before routing if it differs.
5. Run three native controls.
6. Run one EDADB control; expand to three after native variability or EDADB mismatch.
7. Compare normalized DEF, timestamp-normalized Verilog, stable DB-report content, point-tool feature JSON, and runtime/memory-normalized summary JSON.

Before a mismatch is called an adapter bug, require a stable native control, identical pre-tool
state, a localized first divergence, and a source-derived causal explanation. Only complete local
fixes with failing-before/passing-after evidence are committed; partial point-tool determinism
patches are not accepted merely because they change the final diff.

`feature_tool` fields `optDrv/optHold/optSetup.HPWL` and `.STWL` are excluded from semantic comparison. `ToApi::outputSummary()` declares these fields in `TimingOptSummary` but does not initialize or assign them (`src/operation/iTO/api/ToApi.cpp:114-163`), so emitted subnormal values depend on process memory rather than design state. The raw JSON remains in every run directory as evidence.

Raw DEF differences remain available in the artifacts. The existing normalizer may reorder only Level-D root records; it never reorders A/B/C roots or nested vectors.

## Current iPL Status

Sky130 GCD and IHP130 AES both pass strict native-vs-EDADB iPL validation. The AES test
exposed a native iPL defect in which `NesterovPlace::completeConnection()` iterated a
pointer-keyed map while constructing order-sensitive instance-pin and net-loader vectors.
The implementation now traverses the existing logical `_nPin_list` and uses the map only
for lookup. Three native controls and the EDADB result are identical on both datasets; the
full causal chain and hashes are recorded in `../../docs/stage-validation/README.md`.

## Current iRT Status

The generated-via defect is fixed. The strict minimal fixture reports `27 == 27`, and the full
sky130 wrapper gate reports identical native/EDADB iRT input environments with `27,299`
obstacles. Full routing reaches the repeatability review gate because three native controls
produce different routed DEFs and QoR values. Detailed evidence and the next comparison boundary
are recorded in `../../docs/stage-validation/README.md`. A single-thread diagnostic further proves
that pointer-address iteration inside iRT can produce different legal routes from semantically
identical native and EDADB inputs; this is classified separately from adapter restoration.

When native iRT controls vary, the harness writes `variability_summary.json`. It records exact
fixed-structure equality plus native and EDADB samples for total/per-layer wire length, wire,
segment, via, patch, and final DRC-violation counts. Native observed min/max values are descriptive
evidence only; three samples are not promoted into an arbitrary acceptance tolerance.

PicoRV32A currently passes strict iPL, stable-profile iCTS, and the isolated iRT semantic input
gate. Native iTO DRV aborts reproducibly because `trace_data[5]` is absent from the RC tree for
`fanout_net_56`; the native/EDADB iTO pre-tool gate passes first. The detailed duplicate net-alias
evidence and why no speculative fix is accepted are recorded in `../../docs/stage-validation/README.md`.
