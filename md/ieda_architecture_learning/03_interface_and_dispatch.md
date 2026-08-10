---
title: iEDA Tcl 接口与工具调度
tags:
  - iEDA
  - Tcl
  - dispatch
---

# iEDA Tcl 接口与工具调度

> [!info] Source baseline
> 行号按 `edadb-idb-dev/sort-abc-no-sort-d @ 77fbe5c67` 核对。

[学习地图](00_learning_map.md) · [入口](01_entry_runtime.md) · [点工具](04_eda_tools.md)

## Command Shape

```text
registerTclCmd("name")
  -> CmdXXX::check()
  -> CmdXXX::exec()
  -> DataManager / ToolManager / direct tool API
```

Global registration entry: [`registerCommands()`](../../src/interface/tcl/tcl_register.h#L63).

## Data Commands

| Tcl command | C++ entry | Next layer |
| --- | --- | --- |
| `tech_lef_init` | [`CmdInitTechLef::exec()`](../../src/interface/tcl/tcl_idb/tcl_db_file.cpp#L72) | `DataManager::readLef(..., true)` |
| `lef_init` | [`CmdInitLef::exec()`](../../src/interface/tcl/tcl_idb/tcl_db_file.cpp#L110) | `DataManager::readLef()` |
| `def_init` | [`CmdInitDef::exec()`](../../src/interface/tcl/tcl_idb/tcl_db_file.cpp#L148) | `DataManager::readDef()` |
| `def_save` | [`CmdSaveDef::exec()`](../../src/interface/tcl/tcl_idb/tcl_db_file.cpp#L228) | `DataManager::saveDef()` |
| `edadb_read` | [`CmdEdadbRead::exec()`](../../src/interface/tcl/tcl_idb/tcl_db_file.cpp#L531) | `DataManager::readDefFromEdadb()` |
| `edadb_write` | [`CmdEdadbWrite::exec()`](../../src/interface/tcl/tcl_idb/tcl_db_file.cpp#L584) | `DataManager::saveDefToEdadb()` when DB path is present |

## Point-Tool Commands

| Tool | Tcl/API entry | Dispatch behavior |
| --- | --- | --- |
| iFP | [`TclFpInit::exec()`](../../src/interface/tcl/tcl_ifp/tcl_init_ifp.cpp#L73) | Direct calls to `FpApi`; not a generic ToolManager algorithm. |
| iPL | [`CmdPlacerAutoRun::exec()`](../../src/interface/tcl/tcl_ipl/tcl_ipl.cpp#L46) | `ToolManager::autoRunPlacer()` -> `plInst`. |
| iCTS | [`CmdCTSAutoRun::exec()`](../../src/interface/tcl/tcl_icts/tcl_cts.cpp#L41) | `ToolManager::autoRunCTS()` -> CTS API. |
| iRT | `init_rt/run_rt/destroy_rt` command group | Directly drives `RTInterface`; do not follow disabled `autoRunRouter()` as the main sky130 path. |
| iSTA | STA command group | Drives `StaIO`/timing engine. |
| iTO | TO command group | Drives `ToApi` and timing/iDB adapters. |

## `ToolManager` Is a Facade

Examples:

- placement forwarding: [`ToolManager::autoRunPlacer()`](../../src/platform/tool_manager/tool_manager.cpp#L184)
- netlist optimization forwarding: [`ToolManager::RunNOFixFanout()`](../../src/platform/tool_manager/tool_manager.cpp#L224)
- timing optimization forwarding: [`ToolManager::autoRunTO()`](../../src/platform/tool_manager/tool_manager.cpp#L232)
- CTS forwarding: [`ToolManager::autoRunCTS()`](../../src/platform/tool_manager/tool_manager.cpp#L260)

The algorithm and tool-private database remain under `src/operation/<tool>`.

## Trace Recipe

For a Tcl command:

1. find registration;
2. inspect option `check()`;
3. inspect `exec()`;
4. identify manager/API call;
5. locate iDB import and writeback in the operation module.

Do not stop at `ToolManager`: it generally only forwards the call.
