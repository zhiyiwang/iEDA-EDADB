# iEDA + EDADB Adapter README

This document is for humans reviewing or extending the EDADB integration in iEDA.

## What EDADB Adds

EDADB adds an alternate database persistence path for iEDA DEF data.

In this branch, the goal is not yet full object persistence. The current goal is a verified adapter framework:

- iEDA can open/init an EDADB database.
- iEDA exposes Tcl commands `edadb_write` and `edadb_read`.
- The demo can run DEF → EDADB write → EDADB read → DEF.
- The Design, Die, Row, TrackGrid, GCell, Via, Instance, Pin, Blockage, Region, and Slot groups are persisted through EDADB.
- The final DEF roundtrip matches the input DEF.

Current limitation:

- Only Design / Units / BusBit, Die, Row, TrackGrid, GCell, Via, Instance, Pin, Blockage, Region, and Slot are written to and read from EDADB.
- Other iDB object families still rebuild from the original DEF text.
- Continue development on C (`edadb-idb`) only. Use B only as reference for old mappings.

This is intentional for the C init code base.

## Directory Layout

```text
src/database/edadb/core/
```

EDADB implementation submodule.

It provides the EDADB C++ API, including `edadb.h`, `DbTableOp`, table definition utilities, backend code, and EDADB tests.

```text
src/database/edadb/idb/
```

iEDA-side adapter layer.

Important files:

- `edadb_idb_init.*`: opens EDADB, applies primary-key policy, initializes tables.
- `edadb_idb_helper.*`: stores/accesses `IdbDefService`; provides layer and via-rule lookup helpers.
- `edadb_idb_schema.h`: maps iDB/shadow classes to EDADB table definitions.
- `edadb_idb_shadow.h`: aggregates shadow types.
- `shadow/*`: defines per-class iDB ↔ shadow conversion code.

Current C branch rule:

- schema/shadow/table creation is enabled only for object families whose `readIdbXXX()` / `writeIdbXXX()` are active.
- dormant code should be kept under `//EDADB_TODO`, not deleted.

## Naming Rule

Use **adapter**, not **adaptor**.

The adapter namespace is:

```cpp
namespace idb::edadb_adapter
```

Active adapter API:

```cpp
idb::edadb_adapter::initReadDb(const char*)
idb::edadb_adapter::initWriteDb(const char*)
idb::edadb_adapter::EdadbIdbHelper
idb::edadb_adapter::CppStrings
```

Internal init helpers, such as `initPrimKeys()`, `initTable()`, and `initAllTables()`, also live directly in `idb::edadb_adapter` but remain `.cpp`-local by not being declared in the public header.

Do not reintroduce old adapter entry points such as:

```cpp
edadb::init2read()
edadb::init2write()
```

Exception:

- `edadb::Shadow<T>` specializations stay in `namespace edadb`.
- They specialize EDADB core templates and are not adapter namespace APIs.

## Runtime Flow

The demo entry is:

```bash
cd bin/
bash /home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/demo/demo.sh 2>&1 | tee run.out
```

The demo does:

1. Set design/env paths.
2. Run `def2edadb.tcl`.
3. Run `edadb2def.tcl`.
4. Compare input DEF with output `_edadb.def`.

Tcl scripts:

- `scripts/edadb/demo/tcl/def2edadb.tcl`
  - reads LEF
  - reads DEF using normal `def_init`
  - calls `edadb_write -edadb_db_path`

- `scripts/edadb/demo/tcl/edadb2def.tcl`
  - reads LEF
  - calls `edadb_read -edadb_db_path -path`
  - calls `def_save`

## Code Review Order

Start from the iEDA executable entry, then follow the Tcl command path into EDADB.

1. `src/apps/ieda_main.cpp`
   - `main()`
   - handles `-script`
   - calls `plfInst->runTcl()`

2. `src/platform/flow/flow.cpp`
   - `Flow::runTcl()`
   - calls Tcl startup

3. `src/interface/tcl/tcl_main.h`
   - `tcl_start()`
   - installs `registerCommands`

4. `src/interface/tcl/tcl_register.h`
   - `registerCommands()`
   - calls `registerCmdDB()`

5. `src/interface/tcl/tcl_idb/tcl_register_idb.h`
   - registers `edadb_read`
   - registers `edadb_write`

6. `scripts/edadb/demo/demo.sh`
   - shows the full EDADB DEF roundtrip test

7. `scripts/edadb/demo/tcl/def2edadb.tcl`
   - review the write-side Tcl flow

8. `scripts/edadb/demo/tcl/edadb2def.tcl`
   - review the read-side Tcl flow

9. `src/interface/tcl/tcl_idb/tcl_db_file.h`
   - declarations for `CmdEdadbRead` and `CmdEdadbWrite`

10. `src/interface/tcl/tcl_idb/tcl_db_file.cpp`
    - `CmdEdadbRead::exec()`
    - `CmdEdadbWrite::exec()`

11. `src/platform/data_manager/idm_edadb.cpp`
    - `DataManager::readDefFromEdadb()`
    - `DataManager::saveDefToEdadb()`

12. `src/database/manager/builder/builder.cpp`
    - `IdbBuilder::buildDefFromEdadb()`
    - `IdbBuilder::saveDefToEdadb()`

13. `src/database/manager/builder/def_builder/def_write_edadb.*`
    - EDADB write framework
- currently writes Design / Units / BusBit, Die, Row, TrackGrid, GCell, Via, Instance, Pin, Blockage, Region, and Slot

14. `src/database/manager/builder/def_builder/def_read_edadb.*`
    - EDADB read framework
- currently reads Design / Units / BusBit, Die, Row, TrackGrid, GCell, Via, Region, Instance, Pin, Blockage, and Slot from EDADB
    - rebuilds remaining iDB object families from DEF text

15. `src/database/edadb/idb/edadb_idb_init.*`
    - adapter API boundary
    - DB init
    - primary-key/table init policy

16. `src/database/edadb/idb/edadb_idb_helper.*`
    - helper state and lookup functions

17. `src/database/edadb/idb/edadb_idb_schema.h`
    - table mapping restoration point

18. `src/database/edadb/idb/edadb_idb_shadow.h`
    - shadow aggregation restoration point

19. `src/database/edadb/idb/shadow/*`
    - per-class conversion logic

## Current Adapter Behavior

Write path:

```text
edadb_write
  -> CmdEdadbWrite::exec()
  -> DataManager::saveDefToEdadb()
  -> IdbBuilder::saveDefToEdadb()
  -> DefWriteEdadb::writeDb2Edadb()
  -> idb::edadb_adapter::initWriteDb()
  -> writeChip2Edadb()
```

Current write state:

- `initWriteDb()` is active.
- `writeChip2Edadb()` calls `writeIdbDesign()`, `writeIdbDie()`, `writeIdbRow()`, `writeIdbTrackGrid()`, `writeIdbGCellGrid()`, `writeIdbVia()`, `writeIdbInstance()`, `writeIdbPin()`, `writeIdbBlockage()`, `writeIdbRegion()`, and `writeIdbSlot()`.
- `writeIdbDesign()` uses the current EDADB API: `edadb::insertObject<idb::IdbDesign>(design)`.
- `writeIdbDesign()` follows `DefWrite::write_units()` style: use DEF units when valid, otherwise use LEF units, and update active `design->_units` to the effective DBU before EDADB insert.
- `writeIdbDie()` follows `DefWrite::write_die()` style: persist the die point list through `edadb::Shadow<idb::IdbDie>`.
- `writeIdbRow()` follows `DefWrite::write_row()` style: persist each row's name, site, origin, DO/BY count, and STEP values directly as `IdbRow`.
- `writeIdbTrackGrid()` follows `DefWrite::write_track_grid()` style: persist direction/start/DO/STEP plus the DEF layer-name list through `edadb::Shadow<idb::IdbTrackGrid>`.
- `writeIdbGCellGrid()` follows `DefWrite::write_gcell_grid()` style: persist direction/start/DO/STEP directly as `IdbGCellGrid`.
- `writeIdbVia()` follows `DefWrite::write_via()` style and writes root `IdbVia` directly; EDADB converts `_master_instance` through member StoreType.
- `writeIdbInstance()` follows `DefWrite::write_component()` style and writes COMPONENT fields through `Shadow<IdbInstance>`.
- `writeIdbPin()` follows `DefWrite::write_pin()` style and writes PINS fields through `Shadow<IdbPin>`, `Shadow<IdbTerm>`, and `Shadow<IdbPort>`.
- `writeIdbBlockage()` follows `DefWrite::write_blockage()` style and writes only DEF-emitted BLOCKAGES fields through `Shadow<IdbBlockage>`.
- `writeIdbRegion()` follows `DefWrite::write_region()` style and writes REGIONS fields directly as `IdbRegion`.
- `writeIdbSlot()` follows `DefWrite::write_slot()` style and writes SLOTS fields through `Shadow<IdbSlot>`.
- EDADB creates physical table `iDesign`; `IdbUnits` and `IdbBusBitChars` are inline columns inside `iDesign`.
- EDADB creates physical table `iDieSD` plus vector child rows for die coordinates.
- EDADB creates physical table `iRow`; `IdbSite` and original coordinate are inline row columns.
- EDADB creates physical table `iTrackGridSD` plus vector child rows for track-grid layer names.
- EDADB creates physical table `iGCellGrid`.
- EDADB creates physical table `iVia`; generated via master fields are inline columns under `_master_instance__master_generate_sd__...`.
- EDADB creates physical table `iInstSD` for COMPONENT storage.
- EDADB creates physical table `iPinSD` plus nested port/layer-shape/rect child tables.
- EDADB creates physical table `iBlockageSD` plus rect child table.
- EDADB creates physical table `iRegion` plus boundary-rect child table.
- EDADB creates physical table `iSlotSD` plus rect child table.
- Other `writeIdbXXX()` calls are disabled under `//EDADB_TODO`.

Read path:

```text
edadb_read
  -> CmdEdadbRead::exec()
  -> DataManager::readDefFromEdadb()
  -> IdbBuilder::buildDefFromEdadb()
  -> DefReadEdadb::createDbFromEdadb()
  -> idb::edadb_adapter::initReadDb()
  -> createDbByEdadb()
  -> createDbByDef()
```

Current read state:

- `initReadDb()` is active.
- `createDbByEdadb()` calls `readIdbDesign()`, `readIdbDie()`, `readIdbRow()`, `readIdbTrackGrid()`, `readIdbGCellGrid()`, `readIdbVia()`, `readIdbRegion()`, `readIdbInstance()`, `readIdbPin()`, `readIdbBlockage()`, and `readIdbSlot()`.
- `readIdbDesign()` uses the current EDADB cursor API: `makeReadAllOp<idb::IdbDesign>()` + `readNext()`.
- `readIdbDesign()` follows the old DbMap implementation semantics: transfer owned `_units` and `_bus_bit_chars` pointers into the active `IdbDesign`, then null them in the temporary object.
- The temporary `got` object is a safe buffer. Reading directly into active `design` would better match original iEDA object reuse, but EDADB NULL inline pointer columns could clear active pointers, so keep the buffered style for now.
- `readIdbDie()` reads `edadb::Shadow<idb::IdbDie>` and rebuilds the active die through `IdbDie::add_point()` plus `set_bounding_box()`, matching `DefRead::parse_die()`.
- `readIdbRow()` reads `IdbRow` directly, then rebuilds each row's site from LEF site clone and calls `set_bounding_box()`, matching `DefRead::parse_row()`.
- `readIdbTrackGrid()` reads `edadb::Shadow<idb::IdbTrackGrid>`, rebuilds track scalar fields, resolves layer names through LEF `IdbLayers`, and updates each routing layer's track-grid list, matching `DefRead::parse_track_grid()`.
- `readIdbGCellGrid()` reads `IdbGCellGrid` directly and rebuilds the gcell list, matching `DefRead::parse_gcell_grid()`.
- `readIdbVia()` reads `IdbVia` directly; member-level StoreType rebuilds `IdbViaMaster`, via-rule/layer pointers, pattern, and generated cut shapes through `EdadbIdbHelper`, matching `DefRead::parse_via()`.
- `readIdbInstance()` reads `Shadow<IdbInstance>`, resolves cell master/layers/region by name, and rebuilds instance state with normal setters, matching `DefRead::parse_component()`.
- `readIdbPin()` reads `Shadow<IdbPin>`, rebuilds IO terms/ports/layer-shapes, recomputes average/bbox fields, and leaves net pointer reconnect for DEF net callbacks, matching `DefRead::parse_pin()`.
- `readIdbBlockage()` reads `Shadow<IdbBlockage>`, rebuilds routing/placement derived objects, resolves layers/instances by name, and restores rects, matching `DefRead::parse_blockage()`.
- `readIdbRegion()` reads `IdbRegion` directly and restores name/type/boundary rects before instances resolve region names, matching `DefRead::parse_region()`.
- `readIdbSlot()` reads `Shadow<IdbSlot>` and restores layer name plus rects, matching `DefRead::parse_slot()`.
- `createDbByDef()` disables version/design/units/busbit/die/row/track/gcell/via/component/pin/blockage/region/slot callbacks and restores all remaining object families from DEF text.

Important ownership note:

- Avoid `edadb::readAll(std::vector<T>&)` for owning raw-pointer iDB classes such as `IdbDesign` unless copy/move ownership is explicitly safe.
- Prefer cursor readback for these classes, because it matches the original EDADB implementation style and avoids shallow-copy lifetime hazards.
- Implement adapter code in the direct style of `DefWrite` / `DefRead`: read the original writer/parser first, persist the values normal DEF output would use, and avoid hidden raw-pointer swaps or temporary ownership tricks.

## How To Extend Persistence

Restore one object family at a time.

Recommended order:

1. design / units / busbit
2. die
3. row
4. track
5. gcell
6. via / via-master / layer-shape
7. instance
8. pin
9. blockage
10. region
11. slot / group / fill
12. special-net / net

For each object family, use this migration workflow.

1. Review the original DEF writer/parser:
   - Find the matching `DefWrite::write_xxx()` method.
   - Find the matching `DefRead::parse_xxx()` method and callback registration.
   - List exactly which iDB members are emitted to DEF.
   - List which iDB members are reconstructed from those emitted values.
   - List which members are derived and should not be persisted.

2. Decide the EDADB storage view:
   - Direct `TABLE4CLASS` is OK when the iDB object already has a stable root key and its members match the DEF storage semantics.
   - Keep root iDB classes direct when EDADB can convert their members implicitly.
   - Use the minimum shadow needed for member pointer-to-name/key conversion, vector ownership, synthetic keys, reduced DEF views, or helper-based rebuilds.
   - Do not add wrapper shadows when `TABLE4CLASS(root, ...)` plus member StoreType conversion is enough.

3. Define schema and init:
   - Add or enable `TABLE4CLASS`, `TABLE4SHADOW`, or `TABLE4CLASS_WVEC` in `src/database/edadb/idb/edadb_idb_schema.h`.
   - If shadow is needed, add the class under `src/database/edadb/idb/shadow` and include it from `edadb_idb_shadow.h`.
   - Update primary-key rules and `EDADB_INIT_TABLE(...)` in `src/database/edadb/idb/edadb_idb_init.cpp`.
   - Keep schema/init/shadow disabled when the matching `readIdbXXX()` / `writeIdbXXX()` path is disabled.

4. Port the write path:
   - Read the normal `DefWrite::write_xxx()` implementation first.
   - Implement `DefWriteEdadb::writeIdbXXX()` with the same object source, null checks, fallback rules, field set, and call-order assumptions.
   - Persist the values that normal DEF output would contain. If normal DEF writer uses a fallback value, the EDADB adapter may canonicalize active iDB to that value before insertion.
   - Use current EDADB API such as `edadb::insertObject<T>()` or `edadb::insertVector<T>()`; do not restore old `DbMap` code except as reference.

5. Port the read path:
   - Read the normal `DefRead::parse_xxx()` implementation first.
   - Implement `DefReadEdadb::readIdbXXX()` so it rebuilds the active iDB object with the same semantics as the parser.
   - Recompute derived fields through the same iDB methods, such as `set_bounding_box()`, reverse layer links, or helper lookups.
   - Prefer cursor reads (`makeReadAllOp()` + `readNext()`) for owning raw-pointer classes unless copy/move ownership is proven safe.

6. Disable matching DEF callbacks:
   - Move the matching `defrSetXXXCbk(...)` into the disabled EDADB callback block in `DefReadEdadb::createDbByDef()`.
   - Keep unrelated callbacks enabled so the rest of the design can still rebuild from DEF text.
   - This makes the demo prove that the enabled object family really comes from EDADB.

7. Validate:
   - Build the relevant targets, normally `cmake --build build -j40 --target db_edadb def_builder iEDA`.
   - Run only the canonical demo unless explicitly asked otherwise.
   - Check adapter logs for `writeIdbXXX` and `readIdbXXX`.
   - Inspect SQLite tables for object counts and key fields.
   - Confirm the final DEF comparison says input and output are the same.

Important compatibility rule:

- During migration, iDB content can come from either EDADB object readback or DEF text callbacks.
- For an enabled `readIdbXXX()` object family, disable the matching `defrSetXXXCbk` callbacks.
- This prevents duplicate object creation and makes the test prove that the enabled object family really comes from EDADB.

## Adapter Correctness Audit

After each migration, check:

- **Schema**: Persist only DEF-emitted fields or fields needed to rebuild them; do not store parser-derived caches.
- **Shadow**: Keep root classes direct when possible; use only the minimum member-level shadow needed for vector ownership, synthetic keys, pointer-to-name/key conversion, reduced DEF views, or helper-based rebuilds.
- **Write**: Match `DefWrite::write_xxx()` for source object, null/empty handling, fallback values, and emitted fields.
- **Read**: Match `DefRead::parse_xxx()` rebuild semantics and disable the corresponding DEF callback.
- **Runtime**: Run only the canonical demo, check write/read logs, inspect SQLite counts/key columns, and record uncovered cases.

Active targets: design / units / busbit, die, row, track grid, gcell grid, via, instance, pin, blockage, region, and slot.

Mapping for these targets:

| EDADB function | DEF callbacks to disable after enabling readback | Normal DEF writer functions |
| --- | --- | --- |
| `writeIdbDesign()` / `readIdbDesign()` | `defrSetVersionStrCbk`, `defrSetDesignCbk`, `defrSetUnitsCbk`, `defrSetBusBitCbk` | `write_version`, `write_design`, `write_units`, `write_busbit_char` |
| `writeIdbDie()` / `readIdbDie()` | `defrSetDieAreaCbk` | `write_die` |
| `writeIdbRow()` / `readIdbRow()` | `defrSetRowCbk` | `write_row` |
| `writeIdbTrackGrid()` / `readIdbTrackGrid()` | `defrSetTrackCbk` | `write_track_grid` |
| `writeIdbGCellGrid()` / `readIdbGCellGrid()` | `defrSetGcellGridCbk` | `write_gcell_grid` |
| `writeIdbVia()` / `readIdbVia()` | `defrSetViaStartCbk`, `defrSetViaCbk` | `write_via` |
| `writeIdbInstance()` / `readIdbInstance()` | `defrSetComponentCbk`, `defrSetComponentStartCbk`, `defrSetComponentEndCbk` | `write_component` |
| `writeIdbPin()` / `readIdbPin()` | `defrSetPinCbk`, `defrSetPinEndCbk`, `defrSetStartPinsCbk` | `write_pin` |
| `writeIdbBlockage()` / `readIdbBlockage()` | `defrSetBlockageCbk` | `write_blockage` |
| `writeIdbRegion()` / `readIdbRegion()` | `defrSetRegionCbk` | `write_region` |
| `writeIdbSlot()` / `readIdbSlot()` | `defrSetSlotCbk` | `write_slot` |

Row does not currently need a shadow class. `IdbRow::_name` is the root table primary key, so EDADB can attach inline `IdbSite` and original-coordinate columns directly. `IdbDie` needed shadow because its coordinate vector requires a synthetic root `primary_key` for child rows.

TrackGrid does use a shadow class. Its DEF meaning is a scalar track definition plus a vector of layer names. The layer-name vector needs child rows grouped under the correct track grid root, so the explicit shadow `primary_key` is part of the storage model. Do not hide that grouping by making EDADB implicitly replace the class.

GCellGrid does not need a shadow class. Its DEF meaning is exactly four scalar fields: direction, start, count, and step. There are no raw pointer members, vector child rows, or derived fields in the persisted object.

Via does not use a root shadow class. `IdbVia::_name` is the root primary key, and `_master_instance` is converted by EDADB through member StoreType; `IdbViaMaster` / `IdbLayerShape` keep only the member-level conversion needed for layer-name lookup and generated/fixed shape rebuild.

Instance uses `Shadow<IdbInstance>`. DEF COMPONENT output needs a reduced view: instance name, cell master name, type/source, placement status, orient, weight, coordinate, HALO, ROUTEHALO layer names, and region name. Readback resolves names and calls normal iDB setters.

Pin uses `Shadow<IdbPin>`. DEF PINS output is an IO-term/port/layer-shape view, not the full `IdbPin` object. The adapter stores the DEF-emitted fields and recomputes bbox/average fields during readback; net pointers are reconnected later by normal DEF net parsing.

Blockage uses `Shadow<IdbBlockage>` because the iDB class is polymorphic. The stored fields are limited to what `DefWrite::write_blockage()` emits: blockage type, layer name, pushdown, exceptpgnet, component name, and rects.

Region does not need a shadow class. DEF REGIONS maps directly to region name, type, and boundary rect list, and `_name` is a natural root key. Read Region before Instance so component region references can resolve against the restored region list.

Slot uses `Shadow<IdbSlot>`. DEF SLOTS maps to layer name plus rect list, but `_layer_name` is not a unique root key, so the shadow adds `primary_key` for stable child-rect grouping.

Physical DB shape for active targets:

```sql
iDesign(
  _design_name,
  _version,
  _units__micron_dbu,
  _bus_bit_chars__left_delimiter,
  _bus_bit_chars__right_delimiter,
  ...
)

iDieSD(primary_key, ...)
iDieSD_points_sd_iCoordSD(iDieSD_primary_key, _vec_idx, _x_sd, _y_sd)

iRow(
  _name,
  _site__name,
  _site__orient,
  _original_coordinate__x_sd,
  _original_coordinate__y_sd,
  _row_num_x,
  _row_num_y,
  _step_x,
  _step_y,
  ...
)

iTrackGridSD(primary_key, _track_num_sd, _track_sd__start, _track_sd__direction, _track_sd__pitch)
iTrackGridSD__layer_name_vec_sd_CppStr(iTrackGridSD_primary_key, str)

iGCellGrid(_direction, _start, _num, _space)

iVia(_name, _master_instance__name_sd, _master_instance__type_sd, _master_instance__master_generate_sd__...)

iInstSD(_name_sd, _cell_master_name_sd, _type_sd, _status_sd, _orient_sd, _weight_sd, _coordinate_sd__..., _halo_sd__..., _route_halo_sd__..., _region_name_sd)

iPinSD(_pin_name_sd, _net_name_sd, _io_term_sd__..., _average_coordinate_sd__..., _location_sd__..., _orient_sd, ...)
iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD(...)
iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD__layer_shape_list_sd_iLayerShapeSD(...)
iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD__layer_shape_list_sd_iLayerShapeSD__rect_list_sd_IdbRect(...)

iBlockageSD(primary_key, _instance_name_sd, _is_pushdown_sd, _type_sd, _layer_name_sd, _is_except_pgnet_sd)
iBlockageSD__rect_list_sd_IdbRect(iBlockageSD_primary_key, ...)

iRegion(_name, _type)
iRegion__boudary_list_IdbRect(iRegion__name, ...)

iSlotSD(primary_key, _layer_name_sd)
iSlotSD__rect_list_sd_IdbRect(iSlotSD_primary_key, ...)
```

NET and SPECIALNET are not implemented yet.

## Validation

Build:

```bash
bash build.sh -j40
```

Targeted compile check:

```bash
cmake --build build -j40 --target db_edadb def_builder
```

Demo:

```bash
cd bin/
bash /home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/demo/demo.sh 2>&1 | tee run.out
```

Expected current result:

- logs show `[EDADB-IDB] initWriteDb`
- logs show `[EDADB-IDB] initReadDb`
- logs show `writeIdbDesign insert name=gcd version=5.8 micron_dbu=1000`
- logs show `readIdbDesign restored name=gcd version=5.8 micron_dbu=1000`
- logs show `writeIdbDie insert point_count=2`
- logs show `readIdbDie restored point_count=2`
- logs show `writeIdbRow insert row_count=39`
- logs show `readIdbRow restored row_count=39`
- logs show `writeIdbTrackGrid insert track_grid_count=12`
- logs show `readIdbTrackGrid restored track_grid_count=12 layer_ref_count=12`
- logs show `writeIdbGCellGrid insert gcell_grid_count=0`
- logs show `readIdbGCellGrid restored gcell_grid_count=0`
- logs show `writeIdbVia insert via_count=4`
- logs show `readIdbVia restored via_count=4`
- logs show `writeIdbInstance insert instance_count=1458`
- logs show `readIdbInstance restored instance_count=1458`
- logs show `writeIdbPin insert pin_count=56`
- logs show `readIdbPin restored pin_count=56`
- logs show `writeIdbBlockage insert blockage_count=0`
- logs show `readIdbBlockage restored blockage_count=0`
- logs show `writeIdbRegion insert region_count=0`
- logs show `readIdbRegion restored region_count=0`
- logs show `writeIdbSlot insert slot_count=0`
- logs show `readIdbSlot restored slot_count=0`
- SQLite `iDesign` row contains `gcd|5.8|1000|[|]`
- SQLite `iDieSD_points_sd_iCoordSD` contains `(0,0)` and `(149960,150128)`
- SQLite `iRow` row count is `39`
- SQLite `iTrackGridSD` row count is `12`
- SQLite `iGCellGrid` row count is `0`
- SQLite `iVia` row count is `4`
- SQLite `iInstSD` row count is `1458`
- SQLite `iPinSD` row count is `56`
- SQLite `iBlockageSD` row count is `0`; current sky130_gcd demo covers only the empty-table path for Blockage
- SQLite `iRegion` row count is `0`; current sky130_gcd demo covers only the empty-table path for Region
- SQLite `iSlotSD` row count is `0`; current sky130_gcd demo covers only the empty-table path for Slot
- final message says input DEF and output DEF are the same
