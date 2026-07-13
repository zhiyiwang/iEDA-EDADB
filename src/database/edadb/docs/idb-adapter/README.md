# EDADB iDB Adapter Review Process

本目录记录把 iEDA DEF read/write 迁移到 EDADB read/write 时的逐类 review 方法。

## Goal

EDADB adapter 文档的核心目标是：每个 root class 都必须按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 的约束检查，并把检查结论写进对应 `0X_idb_*.md`。

在这个核心目标下，EDADB adapter 不是 dump 完整 C++ 对象，而是贴近 iEDA 原始 DEF 语义：

- 先确认 DEF section 到 iEDA class/root list 的映射，以及 root order 等级 A/B/C/D。
- `writeIdbT()` 对齐 `DefWrite::write_xxx()` 实际输出的 DEF 字段。
- `readIdbT()` 对齐 `DefRead::parse_xxx()` 实际重建的 iDB 状态。
- DB schema 只保存 DEF 语义需要的字段，以及 read 时无法从上下文重新计算的字段。

## Adapter Rules

- 先读原始 `DefWrite::write_xxx()` / `DefRead::parse_xxx()`，再改 EDADB adapter。
- EDADB 表达的是 DEF 语义视图，不一定等于完整 C++ object dump。
- 优先 direct mapping；只有 direct mapping 无法表达 polymorphism、anonymous root identity、non-owning pointer/name-reference rebuild、nested vector owner/order，或 reduced DEF storage view 时才引入 `Shadow<T>`。
- 如果某个成员类型 `T` 已注册 `TABLE4SHADOW(T)` 或 `TABLE4SHADOW_WVEC(T)`，则包含它的 root class 可以继续 direct mapping；EDADB 遍历成员时会自动把 `T` / `T*` 的 store type 改写为 `edadb::Shadow<T>`。
- Shadow 自动转换流程：write 阶段对原始成员指针/对象调用 `toShadow()` 后写 shadow fields；read 阶段先读 shadow fields，再调用 `fromShadow()` 重建原始成员，最后写回 root object。
- 对 root list，identity 和 order 必须分开：不要用 vector order index 当 PK。
- 没有天然 identity 的 root record 用 `primary_key`；有天然 name 的对象用 name 做 PK。
- Primary key 只用于 root identity 或 nested vector-owner storage view；纯 inline/nested scalar value view 必须关闭 PK。例如 `Shadow<IdbViaMasterGenerate>` 只是 `Shadow<IdbViaMaster>::_master_generate_sd`，不是独立 root/vector owner，因此在 `initPrimKeys()` 中关闭 PK；`Shadow<IdbViaMaster>` owns `fixed_layer_shape_list_sd`，保留 EDADB 默认 PK。
- `edadb-idb-dev/no-sort-abcd` 是顺序实验分支：A/B/C/D 四级 root list 均不保存 `_order_sd`，read path 均不使用 root `ORDER BY`。
- `def-ieda-mapping-and-order.md` 的 A/B/C/D 结论仍是 iEDA 使用证据；本分支刻意覆盖该实现策略，用于验证“不保存 root order”的实际影响。
- nested vector 不属于本实验范围，仍使用 EDADB child-vector index、`_vec_idx` 或局部 pin-ref `_order_sd` 保持父对象内部顺序。
- computed fields 不入库；read path 按原始 parser 语义重新计算或重建。
- Roundtrip mapping 必须拆成 `Original DEF Write Flow` 和 `Original DEF Read Flow` 两张表，分别以原始 `DefWrite` / `DefRead` 实际执行顺序为主线，不按 shadow class 或 DB 列顺序组织。
- 两张 roundtrip 表固定为三列：原始执行顺序、EDADB `write/toShadow` 或 `read/fromShadow` 对应、`DEF 域 / iDB 变量 / EDADB 域`。分支、fallback、name lookup、computed field 和 parser-only/writer-only 差异必须在对应执行步骤中写明。
- 每个 root 文档必须说明 child storage view：哪些子节点 direct mapping，哪些子节点 shadow，哪些运行时 pointer/cache 不入库以及如何重建。
- 每启用一个 `readIdbXXX/writeIdbXXX`，必须同步 schema/init、DEF callback、测试 SQL 和文档。
- 未被任何 enabled adapter 读写、注册或验证的 schema macro 必须休眠并标 `EDADB_TODO`，不能因为原始类存在就默认建表。

## Per-Class Checklist

对每个 iEDA class `T`，按以下顺序检查：

1. 找到 `def_write.cpp` 中对应的 `write_xxx()`，一个 `T` 可能对应多个 writer。
2. 列出 writer 实际输出到 DEF 的字段，包括嵌套 class 和 vector。
3. 找到 `def_read.cpp` 中对应的 callback / `parse_xxx()`，一个 `T` 可能对应多个 parser。
4. 列出 parser 从 DEF 文本读取并设置的字段。
5. 区分 DB 读取字段和读后计算字段；计算字段必须说明依赖和计算方式。
6. 检查 `def-ieda-mapping-and-order.md` 中对应 DEF section 的 root order 等级，并在类文档中记录该约束。
7. 检查 `edadb_idb_schema.h` 中 `TABLE4CLASS` / `TABLE4SHADOW` 是否覆盖上述字段，并记录宏定义代码位置。
8. 判断是否需要 shadow：优先直接映射；只有 direct mapping 无法表达 PK、vector ownership、引用查找、重建视图时才定义 shadow。
9. 检查 `edadb_idb_init.cpp` 中 primary-key 设置和表初始化是否和 schema、write/read 启用范围一致，并记录代码位置。
10. 任何 helper/child class table macro 如果没有 enabled adapter 使用，必须休眠；文档要说明为什么不需要建表。
11. 检查 `DefReadEdadb::createDbByDef()` 是否只禁用了已由 EDADB 完整恢复的 DEF callbacks。
12. 用 demo roundtrip、DB 表内容和关键对象数量验证。

## Order / Index Policy

本分支不保存任何 A/B/C/D root list 的原始 append 顺序。iEDA 原始 parser/writer 和点工具证据仍按 `def-ieda-mapping-and-order.md` 记录，但不转化为 root `_order_sd`。

判断原则：

- 不要为了稳定输出对 iDB list 做 name sort；这会改变原始 DEF statement order。
- 先区分使用方式：对象是通过 name lookup 找到，还是通过 vector traversal / index 使用。
- 如果代码使用 `front()`、`operator[]`、row id、按 vector 遍历生成内部 id，则 root list 顺序更重要。
- 如果对象主要通过 name lookup 引用，EDA 语义通常不依赖顺序；Level D 的 raw text order 差异优先交给 normalized diff，除非该类文档明确列为 raw-roundtrip exception。
- EDADB API/DB backend 不保证无 `ORDER BY` 查询的返回顺序；本分支接受 root list 顺序变化，并在 DEF 比较时对 A/B/C/D root record 做 normalized diff。
- 成员 vector child 的顺序由对应 shadow/EDADB vector 机制处理；本表只判断 root list 顺序。

已 review 类的顺序需求：

| Class / Root List | Preserve Order? | Usage Basis | Current State |
| --- | --- | --- | --- |
| `IdbDesign` | No | singleton object | No root list order. |
| `IdbDie` | No for root; yes for points | singleton root; point order is geometry/DEF semantics | Root no order; point order already handled by nested shadow `_vec_idx`. |
| `IdbRowList` | No in this branch | Level B evidence retained for comparison | `_name_sd` identity; no `_order_sd`; read-all without root ordering. |
| `IdbTrackGridList` | No | Level D; no point-tool root index/order dependency found | `primary_key` identity; no `_order_sd`; nested layer-name vector preserves order. |
| `IdbGCellGridList` | No | Level D; no point-tool root index/order dependency found | Direct no-shadow/no-order mapping; normalized diff handles root order-only differences. |
| `IdbRegionList` | No | Level D; references are name-based and no point-tool root index/order dependency found | Direct no-shadow/no-order mapping; normalized diff handles root order-only differences. |
| `IdbSlotList` | No | Level D; anonymous records still need identity, not order | `primary_key` identity; no `_order_sd`; rect vector uses `Shadow<IdbRect>::_vec_idx`. |
| `IdbBlockageList` | No | Level D; no point-tool root index/order dependency found | Synthetic `primary_key` identity; no `_order_sd`; rect vector uses `Shadow<IdbRect>::_vec_idx`. |
| `IdbGroupList` | No | Level D; references are name-based and no point-tool root index/order dependency found | `_group_name_sd` identity; no `_order_sd`; member vector preserves order. |
| `IdbFillList` | No | Level D; no point-tool root index/order dependency found | `primary_key` identity; no `_order_sd`; rect/coordinate vectors preserve order. |
| `IdbSpecialNetList` | No | Level D; PDN tools resolve nets by name, no root index/order dependency found | `_net_name_sd` identity; no root `_order_sd`; pin refs and wire/segment/point vectors preserve order. |
| `IdbInstanceList` | No in this branch | Level C evidence retained for comparison | `_name_sd` identity; no `_order_sd`; read-all without root ordering. |
| `IdbPins` | No in this branch | Level B evidence retained for comparison | `_pin_name_sd` identity; no `_order_sd`; nested port/layer/rect order preserved. |
| `IdbNetList` | No in this branch | Level A evidence retained for comparison | `_net_name_sd` identity; no root `_order_sd`; nested pin/wire/segment/point order preserved. |

## Current Progress

| Area | Status | Notes |
| --- | --- | --- |
| Design / Units / BusBitChars | Done | Direct `IdbDesign`; only DEF-visible fields are persisted. |
| Die | Done | Shadow root plus ordered point vector. |
| Row | Done | `Shadow<IdbRow>`; `_name_sd` identity, no root order, site cloned from LEF. |
| TrackGrid | Done | Level D root order; `primary_key` identity, no `_order_sd`, layer names rebuild LEF/routing references. |
| GCellGrid | Done | Direct `IdbGCellGrid`; no shadow, no `_order_sd`, DEF four-field view. |
| Via | Done | Direct root object; member-level via master/layer-shape shadows handle rebuild. |
| Instance | Done | `_name_sd` identity, no root order, master/region/layer references rebuilt by name. |
| Pin | Done | `_pin_name_sd` identity, no root order, port/layer shape relative geometry preserved. |
| Blockage | Done | Level D root order; `primary_key` identity, no `_order_sd`, layer/instance references rebuilt by name. |
| Region | Done | Direct `IdbRegion`; `_name` identity, no `_order_sd`, boundary vector preserved. |
| Slot | Done | `primary_key` identity for anonymous root records, no root order, rectangle vector uses `Shadow<IdbRect>::_vec_idx`. |
| Group | Done | Level D root order; `_group_name_sd` identity, no `_order_sd`, member vector order preserved. |
| Fill | Done | Level D root order; `primary_key` identity, no `_order_sd`, layer/via references rebuilt by name. |
| SpecialNet | Done | Level D root order; `_net_name_sd` identity, no root `_order_sd`, pin-string/explicit pin refs plus via/rect/point segment branches covered. |
| Net | Done | `_net_name_sd` identity, no root order, pin/wire/segment vectors preserved. |

## Output Template

每个类的 review 文档保持这个结构：

- Scope: 当前类覆盖哪些 DEF section / callback / writer。
- Original Write Semantics: 原始 writer 输出哪些字段。
- Original Read Semantics: 原始 parser 如何重建对象。
- EDADB Schema: 当前 DB 中保存哪些 class/member。
- Schema / Init: 记录 `TABLE4CLASS` / `TABLE4SHADOW` 宏、`initPrimKeys()`、`EDADB_INIT_TABLE()` 的代码位置；同时说明 PK 是否启用。
- Original DEF Write/Read Roundtrip Mapping: 使用两张三列表；write 表按原始 `DefWrite` 执行顺序，read 表按原始 `DefRead` 执行顺序，逐步标明 EDADB 实现、DEF tag、iDB 成员和 EDADB 字段。
- Child Storage View: root 下有哪些子节点、direct/shadow 选择、为什么不用原始类。
- EDADB Write Path: `writeIdbT()` 是否贴近原始 writer。
- EDADB Read Path: `readIdbT()` 是否贴近原始 parser。
- Computed Fields: 哪些字段不入库，如何计算。
- Risks / TODO: 与原始语义不一致或 ownership 风险。
- Order / Index: root list 是否需要保持顺序、依据是什么、当前是否已显式实现。

## Class Review Index

- `01_idb_design.md`: `IdbDesign`, `IdbUnits`, `IdbBusBitChars` for `VERSION`, `BUSBITCHARS`, `DESIGN`, `UNITS`.
- `02_idb_die.md`: `IdbDie` for `DIEAREA`, including point vector persistence and bounding-box rebuild.
- `03_idb_row.md`: `IdbRow` for `ROW`, including site clone/rebuild, origin, DO/BY, STEP, and bbox recomputation.
- `04_idb_track_grid.md`: `IdbTrackGrid` for `TRACKS`, including track fields, layer-name references, and routing-layer backlink rebuild.
- `05_idb_gcell_grid.md`: `IdbGCellGrid` for `GCELLGRID`, including direct four-field mapping and empty-list adapter semantics.
- `06_idb_via.md`: `IdbVia` for `VIAS`, including direct root storage and via-master/layer-shape shadow rebuild.
- `07_idb_instance.md`: `IdbInstance` for `COMPONENTS`, including component fields, name references, and no-sort root policy.
- `08_idb_pin.md`: `IdbPin` for `PINS`, including IO term, port/layer shape storage, computed absolute geometry, and no-sort root policy.
- `09_idb_blockage.md`: `IdbBlockage` for `BLOCKAGES`, including routing/placement polymorphism, rect vector, name references, and Level D root-order policy.
- `10_idb_region.md`: `IdbRegion` for `REGIONS`, including name/type and boundary rectangle vector persistence.
- `11_idb_slot.md`: `IdbSlot` for `SLOTS`, including layer name, rectangle vector, and anonymous root identity.
- `12_idb_group.md`: `IdbGroup` for `GROUPS`, including region/member name references, Level D root-order policy, and member order.
- `13_idb_fill.md`: `IdbFill` for `FILLS`, including layer/via typed storage, geometry vectors, and Level D root-order policy.
- `14_idb_special_net.md`: `IdbSpecialNet` for `SPECIALNETS`, including pin refs, special wires, segments, geometry, and Level D root-order policy.
- `15_idb_net.md`: `IdbNet` for `NETS`, including pin refs, regular wires, segments, geometry, and no-sort root policy.
- `todo.md`: root list order guarantees that still need implementation or verification.

## Suggested Next Steps

1. Keep each new root adapter aligned with original `DefWrite` / `DefRead` semantics.
2. In this experiment branch, keep every root list free of `_order_sd`; preserve only nested vector order and use A/B/C/D normalized diff for root-order-only changes.
3. After each class: update schema/read/write if needed, extend SQL assertions, run demo and regression, then commit.
