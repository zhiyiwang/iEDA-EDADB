# EDADB + iEDA 研究点评审与行动计划

本文从 CS 论文评审和 PI 决策视角，评价 `docs/paper/` 中已有研究点。重点回答：

1. 哪些点有 DAC / ICCAD / SIGMOD / VLDB / ICDE 级别潜力。
2. 哪些点工程价值高但不适合单独作为第一篇主贡献。
3. 除 DEF read/write 外，EDADB 能怎样进入 iEDA 各点工具提高性能、复现性和调试能力。
4. 如果要做，第一阶段应做什么、为什么这么做、怎样验证。

本文不声称任何会议 CCF 分类已完成最终核验。投稿前仍需按最新版 CCF 推荐目录和目标 track 人工确认。

## 1. 证据基础

### 1.1 外部研究线索

| 线索 | 当前可支撑的结论 | 对 EDADB 的含义 |
| --- | --- | --- |
| OpenROAD / OpenDB | shared physical design database 已经是开源后端 flow 的核心基础设施。 | EDADB 不能只做 DEF roundtrip，必须体现 persistent query、incremental view、provenance、debug/replay。 |
| iEDA paper / iEDA codebase | iEDA 覆盖 placement、routing、DRC、CTS、STA、timing optimization、power/IR/PDN 等后端阶段。 | EDADB 有真实开源工具链可集成，不必停在 toy benchmark。 |
| CircuitNet / EDA-Schema-V2 / R2G / AiEDA | ML for EDA 数据、schema、多阶段多视图正在变热。 | 单做数据集风险高；EDADB 应强调 exact-tool validator、stage provenance、database-native incremental data。 |
| DBSP / IVM | rich query incremental view maintenance 是数据库系统核心问题。 | 可把 HPWL、bin density、DRC candidates、route occupancy、timing cone 抽象为 EDA-specific views。 |
| provenance-based data skipping / sketch maintenance | provenance 可用于跳过无关数据，但更新后维护 sketch 本身是难题。 | EDA 的 ECO、move、reroute、resize 天然提供局部更新 workload，可做 provenance-guided skipping。 |
| SIGMOD / VLDB / ICDE | 严肃数据库系统目标，VLDB 2026 官方主题包含 provenance/workflows、graph data、specialized/domain-specific data management。 | 如果冲 DB 会议，必须把 iEDA case 抽象成 domain-specific data management 和 hybrid object-spatial-graph workload。 |
| DAC / ICCAD | 严肃 EDA 目标，适合 physical design、design database、ECO/debug、AI for design。 | 如果冲 EDA 会议，应强调真实 flow 加速、full-tool correctness、PPA/DRC/timing 不变。 |

### 1.2 本地代码证据

| 代码事实 | 位置 | 研究含义 |
| --- | --- | --- |
| EDADB public API 已有 transaction、typed insert/read/update/upsert/query。 | `src/database/edadb/core/include/edadb.h` | 已具备 C++ object graph persistence 底座。 |
| EDADB 注释明确全局 manager / op / table cache 非线程安全。 | `src/database/edadb/core/include/edadb.h:32` | 当前不能直接包装成高性能并行 EDA DBMS。 |
| iPL 已有 changed instance incremental legalization 入口。 | `src/platform/tool_manager/tool_api/ipl_io/ipl_io.cpp:101` | EDADB dirty inst 可直接喂给 iPL。 |
| iPL wrapper 支持全量和按 instance list 更新。 | `src/operation/iPL/source/module/wrapper/IDBWrapper.cc:48`、`:77` | PlacementView 可从 wrapper cache 升级为 persistent incremental view。 |
| iDRC 当前从 iDB 构造 env/result shapes 后全量 check。 | `src/operation/iDRC/interface/DRCInterface.cpp:83` | EDADB 可避免每次 full shape rebuild。 |
| iDRC RuleValidator 已有 box partition、expand_size、per-box verify。 | `src/operation/iDRC/source/module/rule_validator/RuleValidator.cpp:51`、`:84`、`:100` | GeometryView / dirty box 是低风险高价值切入点。 |
| iNO fanout fix 会插 buffer、建新 net、重连 load pins。 | `src/operation/iNO/source/module/fix_fanout/FixFanout.cpp:32`、`:66` | 可作为真实 ECO delta generator，避免人工伪造 dirty set。 |
| iRT GCellMap 有 add/delete route state 更新路径。 | `src/operation/iRT/source/data_manager/DataManager.cpp:253`、`:289`、`:323` | RoutingView 有强潜力，但第一篇工程风险高。 |

## 2. 评审尺度

每个点按五个维度判断：

| 维度 | 强信号 | 弱信号 |
| --- | --- | --- |
| Novelty | 有可抽象的方法，如 EDA-specific IVM、provenance skipping、hybrid object-spatial-graph model。 | 只是把 report、DEF、log 存进 SQLite。 |
| Engineering readiness | iEDA 已有入口、dirty set、局部 API、validator。 | 需要重写点工具主算法或补大量缺失接口。 |
| Correctness oracle | 能用 full iEDA run / full DRC / full STA 做 oracle。 | 只能看局部指标，无法证明没有漏。 |
| Top-conference fit | 能讲清楚 EDA 痛点或 DB 系统问题，并有多设计实验。 | 只是工程整理，缺少可泛化问题。 |
| Risk | 第一阶段三个月内能跑通最小闭环。 | 依赖 power/timing/routing 多工具复杂联动。 |

## 3. 总体排序

| 排名 | 方向 | 推荐结论 | 为什么好 | 为什么不好 / 风险 | 第一阶段要做什么 |
| --- | --- | --- | --- | --- | --- |
| 1 | `EDA-1 + DB-1 + DB-2`：Persistent Incremental Views for ECO | 第一篇主线 | 跨 iNO/iPL/iDRC，真实 ECO，能同时讲 EDA 加速和 DB incremental/provenance。 | 工程面仍不小；如果只实现缓存会被认为 novelty 弱。 | M0 run/stage metadata，M1 NetlistECOView，M2 PlacementView，M3 GeometryView。 |
| 2 | `EDA-2`：Incremental DRC / Dirty-region Verification | 最强单点 case | iDRC 已有 box partition，correctness oracle 清楚，skip ratio 容易量化。 | rule-specific halo 若处理不严会漏 violation。 | 保存 DrcShape/DrcBox/DrcShapeBox，dirty box recheck 对比 full DRC。 |
| 3 | `DB-1`：EDA-specific IVM | DB-first 论文核心 | 可抽象 object + spatial + graph + metric view，适合 SIGMOD/VLDB/ICDE 叙事。 | 需要不止一个工具 case，否则像 iEDA 工程优化。 | 用 iPL HPWL/bin、iDRC candidate/violation 两个 view 做统一 update model。 |
| 4 | `DB-2`：Provenance-guided Skipping | DB novelty 最强补强项 | provenance unit 可定义为 object/net/layer/tile/stage，和 ECO 局部性高度匹配。 | 需要证明 no false negative 和维护成本，否则只是启发式过滤。 | 先做 tile/net/stage provenance，报告 skip ratio、false positive、maintenance cost。 |
| 5 | `EDA-3`：RoutingView / local reroute | 第二篇强 case | iRT GCellMap 天然是 persistent route-state DB 的雏形。 | local reroute 接口、route object stable id、DRC halo 复杂。 | 第一阶段只抽取 RouteView 和 dirty gcell query，不急着承诺 reroute 加速。 |
| 6 | `EDA-4`：Timing/CTS/Routing ECO DB | 高价值后续 | affected cone、clock subtree、route/timing provenance 很有 EDA 痛点。 | STA/CTS/TO 语义复杂，验证链长。 | 先保存 TimingMetric/RC summary，再做 changed net affected cone coverage。 |
| 7 | `EDA-5` / `AI-5`：PowerIR/PDN provenance | 应用吸引力强 | power/current/PDN/IR graph 多源融合，适合 debug 和 AI prediction。 | 工程跨度最大，单做 predictor 会撞已有工作。 | 先做 PowerView + IRView report 一致，再接 PDN command provenance。 |
| 8 | `EDA-6` / `AI-2`：FlowMemory / Agent memory | 工程和 AI 展示好 | 可服务 DSE、failure blame、agent query。 | 单独论文容易像日志系统或 agent demo。 | 作为主线 A 的 experiment infrastructure，记录 run/stage/config/QoR/artifact。 |
| 9 | `DB-5`：Versioned Object Store / Time-travel | 必要底座 | 对 ECO replay、debug blame、dataset provenance 都关键。 | 单独投顶会需要强 query model 和 replay benchmark。 | 做 run/stage/version/change log，服务 M0 和后续 delta。 |
| 10 | `DB-7`：Partial Update Semantics | 必须补强 | 直接解决 EDADB update/upsert 全图替换导致的写放大。 | 单独 novelty 不够，需要和 incremental views 绑定。 | 实现 dirty child vector append/delete/update microbenchmark。 |
| 11 | `DB-3`：Hybrid storage/index | 系统章节 | SQLite + memory tile/R-tree + adjacency + Arrow/Parquet 是合理架构。 | 做完整会分散，第一篇容易超范围。 | 先做 tile index、net adjacency、batch transaction。 |
| 12 | `DB-4`：Index advisor | 后续方向 | 有数据库味，可从 iEDA trace 推荐索引。 | 需要大量 workload trace 和 cost model。 | 等 M1-M3 产生真实 query log 后再做。 |
| 13 | `DB-6` / `AI-7`：Schema synthesis / adapter generation | 工具论文或 appendix | 能降低 EDADB 适配成本。 | 第一篇主贡献偏弱。 | 做 schema lint、DEF field checker、roundtrip coverage report。 |
| 14 | `EDA-7`：Cross-stage consistency debugging | 工程价值高 | 能定位 DEF/iDB/tool view/report 不一致。 | 单独创新弱。 | 作为 validator/debug artifact，不作为主线。 |
| 15 | `AI-1`：Provenance-aware dataset | 可作为扩展 | 和 EDA-Schema/CircuitNet/AiEDA 热点相关。 | 数据集赛道拥挤；没有模型或 validator 不够。 | 从主线 A 导出 stage-aligned table/grid/graph。 |
| 16 | `AI-3/4/6`：learned planner / ECO prediction / co-training | 长线 | 需要 EDADB 历史数据后才自然。 | 现在数据规模和标签不足。 | 暂缓，等 FlowMemory 和 exact validators 稳定。 |

## 4. 第一篇论文建议

### 4.1 题目

```text
EDADB: Persistent Incremental Views for Open-source Physical Design ECO
```

或者更偏 DB：

```text
Incremental View Maintenance for Hybrid Object-Spatial-Graph EDA Workloads
```

### 4.2 核心论点

物理设计 ECO 后，设计只改变少量 instance/net/pin/shape，但工具常重复构建 placement wrapper、DRC shape partition、routing/timing candidate state。EDADB 可以把这些短生命周期中间状态转成 persistent incremental views，并用 provenance/dirty set 保证只重算受影响区域，同时用 full tool run 验证正确性。

### 4.3 最小闭环

```text
iNO fanout ECO
  -> EcoAction / EcoDirtyInst / EcoDirtyNet / EcoDirtyPin
  -> PlacementView affected net/bin/HPWL/density
  -> iPL incremental legalization(changed_inst_list)
  -> GeometryView dirty bbox/tile/box
  -> iDRC dirty-region recheck
  -> full iEDA/iDRC oracle comparison
```

### 4.4 为什么这条线最好

- iNO 提供真实 ECO delta，不用只做 synthetic dirty set。
- iPL 已有 changed instance incremental legalization 接口，工程落点明确。
- iDRC 的 box partition 已存在，最容易展示 spatial skipping 和 correctness。
- 三个工具组合能证明 EDADB 是跨阶段 view database，而不是 DEF cache。
- 结果可以同时服务 EDA 会议和 DB 会议：EDA 讲 ECO runtime/debug，DB 讲 hybrid IVM/provenance skipping。

### 4.5 为什么它仍可能失败

- 如果 EDADB view update 开销接近或超过 full rebuild，性能主张会弱。
- 如果 dirty DRC 不能和 full DRC violation set exact match，正确性主张会弱。
- 如果只在 `sky130_gcd` 一个小设计上实验，数据库和 EDA 顶会说服力不足。
- 如果论文只描述 schema，不抽象 update/view/provenance model，会被认为是工程系统而非研究贡献。

## 5. 除 DEF read/write 外，EDADB 怎样提升 iEDA

| 工具 | 当前重复成本 | EDADB 切入点 | 性能收益机制 | 研究收益 |
| --- | --- | --- | --- | --- |
| iNO | ECO action 只留在 iDB mutation 和 log 中。 | NetlistECOView 记录 inserted buffer、new net、moved pins。 | 直接产出 dirty inst/net/pin，减少后续工具 diff 成本。 | 真实 ECO delta generator。 |
| iPL | wrapper、HPWL、bin density 常全量重建/扫描。 | PlacementView 保存 PlInst/PlNet/PlNetPin/PlBin。 | changed inst -> affected nets/bins，只更新局部 metric。 | graph-spatial metric IVM。 |
| iDRC | 每次从 iDB 构造 shape list 和 RVBox。 | GeometryView/DrcView 保存 shape/tile/box/violation。 | dirty region -> dirty box，只 recheck affected boxes。 | spatial provenance skipping。 |
| iRT | GCellMap、route result、violation 是临时内存状态。 | RoutingView 保存 route segment/patch/via/gcell object。 | route delta -> dirty gcell/net/layer，局部 recheck/reroute。 | spatial-graph route state DB。 |
| iSTA/iTO | timing graph、RC tree、affected cone 难跨 stage 复用。 | TimingView 保存 pin/arc/RC/path/dirty vertex。 | route/ECO delta -> affected cone，减少 full timing propagation。 | graph IVM + ECO provenance。 |
| iCTS | clock tree、sink、buffer、metric 缺持久化 provenance。 | ClockTreeView 保存 subtree/action/metric dependency。 | clock ECO -> dirty subtree，只更新局部 tree metric。 | clock/timing/routing 联动 debug。 |
| iPDN/iPNP/iPA/iIR | PDN command、power current、IR node、score 分散。 | PDNView + PowerIRView。 | template/shape/current delta -> affected IR region/hotspot。 | power integrity provenance + exact AI validation。 |
| iFP | floorplan/tapcell/IO action 难 replay。 | FloorplanView 保存 command/generated object/provenance。 | stage diff 和 dirty region 复用，支持后续 PL/PDN/DRC。 | flow provenance 底座。 |
| Flow/DSE | 多次 sweep 的 stage、artifact、QoR、failure 难复用。 | FlowMemory 保存 run/stage/config/artifact/metric/failure。 | 复用中间 stage，快速定位 QoR drift/failure。 | DSE memory / agent query substrate。 |

## 6. EDADB 自身必须补强什么

| 优先级 | 能力 | 为什么必须做 | 最小实现 | 验证 |
| --- | --- | --- | --- | --- |
| P0 | run/stage/version metadata | 没有 version 就不能谈 ECO delta、replay、provenance。 | `DbRun`、`DbStage`、`DbVersion`、`DbChangeLog`。 | 同一 flow 每阶段可查询 input/output artifact 和 QoR。 |
| P0 | domain query wrapper | 当前 API 偏 root object query，不适合工具按 bbox/net/tile/stage 查询。 | `queryByNet`、`queryByBBox`、`queryByTile`、`queryDirtyObjects`。 | 对比 iDB full traversal / SQLite naive scan。 |
| P0 | batch transaction / prepared op reuse | 工具 view 写入量大，单条 SQL 成本会淹没增量收益。 | view writer 批量写 shape/net/bin/metric。 | insert/update throughput、statement count、DB size。 |
| P1 | partial child/vector update | 当前 update/upsert 全图替换，ECO 写放大大。 | child append/delete/update、field dirty mask。 | dirty ratio 0.1/1/5% 下的 write amplification。 |
| P1 | tile/R-tree spatial index | iDRC/iRT/PDN 需要 bbox overlap。 | memory tile index 或 SQLite R-tree prototype。 | bbox query latency、candidate count、false positive。 |
| P1 | net/inst adjacency cache | iPL/iRT/iSTA 需要 graph traversal。 | `net -> pins/insts`、`inst -> nets` cache。 | affected net/bin/cone query latency。 |
| P2 | provenance unit and sketch | 支撑 DB novelty，不只是普通 index。 | object/net/layer/tile/stage provenance rows。 | skip ratio、false positive、maintenance cost。 |
| P2 | analytics export | AI/data extension 需要 table/grid/graph 多视图。 | Arrow/Parquet/CSV export。 | cross-view consistency validator。 |
| P2 | schema lint / DEF coverage checker | 降低 adapter 维护成本。 | 检查 PK/FK/shadow/field coverage。 | roundtrip field coverage report。 |

## 7. 实验设计

### 7.1 必做实验

| 实验 | 问题 | Baseline | EDADB variant | 成功标准 |
| --- | --- | --- | --- | --- |
| E1 NetlistECOView correctness | iNO dirty set 是否准确。 | iDB snapshot diff / STA fanout report。 | EcoAction + dirty inst/net/pin。 | inserted buffer/new net/moved pin exact match。 |
| E2 Placement metric IVM | HPWL/bin density 是否可局部维护。 | full iPL metric recompute。 | affected net/bin update。 | per-net HPWL、total HPWL、bin density exact match。 |
| E3 iPL incremental legalization handoff | dirty inst 能否驱动真实工具。 | full legalization。 | EDADB dirty inst -> iPL incr LG。 | legality 通过，runtime 降低或 dirty prep 更低。 |
| E4 GeometryView query | dirty bbox 能否减少 candidate scan。 | full iDB geometry scan。 | bbox/tile/domain query。 | candidate shapes/latency 显著下降。 |
| E5 Dirty DRC correctness | 局部 DRC 是否无漏报。 | full iDRC check。 | dirty box recheck。 | violation set exact match，missed violation = 0。 |
| E6 EDADB write path | EDADB 自身是否拖慢。 | full graph update。 | partial/batch update。 | write amplification 和 update latency 降低。 |
| E7 provenance skipping | provenance 是否比普通 dirty tile 更强。 | no provenance / tile only。 | tile+net+stage provenance。 | 更高 skip ratio，false positive 可控，无 false negative。 |

### 7.2 数据集要求

- 起步可以用 `sky130_gcd` 打通流程。
- 投稿前至少需要多个 design、多 dirty ratio、多 ECO 类型。
- DB 会议版本最好再加 synthetic EDA-like workload，用来控制 dirty ratio、net degree、shape density、layer count。
- AI/data extension 必须有 train/valid/test split 和跨设计泛化，不可只展示单设计拟合。

## 8. 论文贡献写法

### 8.1 EDA 会议版本

主贡献应写成：

1. Persistent incremental view architecture for open-source physical design ECO。
2. Netlist ECO -> placement metric -> DRC dirty region 的跨阶段 dirty propagation。
3. Full-tool oracle correctness validator。
4. iEDA 上的 runtime、skip ratio、debug query、storage overhead 实验。

不要主打：

- “我们把 DEF 存到 SQLite”。
- “我们保存了很多 report 表”。
- “我们做了一个 AI predictor 但没有 exact validation”。

### 8.2 Database 会议版本

主贡献应写成：

1. EDA-specific update model：move / insert buffer / split net / add route segment / resize / reroute。
2. Hybrid object-spatial-graph views：net bbox、bin density、DRC candidates、route occupancy、timing cone。
3. Provenance-guided no-false-negative skipping under local updates。
4. Real iEDA flow + synthetic controlled workload benchmark。

不要主打：

- 单一工具 wrapper cache。
- 手写 schema 数量。
- 没有抽象的 SQLite 工程优化。

## 9. 三个月行动计划

| 时间 | 目标 | 产物 | Go / No-go |
| --- | --- | --- | --- |
| Week 1-2 | M0 实验底座 | run/stage/version 表，runtime/memory/artifact 记录。 | 能复现实验并查询每阶段输入输出。 |
| Week 3-4 | M1 NetlistECOView | iNO action logger，dirty inst/net/pin validator。 | dirty set 与 iDB diff exact match。 |
| Week 5-7 | M2 PlacementView | PlInst/PlNet/PlNetPin/PlBin，HPWL/bin delta。 | full vs incremental metric exact match。 |
| Week 8-10 | M3 GeometryView | DrcShape/DrcBox/DrcShapeBox/DrcViolation，dirty box query。 | full DRC vs dirty DRC violation set exact match。 |
| Week 11 | EDADB microbench | batch/partial/tile/net query benchmarks。 | EDADB overhead 不吞掉增量收益。 |
| Week 12 | paper skeleton | architecture/model/experiment figures。 | 有至少一个完整端到端 ECO case 和多设计扩展计划。 |

## 10. 停止条件与转向

主线 A 应暂停或降级，如果出现：

- iNO 无法稳定产生可控 fanout ECO。
- iPL incremental legalization 不能接受或不能受益于 dirty inst。
- iDRC dirty-region recheck 无法与 full DRC 对齐。
- EDADB query/update overhead 大到超过 full rebuild，且 debug/provenance 也没有明显收益。

转向策略：

- 如果 iNO 不稳定，先用 controlled placement move / buffer insertion workload 做 M2+M3。
- 如果 iPL 收益弱，强化 DB-first：HPWL/bin + DRC candidate 两个 view 的 IVM microbenchmark。
- 如果 iDRC dirty correctness 难，先只做 GeometryView query + full DRC validator，把 local check 作为后续。
- 如果真实设计太少，补 synthetic EDA-like workload，但保留至少一个真实 iEDA flow case。

## 11. 当前最终建议

第一篇论文押：

```text
NetlistECOView + PlacementView + GeometryView
```

研究包装为：

```text
Persistent, provenance-aware incremental views for physical-design ECO.
```

工程实现顺序为：

```text
run/stage metadata
-> iNO dirty set
-> iPL HPWL/bin delta
-> iDRC dirty box/violation validator
-> EDADB batch/partial/domain query optimization
```

这条路线最好，是因为它不依赖空泛的“数据库会让 EDA 更快”，而是把性能收益绑定到可验证的 dirty propagation 和 full-tool correctness。它的风险也最可控：每一步都有现成 iEDA 代码入口、可观测中间状态和明确 oracle。
