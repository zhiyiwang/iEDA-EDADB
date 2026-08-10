---
title: iEDA 点工具与 iDB 数据桥接
aliases:
  - iEDA Point Tools
tags:
  - iEDA
  - point-tool
  - iDB
---

# iEDA 点工具与 iDB 数据桥接

> [!info] Source baseline
> 源码链接按 `edadb-idb-dev/sort-abc-no-sort-d @ 77fbe5c67` 核对。

[学习地图](00_learning_map.md) · [代码主线](10_complete_tutorial.md) · [脚本 Flow](05_scripts_flow.md)

## 1. Do Not Assume Every Tool Uses iDB the Same Way

| Pattern | Tool examples | Data behavior |
| --- | --- | --- |
| Direct mutation | iFP, parts of iNO | 直接取得 active iDB 并增删改对象。 |
| Copy and write back | iPL | iDB 转成工具私有 DB，算法结束后统一回写。 |
| Mapped private model | iCTS, iSTA/iTO | 工具对象与 iDB 对象通过 wrapper/adapter 建立映射。 |
| Import and reconstruct | iRT | 导入 routing DB，结束时重建 `IdbNet` wires/segments。 |
| Read-only analysis | normal iSTA, iDRC, power analysis | 从 iDB 构建分析模型，主要输出 report/violation。 |

这一区分决定了 EDADB 优化应该放在哪里：恢复完整 active iDB、加速工具 import，或者支持工具结果增量回写。

## 2. iFP: Directly Build Floorplan Objects

Tcl/API chain:

- command execution: [`TclFpInit::exec()`](../../src/interface/tcl/tcl_ifp/tcl_init_ifp.cpp#L73)
- call `initDie/initCore`: [`tcl_init_ifp.cpp`](../../src/interface/tcl/tcl_ifp/tcl_init_ifp.cpp#L141)
- call `makeTracks`: [`tcl_init_ifp.cpp`](../../src/interface/tcl/tcl_ifp/tcl_init_ifp.cpp#L197)

Direct active-iDB updates:

- die points: [`InitDesign::initDie()`](../../src/operation/iFP/source/module/init_design/init_design.cpp#L32)
- core-site lookup and row reset: [`InitDesign::initCore()`](../../src/operation/iFP/source/module/init_design/init_design.cpp#L45)
- row creation loop and alternating orientation: [`InitDesign::initCore()`](../../src/operation/iFP/source/module/init_design/init_design.cpp#L87)
- X/Y track grids: [`InitDesign::makeTracks()`](../../src/operation/iFP/source/module/init_design/init_design.cpp#L114)

Reads: LEF sites/layers and current die/layout. Writes: `IdbDie`, `IdbCore`, `IdbRows`, `IdbTrackGridList`.

## 3. iNO: Direct Netlist ECO

iNO scans the timing model, maps a violating timing net back to `IdbNet`, then directly edits iDB:

- scan fanout violations: [`FixFanout::fixFanout()`](../../src/operation/iNO/source/module/fix_fanout/FixFanout.cpp#L32)
- create split net: [`FixFanout::makeNet()`](../../src/operation/iNO/source/module/fix_fanout/FixFanout.cpp#L130)
- create buffer instance from LEF master: [`FixFanout::makeInstance()`](../../src/operation/iNO/source/module/fix_fanout/FixFanout.cpp#L137)
- disconnect/reconnect pins: [`FixFanout.cpp`](../../src/operation/iNO/source/module/fix_fanout/FixFanout.cpp#L151)

Reads: timing fanout plus iDB connectivity. Writes: instance list, net list, pin-to-net relations.

## 4. iPL: Copy, Optimize, Write Back

Entry:

- Tcl: [`CmdPlacerAutoRun::exec()`](../../src/interface/tcl/tcl_ipl/tcl_ipl.cpp#L46)
- platform forwarding: [`ToolManager::autoRunPlacer()`](../../src/platform/tool_manager/tool_manager.cpp#L184)

Import:

- wrapper entry: [`IDBWrapper::wrapIDBData()`](../../src/operation/iPL/source/module/wrapper/IDBWrapper.cc#L211)
- copy layout resources: [`IDBWrapper::wrapLayout()`](../../src/operation/iPL/source/module/wrapper/IDBWrapper.cc#L223)
- copy design instances/nets/regions: [`IDBWrapper::wrapDesign()`](../../src/operation/iPL/source/module/wrapper/IDBWrapper.cc#L394)

Writeback:

- update existing/new instances: [`IDBWrapper::writeBackSourceDatabase()`](../../src/operation/iPL/source/module/wrapper/IDBWrapper.cc#L759)
- write status: [`IDBWrapper.cc`](../../src/operation/iPL/source/module/wrapper/IDBWrapper.cc#L779)
- write coordinate: [`IDBWrapper.cc`](../../src/operation/iPL/source/module/wrapper/IDBWrapper.cc#L819)

Reads: die/core/rows/sites/cell masters/instances/nets/regions. Writes: placement status, orientation, coordinates and inserted filler-cell instances.

> [!note] Filler terminology
> `run_filler` calls `insertLayoutFiller()` through [`PlacerIO::runFillerInsertion()`](../../src/platform/tool_manager/tool_api/ipl_io/ipl_io.cpp#L124). The filler algorithm creates placement `Instance` objects in [`MapFiller::add_filler_instance()`](../../src/operation/iPL/source/module/filler/src/MapFiller.cpp#L194), which are later handled through placement writeback. This is not automatically the same object family as DEF `FILLS` / `IdbFillList`.

## 5. iCTS: Maintain CTS-to-iDB Mappings

Flow:

- `readData -> routing -> evaluate`: [`CTSAPI::runCTS()`](../../src/operation/iCTS/api/CTSAPI.cc#L74)
- create `CtsDBWrapper` from active builder: [`CTSAPI::init()`](../../src/operation/iCTS/api/CTSAPI.cc#L186)
- import configured clock nets: [`CTSAPI::readData()`](../../src/operation/iCTS/api/CTSAPI.cc#L222)
- convert iDB clock nets, pins and instances: [`CtsDBWrapper::read()`](../../src/operation/iCTS/source/data_manager/io/CtsDBWrapper.cc#L40)

Updates:

- create iDB clock buffer instance: [`CtsDBWrapper::makeInstance()`](../../src/operation/iCTS/source/data_manager/io/CtsDBWrapper.cc#L111)
- create iDB clock net: [`CtsDBWrapper::makeNet()`](../../src/operation/iCTS/source/data_manager/io/CtsDBWrapper.cc#L127)
- update cell master through mapped instance: [`CtsDBWrapper::updateCell()`](../../src/operation/iCTS/source/data_manager/io/CtsDBWrapper.cc#L101)

Reads: placed clock nets, sink pins, coordinates, LEF cell masters. Writes: clock buffers, clock nets and updated connectivity.

## 6. iSTA and iTO: Timing Model plus ECO Writeback

Normal STA:

- convert active iDB to timing netlist: [`StaIO::readIdb()`](../../src/platform/tool_manager/tool_api/ista_io/ista_io.cpp#L178)
- build/report timing without normal physical writeback: [`StaIO::runSTA()`](../../src/platform/tool_manager/tool_api/ista_io/ista_io.cpp#L151)

Timing optimization:

- DRV/setup/hold API dispatch: [`ToApi.cpp`](../../src/operation/iTO/api/ToApi.cpp#L61)
- disconnect old sinks: [`StaIO::insertBuffer()`](../../src/platform/tool_manager/tool_api/ista_io/ista_io.cpp#L402)
- create buffer and net: [`ista_io.cpp`](../../src/platform/tool_manager/tool_api/ista_io/ista_io.cpp#L434)
- set iDB placement and update timing engine: [`ista_io.cpp`](../../src/platform/tool_manager/tool_api/ista_io/ista_io.cpp#L450)

Reads: iDB netlist/placement, Liberty, SDC and optional parasitics. Writes during optimization: instances, nets, pin relations, initial buffer placement.

## 7. iRT: Import Routing State and Rebuild Net Wires

Import:

- initialize routing data manager: [`RTInterface::initRT()`](../../src/operation/iRT/interface/RTInterface.cpp#L68)
- import config and active iDB: [`RTInterface::input()`](../../src/operation/iRT/interface/RTInterface.cpp#L526)
- wrap die/row/layers/vias/obstacles/nets: [`RTInterface::wrapDatabase()`](../../src/operation/iRT/interface/RTInterface.cpp#L548)

Algorithm stages:

- pin access, topology, layer assignment, routing, track assignment, detailed routing: [`RTInterface::runRT()`](../../src/operation/iRT/interface/RTInterface.cpp#L117)

Writeback:

- `destroyRT()` triggers `RTDM.output()`: [`RTInterface::destroyRT()`](../../src/operation/iRT/interface/RTInterface.cpp#L165)
- routing DataManager calls `RTInterface::output()`: [`DataManager::output()`](../../src/operation/iRT/source/data_manager/DataManager.cpp#L67)
- output dispatches track/gcell/net results: [`RTInterface::output()`](../../src/operation/iRT/interface/RTInterface.cpp#L1263)
- gather detailed route result: [`RTInterface::outputNetList()`](../../src/operation/iRT/interface/RTInterface.cpp#L1341)
- clear old iDB wires: [`RTInterface.cpp`](../../src/operation/iRT/interface/RTInterface.cpp#L1361)
- create routed wire and append segments: [`RTInterface.cpp`](../../src/operation/iRT/interface/RTInterface.cpp#L1376)

Reads: layer rules, tracks, vias, rows, obstacles, pins and nets. Writes: `IdbRegularWire`, segments, points, vias and route state.

## 8. iDRC: Convert iDB Geometry to Analysis Shapes

- environment shapes from instances, pins and special nets: [`DRCInterface::buildEnvShapeList()`](../../src/operation/iDRC/interface/DRCInterface.cpp#L638)
- routed signal shapes from regular wires: [`DRCInterface::buildResultShapeList()`](../../src/operation/iDRC/interface/DRCInterface.cpp#L876)

Normal iDRC produces violations/reports rather than replacing the iDB geometry it checks.

## 9. Stage Summary

| Stage | Main iDB input | Tool-private model | Main iDB update |
| --- | --- | --- | --- |
| iFP | layout sites/layers + logical design | little/no full copy | die/core/rows/tracks/constraints |
| iNO | nets/pins/instances + timing | timing netlist | buffer/net/pin connectivity |
| iPL | layout + design | placement DB | instance status/orient/coordinate |
| iCTS | placed clock connectivity | CTS design/tree | clock buffers/nets/connections |
| iSTA | design + Liberty/SDC/SPEF | timing graph | report only in normal STA |
| iTO | timing graph + active iDB | timing/optimization model | ECO instances/nets/connections |
| iRT | routing resources + design geometry | routing DB | regular wires/segments/vias |
| iDRC | environment + routed geometry | DRC shapes/index | violation report |

## 10. Reading Checklist

For each tool, verify in source:

1. import entry;
2. exact iDB containers traversed;
3. private object identity/mapping;
4. algorithm output container;
5. iDB writeback method;
6. stage output script.

This checklist is more reliable than assuming a tool directly operates on `IdbDesign` because it includes an `Idb*` header.
