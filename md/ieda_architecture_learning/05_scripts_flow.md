# 示例脚本 Flow

这一层回答：真实运行时，脚本怎样把 LEF/DEF/Lib/SDC 和各点工具串起来。

## 相关类及执行过程

示例目录：`scripts/design/sky130_gcd`

核心内容：

| 文件/目录 | 作用 |
| --- | --- |
| `run_iEDA.py`、`run_iEDA.sh` | 组织执行 iEDA flow。 |
| `config.py` | 设置设计目录、foundry、结果目录等路径。 |
| `iEDA_config/*.json` | 各工具配置，例如 DB、FP、PL、CTS、RT、TO、DRC、PNP。 |
| `script/DB_script` | 读 LEF/DEF/Verilog/Lib/SDC/SPEF，保存 DEF/GDS/JSON/Verilog。 |
| `script/iFP_script` | floorplan flow。 |
| `script/iPL_script` | placement/filler/legalization flow。 |
| `script/iCTS_script` | CTS flow。 |
| `script/iRT_script` | routing flow。 |
| `script/iSTA_script` | STA flow。 |
| `script/iTO_script` | timing optimization flow。 |
| `script/iDRC_script` | DRC flow。 |

### 典型 DB 初始化脚本

位置：`scripts/design/sky130_gcd/script/DB_script/run_db.tcl`

执行过程：

```text
flow_init -config flow_config.json
db_init -config db_default_config.json -output_dir_path result
source db_path_setting.tcl
source db_init_lef.tcl
def_init -path result/iRT_result.def
def_save -path result/data_out.def
netlist_save -path result/data_out.v
flow_exit
```

对应 C++：

| Tcl 命令 | C++ 执行 |
| --- | --- |
| `flow_init` | flow/config Tcl command，初始化平台配置。 |
| `db_init` | DB 配置初始化。 |
| `tech_lef_init` | `CmdInitTechLef::exec()` -> `dmInst->readLef(..., true)` |
| `lef_init` | `CmdInitLef::exec()` -> `dmInst->readLef(...)` |
| `def_init` | `CmdInitDef::exec()` -> `dmInst->readDef(...)` |
| `def_save` | `CmdSaveDef::exec()` -> `dmInst->saveDef(...)` |
| `netlist_save` | `DataManager::saveVerilog(...)` |

### Netlist 到 DEF

位置：`scripts/design/sky130_gcd/script/DB_script/run_netlist_to_def.tcl`

执行过程：

```text
读 flow/db config
读 LEF
verilog_init -path result/verilog/gcd.v -top gcd
def_save -path result/netlist_result.def
netlist_save -path result/netlist_result.v
```

对应 EDA 含义：

```text
Verilog logical netlist
  -> RustVerilogRead
  -> IdbDesign instance/net/pin
  -> DEF physical design shell
```

这里的 DEF 不一定已经完成 placement/routing，它更像把逻辑网表放入 iDB 后导出的物理设计容器。

### Placement flow

位置：`scripts/design/sky130_gcd/script/iPL_script/run_iPL.tcl`

典型过程：

```text
读 config / LEF / input DEF
run_placer -config pl_default_config.json
def_save -path result/iPL_result.def
netlist_save -path result/iPL_result.v
report_db
feature_summary
flow_exit
```

对应 C++：

```text
run_placer
  -> CmdPlacerAutoRun::exec()
  -> ToolManager::autoRunPlacer()
  -> plInst->runPlacement()
  -> 更新 IdbInstance 坐标
```

### CTS flow

位置：`scripts/design/sky130_gcd/script/iCTS_script/run_iCTS.tcl`

典型过程：

```text
读 Lib/SDC/LEF
def_init -path result/iPL_lg_result.def
run_cts -config cts_default_config.json -work_dir result/cts
def_save -path result/iCTS_result.def
netlist_save -path result/iCTS_result.v
report_db
feature_summary
```

对应 C++：

```text
run_cts
  -> CmdCTSAutoRun::exec()
  -> ToolManager::autoRunCTS()
  -> ctsInst->runCTS()
  -> 插入 clock tree 相关 instance/net
```

### Routing flow

位置：`scripts/design/sky130_gcd/script/iRT_script/run_iRT.tcl`

典型过程：

```text
读 Lib/SDC/LEF
def_init -path result/iPL_lg_result.def 或后续输入 DEF
init_drc_api
init_rt ...
run_rt -flow "dr"
destroy_rt
destroy_drc_api
def_save -path result/iRT_result.def
netlist_save -path result/iRT_result.v
```

对应 C++：

```text
init_rt/run_rt/destroy_rt
  -> src/interface/tcl/tcl_irt
  -> src/operation/iRT
  -> 更新 IdbNet wire/via
```

### Timing optimization flow

位置：`scripts/design/sky130_gcd/script/iTO_script`

常见脚本：

| 脚本 | 输入 | 命令 | 输出 |
| --- | --- | --- | --- |
| `run_iTO_drv.tcl` | `iCTS_result.def` | `run_to_drv` | `iTO_drv_result.def` |
| `run_iTO_hold.tcl` | `iTO_drv_result.def` | `run_to_hold` | `iTO_hold_result.def` |
| `run_iTO_setup.tcl` | `iTO_hold_result.def` | `run_to_setup` | `iTO_setup_result.def` |

对应 EDA 含义：

```text
timing/DRV analysis
  -> buffer insertion / netlist modification
  -> iDB design update
  -> save DEF + Verilog
```

## EDA 抽象与 iEDA 类的对应关系

| 脚本阶段 | 输入文件 | iEDA 抽象 | 输出文件 |
| --- | --- | --- | --- |
| DB init | LEF/DEF/Verilog | `IdbLayout`、`IdbDesign` | DEF/Verilog/GDS/JSON |
| Netlist to DEF | Verilog + LEF | `RustVerilogRead`、`IdbDesign` | `netlist_result.def` |
| Floorplan | initial design + FP config | die/core/row/blockage/macro | `iFP_result.def` |
| Placement | floorplan DEF + PL config | instance placement | `iPL_result.def` |
| CTS | placed DEF + Lib/SDC | clock tree | `iCTS_result.def` |
| TO | CTS/placed DEF + timing config | buffer/netlist update | `iTO_*_result.def` |
| Routing | placed/optimized DEF + RT config | wire/via routing | `iRT_result.def` |
| DRC | routed DEF | violation check | `detail.drc` |
| GDS export | routed/final DEF | layout stream | `final_design.gds2` |

## 阅读建议

脚本是最好的“反向索引”。建议每次按下面顺序读：

```text
某个 run_*.tcl
  -> 找其中第一个核心 Tcl 命令
  -> 找 registerTclCmd
  -> 找 CmdXXX::exec()
  -> 看进入 ToolManager、DataManager 还是某个 operation API
```

这样读比从 `src/operation` 目录硬啃源码轻很多。
