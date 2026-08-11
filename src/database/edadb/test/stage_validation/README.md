# EDADB Stage Validation Tests

This directory validates that a point tool consumes an EDADB-restored iDB in the same way as a native DEF-loaded iDB. It is separate from the object-level roundtrip regression in `../run_idb_roundtrip_regression.sh`.

## Current Scope

- Branch baseline: `edadb-idb-dev/sort-abc-no-sort-d` milestone implementation.
- Dataset profiles: `sky130_gcd`, `ihp130_aes`, and `ihp130_picorv32a`.
- Isolated stages: `ipl`, `icts`, `ito_drv`, `ito_hold`, `ipl_lg`, `irt`.
- Native controls: three runs by default.
- EDADB controls: one run when native results are stable and equal; three runs after variability or mismatch.
- Resource-aware scheduling: process concurrency is bounded by CPU capacity and `MemAvailable`; long controls may run concurrently only when every native/EDADB process receives the same internal thread count and writes to an independent output directory.
- Native iEDA source remains identical to the adapter milestone; known point-tool/Verilog defects are reproduced and documented, not fixed on this branch.
- iRT wrapper gate: native and EDADB paths compare original iEDA's `env_map.json` before routing starts.

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

On the current 40-logical-CPU, 125-GiB host, the CPU cap is three concurrent iPL controls. The actual count is reduced when available memory cannot preserve the default 16-GiB host reserve plus 16 GiB per iEDA process. Override the estimates with `IEDA_PROCESS_MEMORY_GIB` and `IEDA_MEMORY_RESERVE_GIB` only after measuring the dataset.

For an expensive iRT repeatability run, EDADB controls can run with the native controls. The EDADB file is complete before this batch begins; all controls read the same fixed input and write separate result directories. When native routing is already known to vary, run all three EDADB controls up front so a second routing batch is unnecessary:

```bash
NATIVE_RUNS=3 STAGE_RUN_JOBS=3 PARALLEL_FIRST_EDADB=1 EDADB_RUNS_UPFRONT=3 RT_THREAD_NUMBER=12 \
bash src/database/edadb/test/stage_validation/run_stage_validation.sh irt
```

Do not combine process concurrency with the former 64-thread default: `6 x 64` oversubscribes this host. The 12-thread setting intentionally allows modest oversubscription because static DR-box scheduling leaves many workers at an OpenMP barrier near each batch tail. Measure wall time and CPU time before changing the per-process partition because some iRT phases remain only partially parallel.

The [generated-via fixture](fixtures/sky130_generated_via/README.md) provides a separate,
first-principles iRT input test:

```bash
bash src/database/edadb/test/stage_validation/run_generated_via_fixture.sh
```

This strict mode requires native/EDADB equality. Set `EXPECT_KNOWN_DEFECT=1` only with a pre-fix
binary to reproduce the historical `27 -> 65` signature; that mode must fail for the fixed
implementation.

The minimal Verilog alias fixture records the original iEDA defect without patching it:

```bash
EXPECT_KNOWN_NATIVE_DEFECT=1 \
bash src/database/edadb/test/stage_validation/test_verilog_alias_roundtrip.sh
```

The known-defect mode requires the stable baseline signature: reversed output assignments and
root-net membership counts `in=2`, `out0=2`, `out1=3`, `out2=1`. Strict mode omits the variable
and fails until the native Verilog builder is fixed upstream.

For iRT input-state diagnosis, set `RT_ENABLE_NOTIFICATION=1 RT_SNAPSHOT_ONLY=1`. Normal runs leave both disabled. Original iEDA emits `env_map.json` under each run's `rt/data_manager/` directory and stops before routing; notification delivery itself may remain disabled.

Run only the strict native-vs-EDADB iRT wrapper gate without starting full routing:

```bash
IRT_INPUT_GATE_ONLY=1 \
bash src/database/edadb/test/stage_validation/run_stage_validation.sh irt
```

The gate strictly compares die, obstacle and pin-shape records in `env_map.json`. The deeper
semantic/pointer-order snapshot used during diagnosis required temporary `DataManager` changes
and is intentionally not retained in this milestone.

Generated artifacts stay outside the repository by default. Every process writes a `manifest.json` containing the commit, branch, input/config hashes, command, host, thread setting, and exit status. A dirty worktree also records `git_status_short` and `git_diff_sha256`.

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
adapter fixes with failing-before/passing-after evidence are committed here. Native point-tool
defects are retained as reproducible evidence and must be fixed upstream, not in this branch.

`feature_tool` fields `optDrv/optHold/optSetup.HPWL` and `.STWL` are excluded from semantic comparison. `ToApi::outputSummary()` declares these fields in `TimingOptSummary` but does not initialize or assign them (`src/operation/iTO/api/ToApi.cpp:114-163`), so emitted subnormal values depend on process memory rather than design state. The raw JSON remains in every run directory as evidence.

Raw DEF differences remain available in the artifacts. The existing normalizer may reorder only Level-D root records; it never reorders A/B/C roots or nested vectors.

## Current iPL Status

The pre-tool native/EDADB gate passes. Post-tool AES and Sky130 comparisons expose the original
iPL pointer-map iteration defect. The experimental correction proved the cause, but the final
adapter milestone restores original `NesterovPlace.cc`; the stage remains a known-native-defect
reproducer rather than an adapter acceptance pass.

## Current iRT Status

The generated-via adapter defect is fixed. The strict minimal fixture reports `27 == 27`, and the
original `env_map.json` gate compares native/EDADB iRT obstacle and pin-shape environments. Full
routing remains `REVIEW` because original iRT uses pointer-address iteration and native controls
produce different legal routes.

When native iRT controls vary, the harness writes `variability_summary.json`. It records exact
fixed-structure equality plus native and EDADB samples for total/per-layer wire length, wire,
segment, via, patch, final DRC-violation counts, and iRT runtime. Each metric includes means,
sample standard deviations, standardized mean difference, and an exact two-sided permutation
value. Native observed min/max values and exploratory statistics are descriptive evidence only;
three samples are not promoted into an arbitrary acceptance tolerance and cannot yield
`p < 0.1` in the 3+3 exact test.

PicoRV32A historical diagnostic runs established the iPL, Verilog alias and iRT causes. The final
adapter milestone does not retain those native fixes: iPL and iTO DRV are known-native-defect
reproducers, stable-profile iCTS remains usable, and iRT `env_map` plus generated-via checks remain
the current adapter-facing gates. Detailed evidence is recorded in
`../../docs/stage-validation/README.md` and `../../docs/stage-validation/known-native-defects.md`.
