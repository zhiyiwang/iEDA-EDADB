---
title: iEDA 代码与数据流主线
aliases:
  - iEDA Code-First Tutorial
tags:
  - iEDA
  - architecture
  - code-reading
---

# iEDA 代码与数据流主线

> [!info] Source baseline
> 本文只描述当前源码行为。源码链接和行号已按 `edadb-idb-dev/sort-abc-no-sort-d @ 77fbe5c67` 核对。

[学习地图](00_learning_map.md) · [iDB](02_data_model_idb.md) · [点工具](04_eda_tools.md) · [脚本 Flow](05_scripts_flow.md) · [EDADB](06_edadb_integration.md)

## 1. Architecture in One Diagram

```text
Tcl/Python scripts
  -> Interface command classes
  -> Platform managers
       DataManager: LEF/DEF/Verilog and output
       ToolManager: point-tool dispatch
  -> active iDB
       IdbLayout: technology/library/physical resources
       IdbDesign: current design objects and implementation state
  -> tool-private model
  -> algorithm
  -> write back active iDB
  -> DEF / Verilog / GDS / reports / EDADB
```

| Module | Directory | Code role |
| --- | --- | --- |
| Runtime | `src/apps`, `src/platform/flow` | 解析参数并启动 Tcl shell。 |
| Interface | `src/interface` | 命令注册、参数检查、调用 C++ API。 |
| Platform | `src/platform` | 数据和点工具的统一门面。 |
| Database | `src/database` | iDB、builder/service、parser/writer、EDADB。 |
| Operations | `src/operation` | 点工具内部模型与算法。 |

## 2. Runtime Entry

`iEDA -script stage.tcl` 的真实调用链：

1. [`main()`](../../src/apps/ieda_main.cpp#L35) 解析 `-v`、`-log`、`-script`。
2. [`main()`](../../src/apps/ieda_main.cpp#L67) 调用 `plfInst->runTcl()`。
3. [`Flow::runTcl()`](../../src/platform/flow/flow.cpp#L60) 转入 `tcl_start()`。
4. [`tcl_start()`](../../src/interface/tcl/tcl_main.h#L35) 获取 `UserShell`。
5. [`tcl_start()`](../../src/interface/tcl/tcl_main.h#L43) 设置 `registerCommands` 并执行 Tcl。
6. [`registerCommands()`](../../src/interface/tcl/tcl_register.h#L63) 注册 DB、PL、CTS、TO、RT、STA 等命令组。

因此，阅读任意功能应先找 Tcl command，而不是直接进入算法目录。

## 3. LEF and DEF Build the Active iDB

### 3.1 LEF path

```text
tech_lef_init / lef_init
  -> CmdInitTechLef / CmdInitLef
  -> DataManager::readLef()
  -> DataManager::initLef()
  -> IdbBuilder::buildLef()
  -> LefRead::createDb()
  -> IdbLayout
```

Code anchors:

- Tcl tech LEF command: [`CmdInitTechLef::exec()`](../../src/interface/tcl/tcl_idb/tcl_db_file.cpp#L72)
- Tcl cell LEF command: [`CmdInitLef::exec()`](../../src/interface/tcl/tcl_idb/tcl_db_file.cpp#L110)
- tech LEF then cell LEF: [`DataManager::readLef()`](../../src/platform/data_manager/idm.cpp#L63)
- save service and layout pointer: [`DataManager::initLef()`](../../src/platform/data_manager/idm_init.cpp#L33)
- create/reuse `IdbLefService` and invoke `LefRead`: [`IdbBuilder::buildLef()`](../../src/database/manager/builder/builder.cpp#L158)

LEF contributes layer, site, cell master, via rule and other technology/library objects. Root object: [`IdbLayout`](../../src/database/data/design/IdbLayout.h#L56).

### 3.2 DEF path

```text
def_init
  -> CmdInitDef::exec()
  -> DataManager::readDef()
  -> DataManager::initDef()
  -> IdbBuilder::buildDef()
  -> DefRead::createDb()
  -> DEF callbacks
  -> buildNet() / buildBus()
  -> IdbDesign + shared IdbLayout
```

Code anchors:

- Tcl command: [`CmdInitDef::exec()`](../../src/interface/tcl/tcl_idb/tcl_db_file.cpp#L148)
- require existing builder/LEF/layout: [`DataManager::readDef()`](../../src/platform/data_manager/idm.cpp#L98)
- build service and active design pointer: [`DataManager::initDef()`](../../src/platform/data_manager/idm_init.cpp#L41)
- create `IdbDefService` with existing LEF layout: [`IdbBuilder::buildDef()`](../../src/database/manager/builder/builder.cpp#L100)
- register DEF callbacks: [`DefRead::createDb()`](../../src/database/manager/builder/def_builder/def_read.cpp#L70)
- execute `defrRead`: [`DefRead::createDb()`](../../src/database/manager/builder/def_builder/def_read.cpp#L134)
- rebuild net/bus relations after parsing: [`IdbBuilder::buildDef()`](../../src/database/manager/builder/builder.cpp#L122)

DEF creates the current implementation state under [`IdbDesign`](../../src/database/data/design/IdbDesign.h#L57), while rows/tracks/die still live on the shared layout side.

## 4. The Two iDB Roots

| Root | Typical source | Representative objects | Meaning |
| --- | --- | --- | --- |
| `IdbLayout` | tech LEF, LEF, floorplan DEF | layer, site, row, track, gcell, cell master, via rule, die/core | Technology/library and physical resources. |
| `IdbDesign` | DEF, Verilog, point-tool updates | instance, IO pin, net, special net, blockage, region, group, fill, routed wire | Current chip design and implementation state. |

Code anchors:

- `IdbLayout::get_rows()`: [`IdbLayout.h`](../../src/database/data/design/IdbLayout.h#L69)
- `IdbLayout::get_track_grid_list()`: [`IdbLayout.h`](../../src/database/data/design/IdbLayout.h#L71)
- `IdbDesign::get_instance_list()`: [`IdbDesign.h`](../../src/database/data/design/IdbDesign.h#L69)
- `IdbDesign::get_net_list()`: [`IdbDesign.h`](../../src/database/data/design/IdbDesign.h#L71)
- `IdbDesign::get_special_net_list()`: [`IdbDesign.h`](../../src/database/data/design/IdbDesign.h#L77)

> [!note] Shared object graph
> `IdbDesign` holds or references objects that point back to LEF/layout objects, such as instance-to-cell-master, pin-to-layer and via-to-layer. A database restore must rebuild these references against the active layout rather than duplicate technology objects.

## 5. Four Point-Tool Data Patterns

### 5.1 Direct iDB mutation: iFP

iFP gets the active design/layout and directly resets or creates objects:

- reset and set die points: [`InitDesign::initDie()`](../../src/operation/iFP/source/module/init_design/init_design.cpp#L32)
- lookup LEF sites and build core/rows: [`InitDesign::initCore()`](../../src/operation/iFP/source/module/init_design/init_design.cpp#L45)
- create every row in active iDB: [`InitDesign::initCore()`](../../src/operation/iFP/source/module/init_design/init_design.cpp#L87)
- create X/Y track grids: [`InitDesign::makeTracks()`](../../src/operation/iFP/source/module/init_design/init_design.cpp#L114)

### 5.2 Copy, optimize, write back: iPL

iPL constructs an internal placement database:

- command entry: [`CmdPlacerAutoRun::exec()`](../../src/interface/tcl/tcl_ipl/tcl_ipl.cpp#L46)
- platform forwarding: [`ToolManager::autoRunPlacer()`](../../src/platform/tool_manager/tool_manager.cpp#L184)
- wrap active iDB: [`IDBWrapper::wrapIDBData()`](../../src/operation/iPL/source/module/wrapper/IDBWrapper.cc#L211)
- copy layout: [`IDBWrapper::wrapLayout()`](../../src/operation/iPL/source/module/wrapper/IDBWrapper.cc#L223)
- copy design: [`IDBWrapper::wrapDesign()`](../../src/operation/iPL/source/module/wrapper/IDBWrapper.cc#L394)
- write coordinates/status/orient back: [`IDBWrapper::writeBackSourceDatabase()`](../../src/operation/iPL/source/module/wrapper/IDBWrapper.cc#L759)

### 5.3 Mapped private model: iCTS and iSTA/iTO

iCTS builds `CtsDesign`, while wrapper maps CTS objects to iDB objects:

- top flow: [`CTSAPI::runCTS()`](../../src/operation/iCTS/api/CTSAPI.cc#L74)
- read active iDB: [`CTSAPI::readData()`](../../src/operation/iCTS/api/CTSAPI.cc#L222)
- import clock nets/pins/instances: [`CtsDBWrapper::read()`](../../src/operation/iCTS/source/data_manager/io/CtsDBWrapper.cc#L40)
- create new iDB clock instance: [`CtsDBWrapper::makeInstance()`](../../src/operation/iCTS/source/data_manager/io/CtsDBWrapper.cc#L111)
- create new iDB clock net: [`CtsDBWrapper::makeNet()`](../../src/operation/iCTS/source/data_manager/io/CtsDBWrapper.cc#L127)

iSTA builds a timing netlist through `TimingIDBAdapter`:

- convert iDB to timing netlist: [`StaIO::readIdb()`](../../src/platform/tool_manager/tool_api/ista_io/ista_io.cpp#L178)
- report-only STA entry: [`StaIO::runSTA()`](../../src/platform/tool_manager/tool_api/ista_io/ista_io.cpp#L151)
- optimization path creates net/instance and updates both timing and iDB: [`StaIO::insertBuffer()`](../../src/platform/tool_manager/tool_api/ista_io/ista_io.cpp#L402)

### 5.4 Import routing DB and rebuild wires: iRT

- import config and iDB: [`RTInterface::input()`](../../src/operation/iRT/interface/RTInterface.cpp#L526)
- wrap die/row/layer/via/obstacle/net: [`RTInterface::wrapDatabase()`](../../src/operation/iRT/interface/RTInterface.cpp#L548)
- run PA/TG/LA/SR/TA/DR: [`RTInterface::runRT()`](../../src/operation/iRT/interface/RTInterface.cpp#L117)
- convert route results to iDB segments: [`RTInterface::outputNetList()`](../../src/operation/iRT/interface/RTInterface.cpp#L1341)
- clear old wires and append routed segments: [`RTInterface::outputNetList()`](../../src/operation/iRT/interface/RTInterface.cpp#L1361)

For detailed tool-by-tool reading, continue with [EDA 点工具](04_eda_tools.md).

## 6. Analysis Tools

iDRC reads iDB geometry and converts it to its own `ids::Shape` representation:

- instances, special nets and IO pins: [`DRCInterface::buildEnvShapeList()`](../../src/operation/iDRC/interface/DRCInterface.cpp#L638)
- regular routed nets: [`DRCInterface::buildResultShapeList()`](../../src/operation/iDRC/interface/DRCInterface.cpp#L876)

Normal DRC/STA runs primarily produce violations or reports. iTO/iNO/PNP-style optimization tools are different because they may add instances/nets or update physical geometry.

## 7. Process Boundaries and Checkpoints

The sky130 driver launches one iEDA process per stage:

- first iFP process: [`run_iEDA.sh`](../../scripts/design/sky130_gcd/run_iEDA.sh#L25)
- stage sequence: [`run_iEDA.sh`](../../scripts/design/sky130_gcd/run_iEDA.sh#L28)
- loop starts a fresh executable: [`run_iEDA.sh`](../../scripts/design/sky130_gcd/run_iEDA.sh#L42)

```text
LEF + Verilog -> iFP -> iFP_result.def
  -> iNO -> fanout-fixed DEF/Verilog
  -> iPL -> iPL_result.def
  -> iCTS -> iCTS_result.def
  -> iTO -> iTO_*_result.def
  -> incremental legalization -> iPL_lg_result.def
  -> iRT -> iRT_result.def
  -> DRC/STA reports
  -> filler instances -> final DEF
  -> GDS export
```

> [!important] Correct interpretation
> “点工具共享同一个 iDB”只描述单个 iEDA 进程内部。完整脚本中，下一阶段通常重新读取上一阶段写出的 DEF/Verilog，因此 DEF 是阶段 checkpoint，也是当前 EDADB 最直接可以替代或增强的持久化边界。

## 8. Saving the Updated Design

```text
def_save
  -> CmdSaveDef::exec()
  -> DataManager::saveDef()
  -> IdbBuilder::saveDef()
  -> DefWrite::writeChip()
```

- Tcl entry: [`CmdSaveDef::exec()`](../../src/interface/tcl/tcl_idb/tcl_db_file.cpp#L228)
- DataManager forwarding: [`DataManager::saveDef()`](../../src/platform/data_manager/idm_save.cpp#L60)
- construct writer: [`IdbBuilder::saveDef()`](../../src/database/manager/builder/builder.cpp#L263)
- canonical DEF section order: [`DefWrite::writeChip()`](../../src/database/manager/builder/def_builder/def_write.cpp#L205)

The writer serializes the already-updated active iDB; it does not rerun the point tool.

## 9. Where EDADB Fits

```text
active iDB -> DefWriteEdadb -> SQLite
SQLite + LEF/DEF context -> DefReadEdadb -> active iDB
active iDB -> unchanged point-tool interfaces
```

- read entry: [`DataManager::readDefFromEdadb()`](../../src/platform/data_manager/idm_edadb.cpp#L13)
- write entry: [`DataManager::saveDefToEdadb()`](../../src/platform/data_manager/idm_edadb.cpp#L33)
- rebuild normal `IdbDefService`: [`IdbBuilder::buildDefFromEdadb()`](../../src/database/manager/builder/builder.cpp#L341)
- EDADB read orchestration: [`DefReadEdadb::createDbFromEdadb()`](../../src/database/manager/builder/def_builder/def_read_edadb.cpp#L24)
- EDADB write orchestration: [`DefWriteEdadb::writeDb2Edadb()`](../../src/database/manager/builder/def_builder/def_write_edadb.cpp#L26)

Current point tools do not query SQLite directly. EDADB first restores the same iDB object graph expected by iFP/iPL/iCTS/iRT/iSTA/iDRC.

## 10. Efficient Code Reading

For one Tcl command:

```text
register command
  -> CmdXXX::exec()
  -> platform/tool API
  -> iDB import or direct access
  -> algorithm
  -> iDB writeback
  -> output script command
```

Recommended concrete path:

1. [`run_iPL.tcl`](../../scripts/design/sky130_gcd/script/iPL_script/run_iPL.tcl#L35)
2. [`CmdPlacerAutoRun::exec()`](../../src/interface/tcl/tcl_ipl/tcl_ipl.cpp#L46)
3. [`ToolManager::autoRunPlacer()`](../../src/platform/tool_manager/tool_manager.cpp#L184)
4. [`IDBWrapper::wrapIDBData()`](../../src/operation/iPL/source/module/wrapper/IDBWrapper.cc#L211)
5. [`IDBWrapper::writeBackSourceDatabase()`](../../src/operation/iPL/source/module/wrapper/IDBWrapper.cc#L759)
6. [`DefWrite::writeChip()`](../../src/database/manager/builder/def_builder/def_write.cpp#L205)

This path shows the complete read–compute–update–serialize cycle without entering every placement algorithm at once.
