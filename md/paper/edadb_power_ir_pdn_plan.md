# EDADB + iPNP/iPA/iIR PowerIRView / PDN 优化实验计划

本文是 `EDADB + iEDA` 研究路线中 `EDA-5 / DB-5 / AI-5` 的细化计划。目标是把 power analysis、IR drop analysis、PDN synthesis/optimization 的关键中间结果做成可持久化、可查询、可增量验证的 EDADB view，用于 PDN 模板优化、IR hotspot 定位、ECO 影响分析和 AI 预测任务。iPDN 命令式建网的细化计划单独记录在 `edadb_ipdn_pdnview_plan.md`。

## 1. 研究问题

核心问题：

```text
Can a persistent PowerIRView reduce repeated power/IR/PDN analysis cost and expose useful provenance for PDN optimization and AI-based IR prediction?
```

要避免的弱点：

- 不能只保存 power report 或 IR report。
- 不能只做 DEF/GDS 几何存储。
- 不能只训练一个 IR predictor，而不解释数据来源和正确性。

有价值的方向：

- 把 power instance、RC node、IR voltage、PDN template、congestion score 连接成统一 view。
- 在 PDN 模板变更、placement 局部变化、VCD/toggle 局部变化后，只重算 affected region。
- 用 full iPA/iIR/iPNP 结果验证增量 view 或 AI 预测的正确性边界。

## 2. 代码证据

### 2.1 iPNP

入口：

- `src/operation/iPNP/api/ipnp_api.cpp`
  - `PNPApi::run_pnp()` 调用 `PNP::init()`、`PNP::runSynthesis()`、`PNP::saveToIdb()`。
  - `PNPApi::connect_M2_M1()` 调用 `PNP::connect_M2_M1()`。
- `src/operation/iPNP/source/PNP.cpp`
  - `PNP::init()` 从 iDB 读取 die 宽高和 power layer 配置。
  - `PNP::runSynthesis()` 通过 `NetworkSynthesis` 生成 power network。
  - `PNP::runOptimize()` 调用 `PdnOptimizer::optimizeGlobal()`。
  - `PNP::runAnalysis()` 调用 congestion eval 和 IR eval，并输出 max/min/avg IR drop、overflow。

优化和评估：

- `src/operation/iPNP/source/module/optimizer/PdnOptimizer.cpp`
  - 使用 `SimulatedAnnealing` 做全局优化。
- `src/operation/iPNP/source/module/optimizer/SimulatedAnnealing.cpp`
  - `evaluateCost()` 同时使用 IR drop 和 overflow。
- `src/operation/iPNP/source/module/evaluator/IREval.cpp`
  - 通过 iPA/PowerEngine 获取 IR drop map。
- `src/operation/iPNP/source/module/evaluator/CongestionEval.cpp`
  - 调用 EGR 相关流程评估 congestion overflow。

### 2.2 iPA

入口：

- `src/platform/tool_manager/tool_api/ipw_io/ipw_io.cpp`
  - `PowerIO::autoRunPower()` 初始化 STA 并调用 `reportSummaryPower()`。
  - `PowerIO::reportSummaryPower()` 创建 `ipower::Power` 并调用 `runCompleteFlow()`。
- `src/operation/iPA/api/Power.cc`
  - `Power::buildGraph()` 从 STA graph 构建 power graph。
  - `Power::readRustVCD()` 读取 VCD 并生成 toggle/SP 数据。
  - `Power::calcLeakagePower()`、`calcInternalPower()`、`calcSwitchPower()` 分别计算 leakage/internal/switching power。
  - `Power::analyzeGroupPower()` 聚合 power group。
  - `Power::getInstancePowerData()` 为 iIR 提供 instance power data。
  - `Power::reportIRDropCSV()` / `reportIRDropTable()` 输出 IR drop 结果。

### 2.3 iIR

入口：

- `src/operation/iIR/api/iIR.cc`
  - `iIR::readSpef()` 读取 RC 数据。
  - `iIR::readInstancePowerDB()` 或 `setInstancePowerData()` 读取/设置 instance power。
  - `iIR::solveIRDrop()` 对指定 power net 构建 conductance matrix 和 current vector，并用 LU/CG solver 求 grid voltage。

关键数据流：

```text
SPEF/RC data + instance power
  -> conductance matrix G
  -> current vector J
  -> solver
  -> grid voltages / instance IR drop
```

## 3. 建议 EDADB View

### 3.1 PowerView

建议表：

- `PowerRun(run_id, design, stage, git_commit, config_hash, vcd_hash, spef_hash)`
- `PowerInst(run_id, inst_name, group_type, leakage, internal, switching, total, nominal_voltage)`
- `PowerNet(run_id, net_name, switching_power, toggle, cap, driver_inst)`
- `PowerGroupMetric(run_id, group_type, leakage, internal, switching, total, percent)`
- `PowerToggle(run_id, object_name, object_type, toggle_rate, static_probability)`

作用：

- 记录 iPA 的 instance/net/group 级 power 结果。
- 为 iIR current vector、AI IR prediction、ECO impact prediction 提供输入。

### 3.2 IRView

建议表：

- `IRRun(run_id, power_run_id, net_name, solver, nominal_voltage, rc_version)`
- `IRNode(run_id, net_name, node_id, x, y, layer, voltage, ir_drop)`
- `IREdge(run_id, net_name, from_node, to_node, resistance)`
- `IRInstCurrent(run_id, inst_name, net_name, node_id, current)`
- `IRHotspot(run_id, hotspot_id, bbox, max_drop, avg_drop, inst_count)`

作用：

- 记录 iIR 的 matrix/graph/result。
- 支持 hotspot query、region-level delta update、solver result validation。

### 3.3 PDNView

建议表：

- `PNPRun(run_id, config_hash, region_grid, power_layers)`
- `PNPRegion(run_id, region_id, bbox, template_id)`
- `PNPTemplate(run_id, template_id, layer, width, spacing, pitch, offset)`
- `PNPShape(run_id, shape_id, net_name, layer, rect, via_name, region_id, template_id)`
- `PNPScore(run_id, iter_id, ir_score, overflow_score, drc_score, total_score)`
- `PNPAction(run_id, iter_id, action_type, region_id, old_template, new_template, accepted)`

作用：

- 记录 iPNP synthesis/optimization 的模板选择、生成 geometry 和评估分数。
- 支持 simulated annealing 过程复盘、局部模板替换、optimization trace 学习。

## 4. 增量更新设想

### 4.1 Power delta

变更来源：

- VCD/toggle 局部变化。
- instance resize / buffer insertion / placement move。
- timing graph 或 load cap 变化。

增量逻辑：

- 标记 changed instance/pin/net。
- 更新 affected net switching power 和 instance power。
- 更新 group power summary。
- 只把 affected instance power 传给 IRView。

### 4.2 IR delta

变更来源：

- instance current 变化。
- PDN shape/via 变化。
- SPEF/RC 局部变化。

增量逻辑：

- 标记 affected IR nodes/edges。
- 对小范围变化做 localized solve 或 reuse previous solution as initial vector。
- full solve 作为 correctness oracle。

### 4.3 PDN delta

变更来源：

- 一个 region 的 PDN template 替换。
- power via 增删。
- placement/congestion hotspot 变化。

增量逻辑：

- 标记 affected region。
- 重新生成该 region 的 PDN shapes。
- 更新 affected RC graph、IR hotspot、congestion overflow。
- 保留未受影响 region 的 score 和 geometry。

## 5. Correctness 验证

必须先证明 view 没有改变工具语义：

- `PowerView`：EDADB 导出的 instance/net/group power 与 iPA report 一致。
- `IRView`：EDADB 导出的 max/min/avg IR drop 与 iIR/iPA report 一致。
- `PDNView`：EDADB 导出的 PDN shapes 与 `saveToIdb()` 后 iDB/DEF 中的 PG geometry 一致。
- 增量模式：delta update 后结果与 full iPA/iIR/iPNP rerun 一致或在数值容差内。

建议容差：

- power 数值：相对误差和绝对误差同时约束。
- IR voltage/drop：按 solver 精度设置容差。
- geometry：必须 exact match。
- hotspot：要求 top-k hotspot recall，先以 full result 为 oracle。

## 6. 实验 workloads

基础 workload：

- 单 region 替换 PDN template。
- 添加/删除 power via。
- 移动一组高功耗 instances。
- 局部修改 VCD/toggle。
- 修改 power layer/template 参数。
- 对比 congestion-aware 和 IR-aware 权重。

复杂 workload：

- Simulated annealing 多轮 trace replay。
- placement ECO 后重跑 power/IR/PNP。
- routing congestion 变化后重评估 PDN 模板。
- 多 design、多 utilization、多 power layer 配置。

## 7. 论文潜力

EDA 方向：

- Persistent PowerIRView for fast PDN optimization and IR hotspot debugging。
- 目标会议：DAC / ICCAD 的 physical design、power integrity、AI for design track。

Database 方向：

- Incremental maintenance of matrix/spatial/graph views under EDA-local updates。
- 目标会议：SIGMOD / VLDB / ICDE，但需要把 EDADB 做成真正系统贡献，而不是单一应用。

AI 交叉方向：

- DB-backed, provenance-aware IR/PDN prediction dataset and closed-loop optimizer。
- baseline 可参考 PowerNet、PDNNet、CircuitNet、EDA-Schema-V2、R2G 等方向。

## 8. 推荐实施顺序

1. `PowerView`：先只保存 iPA summary 和 instance power，验证 report 一致。
2. `IRView`：保存 instance IR drop 和 hotspot，验证 report/CSV 一致。
3. `PDNView`：保存 iPNP region/template/shape/score，验证 DEF PG geometry 一致。
4. `PowerIR provenance`：建立 inst/net/node/region 的依赖关系。
5. `delta workload`：从单 region template change 开始做 full vs incremental 对比。
6. `AI dataset`：导出 graph/grid/table 三种视图，先做 IR hotspot prediction。
