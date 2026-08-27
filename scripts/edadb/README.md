# iEDA + EDADB DEF Read/Write And Test Guide

This directory contains the smallest executable iEDA + EDADB DEF roundtrip demo. The reusable
adapter regression and point-tool validation frameworks live under `src/database/edadb/test/`.
This README connects those entry points and explains how LEF/DEF data reaches iEDA, EDADB, and the
test oracles.

## Scope

The current complete adapter persists and restores these DEF/iDB root families:

- Design, Die, Row, TrackGrid, GCellGrid, Via;
- Instance, Pin, Blockage, Region, Slot, Group, Fill;
- SpecialNet and Net.

LEF is not persisted by this adapter. Every process must load the same technology and cell LEFs
before it parses a DEF or restores an EDADB database. Restored objects resolve non-owning references
such as Site, Cell Master, Layer, and ViaRule by name against that already-loaded LEF database.

Detailed class-by-class adapter semantics are documented in
`src/database/edadb/docs/idb-adapter/`. The test source of truth is
`src/database/edadb/test/README.md` and `src/database/edadb/test/stage_validation/README.md`.

## Directory Layout

```text
scripts/edadb/
├── README.md
├── demo/
│   ├── README.md               # stage aliases, DEF coverage, and usage boundaries
│   ├── demo.sh                 # stable milestone wrapper using sky130_gcd defaults
│   ├── tcl/                    # original demo-specific Tcl kept for compatibility
│   └── result/                 # fixed local output used by demo.sh
├── performance/
│   └── TODO.md                 # planned phase-level timing and SQLite profiling
└── roundtrip/
    ├── run.sh                  # generic native-vs-EDADB three-process runner
    ├── tcl/
    │   ├── direct_def_roundtrip.tcl
    │   ├── def2edadb.tcl
    │   └── edadb2def.tcl
    └── result/                 # timestamped local runs when RUN_DIR is not supplied

src/database/edadb/test/
├── run_idb_roundtrip_regression.sh
├── normalize_def_for_diff.py
├── test_normalize_def_for_diff.sh
├── tcl/                        # reusable direct/write/read Tcl entry points
└── stage_validation/           # native-vs-EDADB point-tool validation
```

Generated test artifacts belong under `/tmp` or another external output directory. Generated
Sky130 stage DEFs under `scripts/design/sky130_gcd/result/` are also ignored rather than committed;
the reproducible build and flow commands are documented in `scripts/edadb/demo/README.md`.

## Tcl Directory Roles

Several Tcl directories exist because they have different ownership and abstraction levels:

| Directory | Responsibility | Dataset-specific? | Used by |
| --- | --- | --- | --- |
| `scripts/edadb/demo/tcl/` | Original two-step demo with `READ_DEF`, `WRITE_EDADB`, and `READ_EDADB` flags. | Assumes the environment established by the sky130 demo. | Legacy/direct callers; the current `demo.sh` delegates to `roundtrip/run.sh`. |
| `scripts/edadb/roundtrip/tcl/` | User-facing native baseline, EDADB write, and fresh EDADB read operations. | No hard-coded PDK; obtains LEF initialization from `DESIGN_TCL_SCRIPT_DIR`. | `scripts/edadb/roundtrip/run.sh` and the current demo wrapper. |
| `src/database/edadb/test/tcl/` | Regression-owned equivalents used by the 15-case automated test suite and stage writer. | No hard-coded PDK; receives the profile through environment variables. | `run_idb_roundtrip_regression.sh` and `run_stage_validation.sh`. |
| `scripts/design/<dataset>/script/DB_script/` | Defines PDK paths and loads LEF, LIB, SDC, and SPEF. | Yes. This is the dataset/PDK binding layer. | Demo, roundtrip, and point-tool scripts. |
| `scripts/design/<dataset>/script/i*_script/` | Runs physical-design stages such as iFP, iPL, iCTS, iTO, and iRT. | Yes. Tool configuration and default stage inputs are design-specific. | Full iEDA design flow and stage validation. |
| `src/database/edadb/test/stage_validation/tcl/` | Runs one selected point tool through either native DEF or EDADB input. | Mostly generic; profile supplies files, layers, and configuration. | Native-vs-EDADB stage validation. |

The EDADB Tcl files do not define a PDK. They call the design profile's `DB_script` to load one.
Conversely, the design profile Tcl files know how to initialize a PDK and point tool but do not
implement EDADB persistence.

## What `sky130` And `sky130_gcd` Mean

`sky130` is the open SkyWater 130 nm PDK data used by this repository. Its physical and timing
inputs are under:

```text
scripts/foundry/sky130/
├── lef/       # technology, standard-cell, IO, and SRAM LEFs
├── lib/       # timing libraries
├── sdc/       # constraints
└── spef/      # parasitics used by the example flow
```

GCD means **Greatest Common Divisor**. It is the small digital benchmark circuit implemented by
this example flow. `sky130_gcd` is a dataset/profile, not a PDK name. It combines the GCD design,
sky130 technology, iEDA configurations, Tcl flow scripts, netlist, and intermediate DEF files:

```text
scripts/design/sky130_gcd/
├── iEDA_config/
├── script/
└── result/
```

The current profile selects the `sky130_fd_sc_hs` high-speed standard-cell library. Object-level
regression uses:

- `scripts/design/sky130_gcd/result/iPL_result.def` as the placed base DEF;
- `scripts/design/sky130_gcd/result/iRT_result.def` as the routed base DEF.

## EDA Input Model

The test framework follows the same abstraction used by a physical-design flow:

```text
RTL
  -> synthesis
  -> gate-level netlist (logical instances and connectivity)
  -> floorplan/placement/CTS/routing
  -> progressively richer DEF snapshots (physical design state)

PDK
  -> technology LEF (layers, sites, vias, design rules)
  -> cell LEF (master geometry, pins, obstructions)
  -> Liberty (timing and power models)

SDC (timing intent) + SPEF (extracted parasitics)
  -> timing-driven tools and analysis
```

These are different views of one design:

- **Netlist** is normally the synthesis front-end output. It names the logical cells, instances,
  ports, and nets. Later tools may add buffers and reconnect nets, producing an updated netlist.
- **LEF/PDK** constrains how that logic may be physically implemented: legal layers, sites, cell
  dimensions, pin geometry, spacing, vias, and routing rules.
- **DEF** is a physical-state snapshot of one design stage. It combines references to LEF masters
  with die/row/grid, placement, physical pins, blockages, nets, and optionally routing geometry.
- **SDC** states timing intent such as clocks, uncertainty, input/output delay, false paths, and
  timing exceptions. It is design-specific, not a universal property of a PDK.
- **SPEF** describes parasitic resistance and capacitance extracted for a particular implemented
  netlist and physical state. It cannot safely be reused after unrelated placement/routing changes.
- **iEDA JSON configuration** selects algorithms, legal cells/layers, limits, threads, and other
  tool policies. It must agree with the PDK and intended stage.

The current pure DEF roundtrip loads only LEF and DEF. Some design-profile path scripts still read
the netlist/SDC/SPEF environment variable names while defining all flow paths; the generic runner
supplies inert `/dev/null` defaults for that declaration step. It never sources the LIB, SDC, SPEF,
or netlist initialization Tcl. Real matching inputs become required only when a point tool or timing
analysis is executed.

## How LEF And DEF Reach iEDA

### 1. Bash selects a dataset

The shell entry point exports at least:

```text
DESIGN_TCL_SCRIPT_DIR  directory containing DB_script/
FOUNDRY_DIR            PDK data root
INPUT_DEF              input DEF
OUTPUT_DEF             output DEF, when applicable
EDADB_DB_PATH          SQLite EDADB file
```

For sky130 GCD, `DESIGN_TCL_SCRIPT_DIR` points to
`scripts/design/sky130_gcd/script`, and `FOUNDRY_DIR` points to
`scripts/foundry/sky130`.

### 2. Tcl loads LEF

Every generic roundtrip Tcl script first sources:

```tcl
source $::env(DESIGN_TCL_SCRIPT_DIR)/DB_script/db_path_setting.tcl
source $::env(DESIGN_TCL_SCRIPT_DIR)/DB_script/db_init_lef.tcl
```

For sky130, `db_path_setting.tcl` derives `TECH_LEF_PATH` and `LEF_PATH` from `FOUNDRY_DIR`.
`db_init_lef.tcl` then executes:

```tcl
tech_lef_init -path $TECH_LEF_PATH
lef_init -path $LEF_PATH
```

### 3. Tcl chooses the native or EDADB data path

Native baseline:

```text
LEF -> def_init(INPUT_DEF) -> active iDB -> def_save(OUTPUT_DEF)
```

EDADB write:

```text
LEF -> def_init(INPUT_DEF) -> active iDB -> edadb_write(EDADB_DB_PATH)
```

EDADB read in a fresh process:

```text
LEF -> edadb_read(EDADB_DB_PATH, reference INPUT_DEF) -> active iDB
    -> def_save(OUTPUT_DEF)
```

`INPUT_DEF` remains a required `edadb_read` argument. On the complete adapter branch, DEF callbacks
for EDADB-restored root families are not registered, so those objects come from the database rather
than being duplicated from DEF text. The reference path still participates in the existing iDB DEF
service/read lifecycle.

## C++ Program Call Chain

The Tcl commands are registered as `edadb_write` and `edadb_read` in
`src/interface/tcl/tcl_idb/tcl_register_idb.h`.

Write path:

```text
edadb_write
-> CmdEdadbWrite::exec()
-> DataManager/IdbBuilder::saveDefToEdadb()
-> DefWriteEdadb::writeDb2Edadb()
-> idb::edadb_adapter::initWriteDb()
-> create EDADB schema/tables
-> writeChip2Edadb()
-> writeIdbDesign() ... writeIdbNet()
```

Read path:

```text
edadb_read
-> CmdEdadbRead::exec()
-> DataManager/IdbBuilder::buildDefFromEdadb()
-> DefReadEdadb::createDbFromEdadb()
-> idb::edadb_adapter::initReadDb()
-> map existing EDADB schema/tables
-> createDbByEdadb()
-> readIdbDesign() ... readIdbNet()
-> createDbByDef(reference DEF) with migrated callbacks disabled
-> buildNet()/buildBus()
```

The schema is declared in `src/database/edadb/idb/edadb_idb_schema.h`; table initialization and
primary-key policy are in `src/database/edadb/idb/edadb_idb_init.cpp`; Shadow conversion is under
`src/database/edadb/idb/shadow/`.

## Prerequisites And Build

Required runtime tools include `bash`, `python3`, `sqlite3`, and the built `bin/iEDA`. The Sky130
demo additionally requires the Sky130 PDK/profile and generated stage DEFs. Generate those DEFs by
following `scripts/edadb/demo/README.md`; do not commit the files under the design result directory.

Full clean rebuild from the repository root:

```bash
bash build.sh -d -n -y
bash build.sh -j40
```

The EDADB submodule revision used by iEDA is the gitlink recorded by the current iEDA branch. Align
it before building:

```bash
git submodule update --init --recursive
git submodule status src/database/edadb/core
```

An untracked `src/database/edadb/core/build/` directory makes the superproject report
`src/database/edadb/core (untracked content)` but does not change the recorded EDADB source revision.

## Smoke Demo

The established command can still be run from `bin/`:

```bash
cd bin
bash /home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/demo/demo.sh \
  2>&1 | tee run.out
```

Default input:

```text
scripts/design/sky130_gcd/result/iPL_filler_result.def
```

Use `demo.sh --list` to select another existing flow stage such as `irt` or `filler`. Detailed
aliases, stage meanings and coverage boundaries are documented in `scripts/edadb/demo/README.md`.

`demo.sh` is now a compatibility wrapper around `roundtrip/run.sh`. The runner performs three
independent iEDA invocations:

1. load sky130 LEF and the input DEF, then write native canonical `direct.def`;
2. load sky130 LEF and the same input DEF, then create `edadb.db`;
3. load sky130 LEF again, restore that database, and save `edadb.def`;
4. compare `direct.def` with `edadb.def` exactly; every raw DEF difference currently fails;
5. report the original input versus `direct.def` difference as native-writer canonicalization, not
   automatically as an adapter failure.

Default demo artifacts are under `scripts/edadb/demo/result/`:

```text
direct.def
edadb.def
edadb.db
direct.log
def2edadb.log
edadb2def.log
input_vs_direct.diff                  # only when native iEDA canonicalizes input
direct_vs_edadb.diff                  # only when raw adapter comparison differs
```

The original `demo/tcl` flag semantics are `1 = enabled`:

```text
READ_DEF=1       parse INPUT_DEF before EDADB write
WRITE_EDADB=1    write EDADB_DB_PATH
READ_EDADB=1     restore EDADB_DB_PATH
```

Use another DEF with the same sky130 LEF universe:

```bash
cd bin
bash ../scripts/edadb/demo/demo.sh /absolute/path/to/new_sky130.def
```

This succeeds only when every DEF layer, site, macro, and via rule can be resolved from the loaded
sky130 LEFs.

Run the generic entry directly when selecting another profile or output directory:

```bash
RUN_DIR=/tmp/my_def_roundtrip \
DESIGN_PROFILE_DIR=/path/to/design/profile \
FOUNDRY_DIR=/path/to/matching/pdk \
bash scripts/edadb/roundtrip/run.sh /absolute/path/to/input.def
```

## Existing Sky130 GCD DEF Inputs

The repository contains a stage sequence under `scripts/design/sky130_gcd/result/`. Use the stage
that contains the state required by the adapter path under review; a later filename is not
automatically broader in every DEF section.

| Input DEF | Physical stage represented | Additional adapter coverage |
| --- | --- | --- |
| `iFP_result.def` | Floorplan/PDN output | Baseline Die/Row/Track/Via/Instance/Pin/Net/SpecialNet state. |
| `iPL_result.def` | Global placement output | Placed instance coordinates and orientations; current smoke-demo default. |
| `iCTS_result.def` / `iTO_*_result.def` | CTS and timing-optimization outputs | Added clock/optimization instances and nets. |
| `iRT_result.def` | Detailed-routing output | Six GCellGrid records plus 677 routed Nets, 8997 regular-wire segments and 3716 via references. |
| `iPL_filler_result.def` | Routed design after filler-cell insertion | Same routed/GCell coverage as `iRT_result.def`, with 2604 Components instead of 1460. |

For the broadest existing integration input, use `iPL_filler_result.def`. Use `iRT_result.def` when
the test should isolate routed Net/Wire/Via restoration without the extra filler-cell instances.
The `run_filler` result adds physical-only **Components**; it does not create the DEF `FILLS`
section, so `IdbFill` still requires the focused `aux_optional`/Fill fixture. These repository DEFs
also have zero Blockage, Region, Slot and Group roots. The focused regression cases remain necessary
for those tags and for optional syntax branches that the full GCD flow does not emit.

Validated with the current complete adapter:

```bash
RUN_DIR=/tmp/iedadb_irtroundtrip_current \
bash scripts/edadb/roundtrip/run.sh scripts/design/sky130_gcd/result/iRT_result.def

RUN_DIR=/tmp/iedadb_fillerroundtrip_current \
bash scripts/edadb/roundtrip/run.sh scripts/design/sky130_gcd/result/iPL_filler_result.def
```

Both native canonical DEF outputs matched their EDADB-restored outputs exactly. The logs confirmed
that `writeIdbNet()` and `readIdbNet()` handled all 677 Nets and 8997 segments; the reference DEF
path remains part of the builder lifecycle but migrated root families, including Net, are restored
from EDADB.

## Test Framework Overview

The tests form a validation ladder. A higher layer does not replace the lower layers.

### Layer 1: EDADB core tests

These tests validate EDADB transactions, CRUD, query operators, recursive storage, Shadow callback
contracts, vector indices, nulls, replacement, and failure behavior independently of iEDA:

```bash
cd src/database/edadb/core/build
ctest --output-on-failure
```

### Layer 2: DEF normalizer tests

The normalizer has standalone tests for future order-insensitive comparisons, but it is currently
disabled in `scripts/edadb/roundtrip/run.sh`. The active roundtrip oracle requires exact DEF text:

```bash
bash src/database/edadb/test/test_normalize_def_for_diff.sh
```

### Layer 3: object/schema/field roundtrip regression

Run from the repository root:

```bash
OUT_DIR=/tmp/iedadb_regression \
bash src/database/edadb/test/run_idb_roundtrip_regression.sh
```

Each case owns a separate directory, SQLite database, logs, and iEDA processes. For every case the
framework performs:

```text
direct path:  LEF + DEF -> iDB -> DEF
write path:   LEF + DEF -> iDB -> EDADB
read path:    LEF + EDADB -> reconstructed iDB -> DEF
oracle:       direct DEF vs EDADB DEF + SQL/schema/field/log assertions
```

Selected cases also physically reorder SQLite root or child rows to prove that explicit order/index
restoration works independently of database fetch order.

The current cases are:

```text
default_ipl          design_fields       design_fallback
die_polygon          aux_optional        pin_derived
pin_writer           pin_branches        group_branches
special_net_branches grid_branches       via_branches
instance_branches    routed_irt          net_branches
```

Most branch fixtures are generated at runtime from the sky130 GCD placed or routed DEF. They test
optional parser branches without committing another complete design database.

Run selected cases:

```bash
EDADB_TEST_JOBS=2 OUT_DIR=/tmp/iedadb_selected \
bash src/database/edadb/test/run_idb_roundtrip_regression.sh via_branches net_branches
```

Automatic process concurrency is bounded by logical CPU count and `MemAvailable`. Override
`EDADB_TEST_JOBS`, `EDADB_TEST_PROCESS_MEMORY_GIB`, and `EDADB_TEST_MEMORY_RESERVE_GIB` only after
measuring the selected dataset.

### Layer 4: point-tool stage validation

Stage validation proves that EDADB-restored iDB state can be consumed and modified by iEDA tools,
not merely written back as similar DEF text. It supports these profiles:

```text
sky130_gcd
ihp130_aes
ihp130_picorv32a
```

And these isolated stages:

```text
ipl -> icts -> ito_drv -> ito_hold -> ipl_lg -> irt
```

For one stage, the framework:

1. loads the same LEF and input DEF natively and saves a pre-tool DEF/report;
2. writes the input DEF to EDADB;
3. restores EDADB in a fresh process and saves the same pre-tool outputs;
4. stops if native and EDADB pre-tool state differ;
5. for iRT, compares the wrapped `env_map.json` before routing;
6. runs three native controls and one EDADB control;
7. expands to three EDADB controls if native variability or a mismatch is observed;
8. compares normalized DEF, normalized Verilog, DB reports, feature JSON, and stable summary fields.

Examples:

```bash
bash src/database/edadb/test/stage_validation/run_stage_validation.sh ipl
bash src/database/edadb/test/stage_validation/run_stage_validation.sh icts ito_drv ito_hold
IRT_INPUT_GATE_ONLY=1 \
bash src/database/edadb/test/stage_validation/run_stage_validation.sh irt
```

IHP130 inputs are generated outside the repository before validation:

```bash
DATASET=ihp130_aes PREPARE_THROUGH=ipl \
bash src/database/edadb/test/stage_validation/prepare_ihp130_stage_inputs.sh

DATASET=ihp130_aes \
DATASET_RESULT_DIR=/tmp/iedadb_stage_inputs/ihp130_aes/result \
bash src/database/edadb/test/stage_validation/run_stage_validation.sh ipl
```

### Layer 5: focused first-principles fixtures

The generated-via fixture contains four real sky130 generated vias reduced from the GCD flow. It
checks the exact physical oracle `19 cuts + 8 enclosures = 27 obstacles` at the iRT input boundary:

```bash
bash src/database/edadb/test/stage_validation/run_generated_via_fixture.sh
```

Focused fixtures should be used when DEF output cannot expose an incorrect derived iDB state.

## Using A New Dataset

Choose the smallest integration level that matches the new data.

### Case A: another DEF using the existing sky130 PDK

Start with the demo or generic Tcl scripts; no source modification is required. The new DEF must
reference only objects available in the currently loaded sky130 technology and cell LEFs.

Changing only `INPUT_DEF` is sufficient for the **pure native-vs-EDADB roundtrip** when all of the
following are true:

- the DEF syntax is supported by the current iEDA parser;
- its `UNITS` and layer/site/via names belong to the loaded sky130 technology LEF;
- every `COMPONENTS` master exists in the loaded sky130 cell LEFs;
- every reference needed by the enabled adapter families can be rebuilt from LEF or another DEF
  root object;
- the test goal is data import/persistence/reconstruction, not running a timing or physical tool.

Changing only `INPUT_DEF` is not sufficient to prove a complete EDA stage when the DEF belongs to a
different design or stage. The old GCD netlist, SDC, SPEF, and JSON settings may then describe a
different logical design or physical state.

Recommended gate order:

1. native `DEF -> DEF`;
2. `DEF -> EDADB -> DEF`;
3. inspect EDADB root and child-table counts;
4. compare native and EDADB pre-tool `report_db` state;
5. run the first consuming point tool.

Do not replace `BASE_DEF` in `run_idb_roundtrip_regression.sh` and assume the full suite is portable:
its SQL assertions contain sky130 GCD object names, counts, and feature-specific expectations.

### Checking DEF Against LEF Before Testing

The definitive compatibility test is native iEDA loading with the intended LEF profile:

```text
load Tech LEF + Cell LEF
-> def_init(INPUT_DEF)
-> def_save(direct.def)
-> inspect errors and unresolved references
```

Static checks should additionally cover:

- every DEF `COMPONENTS` macro name exists as a LEF `MACRO`;
- every DEF `ROW` site exists as a LEF `SITE`;
- every `TRACKS`, pin geometry, routed wire, and blockage layer exists as a LEF `LAYER`;
- every generated ViaRule and referenced fixed Via exists in LEF or the DEF `VIAS` section;
- DEF `UNITS DISTANCE MICRONS` agrees with the database/technology scale;
- orientation and geometry are legal for the referenced Site/Macro.

Text search can find candidate names, but successful native parsing and object-level checks are the
acceptance oracle because LEF files may be merged and names may occur in comments or other scopes.

### Case B: another design using an existing PDK

Add a dataset profile to the `case "$DATASET"` block in
`src/database/edadb/test/stage_validation/run_stage_validation.sh`. Define:

```text
WORKSPACE               design workspace
DATASET_RESULT_DIR      directory containing stage DEFs
CONFIG_DIR              iEDA JSON configuration directory
FOUNDRY_DIR             PDK root
TCL_SCRIPT_DIR          design Tcl root
DESIGN_TOP              top module
NETLIST_FILE            gate-level netlist
SDC_FILE                timing constraints
SPEF_FILE               optional parasitics
IPL_INPUT_DEF           placement-stage input name
RT_BOTTOM_LAYER         lowest routing layer used by iRT
RT_TOP_LAYER            highest routing layer used by iRT
```

If intermediate DEFs do not already exist, extend or generalize
`src/database/edadb/test/stage_validation/prepare_ihp130_stage_inputs.sh` with the new top name,
netlist, die/core bounds, and any dataset-specific configurations.

### Case C: a new PDK

Use a profile-compatible layout:

```text
scripts/foundry/<pdk>/
├── lef/
├── lib/
├── sdc/
└── spef/

scripts/design/<dataset>/
├── iEDA_config/
├── script/
│   └── DB_script/
│       ├── db_path_setting.tcl
│       ├── db_init_lef.tcl
│       ├── db_init_lib.tcl
│       ├── db_init_lib_drv.tcl
│       ├── db_init_lib_hold.tcl
│       └── db_init_sdc.tcl
└── result/
```

Before EDADB testing, verify:

- Tech LEF defines every DEF routing/cut layer and compatible DBU;
- cell LEFs define every `COMPONENTS` master;
- LEF contains every Site and ViaRule referenced by DEF;
- timing libraries and SDC match the netlist and point-tool configuration;
- configured CTS/TO cells exist;
- iRT bottom/top layer names match the PDK.

If this interface is preserved, the generic roundtrip Tcl files and
`stage_validation/tcl/run_stage.tcl` normally require no modification.

## Point-Tool Input Requirements

The table lists the inputs used by the current sky130 Tcl scripts. "DEF" always means a stage-
appropriate DEF, not an arbitrary snapshot from the same design.

| Tool/stage | Required physical/logical inputs | Why |
| --- | --- | --- |
| DEF/EDADB roundtrip | Tech LEF, Cell LEF, DEF | Resolve physical references and compare iDB persistence; no point tool runs. |
| iFP floorplan | Tech/Cell LEF, synthesized gate-level netlist, top name, die/core bounds, floorplan/PDN configuration | Creates the first physical design, rows, tracks, IO placement, tap/endcap cells, and PDN. It does not start from a completed placement DEF. |
| iPL placement/legalization | LEF, Liberty, SDC, placement-stage DEF, placement JSON | Moves/legalizes instances while estimating timing, wirelength, density, and congestion. The DEF connectivity must match the loaded timing/library view. |
| iCTS | LEF, Liberty, SDC with a valid clock, placed DEF, CTS JSON and legal clock cells | Inserts clock buffers/nets and builds the clock tree according to the clock definition and library delays. |
| iTO DRV/hold/setup | LEF, stage-specific Liberty, SDC, post-CTS/optimization DEF, TO JSON and legal optimization cells | Performs timing-driven buffer insertion, resizing, and reconnection. |
| iSTA | Liberty, SDC, LEF/DEF or netlist-based design state; SPEF when post-route RC is required | Computes timing using logical arcs, constraints, and optionally extracted parasitics. |
| iRT | Tech/Cell LEF, legal placed DEF with pins/nets/tracks/gcells, routing-layer bounds, router settings; Liberty/SDC only for timing-aware integration/reporting | Creates physical wire/via geometry. The current sky130 script initializes LIB/SDC as part of the common flow even though geometric routing fundamentally depends on LEF/DEF. |
| iDRC | Tech LEF design rules and routed DEF | Checks physical wire/via/shape geometry; current script does not load Liberty or SDC. |
| iPNP/power analysis | LEF, Liberty, SDC, suitable DEF; SPEF/RC and activity/current information according to analysis mode | Estimates power/IR/noise from the implemented design and electrical models. |

Consequences when replacing only a DEF:

- Pure roundtrip can be valid with only matching LEF and DEF.
- iDRC may be valid for a matching routed DEF and Tech LEF without netlist/SDC.
- iPL/iCTS/iTO/iSTA cannot safely reuse GCD netlist or SDC for an unrelated design.
- SPEF is tied to both logical net names and one physical implementation; reusing it with another
  DEF can silently produce meaningless timing results.
- A DEF from the same design but the wrong stage may still be invalid input: for example, iCTS needs
  placed instances, while iDRC expects routed geometry for meaningful coverage.

## Current Framework Boundaries

The framework is correct for adapter validation, but it has explicit limits:

1. The generic roundtrip proves native-vs-EDADB reconstruction, not that the original downloaded
   DEF is already canonical or that every LEF/DEF standard section is supported.
2. The 15-case object regression is intentionally tied to sky130 GCD names, counts, and generated
   fixtures. A new dataset needs its own SQL oracle rather than replacing one path variable.
3. Pure DEF diff cannot expose all derived iDB state. Generated-via geometry required a focused iRT
   boundary fixture even when text roundtrip looked correct.
4. Point-tool validation needs stable native controls. A post-tool mismatch is not an adapter bug
   when native runs themselves produce different legal results.
5. The current sky130 `db_path_setting.tcl` reads `SDC_FILE` while defining all paths even when only
   LEF initialization follows. The generic runner supplies inert defaults to satisfy that profile
   interface; this must not be confused with loading or validating those files.

## Adding Object-Level Assertions For A New Dataset

Keep the existing sky130 GCD regression as a stable baseline. For a new dataset:

1. add a dataset profile instead of overwriting sky130 paths;
2. define dataset-specific expected object counts and stable named-object probes;
3. separate generic schema assertions from design-specific SQL assertions;
4. keep each generated fixture derived from one documented base DEF;
5. add a first-principles README for any minimized fixture;
6. verify native parsing first so an original iEDA failure is not mislabeled as an adapter defect.

At minimum, check:

- root object counts and unique identities;
- nested child counts and owner foreign keys;
- name-based LEF/iDB reference reconstruction;
- ordered nested vectors where the iDB semantics require order;
- parser-derived fields that are intentionally recomputed rather than persisted;
- output reparsing through the original DEF parser;
- at least one point-tool boundary that consumes the restored object family.

## Output And Failure Diagnosis

For object regression, inspect:

```text
/tmp/iedadb_regression/<case>/direct.log
/tmp/iedadb_regression/<case>/def2edadb.log
/tmp/iedadb_regression/<case>/edadb2def.log
/tmp/iedadb_regression/<case>/direct_vs_edadb.diff
/tmp/iedadb_regression/<case>/edadb.db
```

For stage validation, every process writes an independent result directory and a `manifest.json`
containing branch/commit, dirty state, input/config hashes, command, host resources, and exit status.

Classify failures in this order:

1. LEF/DEF compatibility or missing input;
2. native iEDA parse/write failure;
3. EDADB schema/write failure;
4. EDADB read/reference-rebuild failure;
5. pre-tool semantic mismatch;
6. point-tool native nondeterminism;
7. stable native versus EDADB post-tool mismatch.

Do not call a point-tool mismatch an adapter bug until the native controls are stable, the pre-tool
gate passes, the first divergence is localized, and the cause is supported by source and a focused
failing fixture.

## Recommended Reading Order

1. `scripts/edadb/demo/demo.sh`
2. `scripts/edadb/demo/tcl/def2edadb.tcl`
3. `scripts/edadb/demo/tcl/edadb2def.tcl`
4. `src/database/edadb/test/tcl/`
5. `src/database/edadb/test/run_idb_roundtrip_regression.sh`
6. `src/database/edadb/test/stage_validation/run_stage_validation.sh`
7. `src/database/edadb/test/stage_validation/tcl/run_stage.tcl`
8. `src/database/edadb/docs/idb-adapter/`
