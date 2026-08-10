---
title: iEDA 架构学习地图
aliases:
  - iEDA Architecture Map
tags:
  - iEDA
  - architecture
  - index
---

# iEDA 架构学习地图

> [!info] Source baseline
> 源码位置和行号已按 `edadb-idb-dev/sort-abc-no-sort-d @ 77fbe5c67` 核对。

[文档首页](../README.md) · [代码主线](10_complete_tutorial.md) · [点工具](04_eda_tools.md) · [脚本 Flow](05_scripts_flow.md) · [EDADB](06_edadb_integration.md)

## One-Sentence Model

iEDA 通过 Tcl 脚本调用 C++ 点工具；LEF/DEF/Verilog 被构建为 active iDB，点工具通常再建立自己的内部数据库，运行算法后把结果回写 iDB，最后由 DEF/Verilog/GDS writer 形成阶段 checkpoint。

## The Five Layers

| 层 | 主要目录 | 核心职责 |
| --- | --- | --- |
| Runtime | `src/apps`、`src/platform/flow` | 启动 iEDA 并进入 Tcl。 |
| Interface | `src/interface/tcl` | 注册命令并把 Tcl 参数转换成 C++ 调用。 |
| Platform | `src/platform` | `DataManager` 管数据，`ToolManager` 转发点工具。 |
| Database | `src/database` | iDB 对象、parser、writer、builder/service、EDADB adapter。 |
| Operations | `src/operation` | iFP、iPL、iCTS、iRT、iSTA、iTO、iDRC 等算法实现。 |

## Runtime Chain

```text
iEDA -script stage.tcl
  -> main()
  -> Flow::runTcl()
  -> tcl_start()
  -> registerCommands()
  -> CmdXXX::exec()
  -> DataManager or ToolManager/tool API
  -> active iDB
  -> tool-private database/algorithm
  -> write back iDB
  -> DEF/Verilog/GDS/report
```

Code anchors:

- [`main()`](../../src/apps/ieda_main.cpp#L35)
- [`Flow::runTcl()`](../../src/platform/flow/flow.cpp#L60)
- [`tcl_start()`](../../src/interface/tcl/tcl_main.h#L35)
- [`registerCommands()`](../../src/interface/tcl/tcl_register.h#L63)

## Two Data Lifetimes

> [!important] Do not mix these lifetimes
> **单个 Tcl 脚本进程内**，点工具共享同一个 active iDB。**完整 sky130 flow 中**，`run_iEDA.sh` 为每个阶段重新启动 iEDA，因此阶段间通过 DEF/Verilog 文件传递状态。

- 第一个 iFP 进程：[`run_iEDA.sh`](../../scripts/design/sky130_gcd/run_iEDA.sh#L25)
- 后续阶段列表：[`run_iEDA.sh`](../../scripts/design/sky130_gcd/run_iEDA.sh#L28)
- 每个阶段重新执行 `./iEDA -script`：[`run_iEDA.sh`](../../scripts/design/sky130_gcd/run_iEDA.sh#L42)

## Recommended Reading

1. [iEDA 代码与数据流主线](10_complete_tutorial.md)：先获得全局模型。
2. [入口与运行时](01_entry_runtime.md)：只追 `main -> Tcl`。
3. [iDB 数据模型](02_data_model_idb.md)：理解 LEF/DEF 进入哪里。
4. [Tcl/Python 调度](03_interface_and_dispatch.md)：学会从命令找实现。
5. [EDA 点工具](04_eda_tools.md)：理解每个工具的数据桥接模式。
6. [示例脚本 Flow](05_scripts_flow.md)：理解阶段输入、输出和进程边界。
7. [EDADB 集成](06_edadb_integration.md)：理解数据库持久化在架构中的位置。

## Code Review Method

追一个功能时固定回答五个问题：

1. Tcl 命令在哪里注册？
2. `CmdXXX::exec()` 调用 DataManager、ToolManager 还是工具 API？
3. 工具读取哪些 `IdbLayout/IdbDesign` 对象？
4. 工具是否创建私有数据库，结果在哪里回写 iDB？
5. 哪个 writer 把更新后的 iDB 保存为下一阶段输入？

这五个问题也是判断 EDADB 能否替代某个阶段数据通道的基础。
