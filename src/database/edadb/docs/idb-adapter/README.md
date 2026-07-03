# EDADB iDB Adapter Review Process

本目录记录把 iEDA DEF read/write 迁移到 EDADB read/write 时的逐类 review 方法。

## Goal

EDADB adapter 的目标不是 dump 完整 C++ 对象，而是贴近 iEDA 原始 DEF 语义：

- `writeIdbT()` 对齐 `DefWrite::write_xxx()` 实际输出的 DEF 字段。
- `readIdbT()` 对齐 `DefRead::parse_xxx()` 实际重建的 iDB 状态。
- DB schema 只保存 DEF 语义需要的字段，以及 read 时无法从上下文重新计算的字段。

## Adapter Rules

- 先读原始 `DefWrite::write_xxx()` / `DefRead::parse_xxx()`，再改 EDADB adapter。
- EDADB 表达的是 DEF 语义视图，不一定等于完整 C++ object dump。
- 优先 direct mapping；只有需要 PK、root order、name lookup、vector ownership 或重建视图时才引入 `Shadow<T>`。
- 对 root list，identity 和 order 必须分开：不要用 vector order index 当 PK。
- 没有天然 identity 的 root record 用 `primary_key`；有天然 name 的对象用 name 做 PK。
- 只有 iEDA 语义需要保序或明确要求 raw roundtrip 保序的 root list 才增加 `_order_sd`；Level D root list 可优先依赖 normalized diff。
- computed fields 不入库；read path 按原始 parser 语义重新计算或重建。
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

对 iDB root list，例如 `IdbDesign::_region_list` 或 `IdbLayout::_rows`，默认目标是保持原始 `t1, t2, t3` append 顺序。iEDA 原始 parser 通常按 DEF 出现顺序 append，writer 再按 vector 当前顺序输出。

判断原则：

- 不要为了稳定输出对 iDB list 做 name sort；这会改变原始 DEF statement order。
- 先区分使用方式：对象是通过 name lookup 找到，还是通过 vector traversal / index 使用。
- 如果代码使用 `front()`、`operator[]`、row id、按 vector 遍历生成内部 id，则 root list 顺序更重要。
- 如果对象主要通过 name lookup 引用，EDA 语义通常不依赖顺序，但 DEF roundtrip 仍需要保持 insertion order。
- 如果 EDADB API/DB backend 不能明确保证 `insertVector()` / `readAll` 顺序稳定，A/B/C root list 必须补显式 `_order`；Level D 不强制。
- 成员 vector child 的顺序由对应 shadow/EDADB vector 机制处理；本表只判断 root list 顺序。

已 review 类的顺序需求：

| Class / Root List | Preserve Order? | Usage Basis | Current State |
| --- | --- | --- | --- |
| `IdbDesign` | No | singleton object | No root list order. |
| `IdbDie` | No for root; yes for points | singleton root; point order is geometry/DEF semantics | Root no order; point order already handled by nested shadow `_vec_idx`. |
| `IdbRowList` | Yes | vector traversal plus `front()` / index-derived row logic | Implemented with `_order_sd` and ordered read. |
| `IdbTrackGridList` | Yes | vector traversal, layer back links, DEF writer order | Implemented with `primary_key` as identity, `_order_sd` as list order, and ordered read. |
| `IdbGCellGridList` | No | Level D; no point-tool root index/order dependency found | Direct no-shadow/no-order mapping; normalized diff handles root order-only differences. |
| `IdbRegionList` | No | Level D; references are name-based and no point-tool root index/order dependency found | Direct no-shadow/no-order mapping; normalized diff handles root order-only differences. |
| `IdbSlotList` | Yes | anonymous `SLOTS` records should preserve DEF append order for raw roundtrip | `primary_key` identity plus `_order_sd` ordered read; rect vector uses `Shadow<IdbRect>::_vec_idx`. |
| `IdbBlockageList` | Yes | vector traversal and DEF writer order; no natural name identity | Implemented with `primary_key` as identity, `_order_sd` as list order, and ordered read. |

## Current Progress

| Area | Status | Notes |
| --- | --- | --- |
| Design / Units / BusBitChars | Done | Direct `IdbDesign`; only DEF-visible fields are persisted. |
| Die | Done | Shadow root plus ordered point vector. |
| Row | Done | `Shadow<IdbRow>`; `_name_sd` identity, `_order_sd` root order, site cloned from LEF. |
| TrackGrid | Done | `primary_key` identity, `_order_sd` root order, layer names rebuild LEF/routing references. |
| GCellGrid | Done | Direct `IdbGCellGrid`; no shadow, no `_order_sd`, DEF four-field view. |
| Via | Done | Direct root object; member-level via master/layer-shape shadows handle rebuild. |
| Instance | Done | `_name_sd` identity, `_order_sd` root order, master/region/layer references rebuilt by name. |
| Pin | Done | `_pin_name_sd` identity, `_order_sd` root order, port/layer shape relative geometry preserved. |
| Blockage | Done | `primary_key` identity, `_order_sd` root order, layer/instance references rebuilt by name. |
| Region | Done | Direct `IdbRegion`; `_name` identity, no `_order_sd`, boundary vector preserved. |
| Slot | Done | `primary_key` identity for anonymous root records, `_order_sd` root order, rectangle vector uses `Shadow<IdbRect>::_vec_idx`. |
| Group | Done | `_group_name_sd` identity, `_order_sd` root order, member vector order preserved. |
| Fill | Done | `primary_key` identity, `_order_sd` root order, layer/via references rebuilt by name. |
| SpecialNet | Done | `_net_name_sd` identity, `_order_sd` root order, pin/wire/segment vectors preserved. |
| Net | Done | `_net_name_sd` identity, `_order_sd` root order, pin/wire/segment vectors preserved. |

## Output Template

每个类的 review 文档保持这个结构：

- Scope: 当前类覆盖哪些 DEF section / callback / writer。
- Original Write Semantics: 原始 writer 输出哪些字段。
- Original Read Semantics: 原始 parser 如何重建对象。
- EDADB Schema: 当前 DB 中保存哪些 class/member。
- Schema / Init: 记录 `TABLE4CLASS` / `TABLE4SHADOW` 宏、`initPrimKeys()`、`EDADB_INIT_TABLE()` 的代码位置；同时说明 PK 是否启用。
- Field Mapping To Original DEF Flow: 按 DB 域列出对应的 `DefWrite` / `DefRead` 函数和源码行范围。
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
- `07_idb_instance.md`: `IdbInstance` for `COMPONENTS`, including component fields, name references, and explicit root order.
- `08_idb_pin.md`: `IdbPin` for `PINS`, including IO term, port/layer shape storage, computed absolute geometry, and explicit root order.
- `09_idb_blockage.md`: `IdbBlockage` for `BLOCKAGES`, including routing/placement polymorphism, rect vector, name references, and explicit root order.
- `10_idb_region.md`: `IdbRegion` for `REGIONS`, including name/type and boundary rectangle vector persistence.
- `11_idb_slot.md`: `IdbSlot` for `SLOTS`, including layer name, rectangle vector, and anonymous root identity.
- `12_idb_group.md`: `IdbGroup` for `GROUPS`, including region/member name references and explicit root/member order.
- `13_idb_fill.md`: `IdbFill` for `FILLS`, including layer/via typed storage, geometry vectors, and explicit root order.
- `14_idb_special_net.md`: `IdbSpecialNet` for `SPECIALNETS`, including pin refs, special wires, segments, geometry, and explicit root order.
- `15_idb_net.md`: `IdbNet` for `NETS`, including pin refs, regular wires, segments, geometry, and explicit root order.
- `todo.md`: root list order guarantees that still need implementation or verification.

## Suggested Next Steps

1. Keep each new root adapter aligned with original `DefWrite` / `DefRead` semantics.
2. For each root list, decide whether order needs explicit `_order_sd`; A/B/C preserve order, Level D may use normalized diff.
3. After each class: update schema/read/write if needed, extend SQL assertions, run demo and regression, then commit.
