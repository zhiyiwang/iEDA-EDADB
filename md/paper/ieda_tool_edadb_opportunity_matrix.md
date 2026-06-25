# iEDA 工具侧 EDADB 利用机会矩阵

本文回答一个工程问题：除了 DEF read/write，EDADB 还能插入 iEDA 哪些工具路径，从而提高性能、可复现性或调试能力。

具体里程碑和实验矩阵见 `docs/paper/research_execution_plan.md`。

## 1. 总体判断

当前最值得做的不是“把每个工具输出都 dump 到 SQLite”，而是把 EDADB 做成：

- stage snapshot：记录每阶段的对象状态和 QoR。
- domain view：为工具常用访问模式建立 view。
- dirty set：记录 ECO/优化动作影响哪些 object/net/tile。
- validator：用原始 iEDA full run 校验 EDADB view 正确性。

优先级：

1. iPL / iDRC：闭环小，最适合先证明 incremental view。
2. iRT / iSTA / iTO：能体现 graph + spatial + ECO。
3. iCTS / iPNP / iPA / iIR：能体现跨工具 timing/power/IR/PDN。
4. iFP / iPDN / iNO / Flow：更适合做 provenance、stage memory 和 action log。

## 2. 工具机会矩阵

| 工具 | 代码证据 | 当前数据流 | EDADB 机会 | 优先级 |
| --- | --- | --- | --- | --- |
| iPL | `src/platform/tool_manager/tool_api/ipl_io/ipl_io.cpp`、`src/operation/iPL/source/module/wrapper/IDBWrapper.cc` | 从 iDB wrapper 出 placement DB，支持 changed inst incremental legalization。 | PlacementView：inst/net/bin/HPWL/density dirty update。 | P0 |
| iDRC | `src/operation/iDRC/interface/DRCInterface.cpp` | 从 iDB 构建 shape/rule/layer，再按 box/rule 验证 violation。 | GeometryView：tile/bbox index、violation provenance、dirty region recheck。 | P0 |
| iRT | `src/operation/iRT/interface/RTInterface.cpp` | EGR/RT flow 生成 guide、wire、via、violation，route result 会回写 iDB。 | RoutingView：guide/segment/via/tile occupancy、local reroute cache、route diff。 | P1 |
| iSTA/iTO | `src/platform/tool_manager/tool_api/ista_io/ista_io.cpp`、`src/operation/iTO/api/ToApi.cpp` | 构建 timing graph、RC tree，优化时插 buffer/resize 并 update timing。 | TimingView：affected cone、RC graph、ECO action provenance。 | P1 |
| iCTS | `src/operation/iCTS/api/CTSAPI.cc`、`src/platform/tool_manager/tool_api/icts_io/icts_io.cpp` | 生成 clock tree、clock net、buffer/wire，并评估 timing。 | ClockTreeView：subtree、sink、buffer、skew/latency、CTS action log。 | P1 |
| iPA/iIR | `src/operation/iPA/api/Power.cc`、`src/operation/iIR/api/iIR.cc` | iPA 计算 instance/net/group power；iIR 用 RC + current 解 IR drop。 | PowerIRView：power/RC/current/voltage/hotspot provenance。 | P2 |
| iPNP/iPDN | `src/operation/iPNP/source/PNP.cpp`、`src/operation/iPDN/source/module/pdn_plan/pdn_plan.cpp` | iPNP 优化 PDN；iPDN Tcl/Python 命令创建 grid/stripe/via/special wire。 | PowerIR/PDNView：template/stripe/via/region/score/action trace。 | P2 |
| iFP | `src/operation/iFP/api/ifp_api.cpp`、`src/operation/iFP/source/module/tap_cell/tapcell.cpp` | 初始化 die/core/track，放 IO/filler/tapcell/endcap。 | FloorplanView：die/core/row/track/io/tapcell provenance，支持 stage diff。 | P3 |
| iNO | `src/operation/iNO/source/module/fix_fanout/FixFanout.cpp` | 扫 STA net fanout，插 buffer、建新 net、重连 pin。 | NetlistECOView：fanout violation、inserted buffer、net split delta。 | P2 |
| Flow/DSE | `src/interface/tcl/tcl_register.h`、`src/platform/tool_manager/tool_manager.cpp` | Tcl 驱动多阶段 flow，ToolManager 调用各点工具。 | FlowMemory：run/stage/config/QoR/failure/provenance，服务 DSE 和 agent。 | P1 |

## 3. 缺口工具细化

### 3.1 iFP / FloorplanView

细化计划见 `docs/paper/edadb_ifp_floorplanview_plan.md`。

代码事实：

- `FpApi::initDie()`、`initCore()`、`makeTracks()` 修改 die/core/track。
- `FpApi::placePort()`、`autoPlacePad()`、`placeIOFiller()` 修改 IO/pad/filler。
- `TapCellPlacer::tapCells()` 创建 tapcell/endcap instances。

EDADB 可做：

- 保存 floorplan action log：die/core/track/io/filler/tapcell。
- 对每个 action 记录输入参数、生成对象、stage version。
- 支持 stage diff：floorplan 前后 die/core/row/track/instance 差异。

研究价值：

- 单独作为论文点偏弱。
- 但对 full-flow provenance、DSE memory、agent debug 很重要。

### 3.2 iPDN / PDNView

细化计划见 `docs/paper/edadb_ipdn_pdnview_plan.md`。

代码事实：

- Tcl 注册 `create_grid`、`create_stripe`、`connect_two_layer`、`connect_macro_pdn`、`add_segment_stripe`、`add_segment_via`。
- `PdnPlan::createGrid()` 根据 core/row/blockage 创建 followpin special wire segment。
- `PdnPlan` 创建/修改 `IdbSpecialNet`、`IdbSpecialWireSegment`、IO pin port 和 via/stripe。

EDADB 可做：

- 保存 PDN command -> generated special wire/via 的 provenance。
- 保存 stripe/grid/template 参数，避免只看到最后 special net geometry。
- 支持局部 PDN 变更后重算 affected IR/congestion/DRC。

研究价值：

- 与 iPNP/iPA/iIR 结合后很强。
- 单独 iPDN 更像工程 provenance；结合 IR/PDN optimization 才有论文潜力。

### 3.3 iNO / NetlistECOView

代码事实：

- `FixFanout::fixFanout()` 从 TimingEngine 扫 STA net fanout。
- fanout 超限时创建 buffer instance、新 net，并 disconnect/connect load pins。
- 细化计划见 `docs/paper/edadb_ino_netlist_eco_plan.md`。

EDADB 可做：

- 保存 fanout violation set。
- 保存每次 net split：old net、new net、inserted buffer、moved load pins。
- 直接产出 dirty inst/net/pin 给 iPL、iRT、iSTA。

研究价值：

- 很适合作为 ECO delta generator。
- 可与 TimingView / PlacementView / RoutingView 组成跨阶段 ECO benchmark。

### 3.4 Flow / DSE Memory

细化计划见 `docs/paper/edadb_flow_dse_memory_plan.md`。

代码事实：

- `src/interface/tcl/tcl_register.h` 注册各点工具 Tcl 命令。
- `ToolManager` 把 flow 调用分发到 FP/PL/CTS/NO/TO/RT/DRC/STA/Power/PNP。
- 各 tool_io 普遍记录 stage runtime 和 memory。

EDADB 可做：

- 保存 run_id、stage_id、tool config、git commit、runtime、memory、QoR summary。
- 保存 stage object delta 和 failure reason。
- 支持查询“哪个参数/阶段导致 QoR 或 violation 变化”。

研究价值：

- 对 AI agent / DSE 很有用。
- 需要和真实多参数 sweep 结合，避免只是日志数据库。

## 4. 推荐下一步

最小工程闭环：

1. iPL `PlacementView`：dirty inst -> affected net/bin。
2. iDRC `GeometryView`：dirty region -> affected shape/violation。
3. iNO `NetlistECOView`：fanout fix -> dirty inst/net/pin。

原因：

- iPL/iDRC 能证明 EDADB 增量 view 的性能收益。
- iNO 能产生真实 ECO delta，避免只做人工修改。
- 三者组合可以形成：netlist ECO -> placement legalization -> DRC/geometry check 的小型跨阶段 flow。

后续再扩展：

- iRT local reroute。
- iSTA/iTO affected cone。
- iPDN/iPNP/iIR PowerIRView。
