# Tcl/Python 接口与工具调度

这一层回答：脚本命令如何进入 C++，又如何被分发到 DataManager 或 EDA 点工具。

## 相关类及执行过程

### Tcl 命令注册

总入口：`src/interface/tcl/tcl_register.h`

`registerCommands()` 把每类命令注册到 Tcl shell。每个命令通常有三个部分：

1. `tcl_register_xxx.h`：注册命令名。
2. `tcl_xxx.h/.cpp`：定义 `TclCmd` 派生类。
3. `CmdXXX::exec()`：真正执行 C++ 调用。

例子：数据库命令

位置：`src/interface/tcl/tcl_idb/tcl_register_idb.h`

| Tcl 命令 | C++ command class | 作用 |
| --- | --- | --- |
| `idb_init` | `CmdInitIdb` | 通过 `ToolManager::idbStart` 初始化 iDB。 |
| `tech_lef_init` | `CmdInitTechLef` | 读 tech LEF。 |
| `lef_init` | `CmdInitLef` | 读普通 LEF。 |
| `def_init` | `CmdInitDef` | 读 DEF。 |
| `verilog_init` | `CmdInitVerilog` | 读 Verilog netlist。 |
| `def_save` | `CmdSaveDef` | 保存 DEF。 |
| `netlist_save` | `CmdSaveNetlist` | 保存 Verilog。 |
| `gds_save` | `CmdSaveGDS` | 保存 GDS。 |
| `edadb_read` | `CmdEdadbRead` | 从 EDADB 读 DEF/design。 |
| `edadb_write` | `CmdEdadbWrite` | 把 DEF/design 写入 EDADB。 |

例子：placement 命令

位置：`src/interface/tcl/tcl_ipl/tcl_register_pl.h`

| Tcl 命令 | 作用 |
| --- | --- |
| `run_placer` | 完整运行 iPL placement。 |
| `run_filler` | 插入 filler。 |
| `run_incremental_flow` | 增量 placement flow。 |
| `placer_run_gp` | global placement。 |
| `placer_run_lg` | legalization。 |
| `placer_run_dp` | detailed placement。 |

例子：routing 命令

位置：`src/interface/tcl/tcl_irt/include/tcl_register_irt.h`

| Tcl 命令 | 作用 |
| --- | --- |
| `init_rt` | 初始化 routing 数据和配置。 |
| `run_rt` | 执行 routing flow，例如 global/detail routing。 |
| `destroy_rt` | 释放 routing 运行状态。 |

注意：`ToolManager::autoRunRouter` 当前实现中只打印禁用提示，但 sky130 脚本使用的是 `init_rt/run_rt/destroy_rt` 入口，因此追 iRT 应从 `src/interface/tcl/tcl_irt` 继续进入 `src/operation/iRT`。

### `iplf::ToolManager`

位置：`src/platform/tool_manager/tool_manager.h/.cpp`

`ToolManager` 是工具调度门面。它不实现算法，主要把接口调用转给各点工具 API/IO 单例。

关键转发：

| 方法 | 转发目标 | EDA 阶段 |
| --- | --- | --- |
| `idbStart(config)` | `dmInst->init(config)` | 数据库初始化。 |
| `idbSave(name)` | `dmInst->save(name)` | 保存设计。 |
| `autoRunPlacer(config)` | `plInst->runPlacement(config)` | Placement。 |
| `runPlacerFiller(config)` | `plInst->runFillerInsertion(config)` | Filler insertion。 |
| `autoRunCTS(config, work_dir)` | `ctsInst->runCTS(config, work_dir)` | Clock tree synthesis。 |
| `autoRunSTA(config)` | `staInst->autoRunSTA(config)` | Static timing analysis。 |
| `RunTODrv/RunTOHold/RunTOSetup` | `iTOInst->...` | Timing optimization。 |
| `RunNOFixFanout(config)` | `iNOInst->runNOFixFanout(config)` | Netlist optimization。 |
| `autoRunDRC(config, path, has_init)` | `drcInst->runDRC(...)` | DRC。 |
| `autoRunPower(config)` | `powerInst->autoRunPower(config)` | Power analysis。 |
| `autoRunPNP(config)` | `pnpInst->runPNP(config)` | Power network planning。 |

### Python 接口

位置：`src/interface/python`

Python 侧使用 pybind11 暴露类似接口。例如：

| Python 注册文件 | 暴露内容 |
| --- | --- |
| `py_idb/py_register_idb.h` | `idb_init`、`lef_init`、`def_init`、`def_save` 等。 |
| `py_ipl/py_register_ipl.h` | `run_placer(config)`。 |
| `py_icts/py_register_icts.h` | `run_cts(cts_config, cts_work_dir)`。 |
| `py_ista/py_register_ista.h` | STA 相关接口。 |

Python 与 Tcl 的区别主要是前端语言不同，后端依然共享 DataManager、ToolManager 和点工具 API。

## EDA 抽象与 iEDA 类的对应关系

| EDA 抽象 | Tcl/Python 表达 | iEDA C++ 入口 |
| --- | --- | --- |
| 读工艺库 | `tech_lef_init`、`lef_init` | `CmdInitTechLef`、`CmdInitLef`、`DataManager::readLef` |
| 读设计 | `def_init`、`verilog_init` | `CmdInitDef`、`CmdInitVerilog`、`DataManager::readDef/readVerilog` |
| 初始化 flow | `flow_init`、`db_init` | flow/config/db Tcl command classes |
| Placement | `run_placer` | `CmdPlacerAutoRun`、`ToolManager::autoRunPlacer`、`plInst` |
| CTS | `run_cts` | `CmdCTSAutoRun`、`ToolManager::autoRunCTS`、`ctsInst` |
| Routing | `init_rt/run_rt/destroy_rt` | `tcl_irt` command classes、`iRT` data manager/engine |
| STA | `init_sta/run_sta/report_sta` | `tcl_ista` command classes、`staInst` |
| Timing optimization | `run_to_drv/run_to_hold/run_to_setup` | `tcl_ito`、`ToolManager`、`iTOInst` |
| DRC | `run_drc` / DRC API commands | `tcl_idrc`、`drcInst` |
| Save output | `def_save/netlist_save/gds_save/json_save` | `DataManager`、`IdbBuilder`、writer classes |

## 阅读建议

查任何命令时使用这个模式：

```text
rg 'registerTclCmd.*命令名' src/interface/tcl
  -> 找到 CmdXXX
  -> rg 'CmdXXX::exec' src/interface/tcl
  -> 看 exec() 调用了 ToolManager 还是 DataManager
  -> 继续进入 src/platform 或 src/operation
```
