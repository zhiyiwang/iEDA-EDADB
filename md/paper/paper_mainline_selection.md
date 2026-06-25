# EDADB + iEDA 论文主线收敛

本文把已有候选研究点收敛成优先级明确的论文路线。目标是避免同时推进过多方向，导致工程发散。

## 1. 推荐主线

### 主线 A：EDA-first，Persistent Incremental Views for Physical Design ECO

建议优先做这条。

题目雏形：

```text
EDADB: Persistent Incremental Views for Open-source Physical Design ECO
```

核心闭环：

```text
iNO fanout ECO
  -> NetlistECOView
  -> PlacementView
  -> iPL incremental legalization / HPWL / bin density
  -> GeometryView
  -> iDRC dirty-region validation
```

为什么优先：

- iNO 能产生真实 ECO，不用伪造 dirty set。
- iPL 已有 incremental legalization 接口，可直接验证 dirty inst handoff。
- iDRC 的 spatial dirty region 最适合体现 database/provenance skipping。
- 三个工具组合能证明 EDADB 不是 DEF cache，而是跨阶段增量 view database。
- 正确性可用 full iEDA run 做 oracle，论文说服力比纯性能缓存更强。

最小贡献：

- 一套 EDA object/spatial/graph dirty-set model。
- 三个 persistent views：`NetlistECOView`、`PlacementView`、`GeometryView`。
- full vs incremental correctness validator。
- runtime / rebuild cost / skip ratio / debug trace 对比。

目标会议：

- DAC / ICCAD。
- 最终投稿前仍需人工核验最新版 CCF 分类和 track。

## 2. 数据库增强线

### 主线 B：DB-first，EDA-specific Incremental View Maintenance

这条作为第二主线或 A 的系统章节，不建议一开始单独做。

题目雏形：

```text
Incremental View Maintenance for Hybrid Object-Spatial-Graph EDA Workloads
```

适合何时切换：

- A 的 iNO/iPL/iDRC 闭环已经跑通。
- EDADB 已有 stable dirty traversal、domain query、view validator。
- 能抽象出通用 update model，而不是只写 iEDA 适配代码。

需要补强：

- Query/update algebra：object graph、bbox tile、netlist graph、metric view。
- Baseline：SQLite naive query、full rebuild、manual in-memory recompute。
- Benchmark：真实 iEDA flow + synthetic EDA-like workload。
- 指标：maintenance latency、query latency、storage overhead、false positive、skip ratio。

风险：

- 如果只有 SQLite table + C++ adapter，数据库系统贡献不够。
- 如果只有一个 iEDA flow case，泛化性不足。

## 3. AI 交叉线

### 主线 C：AI extension，Provenance-aware Design Memory

这条适合作为 A/B 的扩展实验，不建议一开始当唯一主线。

题目雏形：

```text
Provenance-aware Design Memory for ML and Agentic EDA Optimization
```

可做内容：

- FlowMemory 保存 stage snapshot、object delta、QoR、failure。
- Agent 通过 EDADB query API 定位失败 stage/object/config。
- ML 任务使用 table/grid/graph 多视图预测 ECO impact、DRC risk、timing/power risk。

必须坚持：

- AI 输出必须由 exact iEDA tool validator 验证。
- 不能只做 log parser 或普通 predictor。
- 必须证明 EDADB memory 带来更好的可解释性、复现性或工具调用效率。

风险：

- 单独做容易变成 agent demo。
- 数据规模不够时，ML 结果很难支撑强论文。

## 4. 暂缓方向

| 方向 | 暂缓原因 | 何时恢复 |
| --- | --- | --- |
| PowerIRView / PDNView | 跨 iPA/iIR/iPNP/iPDN，工程面宽，验证链长。 | A 跑通后，用作第二个复杂 case。 |
| ClockTreeView / TimingView | 数据结构复杂，涉及 STA/CTS/TO 多工具语义。 | 有 stable run/stage/version schema 后恢复。 |
| RoutingView | 价值高，但当前 `ToolManager::autoRunRouter()` 有禁用状态，需先确认可运行路径。 | iRT 独立 run 和 route view validator 可稳定运行后恢复。 |
| Schema synthesis | 有研究味，但容易变成工程工具，和性能主线距离远。 | DEF/EDADB 适配继续扩展后，用作 tooling paper 或 appendix。 |
| Full data lake | 资料方向已有 CircuitNet/EDA-Schema/R2G 线索，单做数据集风险高。 | EDADB 已有 provenance + exact validator 后再做。 |

## 5. 三个月执行切片

### Month 1：M0 + M1

目标：

- 实现 run/stage metadata。
- 实现 iNO NetlistECOView。
- 写 dirty-set validator。

验收：

- 能查询一次 iNO run 的 inserted buffer、created net、moved pins。
- EDADB dirty set 与 iDB diff 一致。

### Month 2：M2

目标：

- 实现 PlacementView full build。
- 实现 HPWL/bin density validator。
- 接 iNO dirty inst 到 iPL incremental legalization。

验收：

- EDADB HPWL/bin density 与 full eval 一致。
- iNO 后 incremental legalization 可运行并记录 affected inst/net/bin。

### Month 3：M3 + paper skeleton

目标：

- 实现 GeometryView full build。
- 实现 dirty region -> affected DRC check 的第一版。
- 写论文 skeleton 和实验表格。

验收：

- full DRC 与 dirty-region DRC 的 violation set 可对比。
- 有三类指标：correctness、runtime、debug/provenance query。

## 6. 实验必须产出的图表

| 图表 | 说明 |
| --- | --- |
| Architecture | iDB + EDADB + Tool Views + FlowMemory 的系统结构。 |
| Data Model | object graph / spatial tile / netlist graph / stage version 的统一模型。 |
| Runtime | full rebuild vs EDADB incremental view maintenance。 |
| Correctness | full tool oracle vs EDADB query/incremental result。 |
| Skip Ratio | DRC/geometry dirty region 跳过比例。 |
| Debug Query | 从 failure/QoR drift 回溯到 stage/object/config 的 query 示例。 |
| Ablation | no provenance / no spatial index / full snapshot / delta view 对比。 |

## 7. 停止或降级条件

主线 A 应降级，如果出现：

- iNO 无法稳定产生可控 ECO dirty set。
- iPL incremental legalization 无法接收或受益于 dirty inst。
- iDRC dirty-region check 无法与 full DRC 对齐。
- EDADB view maintenance 开销大到超过 full rebuild，且没有 debug/provenance 收益。

此时备选：

- 转向 FlowMemory + stage provenance，先做工程系统论文。
- 或转向 DB-first synthetic workload，用 iEDA 作为 case study。

## 8. 当前结论

先做：

```text
M0 run/stage metadata
M1 NetlistECOView
M2 PlacementView
M3 GeometryView
```

暂不把 PowerIR、CTS/Timing、Routing、AI agent 作为第一篇论文主线。它们保留为后续扩展 case。
