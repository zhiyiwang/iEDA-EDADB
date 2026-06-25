# 文献与会议目标台账

本文记录 EDADB + iEDA 研究路线用到的外部资料、能支撑的结论、对应痛点和投稿目标风险。目的是防止后续总结时把“线索”写成“已证明结论”。

## 1. 会议目标核验状态

当前只把以下会议作为严肃目标讨论：

- EDA：`DAC`、`ICCAD`。
- Database：`SIGMOD`、`VLDB`、`ICDE`。

状态：

- `DAC`：已核验官网入口，适合 EDA research / engineering / AI for design 相关工作。
- `ICCAD`：已核验官网入口，但本地未完成 CCF 最新目录核验。
- `SIGMOD / VLDB / ICDE`：数据库系统方向严肃目标，但本地未完成 CCF 最新目录核验。

写作约束：

- 文档中可以说“目标会议”或“严肃目标”，不要直接写“已确认 CCF A”。
- 最终投稿前必须人工核对最新版 CCF 推荐目录。
- 如果 CCF 目录中某会议分类与本文假设不一致，以 CCF 最新目录为准。

## 2. 外部资料分组

### 2.1 EDA database / open flow baseline

| 资料 | 链接 | 支撑结论 | 对 EDADB 的启发 |
| --- | --- | --- | --- |
| OpenDB 文档 | `https://openroad.readthedocs.io/en/latest/main/src/odb/README.html` | OpenDB 是 OpenROAD 的 physical design database，基于 LEF/DEF，支持 binary save/load；其类设计目标是足够快，避免工具复制到专用结构。 | EDADB 不能停留在 DEF roundtrip；需要 query、index、view、incremental 能力。 |
| OpenROAD 文档 | `https://openroad.readthedocs.io/en/latest/main/README.html` | OpenROAD 覆盖 floorplan、placement、CTS、routing、PDN、IR、STA 等完整后端阶段。 | EDADB 研究应覆盖跨阶段 flow，而不是孤立数据库读写。 |
| iEDA paper | `https://arxiv.org/abs/2308.01857` | iEDA 覆盖 physical implementation flow 和分析工具，包括 placement、CTS、routing、timing optimization、STA、power analysis。 | 本项目有真实开源 EDA flow，可做工具集成实验。 |

潜在痛点：

- 开源 EDA 已有 shared in-memory DB baseline。
- 如果 EDADB 只做另一个 object store，创新不足。

可转化创新：

- persistent incremental view manager；
- stage/version/provenance database；
- spatial/graph/object 混合查询系统。

### 2.2 ML for EDA / dataset / schema

| 资料 | 链接 | 支撑结论 | 对 EDADB 的启发 |
| --- | --- | --- | --- |
| ML for EDA Survey | `https://arxiv.org/abs/2102.03357` | ML 正在进入 EDA 多个层级，设计复杂度推动数据驱动方法。 | EDADB 可作为可复现数据底座，但不能只做数据搬运。 |
| CircuitNet | `https://arxiv.org/abs/2208.01040` | ML for EDA 缺少公开大规模数据集，CircuitNet 以数据集形式支撑多个任务。 | EDADB 的 dataset 方向必须强调 provenance、stage alignment 和 exact-tool validation。 |
| EDA-Schema-V2 | `https://arxiv.org/abs/2605.06952` | 待人工核验：physical design 多模态 schema、跨阶段数据和 benchmark 已成为前沿线索。 | EDADB 创新应转向 database-native incremental/provenance，而不是只定义 schema。 |
| R2G | `https://arxiv.org/abs/2604.08810` | 待人工核验：RTL-to-GDS 多视图 graph benchmark 强调 representation 对模型效果有影响。 | EDADB 可提供一致的 table/grid/graph 多视图导出。 |
| DALI-PD | `https://arxiv.org/abs/2507.10606` | 待人工核验：synthetic layout heatmap 覆盖 power、IR drop、congestion、macro placement、density。 | EDADB 可提供真实 flow 数据校准 synthetic 数据。 |
| AiEDA / iDATA | `https://arxiv.org/abs/2511.05823` | 待人工核验：AI-EDA infrastructure 和 design-to-vector 数据流水线正在形成系统化平台。 | EDADB 不能只做 feature export；要突出 exact-tool validation、object provenance 和增量数据库能力。 |
| EDA Corpus | `https://arxiv.org/abs/2405.06676` | LLM/agent for EDA 需要公开、可许可的数据和脚本/问答语料。 | EDADB 的 agent 方向应让 agent 精确查询 design state，而不是只读自然语言日志。 |

潜在痛点：

- 数据缺乏 provenance，模型结果难以回连到 EDA object。
- 多视图数据容易不一致。
- synthetic 数据快但真实性需要校验。
- AI-for-EDA 基础设施和数据集工作正在增多，单纯导出更多特征或脚本语料很难成为第一篇强贡献。

可转化创新：

- DB-backed stage-aware dataset；
- real/synthetic co-training with EDADB validation；
- LLM/agent queryable design memory。

### 2.3 Power / IR / PDN prediction

| 资料 | 链接 | 支撑结论 | 对 EDADB 的启发 |
| --- | --- | --- | --- |
| PowerNet | `https://arxiv.org/abs/2011.13494` | Dynamic IR drop estimation 成本高，ML 方法用于加速估计。 | EDADB 可保存 power/RC/IR 数据和验证闭环，而不只训练 predictor。 |
| PDNNet | `https://arxiv.org/abs/2403.18569` | PDN structure 与 cell-PDN relation 对 IR prediction 重要。 | EDADB 的 PowerIRView 应同时保存 PDN graph、cell current、IR result。 |

潜在痛点：

- IR/PDN 预测需要 power、placement、PDN geometry、RC graph 多源特征。
- 预测模型若没有 exact solver/tool 验证，很难被设计 flow 信任。

可转化创新：

- provenance-aware PowerIRView；
- prediction + exact validation 混合流程；
- PDN template optimization trace database。

### 2.4 Database / incremental view / provenance

| 资料 | 链接 | 支撑结论 | 对 EDADB 的启发 |
| --- | --- | --- | --- |
| DBSP | `https://arxiv.org/abs/2203.16684` | Incremental view maintenance 是数据库核心问题，DBSP 尝试为 rich query language 做自动 IVM。 | 可把 HPWL、density、DRC candidate、timing cone 抽象成 EDA view。 |
| Provenance-based Data Skipping | `https://arxiv.org/abs/2104.12815` | provenance sketches 可记录 query 相关数据，用于后续 data skipping。 | 可把 EDA query provenance 单元定义为 object/net/layer/tile/stage。 |
| Incremental Maintenance of Provenance Sketches | `https://arxiv.org/abs/2505.20683` | provenance sketch 在更新后会 stale，需要增量维护。 | ECO/placement/routing update 正好是 EDA 版 sketch maintenance workload。 |
| Cost-based Selection of Provenance Sketches | `https://arxiv.org/abs/2504.19252` | sketch 选择会影响效果，需要 cost-based selection。 | EDADB 可研究 tile/net/layer/stage provenance 粒度选择。 |

潜在痛点：

- 通用数据库工作负载不能充分覆盖 EDA 的 object + spatial + graph + matrix 混合访问。
- EDA 更新频繁，full rebuild 成本高。

可转化创新：

- EDA-specific IVM；
- provenance-guided spatial/graph data skipping；
- workload-aware hybrid index/view selection。

## 3. 研究点与资料映射

| 研究点 | 外部依据 | 本地代码依据 | 最小验证 |
| --- | --- | --- | --- |
| Incremental PlacementView | OpenDB / DBSP | iPL `IDBWrapper`、`runIncrementalLegalization()` | HPWL/bin density full vs delta 一致。 |
| GeometryView / incremental DRC | OpenDB / PBDS / IMP | iDRC shape/rule/RVBox/Violation | full DRC violation set vs dirty-region check。 |
| RoutingView / local reroute | OpenDB / PBDS | iRT GCellMap、routing result、violation reporter | full route view vs route delta 一致。 |
| TimingView / affected cone | DBSP / graph IVM | iSTA TimingEngine、iTO buffer/resize API | full STA endpoints vs affected cone coverage。 |
| PowerIRView | PowerNet / PDNNet | iPA Power、iIR solver、iPNP optimization | power/IR report 与 EDADB query 一致。 |
| Stage-aware ML dataset | CircuitNet / EDA-Schema-V2 / R2G | iEDA full flow、ToolManager、EDADB stage schema | 多视图一致性 + downstream baseline。 |
| Agent/queryable design memory | EDA Corpus / AiEDA / agentic EDA 线索 | FlowMemory、EDADB query layer、tool report/artifact | log-only agent vs EDADB-query agent 成功率和工具调用数。 |
| Provenance skipping DB system | PBDS / IMP / cost-based sketches | EDADB query layer、dirty set、tool views | skip ratio、maintenance cost、false positive。 |

## 4. 当前最有把握的论文雏形

### 4.1 EDA 会议版本

题目雏形：

```text
An EDA-native Persistent Incremental View Database for Open-source Physical Design ECO
```

核心贡献：

- 用 EDADB 定义 placement/geometry/routing/timing view。
- 将 iEDA ECO 转换为 dirty object/net/tile。
- 用 full iEDA run 证明增量 view 正确，并评估 runtime/debug/replay 收益。

风险：

- 如果只做 iEDA 工程缓存，方法泛化性不够。
- 至少需要两个工具闭环，最好 iPL + iDRC + iNO ECO。

### 4.2 Database 会议版本

题目雏形：

```text
Incremental Maintenance and Provenance Skipping for Hybrid Object-Spatial-Graph EDA Workloads
```

核心贡献：

- 抽象 EDA workload：object graph + bbox geometry + netlist graph + iterative local update。
- 提供 view maintenance / provenance skipping / hybrid index 的系统设计。
- 在 iEDA 真实 flow 和 synthetic EDA-like workload 上评估。

风险：

- 单一 SQLite adapter 不够数据库系统贡献。
- 需要明确 query algebra、update model、cost model、baseline。

### 4.3 AI 交叉版本

题目雏形：

```text
Provenance-aware Physical Design Data Memory for ML and Agentic EDA Optimization
```

核心贡献：

- EDADB 保存 stage-aligned design memory。
- 导出 table/grid/graph 多视图并保持一致。
- Agent/ML 预测必须通过 EDADB provenance 和 exact-tool validator 闭环。

风险：

- 容易落入“又一个数据集/agent demo”。
- 必须证明 database memory 提升了可复现性、可解释性或迭代效率。

## 5. 后续需要人工核验

- 最新 CCF 推荐目录中 DAC、ICCAD、SIGMOD、VLDB、ICDE 的分类。
- EDA-Schema-V2、R2G、DALI-PD、AiEDA / iDATA 的最终发表 venue、代码和数据可用性。
- PowerNet、PDNNet 是否已有更强近年 baseline。
- OpenDB / OpenROAD 中是否已有类似 stage/version/provenance 功能，避免重复造轮子。
