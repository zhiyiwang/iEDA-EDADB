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

## Branch Map

| Label | iEDA branch / commit | EDADB location | Adapter location | EDADB commit | Status |
| --- | --- | --- | --- | --- | --- |
| original | `master @ 007435241` | none | none | none | official iEDA, no EDADB |
| A | `origin/edadb @ 2f028c426` | `src/database/edadb/core` | `src/database/edadb/idb` | `8c724ef` | canonical non-owning layout |
| B | `origin/edadb-shadow-transitive @ 664829eef` | `src/third_party/edadb` | `src/database/edadb` | `f1214007` | old shadow-transitive layout, DEF roundtrip OK |
| C | `edadb-idb @ 99afe9c71` | `src/database/edadb/core` | `src/database/edadb/idb` | `8a4e3bf` | current init code base, build/demo OK |

Notes:

- A must not be described as using `src/third_party/edadb`; that is B’s layout.
- C starts from A’s layout and is the current development line.
- B is reference-only for old DEF/EDADB mappings; do not copy shadow-transitive behavior into C.
- `IdbVia` is enabled in C after re-audit with the new EDADB implicit member StoreType path.
- EDADB `8a4e3bf` = `293c162` plus local CMake fix for embedding EDADB as an iEDA submodule.

## Current C Milestone

Commit:

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
- `iDesign` stores `IdbUnits` and `IdbBusBitChars` as inline columns.
- `iTrackGridSD` stores track grid scalar data and owns vector child rows for layer names.
- `iVia` stores generated via fields inline through EDADB member StoreType conversion; no `Shadow<IdbVia>` is defined.
- `iInstSD` stores DEF COMPONENT fields: instance name, cell master name, source/type, placement status, orient, weight, coordinate, HALO, ROUTEHALO, and region name.
- `iPinSD` stores DEF PINS fields: pin/net names, IO term direction/use/special, port/layer rectangles, placement status, location, orient, and derived average/bbox rebuild inputs.
- `iBlockageSD` stores only DEF writer-emitted BLOCKAGES fields: type, layer name, pushdown, exceptpgnet, component name, and rects.
- `iRegion` stores DEF REGIONS fields: name, type, and boundary rects.
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
- Other object families still come from DEF text callbacks.

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
- SQLite blockage check: `select count(*) from iBlockageSD;` returns `0`; current sky130_gcd demo validates the empty-table path only.
- Demo logs on 2026-06-15 show `writeIdbRegion insert region_count=0`.
- Demo logs on 2026-06-15 show `readIdbRegion restored region_count=0`.
- SQLite region check: `select count(*) from iRegion;` returns `0`; current sky130_gcd demo validates the empty-table path only.
- Final demo message: `Input def and output def are the same.`

Current behavior:

- `edadb_write` writes the Design, Die, Row, TrackGrid, GCell, Via, Instance, Pin, Blockage, and Region groups to EDADB.
- `edadb_read` reads the Design, Die, Row, TrackGrid, GCell, Via, Region, Instance, Pin, and Blockage groups from EDADB.
- DEF text callbacks rebuild slot/group/fill/net/special-net.
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
- Next targets: `IdbSlot`, `IdbGroup`, `IdbFill`, `SpecialNet`, `Net`, one commit per target.
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
- `idb::edadb_adapter::CppStrings`

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
15. `src/database/edadb/idb/edadb_idb_helper.*`: helper state for future shadow readback.
16. `src/database/edadb/idb/edadb_idb_schema.h`, `edadb_idb_shadow.h`, `shadow/*`: dormant schema/shadow restoration area.

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
- `edadb_idb_schema.h`: iDB/shadow table mappings; Design, Die, and Row groups enabled, others dormant.
- `edadb_idb_shadow.h`: shadow aggregation; geometry/die enabled, other shadows dormant.
- `shadow/*`: per-class iDB ↔ shadow conversion definitions for later restoration.
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
- Touched areas include geometry, layer shape, design, units, busbit, die/row/site, track/gcell, via/via-master, instance/pin/blockage/group/fill.
- Recheck `IdbTerm.h` later; one visibility guard differs from the usual EDADB pattern.

Tcl stability:

- `ScriptEngine.*` changes Tcl command/option names from manually managed `const char*` buffers to `std::string`.

## Current Next Step

Restore persistence class by class. For each object family:

1. schema/table mapping
2. `initAllTables()`
3. `writeIdbXXX()`
4. `readIdbXXX()`
5. disable the matching DEF parser callbacks in `DefReadEdadb::createDbByDef()`
6. demo validation

DEF callback ownership rule:

- If `readIdbXXX()` is enabled for an object family, the corresponding `defrSetXXXCbk` path in `createDbByDef()` must be disabled.
- Do not let EDADB readback and DEF text callbacks create the same iDB object family twice.

Planned object order:

- design / units / busbit
- die
- row
- track
- gcell
- via / via-master / layer-shape

NET and SPECIALNET are not implemented yet.

Current implementation target:

- Design / units / busbit, die, row, track, gcell, and via are active for the current demo milestone.
- Next object family is TBD; keep using the old-code-first migration rule.
