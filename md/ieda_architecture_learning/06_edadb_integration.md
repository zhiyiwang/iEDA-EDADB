---
title: EDADB 在 iEDA 中的集成位置
tags:
  - iEDA
  - EDADB
  - adapter
---

# EDADB 在 iEDA 中的集成位置

> [!info] Source baseline
> 行号按 `edadb-idb-dev/sort-abc-no-sort-d @ 77fbe5c67` 核对。

[学习地图](00_learning_map.md) · [代码主线](10_complete_tutorial.md) · [Adapter Docs](../../src/database/edadb/docs/README.md)

## 1. Architectural Position

EDADB currently adds another persistence path around active iDB:

```text
active iDB -> DefWriteEdadb -> EDADB SQLite
EDADB SQLite + active LEF/DEF context -> DefReadEdadb -> active iDB
active iDB -> unchanged iFP/iPL/iCTS/iRT/iSTA/iDRC interfaces
```

Point tools do not directly query EDADB tables.

## 2. Tcl and DataManager Entries

- read command: [`CmdEdadbRead::exec()`](../../src/interface/tcl/tcl_idb/tcl_db_file.cpp#L531)
- write command: [`CmdEdadbWrite::exec()`](../../src/interface/tcl/tcl_idb/tcl_db_file.cpp#L584)
- read DataManager path: [`DataManager::readDefFromEdadb()`](../../src/platform/data_manager/idm_edadb.cpp#L13)
- write DataManager path: [`DataManager::saveDefToEdadb()`](../../src/platform/data_manager/idm_edadb.cpp#L33)

Current `CmdEdadbWrite::exec()` prioritizes the EDADB path when `-edadb_db_path` is present: [`tcl_db_file.cpp`](../../src/interface/tcl/tcl_idb/tcl_db_file.cpp#L605).

## 3. Builder Bridge

Read:

```text
DataManager::readDefFromEdadb()
  -> IdbBuilder::buildDefFromEdadb()
  -> create IdbDefService(existing layout)
  -> DefReadEdadb::createDbFromEdadb()
  -> buildNet() / buildBus()
```

- builder read: [`IdbBuilder::buildDefFromEdadb()`](../../src/database/manager/builder/builder.cpp#L341)
- normal post-build relations: [`builder.cpp`](../../src/database/manager/builder/builder.cpp#L370)

Write:

```text
DataManager::saveDefToEdadb()
  -> IdbBuilder::saveDefToEdadb()
  -> DefWriteEdadb::writeDb2Edadb()
```

- builder write: [`IdbBuilder::saveDefToEdadb()`](../../src/database/manager/builder/builder.cpp#L378)

## 4. EDADB Read Orchestration

[`DefReadEdadb::createDbFromEdadb()`](../../src/database/manager/builder/def_builder/def_read_edadb.cpp#L24) does four things:

1. register active `IdbDefService` in the adapter helper;
2. initialize EDADB read API;
3. restore enabled object families from EDADB;
4. run the DEF parser with EDADB-restored object callbacks omitted; with all 15 families enabled, this pass mainly preserves the existing DEF parser/context path.

All 15 enabled root families are called from [`createDbByEdadb()`](../../src/database/manager/builder/def_builder/def_read_edadb.cpp#L208).

The DEF pass deliberately omits callbacks for EDADB-restored families: [`createDbByDef()`](../../src/database/manager/builder/def_builder/def_read_edadb.cpp#L60).

## 5. EDADB Write Orchestration

- initialize DB and dispatch by `DefWriteType`: [`DefWriteEdadb::writeDb2Edadb()`](../../src/database/manager/builder/def_builder/def_write_edadb.cpp#L26)
- write enabled root families: [`DefWriteEdadb::writeChip2Edadb()`](../../src/database/manager/builder/def_builder/def_write_edadb.cpp#L73)

The adapter persists a DEF/iDB storage view. It does not serialize every tool-private placement, routing or timing object.

## 6. Adapter Layers

| Layer | Responsibility |
| --- | --- |
| `src/database/edadb/core` | EDADB table definitions, SQLite operators and generic C++ conversion. |
| `src/database/edadb/idb/edadb_idb_schema.h` | iDB/shadow table schemas. |
| `src/database/edadb/idb/edadb_idb_init.cpp` | Open DB, configure PK behavior and initialize root tables. |
| `src/database/edadb/idb/shadow/*` | Reduced storage views and reference/derived-state rebuild. |
| `def_write_edadb.cpp` | Traverse active iDB and insert root objects. |
| `def_read_edadb.cpp` | Read roots and append rebuilt objects into active iDB. |

Detailed field mappings belong in [adapter class documentation](../../src/database/edadb/docs/idb-adapter/README.md), not in this architecture overview.

## 7. Current Research Boundary

EDADB can currently target the stage checkpoint boundary, but a complete resume system must distinguish:

- DEF/iDB source data;
- references resolved against LEF/layout;
- derived iDB state rebuilt by setters/helpers;
- tool-private IDs, caches and solver state;
- Liberty/SDC/SPEF analysis inputs.

This distinction is the bridge from DEF roundtrip work to incremental or cross-tool EDADB research.
