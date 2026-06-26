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
- 需要稳定 DEF roundtrip 的 root list 增加 `_order_sd`，read path 必须 `ORDER BY "_order_sd"`。
- computed fields 不入库；read path 按原始 parser 语义重新计算或重建。
- 每启用一个 `readIdbXXX/writeIdbXXX`，必须同步 schema/init、DEF callback、测试 SQL 和文档。

## Per-Class Checklist

对每个 iEDA class `T`，按以下顺序检查：

1. 找到 `def_write.cpp` 中对应的 `write_xxx()`，一个 `T` 可能对应多个 writer。
2. 列出 writer 实际输出到 DEF 的字段，包括嵌套 class 和 vector。
3. 找到 `def_read.cpp` 中对应的 callback / `parse_xxx()`，一个 `T` 可能对应多个 parser。
4. 列出 parser 从 DEF 文本读取并设置的字段。
5. 区分 DB 读取字段和读后计算字段；计算字段必须说明依赖和计算方式。
6. 检查 `edadb_idb_schema.h` 中 `TABLE4CLASS` / `TABLE4SHADOW` 是否覆盖上述字段。
7. 判断是否需要 shadow：优先直接映射；只有 direct mapping 无法表达 PK、vector ownership、引用查找、重建视图时才定义 shadow。
8. 检查 `edadb_idb_init.cpp` 中表初始化是否和 schema、write/read 启用范围一致。
9. 检查 `DefReadEdadb::createDbByDef()` 是否只禁用了已由 EDADB 完整恢复的 DEF callbacks。
10. 用 demo roundtrip、DB 表内容和关键对象数量验证。

## Order / Index Policy

对 iDB root list，例如 `IdbDesign::_region_list` 或 `IdbLayout::_rows`，默认目标是保持原始 `t1, t2, t3` append 顺序。iEDA 原始 parser 通常按 DEF 出现顺序 append，writer 再按 vector 当前顺序输出。

判断原则：

- 不要为了稳定输出对 iDB list 做 name sort；这会改变原始 DEF statement order。
- 先区分使用方式：对象是通过 name lookup 找到，还是通过 vector traversal / index 使用。
- 如果代码使用 `front()`、`operator[]`、row id、按 vector 遍历生成内部 id，则 root list 顺序更重要。
- 如果对象主要通过 name lookup 引用，EDA 语义通常不依赖顺序，但 DEF roundtrip 仍需要保持 insertion order。
- 如果 EDADB API/DB backend 不能明确保证 `insertVector()` / `readAll` 顺序稳定，root list 必须补显式 `_order`。
- 成员 vector child 的顺序由对应 shadow/EDADB vector 机制处理；本表只判断 root list 顺序。

已 review 类的顺序需求：

| Class / Root List | Preserve Order? | Usage Basis | Current State |
| --- | --- | --- | --- |
| `IdbDesign` | No | singleton object | No root list order. |
| `IdbDie` | No for root; yes for points | singleton root; point order is geometry/DEF semantics | Root no order; point order already handled by nested shadow `_vec_idx`. |
| `IdbRowList` | Yes | vector traversal plus `front()` / index-derived row logic | Implemented with `_order_sd` and ordered read. |
| `IdbTrackGridList` | Yes | vector traversal, layer back links, DEF writer order | Implemented with `primary_key` as identity, `_order_sd` as list order, and ordered read. |
| `IdbGCellGridList` | Yes | vector traversal and DEF writer order | Implemented with `primary_key` as identity, `_order_sd` as list order, and ordered read. |
| `IdbRegionList` | Yes | name lookup for references, but vector traversal assigns internal order/id and DEF writer order | Implemented with `_name_sd` as identity, `_order_sd` as list order, and ordered read. |

## Current Progress

| Area | Status | Notes |
| --- | --- | --- |
| Design / Units / BusBitChars | Done | Direct `IdbDesign`; only DEF-visible fields are persisted. |
| Die | Done | Shadow root plus ordered point vector. |
| Row | Done | `Shadow<IdbRow>`; `_name_sd` identity, `_order_sd` root order, site cloned from LEF. |
| TrackGrid | Done | `primary_key` identity, `_order_sd` root order, layer names rebuild LEF/routing references. |
| GCellGrid | Done | `primary_key` identity, `_order_sd` root order, DEF four-field view. |
| Via | Done | Direct root object; member-level via master/layer-shape shadows handle rebuild. |
| Instance / Pin | Done | EDADB roundtrip enabled and regression-covered. |
| Blockage | Done | EDADB roundtrip enabled and regression-covered; detailed review doc still missing. |
| Region | Done | `_name_sd` identity, `_order_sd` root order, boundary vector preserved. |
| Slot | Done | `primary_key` identity, `_order_sd` root order, rectangle vector preserved. |
| Group | Done | `_group_name_sd` identity, `_order_sd` root order, member vector order preserved. |
| Fill | Implemented | Regression-covered by `aux_optional`; detailed docs/order audit still pending. |
| SpecialNet / Net | Implemented | Regression-covered by `default_ipl`, `aux_optional`, and `routed_irt`; detailed docs/order audit still pending. |

## Output Template

每个类的 review 文档保持这个结构：

- Scope: 当前类覆盖哪些 DEF section / callback / writer。
- Original Write Semantics: 原始 writer 输出哪些字段。
- Original Read Semantics: 原始 parser 如何重建对象。
- EDADB Schema: 当前 DB 中保存哪些 class/member。
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
- `06_idb_region.md`: `IdbRegion` for `REGIONS`, including name/type and boundary rectangle vector persistence.
- `07_idb_slot.md`: `IdbSlot` for `SLOTS`, including layer name, rectangle vector, and explicit root order.
- `08_idb_group.md`: `IdbGroup` for `GROUPS`, including region/member name references and explicit root/member order.
- `todo.md`: root list order guarantees that still need implementation or verification.

## Suggested Next Steps

1. Add review docs for already-implemented classes in DEF write order: `IdbBlockage`, `IdbFill`, `IdbSpecialNet`, `IdbNet`.
2. For each root list, decide whether order needs explicit `_order_sd`; do not rely on EDADB read-all physical order.
3. Continue with `IdbFill`, because it is covered by `aux_optional` and smaller than routed nets.
4. After each class: update schema/read/write if needed, extend SQL assertions, run demo and regression, then commit.
