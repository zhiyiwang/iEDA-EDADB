# Known Native iEDA Defects And Reproduction Cases

This document records native iEDA defects exposed by EDADB stage validation. The adapter branch
keeps original iEDA production source unchanged; these cases are evidence and upstream work
items, not local point-tool fixes.

## Policy

- First require native and EDADB pre-tool DEF/DB state to match.
- Reproduce the post-tool difference with fixed input, configuration, revision and thread count.
- Classify the first divergent native consumer before blaming the adapter.
- Keep a minimal fixture or an exact real-design command and artifact path.
- Fix adapter defects locally; only document native iEDA defects on this branch.

## 1. iPL Pointer-Ordered Connectivity

### Source evidence

`NesterovPlace::completeConnection()` iterates the pointer-keyed `_nPin_map` at
`src/operation/iPL/source/module/global_placer/electrostatic_placer/NesterovPlace.cc:252`.
Allocation addresses therefore select the append order of instance-pin and net-loader vectors
used by placement calculations.

### Real test case

- Dataset: IHP130 AES.
- Input: isolated `iNO_fix_fanout_result.def`.
- Native and EDADB pre-tool DEFs: byte-identical.
- Observation: three native runs converge to one stable result, while three EDADB runs converge
  to another; one-thread execution retains the split.
- First observed numerical difference: two DBU of HPWL at Nesterov iteration 320.

### Reproduction

```bash
DATASET=ihp130_aes \
DATASET_RESULT_DIR=/tmp/iedadb_stage_inputs/ihp130_aes/result \
OUT_ROOT=/tmp/iedadb_known_native_ipl \
NATIVE_RUNS=3 \
STAGE_RUN_JOBS=3 \
bash src/database/edadb/test/stage_validation/run_stage_validation.sh ipl
```

Expected result on the native baseline: the pre-tool gate passes, native controls are stable,
and the EDADB post-tool result differs. Commit `eaba42801` contains the diagnostic correction that
proved the cause, but the final adapter milestone restores the original iPL source.

## 2. Verilog IO Alias Ownership And Direction

### Source evidence

- `RustVerilogRead::build_assign()` starts at
  `src/database/manager/builder/verilog_builder/verilog_read.cpp:405` and can add one IO pin to a
  second root-net container without consistently removing its old membership.
- `VerilogWriter::writeAssign()` writes both input and output aliases as `assign net = port` at
  `src/database/manager/builder/verilog_builder/verilog_write.cpp:307` and line 312.

### Minimal test case

Fixture: `src/database/edadb/test/stage_validation/fixtures/verilog/io_port_alias.v`.

```bash
EXPECT_KNOWN_NATIVE_DEFECT=1 \
OUT_DIR=/tmp/iedadb_known_native_alias \
bash src/database/edadb/test/stage_validation/test_verilog_alias_roundtrip.sh
```

The known-defect oracle is exact:

```text
assign shared = in ;
assign shared = out0 ;
assign shared = out1 ;
assign out2 = out1 ;

DEF root-net memberships:
in=2, out0=2, out1=3, out2=1
```

Strict mode omits `EXPECT_KNOWN_NATIVE_DEFECT`; it must fail on the unchanged native baseline.
Commit `ef07f23df` contains the diagnostic correction that proved the ownership and direction
causes, but the final adapter milestone restores the original Verilog builder source.

## 3. IHP130 Bound-Skew-Tree Boundary

The default IHP130 iCTS profile fails natively at
`src/operation/iCTS/source/solver/tools/tree_builder/bound_skew_tree/BoundSkewTree.cc:1600`: an
edge stores length zero while its endpoint Manhattan distance is `55.239`.

Reproduce the native failure with the default CTS configuration. For adapter comparison, use the
existing non-skew-tree implementation without modifying iCTS:

```bash
CTS_CONFIG_FILE=$PWD/src/database/edadb/test/stage_validation/config/ihp130_cts_no_skew_tree.json \
DATASET=ihp130_aes \
DATASET_RESULT_DIR=/tmp/iedadb_stage_inputs/ihp130_aes/result \
OUT_ROOT=/tmp/iedadb_known_native_cts \
bash src/database/edadb/test/stage_validation/run_stage_validation.sh icts
```

## 4. iTO Undefined Summary Fields

`ToApi::outputSummary()` creates a local `TimingOptSummary` at
`src/operation/iTO/api/ToApi.cpp:114` but does not initialize or assign `HPWL/STWL` before return.
The values can therefore differ between otherwise equivalent processes.

The test comparator excludes only `optDrv/optHold/optSetup.HPWL` and `.STWL` from semantic
comparison while retaining raw JSON. No iTO production source is changed.

## 5. iRT Pointer-Order Nondeterminism

### Source evidence

- `DataManager::getTypeLayerNetFixedRectMap()` returns pointer-keyed sets at
  `src/operation/iRT/source/data_manager/DataManager.cpp:345`.
- GCell stores fixed rectangles, access points, segments, patches and violations in pointer-keyed
  sets beginning at `src/operation/iRT/source/data_manager/advance/GCell.hpp:85`.
- Downstream algorithms iterate these sets; different allocation histories can therefore select
  different legal routes.

### Historical six-control case

- Dataset: IHP130 PicoRV32A.
- Input SHA-256: `77ca164086e2fddc030714edf91a02e83ba1751c6e3b5efc5121bf82790cd9fd`.
- Controls: three native plus three EDADB, 12 iRT threads each.
- All fixed structures match, but both groups produce different routed DEFs.
- No measured metric proves a native/EDADB group difference with three samples per group.

The deep semantic/pointer-order snapshot used during diagnosis required a temporary
`DataManager` instrumentation commit (`1e00b5940`). That instrumentation is not retained. The
active adapter milestone uses original iEDA's `env_map.json` gate and records full routing as
`REVIEW`.

## Current Conclusions

- These native defects do not prove an EDADB core or adapter defect.
- The only confirmed adapter defect in this campaign is generated-via non-idempotent
  reconstruction, fixed in `shadow_idb_via_master.h` by commit `e63ebd001`.
- Point-tool post-state equality is not an acceptance requirement when the native baseline is a
  documented known defect; pre-tool adapter equality and the relevant adapter-specific fixture
  remain mandatory.
