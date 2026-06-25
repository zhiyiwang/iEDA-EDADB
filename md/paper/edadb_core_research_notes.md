# EDADB Core 代码审计与系统研究路线

本文只记录从当前 EDADB core 代码直接能推出的工程事实、瓶颈和研究机会。它不是论文宣传稿，后续写 proposal 时需要继续补实验数据。

## 1. 当前 core 架构事实

### 1.1 Public facade

入口文件：

- `src/database/edadb/core/include/edadb.h`
- `src/database/edadb/core/demo/api_quickstart.cpp`

当前 public API 已覆盖：

- database lifecycle：`initDatabase()`、`closeDatabase()`。
- transaction：`beginTransaction()`、`commitTransaction()`、`rollbackTransaction()`。
- table lifecycle：`createTable<T>()`、`dropTable<T>()`。
- DML：`insertObject()`、`insertVector()`、`updateObject()`、`updateVector()`、`upsertObject()`、`deleteObject()`。
- DQL：`readByPrimaryKey()`、`readAll()`、`readByPredicate()`、`readVectorByPredicate()`。
- reusable op：`makeInsertOp()`、`makeReadAllOp()`、`makeGenericQueryOp()` 等。

直接结论：

- EDADB 已经不是单纯 demo API，而是一个可复用的 C++ object graph ORM。
- 但 public query 仍主要围绕 root table，缺少 EDA domain query，例如 bbox/net/tile/stage/dirty-set query。

### 1.2 Schema / table generation

入口文件：

- `src/database/edadb/core/include/edadb/Table4Class.h`
- `src/database/edadb/core/include/edadb/DbTableDefTraverser.h`
- `src/database/edadb/core/include/edadb/DbTableDefBuilder.h`
- `src/database/edadb/core/include/edadb/DbTableDefCatalog.h`

当前机制：

- `TABLE4CLASS*` 宏定义 C++ class 到 DB table 的映射。
- scalar / inline object 成员被展开到当前 row。
- vector child 被展开为 child table，并通过 parent chain 生成 FK。
- shadow 通过 `StoreProperty` 把原 C++ object 转换成存储视图。

直接结论：

- 这套机制适合表达 iDB object graph。
- 对 EDA 来说，schema 的关键不是“存所有字段”，而是“存 DEF/工具语义需要的字段 + 可重建 derived fields”。
- shadow 应收敛使用，只在 stable PK、归属关系、轻量视图、名字化引用、ordered child relation 无法 direct mapping 时使用。

### 1.3 Object traversal

入口文件：

- `src/database/edadb/core/include/edadb/DbObjectTraverser.h`

当前机制：

- traversal 分两阶段：
  - 先遍历当前 row 的 scalar / inline / shadow scalar。
  - 当前 row 完成后，再遍历 vector child table。
- INSERT 时 parent row 先落库，child rows 再通过 FK 绑定。
- SELECT 时先恢复当前 row，再递归查询 child vector。
- null inline pointer 有专门逻辑，避免对 null object 调用 generated metadata。

直接结论：

- EDADB 已经能处理 object graph 的基本生命周期。
- 但 traversal 是全图遍历；对局部 ECO 来说，缺少 field-level / child-level dirty traversal。

### 1.4 SQLite backend

入口文件：

- `src/database/edadb/core/include/edadb/backend/sqlite/DbManager4Sqlite.h`
- `src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpInsert4Sqlite.h`
- `src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h`
- `src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpQueryGeneric4Sqlite.h`
- `src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpUpdate4Sqlite.h`
- `src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpUpsert4Sqlite.h`

当前机制：

- SQLite 是唯一真实 backend。
- `DbManager` 是 process-global singleton，代码注释明确不是 thread-safe。
- `QUERY_GENERIC` 只支持 root table op，predicate 拼接到 generated root SELECT 后。
- `UPDATE` 要求 root PK 已存在，然后 delete old graph + insert new graph。
- `UPSERT` 也是 delete + insert full graph。

直接结论：

- SQLite backend 很适合 correctness backend 和 debugging backend。
- 当前 UPDATE/UPSERT 写放大明显，不适合大规模 ECO 直接当高性能增量更新。
- Root-only generic query 不足以支撑 EDA 工具的 bbox/net/tile/stage 查询。
- 单全局连接限制并行工具或多 stage query。

## 2. 已有测试覆盖

入口文件：

- `src/database/edadb/core/test/DbTableOpQueryGeneric.cpp`
- `src/database/edadb/core/test/DbTableOpShapeMutation.cpp`
- `src/database/edadb/core/test/DbPublicApi.cpp`
- `src/database/edadb/core/test/DbTableOpInsert.cpp`
- `src/database/edadb/core/test/DbTableOpUpdate.cpp`
- `src/database/edadb/core/test/DbTableOpUpsert.cpp`

已覆盖的能力：

- public API quick path。
- root predicate query。
- insert/read/update/upsert/delete object graph。
- inline pointer null/restore。
- shadow pointer/vector shape mutation。
- foreign-key content validation。

需要补的测试：

- child table predicate query。
- bbox/tile/net adjacency query。
- partial child update。
- batch transaction 性能。
- 多连接/多线程安全边界测试。
- EDA schema lint：字段覆盖、PK/FK、shadow 必要性、DEF writer/read parser 语义一致性。

## 3. EDADB 系统研究点

### Core-1：EDA domain query layer

问题：

- 当前 `readByPredicate()` 面向 root table；EDA 工具需要更自然的 domain query。

建议 API：

- `queryByBBox(layer, rect)`
- `queryByNet(net_name)`
- `queryByTile(tile_id)`
- `queryDirtyObjects(run_id, stage)`
- `queryStageDelta(stage_a, stage_b)`

研究价值：

- 把 EDADB 从 ORM 推向 EDA DBMS。
- 为 iDRC/iRT/iPL/iSTA/iPNP 的增量 view 提供统一查询层。

### Core-2：Partial update and dirty traversal

问题：

- 当前 `UPDATE/UPSERT` 是 delete + insert full graph。
- ECO 通常只改少量 instance、net、wire、via、region。

建议机制：

- 为 root/child table 引入 dirty path。
- 支持 child vector append/delete/update。
- 支持 field-level changed mask。
- 为 `DbObjectTraverser` 增加 dirty-aware traversal policy。

研究价值：

- 可支撑数据库方向的 EDA-specific incremental maintenance。
- 可直接提升 iEDA ECO workload 性能。

### Core-3：Hybrid index and view manager

问题：

- SQLite row-store 不适合所有 EDA query。
- EDA 同时需要 object graph、spatial bbox、netlist graph、timing/power matrix。

建议机制：

- SQLite 作为 source of truth。
- memory tile/R-tree index 支撑 geometry query。
- adjacency cache 支撑 net/timing graph query。
- column cache / Arrow export 支撑 ML feature scan。
- view manager 负责 source-of-truth 与 index/view 一致。

研究价值：

- 可形成 database 论文的 workload-aware hybrid storage/index contribution。
- 与 OpenDB binary DB 区分：EDADB 重点是 persistent query/view/provenance，而不只是 shared in-memory DB。

### Core-4：Provenance and time-travel

问题：

- 当前只保存对象当前状态，不能回答“哪个 stage/哪个改动引入了 QoR 变化”。

建议机制：

- 每个 object row 增加 run/stage/version 语义。
- 每个 query/view 记录 provenance units：object pk、net、layer、tile、stage。
- 支持 object delta 和 stage diff。

研究价值：

- 支撑 stage-aware debug、ECO impact prediction、agent memory。
- 支撑 provenance-based data skipping 和 incremental sketch maintenance。

### Core-5：Schema synthesis and validation

问题：

- 当前 schema 依赖人工 `TABLE4CLASS*` 与 shadow 定义，容易和 iEDA DEF read/write 语义不一致。

建议机制：

- 从 `DefWrite`/`DefRead` 中抽取被写出/读入的字段。
- 从 C++ class metadata 中生成候选 schema。
- 自动检查：
  - first field 是否适合作 PK；
  - vector child 是否有稳定 FK；
  - shadow 是否必要；
  - derived fields 是否应该重建而非持久化；
  - EDADB roundtrip 是否覆盖 DEF roundtrip 字段。

研究价值：

- 降低接入 iDB 大量类的工程成本。
- 可作为 C++ object ORM + domain semantics 的 static verification 方向。

## 4. 与 iEDA 工具结合的优先级

建议优先级：

1. `iPL PlacementView`：最容易定义 dirty instance -> affected net/bin。
2. `iDRC GeometryView`：最能体现 spatial index/provenance skipping。
3. `iRT RoutingView`：最能体现 tile occupancy、route diff、local reroute。
4. `iSTA/iTO TimingView`：最能体现 graph affected cone。
5. `iPNP/iPA/iIR PowerIRView`：最能体现 matrix/spatial/graph 混合 view。
6. `Flow/DSE Memory`：最能体现 stage/version/provenance 和 AI agent。

每个工具接入必须满足：

- full build view；
- delta update view；
- validator against original iEDA full result；
- query workload；
- runtime / memory / DB size / correctness 指标。

## 5. 最小实现路线

Phase A：query correctness

- 保持 SQLite backend。
- 增加 child/root predicate query 封装。
- 为 iPL/iDRC 各写一个 domain query wrapper。

Phase B：dirty update correctness

- 增加 `DirtyObject`、`DirtyRegion`、`DirtyNet`。
- 先不做 partial SQL update，只做 dirty full refresh of affected root rows。
- 用 full recompute 做 oracle。

Phase C：partial update

- 为 vector child table 增加 append/delete/update API。
- 给 `UPDATE` 增加非整图替换路径。
- 统计 write amplification。

Phase D：hybrid view

- 增加 memory tile index 和 net adjacency cache。
- SQLite 继续保存 source of truth。
- 每个 view 都有 rebuild、deltaUpdate、validate。

Phase E：research benchmark

- 收集 iEDA 真实 flow trace。
- 多 design、多 dirty ratio、多工具 workload。
- 输出 EDA 论文和 DB 论文两套评价口径。

