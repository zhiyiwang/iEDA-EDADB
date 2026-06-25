# 目标覆盖审计

本文按原始目标逐项检查当前 `docs/paper` 文档是否已经覆盖、证据在哪里、哪些内容仍停留在计划阶段。它用于后续总结和查漏补缺，不替代具体实验。

## 1. 原始目标

目标 1：

```text
查找 EDA 和 database 相关领域的研究，思考利用现有项目做什么研究创新和工程工作。
要求面向 CCF A 类目标会议，不找野鸡会议。
```

目标 2：

```text
深入研究 iEDA 中各项工具，说明除了 DEF read/write，如何利用 EDADB 提高 EDA 性能。
```

目标 3：

```text
规划 EDADB 自身如何完善性能和功能，帮助实现目标 1 和目标 2。
```

## 2. 覆盖矩阵

| 需求 | 当前覆盖状态 | 证据文档 | 已完成内容 | 仍需补强 |
| --- | --- | --- | --- | --- |
| 外部资料查找 | 已覆盖初版 | `literature_and_venue_notes.md`、`edadb_eda_research_roadmap.md` | 已整理 OpenDB/OpenROAD、iEDA paper、ML for EDA、CircuitNet、EDA-Schema-V2、R2G、PowerNet、PDNNet、DBSP、provenance skipping 等资料。 | 需要人工核验最新版 CCF 分类；需要继续查近两年更强 IR/PDN、IVM、EDA database baseline。 |
| 严肃会议目标 | 已覆盖初版 | `literature_and_venue_notes.md` | 明确 DAC/ICCAD/SIGMOD/VLDB/ICDE 只是严肃目标，不直接声称已确认 CCF A。 | 投稿前必须核验 CCF 最新目录和目标 track。 |
| EDA 研究点 | 已覆盖 | `edadb_eda_research_roadmap.md` | 已列 EDA-1 到 EDA-7，包括 DRC、placement、routing ECO、cross-stage debug、PowerIR、CTS/timing/routing 联动、Flow/DSE memory。 | 每个方向还需要实验数据验证 feasibility 和 novelty。 |
| Database 研究点 | 已覆盖 | `edadb_eda_research_roadmap.md`、`edadb_system_improvement_plan.md` | 已列 DB-1 到 DB-7，包括 EDA-specific IVM、provenance skipping、hybrid storage、schema synthesis、time-travel、index advisor、matrix/graph view。 | 数据库会版本需要更形式化的 query/update/view model 和更广 benchmark。 |
| AI 交叉研究点 | 已覆盖 | `edadb_eda_research_roadmap.md`、`literature_and_venue_notes.md` | 已列 AI-1 到 AI-7，包括 stage-aware dataset、agent debug、learning-guided query planning、ECO impact prediction、Power/IR/PDN prediction、synthetic+real co-training、agentic optimization。 | 需要避免落成普通数据集或 agent demo，必须保留 provenance 和 exact-tool validation。 |
| 研究点好坏评价 | 已覆盖 | `research_idea_evaluation_and_action_plan.md` | 已按 novelty、engineering readiness、correctness oracle、top-conference fit、risk 对主要方向排序，并说明为什么好/不好、第一阶段做什么。 | 后续需要用实验数据替换部分定性判断。 |
| iEDA 工具研究 | 已覆盖初版 | `ieda_tool_edadb_opportunity_matrix.md`、`paper_mainline_selection.md` | 已覆盖 iPL、iDRC、iRT、iSTA/iTO、iCTS、iPA/iIR、iPNP/iPDN、iFP、iNO、Flow/DSE；主要方向均已补独立细化计划，并收敛到第一主线。 | 还需要用真实实验筛选可发表贡献。 |
| 除 DEF 外提升性能 | 已覆盖方案 | `ieda_tool_edadb_opportunity_matrix.md`、`research_execution_plan.md` | 已提出 PlacementView、GeometryView、RoutingView、TimingView、ClockTreeView、PowerIRView、FlowMemory、NetlistECOView。 | 目前是设计方案，尚未实现或测量性能收益。 |
| EDADB core 能力审计 | 已覆盖 | `edadb_core_research_notes.md` | 已审计 public API、schema/table generation、object traversal、SQLite backend、测试覆盖和瓶颈。 | 还需补真实 benchmark，测量 insert/query/update/partial update 的 overhead。 |
| EDADB 系统补强 | 已覆盖 | `edadb_system_improvement_plan.md` | 已规划 query layer、index layer、delta layer、view layer、storage layer、schema layer。 | 需要从 Phase 0/1 开始实现，当前仍是路线图。 |
| 可执行研究路线 | 已覆盖 | `research_execution_plan.md` | 已收敛三条论文路线，提出 iNO -> iPL -> iDRC 最小闭环、M0-M5 里程碑和 E1-E7 实验矩阵。 | 需要实际执行 M0/M1，建立 run metadata 和 NetlistECOView。 |
| 阅读入口 | 已覆盖 | `README.md` | 已给出文档阅读顺序和当前最小闭环。 | 后续新增文档需同步更新索引。 |

## 3. 当前文档包结构

总览：

- `README.md`
- `edadb_eda_research_roadmap.md`
- `literature_and_venue_notes.md`
- `paper_mainline_selection.md`
- `research_idea_evaluation_and_action_plan.md`
- `research_execution_plan.md`
- `research_package_completion_audit.md`

代码和系统：

- `edadb_core_research_notes.md`
- `edadb_system_improvement_plan.md`
- `ieda_tool_edadb_opportunity_matrix.md`

工具细化：

- `edadb_ipl_incremental_placement_plan.md`
- `edadb_ifp_floorplanview_plan.md`
- `edadb_ino_netlist_eco_plan.md`
- `edadb_idrc_incremental_drc_plan.md`
- `edadb_irt_routing_eco_plan.md`
- `edadb_timing_eco_plan.md`
- `edadb_icts_clock_tree_plan.md`
- `edadb_ipdn_pdnview_plan.md`
- `edadb_power_ir_pdn_plan.md`
- `edadb_flow_dse_memory_plan.md`

## 4. 当前最强研究路线

最强路线仍是：

```text
iNO fanout ECO
  -> EDADB NetlistECOView
  -> iPL PlacementView
  -> iDRC GeometryView
```

原因：

- iNO 产生真实 ECO，不需要人工伪造 dirty set。
- iPL 已有 incremental legalization 接口。
- iDRC 的 spatial dirty region 最适合证明 provenance skipping。
- 三者组合能讲清楚跨阶段数据库，而不是单工具缓存。

对应论文定位：

- EDA 版本：persistent incremental views for physical design ECO。
- DB 版本：object-spatial-graph workload 的 incremental view maintenance。
- AI 版本：可解释、可验证的 design memory 和 ECO impact data。

## 5. 当前没有完成的事

这些内容尚未完成，不能在总结中写成已有结果：

- 尚未实现 NetlistECOView / PlacementView / GeometryView。
- 尚未运行 full vs incremental correctness 实验。
- 尚未测量 EDADB 相比 iDB full traversal / SQLite naive query 的性能收益。
- 尚未核验最新版 CCF 目录。
- 尚未把研究文档提交到 git。
- 尚未形成最终论文 outline 或实验图表。

## 6. 下一步优先级

P0：

- 提交当前 `docs/paper` 文档作为研究台账 baseline。
- 实现 M0：run/stage metadata 表和实验记录规范。
- 实现 M1：iNO `FixFanout` action log 和 dirty-set validator。

P1：

- 实现 PlacementView full build 和 HPWL/bin density validator。
- 实现 GeometryView full build 和 dirty region validator。
- 建立 full-run vs EDADB-view 对比脚本。

P2：

- 查更多近年数据库/EDA/AI baseline。
- 设计论文 outline 和实验图表模板。
- 按 `paper_mainline_selection.md` 执行主线 A，并用实验决定是否扩展到主线 B/C。
