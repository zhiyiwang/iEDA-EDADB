# iEDA-EDADB Codex Handoff

This file records the validated iEDA + EDADB branch mapping, directory layout, and the minimal DEF read/write validation flow.

## Current Objective

Keep the A/B branch relationship clear and validate old iEDA + EDADB states by using the iEDA superproject gitlink as the source of truth for EDADB.

Correct method:

1. Checkout the target iEDA commit or branch.
2. Run `git submodule update --init --recursive`.
3. Build iEDA when runtime validation is needed.
4. Run only the EDADB DEF write/read demo from `bin/`.

Required runtime validation command:

```bash
cd bin/
pwd
bash /home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/demo/demo.sh 2>&1 | tee run.out
```

Do not manually pair EDADB commits across iEDA commits when a superproject gitlink is available. Do not run the full physical-design flow unless explicitly requested.

## Branch/Version Knowledge

Canonical branch states:

| Label | iEDA branch / commit | iEDA local describe/tag | EDADB implementation location | iEDA adapter location | EDADB gitlink commit | EDADB local describe/tag | Status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| original | `origin/master @ 0074352` | no EDADB milestone tag | none | none | none | none | Official iEDA baseline; no EDADB integration |
| A | `origin/edadb @ 2f028c426bb9b435151b3ae57118c0cbcc5680cb` | `2f028c426`; no local tag found | `src/database/edadb/core` | `src/database/edadb/idb` | `8c724ef1c1afc3c0ab7d03135fdad6afe6b85e34` | `8c724ef`; local branches include `shadow-non-owning` | Canonical non-owning branch layout |
| B | `origin/edadb-shadow-transitive @ 664829eef911447f9738d928c8167f32a94bbaf8` | `664829eef`; no local tag found | `src/third_party/edadb` | `src/database/edadb` | `f121400763073faf322161c5b083efad3c00a20b` | `f121400`; local submodule status showed `remotes/origin/main-backup` | Shadow-transitive milestone; DEF roundtrip OK |
| C | local `edadb-idb` from `origin/edadb @ 2f028c426bb9b435151b3ae57118c0cbcc5680cb` | work in progress | `src/database/edadb/core` | `src/database/edadb/idb` | `293c162ff4f36d71f0735e30cb22df6e227e48f6` | EDADB `main` local describe `milestone/dbmap-legacy-before-orm-refactor-59-g293c162` | Empty EDADB read/write framework adapted to new DbTableOp API |

Important notes:

- A does not use `src/third_party/edadb` in its canonical branch layout. EDADB core is under `src/database/edadb/core`.
- A’s iEDA adapter code is under `src/database/edadb/idb`.
- B uses `src/third_party/edadb` for the EDADB implementation repository and `src/database/edadb` for the iEDA-side adapter/shadow code.
- The milestone identity should be the iEDA branch/commit. EDADB commit/path are supporting evidence from `git submodule update --init --recursive`.
- Earlier manual/historical pairings, such as `abc469e7e + src/third_party/edadb`, are not the canonical A layout and should not be used to describe A.

## Milestone: C `edadb-idb` Empty Framework

Purpose:

- Use A-specific layout as the new development baseline: EDADB core in `src/database/edadb/core`, iEDA adapter in `src/database/edadb/idb`.
- Track EDADB `main` through `.gitmodules` at `src/database/edadb/core`; the intended pinned core commit is `293c162`.
- First establish a minimal compile/run framework before restoring per-object persistence.

Implemented framework policy:

- `def_read_edadb.*` keeps the EDADB initialization phase and `createDbByEdadb()` call.
- `def_read_edadb.*` calls stable adapter wrapper `idb::edadb_adapter::initReadDb()` instead of directly depending on the old `edadb::init2read()` entry.
- `createDbByEdadb()` currently performs no `readIdbXXX()` calls; all iDB objects are read from DEF text through `createDbByDef()`.
- `createDbByDef()` restores the basic DEF parser callbacks for version, busbit chars, units, design, die area, rows, tracks, gcell grid, vias, components, pins, blockages, groups, fills, nets, and special nets.
- `def_write_edadb.*` calls stable adapter wrapper `idb::edadb_adapter::initWriteDb()` instead of directly depending on the old `edadb::init2write()` entry.
- `writeChip2Edadb()` currently performs no `writeIdbXXX()` calls; object persistence remains dormant for later stepwise restoration.
- `writeDbSynthesis2Edadb()` also keeps `writeIdbDesign()` dormant so this stage does not write any iDB object into EDADB.
- Runtime log lines are prefixed with `[EDADB-IDB]` in init/read/write paths so the demo log clearly shows EDADB DB path, core API mode, schema initialization, and skipped object persistence.
- Do not delete dormant EDADB adapter code. Mark inactive code and temporary stubs with `//EDADB_TODO` so the preserved implementation can be restored later.
- When DEF read/write object paths are disabled, keep schema/init/shadow disabled too. Do not leave `TABLE4*` registrations or `EDADB_INIT_TABLE(...)` active for classes whose `readIdbXXX/writeIdbXXX` paths are currently skipped.

Adapter API boundary:

- `src/database/edadb/idb/edadb_idb_init.*` now owns the stable iEDA-facing init wrappers:
  - `idb::edadb_adapter::initReadDb(const char*)`
  - `idb::edadb_adapter::initWriteDb(const char*)`
- Existing `edadb::init2read()` and `edadb::init2write()` remain as compatibility shims that delegate to the new wrappers.
- EDADB `293c162` removes old `DbMap` from the public API and uses the new `DbTableOp` facade. The C adapter init path now uses `edadb::createTable<T>()` / `edadb::getTableDef<T>()`.
- The old `readIdbXXX()` / `writeIdbXXX()` bodies are kept under `#if 0` with `//EDADB_TODO` markers; active stub definitions also carry `//EDADB_TODO` and return success while logging that the object path is skipped.
- `edadb::CppStrings` is defined in the adapter layer for later shadow restoration, but its `TABLE4CLASS(..., "CppStr", ...)` mapping is also under `#if 0 //EDADB_TODO` in the empty-framework phase.
- `edadb_idb_schema.h` currently wraps the active A schema groups in `#if 0 //EDADB_TODO`, so no iDB object `TABLE4*` metadata is registered in the C empty-framework phase.
- `edadb_idb_shadow.h` currently wraps the basic shadow aggregation includes in `#if 0 //EDADB_TODO`; individual shadow files stay on disk for later restoration.
- `edadb_idb_init.cpp::initAllTables()` logs empty-framework mode and intentionally performs no `EDADB_INIT_TABLE(...)` calls.
- EDADB core is built as an iEDA submodule, so its own `CMakeLists.txt` must derive `EDADB_HOME` from the EDADB core source directory, not iEDA's top-level `CMAKE_SOURCE_DIR`.
- Public iEDA headers such as `def_read_edadb.h` and `def_write_edadb.h` must not include the heavy EDADB aggregate headers. Keep `edadb.h` and schema/shadow includes in adapter `.cpp` files to avoid leaking EDADB Boost.Fusion dependencies into unrelated iEDA modules.

Validation result:

- `bash build.sh -j40` completed successfully on local branch `edadb-idb` after the EDADB core CMake include-path fix and public-header cleanup.
- The previous required demo command completed successfully from `bin/` before the latest C empty-framework synchronization.
- `run.out` previously ended with `Input def and output def are the same.`
- The required demo command was rerun by the user after the latest full-build fixes and passed.
- In this C framework state, EDADB write initializes the database and creates schema tables, but intentionally performs no `writeIdbXXX()` object writes.
- EDADB read initializes the database and calls `createDbByEdadb()`, but intentionally performs no `readIdbXXX()` object reads; the iDB content is rebuilt from DEF text via restored callbacks in `createDbByDef()`.
- After schema/init synchronization, EDADB write initializes the database but does not create iDB object tables in this empty-framework phase.

Current milestone:

- C `edadb-idb` init code base is compile-verified and demo-verified.
- A targeted local compile check passed for `db_edadb` and `def_builder` after the API adaptation:
  `cmake --build build -j40 --target db_edadb def_builder`.

Validation command remains exactly:

```bash
cd bin/
pwd
bash /home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/demo/demo.sh 2>&1 | tee run.out
```

## GitHub Verification Status

GitHub remotes configured locally:

- iEDA: `git@github.com:zhiyiwang/iEDA-EDADB.git`
- EDADB: `git@github.com:zhiyiwang/edadb.git`

Live GitHub verification attempted from terminal with the configured proxy environment:

```bash
timeout 45 git ls-remote https://github.com/zhiyiwang/iEDA-EDADB.git refs/heads/master refs/heads/edadb refs/heads/edadb-shadow-transitive
timeout 45 git ls-remote https://github.com/zhiyiwang/edadb.git refs/heads/shadow-non-owning refs/heads/shadow-transitive refs/heads/main-backup refs/heads/main
curl -I --connect-timeout 15 --max-time 30 https://github.com/zhiyiwang/iEDA-EDADB
```

Observed result:

- SSH `git ls-remote` hung and had to be interrupted.
- HTTPS `git ls-remote` timed out.
- `curl` failed with `Proxy CONNECT aborted due to timeout`.
- Therefore the file-tree verification below is based on local GitHub remote-tracking refs (`origin/master`, `origin/edadb`, `origin/edadb-shadow-transitive`) and the EDADB submodule object database already fetched from GitHub.

This is still the correct local evidence for repository structure, but if a live GitHub fetch is required later, run it from a host terminal with confirmed proxy connectivity.

## iEDA vs Original Master

`origin/master` has no EDADB submodule, no `src/database/edadb`, and no `scripts/edadb/demo` validation entry.

Compared with `origin/master`, EDADB branches add these areas:

- `scripts/edadb/demo`: Tcl/demo validation harness for DEF to EDADB DB and EDADB DB back to DEF.
- `src/database/manager/builder/def_builder/def_read_edadb.*`: EDADB-backed DEF readback path; converts EDADB tables/shadows back into iDB data.
- `src/database/manager/builder/def_builder/def_write_edadb.*`: EDADB-backed DEF write path; converts selected iDB DEF objects into EDADB tables/shadows.
- `src/interface/tcl/tcl_idb/tcl_db_file.*`: Tcl command integration so scripts can invoke EDADB read/write.
- `src/platform/data_manager/idm_edadb.cpp`: Data-manager bridge that routes iEDA DB operations to EDADB-backed DEF read/write.

A-specific layout:

- `src/database/edadb/core`: EDADB implementation submodule, pinned to `8c724ef`.
- `src/database/edadb/idb`: iEDA/EDADB adapter layer, including schema, init helpers, and iDB shadow conversion code.

B-specific layout:

- `src/third_party/edadb`: EDADB implementation submodule, pinned to `f1214007`.
- `src/database/edadb`: iEDA/EDADB adapter layer for the older shadow-transitive direction, including `edadb_core.*`, `edadb_schema.h`, `edadb_shadow.*`, and `shadow/*`.

Coverage intent:

- A is the non-owning shadow direction and should avoid transitive shadow propagation.
- B has broader class coverage in its adapter layer but is the old shadow-transitive direction.
- NET/SPECIALNET are not the current EDADB persistence focus. The demo can still produce equal DEF through the existing read/write path even when nets are not stored as dedicated EDADB tables.

## Verified File Trees

The following trees are read from local GitHub remote-tracking refs and submodule commits.

### A: `origin/edadb`

`src/database/edadb`:

```text
src/database/edadb/CMakeLists.txt
src/database/edadb/core
src/database/edadb/idb/CMakeLists.txt
src/database/edadb/idb/docs/def_lef_read_func_call.mk
src/database/edadb/idb/docs/def_read.mk
src/database/edadb/idb/docs/lef_read.mk
src/database/edadb/idb/docs/orm.mk
src/database/edadb/idb/edadb_idb.h
src/database/edadb/idb/edadb_idb_helper.cpp
src/database/edadb/idb/edadb_idb_helper.h
src/database/edadb/idb/edadb_idb_init.cpp
src/database/edadb/idb/edadb_idb_init.h
src/database/edadb/idb/edadb_idb_schema.h
src/database/edadb/idb/edadb_idb_shadow.h
src/database/edadb/idb/shadow/shadow_idb_blockage.h
src/database/edadb/idb/shadow/shadow_idb_die.h
src/database/edadb/idb/shadow/shadow_idb_fill.h
src/database/edadb/idb/shadow/shadow_idb_geometry.h
src/database/edadb/idb/shadow/shadow_idb_group.h
src/database/edadb/idb/shadow/shadow_idb_halo.h
src/database/edadb/idb/shadow/shadow_idb_instance.h
src/database/edadb/idb/shadow/shadow_idb_layer_shape.h
src/database/edadb/idb/shadow/shadow_idb_pin.h
src/database/edadb/idb/shadow/shadow_idb_port.h
src/database/edadb/idb/shadow/shadow_idb_term.h
src/database/edadb/idb/shadow/shadow_idb_track_grid.h
src/database/edadb/idb/shadow/shadow_idb_via_master.h
src/database/edadb/idb/todo
```

`src/database/edadb/core @ 8c724ef` selected EDADB core tree:

```text
CMakeLists.txt
demo/CMakeLists.txt
demo/unit-test-composite.cpp
demo/unit-test-shadow.cpp
include/edadb.h
include/edadb/Config.h
include/edadb/Cpp2SqlTypeTrait.h
include/edadb/DbBackendType.h
include/edadb/DbManager.h
include/edadb/DbMap.h
include/edadb/DbMapAll.h
include/edadb/DbMapBase.h
include/edadb/DbMapDbStmtOp.h
include/edadb/DbMapOperation.h
include/edadb/DbMapReader.h
include/edadb/DbMapWriter.h
include/edadb/DbStatement.h
include/edadb/Shadow.h
include/edadb/Singleton.h
include/edadb/SqlStatement.h
include/edadb/SqlStatementStack.h
include/edadb/SqlType.h
include/edadb/StoreProperty.h
include/edadb/Table4Class.h
include/edadb/TraitUtils.h
include/edadb/TypeInfoTrait.h
include/edadb/TypeMetaData.h
include/edadb/TypeStack.h
include/edadb/VecMetaData.h
include/edadb/backend/sqlite/DbManager4Sqlite.h
include/edadb/backend/sqlite/DbStatement4Sqlite.h
include/edadb/backend/sqlite/Macro4Sqlite.h
include/edadb/backend/sqlite/SqlStatement4Sqlite.h
src/core/CMakeLists.txt
src/core/backend/CMakeLists.txt
src/core/backend/sqlite/CMakeLists.txt
src/core/edadb.cpp
```

### B: `origin/edadb-shadow-transitive`

`src/database/edadb`:

```text
src/database/edadb/CMakeLists.txt
src/database/edadb/docs/def_lef_read_func_call.mk
src/database/edadb/docs/def_read.mk
src/database/edadb/docs/lef_read.mk
src/database/edadb/docs/orm.mk
src/database/edadb/edadb_api.h
src/database/edadb/edadb_core.cpp
src/database/edadb/edadb_core.h
src/database/edadb/edadb_schema.h
src/database/edadb/edadb_shadow.cpp
src/database/edadb/edadb_shadow.h
src/database/edadb/shadow/shadow_idb_blockage.h
src/database/edadb/shadow/shadow_idb_die.h
src/database/edadb/shadow/shadow_idb_fill.h
src/database/edadb/shadow/shadow_idb_geometry.h
src/database/edadb/shadow/shadow_idb_group.h
src/database/edadb/shadow/shadow_idb_halo.h
src/database/edadb/shadow/shadow_idb_instance.h
src/database/edadb/shadow/shadow_idb_layer_shape.h
src/database/edadb/shadow/shadow_idb_pin.h
src/database/edadb/shadow/shadow_idb_port.h
src/database/edadb/shadow/shadow_idb_term.h
src/database/edadb/shadow/shadow_idb_track_grid.h
src/database/edadb/shadow/shadow_idb_via.h
src/database/edadb/shadow/shadow_idb_via_master.h
src/database/edadb/todo
```

`src/third_party/edadb @ f1214007` selected EDADB core tree:

```text
CMakeLists.txt
demo/CMakeLists.txt
demo/composite.cpp
include/edadb.h
include/edadb/Config.h
include/edadb/Cpp2SqlTypeTrait.h
include/edadb/DbBackendType.h
include/edadb/DbManager.h
include/edadb/DbMap.h
include/edadb/DbMapAll.h
include/edadb/DbMapBase.h
include/edadb/DbMapDbStmtOp.h
include/edadb/DbMapOperation.h
include/edadb/DbMapReader.h
include/edadb/DbMapWriter.h
include/edadb/DbStatement.h
include/edadb/Shadow.h
include/edadb/Singleton.h
include/edadb/SqlStatement.h
include/edadb/SqlType.h
include/edadb/Table4Class.h
include/edadb/TraitUtils.h
include/edadb/TypeInfoTrait.h
include/edadb/TypeMetaData.h
include/edadb/TypeStack.h
include/edadb/VecMetaData.h
include/edadb/backend/sqlite/DbManager4Sqlite.h
include/edadb/backend/sqlite/DbStatement4Sqlite.h
include/edadb/backend/sqlite/Macro4Sqlite.h
include/edadb/backend/sqlite/SqlStatement4Sqlite.h
src/core/CMakeLists.txt
src/core/backend/CMakeLists.txt
src/core/backend/sqlite/CMakeLists.txt
src/core/edadb.cpp
```

### Shared iEDA Integration Files

A and B both contain these EDADB-facing integration files:

```text
scripts/edadb/demo/demo.sh
scripts/edadb/demo/tcl/def2edadb.tcl
scripts/edadb/demo/tcl/edadb2def.tcl
src/database/manager/builder/def_builder/def_read.cpp
src/database/manager/builder/def_builder/def_read.h
src/database/manager/builder/def_builder/def_read_edadb.cpp
src/database/manager/builder/def_builder/def_read_edadb.h
src/database/manager/builder/def_builder/def_write.cpp
src/database/manager/builder/def_builder/def_write.h
src/database/manager/builder/def_builder/def_write_edadb.cpp
src/database/manager/builder/def_builder/def_write_edadb.h
src/interface/tcl/tcl_idb/tcl_db_file.cpp
src/interface/tcl/tcl_idb/tcl_db_file.h
src/platform/data_manager/idm_edadb.cpp
```

## Milestone A: Canonical Layout Verified

Date: 2026-06-11.

Verified by reading the iEDA tree:

- iEDA branch/commit: `origin/edadb @ 2f028c426bb9b435151b3ae57118c0cbcc5680cb`
- iEDA subject: `edadb: update before re-write`
- iEDA local describe/tag: `2f028c426`; no local tag found
- EDADB core path from gitlink: `src/database/edadb/core`
- EDADB commit from gitlink: `8c724ef1c1afc3c0ab7d03135fdad6afe6b85e34`
- EDADB local describe/tag: `8c724ef`; local branches include `shadow-non-owning`
- Adapter path: `src/database/edadb/idb`

Verified tree evidence:

```text
160000 commit 8c724ef1c1afc3c0ab7d03135fdad6afe6b85e34 src/database/edadb/core
040000 tree ... src/database/edadb/idb
```

Important correction:

- `src/third_party/edadb` should not be used to describe canonical A.
- The earlier `abc469e7e` checkpoint had `src/third_party/edadb`; that was a historical/transitional runnable point, not the canonical A branch layout.

Runtime validation note:

- The user has independently validated A. For future Codex validation, use the same method: checkout A commit/branch, run `git submodule update --init --recursive`, build, then run only the EDADB DEF write/read demo.

## Milestone B: EDADB DEF Roundtrip OK

Date: 2026-06-11.

Validated combination:

- iEDA branch/commit: `origin/edadb-shadow-transitive @ 664829eef911447f9738d928c8167f32a94bbaf8`
- iEDA subject: `iEDA: IdbFill`
- iEDA local describe/tag: `664829eef`; no local tag found
- EDADB core path from gitlink: `src/third_party/edadb`
- EDADB commit from gitlink: `f121400763073faf322161c5b083efad3c00a20b`
- EDADB local describe/tag: `f121400`; local submodule status also showed `remotes/origin/main-backup`
- Adapter path: `src/database/edadb`

Validation setup:

```bash
git checkout 664829e
git submodule update --init --recursive
bash build.sh -j40 2>&1 | tee build.out
```

Runtime command:

```bash
cd bin/
pwd
bash /home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/demo/demo.sh 2>&1 | tee run.out
```

Result:

- `git submodule update --init --recursive` resolved EDADB to `src/third_party/edadb @ f1214007`.
- Build completed successfully with `bash build.sh -j40`.
- Demo exited with code `0`.
- Demo wrote EDADB database to `scripts/edadb/demo/result/edadb.db`.
- Demo read EDADB back and wrote `scripts/design/sky130_gcd/result/iPL_result_edadb.def`.
- Final check reported `Input def and output def are the same.`

Observed B EDADB DB content from the earlier direct check:

- `iDesign`: `gcd | 5.8 | 1000`.
- Core counts: `iRow=39`, `iTrackGridSD=12`, `iViaSD=4`, `iInstSD=1458`, `iPinSD=56`.
- Pin port/detail tables are populated.
- No dedicated `iNetSD` or `iSpecialNetSD` table was observed; this is expected for the current milestone.

## EDADB `edadb.h` API Comparison

This compares the EDADB core public API used by canonical A and B.

| Area | A: EDADB `8c724ef` at `src/database/edadb/core` | B: EDADB `f1214007` at `src/third_party/edadb` | Correspondence |
| --- | --- | --- | --- |
| Header path | `src/database/edadb/core/include/edadb.h` | `src/third_party/edadb/include/edadb.h` | Same public header name, different submodule location |
| Common utilities | `initDatabase`, `executeSql`, `beginTransaction`, `commitTransaction`, `tableExists` | Same names and meanings | Directly compatible |
| `CppStrings` | Defines `edadb::CppStrings` and `TABLE4CLASS(..., "CppStr", ...)` | Same | Directly compatible |
| Table APIs | `createTable(DbMap<StoreType>&)`, `dropTable(DbMap<StoreType>&)` | `createTable(DbMap<T>&)`, `dropTable(DbMap<T>&)` | Same behavior; template parameter semantics differ |
| Writer alias | `DbMapWriter<T> = DbMap<StoreProperty<T>::StoreType>::Writer` | `DbMapWriter<T> = DbMap<T>::Writer` | A maps object type to storage type internally; B expects caller to pass the storage/shadow type directly |
| Insert/update/delete | `insertObject(DbMap<StoreProperty<T>::StoreType>&, T*)`, `insertVector`, `updateObject`, `updateVector`, `deleteObject` | `insertObject(DbMap<T>&, T*)`, `insertVector`, `updateObject`, `updateVector`, `deleteObject` | Same operations; A uses `StoreProperty<T>::StoreType`, B uses direct `T` |
| Reader alias | `DbMapReader<T> = DbMap<StoreProperty<T>::StoreType>::Reader` | `DbMapReader<T> = DbMap<T>::Reader` | Same semantic role; template target changed |
| Scan read | `readNext(reader, dbmap, obj)` | `read2Scan(reader, dbmap, obj)` | Function rename plus direct `DbMap<T>` typing in B |
| Predicate read | `readByPredicate(reader, dbmap, obj, pred)` using `StoreProperty<T>::StoreType` map | `readByPredicate(reader, dbmap, obj, pred)` using direct `DbMap<T>` | Same operation; template target changed |
| Primary-key read | `readByPrimaryKey(DbMap<StoreProperty<T>::StoreType>&, T*)` | `readByPrimaryKey(DbMap<T>&, T*)` | Same operation; template target changed |

Practical implication:

- A’s `edadb.h` API is object-type oriented: callers can pass iEDA-facing object type `T`, and EDADB derives the stored type through `StoreProperty<T>::StoreType`.
- B’s `edadb.h` API is storage/shadow-type oriented: callers work directly with the type used by `DbMap<T>`.
- A keeps this adaptation under `src/database/edadb/idb`; B keeps the older adaptation under `src/database/edadb`.

## Current Local State

Last checked state:

```text
## HEAD (no branch)
?? agent.md
?? build.out
```

Current checkout:

- iEDA: detached `664829eef911447f9738d928c8167f32a94bbaf8`.
- EDADB: `src/third_party/edadb @ f121400763073faf322161c5b083efad3c00a20b`.
- This is B layout: EDADB implementation in `src/third_party/edadb`, adapter in `src/database/edadb`.
- `build.out` is from the B `bash build.sh -j40` validation.
- `bin/run.out` is from the B EDADB DEF write/read demo and ends with `Input def and output def are the same.`
- A previous untracked `src/database/edadb/` leftover was moved to `/tmp/iEDA-EDADB-src-database-edadb-backup-20260611-114447` before switching to B.

## Validation Constraints

- Use `git submodule update --init --recursive` after each iEDA checkout.
- Use `bash build.sh -j40` for builds on this machine.
- Run only the EDADB DEF write/read demo unless the user explicitly asks for another flow.
- Do not clean `build/` or `bin/` by default.
- Do not delete backup or untracked directories unless explicitly requested.
