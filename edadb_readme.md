# iEDA + EDADB Adapter README

This document is for humans reviewing or extending the EDADB integration in iEDA.

## What EDADB Adds

EDADB adds an alternate database persistence path for iEDA DEF data.

In this branch, the goal is not yet full object persistence. The current goal is a verified adapter framework:

- iEDA can open/init an EDADB database.
- iEDA exposes Tcl commands `edadb_write` and `edadb_read`.
- The demo can run DEF → EDADB write → EDADB read → DEF.
- The Design and Die groups are persisted through EDADB.
- The final DEF roundtrip matches the input DEF.

Current limitation:

- Only Design / Units / BusBit and Die are written to and read from EDADB.
- Other iDB object families still rebuild from the original DEF text.

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
    - currently writes Design / Units / BusBit and Die

14. `src/database/manager/builder/def_builder/def_read_edadb.*`
    - EDADB read framework
    - currently reads Design / Units / BusBit and Die from EDADB
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
- `writeChip2Edadb()` calls `writeIdbDesign()` and `writeIdbDie()`.
- `writeIdbDesign()` uses the current EDADB API: `edadb::insertObject<idb::IdbDesign>(design)`.
- `writeIdbDesign()` follows `DefWrite::write_units()` style: use DEF units when valid, otherwise use LEF units, and update active `design->_units` to the effective DBU before EDADB insert.
- `writeIdbDie()` follows `DefWrite::write_die()` style: persist the die point list through `edadb::Shadow<idb::IdbDie>`.
- EDADB creates physical table `iDesign`; `IdbUnits` and `IdbBusBitChars` are inline columns inside `iDesign`.
- EDADB creates physical table `iDieSD` plus vector child rows for die coordinates.
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
- `createDbByEdadb()` calls `readIdbDesign()` and `readIdbDie()`.
- `readIdbDesign()` uses the current EDADB cursor API: `makeReadAllOp<idb::IdbDesign>()` + `readNext()`.
- `readIdbDesign()` follows the old DbMap implementation semantics: transfer owned `_units` and `_bus_bit_chars` pointers into the active `IdbDesign`, then null them in the temporary object.
- The temporary `got` object is a safe buffer. Reading directly into active `design` would better match original iEDA object reuse, but EDADB NULL inline pointer columns could clear active pointers, so keep the buffered style for now.
- `readIdbDie()` reads `edadb::Shadow<idb::IdbDie>` and rebuilds the active die through `IdbDie::add_point()` plus `set_bounding_box()`, matching `DefRead::parse_die()`.
- `createDbByDef()` disables version/design/units/busbit/die callbacks and restores all remaining object families from DEF text.

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
4. track / gcell
5. via / via-master / layer-shape

For each object family:

1. enable schema/table mapping in `edadb_idb_schema.h`
2. enable table init in `edadb_idb_init.cpp`
3. port `writeIdbXXX()` to current EDADB `DbTableOp` API
4. port `readIdbXXX()` to current EDADB `DbTableOp` API
5. disable matching DEF parser callbacks in `DefReadEdadb::createDbByDef()`
6. run the canonical demo
7. inspect logs and, when object persistence is active, inspect EDADB DB contents

Important compatibility rule:

- During migration, iDB content can come from either EDADB object readback or DEF text callbacks.
- For an enabled `readIdbXXX()` object family, disable the matching `defrSetXXXCbk` callbacks.
- This prevents duplicate object creation and makes the test prove that the enabled object family really comes from EDADB.

Active targets: design / units / busbit and die.

Mapping for these targets:

| EDADB function | DEF callbacks to disable after enabling readback | Normal DEF writer functions |
| --- | --- | --- |
| `writeIdbDesign()` / `readIdbDesign()` | `defrSetVersionStrCbk`, `defrSetDesignCbk`, `defrSetUnitsCbk`, `defrSetBusBitCbk` | `write_version`, `write_design`, `write_units`, `write_busbit_char` |
| `writeIdbDie()` / `readIdbDie()` | `defrSetDieAreaCbk` | `write_die` |

Physical DB shape after this target:

```sql
iDesign(
  _design_name,
  _version,
  _units__micron_dbu,
  _bus_bit_chars__left_delimiter,
  _bus_bit_chars__right_delimiter,
  ...
)
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
- SQLite `iDesign` row contains `gcd|5.8|1000|[|]`
- SQLite `iDieSD_points_sd_iCoordSD` contains `(0,0)` and `(149960,150128)`
- final message says input DEF and output DEF are the same
