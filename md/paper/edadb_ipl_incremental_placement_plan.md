# EDADB + iPL 增量 Placement View 实验计划

本文是 `EDADB + iEDA` 研究路线中 `EDA-2 / DB-1 / AI-1` 的细化计划。目标不是替换 iPL 算法，而是把 placement 相关对象和评估指标做成可持久化、可增量维护、可复验的 EDADB view，用真实 iEDA flow 验证局部改动后能否减少重复 wrapper、重复 HPWL/bin-density 计算和增量 legalization 的数据准备成本。

## 1. 研究问题

### 1.1 核心问题

当前 iPL 已有全量 placement flow、incremental legalization 和 changed instance list 接口。问题是：

- iPL 每次需要从 iDB wrapper 出 placement 专用对象；
- HPWL、bin density、pin/net 关系等评估数据主要是内存临时结构；
- ECO 或 buffer insertion 后，changed instance list 已存在，但缺少持久化 dirty inst/net/bin view；
- placement 中间状态、metric provenance、局部更新范围没有统一记录，难以复验和导出 ML 数据。

研究目标：

```text
Can a persistent incremental placement view maintain HPWL, density, and changed-object provenance under local placement updates while preserving full-evaluation consistency?
```

### 1.2 预期贡献

1. 定义 placement workload 的对象表、metric view 和 dirty-object view。
2. 设计 moved-inst 驱动的 net bbox / HPWL / bin density 增量维护算法。
3. 将 EDADB dirty set 与 iPL `runIncrementalLegalization(changed_inst_list)` 对接。
4. 在 iEDA/iPL 上验证 full evaluation 与 incremental view 的一致性和性能差异。

## 2. 本地代码证据

### 2.1 iPL 外部入口

- `src/platform/tool_manager/tool_api/ipl_io/ipl_io.cpp:52`
  - `runPlacement()` 首次初始化 placer，否则调用 `iPLAPIInst.updatePlacerDB()`。
- `src/platform/tool_manager/tool_api/ipl_io/ipl_io.cpp:78`
  - `runIncrementalLegalization()` 可复用已启动的 legalizer。
- `src/platform/tool_manager/tool_api/ipl_io/ipl_io.cpp:101`
  - `runIncrementalLegalization(changed_inst_list)` 调用 `updatePlacerDB(changed_inst_list)` 和 `runIncrLG(changed_inst_list)`。
- `src/platform/tool_manager/tool_api/ipl_io/ipl_io.cpp:243`
  - `runIncrementalFlow()` 运行 legalization、report 和 writeback。

### 2.2 iDB 到 iPL 的 wrapper 路径

- `src/operation/iPL/source/module/wrapper/IDBWrapper.cc:48`
  - `updateFromSourceDataBase()` 全量扫描 iDB instances 和 nets。
- `src/operation/iPL/source/module/wrapper/IDBWrapper.cc:77`
  - `updateFromSourceDataBase(inst_list)` 只根据 changed instances 找相关 nets 并局部 wrapper。
- `src/operation/iPL/source/module/wrapper/IDBWrapper.cc:223`
  - `wrapLayout()` 从 iDB 取 dbu、die/core、rows、cells、routing info。
- `src/operation/iPL/source/module/wrapper/IDBWrapper.cc:394`
  - `wrapDesign()` 从 iDB 取 design name、instances、nets、regions。
- `src/operation/iPL/source/module/wrapper/IDBWrapper.cc:438`
  - `wrapIdbInstance()` 转换 instance 坐标、状态、类型。
- `src/operation/iPL/source/module/wrapper/IDBWrapper.cc:572`
  - `wrapIdbNet()` 转换 net 类型、driver/load pins。
- `src/operation/iPL/source/module/wrapper/IDBWrapper.cc:631`
  - `wrapPin()` 转换 IO pin / instance pin、offset、center coordinate。
- `src/operation/iPL/source/module/wrapper/IDBWrapper.cc:759`
  - `writeBackSourceDatabase()` 把 iPL 结果写回 iDB instance status / coordinate。

### 2.3 增量 legalization 和指标

- `src/operation/iPL/api/PLAPI.cc:176`
  - `runIncrementalFlow()` 运行 `runLG()`、report、writeback。
- `src/operation/iPL/api/PLAPI.cc:498`
  - `runIncrLG(inst_name_list)` 将 instance name 转成 `Instance*`，调用 `LegalizerInst.updateInstanceList(inst_list)` 和 `runIncrLegalize()`。
- `src/operation/iPL/api/PLAPI.cc:704`
  - `updatePlacerDB()` 全量更新 placer DB。
- `src/operation/iPL/api/PLAPI.cc:709`
  - `updatePlacerDB(inst_list)` 局部更新 placer DB。
- `src/operation/iPL/api/report/PLReport.cc:199`
  - `reportBinDensity()` 重新构建 `GridManager` 并扫描 instances 计算 peak bin density。
- `src/operation/iPL/api/report/PLReport.cc:723`
  - HPWL report 输出 total/max/long-net 等指标。
- `src/operation/iPL/source/module/grid_manager/GridManager.cc:452`
  - `obtainPeakGridDensity()` 扫描所有 grid 找 peak density。

### 2.4 关键观察

iPL 已经暴露了增量入口，但 dirty-object、affected-net、affected-bin 和 metric provenance 仍由调用方隐式管理。EDADB 可以作为中间层，把当前 “changed inst list” 升级为：

```text
changed instances
  -> affected nets
  -> old/new net bbox and HPWL
  -> old/new overlapped bins
  -> dirty metric rows
  -> incremental legalization input
```

这比直接重写 placement algorithm 风险小，因为第一阶段只维护 placement view 和指标一致性。

## 3. EDADB View Schema 草案

### 3.1 基础对象表

| 表 | 主键 | 字段 | 作用 |
| --- | --- | --- | --- |
| `PlRun` | `run_id` | design, stage, config_hash, def_path, timestamp | 记录一次 placement/eval run。 |
| `PlInst` | `(run_id, inst_name)` | master_name, width, height, status, orient, x, y, llx,lly,urx,ury, is_fixed | 保存 placement instance state。 |
| `PlPin` | `(run_id, pin_name)` | inst_name, net_name, pin_type, io_type, offset_x, offset_y, center_x, center_y | 保存 pin 与 net/inst 关系。 |
| `PlNet` | `(run_id, net_name)` | net_type, driver_pin, pin_count, bbox_llx,lly,urx,ury, hpwl | 保存 net bbox 和 HPWL view。 |
| `PlNetPin` | `(run_id, net_name, pin_name)` | inst_name, pin_role | 保存 net-pin adjacency。 |
| `PlRegion` | `(run_id, region_name)` | type | 保存 fence/guide/blockage placement region。 |
| `PlRegionRect` | `(run_id, region_name, rect_id)` | llx,lly,urx,ury | 保存 region/blockage boundary。 |

### 3.2 Metric 和 dirty 表

| 表 | 主键 | 字段 | 作用 |
| --- | --- | --- | --- |
| `PlBin` | `(run_id, bin_x, bin_y)` | llx,lly,urx,ury, occupied_area, density, pin_count | 保存 bin density view。 |
| `PlDirtyInst` | `(run_id, inst_name)` | old_llx,lly,urx,ury, new_llx,lly,urx,ury, reason | 记录移动或新增/删除 instance。 |
| `PlDirtyNet` | `(run_id, net_name)` | reason, old_hpwl, new_hpwl | 记录受 changed inst 影响的 nets。 |
| `PlDirtyBin` | `(run_id, bin_x, bin_y)` | reason | 记录受 changed inst old/new bbox 影响的 bins。 |
| `PlMetric` | `(run_id, metric_name)` | value, unit, stage | 保存 total HPWL、peak density 等 summary。 |
| `PlMetricProvenance` | `(run_id, metric_name, object_kind, object_key)` | contribution | 记录 metric 依赖的 net/bin/inst。 |

### 3.3 最小可实现版本

第一版先做：

- `PlInst`
- `PlNet`
- `PlNetPin`
- `PlBin`
- `PlDirtyInst`
- `PlDirtyNet`
- `PlDirtyBin`
- `PlMetric`

第二版再补：

- `PlPin`
- `PlRegion`
- `PlMetricProvenance`
- stage/run 版本管理。

## 4. 增量算法草案

### 4.1 Full Build

```text
1. 复用 iPL IDBWrapper 从 iDB 获得 instances / nets / pins
2. 写入 PlInst、PlNetPin
3. 对每个 net 计算 bbox 和 HPWL，写入 PlNet
4. 按 iPL GridManager 的 bin 定义计算 PlBin density
5. 写入 PlMetric(total HPWL, peak density, overflow 等)
```

### 4.2 Delta Update

输入：

- changed instance list；
- instance old/new bbox；
- 新增/删除 buffer；
- 或两个 placement snapshot 的 diff。

流程：

```text
1. 根据 changed instances 写 PlDirtyInst
2. 通过 PlNetPin 找 affected nets，写 PlDirtyNet
3. 只重算 affected nets 的 bbox/HPWL
4. 用 instance old/new bbox 找 affected bins，写 PlDirtyBin
5. 只更新 affected bins 的 occupied area/density
6. 增量更新 PlMetric(total HPWL, peak density)
7. 将 changed_inst_list 传给 iPL runIncrementalLegalization()
8. writeback 后再次抽取 changed inst，验证 view 与 full eval 一致
```

### 4.3 Correctness Invariant

必须满足：

```text
FullEval(new_placement).HPWL == IncrementalView(old_db, moved_inst_delta).HPWL
FullEval(new_placement).bin_density == IncrementalView(old_db, moved_inst_delta).bin_density
FullEval(new_placement).legalization_result == iPL incremental legalization result under same changed_inst_list
```

HPWL 和 geometry 坐标应使用 iDB/iPL 当前整数 DBU 语义，避免浮点误差。

## 5. 实验设计

### 5.1 Baseline

| Baseline | 含义 |
| --- | --- |
| Full iPL Wrapper | 当前 `updateFromSourceDataBase()` 全量 wrapper instances/nets。 |
| Full EDADB View | 从 EDADB 全量生成 placement view 和 metrics。 |
| Incremental EDADB View | 只更新 dirty inst/net/bin/metric。 |
| iPL Incr LG | 当前 `runIncrementalLegalization(changed_inst_list)`。 |

### 5.2 Workloads

| Workload | 变更类型 | 预期 dirty 范围 |
| --- | --- | --- |
| W1 | 移动 1 个 standard cell | 该 cell、相连 nets、old/new overlapped bins。 |
| W2 | 移动 0.1% / 1% / 5% cells | 多个局部 inst/net/bin。 |
| W3 | 移动一个 macro | 大范围 bins、macro 相关 nets。 |
| W4 | 插入 buffer | 新 instance、拆分/新增 nets、局部 bins。 |
| W5 | placement legalization 后局部 cell relocation | changed inst list 对应 affected nets/bins。 |
| W6 | 新增/修改 placement blockage/region | region 影响的 bins 和 legalization candidate。 |

### 5.3 Metrics

Correctness：

- total HPWL exact match；
- per-net HPWL exact match；
- peak/avg bin density match；
- dirty net/bin 覆盖率；
- incremental legalization 后 DEF 输出与 full flow 对比。

Performance：

- wrapper time；
- affected net count；
- affected bin count；
- HPWL update time；
- bin density update time；
- EDADB query/update time；
- total incremental runtime；
- storage overhead。

Scalability：

- changed instance ratio；
- net degree distribution；
- bin size；
- design size；
- macro count。

## 6. 可能的论文定位

### 6.1 EDA 会议版本

目标：

- `DAC` / `ICCAD`

叙事：

```text
Incremental placement flows expose changed-object APIs, but their data preparation and metric evaluation remain largely transient and repeatedly rebuilt.
We propose a persistent placement view that turns moved instances into affected nets/bins/metrics and connects directly to incremental legalization.
```

核心结果：

- HPWL / bin density 与 full evaluation 一致；
- changed-inst workload 下减少重复 wrapper 和指标计算；
- placement 中间状态可复现、可查询、可导出。

### 6.2 Database 会议版本

目标：

- `SIGMOD` / `VLDB` / `ICDE`

叙事：

```text
Physical placement evaluation is an update-heavy spatial-graph workload.
We introduce domain-aware incremental views for net bounding boxes and spatial bin density, combining graph adjacency and geometric overlap maintenance.
```

核心结果：

- 抽象出 graph-spatial incremental view 模型；
- moved-object delta 下避免 full graph/geometry scan；
- 在真实 EDA placement flow 上验证性能和正确性。

风险：

- 只做 HPWL/bin density 可能系统贡献偏小；需要和 provenance、query optimizer 或多工具 stage view 结合。
- 数据库会版本需要展示方法可泛化到 DRC/routing/timing，而不只是 iPL wrapper cache。

## 7. 实施阶段

### Phase 0：只做离线验证

- 从已完成 EDADB DEF adapter 读出 Design/Instance/Pin/Net。
- 离线生成 `PlInst`、`PlNet`、`PlBin`。
- 与 iPL report 的 HPWL/bin density 对齐。

### Phase 1：增量 HPWL

- 保存 `PlNetPin` 和 `PlNet` bbox。
- 输入 changed inst list，只重算 affected nets。
- 与 full HPWL 对比。

### Phase 2：增量 bin density

- 保存 `PlBin` occupied area。
- 用 old/new instance bbox 更新 affected bins。
- 与 full `GridManager` 扫描结果对比。

### Phase 3：接入 iPL incremental legalization

- EDADB 生成 `changed_inst_list`。
- 调用 `runIncrementalLegalization(changed_inst_list)`。
- writeback 后重新验证 placement view。

### Phase 4：跨工具联动

- 将 placement dirty bins 传给 iDRC / iRT / timing affected cone。
- 形成 placement -> routing/DRC/timing 的 stage-aware delta pipeline。

## 8. 需要进一步核验的问题

- iPL 内部 pin center 和 iDB pin average coordinate 的更新时机。
- `GridManager` bin 定义和 report bin density 的精确一致性。
- macro/fixed/outside instance 在 bin density 中的处理规则。
- buffer insertion 后 net split / new net naming 的稳定主键。
- DEF read/write 与 placement writeback 对 instance status/orient 的边界语义。
