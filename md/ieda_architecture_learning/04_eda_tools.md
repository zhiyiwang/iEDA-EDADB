# EDA 点工具与后端阶段

这一层回答：数字后端每个阶段在 iEDA 中对应哪个模块，以及它们怎样共享 iDB 数据。

## 相关类及执行过程

### 平台调度视角

大多数工具由 Tcl 命令进入 `ToolManager`，再由 `ToolManager` 转发到对应点工具的 API/IO 单例。

```text
Tcl command
  -> CmdXXX::exec()
  -> ToolManager::autoRunXXX()
  -> xxxInst / XxxApi / XxxIO
  -> src/operation/iXXX
  -> 读取或修改 iDB
  -> DataManager 保存结果
```

也有例外，例如 iRT 的常用脚本入口是 `init_rt/run_rt/destroy_rt`，需要从 `src/interface/tcl/tcl_irt` 直接追到 `src/operation/iRT`。

### iFP: Floorplan

位置：`src/operation/iFP`

典型输入：

- 工艺 LEF、cell LEF。
- 初始 DEF 或 Verilog 转 DEF。
- floorplan 配置，例如 die/core、IO、macro、tapcell/blockage 等。

典型输出：

- 带 die/core/row/macro/blockage 等 floorplan 信息的 DEF。

EDA 抽象：

| 概念 | iEDA 对应 |
| --- | --- |
| Die/Core | `IdbLayout::get_die()`、`IdbCore`、DataManager 的 die/core 操作 |
| Row/Site | `IdbRows`、`IdbSites` |
| Macro placement | `IdbInstance` 的位置和状态 |
| Blockage | `IdbPlacementBlockage`、`IdbBlockageList` |

### iPL: Placement

位置：`src/operation/iPL`

入口：

- Tcl: `run_placer`、`placer_run_gp`、`placer_run_lg`、`placer_run_dp`
- 平台转发：`ToolManager::autoRunPlacer()` 调用 `plInst->runPlacement(config, enableJsonOutput)`

典型过程：

```text
读入 iDB design/layout
  -> global placement
  -> legalization
  -> detailed placement
  -> filler insertion
  -> 更新 IdbInstance 坐标和 placement status
  -> def_save / netlist_save
```

EDA 抽象：

| 概念 | iEDA 对应 |
| --- | --- |
| Cell instance | `IdbInstance`、`IdbInstanceList` |
| Net connectivity | `IdbNet`、`IdbPin` |
| Placement row/site | `IdbRows`、`IdbSites` |
| Legalization | iPL 内部 legalizer，结果回写 instance 坐标 |
| Filler | `run_filler`，回写 filler instance |

### iCTS: Clock Tree Synthesis

位置：`src/operation/iCTS`

入口：

- Tcl: `run_cts -config ... -work_dir ...`
- 平台转发：`ToolManager::autoRunCTS()` 调用 `ctsInst->runCTS(config, work_dir)`

典型过程：

```text
读取 placement 后的 iDB
  -> 找 clock net / sink pins
  -> 插入 clock buffer/inverter
  -> 生成 clock tree 拓扑和线段
  -> 更新 iDB net/instance
  -> 保存 DEF/Verilog/report
```

EDA 抽象：

| 概念 | iEDA 对应 |
| --- | --- |
| Clock net | `DataManager::getClockNetList()`、`IdbNet` |
| Clock sink | `IdbPin` / instance pin |
| Clock buffer | 新增或修改的 `IdbInstance` |
| Clock tree report | `cts_report`、`cts_save_tree` |

### iRT: Routing

位置：`src/operation/iRT`

入口：

- Tcl: `init_rt`、`run_rt`、`destroy_rt`
- sky130 示例中常见：`run_rt -flow "dr"`

典型过程：

```text
读取 placement/CTS/TO 后的 DEF
  -> 初始化 routing resource
  -> global routing / detailed routing
  -> 生成 wire/via
  -> 回写 IdbNet / IdbRegularWire / IdbVias
  -> def_save
```

注意：`ToolManager::autoRunRouter()` 当前代码里打印禁用提示；脚本 flow 中应以 `tcl_irt` 的命令入口为准。

EDA 抽象：

| 概念 | iEDA 对应 |
| --- | --- |
| Routing layer | `IdbLayers` |
| Track/grid | `IdbTrackGridList`、`IdbGCellGridList` |
| Wire segment | `IdbRegularWire`、`IdbRegularWireSegment` |
| Via | `IdbVia`、`IdbVias` |
| DRC-aware routing | iRT 与 DRC API/规则数据交互 |

### iSTA: Static Timing Analysis

位置：`src/operation/iSTA`

入口：

- Tcl: `init_sta`、`run_sta`、`report_sta`
- 平台转发：`ToolManager::autoRunSTA()`、`initSTA()`、`runSTA()`

输入：

- Netlist/DEF。
- Liberty `.lib`。
- SDC。
- 可选 SPEF/SDF。

EDA 抽象：

| 概念 | iEDA 对应 |
| --- | --- |
| Timing graph | iSTA 内部 timing engine |
| Liberty cell arc | iSTA liberty parser/model |
| Constraint | SDC reader |
| Parasitic | SPEF/SDF reader |
| Timing report | `run_sta`、`report_sta` |

### iTO / iNO: 优化

位置：

- `src/operation/iTO`
- `src/operation/iNO`

入口：

| 工具 | Tcl 命令 | 平台转发 |
| --- | --- | --- |
| iTO | `run_to`、`run_to_drv`、`run_to_hold`、`run_to_setup` | `ToolManager::autoRunTO/RunTODrv/RunTOHold/RunTOSetup` |
| iNO | `run_no_fix_fanout` 或相关 NO 命令 | `ToolManager::RunNOFixFanout` |

EDA 抽象：

| 概念 | iEDA 对应 |
| --- | --- |
| DRV fix | iTO 插 buffer、调整 net/instance |
| Hold/setup optimization | iTO 调用 timing 信息并修改设计 |
| Fanout fix | iNO 插 buffer 或拆分负载 |
| ECO-style update | 修改 `IdbInstance`、`IdbNet` 后保存 DEF/Verilog |

### iDRC / iPA / iIR / iPNP

位置：

- `src/operation/iDRC`
- `src/operation/iPA`
- `src/operation/iIR`
- `src/operation/iPNP`

职责：

| 工具 | 作用 |
| --- | --- |
| iDRC | 检查 routing/design rule violation，并可输出 detail DRC。 |
| iPA/iPW | 功耗分析。 |
| iIR | IR drop 分析。 |
| iPNP | Power network planning，生成或优化电源网络。 |

## EDA 抽象与 iEDA 类的对应关系

| 后端阶段 | 输入 | 修改对象 | 输出 |
| --- | --- | --- | --- |
| Floorplan | LEF、初始 DEF/Verilog、FP config | `IdbDie`、`IdbCore`、`IdbRows`、`IdbInstance`、`IdbBlockage` | `iFP_result.def` |
| Placement | floorplan DEF、PL config | `IdbInstance` 坐标/status、filler | `iPL_result.def`、`iPL_lg_result.def` |
| CTS | placement DEF、CTS config、Lib/SDC | clock buffer instance、clock net | `iCTS_result.def`、clock report |
| Routing | CTS/TO 后 DEF、RT config | net wires、vias、routing shapes | `iRT_result.def` |
| STA | DEF/Verilog、Lib、SDC、SPEF | timing graph/report，不一定修改 iDB | timing report |
| TO/NO | timed design、optimization config | instance/net/buffer | `iTO_*_result.def`、netlist |
| DRC | routed DEF、tech/rule data | violation report | `detail.drc`、DRC report |
| Power/IR/PNP | routed design、power config | power report 或 PDN shapes | report/DEF |

## 阅读建议

不要从 `src/operation` 全量阅读。每次只选一个阶段：

1. 先看该工具 README。
2. 找 Tcl 注册文件。
3. 找 `CmdXXX::exec()`。
4. 找 `ToolManager` 转发或工具 API。
5. 看它读取了哪些 iDB 对象，最后回写了哪些 iDB 对象。
