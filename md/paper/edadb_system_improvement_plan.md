# EDADB 系统能力补强与研究计划

本文记录 EDADB 自身需要如何演进，才能支撑 iEDA 工具侧的增量 DRC、增量 placement、routing ECO、stage-aware dataset 和 database 方向论文。重点不是再列一个功能愿望清单，而是从当前代码事实出发，明确瓶颈、系统模块、实验指标和研究价值。

更细的 core 代码审计见 `docs/paper/edadb_core_research_notes.md`。本文保留系统路线图和实验矩阵。

从研究点到工程里程碑的执行路线见 `docs/paper/research_execution_plan.md`。

## 1. 当前 EDADB 的系统定位

### 1.1 已有能力

EDADB 当前更接近一个 C++ object graph ORM：

- 支持 `TABLE4CLASS*` / `TABLE4SHADOW*` 宏定义 C++ 类到 table 的映射。
- 支持 scalar、inline object、pointer、vector child table、primitive vector 和 shadow store type。
- 支持 `createTable()`、`insertObject()`、`readAll()`、`readByPrimaryKey()`、`readVectorByPredicate()`、`updateObject()`、`upsertObject()`、`deleteObject()`。
- SQLite 是当前唯一真实后端，可作为 correctness backend。

### 1.2 代码证据

- `src/database/edadb/core/include/edadb.h:33`
  - 说明 EDADB object、operator、global `DbManager` 和 table-definition cache 不是 thread-safe。
- `src/database/edadb/core/include/edadb.h:107`
  - `initDatabase()` 只通过 `DbManager::i().connect()` 打开全局连接。
- `src/database/edadb/core/include/edadb/backend/sqlite/DbManager4Sqlite.h:24`
  - SQLite manager 是 process-global singleton，不是 thread-safe。
- `src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpUpdate4Sqlite.h:49`
  - `UPDATE` 先检查 root row，再 `deleteByPrimaryKey()` + `insert()`。
- `src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpUpsert4Sqlite.h:47`
  - `UPSERT` 也是 `deleteByPrimaryKey()` + `insert()`。
- `src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpQueryGeneric4Sqlite.h:47`
  - generic query 只支持 root table operator。
- `src/database/edadb/core/include/edadb/DbObjectTraverser.h:29`
  - object traversal 分 scalar phase 和 vector child phase。
- `src/database/edadb/core/include/edadb/DbTableDefTraverser.h:34`
  - schema traverser 负责展开 inline object 和 vector child table。

### 1.3 当前瓶颈

| 瓶颈 | 当前表现 | 对 iEDA 的影响 |
| --- | --- | --- |
| 单 SQLite 后端 | 所有 DML/DQL 走 SQLite row-store。 | correctness 足够，但大规模 geometry/graph/metric query 可能慢。 |
| 全局单连接 | `DbManager` singleton，非线程安全。 | iDRC/iRT/iPL 中并行阶段难直接共享 EDADB。 |
| update/upsert 整图替换 | root delete + insert，child vector 级别没有 partial update。 | ECO 场景会破坏“只改局部”的性能优势。 |
| root query 限制 | generic query 主要支持 root table。 | child table、geometry table、metric view 查询需要更灵活 API。 |
| 缺少 domain index | 没有 bbox/tile/layer/net graph index。 | DRC/routing/placement 仍可能 full scan。 |
| schema 靠手写 | `edadb_idb_schema.h` 人工维护宏和 shadow。 | 适配 iEDA 新类成本高，容易字段/PK/shadow 出错。 |
| 缺少 stage/version | 当前更像单 snapshot DB。 | 无法原生支持 flow provenance、ECO delta、ML dataset。 |

## 2. 系统补强路线

### 2.1 Query Layer：从 root object query 到 EDA domain query

目标：

- 在 ORM API 外增加面向 EDA 的 query layer。
- 支持 object、child table、spatial、graph、metric 四类查询。

建议 API：

```text
queryByInst(inst_name)
queryByNet(net_name)
queryByLayer(layer_id)
queryByBBox(layer_id, llx,lly,urx,ury)
queryByTile(stage_id, tile_x,tile_y)
queryDirtyObjects(stage_id, version_id)
queryAffectedNets(changed_inst_list)
```

需要实现：

- child table predicate query；
- typed projection，而不是每次恢复完整 root object；
- common EDA query template；
- query plan trace，记录使用了哪些 table/index/view。

研究价值：

- 可支撑 `DB-2 Provenance-based Data Skipping`。
- 可支撑 `DB-3 Hybrid Row/Column/Spatial/Graph Physical Layout`。

### 2.2 Index Layer：空间、图和 stage/version 索引

目标：

- 不把所有问题压给 SQLite naive scan。
- 对 EDA 高频访问模式提供 domain index。

建议索引：

| 索引 | key | value | 服务对象 |
| --- | --- | --- | --- |
| Tile index | `(stage_id, layer_id, tile_x, tile_y)` | shape ids / net ids | iDRC、iRT、congestion。 |
| BBox index | `(layer_id, bbox)` | overlapping shapes | DRC candidate、routing obstacle。 |
| Net adjacency | `net_name` | pins / instances / route segments | iPL HPWL、iRT reroute、timing affected cone。 |
| Inst adjacency | `inst_name` | pins / nets / rows / bins | placement delta、ECO。 |
| Stage version | `(run_id, stage_id, version_id)` | snapshot/delta rows | flow provenance、ML dataset。 |

第一阶段不要急着做复杂 R-tree，可以先做 conservative tile index：

```text
geometry bbox -> overlapped tile ids -> TileObject rows
```

研究价值：

- EDA workload 的局部性强，tile/net/layer index 比通用 row scan 更容易产生明显收益。
- 可与 iDRC `RVBox`、iPL `GridManager`、iRT routing panel/tile 概念对齐。

### 2.3 Delta Layer：对象级 diff 和 partial update

目标：

- 从“整图 delete + insert”改成“可记录、可查询、可传播的 delta”。

建议数据结构：

| 表 | 字段 | 作用 |
| --- | --- | --- |
| `DbVersion` | run_id, stage_id, version_id, parent_version_id | 版本 DAG。 |
| `DbChangeLog` | version_id, object_kind, object_pk, op, old_hash, new_hash | 对象级变更记录。 |
| `DbFieldDelta` | change_id, field_name, old_value, new_value | 字段级 diff，可选。 |
| `DbVectorDelta` | change_id, vector_name, child_pk, op | child vector 局部更新。 |
| `DbDirtyObject` | version_id, object_kind, object_pk, reason | 工具侧 dirty set。 |

需要实现：

- object hash / field hash；
- vector child partial insert/delete/update；
- delta transaction；
- view invalidation：根据 change log 找 affected view rows；
- rollback/compare：比较两个 version 的 object/view 差异。

研究价值：

- 是 `DB-1 EDA-specific Incremental View Maintenance` 的核心基础。
- 对 iEDA ECO flow 很关键：buffer insertion、cell move、wire edit、via edit 都是 delta。

### 2.4 View Layer：从存对象到维护工具视图

目标：

- EDADB 不只保存 iDB object；还保存工具真正反复使用的 derived view。

建议第一批 view：

| View | 来源 | 用途 |
| --- | --- | --- |
| `PlacementView` | instance、pin、net、row、region | HPWL、bin density、incremental legalization。 |
| `GeometryView` | wire、via、pin shape、obs、blockage | DRC、routing obstacle、congestion。 |
| `RoutingView` | guide、wire segment、via、tile occupancy | ECO routing、local reroute。 |
| `TimingView` | inst-pin-net mapping、timing arc、critical path | iSTA/iTO affected cone。 |
| `StageMetricView` | tool summary/report | DSE、ML dataset、debug。 |

每个 view 需要同时提供：

```text
fullBuild(source_snapshot)
deltaUpdate(change_log)
validateAgainstFullBuild()
exportForTool()
exportForML()
```

研究价值：

- 这是连接 EDA 顶会问题和数据库顶会问题的中间层。
- 工具侧论文看 runtime/QoR；数据库论文看 view maintenance/update/query。

### 2.5 Storage Layer：多后端和混合物理布局

目标：

- SQLite 继续作为 correctness backend，但不要承担所有高性能 workload。

建议分层：

| 层 | 后端 | 作用 |
| --- | --- | --- |
| Source of truth | SQLite | correctness、roundtrip、调试、可复现。 |
| Runtime cache | in-memory tables/indexes | iEDA 点工具在线查询。 |
| Analytics | DuckDB/Arrow/Parquet | ML feature、stage summary、批量 scan。 |
| Spatial | tile index / R-tree | bbox overlap、DRC/routing query。 |
| Graph | adjacency table/cache | netlist/timing/routing traversal。 |

第一阶段可以只做：

- SQLite source of truth；
- in-memory tile index；
- adjacency cache；
- Parquet/CSV export。

研究价值：

- 对 `DB-3 Hybrid Row/Column/Spatial/Graph Physical Layout` 形成系统贡献。
- 如果要冲数据库会，需要清楚定义 consistency protocol 和 query routing 策略。

### 2.6 Schema Layer：从人工 schema 到可验证 schema

目标：

- 减少 iEDA 类适配成本，避免 shadow 滥用。

建议工具：

| 工具 | 输入 | 输出 |
| --- | --- | --- |
| schema lint | `edadb_idb_schema.h` | PK、FK、shadow、field order 警告。 |
| DEF field checker | `DefWrite` / `DefRead` | 需要持久化字段清单。 |
| shadow advisor | class layout + PK/ownership | direct mapping / shadow mapping 建议。 |
| SQL validator generator | schema + test DEF | row count、field value、FK 检查 SQL。 |

当前 shadow 收敛原则：

- 优先 direct mapping。
- 只有 direct mapping 无法表达 stable PK、归属关系、轻量视图、名字化引用或 ordered child relation 时才定义 shadow。
- 不为“看起来更整洁”额外定义 shadow。

研究价值：

- 可作为工程支撑，不建议单独作为主论文。
- 对大规模接入 iDB 类非常重要。

## 3. 与 iEDA 工具侧研究的对应关系

| EDADB 能力 | 支撑 iEDA 工具 | 研究点 |
| --- | --- | --- |
| Tile/BBox index | iDRC、iRT | 增量 DRC、local reroute、provenance skipping。 |
| Net/inst adjacency | iPL、iRT、iSTA/iTO | HPWL delta、affected cone、routing ECO。 |
| Change log/version | 全 flow | ECO delta、stage provenance、debug replay。 |
| Incremental view | iPL/iDRC/iRT | EDA-specific IVM。 |
| Query provenance | iDRC/iRT/iSTA | data skipping、no-false-negative dirty region。 |
| Matrix/PowerIR view | iPA/iIR/iPNP | power/IR/PDN 增量分析、IR hotspot provenance。 |
| Analytics export | ML/agent | stage-aware dataset、QoR prediction、LLM debug memory。 |
| Schema lint | DEF adapter 和未来工具 view | 降低接入成本，提高 roundtrip 可信度。 |

## 4. 推荐实施顺序

### Phase 0：把 correctness backend 做稳

- 保持 SQLite backend。
- 补齐 schema lint 和 validation SQL。
- 每个已接入 iDB 类都有 roundtrip + DB row/value 验证。
- 所有实验记录 run_id、git commit、EDADB commit、test command。

### Phase 1：增加 domain query 和 dirty set

- 实现 child/root predicate query 的统一 API。
- 增加 `DirtyObject`、`DirtyRegion`、`DirtyNet` 基础表。
- iPL 先接 changed inst -> affected nets/bins。
- iDRC 先接 dirty region -> affected boxes。

### Phase 2：实现两个最小 incremental view

- `PlacementView`：HPWL + bin density。
- `GeometryView`：shape box index + violation summary。
- 每个 view 必须有 `fullBuild`、`deltaUpdate`、`validateAgainstFullBuild`。

### Phase 3：加 provenance 和 skipping

- 记录 query/view 的 object/tile/net provenance。
- 更新时维护 dirty provenance unit。
- 用 sketch / conservative set 做 skipping。

### Phase 4：多后端和分析导出

- 保留 SQLite correctness。
- 增加 memory index/cache。
- 导出 Arrow/Parquet/CSV，服务 ML 和大规模 sweep。

## 5. 实验矩阵

### 5.1 Microbenchmark

| Benchmark | 比较对象 | 指标 |
| --- | --- | --- |
| insert/read object graph | EDADB current vs batch op | runtime、statement count、DB size。 |
| update vector child | delete+insert vs partial update | dirty child ratio、runtime、write amplification。 |
| query by bbox | SQLite scan vs tile index | query latency、candidate count、false positive。 |
| query by net | SQLite join vs adjacency cache | query latency、memory overhead。 |
| view refresh | full rebuild vs delta update | update time、dirty ratio、correctness。 |

### 5.2 Tool-integrated benchmark

| 工具 | View | Workload | Correctness |
| --- | --- | --- | --- |
| iPL | PlacementView | cell move、buffer insertion、legalization | HPWL/bin density 与 full eval 一致。 |
| iDRC | GeometryView | wire/via/blockage change | violation set 与 full DRC 一致。 |
| iRT | RoutingView | local reroute | wire/via/tile occupancy 与 full route view 一致。 |
| iSTA/iTO | TimingView | buffer/resize/net reconnect | affected cone 覆盖 full timing changed endpoints。 |
| iNO | NetlistECOView | fix fanout、insert buffer、split net | dirty inst/net/pin 与 iDB/STA graph 一致。 |
| iPDN/iPNP/iIR | PowerIR/PDNView | stripe/via/template/power change | PDN geometry、IR hotspot、overflow 与 full run 一致。 |
| Flow/DSE | FlowMemory | config sweep、stage failure、QoR drift | stage snapshot、report、object delta 可追溯；细化见 `edadb_flow_dse_memory_plan.md`。 |

## 6. 论文定位

### 6.1 EDA 会议版本

目标：

- `DAC` / `ICCAD`

核心叙事：

```text
Open-source EDA tools repeatedly rebuild tool-specific internal databases from shared design snapshots.
EDADB turns the shared database into a persistent incremental view manager for physical-design ECO workloads.
```

需要强调：

- 真实 iEDA flow 集成；
- iPL/iDRC 至少两个工具验证；
- full vs incremental correctness；
- runtime 和 debug/replay 收益。

### 6.2 Database 会议版本

目标：

- `SIGMOD` / `VLDB` / `ICDE`

核心叙事：

```text
Physical design workloads combine object graphs, spatial geometry, netlist graphs, and iterative local updates.
EDADB studies incremental view maintenance and provenance skipping for this hybrid spatial-graph-object workload.
```

需要强调：

- workload 抽象不能只绑定 iEDA；
- query/update/view model 要清楚；
- 至少有 spatial + graph + object 三类 view；
- benchmark 需要覆盖不同 dirty ratio 和 design scale。

## 7. 当前不要做的事

- 不要只优化 DEF read/write，然后包装成 database research。
- 不要把 SQLite table dump 当成高性能 database。
- 不要为了每个 iDB 类都定义 shadow；shadow 必须有 PK/归属/视图/引用上的必要性。
- 不要在没有 full-build validator 的情况下宣称 incremental view 正确。
- 不要先做复杂多后端抽象；先把 SQLite correctness + memory index 跑通。

## 8. 下一步最小闭环

推荐从两个小闭环开始：

1. iPL：
   - `PlInst` + `PlNetPin` + `PlNet` + `PlBin`；
   - changed inst -> affected net/bin；
   - full HPWL/bin density vs delta HPWL/bin density。
2. iDRC：
   - `DrcShape` + `DrcBox` + `DrcShapeBox`；
   - dirty region -> affected box；
   - full violation set vs affected-box recheck violation set。

这两个闭环覆盖：

- object graph；
- spatial view；
- graph adjacency；
- incremental update；
- correctness validation。

如果这两个闭环跑通，才有资格继续扩展到 iRT、iSTA/iTO 和 stage-aware ML dataset。
