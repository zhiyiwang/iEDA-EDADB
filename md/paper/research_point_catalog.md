# EDADB + iEDA 研究点候选清单

本文只列候选问题，不声称已经成立为论文贡献。每个点后续都需要文献核验、baseline 对比、代码实现和实验验证。

## 0. 外部依据

- OpenDB / OpenROAD：shared physical design database 是开源后端 flow 的关键基础设施；EDADB 不能只做 DEF roundtrip。
  - `https://openroad.readthedocs.io/en/latest/main/src/odb/README.html`
  - `https://openroad.readthedocs.io/en/latest/main/README.html`
- iEDA：已有 physical design flow 和分析工具，可作为真实系统实验平台。
  - `https://arxiv.org/abs/2308.01857`
- ML for EDA / dataset：EDA 数据、schema、跨阶段 benchmark 是活跃方向，但只做数据集不够。
  - `https://arxiv.org/abs/2102.03357`
  - `https://arxiv.org/abs/2208.01040`
  - `https://arxiv.org/abs/2605.06952`
- Database IVM / provenance：incremental view maintenance 和 provenance skipping 可作为 EDADB 系统创新依据。
  - `https://arxiv.org/abs/2203.16684`
  - `https://arxiv.org/abs/2104.12815`
  - `https://arxiv.org/abs/2505.20683`

## 1. EDA 领域研究点

| 编号 | 研究点 | 痛点 | EDADB 可做的创新 | 最小验证 |
| --- | --- | --- | --- | --- |
| EDA-1 | Persistent Incremental ECO View Database | ECO 后 placement/DRC/routing/timing 常要重复 full run。 | 将 iNO 产生的 netlist ECO 转成 dirty inst/net/pin/bbox，并驱动 iPL/iDRC/iRT 增量视图。 | iNO fanout ECO 后，增量 legalization + dirty DRC 与 full run 一致。 |
| EDA-2 | Incremental DRC / Dirty-region Verification | DRC 全量检查慢，局部 ECO 后多数区域无变化。 | 建立 GeometryView：object/layer/tile/provenance 索引，只检查受影响区域。 | full DRC violation set 与 dirty-region DRC 差异为 0，报告 skip ratio。 |
| EDA-3 | Routing ECO and Local Reroute Database | routing 修改后难追踪 guide/shape/violation 的局部影响。 | 建立 RoutingView：net/guide/gcell/shape/violation/version 视图，支持 route diff 与 local reroute。 | 修改局部 net 后，增量 routing view 与 iRT full route 输出一致。 |
| EDA-4 | Timing / CTS / Routing Cross-stage ECO DB | buffer、clock、route、timing 的影响跨工具传播，调试困难。 | 建立 ClockTimingView：clock subtree、timing cone、routing shape 和 ECO action provenance 统一记录。 | CTS 或 buffer ECO 后，affected cone 覆盖 full STA 变化 endpoint。 |
| EDA-5 | PDN / Power / IR Provenance View | IR hotspot 依赖 placement、power、PDN geometry、RC graph，多源数据难对齐。 | 建立 PowerIRView：PDN graph、instance current、IR node、hotspot 和 PDN action provenance。 | EDADB 查询的 IR hotspot 与 iIR/iPA report 一致，并能回溯到 PDN shape。 |
| EDA-6 | Flow-level DSE Memory and Replay | 多参数 flow sweep 成本高，失败原因和中间状态难复用。 | 将每次 flow 的参数、stage snapshot、QoR、violation、dirty diff 持久化为 DSE memory。 | 对比无记忆 sweep，评估复用中间结果后的 runtime、失败定位和 QoR 搜索效率。 |
| EDA-7 | Cross-stage Consistency Debugging | DEF、iDB、tool internal view、report 之间容易不一致。 | EDADB 提供 stage/version/schema validator，检查 object count、geometry、net connectivity、timing/power report 对齐。 | 对每个 stage 自动生成一致性报告，定位 injected mismatch。 |

## 2. Database 领域研究点

| 编号 | 研究点 | 痛点 | EDADB 可做的创新 | 最小验证 |
| --- | --- | --- | --- | --- |
| DB-1 | EDA-specific Incremental View Maintenance | 通用 IVM 不直接覆盖 object + spatial + graph + matrix 混合 workload。 | 定义 EDA view algebra：object graph、bbox geometry、netlist graph、timing cone、RC matrix。 | HPWL、bin density、DRC candidate、timing cone 等 view 的增量维护正确且快于 rebuild。 |
| DB-2 | Provenance-based Spatial/Graph Data Skipping | EDA 查询常按 net/layer/tile/stage 访问，传统索引不能充分利用历史查询相关性。 | 将 provenance sketch 粒度设计为 object/net/layer/tile/stage，并支持 ECO 后增量维护。 | 测量 skip ratio、false positive、sketch maintenance cost。 |
| DB-3 | Hybrid Row/Column/Spatial/Graph Storage | 单一 SQLite row table 不适合所有 EDA 查询。 | 根据 workload 自动选择 row store、column export、R-tree/tile index、graph adjacency view。 | 对 placement/DRC/routing/timing 查询比较不同 layout 的延迟和空间成本。 |
| DB-4 | Workload-aware EDA Index Advisor | 不同工具需要不同索引，手工建索引难维护。 | 从 iEDA tool trace 中学习查询模式，推荐 tile/layer/net/instance/timing-cone 索引。 | 比较 no-index、manual-index、advisor-index 的 runtime 和维护成本。 |
| DB-5 | Versioned EDA Object Store / Time-travel Query | ECO 调试需要知道对象何时、为何、被哪个工具修改。 | 为 iDB object 增加 stage version、operation log、delta chain 和 time-travel query。 | 查询任意 stage 的 object 状态，并重放 ECO diff 得到相同结果。 |
| DB-6 | Schema Synthesis and Static Verification for C++ Object ORM | 手写 TABLE4CLASS/shadow 容易漏字段、错 ownership、错 primary key。 | 从 C++ class + DefRead/DefWrite 语义生成 schema 候选，并静态检查 shadow 必要性。 | 对已适配类生成 field coverage report，发现漏列/冗余列/不必要 shadow。 |
| DB-7 | Partial Update Semantics for Object Graphs | 当前对象图更新容易退化为 delete + insert，复杂对象代价高。 | 支持 dirty-field、dirty-child-vector、primary-key-preserving partial update。 | 对 instance/net/via/special-net 更新，比较 partial update 与 full rewrite 成本。 |

## 3. AI + EDA + Database 交叉研究点

| 编号 | 研究点 | 痛点 | EDADB 可做的创新 | 最小验证 |
| --- | --- | --- | --- | --- |
| AI-1 | Provenance-aware Stage Dataset | 现有 ML 数据常缺少跨阶段 provenance，预测结果难回连到 EDA object。 | EDADB 导出 stage-aligned table/grid/graph 多视图，并保留 object provenance。 | 在 congestion/timing/IR 任务上比较普通特征与 provenance-aware 特征。 |
| AI-2 | Agentic EDA Optimization with Queryable Memory | LLM/agent 容易只读日志、不能精确查询 design state。 | 让 agent 通过 EDADB 查询 object/violation/delta，而不是解析文本 report。 | 同一 ECO/debug 任务中，比较 log-only agent 与 EDADB-query agent 的成功率和工具调用数。 |
| AI-3 | Learning-guided Incremental Query Planning | dirty-region、affected-cone、index 粒度选择依赖设计和 ECO 类型。 | 用历史 EDADB workload 训练 planner，选择 incremental vs full、tile 粒度、索引组合。 | 在多设计多 ECO 上比较 rule-based planner 与 learned planner。 |
| AI-4 | ECO Impact Prediction from Delta Graph | ECO 后 PPA/violation 影响难提前估计，full run 代价高。 | 将 ECO action、dirty net、placement neighborhood、routing/timing provenance 构成 delta graph。 | 预测 legalization 成本、DRC 风险、timing endpoint 变化，并用 exact tool 验证。 |
| AI-5 | Power/IR/PDN Prediction with Exact Validation | IR prediction 若缺少 PDN/RC/current provenance，可信度低。 | EDADB 保存 PDN graph、instance current、IR solver result，形成 prediction + exact validation 闭环。 | 预测 hotspot 后用 iIR exact solver 验证，报告误差和可解释 provenance。 |
| AI-6 | Real/Synthetic Physical-design Co-training | synthetic 数据量大但 domain gap 难控制。 | 用 EDADB 对 synthetic 与 real flow 数据做 schema 对齐、feature distribution 对齐和 exact-tool 校验。 | 比较 real-only、synthetic-only、co-training 的跨设计泛化。 |
| AI-7 | LLM-assisted Schema / Adapter Generation | EDADB adapter 手工维护成本高，容易偏离 DefRead/DefWrite 语义。 | LLM 只生成候选 schema/adapter，EDADB validator 自动检查字段覆盖、roundtrip、ownership 风险。 | 对新类适配时间、错误数、roundtrip 通过率做 ablation。 |

## 4. 当前优先级建议

1. 先做 `EDA-1 + DB-1 + DB-2`：这是最像系统论文主线的组合。
2. 工程闭环建议仍从 `iNO -> iPL -> iDRC` 开始。
3. AI 方向先作为增强实验，不要先做成主线；否则容易变成普通 predictor 或 agent demo。
4. 每个点必须保留 full-run validator；没有 validator 的增量结果不能作为论文结论。
