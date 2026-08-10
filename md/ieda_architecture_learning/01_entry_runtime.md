---
title: iEDA 入口与运行时
tags:
  - iEDA
  - runtime
  - Tcl
---

# iEDA 入口与运行时

> [!info] Source baseline
> 行号按 `edadb-idb-dev/sort-abc-no-sort-d @ 77fbe5c67` 核对。

[学习地图](00_learning_map.md) · [代码主线](10_complete_tutorial.md) · [接口调度](03_interface_and_dispatch.md)

## Runtime Chain

```text
iEDA -script stage.tcl
  -> main()
  -> Flow::runTcl()
  -> tcl_start()
  -> UserShell::userMain()
  -> registerCommands()
  -> CmdXXX::exec()
```

## Code Anchors

### `main()`

- entry: [`src/apps/ieda_main.cpp`](../../src/apps/ieda_main.cpp#L35)
- parse `-v`, `-log`, `-script`: [`ieda_main.cpp`](../../src/apps/ieda_main.cpp#L43)
- call `plfInst->runTcl()`: [`ieda_main.cpp`](../../src/apps/ieda_main.cpp#L67)

`main()` does not initialize LEF/DEF or a point tool. It only prepares runtime options and enters the flow/Tcl layer.

### `Flow::runTcl()`

- direct Tcl forwarding: [`src/platform/flow/flow.cpp`](../../src/platform/flow/flow.cpp#L60)

`Flow` is a platform entry facade here; it is not the physical-design algorithm pipeline.

### `tcl_start()`

- get `UserShell`: [`src/interface/tcl/tcl_main.h`](../../src/interface/tcl/tcl_main.h#L35)
- install `registerCommands` and execute shell: [`tcl_main.h`](../../src/interface/tcl/tcl_main.h#L43)

### `registerCommands()`

- registration entry: [`src/interface/tcl/tcl_register.h`](../../src/interface/tcl/tcl_register.h#L63)
- DB commands: [`tcl_register.h`](../../src/interface/tcl/tcl_register.h#L72)
- placement and CTS: [`tcl_register.h`](../../src/interface/tcl/tcl_register.h#L84)
- TO, RT and STA: [`tcl_register.h`](../../src/interface/tcl/tcl_register.h#L93)

## Process Boundary

The sky130 driver starts a fresh process for each Tcl stage:

- first process: [`run_iEDA.sh`](../../scripts/design/sky130_gcd/run_iEDA.sh#L25)
- loop over later stages: [`run_iEDA.sh`](../../scripts/design/sky130_gcd/run_iEDA.sh#L42)

So static singletons such as `dmInst` and `tmInst` are process-local. They persist across commands in one Tcl script, not across the complete shell flow.

## How to Trace a Command

1. Search command registration in `src/interface/tcl`.
2. Open the matching `CmdXXX::exec()`.
3. Identify whether it calls `DataManager`, `ToolManager`, or a tool API directly.
4. Continue to the iDB import/writeback boundary.

Example: [`CmdPlacerAutoRun::exec()`](../../src/interface/tcl/tcl_ipl/tcl_ipl.cpp#L46) calls [`ToolManager::autoRunPlacer()`](../../src/platform/tool_manager/tool_manager.cpp#L184).
