# Tcl 命令索引与追踪方法

这个文件用于快速从脚本命令定位到 C++ 类和 EDA 阶段。它比 dashboard 更适合日常查阅：先找到命令，再顺着 `CmdXXX::exec()` 进入平台层或点工具。

## 相关类及执行过程

### 通用追踪模板

```text
Tcl command
  -> registerTclCmd(CmdXXX, "command_name")
  -> CmdXXX::exec()
  -> DataManager / ToolManager / operation API
  -> iDB object update or report/output
```

常用搜索命令：

```bash
rg 'registerTclCmd.*run_placer' src/interface/tcl
rg 'CmdPlacerAutoRun::exec' src/interface/tcl
```

### Flow / Config

| Tcl 命令 | 注册位置 | 主要 C++ 类/函数 | 作用 |
| --- | --- | --- | --- |
| `flow_init` | `src/interface/tcl/tcl_config/tcl_register_config.h` | `CmdFlowInitConfig::exec()` | 初始化 flow config。 |
| `db_init` | `src/interface/tcl/tcl_config/tcl_register_config.h` | `CmdDbConfigSetting::exec()` | 初始化 DB 配置、输出目录等。 |
| `flow_exit` | `src/interface/tcl/tcl_flow/tcl_register_flow.h` | `CmdFlowExit::exec()` | 结束 Tcl flow。 |

### iDB / 文件读写

| Tcl 命令 | C++ command class | 后端调用 | EDA 含义 |
| --- | --- | --- | --- |
| `idb_init` | `CmdInitIdb` | `tmInst->idbStart()` -> `dmInst->init()` | 按配置初始化 iDB。 |
| `tech_lef_init` | `CmdInitTechLef` | `dmInst->readLef(..., true)` | 读 tech LEF。 |
| `lef_init` | `CmdInitLef` | `dmInst->readLef(...)` | 读 cell/macro LEF。 |
| `def_init` | `CmdInitDef` | `dmInst->readDef(...)` | 读 DEF design。 |
| `verilog_init` | `CmdInitVerilog` | `dmInst->readVerilog(...)` | 读 Verilog netlist。 |
| `def_save` | `CmdSaveDef` | `dmInst->saveDef(...)` | 输出 DEF。 |
| `netlist_save` | `CmdSaveNetlist` | `dmInst->saveVerilog(...)` | 输出 Verilog。 |
| `gds_save` | `CmdSaveGDS` | `dmInst->saveGDSII(...)` | 输出 GDS。 |
| `json_save` | `CmdSaveJSON` | `dmInst->saveJSON(...)` | 输出 layout JSON。 |
| `edadb_read` | `CmdEdadbRead` | `dmInst->readDefFromEdadb(...)` | 从 EDADB 读设计。 |
| `edadb_write` | `CmdEdadbWrite` | `dmInst->saveDefToEdadb(...)` | 写 EDADB。 |

### Floorplan / PDN

| Tcl 命令 | 注册位置 | EDA 含义 |
| --- | --- | --- |
| `init_floorplan` | `src/interface/tcl/tcl_ifp/tcl_register_fp.h` | 初始化 die/core/row 等 floorplan 信息。 |
| `gern_track` | `src/interface/tcl/tcl_ifp/tcl_register_fp.h` | 生成 track。 |
| `auto_place_pins` | `src/interface/tcl/tcl_ifp/tcl_register_fp.h` | 自动摆放 IO pin。 |
| `place_port` | `src/interface/tcl/tcl_ifp/tcl_register_fp.h` | 指定 port/pin 位置。 |
| `add_placement_blockage` | `src/interface/tcl/tcl_ifp/tcl_register_fp.h` | 添加 placement blockage。 |
| `add_routing_blockage` | `src/interface/tcl/tcl_ifp/tcl_register_fp.h` | 添加 routing blockage。 |
| `tapcell` | `src/interface/tcl/tcl_ifp/tcl_register_fp.h` | 插入 tapcell。 |
| `create_grid`、`create_stripe` | `src/interface/tcl/tcl_ipdn/tcl_register_pdn.h` | 创建 PDN grid/stripe。 |

### Placement

| Tcl 命令 | C++ command class | 后端调用 | EDA 含义 |
| --- | --- | --- | --- |
| `run_placer` | `CmdPlacerAutoRun` | `ToolManager::autoRunPlacer()` | 完整 placement。 |
| `run_filler` | `CmdPlacerFiller` | `ToolManager::runPlacerFiller()` | filler insertion。 |
| `placer_run_gp` | `CmdPlacerRunGP` | iPL API | global placement。 |
| `placer_run_lg` | `CmdPlacerRunLG` | iPL API | legalization。 |
| `placer_run_dp` | `CmdPlacerRunDP` | iPL API | detailed placement。 |
| `placer_check_legality` | `CmdPlacerCheckLegality` | `ToolManager::checkLegality()` | 合法性检查。 |

### CTS / STA / TO

| Tcl 命令 | C++ command class | 后端调用 | EDA 含义 |
| --- | --- | --- | --- |
| `run_cts` | `CmdCTSAutoRun` | `ToolManager::autoRunCTS()` | clock tree synthesis。 |
| `cts_report` | `CmdCTSReport` | `ToolManager::reportCTS()` | CTS 报告。 |
| `init_sta` | `CmdSTAInit` | `ToolManager::initSTA()` | 初始化 STA。 |
| `run_sta` | `CmdSTARun` | `ToolManager::runSTA()` / `autoRunSTA()` | 静态时序分析。 |
| `report_sta` | `CmdSTAReport` | STA report API | 输出时序报告。 |
| `read_liberty` | `ista::CmdReadLiberty` | iSTA shell command | 读 Liberty。 |
| `read_sdc` | `ista::CmdReadSdc` | iSTA shell command | 读 SDC。 |
| `read_spef` | `ista::CmdReadSpef` | iSTA shell command | 读 parasitic。 |
| `run_to_drv` | `CmdTORunDrv` | `ToolManager::RunTODrv()` | 修复 DRV。 |
| `run_to_hold` | `CmdTORunHold` | `ToolManager::RunTOHold()` | 修复 hold。 |
| `run_to_setup` | `CmdTORunSetup` | `ToolManager::RunTOSetup()` | 修复 setup。 |

### Routing / DRC / Power / PNP

| Tcl 命令 | 注册位置 | EDA 含义 |
| --- | --- | --- |
| `init_rt` | `src/interface/tcl/tcl_irt/include/tcl_register_irt.h` | 初始化 routing。 |
| `run_rt` | `src/interface/tcl/tcl_irt/include/tcl_register_irt.h` | 执行 routing flow。 |
| `destroy_rt` | `src/interface/tcl/tcl_irt/include/tcl_register_irt.h` | 释放 routing 状态。 |
| `run_egr` | `src/interface/tcl/tcl_irt/include/tcl_register_irt.h` | early/global routing 相关入口。 |
| `init_drc` | `src/interface/tcl/tcl_idrc/include/tcl_register_idrc.h` | 初始化 DRC。 |
| `run_drc` | `src/interface/tcl/tcl_idrc/include/tcl_register_idrc.h` | 执行 DRC。 |
| `save_drc` | `src/interface/tcl/tcl_idrc/include/tcl_register_idrc.h` | 保存 DRC detail。 |
| `run_power` | `src/interface/tcl/tcl_ipw/tcl_register_power.h` | 功耗分析。 |
| `report_power` | `src/interface/tcl/tcl_ipw/tcl_register_power.h` | 功耗报告。 |
| `report_ir_drop` | `src/interface/tcl/tcl_ipw/tcl_register_power.h` | IR drop 报告。 |
| `run_pnp` | `src/interface/tcl/tcl_ipnp/tcl_register_pnp.h` | power network planning。 |

## EDA 抽象与 iEDA 类的对应关系

| 抽象层 | 命令例子 | 主要 C++ 落点 |
| --- | --- | --- |
| 文件/数据库 | `lef_init`、`def_init`、`def_save` | `DataManager`、`IdbBuilder` |
| 物理实现 | `init_floorplan`、`run_placer`、`run_cts`、`run_rt` | `iFP`、`iPL`、`iCTS`、`iRT` |
| 时序与优化 | `run_sta`、`run_to_drv`、`run_to_hold` | `iSTA`、`iTO` |
| 验证与分析 | `run_drc`、`run_power`、`report_ir_drop` | `iDRC`、`iPA/iPW`、`iIR` |
| 电源网络 | `create_grid`、`create_stripe`、`run_pnp` | `iPDN`、`iPNP` |
| 持久化实验 | `edadb_read`、`edadb_write` | `DataManager`、`IdbBuilder`、`DefReadEdadb`、`DefWriteEdadb` |
