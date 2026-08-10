# EDADB DEF Read/Write Onboarding

This guide explains how to read the iEDA + EDADB DEF roundtrip code path. It is generated from the local `.understand-anything/knowledge-graph.json` and verified against the current source tree.

## Project Overview

- Project: `iEDA-EDADB`
- Main languages: C++, C, Tcl, Python, CMake, Shell
- Main frameworks/components: CMake, Tcl, SQLite, iDB, EDADB
- Current focus: replace selected DEF text read/write sections with EDADB persistence while keeping iEDA's original DEF semantics.

## Architecture Layers

1. Demo scripts: drive the two-pass validation flow.
   - `scripts/edadb/demo/demo.sh`
   - `scripts/edadb/demo/tcl/def2edadb.tcl`
   - `scripts/edadb/demo/tcl/edadb2def.tcl`
2. Tcl interface: exposes `def_init`, `def_save`, `edadb_read`, `edadb_write`.
   - `src/interface/tcl/tcl_idb/tcl_register_idb.h`
   - `src/interface/tcl/tcl_idb/tcl_db_file.cpp`
3. DataManager bridge: connects Tcl commands to iDB builder APIs.
   - `src/platform/data_manager/idm_edadb.cpp`
4. Builder bridge: creates `DefReadEdadb` / `DefWriteEdadb` and keeps normal iDB post-processing.
   - `src/database/manager/builder/builder.cpp`
5. DEF builder: owns the EDADB-aware read/write order.
   - `src/database/manager/builder/def_builder/def_read_edadb.cpp`
   - `src/database/manager/builder/def_builder/def_write_edadb.cpp`
   - Baseline comparison: `def_read.cpp`, `def_write.cpp`
6. EDADB adapter: maps iDB/shadow objects to EDADB schema and initialization.
   - `src/database/edadb/idb/edadb_idb_init.cpp`
   - `src/database/edadb/idb/edadb_idb_schema.h`
   - `src/database/edadb/idb/edadb_idb_shadow.h`
   - `src/database/edadb/idb/edadb_idb_helper.h`
   - `src/database/edadb/idb/shadow/*.h`
7. EDADB core: provides table creation, insertion, reading, type metadata, and SQLite-backed storage.
   - `src/database/edadb/core/include/edadb.h`

## Key Concepts

- EDADB does not parse DEF text directly; iEDA still owns DEF/LEF parsing and final DEF formatting.
- The demo has two passes: `DEF text -> iDB -> EDADB`, then `LEF + EDADB -> iDB -> DEF text`.
- `DefReadEdadb::createDbByDef()` and `DefReadEdadb::createDbByEdadb()` must stay synchronized: if a DEF parser callback is disabled, the matching `readIdbXXX()` must rebuild that object from EDADB.
- `DefWriteEdadb::writeChip2Edadb()`, `edadb_idb_schema.h`, and `edadb_idb_init.cpp` must stay synchronized: if a `writeIdbXXX()` is enabled, its table schema/init must also be enabled.
- Every `writeIdbXXX()` should first be checked against the matching `DefWrite::write_xxx()` method; every `readIdbXXX()` should be checked against the matching `DefRead` callback/parser path.
- Shadow classes are minimized: use direct `TABLE4CLASS` when EDADB can store/rebuild the iDB object; define `edadb::Shadow<T>` only when a stable PK, vector ownership, layer/site/via lookup, or reconstruction view is needed.
- LEF must be loaded before EDADB read, because many EDADB restore paths need LEF objects such as layers, sites, cell masters, and via rules.

## Guided Tour

### 1. Start From The Test Flow

Read `scripts/edadb/demo/demo.sh` first.

- It sets `INPUT_DEF`, `EDADB_DB_PATH`, `READ_DEF=1`, `WRITE_EDADB=1`, and `READ_EDADB=1`.
- It runs `def2edadb.tcl` to write EDADB from DEF.
- It runs `edadb2def.tcl` to read EDADB and save DEF.
- It compares `INPUT_DEF` with `${INPUT_DEF%.*}_edadb.def`.

Run only this validation command when checking DEF roundtrip:

```bash
cd bin/
bash /home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/demo/demo.sh 2>&1 | tee run.out
```

Expected final line:

```text
Input def and output def are the same.
```

### 2. Follow The Tcl Commands

Read `src/interface/tcl/tcl_idb/tcl_register_idb.h`.

- `def_init` -> normal DEF text read.
- `def_save` -> normal DEF text write.
- `edadb_read` -> EDADB read path.
- `edadb_write` -> EDADB write path.

Then read `src/interface/tcl/tcl_idb/tcl_db_file.cpp`.

- `CmdEdadbRead::exec()` parses `-edadb_db_path` and `-path`, then calls `dmInst->readDefFromEdadb()`.
- `CmdEdadbWrite::exec()` parses `-edadb_db_path`, then calls `dmInst->saveDefToEdadb()`.

### 3. Follow The DataManager Bridge

Read `src/platform/data_manager/idm_edadb.cpp`.

- `readDefFromEdadb()` calls `IdbBuilder::buildDefFromEdadb()`, then refreshes `_design` and applies die transform if needed.
- `saveDefToEdadb()` calls `IdbBuilder::saveDefToEdadb()`.

### 4. Follow The Builder Bridge

Read `src/database/manager/builder/builder.cpp`.

- `buildDefFromEdadb()` creates a new `IdbDefService`, calls `DefReadEdadb::createDbFromEdadb()`, then runs `buildNet()`, `buildBus()`, and `log()`.
- `saveDefToEdadb()` initializes DEF output state and calls `DefWriteEdadb::writeDb2Edadb()`.

### 5. Read The EDADB Write Path

Start at `src/database/manager/builder/def_builder/def_write_edadb.cpp`.

Call order:

1. `DefWriteEdadb::writeDb2Edadb()`
2. `idb::edadb_adapter::initWriteDb()`
3. `DefWriteEdadb::writeChip2Edadb()`
4. `writeIdbDesign()`, `writeIdbDie()`, `writeIdbRow()`, `writeIdbTrackGrid()`, `writeIdbGCellGrid()`, `writeIdbVia()`, `writeIdbInstance()`, `writeIdbPin()`, `writeIdbBlockage()`, `writeIdbRegion()`, `writeIdbSlot()`, `writeIdbGroup()`, `writeIdbFill()`, `writeIdbSpecialNet()`, `writeIdbNet()`

For each class, compare with `src/database/manager/builder/def_builder/def_write.cpp`:

- `writeIdbDesign()` <-> `write_version()`, `write_design()`, `write_units()`, `write_busbit_char()`
- `writeIdbDie()` <-> `write_die()`
- `writeIdbRow()` <-> `write_row()`
- `writeIdbTrackGrid()` <-> `write_track_grid()`
- `writeIdbGCellGrid()` <-> `write_gcell_grid()`
- `writeIdbVia()` <-> `write_via()`
- `writeIdbInstance()` <-> `write_component()`
- `writeIdbPin()` <-> `write_pin()`
- `writeIdbBlockage()` <-> `write_blockage()`
- `writeIdbRegion()` <-> `write_region()`
- `writeIdbSlot()` <-> `write_slot()`
- `writeIdbGroup()` <-> `write_group()`
- `writeIdbFill()` <-> `write_fill()`
- `writeIdbSpecialNet()` <-> `write_special_net()`
- `writeIdbNet()` <-> `write_net()`

### 6. Read The EDADB Read Path

Start at `src/database/manager/builder/def_builder/def_read_edadb.cpp`.

Call order:

1. `DefReadEdadb::createDbFromEdadb()`
2. `idb::edadb_adapter::initReadDb()`
3. `DefReadEdadb::createDbByEdadb()`
4. `readIdbDesign()`, `readIdbDie()`, `readIdbRow()`, `readIdbTrackGrid()`, `readIdbGCellGrid()`, `readIdbVia()`, `readIdbRegion()`, `readIdbInstance()`, `readIdbPin()`, `readIdbBlockage()`, `readIdbSlot()`, `readIdbGroup()`, `readIdbFill()`, `readIdbSpecialNet()`, `readIdbNet()`
5. `DefReadEdadb::createDbByDef()` still parses DEF text, but disables callbacks for objects restored from EDADB.

For each class, compare with `src/database/manager/builder/def_builder/def_read.cpp` and its callbacks. The EDADB read path must rebuild the same iDB state that the original DEF callback would have built.

### 7. Read The Adapter Layer

Read in this order:

1. `src/database/edadb/idb/edadb_idb_init.cpp`
   - `initWriteDb()` opens DB and creates tables.
   - `initReadDb()` opens DB and registers table metadata.
   - `initPrimKeys()` sets special primary-key behavior.
   - `initAllTables()` must match enabled DEF object families.
2. `src/database/edadb/idb/edadb_idb_schema.h`
   - `TABLE4CLASS` and `TABLE4CLASS_WVEC` define the physical table view.
   - Check that columns match the DEF-visible fields or fields needed for reconstruction.
3. `src/database/edadb/idb/edadb_idb_shadow.h`
   - Aggregates shadow definitions.
4. `src/database/edadb/idb/shadow/*.h`
   - Each shadow should explain a real need: PK, vector ownership, lookup names, or reconstruction.
5. `src/database/edadb/idb/edadb_idb_helper.h`
   - Provides access to `IdbDefService`, layer lookup, and via-rule lookup during `fromShadow()` rebuild.

### 8. Read The EDADB Core API Last

Read `src/database/edadb/core/include/edadb.h` after the adapter path is clear.

Focus on these APIs:

- `edadb::initDatabase(path)`
- `edadb::createTable<T>()`
- `edadb::insertObject<T>()`
- `edadb::insertVector<T>()`
- `edadb::makeReadAllOp<T>()`
- `edadb::readNext<T>()`

Only then go deeper into EDADB core traversers and SQLite implementation.

## Review And Validation References

This onboarding guide owns only the architecture and reading path. Use the canonical documents for implementation decisions:

- `idb-adapter/README.md`: adapter rules, per-class checklist, review order, and document template.
- `def-ieda-mapping-and-order.md`: root-order classification, current implementation status, and future order experiments.
- `../test/README.md`: executable roundtrip cases, commands, assertions, and concurrency behavior.

## Useful Search Commands

```bash
rg -n "CmdEdadb|edadb_read|edadb_write|def_init|def_save" src/interface/tcl/tcl_idb
rg -n "readDefFromEdadb|saveDefToEdadb|buildDefFromEdadb|saveDefToEdadb" src/platform src/database
rg -n "createDbFromEdadb|createDbByDef|createDbByEdadb|readIdb" src/database/manager/builder/def_builder/def_read_edadb.cpp
rg -n "writeDb2Edadb|writeChip2Edadb|writeIdb" src/database/manager/builder/def_builder/def_write_edadb.cpp
rg -n "TABLE4|EDADB_INIT_TABLE|initReadDb|initWriteDb|Shadow<" src/database/edadb/idb
```

## Complexity Hotspots

- `def_read_edadb.cpp`: high-risk because it mixes EDADB reconstruction with disabled DEF callbacks.
- `def_write_edadb.cpp`: must track original DEF writer field semantics exactly.
- `edadb_idb_schema.h`: table columns define what can be restored; wrong columns create silent semantic drift.
- `shadow/*.h`: most ownership, PK, vector, and lookup bugs appear here.
- EDADB core traversal: inspect only after adapter behavior is understood.
