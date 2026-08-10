# EDADB Stage Validation Tests

This directory validates that a point tool consumes an EDADB-restored iDB in the same way as a native DEF-loaded iDB. It is separate from the object-level roundtrip regression in `../run_idb_roundtrip_regression.sh`.

## Current Scope

- Branch baseline: `edadb-idb-dev/sort-abc-no-sort-d` milestone implementation.
- Dataset: `sky130_gcd`.
- Isolated stages: `ipl`, `icts`, `ito_drv`, `ito_hold`, `ipl_lg`, `irt`.
- Native controls: three runs by default.
- EDADB controls: one run when native results are stable and equal; three runs after variability or mismatch.
- Resource-aware scheduling: process concurrency is bounded by both CPU capacity and `MemAvailable`; iCTS, iTO, and iRT remain serial because each tool already uses many worker threads.
- iRT wrapper gate: native and EDADB paths must produce identical wrapped die, obstacle, and pin-shape environments before routing starts.

## Run

From the repository root:

```bash
bash src/database/edadb/test/stage_validation/run_stage_validation.sh ipl
bash src/database/edadb/test/stage_validation/run_stage_validation.sh icts ito_drv ito_hold
bash src/database/edadb/test/stage_validation/run_stage_validation.sh irt
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

For iRT input-state diagnosis, set `RT_ENABLE_NOTIFICATION=1 RT_SNAPSHOT_ONLY=1`. Normal runs leave both disabled. The options ask iRT to emit its wrapped environment JSON under each run's `rt/` directory and stop before routing; notification delivery itself may remain disabled.

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

## Current iRT Status

The generated-via defect is fixed. The strict minimal fixture reports `27 == 27`, and the full
sky130 wrapper gate reports identical native/EDADB iRT input environments with `27,299`
obstacles. Full routing reaches the repeatability review gate because three native controls
produce different routed DEFs and QoR values. Detailed evidence and the next comparison boundary
are recorded in `../../docs/stage-validation/README.md`. A single-thread diagnostic further proves
that pointer-address iteration inside iRT can produce different legal routes from semantically
identical native and EDADB inputs; this is classified separately from adapter restoration.
