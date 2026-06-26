# iEDA-EDADB Handoff

This file keeps only the branch facts, EDADB layout, validation command, and current C-branch rules.

## Validation Rule

Use the iEDA superproject commit as the source of truth:

1. Checkout target iEDA branch/commit.
2. Run `git submodule update --init --recursive`.
3. Build only when validation is needed.
4. Run only the EDADB DEF roundtrip demo unless explicitly asked otherwise.

Canonical demo command:

```bash
cd bin/
pwd
bash /home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/demo/demo.sh 2>&1 | tee run.out
```

Do not run the full physical-design flow unless requested.

Repeatable EDADB adapter regression command:

```bash
cd /home/zhiyiwang/cs/arch/eda/iEDA-EDADB
bash src/database/edadb/test/run_idb_roundtrip_regression.sh
```

The regression definition is stored in `src/database/edadb/test/`.
By default it writes generated fixtures, logs, EDADB SQLite databases, direct iDB
DEF baselines, EDADB DEF outputs, and diffs to `/tmp/iedadb_regression`.
Override paths with:

```bash
IEDA_BIN=/path/to/iEDA OUT_DIR=/tmp/my_edadb_run bash src/database/edadb/test/run_idb_roundtrip_regression.sh
```

Current cases:

- `default_ipl`: sky130_gcd `iPL_result.def`, direct iDB DEF output compared with EDADB output.
- `aux_optional`: generated from `iPL_result.def`; adds non-empty `BLOCKAGES`, `REGIONS`,
  `SLOTS`, `GROUPS`, `FILLS`, plus special-net and regular-net optional fields.
- `routed_irt`: sky130_gcd `iRT_result.def`; covers non-empty regular NETS routed wires
  and segments.

The `aux_optional` case also checks SQLite table content for blockage/region/slot/group/fill
counts, group region/member rows, fill child rows, special-net optional fields, and regular-net
optional fields.
The `routed_irt` case checks SQLite counts for `iNetSD`, regular wire child rows, regular
wire segment child rows, and regular wire point child rows.

Latest run on 2026-06-24:

- Command: `bash src/database/edadb/test/run_idb_roundtrip_regression.sh`
- Output directory: `/tmp/iedadb_regression`
- Result: passed.
- `default_ipl`: direct iDB DEF output matched EDADB DEF output.
- `aux_optional`: direct iDB DEF output matched EDADB DEF output; SQLite checks passed for
  non-empty Blockage/Region/Slot/Group/Fill tables and optional regular/special net fields.
- `routed_irt`: direct iDB DEF output matched EDADB DEF output; SQLite checks passed for
  `iNetSD=677`, regular wire rows `677`, regular wire segment rows `8997`, and point rows `14256`.
- EDADB/core check at `src/database/edadb/core @ 3077132`: `cd build && ctest --output-on-failure`
  passed `13/13` tests on 2026-06-24.

When the request is about EDADB internal support or adapter correctness, also run:

```bash
cd /home/zhiyiwang/cs/arch/eda/iEDA-EDADB/build
ctest --output-on-failure
```

When validating routed NETS, do not rely only on raw input DEF vs EDADB-output DEF byte diff.
The normal iDB `def_init -> def_save` path can canonicalize routed segment endpoint order.
Use direct DEF roundtrip output as the baseline:

1. Run normal `def_init -> def_save` for the same input DEF.
2. Run `DEF -> EDADB -> DEF`.
3. Compare the two generated DEF files.
4. Treat differences between these generated files as EDADB adapter differences.

The raw input DEF byte diff is still useful for simpler cases such as default `iPL_result.def`
and `iPL_filler_result.def`, where the demo script currently passes byte-for-byte.

## 2026-06-16 Adapter Audit Notes

Recent issue found after the first green demo/CTest pass:

- `Shadow<IdbNet>` and `Shadow<IdbSpecialNet>` persisted `_source_type_sd`, but
  `DefReadEdadb` did not restore it.
- Fix: add enum setters `IdbNet::set_source_type(IdbInstanceType)` and
  `IdbSpecialNet::set_source_type(IdbInstanceType)`, then restore `_source_type_sd`
  in `readIdbNet()` and `readSpecialNet()`.
- Follow-up fix: `DefWrite::write_net()` and `DefWrite::write_special_net()` now emit
  `+ SOURCE <type>` when source type is not `kNone`.
- Validation: a routed `iRT_result.def` fixture with `+ SOURCE USER` on
  `ctrl$a_mux_sel[0]` writes `_source_type_sd=3`, reads back through EDADB, emits
  `+ SOURCE USER`, and matches the direct iDB DEF baseline exactly.

Additional issue found by focused optional-field testing:

- `DefRead::read_net()` did not copy regular-net `+ FIXEDBUMP` into `IdbNet`, even
  though EDADB already stored `_fix_bump_sd`.
- `DefWrite::write_net()` did not emit regular-net `ORIGINAL`, `WEIGHT`, `XTALK`,
  `FIXEDBUMP`, or `FREQUENCY`; `DefWrite::write_special_net()` did not emit
  special-net `ORIGINAL` or `WEIGHT`.
- Fix: restore `FIXEDBUMP` in normal DEF read, and emit these existing iDB fields from
  the DEF writer. Only emit regular-net `FREQUENCY` when it is positive, because the
  iDB default is `-1`.
- Validation: a routed fixture with special-net `VDD + SOURCE NETLIST + ORIGINAL
  orig_vdd_net + WEIGHT 5` and regular net `ctrl$a_mux_sel[0] + SOURCE USER +
  ORIGINAL orig_ctrl_net + WEIGHT 7 + XTALK 11 + FIXEDBUMP + FREQUENCY 250` writes the
  expected SQLite values and the EDADB DEF matches the direct iDB baseline exactly.

Recent issue found by routed-net testing:

- EDADB child rows for `NetPinRef` / `SpecialNetPinRef` previously used
  `instance_name` as the local key and did not preserve original instance-pin order.
- On clock nets such as `clk_0` / `clk_1`, SQLite readback sorted by key and changed
  DEF connection order relative to normal iDB output.
- Fix: add `_order_sd` to `NetPinRef` and `SpecialNetPinRef`, make it the local key,
  store vector order on write, and sort by `_order_sd` before readback reconstruction.

Current uncovered or weakly covered areas:

- Net `source_type`, `original_net_name`, `weight`, `xtalk`, `frequency`, and
  `fix_bump` now have repeatable fixture coverage in
  `src/database/edadb/test/run_idb_roundtrip_regression.sh` through direct iDB baseline
  comparison and SQLite value checks. A future adapter-level C++ test would still be useful
  for faster direct object assertions.
- New EDADB primitive vector support stores scalar vector children, such as group
  instance names and IO-pin name lists, directly as `std::vector<std::string>` using
  `___edadb_primitive_vector` child tables with `__edadb_vec_idx` and `value`.
- Empty object-family paths are validated for Blockage, Region, Slot, Group, and Fill
  on sky130_gcd. Non-empty Blockage, Region, Slot, Group, and Fill paths are now covered by
  the generated `aux_optional` regression fixture.
- Non-empty regular routed NETS are now covered by the `routed_irt` regression fixture.
- Repeated instance references with different pins on the same net should be covered
  by a small targeted fixture; `_order_sd` avoids the previous instance-key collision
  risk for net pin refs.

## EDADB Adapter Current Rules

- Match original DEF semantics first: compare `DefWrite::write_xxx()` and
  `DefRead::parse_xxx()` before changing `writeIdbXXX/readIdbXXX`.
- Persist the DEF storage view, not the whole C++ object graph.
- Prefer direct mapping; use `Shadow<T>` only for stable PK, root order, name lookup,
  vector ownership, or reconstruction views.
- Never use vector order index as PK. Keep identity and order separate:
  `primary_key` or name for identity, `_order_sd` for root list order.
- For root lists that affect DEF roundtrip, read back with `ORDER BY "_order_sd"`.
- Update schema/init, builder read/write, DEF callbacks, regression SQL, and docs together.

Recent root-order milestones:

- `IdbRowList`: `_name_sd` identity, `_order_sd` order; committed `74420696a`.
- `IdbTrackGridList`: `primary_key` identity, `_order_sd` order; committed `9679335f7`.
- `IdbGCellGridList`: `primary_key` identity, `_order_sd` order; committed `3deb1105e`.
- `IdbRegionList`: `_name_sd` identity, `_order_sd` order; committed `35e3999cf`.

Recommended next work:

1. Review/document `IdbBlockage`.
2. Then continue in DEF write order: `IdbSlot`, `IdbGroup`, `IdbFill`,
   `IdbSpecialNet`, `IdbNet`.
3. For each class, verify root/child vector order, add `_order_sd` where needed,
   extend regression SQL, run demo + `run_idb_roundtrip_regression.sh`, then commit.

## Branch Map

| Label | iEDA branch / commit | EDADB location | Adapter location | EDADB commit | Status |
| --- | --- | --- | --- | --- | --- |
| original | `master @ 007435241` | none | none | none | official iEDA, no EDADB |
| A | `origin/edadb @ 2f028c426` | `src/database/edadb/core` | `src/database/edadb/idb` | `8c724ef` | canonical non-owning layout |
| B | `origin/edadb-shadow-transitive @ 664829eef` | `src/third_party/edadb` | `src/database/edadb` | `f1214007` | old shadow-transitive layout, DEF roundtrip OK |
| C | `edadb-idb @ HEAD` | `src/database/edadb/core` | `src/database/edadb/idb` | `3077132` | current EDADB adapter line |

Notes:

- A must not be described as using `src/third_party/edadb`; that is B’s layout.
- C starts from A’s layout and is the current development line.
- B is reference-only for old DEF/EDADB mappings; do not copy shadow-transitive behavior into C.
- `IdbVia` is enabled in C after re-audit with the new EDADB implicit member StoreType path.
- C baseline EDADB `8a4e3bf` = `293c162` plus local CMake fix for embedding EDADB as an iEDA submodule.
- Current C EDADB is `3077132`, which adds primitive vector support and latest core tests on top of the earlier C baseline.

## Branch Change Inventory

Inventory basis:

- Base iEDA commit: `0074352412f6a4a8c88c13739946cdf5004f25c0` (`master`, official iEDA without EDADB).
- Current iEDA commit: `HEAD` (`edadb-idb`).
- Superproject command: `git diff --name-status 007435241..HEAD`.
- EDADB core submodule: `src/database/edadb/core @ 3077132`.
- EDADB core delta from C baseline: `git -C src/database/edadb/core diff --name-status 8a4e3bf..HEAD`.

Original iEDA files modified for EDADB integration:

- Build and module wiring:
  - `.gitmodules`
  - `CMakeLists.txt`
  - `build.sh`
  - `src/apps/CMakeLists.txt`
  - `src/database/CMakeLists.txt`
  - `src/database/manager/builder/CMakeLists.txt`
  - `src/database/manager/builder/def_builder/CMakeLists.txt`
  - `src/platform/data_manager/CMakeLists.txt`
  - `src/third_party/CMakeLists.txt`
- iDB data model headers touched to expose/preserve state needed by EDADB shadows:
  - `src/database/basic/geometry/IdbGeometry.h`
  - `src/database/basic/geometry/IdbLayerShape.h`
  - `src/database/data/design/IdbDesign.h`
  - `src/database/data/design/db_design/IdbBlockages.h`
  - `src/database/data/design/db_design/IdbBus.h`
  - `src/database/data/design/db_design/IdbBusBitChars.h`
  - `src/database/data/design/db_design/IdbFill.h`
  - `src/database/data/design/db_design/IdbGroup.h`
  - `src/database/data/design/db_design/IdbHalo.h`
  - `src/database/data/design/db_design/IdbInstance.h`
  - `src/database/data/design/db_design/IdbNet.h`
  - `src/database/data/design/db_design/IdbPins.h`
  - `src/database/data/design/db_design/IdbRegion.h`
  - `src/database/data/design/db_design/IdbSlot.h`
  - `src/database/data/design/db_design/IdbSpecialNet.h`
  - `src/database/data/design/db_design/IdbSpecialWire.h`
  - `src/database/data/design/db_design/IdbTrackGrid.h`
  - `src/database/data/design/db_design/IdbVias.h`
  - `src/database/data/design/db_layout/IdbGCellGrid.h`
  - `src/database/data/design/db_layout/IdbLayer.h`
  - `src/database/data/design/db_layout/IdbRow.h`
  - `src/database/data/design/db_layout/IdbSite.h`
  - `src/database/data/design/db_layout/IdbTerm.cpp`
  - `src/database/data/design/db_layout/IdbTerm.h`
  - `src/database/data/design/db_layout/IdbUnits.h`
  - `src/database/data/design/db_layout/IdbViaMaster.h`
- Original iEDA DEF/platform/Tcl code modified or extended:
  - `src/database/manager/builder/builder.cpp`
  - `src/database/manager/builder/builder.h`
  - `src/database/manager/builder/def_builder/def_read.cpp`
  - `src/database/manager/builder/def_builder/def_read.h`
  - `src/database/manager/builder/def_builder/def_write.cpp`
  - `src/database/manager/builder/def_builder/def_write.h`
  - `src/database/manager/service/def_service/def_service.cpp`
  - `src/interface/tcl/tcl_definition.h`
  - `src/interface/tcl/tcl_idb/tcl_db_file.cpp`
  - `src/interface/tcl/tcl_idb/tcl_db_file.h`
  - `src/interface/tcl/tcl_idb/tcl_register_idb.h`
  - `src/platform/data_manager/idm.cpp`
  - `src/platform/data_manager/idm.h`
  - `src/utility/tcl/ScriptEngine.cc`
  - `src/utility/tcl/ScriptEngine.hh`

iEDA + EDADB wrapper/adapter code added by this branch:

- Adapter root and table/shadow mapping:
  - `src/database/edadb/CMakeLists.txt`
  - `src/database/edadb/idb/CMakeLists.txt`
  - `src/database/edadb/idb/edadb_idb.h`
  - `src/database/edadb/idb/edadb_idb_helper.cpp`
  - `src/database/edadb/idb/edadb_idb_helper.h`
  - `src/database/edadb/idb/edadb_idb_init.cpp`
  - `src/database/edadb/idb/edadb_idb_init.h`
  - `src/database/edadb/idb/edadb_idb_schema.h`
  - `src/database/edadb/idb/edadb_idb_shadow.h`
  - `src/database/edadb/idb/shadow/*`
- EDADB DEF bridge:
  - `src/database/manager/builder/def_builder/def_read_edadb.cpp`
  - `src/database/manager/builder/def_builder/def_read_edadb.h`
  - `src/database/manager/builder/def_builder/def_write_edadb.cpp`
  - `src/database/manager/builder/def_builder/def_write_edadb.h`
  - `src/platform/data_manager/idm_edadb.cpp`
- Adapter notes, old call-flow notes, demos, and repeatable regression tests:
  - `agent.md`
  - `cmds.md`
  - `edadb_readme.md`
  - `scripts/edadb/demo/*`
  - `src/database/edadb/idb/docs/*`
  - `src/database/edadb/test/*` (working tree addition on 2026-06-16)

EDADB core code:

- `src/database/edadb/core` is a submodule and was not present in original iEDA.
- Current checked-out EDADB commit is `3077132 fix: enable sqlite debug trace tests`.
- Relative to the current C baseline `8a4e3bf`, EDADB core also includes const table definitions,
  primitive vector rows, updated public/table-op internals, and expanded reusable tests.
- Relative to older canonical A commit `8c724ef`, EDADB core also includes the ORM/table-op
  refactor, public API cleanup, demo refresh, and reusable core tests now visible under
  `src/database/edadb/core/include/edadb/*`, `src/database/edadb/core/src/*`,
  `src/database/edadb/core/demo/*`, and `src/database/edadb/core/test/*`.

## Current C Milestone

Current committed state:

- iEDA: `HEAD` (`fix: adapt idb adapter to latest edadb vector api`)
- EDADB submodule: `3077132 fix: enable sqlite debug trace tests`

Initial C milestone:

- iEDA: `99afe9c71 edadb: initialize idb adapter code base`
- EDADB submodule: `8a4e3bf build: support embedding edadb as submodule`

Active persistence groups:

- `writeIdbDesign()` / `readIdbDesign()` are enabled.
- `writeIdbDie()` / `readIdbDie()` are enabled through `edadb::Shadow<idb::IdbDie>`.
- `writeIdbRow()` / `readIdbRow()` are enabled directly on `IdbRow`.
- `writeIdbTrackGrid()` / `readIdbTrackGrid()` are enabled through `edadb::Shadow<idb::IdbTrackGrid>`.
- `writeIdbGCellGrid()` / `readIdbGCellGrid()` are enabled directly on `IdbGCellGrid`.
- `writeIdbVia()` / `readIdbVia()` are enabled directly on root `IdbVia`.
- `writeIdbInstance()` / `readIdbInstance()` are enabled through `edadb::Shadow<idb::IdbInstance>`.
- `writeIdbPin()` / `readIdbPin()` are enabled through `edadb::Shadow<idb::IdbPin>`.
- `writeIdbBlockage()` / `readIdbBlockage()` are enabled through `edadb::Shadow<idb::IdbBlockage>`.
- `writeIdbRegion()` / `readIdbRegion()` are enabled directly on `IdbRegion`.
- `writeIdbSlot()` / `readIdbSlot()` are enabled through `edadb::Shadow<idb::IdbSlot>`.
- `writeIdbGroup()` / `readIdbGroup()` are enabled through `edadb::Shadow<idb::IdbGroup>`.
- `writeIdbFill()` / `readIdbFill()` are enabled through `edadb::Shadow<idb::IdbFill>`.
- `writeSpecialNet()` / `readSpecialNet()` are enabled through `edadb::Shadow<idb::IdbSpecialNet>`.
- `writeIdbNet()` / `readIdbNet()` are enabled through `edadb::Shadow<idb::IdbNet>`.
- `iDesign` stores `IdbUnits` and `IdbBusBitChars` as inline columns.
- `iTrackGridSD` stores track grid scalar data and owns vector child rows for layer names.
- `iVia` stores generated via fields inline through EDADB member StoreType conversion; no `Shadow<IdbVia>` is defined.
- `iInstSD` stores DEF COMPONENT fields: instance name, cell master name, source/type, placement status, orient, weight, coordinate, HALO, ROUTEHALO, and region name.
- `iPinSD` stores DEF PINS fields: pin/net names, IO term direction/use/special, port/layer rectangles, placement status, location, orient, and derived average/bbox rebuild inputs.
- `iBlockageSD` stores only DEF writer-emitted BLOCKAGES fields: type, layer name, pushdown, exceptpgnet, component name, and rects.
- `iRegion` stores DEF REGIONS fields: name, type, and boundary rects.
- `iSlotSD` stores DEF SLOTS fields: layer name and rects.
- `iGroupSD` stores DEF GROUPS fields: group name, region name, and instance names.
- `iFillSD` stores DEF FILLS fields through layer/via member shadows: layer name + rects, via name + coordinates.
- `iSpecNetSD` stores DEF SPECIALNETS fields: net name, USE/connect type, pins, wires, and wire segments through nested child tables.
- `iNetSD` stores DEF NETS fields: net name, USE/connect type, IO pins, instance pins, and regular wire segments through nested child tables.
- Disabled matching DEF parser callbacks in `DefReadEdadb::createDbByDef()`:
  - `defrSetVersionStrCbk`
  - `defrSetDesignCbk`
  - `defrSetUnitsCbk`
  - `defrSetBusBitCbk`
  - `defrSetDieAreaCbk`
  - `defrSetRowCbk`
  - `defrSetTrackCbk`
  - `defrSetGcellGridCbk`
  - `defrSetViaStartCbk`
  - `defrSetViaCbk`
  - `defrSetComponentCbk`
  - `defrSetComponentStartCbk`
  - `defrSetComponentEndCbk`
  - `defrSetPinCbk`
  - `defrSetPinEndCbk`
  - `defrSetStartPinsCbk`
  - `defrSetBlockageCbk`
  - `defrSetRegionCbk`
  - `defrSetSlotCbk`
  - `defrSetGroupCbk`
  - `defrSetFillStartCbk`
  - `defrSetFillCbk`
  - `defrSetSNetStartCbk`
  - `defrSetSNetCbk`
  - `defrSetSNetEndCbk`
  - `defrSetNetStartCbk`
  - `defrSetNetCbk`
  - `defrSetNetEndCbk`
- Remaining unsupported/non-DEF object data still comes from existing iEDA flows or defaults.

Validation:

- `cmake --build build -j40 --target db_edadb def_builder iEDA` passed.
- Canonical demo passed on 2026-06-12.
- Demo logs show `writeIdbDesign insert name=gcd version=5.8 micron_dbu=1000`.
- Demo logs show `readIdbDesign restored name=gcd version=5.8 micron_dbu=1000`.
- Demo logs show `writeIdbDie insert point_count=2`.
- Demo logs show `readIdbDie restored point_count=2`.
- Demo logs show `writeIdbRow insert row_count=39`.
- Demo logs show `readIdbRow restored row_count=39`.
- Demo logs show `writeIdbTrackGrid insert track_grid_count=12`.
- Demo logs show `readIdbTrackGrid restored track_grid_count=12 layer_ref_count=12`.
- Demo logs show `writeIdbGCellGrid insert gcell_grid_count=0`.
- Demo logs show `readIdbGCellGrid restored gcell_grid_count=0`.
- Demo logs show `writeIdbVia insert via_count=4`.
- Demo logs show `readIdbVia restored via_count=4`.
- SQLite content check: `select _design_name, _version, _units__micron_dbu, char(_bus_bit_chars__left_delimiter), char(_bus_bit_chars__right_delimiter) from iDesign;` returns `gcd|5.8|1000|[|]`.
- SQLite die check: `iDieSD` has 1 row and `iDieSD_points_sd_iCoordSD` has 2 rows: `(0,0)` and `(149960,150128)`.
- SQLite row check: `select count(*) from iRow;` returns `39`.
- SQLite track-grid check: `select count(*) from iTrackGridSD;` returns `12`.
- SQLite gcell check: `select count(*) from iGCellGrid;` returns `0`.
- SQLite via check: `select count(*) from iVia;` returns `4`.
- SQLite via key check: `iVia` contains `via_1600x480`, `via2_1600x480`, `via3_1600x480`, `via4_1600x1600` with expected VIARULE/layer/ROWCOL fields.
- Demo logs on 2026-06-15 show `writeIdbInstance insert instance_count=1458`.
- Demo logs on 2026-06-15 show `readIdbInstance restored instance_count=1458`.
- SQLite instance check: `select count(*) from iInstSD;` returns `1458`.
- Demo logs on 2026-06-15 show `writeIdbPin insert pin_count=56`.
- Demo logs on 2026-06-15 show `readIdbPin restored pin_count=56`.
- SQLite pin check: `select count(*) from iPinSD;` returns `56`; each nested port/layer-shape/rect child table also returns `56` for sky130_gcd.
- Demo logs on 2026-06-15 show `writeIdbBlockage insert blockage_count=0`.
- Demo logs on 2026-06-15 show `readIdbBlockage restored blockage_count=0`.
- SQLite blockage check: `select count(*) from iBlockageSD;` returns `0`; the default sky130_gcd demo validates the empty-table path, while `aux_optional` validates non-empty blockage rows.
- Demo logs on 2026-06-15 show `writeIdbRegion insert region_count=0`.
- Demo logs on 2026-06-15 show `readIdbRegion restored region_count=0`.
- SQLite region check: `select count(*) from iRegion;` returns `0`; the default sky130_gcd demo validates the empty-table path, while `aux_optional` validates non-empty region rows.
- Demo logs on 2026-06-15 show `writeIdbSlot insert slot_count=0`.
- Demo logs on 2026-06-15 show `readIdbSlot restored slot_count=0`.
- SQLite slot check: `select count(*) from iSlotSD;` returns `0`; the default sky130_gcd demo validates the empty-table path, while `aux_optional` validates non-empty slot rows.
- Demo logs on 2026-06-15 show `writeIdbGroup insert group_count=0`.
- Demo logs on 2026-06-15 show `readIdbGroup restored group_count=0`.
- SQLite group check: `select count(*) from iGroupSD;` returns `0`; the default sky130_gcd demo validates the empty-table path, while `aux_optional` validates non-empty group rows and member names.
- Demo logs on 2026-06-15 show `writeIdbFill insert fill_count=0`.
- Demo logs on 2026-06-15 show `readIdbFill restored fill_count=0`.
- SQLite fill check: `select count(*) from iFillSD;` returns `0`; the default sky130_gcd demo validates the empty-table path, while `aux_optional` validates non-empty layer/via fill rows.
- Demo logs on 2026-06-15 show `writeSpecialNet insert special_net_count=2 segment_count=639`.
- Demo logs on 2026-06-15 show `readSpecialNet restored special_net_count=2 segment_count=639`.
- SQLite special-net check: `iSpecNetSD=2`, nested `wire_list=2`, nested `segment_list=639`, nested `point_list=697`.
- Demo logs on 2026-06-15 show `writeIdbNet insert net_count=675 segment_count=0`.
- Demo logs on 2026-06-15 show `readIdbNet restored net_count=675 segment_count=0`.
- SQLite net check: `iNetSD=675`, nested IO pin refs `54`, nested instance pin refs `1726`, nested regular wire rows `0`.
- Final demo message: `Input def and output def are the same.`

Current behavior:

- `edadb_write` writes the Design, Die, Row, TrackGrid, GCell, Via, Instance, Pin, Blockage, Region, Slot, Group, Fill, SpecialNet, and Net families to EDADB.
- `edadb_read` reads the Design, Die, Row, TrackGrid, GCell, Via, Region, Instance, Pin, Blockage, Slot, Group, Fill, SpecialNet, and Net families from EDADB.
- DEF text callbacks no longer rebuild the enabled DEF object families above.
- Disabled `readIdbXXX()` / `writeIdbXXX()` bodies are preserved under `#if 0 //EDADB_TODO`.

Important rule:

- If an object family read/write is disabled, its schema/init/shadow must stay disabled too.
- Do not delete dormant code. Keep it under `//EDADB_TODO` for later restoration.
- To migrate one object family from DEF callbacks to EDADB, first identify the exact DEF writer/parser pair, then map only fields that are written by DEF or required to rebuild those written fields.
- Before implementing each `writeIdbXXX()`, read the matching `DefWrite::write_xxx()` implementation and keep the same object, field, fallback, and call-order semantics.
- Before implementing each `readIdbXXX()`, read the matching `DefRead::parse_xxx()` implementation and keep the same object rebuild and callback-disable semantics.
- EDADB should persist the values that normal DEF write would output. If iDB holds a missing/default value but `DefWrite` would output a fallback value, the adapter may canonicalize active iDB to that output value before insertion.
- Do not persist derived/cache fields if the normal parser recomputes them, such as bounding boxes, area, polygon caches, reverse layer links, or list counters.
- Define the minimum shadow needed for a storage view: root iDB classes stay direct when EDADB can convert their members implicitly; use shadow only for member pointer-to-name/key conversion, vector ownership, synthetic keys, reduced DEF views, or helper-based rebuilds.
- Do not add wrapper shadows such as `Shadow<IdbVia>` when `TABLE4CLASS(idb::IdbVia, ...)` plus member StoreType conversion is enough.
- Keep adapter code direct and close to the existing `def_write/read_edadb` style. Do not use hidden raw-pointer swaps or temporary ownership tricks when a simple explicit update is enough.
- For owning raw-pointer iDB objects such as `IdbDesign`, do not use `edadb::readAll(std::vector<T>&)` unless copy/move ownership is safe. Use a cursor op (`makeReadAllOp()` + `readNext()`) and transfer ownership as in the original DbMap implementation.
- `readIdbDesign()` currently uses a temporary `got` object as a safe buffer. Directly reading into active `design` is closer to original iEDA reuse semantics, but it risks EDADB NULL inline pointer columns clearing active pointers.
- `IdbTrackGrid` uses shadow because `_layer_name_vec_sd` is a vector child table and needs the shadow root `primary_key` to group layer names by track grid. Do not hide this grouping as an implicit EDADB replacement.
- `IdbGCellGrid` does not use shadow because DEF read/write uses only scalar fields: direction, start, count, and step.
- `IdbVia` does not use a root shadow. Its `_master_instance` is converted by EDADB through the member type's StoreType; only `IdbViaMaster` / `IdbLayerShape` keep minimal member-level shadow views for layer-name lookup and fixed/generate geometry rebuild.
- `IdbInstance` uses shadow because DEF COMPONENT persistence stores a reduced view and must convert pointers to names: cell master, region, route-halo layers. Readback resolves those names and uses normal iDB setters.
- `IdbPin` uses shadow because DEF PINS persistence is a reduced IO-term/port/layer-shape view; readback rebuilds port layer shapes, average position, bbox, placement, and then nets later reconnect pins through DEF net callbacks.
- `IdbBlockage` uses shadow because it is polymorphic. The storage view is intentionally reduced to what `DefWrite::write_blockage()` emits; parser-only fields such as slots/fills/spacing/effective-width/soft/partial/density are not stored.
- `IdbRegion` does not use shadow because DEF REGIONS maps directly to name/type/boundary rects, and `_name` is a natural root key.
- Read `IdbRegion` before `IdbInstance` so instance region-name resolution can use the EDADB-restored region list.
- `IdbSlot` uses shadow because DEF SLOTS has no natural unique root key: `_layer_name` is not guaranteed unique, while rect child rows still need a stable parent key.
- `IdbGroup` uses shadow because DEF GROUPS stores region and member references by name; readback resolves region/instance names after Region and Instance are restored.
- `IdbFill` uses shadow because DEF FILLS is a typed layer/via storage view with pointer references converted to layer/via names and child geometry rows.
- Non-empty layer-fill and via-fill paths are covered by the generated `aux_optional` fixture and SQLite checks.
- `IdbSpecialNet` uses shadow because SPECIALNETS is a nested net/wire/segment storage view with layer/via/pin/instance references converted to names and synthetic keys for child rows.
- `IdbNet` uses shadow because NETS stores pin references and regular wire layer/via references by name; nested wire/segment rows need stable parent keys.
- Default sky130_gcd validates Net connectivity storage with regular net segment count `0`;
  `routed_irt` validates non-empty regular-wire NETS with `8997` segments.
- Current requested DEF object-family migration is complete through Net.
- After each migration, run the concise Adapter Correctness Audit in `edadb_readme.md`.

## C Namespace / API Boundary

Use spelling `adapter`, not `adaptor`, for this integration layer.

Use:

```cpp
namespace idb::edadb_adapter
```

Active adapter APIs:

- `idb::edadb_adapter::initReadDb(const char*)`
- `idb::edadb_adapter::initWriteDb(const char*)`
- `idb::edadb_adapter::EdadbIdbHelper`

Do not use the old `CppStrings` wrapper for active scalar vector fields; EDADB now maps
`std::vector<std::string>` through primitive vector child tables.

Internal init helpers such as `initPrimKeys()`, `initTable()`, and `initAllTables()` also live directly in `idb::edadb_adapter` in the `.cpp`; do not expose them in `edadb_idb_init.h` unless they become public API.

Old `edadb::init2read()` / `edadb::init2write()` adapter entry points are removed. Do not reintroduce old EDADB-facing init APIs in `edadb_idb_init.h`; iEDA code should call only `idb::edadb_adapter`.

Keep `edadb::Shadow<T>` specializations in `namespace edadb`; those are EDADB core template specializations, not adapter namespace APIs.

## Code Review Order

Review from executable entry to EDADB adapter internals:

1. `src/apps/ieda_main.cpp`: `main()` handles `-script` and calls `plfInst->runTcl()`.
2. `src/platform/flow/flow.cpp`: `Flow::runTcl()` enters Tcl.
3. `src/interface/tcl/tcl_main.h`: `tcl_start()` installs `registerCommands`.
4. `src/interface/tcl/tcl_register.h`: calls `registerCmdDB()`.
5. `src/interface/tcl/tcl_idb/tcl_register_idb.h`: registers `edadb_read` / `edadb_write`.
6. `scripts/edadb/demo/demo.sh`: runs `def2edadb.tcl`, then `edadb2def.tcl`.
7. `scripts/edadb/demo/tcl/def2edadb.tcl`: calls `edadb_write`.
8. `scripts/edadb/demo/tcl/edadb2def.tcl`: calls `edadb_read`, then `def_save`.
9. `src/interface/tcl/tcl_idb/tcl_db_file.*`: implements `CmdEdadbRead` / `CmdEdadbWrite`.
10. `src/platform/data_manager/idm_edadb.cpp`: bridges Tcl/DataManager to `IdbBuilder`.
11. `src/database/manager/builder/builder.cpp`: `buildDefFromEdadb()` / `saveDefToEdadb()`.
12. `src/database/manager/builder/def_builder/def_write_edadb.*`: EDADB write path.
13. `src/database/manager/builder/def_builder/def_read_edadb.*`: EDADB read path.
14. `src/database/edadb/idb/edadb_idb_init.*`: adapter init API and table framework.
15. `src/database/edadb/idb/edadb_idb_helper.*`: helper state for layer/via/region/instance lookup during shadow readback.
16. `src/database/edadb/idb/edadb_idb_schema.h`, `edadb_idb_shadow.h`, `shadow/*`: active schema/shadow mapping for all enabled DEF object families.

## C Directory Roles

```text
src/database/edadb/core/
```

- EDADB implementation submodule.
- Provides `edadb.h`, `DbTableOp`, table definitions, backend code, and EDADB tests.

```text
src/database/edadb/idb/
```

- iEDA-side adapter layer.
- `edadb_idb_init.*`: open DB, init primary-key policy, init tables.
- `edadb_idb_helper.*`: stores/accesses `IdbDefService`; layer/via-rule lookup helpers.
- `edadb_idb_schema.h`: active iDB/shadow table mappings for Design through Net.
- `edadb_idb_shadow.h`: shadow aggregation for active geometry/die/track/via/instance/pin/blockage/slot/group/fill/net mappings.
- `shadow/*`: per-class iDB ↔ shadow conversion definitions used by current EDADB read/write paths.
- `docs/*`: DEF/LEF parsing and ORM notes.

## Master vs C: What EDADB Adds

Compared with `master @ 007435241`, C adds these integration areas:

Build:

- `.gitmodules`: adds `src/database/edadb/core`.
- `src/database/CMakeLists.txt`: adds `src/database/edadb`.
- `src/database/edadb/CMakeLists.txt`: builds `core` and `idb`.
- `src/database/manager/builder/*/CMakeLists.txt`: links `db_edadb`.
- `src/apps/CMakeLists.txt`: links final `iEDA` with EDADB core library.

Adapter:

- `src/database/edadb/idb/*`
- `src/database/edadb/idb/shadow/*`
- `src/database/edadb/idb/docs/*`

DEF builder:

- `src/database/manager/builder/def_builder/def_read_edadb.*`
- `src/database/manager/builder/def_builder/def_write_edadb.*`
- `def_read.h` / `def_write.h`: make base classes subclass-friendly.

DataManager / Builder / Tcl:

- `IdbBuilder::buildDefFromEdadb()`
- `IdbBuilder::saveDefToEdadb()`
- `DataManager::readDefFromEdadb()`
- `DataManager::saveDefToEdadb()`
- Tcl commands: `edadb_read`, `edadb_write`
- Tcl option: `-edadb_db_path`

Demo:

- `scripts/edadb/demo/demo.sh`
- `scripts/edadb/demo/tcl/def2edadb.tcl`
- `scripts/edadb/demo/tcl/edadb2def.tcl`

iDB data model:

- Many iDB classes expose members under `#if EDADB_ENABLE` so schema/shadow code can map fields.
- Touched areas include geometry, layer shape, design, units, busbit, die/row/site, track/gcell, via/via-master, instance/pin/blockage/group/fill, special-net, and net.
- Recheck `IdbTerm.h` later; one visibility guard differs from the usual EDADB pattern.

Tcl stability:

- `ScriptEngine.*` changes Tcl command/option names from manually managed `const char*` buffers to `std::string`.

## Current State And Next Step

Current EDADB adapter coverage:

- Active write/read object families: Design / Units / BusBit, Die, Row, TrackGrid,
  GCellGrid, Via, Instance, Pin, Blockage, Region, Slot, Group, Fill, SpecialNet,
  and Net.
- Matching DEF callbacks are disabled in `DefReadEdadb::createDbByDef()` for active
  object families, so readback validation proves those families come from EDADB.
- Net and SpecialNet preserve pin order, source type, original name, weight, optional
  regular-net xtalk/fixed-bump/frequency fields, and routed wire segments.
- Fill is stored as a flattened discriminated record: layer fills keep layer name and
  rect children; via fills keep via name and coordinate children. This avoids nullable
  variant child-object mismatches in the current EDADB vector-child schema.

Class-level EDADB storage map:

| DEF family | EDADB storage | Shadow? | Main verification |
| --- | --- | --- | --- |
| Design / Units / BusBit | `iDesign` with inline `iUnits` / `iBusBitChars` | no | `default_ipl` DEF diff and logs |
| Die | `iDieSD` + `iCoordSD` points | yes | `default_ipl` DEF diff |
| Row | `iRow` direct table | no | `default_ipl` DEF diff |
| TrackGrid | `iTrackGridSD` + primitive vector layer names | yes | `default_ipl` DEF diff |
| GCellGrid | `iGCellGrid` direct table | no | `default_ipl` DEF diff |
| Via | `iVia` direct root with via-master/layer-shape member shadows | no root shadow | `default_ipl` DEF diff |
| Instance | `iInstSD` reduced COMPONENT view | yes | `default_ipl` DEF diff |
| Pin | `iPinSD` reduced PINS view | yes | `default_ipl` DEF diff |
| Blockage | `iBlockageSD` + rect rows | yes | `aux_optional` DEF diff + SQLite count |
| Region | `iRegion` + boundary rows | no | `aux_optional` DEF diff + SQLite count |
| Slot | `iSlotSD` + rect rows | yes | `aux_optional` DEF diff + SQLite count |
| Group | `iGroupSD` + primitive vector instance names | yes | `aux_optional` DEF diff + SQLite member check |
| Fill | `iFillSD` flattened layer/via records | yes | `aux_optional` DEF diff + SQLite child checks |
| SpecialNet | `iSpecNetSD` nested wires/segments/pins | yes | `aux_optional` optional fields + `default_ipl` segment logs |
| Net | `iNetSD` nested regular wires/segments/pins | yes | `aux_optional` optional fields + `routed_irt` wire/segment checks |

For future EDADB API changes or new object families, restore or harden persistence class by class:

1. schema/table mapping
2. `initAllTables()`
3. `writeIdbXXX()`
4. `readIdbXXX()`
5. disable the matching DEF parser callbacks in `DefReadEdadb::createDbByDef()`
6. demo validation

DEF callback ownership rule:

- If `readIdbXXX()` is enabled for an object family, the corresponding `defrSetXXXCbk` path in `createDbByDef()` must be disabled.
- Do not let EDADB readback and DEF text callbacks create the same iDB object family twice.

Current weak spots to test next:

- Add adapter-level C++ tests only when they can instantiate iDB classes cheaply; current
  executable proof is the iEDA+EDADB roundtrip regression.
- Add focused repeated-instance-pin and GROUP wildcard/regex fixtures if those textual cases
  become important.

Verification discipline for future classes:

- First compare original `DefWrite::write_xxx()` and `DefRead::parse_xxx()`; EDADB must persist
  the same DEF-visible fields and rebuild computed fields the same way.
- Current C branch native baseline is not byte-identical to `origin/master`: it intentionally adds
  GROUP member callbacks/rebuild and extra net/special-net optional field handling. Use the current
  native direct `DEF -> DEF` path as executable baseline, and note master drift when writing reports.
- Prefer direct EDADB mapping. Add shadow only for synthetic PK, reduced DEF view, ordered child
  vectors, variant flattening, or LEF/name lookup needed during `fromShadow()`.
- For every enabled `readIdbXXX()`, disable the matching DEF callback in
  `DefReadEdadb::createDbByDef()`.
- Tests must include DEF byte-diff against direct iDB output plus SQLite assertions for scalar
  fields, child-row counts/order, empty/non-empty cases, and optional fields.

Latest repeatable regression:

- Command: `bash src/database/edadb/test/run_idb_roundtrip_regression.sh`
- Output root: `/tmp/iedadb_regression`
- Result on 2026-06-24: passed after updating EDADB core to `3077132`; rerun during completion audit also passed.
- `default_ipl`: direct iDB DEF output matches EDADB DEF output.
- `aux_optional`: non-empty BLOCKAGES / REGIONS / SLOTS / GROUPS / FILLS and optional
  regular/special net fields match direct iDB output; SQLite content checks pass.
- `routed_irt`: non-empty regular routed NETS match direct iDB output; SQLite checks show
  `iNetSD=677`, regular wire rows `677`, segment rows `8997`, point rows `14256`.
- EDADB core completion-audit check: `cd build && ctest --output-on-failure` passed `13/13`
  in `428.08 sec`.
- Detailed regression was strengthened after this audit: `default_ipl` now checks design fields,
  core object-family counts, die/row/track/via/instance/pin/special-net/net fields, pin child rows,
  special-net child rows, and key write/read logs; `aux_optional` now uses a two-member group and
  checks blockage/region/slot/fill field values; `routed_irt` now checks GCell fields, regular-net
  wire/segment/point counts, segment type counters, ordered `clk_0` pin refs, and largest routed
  segment nets.

## Objective Completion Audit

Current audit target: EDADB core `3077132` with iEDA branch `edadb-idb`.

| Objective item | Current evidence | Status |
| --- | --- | --- |
| Update EDADB adapter implementation for the new core | `src/database/edadb/idb/edadb_idb_init.*` uses `idb::edadb_adapter`; active code uses current `edadb::initDatabase`, `createTable`, `insertObject`, `insertVector`, `makeReadAllOp`, and `readNext`; scalar vectors now use EDADB primitive vector tables instead of active `CppStrings`. | done |
| Decide direct vs shadow per iEDA class and update shadows | `src/database/edadb/idb/edadb_idb_schema.h` maps direct roots and shadows; `src/database/edadb/idb/shadow/*` implements reduced storage views for classes that need names, synthetic keys, vector child ownership, or DEF-only views. | done |
| Migrate DEF read/write by object family | `DefWriteEdadb::writeChip2Edadb()` writes Design, Die, Row, TrackGrid, GCell, Via, Instance, Pin, Blockage, Region, Slot, Group, Fill, SpecialNet, Net; `DefReadEdadb::createDbByEdadb()` reads the matching families and `createDbByDef()` disables matching DEF callbacks. | done |
| Commit by object-family increments | Git history on `edadb-idb` contains per-family commits from Design/Die/Row through Net, plus follow-up hardening commits for optional net fields, fill variants, primitive vectors, and documentation audits. | done |
| Verify each migrated family | `default_ipl` covers baseline families; `aux_optional` covers non-empty Blockage/Region/Slot/Group/Fill and optional net fields with SQLite assertions; `routed_irt` covers non-empty regular routed NETS with SQLite assertions. | done |
| Compare against original master and prove logic | `edadb_readme.md` section “对照 master 的正确性结论” defines the proof strategy: compare direct iDB DEF roundtrip with EDADB DEF roundtrip, preserving master DEF writer/parser semantics while replacing the persistence middle step. | done |
| Use server resources efficiently | Build/test commands use `-j40` where applicable; regression script runs focused EDADB adapter cases instead of unrelated full design flow. | done |
