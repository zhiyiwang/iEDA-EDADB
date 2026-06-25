# EDADB + iCTS ClockTreeView / Multi-clock CTS 实验计划

本文是 `EDADB + iEDA` 研究路线中 `EDA-4 / DB-1 / AI-1` 的细化计划。目标是把 iCTS 中的 clock net、buffer tree、signal wire、solver net、RC/timing estimate、skew/fanout/cap 约束和评估结果持久化为 EDADB ClockTreeView，用于多时钟 CTS 调试、ECO 后局部 clock tree 修复、stage-aware dataset 和 ML/agent 分析。

## 1. 研究问题

### 1.1 核心问题

iCTS 支持多时钟、skew/fanout/cap/transition/length 约束、buffer insertion、tree builder、router 和 timing evaluation。问题是：

- clock tree solver 的中间结果主要保存在内存对象中；
- `CtsIO` 有单独的文件/GUI tree 数据路径，但没有和 EDADB stage/version/provenance 统一；
- 多时钟 balance、buffer selection、skew 修复和 timing evaluation 的决策过程不易复验；
- ECO 后只影响部分 clock nets/subtrees，但缺少 persistent subtree delta 和 affected sink/latency view。

研究目标：

```text
Can a persistent ClockTreeView preserve CTS topology, buffer decisions, and skew/timing provenance, enabling local CTS ECO repair and reproducible multi-clock analysis?
```

### 1.2 预期贡献

1. 定义 clock tree topology、buffer instance、wire segment、sink latency、constraint violation 的持久化 schema。
2. 设计 clock subtree 级 delta update：新增/删除 buffer、移动 sink、改 buffer cell、局部重连。
3. 将 iCTS evaluation 与 iSTA clock timing 结果关联，验证 full CTS 与 EDADB ClockTreeView 的一致性。
4. 为 stage-aware ML dataset 记录 CTS action、tree topology、QoR delta。

## 2. 本地代码证据

### 2.1 iCTS 外部入口

- `src/platform/tool_manager/tool_api/icts_io/icts_io.cpp:33`
  - `CtsIO::runCTS()` 初始化 `CTSAPI` 并调用 `runCTS()`。
- `src/platform/tool_manager/tool_api/icts_io/icts_io.cpp:53`
  - `reportCTS()` 调用 `CTSAPIInst.report(path)`。
- `src/platform/tool_manager/tool_api/icts_io/icts_io.cpp:170`
  - `getTreeData()` 从 `staInst->getClockTree()` 包装 GUI tree。
- `src/platform/tool_manager/tool_api/icts_io/icts_io.cpp:181`
  - `wrapTree()` 把 `StaClockTree` root/child node/arc 包装成 `CtsTreeNodeMap`。

### 2.2 CTSAPI 主流程

- `src/operation/iCTS/api/CTSAPI.cc:74`
  - `runCTS()` 顺序执行 `readData()`、`routing()`、`evaluate()`、`writeGDS()`。
- `src/operation/iCTS/api/CTSAPI.cc:189`
  - `init()` 读取 config，创建 `CtsDesign`、`CtsDBWrapper`、report、lib、evaluator、model_factory，并初始化 TimingPropagator。
- `src/operation/iCTS/api/CTSAPI.cc:220`
  - `readData()` 读取 clock net names，然后调用 `_db_wrapper->read()`。
- `src/operation/iCTS/api/CTSAPI.cc:244`
  - `routing()` 创建 Router，执行 `init()`、`build()`、`update()`。
- `src/operation/iCTS/api/CTSAPI.cc:259`
  - `evaluate()` 初始化 Evaluator 并计算 CTS 结果。
- `src/operation/iCTS/api/CTSAPI.cc:399`
  - `convertDBToTimingEngine()` 重置 timing netlist/graph，从 DB adapter 转换并 build graph。
- `src/operation/iCTS/api/CTSAPI.cc:409`
  - `reportTiming()` 调用 `updateTiming()` 和 `reportTiming()`。
- `src/operation/iCTS/api/CTSAPI.cc:789`
  - `buildRCTree(eval_nets)` 为 CTS evaluation 构建 RC tree。
- `src/operation/iCTS/api/CTSAPI.cc:796`
  - `buildRCTree(eval_net)` 按 solver tree parent-child 关系生成 RC resistor/cap。

### 2.3 iDB wrapper 和 CTS object

- `src/operation/iCTS/source/data_manager/io/CtsDBWrapper.cc:35`
  - `read()` 遍历 clock net，转换 iDB net/inst/pin 到 CTS design。
- `src/operation/iCTS/source/data_manager/io/CtsDBWrapper.cc:92`
  - `makeInstance()` 创建 iDB buffer instance 并转换为 CTS instance。
- `src/operation/iCTS/source/data_manager/io/CtsDBWrapper.cc:105`
  - `makeNet()` 创建 clock net。
- `src/operation/iCTS/source/data_manager/io/CtsDBWrapper.cc:136`
  - `idbToCts(IdbInstance*)` 保存 instance name、cell master、location、type。
- `src/operation/iCTS/source/data_manager/io/CtsDBWrapper.cc:168`
  - `idbToCts(IdbPin*)` 保存 pin name、type、location、io flag。
- `src/operation/iCTS/source/data_manager/database/CtsNet.hh:32`
  - `CtsNet` 保存 net_name、pins、signal_wires、is_clock_routed。
- `src/operation/iCTS/source/data_manager/database/CtsInstance.hh:40`
  - `CtsInstance` 保存 name、cell_master、pin_list、type、location、level、virtual flag。
- `src/operation/iCTS/source/data_manager/database/CtsPin.hh:35`
  - `CtsPin` 保存 pin_name、pin_type、location、instance、net、io flag。

### 2.4 Router 和 solver tree

- `src/operation/iCTS/source/module/router/Router.cc:25`
  - `Router::init()` 记录 clock unit RC，并收集 clocks 和 instances。
- `src/operation/iCTS/source/module/router/Router.cc:46`
  - `Router::build()` 对每个 clock net 执行 `routing(clk_net)` 并标记 routed。
- `src/operation/iCTS/source/module/router/Router.cc:67`
  - `Router::update()` 把 solver net 结果同步到 CTS design、iDB 和 STA。
- `src/operation/iCTS/source/module/router/Router.cc:97`
  - `routing()` 创建 `Solver(net_name, driver, pins)` 并运行，得到 solver nets。
- `src/operation/iCTS/source/module/router/Router.cc:143`
  - `synthesisPin()` 为新 buffer pin 创建 CTS/iDB instance 和 pins。
- `src/operation/iCTS/source/module/router/Router.cc:166`
  - `synthesisNet()` 创建/连接 clock net，并沿 solver tree 生成 `CtsSignalWire`。
- `src/operation/iCTS/source/solver/database/Net.hh:28`
  - solver `Net` 保存 name、driver_pin、load_pins。
- `src/operation/iCTS/source/solver/database/Pin.hh:28`
  - solver `Pin` 继承 `Node`，记录 pin type、inst、net。
- `src/operation/iCTS/source/solver/database/Inst.hh:31`
  - solver `Inst` 保存 name、location、type、cell_master、insert_delay、driver/load pins。

### 2.5 Evaluation 和 timing report

- `src/operation/iCTS/source/module/evaluator/Evaluator.cc:24`
  - `Evaluator::init()` 设置 propagated clock，并初始化 level / transfer data。
- `src/operation/iCTS/source/module/evaluator/Evaluator.cc:31`
  - `calcInfo()` 计算 wirelength、cell distribution、cell stats、net level、path buffer stats。
- `src/operation/iCTS/source/module/evaluator/Evaluator.cc:44`
  - `calcWL()` 分 top/trunk/leaf 统计 clock wirelength 和 HPWL。
- `src/operation/iCTS/source/module/evaluator/Evaluator.cc:75`
  - `calcCellDist()` 统计新增 cell master 分布。
- `src/operation/iCTS/source/module/evaluator/Evaluator.cc:92`
  - `calcNetLevel()` 统计新增 net level。
- `src/operation/iCTS/source/module/evaluator/EvalNet.hh:80`
  - `netType()` 区分 top/trunk/leaf。
- `src/operation/iCTS/source/module/evaluator/EvalNet.hh:107`
  - `getHPWL()` 计算 clock net HPWL。
- `src/operation/iCTS/api/CTSAPI.cc:1152`
  - `outputSummary()` 读取 per-clock setup/hold WNS/TNS 和 suggest frequency。

### 2.6 关键观察

iCTS 的 EDADB 切入点非常自然：

```text
clock net
  -> solver tree nodes / inserted buffers
  -> CtsSignalWire segments
  -> RC/timing evaluation
  -> skew/cap/fanout/level/cell distribution metrics
```

这条链路比普通 DEF snapshot 更有研究价值，因为它记录了 CTS 的决策和多阶段派生数据。

## 3. EDADB View Schema 草案

### 3.1 基础对象表

| 表 | 主键 | 字段 | 作用 |
| --- | --- | --- | --- |
| `CtsRun` | `run_id` | design, config_hash, lib_hash, sdc_hash, timestamp | 记录一次 CTS run。 |
| `CtsClock` | `(run_id, clock_name)` | clock_net_name, period_ns, propagated | 保存 clock/net 关系。 |
| `CtsInst` | `(run_id, inst_name)` | cell_master, type, x,y, level, is_virtual, is_newly | 保存 sink/buffer/mux/virtual instance。 |
| `CtsPin` | `(run_id, pin_full_name)` | inst_name, net_name, pin_type, x,y, is_io | 保存 CTS pin。 |
| `CtsNet` | `(run_id, net_name)` | clock_name, net_type, is_clock_routed, driver_pin, fanout | 保存 clock net summary。 |
| `CtsNetPin` | `(run_id, net_name, pin_full_name)` | pin_role | 保存 net-pin adjacency。 |

### 3.2 Clock tree / wire / RC 表

| 表 | 主键 | 字段 | 作用 |
| --- | --- | --- | --- |
| `CtsTreeNode` | `(run_id, clock_name, node_name)` | inst_name, pin_name, x,y, node_type, level, leaf_count | 保存 clock tree node。 |
| `CtsTreeEdge` | `(run_id, clock_name, parent_node, child_node)` | net_name, length, layer_pattern, res, cap | 保存 parent-child tree edge。 |
| `CtsSignalWire` | `(run_id, net_name, wire_id)` | start_name,x,y, end_name,x,y, layer_pattern | 保存 synthesized wire。 |
| `CtsRcNode` | `(run_id, net_name, node_name)` | cap, delay | 保存 CTS evaluation RC node。 |
| `CtsRcEdge` | `(run_id, net_name, from_node, to_node)` | res, cap, length | 保存 CTS evaluation RC edge。 |

### 3.3 Metric / ECO / provenance 表

| 表 | 主键 | 字段 | 作用 |
| --- | --- | --- | --- |
| `CtsClockMetric` | `(run_id, clock_name)` | setup_wns, setup_tns, hold_wns, hold_tns, skew, suggest_freq | 保存 per-clock timing metric。 |
| `CtsTreeMetric` | `(run_id, clock_name)` | buffer_count, buffer_area, max_level, min_path_buf, max_path_buf, total_wl, max_wl | 保存 tree summary。 |
| `CtsConstraintViolation` | `violation_id` | run_id, net_name, kind, value, limit, object_name | 保存 skew/fanout/cap/length/transition 违例。 |
| `CtsAction` | `action_id` | run_id, action_type, object_name, reason | 保存 insert buffer、resize、reconnect、skew fix 等 action。 |
| `CtsDirtySubtree` | `(run_id, clock_name, root_node)` | action_id, reason | 保存 ECO 后受影响子树。 |
| `CtsProvenance` | `prov_id` | run_id, metric_name, dependency_kind, dependency_key | 保存 metric 与 tree/buffer/net/path 的依赖。 |

### 3.4 最小可实现版本

第一版先做：

- `CtsClock`
- `CtsInst`
- `CtsPin`
- `CtsNet`
- `CtsSignalWire`
- `CtsTreeMetric`
- `CtsClockMetric`

第二版再补：

- `CtsTreeNode`
- `CtsTreeEdge`
- `CtsConstraintViolation`
- `CtsAction`
- `CtsDirtySubtree`
- `CtsProvenance`

## 4. 增量算法草案

### 4.1 Full Build

```text
1. 调用 CTSAPI.readData() 从 iDB 生成 CtsDesign。
2. 调用 Router.build()/update() 生成 solver nets、buffer instances 和 signal wires。
3. 抽取 CtsClock/CtsInst/CtsPin/CtsNet/CtsSignalWire。
4. 调用 Evaluator 和 TimingEngine 计算 wirelength、skew、WNS/TNS、buffer stats。
5. 写入 metric tables。
6. 用 CtsIO tree data / CTS report / TimingEngine query 做一致性验证。
```

### 4.2 CTS ECO Delta

输入：

- 新增/删除 buffer；
- 修改 buffer cell；
- 移动 sink / clock gate；
- 修改 clock net topology；
- routing layer/RC 参数变化；
- skew constraint 变化。

流程：

```text
1. 从 action 生成 CtsAction。
2. 根据 action object 找 affected clock net / subtree root。
3. 标记 CtsDirtySubtree。
4. 只重建 affected subtree 的 CtsTreeNode/CtsTreeEdge/CtsSignalWire。
5. 只更新 affected net 的 RC/timing metric。
6. 合并 unaffected tree + affected new tree。
7. 与 full CTS rebuild / full evaluation 对比。
```

### 4.3 Correctness Invariant

必须满足：

```text
FullCTS(new_design).clock_tree == IncrementalClockTreeView(old_db, cts_delta).clock_tree
FullCTS(new_design).clock_metrics == IncrementalClockTreeView(old_db, cts_delta).clock_metrics
FullCTS(new_design).constraints == IncrementalClockTreeView(old_db, cts_delta).constraints
```

初期可先验证：

- inserted buffer count；
- clock signal wire set；
- per-clock WNS/TNS；
- total/max clock wirelength；
- path buffer count / max tree level。

## 5. 实验设计

### 5.1 Baseline

| Baseline | 含义 |
| --- | --- |
| Full iCTS | 当前 `runCTS()` 完整 read/routing/evaluate。 |
| Full ClockTreeView | 从 full iCTS 抽取 EDADB view。 |
| Incremental ClockTreeView | 只更新 dirty subtree 和 affected metrics。 |
| CtsIO tree file | 当前 GUI/file tree data 路径，作为对照。 |

### 5.2 Workloads

| Workload | 变更类型 | 预期 dirty 范围 |
| --- | --- | --- |
| W1 | 新增一个 sink | 单 clock net / nearest subtree。 |
| W2 | 移动一个 sink | sink 到 root path 或局部 subtree。 |
| W3 | 修改 skew_bound | 可能触发局部 buffer insertion/resizing。 |
| W4 | 修改 buffer type set | affected buffer candidates 和 subtree。 |
| W5 | 多时钟新增/删除一个 clock net | 单 clock tree 或跨 clock balance。 |
| W6 | routing layer RC 参数变化 | tree metric 和 timing metric 全局/分层更新。 |

### 5.3 Metrics

Correctness：

- clock tree node/edge match；
- signal wire set match；
- inserted buffer count match；
- per-clock WNS/TNS match；
- skew/cap/fanout/length violation match；
- GUI tree node leaf_count match。

Performance：

- readData time；
- routing/build time；
- evaluation time；
- dirty subtree node count；
- updated signal wire count；
- EDADB query/update time；
- storage overhead。

Research quality：

- subtree dirty ratio vs speedup；
- multi-clock balance reproducibility；
- buffer action provenance coverage；
- ML feature export completeness。

## 6. 可能的论文定位

### 6.1 EDA 会议版本

目标：

- `DAC` / `ICCAD`

叙事：

```text
Clock tree synthesis makes many topology and buffering decisions, but most open-source flows only keep final DEF/report artifacts.
We propose a persistent ClockTreeView that records CTS topology, buffer actions, RC/timing estimates, and constraint provenance for local ECO repair and reproducible multi-clock analysis.
```

核心结果：

- ClockTreeView 与 full iCTS/evaluator 一致；
- ECO workload 下减少局部修复和重评估成本；
- 多时钟 skew/timing 变化可以追溯到 tree/action。

### 6.2 AI/EDA 交叉版本

目标：

- `DAC` / `ICCAD` AI & Design track。

叙事：

```text
CTS generates structured decision traces: clustering, buffer insertion, topology choice, and skew repair.
EDADB captures these traces as graph data for clock-tree QoR prediction and action recommendation.
```

可做任务：

- subtree skew violation prediction；
- buffer type/action recommendation；
- clock tree topology quality prediction；
- ECO impact prediction for moved sink / changed clock constraint。

### 6.3 Database 会议版本

目标：

- `SIGMOD` / `VLDB` / `ICDE`

叙事：

```text
Clock trees are dynamic rooted graphs with geometric embedding, timing side structures, and constraint metrics.
We study incremental view maintenance for rooted spatial-timing trees under local topology and parameter updates.
```

风险：

- 单独 CTS 可能过专；数据库会版本需要和 iPL/iRT/iSTA 的 spatial-graph view 统一。

## 7. 实施阶段

### Phase 0：抽取 final ClockTreeView

- 跑 full iCTS。
- 保存 CtsClock/CtsInst/CtsNet/CtsSignalWire/CtsTreeMetric。
- 与 CTS report 和 `CtsIO::getTreeData()` 对齐。

### Phase 1：tree node/edge view

- 从 solver net 或 STA clock tree 抽取 node/edge。
- 保存 leaf_count、level、parent-child。
- 与 GUI tree data 对齐。

### Phase 2：metric provenance

- 关联 WNS/TNS/skew/wirelength 到 clock net、tree edge、buffer action。
- 验证 metric 与 Evaluator/TimingEngine 一致。

### Phase 3：dirty subtree update

- 构造 moved sink / buffer resize / skew bound change。
- 只更新 dirty subtree。
- 与 full rebuild 对比。

### Phase 4：ML/agent 数据导出

- 导出 clock tree graph、action sequence、constraint violation、QoR delta。
- 做 buffer/action prediction baseline。

## 8. 需要进一步核验的问题

- solver `Net/Pin/Inst/Node` 和 final `CtsNet/CtsSignalWire` 的 stable id 对应。
- `CtsIO::wrapTree()` 的 STA clock tree 与 CTS solver tree 是否一一对应。
- iCTS 是否已有局部 rerun/subtree repair API，还是需要新增最小接口。
- 多时钟 balance 的具体数据结构和跨 clock provenance。
- skew/cap/fanout/transition violation 的统一抽取 API。
- ML model 分支当前是否稳定可用，是否适合作为 AI/EDA baseline。
