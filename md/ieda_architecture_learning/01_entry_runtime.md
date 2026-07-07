# 入口与运行时

这一层回答：用户执行 `iEDA -script xxx.tcl` 后，程序如何进入 Tcl 解释器，并让 Tcl 命令可以调用 C++ 后端。

## 相关类及执行过程

### `main`

位置：`src/apps/ieda_main.cpp`

执行过程：

1. `ieda::Time::start()` 启动计时。
2. 遍历命令行参数：
   - `-v` 打印 git version。
   - `-log` 初始化日志目录。
   - `-script` 后面的参数被传给 Tcl interpreter。
3. 调用 `plfInst->runTcl(argc, argv)`。

这里的 `plfInst` 是 `iplf::Flow::getInstance()` 风格的全局单例宏，表示平台 flow 入口。

### `iplf::Flow`

位置：`src/platform/flow/flow.h`、`src/platform/flow/flow.cpp`

关键方法：

| 方法 | 作用 |
| --- | --- |
| `initFlow(string flow_config)` | 读取 flow config，目前 GUI 初始化代码大多注释掉。 |
| `run(int argc, char** argv)` | 直接转到 `runTcl`。 |
| `runTcl(int argc, char** argv)` | 调用 `tcl::tcl_start(argc, argv)`。 |

执行链路：

```text
main()
  -> plfInst->runTcl()
  -> tcl::tcl_start()
```

### `tcl::tcl_start`

位置：`src/interface/tcl/tcl_main.h`

执行过程：

1. 如果打开 `BUILD_GUI`，先调用 `tmInst->guiInit()`。
2. 获取 `ieda::UserShell::getShell()`。
3. 调用 `shell->set_init_func(registerCommands)`。
4. 调用 `shell->userMain(tcl_argc, tcl_argv)`，进入 Tcl shell 主循环。

这里的关键点是：Tcl shell 启动时会执行 `registerCommands`，所以所有 C++ Tcl 命令都在 shell 运行前完成注册。

### `tcl::registerCommands`

位置：`src/interface/tcl/tcl_register.h`

它顺序注册这些命令组：

```text
Config
Flow
DB / iDB
Instance operation
Floorplan
PDN
Placement
CTS
Netlist Optimization
Timing Optimization
Routing
DRC
STA
Power
Report / Feature / Eval / ECO / Vectorization
PNP
Notification
```

例如：

| 注册函数 | 对应功能 |
| --- | --- |
| `registerCmdDB()` | `idb_init`、`lef_init`、`def_init`、`verilog_init`、`def_save`、`edadb_read`、`edadb_write` 等。 |
| `registerCmdPlacer()` | `run_placer`、`run_filler`、`placer_run_gp`、`placer_run_lg` 等。 |
| `registerCmdCTS()` | `run_cts`、`cts_report`、`cts_save_tree` 等。 |
| `registerCmdRT()` | `init_rt`、`run_rt`、`destroy_rt` 等。 |
| `registerCmdSTA()` | `init_sta`、`run_sta`、`report_sta` 等。 |
| `registerCmdTO()` | `run_to`、`run_to_drv`、`run_to_hold`、`run_to_setup` 等。 |

## EDA 抽象与 iEDA 类的对应关系

| EDA 抽象 | 用户看到的形式 | iEDA 入口类/函数 |
| --- | --- | --- |
| Flow script | `iEDA -script run_iPL.tcl` | `main`、`Flow::runTcl` |
| Shell / Interpreter | Tcl 命令行或脚本 | `UserShell`、`tcl_start` |
| EDA command | `lef_init`、`run_placer`、`run_cts` | `TclCmd` 派生类，注册在 `tcl_register_*` |
| Runtime dispatch | 命令转 C++ 调用 | `ToolManager`、`DataManager` |
| Batch flow | 多个 Tcl 脚本顺序执行 | `scripts/design/*/run_iEDA.py`、`run_iEDA.sh` |

## 阅读建议

第一遍只需要记住一条线：

```text
ieda_main.cpp
  -> Flow::runTcl
  -> tcl_start
  -> registerCommands
  -> 某个 CmdXXX::exec()
```

之后你看任何 Tcl 命令，都可以按这个模式追：先找 `registerTclCmd(..., "命令名")`，再找对应 `CmdXXX::exec()`。
