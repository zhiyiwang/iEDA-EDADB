# EDADB + iEDA 研究文档阅读顺序

这个目录保存 EDADB + iEDA 的工程和研究思路。阅读时建议按“总览 -> 系统能力 -> 工具机会 -> 具体实验计划”的顺序看。

## 1. 先读总览

1. `edadb_eda_research_roadmap.md`
   - 先看外部资料、代码证据和研究点扩展清单。
   - 用来回答：我们为什么不只做 DEF read/write，哪些方向有 DAC/ICCAD/SIGMOD/VLDB/ICDE 潜力。

2. `research_point_catalog.md`
   - 看 EDA、Database、AI 交叉三类候选研究点。
   - 用来回答：现在有哪些可讨论的论文问题，痛点、创新和最小验证是什么。

3. `literature_and_venue_notes.md`
   - 看外部资料、会议目标、痛点和创新问题映射。
   - 用来回答：哪些结论有资料支撑，哪些还需要人工核验。

4. `paper_mainline_selection.md`
   - 看论文主线选择、暂缓方向、三个月执行切片和图表清单。
   - 用来回答：这么多研究点里，第一篇论文到底先做什么。

5. `research_idea_evaluation_and_action_plan.md`
   - 看所有点子的评审式排序、为什么好/不好、工程切入和 go/no-go 条件。
   - 用来回答：哪个点最值得押，哪些点暂缓，若要做应该先做什么实验。

6. `research_execution_plan.md`
   - 看候选论文路线、最小闭环、里程碑和实验矩阵。
   - 用来回答：这些研究点下一步具体怎么做。

7. `goal_coverage_audit.md`
   - 看原始目标、当前文档证据和剩余缺口。
   - 用来回答：当前整理工作完成了哪些，还不能声称完成哪些。

8. `research_package_completion_audit.md`
   - 看本轮研究整理的交付审计。
   - 用来回答：当前 paper 文档包是否已经满足“研究整理”目标。

9. `ieda_tool_edadb_opportunity_matrix.md`
   - 看 iEDA 每个点工具能怎样利用 EDADB。
   - 用来回答：除了 DEF，EDADB 能在哪些工具中提高性能或增强调试能力。

## 2. 再读 EDADB 自身

10. `edadb_core_research_notes.md`
   - 从 EDADB core 代码出发，记录 API、schema、traversal、SQLite backend 的能力和瓶颈。
   - 用来回答：EDADB 现在是什么，缺什么，应该如何演进。

11. `edadb_system_improvement_plan.md`
   - 把 EDADB core 能力提升整理成系统路线图和实验矩阵。
   - 用来回答：要支撑论文和工具加速，EDADB 需要补哪些系统模块。

## 3. 最后读具体实验计划

优先级建议：

1. `edadb_ipl_incremental_placement_plan.md`
   - iPL placement view，适合最先做增量 view 闭环。

2. `edadb_ifp_floorplanview_plan.md`
   - iFP FloorplanView，适合记录 floorplan command、die/core/row/track、IO、blockage 和 tapcell provenance。

3. `edadb_ino_netlist_eco_plan.md`
   - iNO NetlistECOView，适合先产生真实 ECO dirty set。

4. `edadb_idrc_incremental_drc_plan.md`
   - iDRC geometry view，适合证明 spatial index / provenance skipping。

5. `edadb_irt_routing_eco_plan.md`
   - iRT routing ECO view，适合扩展到 route diff 和 local reroute。

6. `edadb_timing_eco_plan.md`
   - iSTA/iTO timing view，适合做 graph affected cone 和 ECO action provenance。

7. `edadb_icts_clock_tree_plan.md`
   - iCTS clock tree view，适合做 clock subtree 和 CTS/timing/routing 联动。

8. `edadb_power_ir_pdn_plan.md`
   - iPNP/iPA/iIR PowerIRView，适合做 power/IR/PDN 跨工具研究。

9. `edadb_ipdn_pdnview_plan.md`
   - iPDN PDNView，适合记录 PDN command、special-net geometry、via 和 blockage cutting provenance。

10. `edadb_flow_dse_memory_plan.md`
    - FlowMemory / DSE Memory，适合统一 run、stage、config、artifact、QoR、failure 和跨工具 view provenance。

## 4. 当前最小闭环

建议第一个工程闭环：

```text
iNO fanout ECO
  -> EDADB NetlistECOView 产生 dirty inst/net/pin
  -> iPL PlacementView 做 incremental legalization / HPWL / bin density
  -> iDRC GeometryView 做 dirty region check
```

原因：

- iNO 能产生真实 netlist ECO，不需要只靠人工修改。
- iPL 已有 incremental legalization 接口。
- iDRC 最容易验证 full check 与 dirty check 的 violation set 一致性。

## 5. 写论文时的约束

- 不能声称 EDADB 当前已经是高性能 EDA DBMS；当前 core 更接近 SQLite-backed C++ object graph ORM。
- 不能把 arXiv 2026 线索直接当成已发表顶会论文；需要人工核验 venue 和 CCF 分类。
- 不能只保存 report 或 DEF table 就声称 database research。
- 每个增量方案都必须有 full-run validator。
- 每个 AI 方向都必须保留 database provenance 和 exact-tool validation，否则容易变成普通预测模型。
