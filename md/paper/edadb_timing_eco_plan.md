# EDADB + iSTA/iTO Timing ECO / Affected Cone 实验计划

本文是 `EDADB + iEDA` 研究路线中 `EDA-4 / DB-1 / AI-4` 的细化计划。目标是把 STA/TO 相关的 netlist graph、RC tree、timing metric、优化动作和 affected cone 做成 EDADB view，使 buffer insertion、resize、move instance、routing update 后可以复用已有时序状态，并为 ECO 影响预测和调试提供可查询数据。

## 1. 研究问题

### 1.1 核心问题

iSTA/iTO 已经有 TimingEngine、TimingIDBAdapter、RC tree、incremental timing queue 和 buffer/resize 接口。问题是：

- timing graph、RC tree、critical path、slack 结果主要是内存状态；
- iTO 每次优化后依赖 `updateTiming()`，缺少持久化 affected cone 和优化 provenance；
- routing update 后 RC tree 会重建/更新，但 route object 到 timing endpoint 的依赖没有统一记录；
- 工具间传递的是结果和调用，而不是可复验的 timing view。

研究目标：

```text
Can a persistent timing-provenance database maintain affected cones, RC updates, and optimization actions across ECO steps while preserving full STA consistency?
```

### 1.2 预期贡献

1. 定义 timing graph、RC tree、critical path 和 optimization action 的持久化 view。
2. 设计 net/inst/pin 变化到 affected timing cone 的增量传播算法。
3. 在 iTO buffer/resize/hold/setup/DRV 优化过程中记录 action provenance。
4. 验证 EDADB incremental TimingView 与 full `buildGraph + updateTiming` 的 WNS/TNS/path 一致性。

## 2. 本地代码证据

### 2.1 STA 外部入口

- `src/platform/tool_manager/tool_api/ista_io/ista_io.cpp:268`
  - `StaIO::reportTiming()` 依次调用 `timing_engine->buildGraph()`、`updateTiming()`、`reportTiming()`。
- `src/platform/tool_manager/tool_api/ista_io/ista_io.h:58`
  - STA tool API 暴露 `buildGraph()`。
- `src/platform/tool_manager/tool_api/ista_io/ista_io.h:60`
  - STA tool API 暴露 `updateTiming()`。
- `src/operation/iSTA/api/TimingEngine.cc:135`
  - `setDefDesignBuilder()` 使用 `TimingIDBAdapter` 从 iDB 构建 timing netlist。

### 2.2 TimingEngine RC tree 和增量能力

- `src/operation/iSTA/api/TimingEngine.cc:359`
  - `initRcTree(Net*)` 创建 RC net 并调用 `updateRCTreeInfo(net)`。
- `src/operation/iSTA/api/TimingEngine.cc:370`
  - `initRcTree()` 遍历所有 net 初始化 RC tree。
- `src/operation/iSTA/api/TimingEngine.cc:387`
  - `resetRcTree(Net*)` 清空某个 net 的 RC net。
- `src/operation/iSTA/api/TimingEngine.cc:398`
  - `makeOrFindRCTreeNode(Net*, id)` 创建或查找 internal RC node。
- `src/operation/iSTA/api/TimingEngine.cc:429`
  - `makeOrFindRCTreeNode(DesignObject*)` 创建或查找 pin/port RC node。
- `src/operation/iSTA/api/TimingEngine.cc:494`
  - `incrCap()` 可对 RC node 增量加 cap 或设置 cap。
- `src/operation/iSTA/api/TimingEngine.cc:505`
  - `makeResistor()` 在 RC tree 中插入双向 resistor edge。
- `src/operation/iSTA/api/TimingEngine.cc:563`
  - `updateRCTreeInfo(Net*)` 更新 RC tree 信息、检查 loop、更新 RC timing。
- `src/operation/iSTA/api/TimingEngine.cc:682`
  - `incrUpdateTiming()` 使用 forward/backward queue 做增量 timing propagation。

### 2.3 Routing 到 timing 的更新路径

- `src/operation/iRT/interface/RTInterface.cpp:1696`
  - `updateTimingAndPower()` 从 routing segment 和 real pin coord 构建 RC 数据。
- `src/operation/iRT/interface/RTInterface.cpp:1726`
  - 初始化 TimingEngine：读 Liberty、用 TimingIDBAdapter 转换 DB、读 SDC、buildGraph。
- `src/operation/iRT/interface/RTInterface.cpp:1866`
  - 获取 TimingEngine 和 STA netlist。
- `src/operation/iRT/interface/RTInterface.cpp:1870`
  - 对每个 net `resetRcTree(ista_net)`，再根据 routing segment 创建 RC segment。
- `src/operation/iRT/interface/RTInterface.cpp:1882`
  - 使用 TimingIDBAdapter 从 layer/length 获取 cap。
- `src/operation/iRT/interface/RTInterface.cpp:1884`
  - 使用 TimingIDBAdapter 从 layer/length 获取 res。
- `src/operation/iRT/interface/RTInterface.cpp:1894`
  - 对每个 net 调用 `updateRCTreeInfo(ista_net)`。
- `src/operation/iRT/interface/RTInterface.cpp:1900`
  - 全部 net RC 更新后调用 `updateTiming()` 和 `reportTiming()`。

### 2.4 iTO 优化路径

- `src/platform/tool_manager/tool_api/ito_io/ito_io.cpp:25`
  - `runTO()` 初始化 iTO、reset lib/sdc、initEngine、runTO。
- `src/platform/tool_manager/tool_api/ito_io/ito_io.cpp:51`
  - `runTODrv()` 调用 `ToApiInst.optimizeDrv()`。
- `src/platform/tool_manager/tool_api/ito_io/ito_io.cpp:79`
  - `runTODrvSpecialNet()` 可针对指定 net 优化。
- `src/operation/iTO/api/ToApi.cpp:43`
  - `initEngine()` 调用 timingEngine 初始化。
- `src/operation/iTO/api/ToApi.cpp:51`
  - `runTO()` 调用 iTO 完整流程。
- `src/operation/iTO/api/ToApi.cpp:56`
  - `optimizeDrv()` 调用 DRV 优化。
- `src/operation/iTO/api/ToApi.cpp:61`
  - `optimizeDrvSpecialNet(net_name)` 针对单 net 优化。
- `src/operation/iTO/api/ToApi.cpp:66`
  - `optimizeSetup()` 调用 setup 优化。
- `src/operation/iTO/api/ToApi.cpp:76`
  - `optimizeHold()` 调用 hold 优化。
- `src/operation/iTO/source/module/fix_drv/ViolationOptimizer_buffers.cpp:74`
  - DRV buffer insertion 创建新 buffer instance 和新 net。
- `src/operation/iTO/source/module/fix_drv/ViolationOptimizer_buffers.cpp:114`
  - 插入 buffer 后调用 `timingEngine->get_sta_engine()->insertBuffer(...)` 更新 STA graph。
- `src/operation/iTO/source/module/fix_setup/SetupOptimizer_init.cpp:43`
  - setup 初始化/估算寄生后调用 `updateTiming()`。
- `src/operation/iTO/source/module/fix_hold/HoldOptimizer_process.cpp:44`
  - hold 优化插 buffer 后估算寄生并调用 `updateTiming()`。

### 2.5 TimingEngine netlist ECO 接口

- `src/operation/iSTA/api/TimingEngine.cc:889`
  - `insertBuffer(instance_name)` 会在 timing graph 中构建新 instance，并重建相关 net arcs。
- `src/operation/iSTA/api/TimingEngine.cc:961`
  - `removeBuffer(instance_name)` 会移除 buffer 相关 vertex/arc，并修改 load arc 源点。
- `src/operation/iSTA/api/TimingEngine.cc:1026`
  - `repowerInstance(instance_name, cell_name)` 修改 instance liberty cell 和相关 inst arc。
- `src/operation/iSTA/api/TimingEngine.cc:1072`
  - `moveInstance(instance_name, update_level, prop_type)` 根据 moved instance pins 向增量传播队列插入 affected vertices。

## 3. EDADB View Schema 草案

### 3.1 Timing graph 基础表

| 表 | 主键 | 字段 | 作用 |
| --- | --- | --- | --- |
| `TimingRun` | `run_id` | design, stage, lib_hash, sdc_hash, route_version, timestamp | 记录一次 STA/TO run。 |
| `TimingInst` | `(run_id, inst_name)` | cell_name, x,y, status, is_buffer | 保存 timing instance。 |
| `TimingPin` | `(run_id, pin_name)` | inst_name, net_name, direction, is_clock, vertex_id | 保存 pin/port 到 vertex 映射。 |
| `TimingNet` | `(run_id, net_name)` | driver_pin, load_count, is_clock, route_version | 保存 timing net summary。 |
| `TimingArc` | `(run_id, arc_id)` | src_pin, snk_pin, arc_type, lib_arc_id, net_name | 保存 timing graph arc。 |
| `TimingClock` | `(run_id, clock_name)` | period_ns, propagated | 保存 clock summary。 |

### 3.2 RC / path / metric 表

| 表 | 主键 | 字段 | 作用 |
| --- | --- | --- | --- |
| `RcNode` | `(run_id, net_name, node_name)` | pin_name, fake_id, x,y,layer, cap, delay | 保存 RC node。 |
| `RcEdge` | `(run_id, net_name, from_node, to_node)` | res, length, layer_idx, route_seg_id | 保存 RC resistor edge。 |
| `TimingEndpoint` | `(run_id, pin_name, mode, trans)` | at, rt, slack, slew | 保存 endpoint timing。 |
| `TimingPath` | `(run_id, path_id)` | clock_name, mode, trans, start_pin, end_pin, slack | 保存 critical path summary。 |
| `TimingPathArc` | `(run_id, path_id, arc_order)` | arc_id, delay, slew | 保存 path 组成。 |
| `TimingMetric` | `(run_id, clock_name, mode)` | WNS, TNS, suggest_freq | 保存 clock-level QoR。 |

### 3.3 ECO / provenance 表

| 表 | 主键 | 字段 | 作用 |
| --- | --- | --- | --- |
| `TimingECOAction` | `action_id` | run_id, action_type, object_kind, object_name, reason | 记录 buffer/resize/move/route update。 |
| `TimingDirtyNet` | `(run_id, net_name)` | action_id, reason | 记录 RC/timing 受影响 net。 |
| `TimingDirtyVertex` | `(run_id, vertex_id)` | action_id, prop_type, level | 记录增量传播起点。 |
| `TimingAffectedCone` | `(run_id, action_id, vertex_id)` | direction, level, is_endpoint | 记录 affected timing cone。 |
| `TimingProvenance` | `prov_id` | run_id, metric/path/object, dependency_kind, dependency_key | 保存 WNS/TNS/path 与 inst/net/route 的依赖。 |

### 3.4 最小可实现版本

第一版先做：

- `TimingNet`
- `RcNode`
- `RcEdge`
- `TimingMetric`
- `TimingDirtyNet`
- `TimingECOAction`

第二版再补：

- `TimingPin`
- `TimingArc`
- `TimingEndpoint`
- `TimingPath`
- `TimingAffectedCone`
- `TimingProvenance`

## 4. 增量算法草案

### 4.1 Full Build

```text
1. 调用 TimingIDBAdapter 从 iDB 构建 timing netlist。
2. buildGraph + initRcTree + updateTiming。
3. 抽取 TimingNet、TimingPin、TimingArc、RcNode、RcEdge。
4. 抽取 WNS/TNS、critical paths、endpoint slack。
5. 写入 TimingMetric 和 TimingPath。
6. 用 reportTiming 输出与 EDADB query 结果对齐。
```

### 4.2 Route Delta -> Timing Delta

输入：

- changed route segment；
- changed via；
- changed net route；
- iRT dirty net / dirty route object。

流程：

```text
1. 从 iRT RouteView 得到 changed net list。
2. 对 changed net resetRcTree。
3. 根据 RtRouteSeg/RtPatch 重建 RcNode/RcEdge。
4. updateRCTreeInfo(changed_net)。
5. 记录 TimingDirtyNet。
6. 调用 incrUpdateTiming 或局部 updateTiming。
7. 抽取 changed endpoints / WNS/TNS / path delta。
8. 与 full updateTiming 结果比较。
```

### 4.3 iTO ECO Action -> Timing Delta

输入：

- buffer insertion；
- resize/repower；
- move instance；
- net reconnect；
- hold/setup/DRV 优化动作。

流程：

```text
1. 在 iTO action 发生时写 TimingECOAction。
2. 对 insertBuffer/removeBuffer/repowerInstance/moveInstance 记录 dirty inst/pin/net。
3. 从 dirty pin/net 找 affected vertices。
4. 记录 TimingDirtyVertex 和 TimingAffectedCone。
5. 调用 TimingEngine 增量或全量 timing update。
6. 抽取 TimingMetric delta 和 changed critical path。
7. 与 full rebuild graph + updateTiming 对比。
```

### 4.4 Correctness Invariant

必须满足：

```text
FullSTA(new_design).WNS/TNS == IncrementalTimingView(old_db, eco_delta).WNS/TNS
FullSTA(new_design).endpoint_slack == IncrementalTimingView(old_db, eco_delta).endpoint_slack
FullSTA(new_design).critical_paths == IncrementalTimingView(old_db, eco_delta).critical_paths
```

初期可以只要求：

- WNS/TNS exact 或在数值容忍范围内一致；
- top-k path 的 start/end/slack 一致；
- changed net 的 RC node/edge 与 full RC tree 一致。

## 5. 实验设计

### 5.1 Baseline

| Baseline | 含义 |
| --- | --- |
| Full STA | 当前 `buildGraph + updateTiming + reportTiming`。 |
| Full TimingView | 从 full STA 抽取 EDADB timing view。 |
| Incremental TimingView | 只更新 dirty net/vertex/cone。 |
| iTO current | 当前 iTO 插 buffer/resize 后直接 updateTiming。 |

### 5.2 Workloads

| Workload | 变更类型 | 预期 dirty 范围 |
| --- | --- | --- |
| W1 | 单 net route segment change | changed net RC tree 和其 timing fanout。 |
| W2 | reroute 0.1% / 1% / 5% nets | 多个 RC tree 和 affected cone。 |
| W3 | insert one buffer | 新 buffer instance、新 net、原 net load split。 |
| W4 | resize/repower one instance | instance arcs 和相邻 nets。 |
| W5 | move one instance | connected nets 的 RC 和 pin vertices。 |
| W6 | iTO full DRV/setup/hold iteration | 多个 action 的 cumulative delta。 |

### 5.3 Metrics

Correctness：

- WNS/TNS match；
- per-clock WNS/TNS match；
- endpoint slack match；
- top-k critical path match；
- changed net RC node/edge match；
- affected cone coverage no false negative。

Performance：

- graph build time；
- RC rebuild time；
- timing update time；
- dirty net count；
- dirty vertex count；
- affected endpoint count；
- EDADB query/update time；
- storage overhead。

Research quality：

- dirty ratio vs speedup；
- false positive cone size；
- action provenance usefulness；
- route/placement/timing cross-stage trace length。

## 6. 可能的论文定位

### 6.1 EDA 会议版本

目标：

- `DAC` / `ICCAD`

叙事：

```text
Timing ECO optimization repeatedly changes a small set of nets and instances, but timing state and action provenance are not persistently queryable.
We propose a database-backed TimingView that records RC/tree/graph deltas and affected cones across routing and timing optimization.
```

核心结果：

- full STA 与 incremental TimingView 一致；
- iTO ECO workload 下减少重复 RC/timing 更新；
- 可以解释 WNS/TNS/path 改变来自哪些 route/inst/action。

### 6.2 AI/EDA 交叉版本

目标：

- `DAC` / `ICCAD` AI & Design track。

叙事：

```text
ECO optimization generates rich sequential decisions, but most flows discard action provenance.
EDADB records timing ECO actions, affected cones, and QoR deltas as training data for ECO impact prediction and action recommendation.
```

核心结果：

- action -> affected cone -> QoR delta 数据集；
- 预测 buffer/resize/route update 对 WNS/TNS 的影响；
- agent 可以基于历史 provenance 解释和建议 ECO。

### 6.3 Database 会议版本

目标：

- `SIGMOD` / `VLDB` / `ICDE`

叙事：

```text
Timing ECO is an incremental graph computation with RC-tree side structures and path-based metrics.
We study database-managed affected-cone maintenance and provenance for dynamic timing graphs.
```

风险：

- 数据库会版本需要抽象为 dynamic graph + side-structure view maintenance，而不能只写 iSTA adapter。

## 7. 实施阶段

### Phase 0：只抽取 TimingMetric 和 RC summary

- 跑 full STA。
- 保存 per-clock WNS/TNS 和每个 net 的 RC node/edge count。
- 与 reportTiming / TimingEngine query 对齐。

### Phase 1：changed net RC delta

- 从 iRT RouteView 读 changed net。
- 重建该 net RC tree。
- 保存 `TimingDirtyNet`、`RcNode`、`RcEdge`。
- 与 full RC tree 对齐。

### Phase 2：iTO action provenance

- 在 buffer insertion、resize、move instance 时记录 `TimingECOAction`。
- 记录新/旧 net、inst、pin。
- 关联 action 和 TimingMetric delta。

### Phase 3：affected cone

- 从 dirty vertices 做 forward/backward cone。
- 保存 `TimingAffectedCone`。
- 验证 cone 覆盖 changed endpoints。

### Phase 4：ML/agent 数据导出

- 导出 action sequence、cone graph、path delta、QoR delta。
- 做 ECO impact prediction 或 action ranking baseline。

## 8. 需要进一步核验的问题

- TimingEngine 当前 `updateTiming()` 与 `incrUpdateTiming()` 的调用边界。
- iTO 中哪些 action 已经调用 TimingEngine 的 graph ECO API，哪些只改 iDB/STA adapter。
- `moveInstance()` 的 affected queue 是否已覆盖 RC tree 改动。
- route-derived RC tree 与 estimated parasitics 的一致性。
- critical path 抽取 API 的稳定性和可序列化字段。
- full rebuild graph 与 incremental graph 在 buffer insertion 后的 vertex/arc id 稳定性。
