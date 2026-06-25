# EDADB + iEDA 研究执行计划

本文把已有研究点收敛成可执行工程路线。目标不是一次做完所有工具，而是先建立能支撑论文雏形的最小闭环，再扩展到数据库系统和 AI 交叉方向。

## 1. 目标拆解

原始目标可拆成三件事：

1. 找 EDA/database/AI 相关研究，提出能面向严肃会议的研究点。
2. 深入 iEDA 点工具，说明除了 DEF read/write，EDADB 如何提高性能。
3. 规划 EDADB 自身如何增强性能和功能，支撑前两点。

当前文档覆盖：

- 资料与会议：`literature_and_venue_notes.md`
- 总路线：`edadb_eda_research_roadmap.md`
- 主线选择：`paper_mainline_selection.md`
- 工具机会：`ieda_tool_edadb_opportunity_matrix.md`
- EDADB core：`edadb_core_research_notes.md`
- 系统补强：`edadb_system_improvement_plan.md`
- iNO/M1 细化：`edadb_ino_netlist_eco_plan.md`
- 各工具细化计划：iFP / iNO / iPL / iDRC / iRT / iSTA+iTO / iCTS / iPDN / iPNP+iPA+iIR / Flow+DSE

## 2. 推荐论文路线

主线选择结论见 `paper_mainline_selection.md`。当前推荐先做 EDA-first 路线 A，即 `iNO -> iPL -> iDRC` 的 persistent incremental view 闭环；数据库和 AI 方向先作为系统章节与扩展实验。

### 路线 A：EDA 会议主线

题目雏形：

```text
EDADB: Persistent Incremental Views for Open-source Physical Design ECO
```

核心问题：

- 物理设计 ECO 后，placement/DRC/routing/timing 等工具经常重复构建局部可复用状态。
- EDADB 能否把这些状态变成可持久化、可查询、可增量维护的 view？

最小贡献：

- `NetlistECOView`：iNO fanout fix 产生真实 dirty inst/net/pin。
- `PlacementView`：iPL 对 dirty inst 维护 HPWL/bin density，并对接 incremental legalization。
- `GeometryView`：iDRC 对 dirty region 做 affected shape/violation check。
- full iEDA run 作为 correctness oracle。

目标会议：

- DAC / ICCAD。最终 CCF 分类需人工核验。

### 路线 B：Database 会议主线

题目雏形：

```text
Incremental View Maintenance for Hybrid Object-Spatial-Graph EDA Workloads
```

核心问题：

- EDA workload 同时包含 C++ object graph、spatial geometry、netlist graph、matrix/RC view 和局部更新。
- 通用数据库 IVM / provenance skipping 方法如何适配这种混合 workload？

最小贡献：

- 定义 EDA update model：move/insert/split/reroute/resize。
- 定义 view model：object table + spatial tile + graph adjacency + provenance unit。
- EDADB 实现 dirty traversal、domain query、view validator。
- 在 iEDA 真实 flow + synthetic EDA-like workload 上评估。

目标会议：

- SIGMOD / VLDB / ICDE。需要比 EDA 会议版本更强调抽象、系统设计和 benchmark。

### 路线 C：AI 交叉主线

题目雏形：

```text
Provenance-aware Design Memory for ML and Agentic EDA Optimization
```

核心问题：

- ML/agent for EDA 需要跨阶段、可解释、可校验的数据记忆。
- 只读 log/report/DEF 难以回答对象级原因。

最小贡献：

- EDADB 保存 stage snapshot、object delta、QoR、tool config、failure/provenance。
- 导出 table/grid/graph 多视图，并保证跨视图一致。
- agent 或 ML predictor 的输出必须被 exact tool validator 校验。
- FlowMemory / DSE 细化计划见 `edadb_flow_dse_memory_plan.md`。

目标会议：

- DAC / ICCAD AI for Design，或作为路线 A/B 的扩展章节。

## 3. 第一阶段最小闭环

推荐先做：

```text
iNO fanout ECO
  -> EDADB NetlistECOView
  -> EDADB PlacementView
  -> iPL incremental legalization / metric validation
  -> EDADB GeometryView
  -> iDRC dirty region validation
```

为什么选它：

- iNO 会真实修改 netlist：插 buffer、建新 net、重连 pins。
- iPL 已有 changed inst incremental legalization 接口。
- iDRC 的 spatial check 最适合做 dirty region / provenance skipping。
- 三者组合能体现跨阶段增量，而不是单工具缓存。

## 4. 工程里程碑

### M0：实验底座

产物：

- `run_id / stage_id / git_commit / edadb_commit / design / config` 元数据表。
- 每次实验保存 input/output DEF、report、runtime、memory。
- 所有脚本记录命令和环境。

验收：

- 同一设计重复跑能复现相同 QoR/report。
- EDADB 中能查询每次 run 的工具版本和输入输出文件。

### M1：NetlistECOView

产物：

- 表：`EcoAction`、`EcoDirtyInst`、`EcoDirtyNet`、`EcoDirtyPin`。
- 接入 iNO `FixFanout::fixFanout()`。
- 记录 inserted buffer、new net、moved load pins。

验收：

- EDADB dirty set 与 iDB 中新增 instance/net/pin 改动一致。
- STA net fanout violation 修复前后可查询。

### M2：PlacementView

产物：

- 表：`PlInst`、`PlNet`、`PlNetPin`、`PlBin`、`PlMetric`。
- full build：从 iDB/iPL wrapper 抽取 placement view。
- delta update：dirty inst -> affected nets/bins。

验收：

- full vs EDADB query 的 HPWL 一致。
- full vs EDADB query 的 bin density 一致。
- iNO dirty inst 能喂给 iPL incremental legalization。

### M3：GeometryView

产物：

- 表：`GeoShape`、`GeoTile`、`GeoTileShape`、`DrcViolation`、`DrcDirtyRegion`。
- full build：从 iDB/iDRC 抽取 geometry/tile view。
- delta update：dirty inst/net/pin -> dirty bbox/tile。

验收：

- full DRC violation set 与 dirty region recheck 一致。
- dirty tile candidate 数明显小于 full tile/shape scan。

### M4：系统能力补强

产物：

- EDADB domain query wrapper：by net、by bbox、by tile、by stage。
- batch transaction / prepared op 复用。
- partial child update 原型，避免全图 delete+insert。

验收：

- microbenchmark 展示 query/update/view refresh 收益。
- 对比 baseline：iDB full traversal、SQLite naive scan、EDADB domain view。

### M5：论文实验

产物：

- 多 design、多 dirty ratio、多 ECO 类型实验。
- full run vs incremental view correctness。
- runtime、memory、DB size、write amplification、skip ratio。

验收：

- 至少覆盖 iNO+iPL+iDRC 三工具闭环。
- 至少一个公开 benchmark 或可复现实验集，不只 sky130_gcd。

## 5. 实验矩阵

| 实验 | Workload | Baseline | EDADB variant | 指标 |
| --- | --- | --- | --- | --- |
| E1 | iNO fix fanout | no EDADB action log | NetlistECOView | dirty inst/net/pin correctness、action count。 |
| E2 | dirty inst placement metric | full iPL metric recompute | PlacementView delta | HPWL/density correctness、runtime。 |
| E3 | iPL incremental legalization | full legalization | dirty inst legalization | legality、HPWL、runtime。 |
| E4 | dirty geometry extraction | full iDB geometry scan | GeometryView by bbox/tile | candidate shape count、query latency。 |
| E5 | dirty DRC check | full iDRC check | dirty region recheck | violation set equality、runtime、false negative。 |
| E6 | EDADB update | full graph update | partial child update | write amplification、update latency。 |
| E7 | provenance skipping | no skipping | tile/net/stage sketch | skip ratio、maintenance cost、false positive。 |

## 6. 数据与 benchmark

起步：

- `sky130_gcd`：已有 iEDA flow，适合打通端到端脚本。

扩展：

- 其他 iEDA demo designs。
- OpenROAD/OpenLane 公开设计，若格式和 PDK 可转接。
- 自建 parameter sweep：utilization、clock、aspect ratio、placement perturbation、ECO size。

注意：

- 数据库会议版本不能只用一个小设计。
- AI 版本必须保留 train/valid/test split 和跨设计泛化。

## 7. EDADB 必须补的能力

短期：

- domain query：bbox/net/tile/stage。
- dirty set：object/net/tile/stage。
- run metadata：run_id/stage_id/config/hash。
- validator SQL/API：row count、field、FK、view correctness。

中期：

- partial update。
- memory spatial index。
- adjacency cache。
- provenance unit and sketch。
- Arrow/Parquet export。

长期：

- cost-based query/index/view selection。
- multi-backend correctness + analytics。
- concurrent read / staged write。
- schema synthesis and static lint。

## 8. 停止条件

某条路线应暂停或降级，如果出现：

- 只能证明 DEF roundtrip，不能证明工具性能或调试收益。
- 增量结果没有 full-run validator。
- 只在一个小设计上有效。
- EDADB overhead 大于 full recompute 且无清晰优化路径。
- 无法抽象成通用 EDA object/spatial/graph workload。

## 9. 下一步建议

下一步优先做 M0 + M1：

1. 给 EDADB 增加 run/stage metadata 表。
2. 在 iNO fix fanout 处插入 action/dirty-set 记录。
3. 写 validator：比较 EDADB dirty set 与 iDB diff。
4. 用一个小设计跑通 action log。

这一步不需要先做复杂索引，但能把“EDADB 作为跨阶段 memory”的核心故事立起来。
