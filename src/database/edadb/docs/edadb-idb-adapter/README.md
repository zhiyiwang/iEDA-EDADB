# EDADB iDB Adapter Review Process

本目录记录把 iEDA DEF read/write 迁移到 EDADB read/write 时的逐类 review 方法。

## Goal

EDADB adapter 的目标不是 dump 完整 C++ 对象，而是贴近 iEDA 原始 DEF 语义：

- `writeIdbT()` 对齐 `DefWrite::write_xxx()` 实际输出的 DEF 字段。
- `readIdbT()` 对齐 `DefRead::parse_xxx()` 实际重建的 iDB 状态。
- DB schema 只保存 DEF 语义需要的字段，以及 read 时无法从上下文重新计算的字段。

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
| `IdbTrackGridList` | Yes | vector traversal, layer back links, DEF writer order | Partially implemented by shadow `primary_key`; adapter still should verify/enforce read order. |
| `IdbGCellGridList` | Yes | vector traversal and DEF writer order | Not explicitly implemented; currently depends on EDADB read order. |
| `IdbRegionList` | Yes | name lookup for references, but vector traversal assigns internal order/id and DEF writer order | Not explicitly implemented; currently depends on EDADB read order. |

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
- `todo.md`: root list order guarantees that still need implementation or verification.
