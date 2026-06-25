# EDADB + iDRC 增量 DRC / Provenance Skipping 实验计划

本文是 `EDADB + iEDA` 研究路线中优先方向 `EDA-1 / DB-2` 的细化计划。目标是把 iDRC 当前内存级 box partition 和 violation checking 过程改造成可持久化、可增量维护、可查询的 EDADB view，用真实 iEDA flow 验证局部 ECO/局部 reroute 后是否能减少 DRC 检查成本。

## 1. 研究问题

### 1.1 核心问题

当前 iDRC 已经会把 layout shapes 分到 `RVBox` 网格中，再对每个 box 做 rule validation。问题是：

- box/shape/violation/provenance 都是一次性内存结构；
- 每次检查都从 iDB 重新构建 env/result shape list；
- ECO 或局部 routing 改动后，缺少持久化 dirty region 和 affected box；
- violation 结果没有记录依赖的 shape / net / box，难以做增量 invalidation。

研究目标：

```text
Can an EDA-native persistent spatial-provenance database reduce DRC rechecking cost under local physical-design updates while preserving full-check correctness?
```

### 1.2 预期贡献

1. 定义 EDA DRC workload 的持久化 spatial-provenance view。
2. 设计 dirty-object / dirty-box / affected-neighbor-box 的增量维护算法。
3. 在 iEDA/iDRC 上实现 full-check 与 incremental-check 的一致性验证。
4. 给出真实 flow 上的 runtime、candidate pair、checked box、violation-set 一致性结果。

## 2. 本地代码证据

### 2.1 iDRC 外部入口

- `src/operation/iDRC/interface/DRCInterface.cpp:83`
  - `checkDef()` 调用 `getViolationList(buildEnvShapeList(), buildResultShapeList(), {}, {})`。
- `src/operation/iDRC/interface/DRCInterface.cpp:120`
  - `getViolationList()` 把 `ids::Shape` 转成 `DRCShape`，再调用 `DRCRV.verify(...)`。
- `src/operation/iDRC/interface/DRCInterface.cpp:186`
  - `wrapDatabase()` 构建 micron DBU、die、rules、layer list、layer info。

### 2.2 shape 构建路径

- `src/operation/iDRC/interface/DRCInterface.cpp:638`
  - `buildEnvShapeList()` 从 instance obstruction、instance pin、pin via、special net、IO pin 生成环境 shapes。
- `src/operation/iDRC/interface/DRCInterface.cpp:876`
  - `buildResultShapeList()` 从 regular net wire segment、via、rect 生成 result shapes。
- `src/operation/iDRC/source/data_manager/advance/DRCShape.hpp`
  - `DRCShape` 包含 `LayerRect`、`net_idx`、`is_routing`。

### 2.3 当前 box partition 和 rule validation

- `src/operation/iDRC/source/module/rule_validator/RuleValidator.cpp:55`
  - `verify()` 依次执行 `initRVModel()`、`setRVComParam()`、`buildRVBoxList()`、`verifyRVModel()`、`buildViolationList()`。
- `src/operation/iDRC/source/module/rule_validator/RuleValidator.cpp:96`
  - `buildRVBoxList()` 根据 bounding box、`box_size = 500 * only_pitch`、`expand_size = 5 * only_pitch` 建网格，并把 env/result shapes 分配到 box。
- `src/operation/iDRC/source/module/rule_validator/RuleValidator.cpp:185`
  - `verifyRVModel()` 对每个 `RVBox` 并行检查。
- `src/operation/iDRC/source/module/rule_validator/RVBox.hpp`
  - `RVBox` 保存 box rect、env/result shape pointer list、check type、check region、violation list。
- `src/operation/iDRC/source/data_manager/advance/Violation.hpp`
  - `Violation` 包含 violation type、rect、layer、routing flag、violation net set、required size。

### 2.4 关键观察

iDRC 已经有空间分区思想，但这个分区目前只存在于 `RuleValidator::buildRVBoxList()` 的临时内存中。EDADB 可以把它提升为持久化 view：

```text
iDB shapes
  -> DRCShape facts
  -> ShapeBoxIndex
  -> RVBox facts
  -> RuleCheckProvenance
  -> Violation facts
```

这比“从头设计空间索引”风险小，因为 iDRC 已经证明 box partition 可用于 rule validation。

## 3. EDADB View Schema 草案

### 3.1 基础表

| 表 | 主键 | 字段 | 作用 |
| --- | --- | --- | --- |
| `DrcRun` | `run_id` | design, stage, def_path, git_commit, config_hash, timestamp | 记录一次 DRC run。 |
| `DrcShape` | `shape_id` | run_id, source_kind, source_name, net_idx, layer_idx, is_routing, llx,lly,urx,ury | 统一保存 env/result shapes。 |
| `DrcBox` | `box_id` | run_id, grid_x, grid_y, llx,lly,urx,ury, box_size, expand_size | 对应当前 `RVBox`。 |
| `DrcShapeBox` | `(shape_id, box_id)` | run_id, shape_role, expanded | 记录 shape 被分配到哪些 boxes。 |
| `DrcViolation` | `violation_id` | run_id, violation_type, layer_idx, is_routing, llx,lly,urx,ury, required_size | 保存 violation。 |
| `DrcViolationNet` | `(violation_id, net_idx)` | run_id | 保存 violation net set。 |

### 3.2 增量和 provenance 表

| 表 | 主键 | 字段 | 作用 |
| --- | --- | --- | --- |
| `DrcDirtyObject` | `dirty_id` | run_id, object_kind, object_name, net_idx, reason | 记录 ECO 改动对象。 |
| `DrcDirtyRegion` | `dirty_region_id` | run_id, layer_idx, is_routing, llx,lly,urx,ury | dirty object 映射出的几何区域。 |
| `DrcDirtyBox` | `(run_id, box_id)` | reason | dirty region 命中的 box。 |
| `DrcCheckProvenance` | `prov_id` | run_id, box_id, violation_type, input_shape_id, candidate_shape_id | 记录 rule check 依赖。 |
| `DrcBoxSummary` | `(run_id, box_id)` | env_shape_count, result_shape_count, candidate_count, violation_count, runtime_us | 保存性能和统计数据。 |

### 3.3 最小可实现版本

第一版不需要完整 provenance 到 candidate pair；先做：

- `DrcShape`
- `DrcBox`
- `DrcShapeBox`
- `DrcDirtyRegion`
- `DrcDirtyBox`
- `DrcViolation`
- `DrcBoxSummary`

第二版再补：

- `DrcCheckProvenance`
- per-rule candidate pair。

## 4. 增量算法草案

### 4.1 Full Build

```text
1. 调用现有 buildEnvShapeList() / buildResultShapeList()
2. 给每个 DRCShape 分配 stable shape_id
3. 复用 RuleValidator::setRVComParam() 的 box_size / expand_size
4. 按当前 buildRVBoxList() 同样逻辑生成 DrcBox 和 DrcShapeBox
5. 对所有 boxes 调用现有 verifyRVBox()
6. 保存 DrcViolation 和 DrcBoxSummary
```

### 4.2 Delta Update

输入：

- changed net list；
- changed instance list；
- changed wire segment / via / blockage；
- 或者两个 DB snapshot 的 diff。

流程：

```text
1. 从 changed objects 重建 old/new geometry bbox
2. 生成 DrcDirtyRegion
3. 用 expanded dirty region 查 DrcShapeBox，得到 affected boxes
4. 删除 affected boxes 中旧 violation
5. 只重建 affected boxes 的 shape lists
6. 只对 affected boxes 调用 verifyRVBox()
7. 合并 unaffected old violations + affected new violations
8. 与 full check violation set 比较
```

### 4.3 Correctness Invariant

必须满足：

```text
FullCheck(new_design).violations == IncrementalCheck(old_db, delta).violations
```

为保证 correctness，dirty box 必须 conservatively expand：

- shape bbox 本身；
- rule-specific halo；
- 当前 iDRC 的 `expand_size = 5 * only_pitch`；
- neighbor layer rules 所需的 adjacent cut/routing layer。

## 5. 实验设计

### 5.1 Baseline

| Baseline | 含义 |
| --- | --- |
| Full iDRC | 当前 `DRCInterface::checkDef()` 全量构建 shapes + 全量 verify。 |
| Full EDADB View | 先从 EDADB full build view，再全量 verify。 |
| Incremental EDADB | 只重建 dirty boxes。 |

### 5.2 Workloads

| Workload | 变更类型 | 预期 dirty 范围 |
| --- | --- | --- |
| W1 | 移动 1 个 instance | instance obs/pin box 附近 |
| W2 | 移动 0.1% / 1% / 5% instances | 多个局部 regions |
| W3 | 添加/删除一个 buffer | 新 instance + 相关 nets |
| W4 | 修改一个 routed net segment | 单 net route bbox |
| W5 | 删除/新增 via | cut layer + adjacent routing layers |
| W6 | 修改 blockage / special net | PDN 或 blockage 影响区域 |

### 5.3 Metrics

Correctness：

- violation set exact match；
- per-rule violation count；
- per-layer violation count；
- missed / extra violation 数。

Performance：

- shape build time；
- box build/update time；
- checked box count；
- candidate shape count；
- rule validation time；
- total runtime；
- EDADB query/update time；
- storage overhead。

Scalability：

- dirty object ratio；
- dirty box ratio；
- design size；
- layer count；
- route segment count。

## 6. 可能的论文定位

### 6.1 EDA 会议版本

目标：

- `DAC` / `ICCAD`

叙事：

```text
Open-source physical design flows increasingly need fast ECO/debug loops.
Existing DRC engines rebuild transient spatial partitions from full design snapshots.
We propose a persistent spatial-provenance database that preserves DRC-relevant views across design updates and enables conservative local rechecking.
```

核心结果：

- DRC correctness 与 full check 一致；
- ECO workload 下 runtime 显著降低；
- 可以和 iEDA flow 无缝集成。

### 6.2 Database 会议版本

目标：

- `SIGMOD` / `VLDB` / `ICDE`

叙事：

```text
EDA DRC is a spatial-graph workload with repeated local updates and expensive rule queries.
We introduce a domain-aware provenance skipping technique that maintains spatial-graph sketches under updates.
```

核心结果：

- 抽象出 spatial-graph provenance model；
- 证明/实验说明 over-approx skipping 保证 no false negative；
- 在真实 EDA workload 上比 generic spatial index/full scan 更快。

风险：

- 仅在 iEDA/iDRC 上做可能不够数据库顶会，需要把 abstraction 写得更通用，最好再加 synthetic spatial-graph workloads 或 OpenROAD-like data。

## 7. 工程实现路线

### Phase 0：Instrumentation，不改算法

- 给 `buildEnvShapeList()` / `buildResultShapeList()` 加统计日志。
- 给 `RuleValidator::buildRVBoxList()` 统计：
  - box count；
  - env/result shape count；
  - shape-to-box assignment count；
  - per-box shape count 分布。
- 给 `verifyRVModel()` 统计 per-box runtime 和 violation count。

### Phase 1：EDADB full DRC view

- 定义 DRC view C++ structs。
- 用 EDADB 写入 `DrcShape`、`DrcBox`、`DrcShapeBox`、`DrcViolation`。
- 实现 SQL 检查和导出。

### Phase 2：Dirty region + dirty box

- 构造人工 ECO workloads。
- 由 changed objects 生成 `DrcDirtyRegion`。
- 查询 dirty boxes。
- 暂时仍可调用原 iDRC verify 只验证 dirty boxes。

### Phase 3：Provenance

- 在 rule check 内记录 candidate pair / shape dependencies。
- 保存 `DrcCheckProvenance`。
- 用 provenance 做更细粒度 skipping。

### Phase 4：Paper experiments

- 多设计、多 dirty ratio、多 rule type。
- 和 full iDRC、generic R-tree/tile index、no-provenance incremental 做对比。

## 8. 当前不确定项

- iDRC rule checker 的每种 rule 是否都能安全用统一 `expand_size` 近似；部分 rule 可能需要 rule-specific halo。
- `DRCShape` 目前只有 `net_idx`，没有 stable source object id；EDADB view 需要补 `source_kind/source_name/source_idx`。
- 当前 `RVBox` 内保存的是 shape pointer，EDADB 持久化后需要 stable `shape_id` 和可重建 shape list。
- 多线程 `verifyRVModel()` 与 EDADB 写入不能直接混用，需先内存收集再批量写入，或设计线程安全 writer。
- 如果目标数据库顶会，需要更通用 benchmark；仅 sky130_gcd 不足。

