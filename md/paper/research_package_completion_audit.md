# EDADB + iEDA 研究整理交付审计

本文检查当前 `docs/paper` 是否已经满足“阅读 EDADB/iEDA 代码、查找资料、整理工程和研究思路”的目标。它只证明研究整理已完成，不证明任何 EDADB view 已经实现或实验跑通。

## 1. 目标拆解

原始目标拆成三项：

1. 查找 EDA/database 相关研究，提出可面向严肃会议的研究创新和工程工作。
2. 深入 iEDA 各点工具，说明除了 DEF read/write，EDADB 如何提高 EDA 性能。
3. 从 EDADB core 出发，规划其如何完善性能和功能，支撑 1 和 2。

## 2. 当前交付物

### 2.1 总览和主线

- `README.md`：阅读顺序和当前最小闭环。
- `edadb_eda_research_roadmap.md`：总路线、外部资料、本地代码事实和研究点。
- `research_point_catalog.md`：EDA / Database / AI 三类研究点清单。
- `paper_mainline_selection.md`：第一篇论文主线选择和三个月切片。
- `research_idea_evaluation_and_action_plan.md`：所有研究点的评审式排序、为什么好/不好、工程切入和 go/no-go 条件。
- `research_execution_plan.md`：工程里程碑、实验矩阵和停止条件。
- `goal_coverage_audit.md`：原始目标覆盖矩阵。

### 2.2 文献和会议

- `literature_and_venue_notes.md`：外部资料、痛点、创新问题和会议目标风险。

已覆盖资料类型：

- EDA database / open flow：OpenDB、OpenROAD、iEDA。
- ML for EDA / dataset / schema：ML for EDA survey、CircuitNet、EDA-Schema-V2、R2G、DALI-PD。
- Power / IR / PDN prediction：PowerNet、PDNNet。
- Database / IVM / provenance：DBSP、provenance-based data skipping、provenance sketch maintenance、cost-based sketch selection。

会议处理方式：

- 只讨论 `DAC / ICCAD / SIGMOD / VLDB / ICDE` 这类严肃目标。
- 文档不声称已完成最新版 CCF 分类核验。
- 最终投稿前仍需人工核对 CCF 最新目录和具体 track。

### 2.3 iEDA 工具细化

- `ieda_tool_edadb_opportunity_matrix.md`：工具机会总表。
- `edadb_ifp_floorplanview_plan.md`：iFP FloorplanView。
- `edadb_ino_netlist_eco_plan.md`：iNO NetlistECOView。
- `edadb_ipl_incremental_placement_plan.md`：iPL PlacementView。
- `edadb_idrc_incremental_drc_plan.md`：iDRC GeometryView。
- `edadb_irt_routing_eco_plan.md`：iRT RoutingView。
- `edadb_timing_eco_plan.md`：iSTA/iTO TimingView。
- `edadb_icts_clock_tree_plan.md`：iCTS ClockTreeView。
- `edadb_ipdn_pdnview_plan.md`：iPDN PDNView。
- `edadb_power_ir_pdn_plan.md`：iPNP/iPA/iIR PowerIRView。
- `edadb_flow_dse_memory_plan.md`：FlowMemory / DSE Memory。

这些文档已经覆盖：

- 入口代码。
- 当前数据流。
- EDADB view/schema 建议。
- 可验证对象。
- 实验设计。
- 风险和下一步。

### 2.4 EDADB 自身路线

- `edadb_core_research_notes.md`：EDADB core 代码审计。
- `edadb_system_improvement_plan.md`：query/index/delta/view/storage/schema 系统路线。

已明确的 EDADB 短板：

- 当前更接近 SQLite-backed C++ object graph ORM。
- root query 能力不足以支持 bbox/net/tile/stage 查询。
- update/upsert 仍偏整图替换，不适合高性能 ECO。
- 缺 stage/version/provenance。
- 缺 domain query、spatial/graph index、partial update、view validator 和 workload benchmark。

## 3. 目标覆盖结论

| 原始目标 | 交付状态 | 证据 |
| --- | --- | --- |
| 查找研究资料并提出研究点 | 已完成研究整理 | `literature_and_venue_notes.md`、`edadb_eda_research_roadmap.md`、`research_point_catalog.md` |
| 面向严肃会议，不找野鸡会议 | 已完成目标约束 | `literature_and_venue_notes.md` 只保留 DAC/ICCAD/SIGMOD/VLDB/ICDE，并保留 CCF 人工核验提醒 |
| 深入 iEDA 工具，说明 EDADB 除 DEF 外如何提升性能 | 已完成方案整理 | `ieda_tool_edadb_opportunity_matrix.md` 和 10 个工具细化计划 |
| 规划 EDADB 自身性能/功能完善 | 已完成路线整理 | `edadb_core_research_notes.md`、`edadb_system_improvement_plan.md` |
| 评价点子好坏并说明怎么做 | 已完成评审式整理 | `research_idea_evaluation_and_action_plan.md` |
| 收敛后续工程主线 | 已完成第一版 | `paper_mainline_selection.md`、`research_idea_evaluation_and_action_plan.md`、`research_execution_plan.md` |

## 4. 明确没有完成的事

以下不是本次“研究整理”目标的完成条件，且当前确实尚未完成：

- 未实现 `NetlistECOView / PlacementView / GeometryView`。
- 未运行 full vs incremental correctness 实验。
- 未测量 EDADB 的真实性能收益。
- 未完成最新版 CCF 目录人工核验。
- 未形成最终论文正文和图表。
- 当前 `docs/paper/*.md` 仍未提交到 git。

## 5. 下一步工程建议

第一主线不再发散，按以下顺序执行：

```text
M0 run/stage metadata
M1 iNO NetlistECOView
M2 iPL PlacementView
M3 iDRC GeometryView
```

最小可发表闭环：

```text
iNO fanout ECO
  -> EDADB dirty inst/net/pin
  -> iPL incremental legalization + HPWL/bin-density validator
  -> iDRC dirty-region validation
  -> full iEDA run oracle
```

如果这条线无法证明性能或调试收益，再降级到：

- FlowMemory + stage provenance 工程系统；
- 或 DB-first synthetic EDA workload + iEDA case study。

## 6. 当前交付状态

结论：

- 作为研究和工程思路整理，当前 `docs/paper` 已经达到可查阅、可总结、可进入下一阶段实现的状态。
- 作为论文或系统实现，当前仍只是路线图和设计文档，不能声称已有实验结果。
