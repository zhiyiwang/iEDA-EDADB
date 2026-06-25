# EDADB + iRT Routing ECO / Local Reroute 实验计划

本文是 `EDADB + iEDA` 研究路线中 `EDA-3 / DB-2 / DB-3` 的细化计划。目标不是重写 iRT router，而是把 iRT 当前内存中的 GCellMap、routing result、access point、patch、violation 等局部状态持久化为 EDADB view，支持 routing ECO、局部 reroute、局部 violation recheck 和跨阶段 provenance。

## 1. 研究问题

### 1.1 核心问题

iRT 当前已经把 routing state 分布到 `GCellMap` 中，并提供 `ChangeType::kAdd/kDel` 更新接口。问题是：

- GCellMap 是一次运行中的临时内存结构；
- routing result、patch、violation、access point 的归属和 dirty 范围没有持久化；
- ECO 或局部 reroute 后，缺少可复用的 affected gcell / affected net / affected violation view；
- `clearDef()` 会清理 DEF 中已有 routing 结果，说明 routing result 生命周期仍偏“全量重建”。

研究目标：

```text
Can a persistent routing-state database preserve GCell-level route/provenance views and reduce reroute/violation-check cost under local routing ECO updates?
```

### 1.2 预期贡献

1. 定义 iRT routing workload 的 GCell/tile-level persistent view。
2. 设计 route segment / via / patch / violation 的 delta update 和 dirty gcell 传播算法。
3. 在 iRT 上实现 full route view 与 incremental route view 的一致性验证。
4. 为 iDRC、iSTA/iTO、ML dataset 提供 routing stage 的可查询中间状态。

## 2. 本地代码证据

### 2.1 iRT 主流程

- `src/operation/iRT/interface/RTInterface.cpp:95`
  - `runEGR()` 运行 SupplyAnalyzer 和 EarlyRouter。
- `src/operation/iRT/interface/RTInterface.cpp:117`
  - `runRT()` 顺序运行 PinAccessor、SupplyAnalyzer、TopologyGenerator、LayerAssigner、SpaceRouter、TrackAssigner、DetailedRouter、ViolationReporter。
- `src/operation/iRT/interface/RTInterface.cpp:190`
  - `clearDef()` 清理 net wire、virtual、patch、部分虚拟 IO 连接和空 IO pin。
- `src/operation/iRT/interface/RTInterface.cpp:548`
  - `wrapDatabase()` 从 iDB 包装 DBInfo、DBU、die、row、layer、via master、obstacle、net。
- `src/operation/iRT/interface/RTInterface.cpp:1696`
  - `updateTimingAndPower()` 在 routing 后更新 timing/power。

### 2.2 DataManager 和 GCellMap

- `src/operation/iRT/source/data_manager/DataManager.hpp:26`
  - `DataManager` 提供 `input()` / `output()`，内部管理 `Config` 和 `Database`。
- `src/operation/iRT/source/data_manager/DataManager.hpp:31`
  - GCellMap update API 包括 fixed rect、access point、pin access result/patch、global result、detailed result/patch、violation。
- `src/operation/iRT/source/data_manager/DataManager.cpp:45`
  - `input()` 调用 `RTI.input()`、`buildConfig()`、`buildDatabase()`。
- `src/operation/iRT/source/data_manager/DataManager.cpp:51`
  - `output()` 调用 `RTI.output()` 并销毁 GCellMap。
- `src/operation/iRT/source/data_manager/DataManager.cpp:240`
  - `updateNetGlobalResultToGCellMap()` 将 global route segment 映射到经过的 gcell。
- `src/operation/iRT/source/data_manager/DataManager.cpp:265`
  - `updateNetDetailedResultToGCellMap()` 将 detailed route shape 扩展 detection distance 后映射到 gcell。
- `src/operation/iRT/source/data_manager/DataManager.cpp:293`
  - `updateNetDetailedPatchToGCellMap()` 将 patch 映射到 gcell。
- `src/operation/iRT/source/data_manager/DataManager.cpp:318`
  - `updateViolationToGCellMap()` 按 violation grid rect 更新 gcell violation set。

### 2.3 GCell 中保存的 routing state

- `src/operation/iRT/source/data_manager/advance/GCell.hpp:23`
  - fixed rect map：`is_routing -> layer_idx -> net_idx -> rect set`。
- `src/operation/iRT/source/data_manager/advance/GCell.hpp:24`
  - access point map：`net_idx -> access point set`。
- `src/operation/iRT/source/data_manager/advance/GCell.hpp:25`
  - pin access result map：`net_idx -> pin_idx -> segment set`。
- `src/operation/iRT/source/data_manager/advance/GCell.hpp:28`
  - global routing supply/demand 相关数据。
- `src/operation/iRT/source/data_manager/advance/GCell.hpp:31`
  - global routing result map。
- `src/operation/iRT/source/data_manager/advance/GCell.hpp:32`
  - detailed routing result map。
- `src/operation/iRT/source/data_manager/advance/GCell.hpp:33`
  - detailed patch map。
- `src/operation/iRT/source/data_manager/advance/GCell.hpp:34`
  - violation set。

### 2.4 Routing object 和 violation

- `src/operation/iRT/source/data_manager/advance/Net.hpp:24`
  - `Net` 保存 net_idx、net_name、connect_type、pin_list、bounding_box。
- `src/operation/iRT/source/data_manager/advance/Pin.hpp:27`
  - `Pin` 保存 pin_idx、pin_name、routing/cut shape、driven flag、access point。
- `src/operation/iRT/source/data_manager/advance/NetShape.hpp:22`
  - `NetShape` 保存 net_idx、layer rect、is_routing。
- `src/operation/iRT/source/data_manager/advance/Guide.hpp:21`
  - `Guide` 保存 layer rect 和 grid_coord。
- `src/operation/iRT/source/data_manager/advance/Violation.hpp:27`
  - `Violation` 保存 violation type、shape、routing flag、net set、required size。

### 2.5 局部 reroute 证据

- `src/operation/iRT/source/module/detailed_router/DetailedRouter.cpp:542`
  - `buildRouteViolation()` 从 `RTDM.getViolationSet(dr_box.get_box_rect())` 找当前 box violation，并从 GCellMap 删除被处理 violation。
- `src/operation/iRT/source/module/detailed_router/DetailedRouter.cpp:563`
  - `needRouting()` 根据 task list 和 route violation 决定 box 是否需要 routing。
- `src/operation/iRT/source/module/detailed_router/DetailedRouter.cpp:900`
  - `updateGraph()` 会删除当前 net 的旧 routed rect/patch，再按 task 更新 routing graph。
- `src/operation/iRT/source/module/pin_accessor/PinAccessor.cpp:2575`
  - PinAccessor upload 阶段会删除旧 pin access result/patch/violation，再添加新结果。
- `src/operation/iRT/source/module/topology_generator/TopologyGenerator.cpp:731`
  - TopologyGenerator 上传 global result，并调用 `RTDM.updateNetGlobalResultToGCellMap()`。

### 2.6 关键观察

iRT 已经具备 EDADB 需要的核心结构：

```text
route object
  -> affected GCell
  -> per-net/per-layer route state
  -> violation set
  -> add/delete update API
```

因此第一阶段不必发明新的 router 数据结构，而是把 GCellMap 的内存 update/get 语义提升为 EDADB persistent view。

## 3. EDADB View Schema 草案

### 3.1 基础表

| 表 | 主键 | 字段 | 作用 |
| --- | --- | --- | --- |
| `RtRun` | `run_id` | design, stage, config_hash, def_path, timestamp | 记录一次 EGR/RT run。 |
| `RtGCell` | `(run_id, gcell_x, gcell_y)` | real_llx,lly,urx,ury, grid_llx,lly,urx,ury | 保存 GCell 网格。 |
| `RtNet` | `(run_id, net_idx)` | net_name, connect_type, bbox | 保存 router net。 |
| `RtPin` | `(run_id, net_idx, pin_idx)` | pin_name, is_driven, access_x,y,layer | 保存 router pin 和 access point summary。 |
| `RtFixedShape` | `shape_id` | run_id, source_kind, net_idx, layer_idx, is_routing, llx,lly,urx,ury | 保存 obstacle/pin/fixed rect。 |
| `RtRouteSeg` | `seg_id` | run_id, net_idx, route_stage, first_x,y,layer, second_x,y,layer | 保存 global/detailed route segment。 |
| `RtPatch` | `patch_id` | run_id, net_idx, layer_idx, llx,lly,urx,ury | 保存 detailed patch。 |
| `RtViolation` | `violation_id` | run_id, type, layer_idx, is_routing, llx,lly,urx,ury, required_size | 保存 routing violation。 |
| `RtViolationNet` | `(violation_id, net_idx)` | run_id | 保存 violation net set。 |

### 3.2 GCell index 和 dirty 表

| 表 | 主键 | 字段 | 作用 |
| --- | --- | --- | --- |
| `RtGCellObject` | `(run_id, gcell_x, gcell_y, object_kind, object_id)` | net_idx, layer_idx, route_stage | 替代 GCellMap 中各类 set/map。 |
| `RtDirtyNet` | `(run_id, net_idx)` | reason, old_bbox, new_bbox | 记录受 ECO 或 reroute 影响的 nets。 |
| `RtDirtyGCell` | `(run_id, gcell_x, gcell_y)` | reason | 记录受 route object 变化影响的 gcells。 |
| `RtDirtyLayer` | `(run_id, gcell_x, gcell_y, layer_idx)` | reason | 细化到 layer 的 dirty region。 |
| `RtRouteProvenance` | `prov_id` | run_id, module, task_id, net_idx, gcell_x, gcell_y, object_kind, object_id | 保存 route task 和结果依赖。 |
| `RtGCellSummary` | `(run_id, gcell_x, gcell_y)` | supply, demand, overflow, violation_count, route_seg_count | 保存 congestion/violation summary。 |

### 3.3 最小可实现版本

第一版先做：

- `RtGCell`
- `RtNet`
- `RtRouteSeg`
- `RtPatch`
- `RtViolation`
- `RtGCellObject`
- `RtDirtyNet`
- `RtDirtyGCell`
- `RtGCellSummary`

第二版再补：

- `RtPin`
- `RtFixedShape`
- `RtRouteProvenance`
- module/task 级 provenance。

## 4. 增量算法草案

### 4.1 Full Build

```text
1. 复用 RTDM.buildDatabase() 生成 layer/die/gcell/net/obstacle。
2. 遍历 GCellMap，抽取 fixed shape、access point、global result、detailed result、patch、violation。
3. 写入 RtGCellObject，形成 object -> gcell 的反向索引。
4. 聚合 RtGCellSummary。
5. 用 SQL / EDADB query 验证 row count 与内存 GCellMap 一致。
```

### 4.2 Delta Update

输入：

- changed net list；
- deleted/added route segment；
- deleted/added patch；
- deleted/added via；
- changed violation；
- 或两个 route snapshot 的 diff。

流程：

```text
1. 对每个 deleted route object 查 RtGCellObject，标记 RtDirtyGCell 并删除映射。
2. 对每个 added route object 按当前 RTDM grid/detection_distance 计算 touched gcells。
3. 更新 RtRouteSeg / RtPatch / RtViolation / RtGCellObject。
4. 由 dirty gcell 反查 affected nets、violations、neighbor layers。
5. 只对 affected gcell/box 运行 reroute 或 violation recheck。
6. 合并 unaffected old route view + affected new route view。
7. 与 full route view / full violation report 对比。
```

### 4.3 Correctness Invariant

必须满足：

```text
FullRouteView(new_design).route_objects == IncrementalRouteView(old_db, route_delta).route_objects
FullRouteView(new_design).gcell_index == IncrementalRouteView(old_db, route_delta).gcell_index
FullViolationReport(new_design).violations == IncrementalViolationCheck(old_db, route_delta).violations
```

局部 reroute 必须 conservative：

- route segment 的 real shape；
- via/cut layer 的 adjacent routing layers；
- detection distance；
- DRC rule halo；
- neighbor gcells touched by expanded shape。

## 5. 实验设计

### 5.1 Baseline

| Baseline | 含义 |
| --- | --- |
| Full iRT | 当前完整 `runRT()`。 |
| Full EDADB RouteView | 从完整 GCellMap 生成 EDADB route view。 |
| Incremental EDADB RouteView | 只更新 dirty net/gcell/layer。 |
| Local Reroute Prototype | 只在 affected gcell/box 上调用 reroute/recheck。 |

### 5.2 Workloads

| Workload | 变更类型 | 预期 dirty 范围 |
| --- | --- | --- |
| W1 | 删除/新增一个 route segment | segment 覆盖的 gcell 和邻近 gcell。 |
| W2 | 修改一个 via | cut layer + adjacent routing layers。 |
| W3 | reroute 1 个 net | net bbox / route guide 附近。 |
| W4 | reroute 0.1% / 1% / 5% nets | 多个局部 route regions。 |
| W5 | placement ECO 后局部 reroute | moved inst pins + affected nets + local obstacles。 |
| W6 | blockage / special net 变化 | 对应 obstacle gcell 和受阻 nets。 |

### 5.3 Metrics

Correctness：

- route segment set exact match；
- patch set exact match；
- GCellObject index exact match；
- per-gcell overflow / demand match；
- violation set exact match；
- no missed affected net/gcell。

Performance：

- GCellMap full build time；
- EDADB full route view build time；
- dirty gcell count；
- affected net count；
- route object update time；
- violation recheck time；
- total reroute runtime；
- EDADB query/update time；
- storage overhead。

Scalability：

- net count；
- route segment count；
- layer count；
- gcell count；
- dirty net ratio；
- dirty gcell ratio。

## 6. 可能的论文定位

### 6.1 EDA 会议版本

目标：

- `DAC` / `ICCAD`

叙事：

```text
Modern routing flows repeatedly rebuild transient routing states even when ECO updates are local.
We propose a persistent GCell-level routing-state database that supports local reroute and violation recheck with correctness validation against full routing views.
```

核心结果：

- route/violation view 与 full build 一致；
- ECO workload 下减少候选 gcell、affected net 和 recheck runtime；
- routing stage 中间状态可查询、可复现、可导出。

### 6.2 Database 会议版本

目标：

- `SIGMOD` / `VLDB` / `ICDE`

叙事：

```text
Detailed routing is a hybrid spatial-graph workload with frequent local updates.
We model routing results as persistent spatial-graph views and study conservative delta propagation over gcell, net, layer, and violation provenance.
```

核心结果：

- spatial + graph + versioned delta 的统一 view model；
- no-false-negative affected region；
- 对比 generic spatial index / full scan / current in-memory map。

风险：

- 单 iRT 实现可能不足以支撑数据库会；需要抽象成通用 spatial-graph update workload，并和 iPL/iDRC 形成统一 benchmark。

## 7. 实施阶段

### Phase 0：离线抽取 RouteView

- 完整跑 iRT 后，从 GCellMap 抽取 route object。
- 写入 `RtGCellObject`、`RtRouteSeg`、`RtPatch`、`RtViolation`。
- 与 GCellMap 中的对象数量和 violation 数量对齐。

### Phase 1：Route object delta

- 为 route segment/patch/via 定义 stable id。
- 实现 object -> touched gcell 的计算。
- 增删一个 route object 后，只更新相关 `RtGCellObject`。

### Phase 2：Dirty gcell / dirty net query

- 从 changed route object 生成 `RtDirtyGCell`。
- 反查 affected nets、route objects、violations。
- 与全量扫描得到的 affected set 对比。

### Phase 3：Local violation recheck

- 用 dirty gcell 生成 check region。
- 调用 iDRC/iRT DRCEngine 只检查 affected region。
- 与 full violation report 对比。

### Phase 4：Local reroute prototype

- 对 affected net/gcell 构造 local reroute task。
- 更新 EDADB route view。
- 对比 full reroute 或当前 iRT output。

## 8. 需要进一步核验的问题

- iRT 当前各阶段 result 的 stable ownership：哪些 pointer 由 net/pin 保存，哪些由 GCellMap delete。
- `detection_distance` 与 DRC rule halo 的精确关系。
- global result 的 grid segment 和 detailed result 的 real shape 如何统一成 route object id。
- via 被表达为 layer-changing segment 还是 explicit via object。
- `clearDef()` 清理逻辑与 EDADB route snapshot 的一致性。
- local reroute 是否已有可复用 box/task API，还是需要新增最小 interface。
