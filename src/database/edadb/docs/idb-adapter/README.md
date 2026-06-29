# EDADB iDB Adapter Review Process

本目录记录把 iEDA DEF read/write 逐类迁移到 EDADB read/write 的 review 方法。

## Demo Branch Scope

`demo` 分支只保留当前进度汇报中已经完成并验证的 EDADB adapter：

- `IdbDesign`
- `IdbDie`
- `IdbRow`
- `IdbTrackGrid`
- `IdbGCellGrid`
- `IdbRegion`
- `IdbSlot`

其它 DEF object family 在本分支视为未完成：EDADB write/read 调用不启用，`DefReadEdadb::createDbByDef()` 保留原始 DEF parser callbacks，让对象继续从 DEF 文本读回。

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
- 每个 root 文档必须说明 child storage view：哪些子节点 direct mapping，哪些子节点 shadow，哪些运行时 pointer/cache 不入库以及如何重建。
- 每启用一个 `readIdbXXX/writeIdbXXX`，必须同步 schema/init、DEF callback、测试 SQL 和文档。

## Per-Class Checklist

1. 找到 `def_write.cpp` 中对应的 `write_xxx()`，列出实际输出到 DEF 的字段。
2. 找到 `def_read.cpp` 中对应的 callback / `parse_xxx()`，列出 parser 设置的字段。
3. 区分 DB 读取字段和读后计算字段；计算字段必须说明依赖和计算方式。
4. 检查 `edadb_idb_schema.h` 中 `TABLE4CLASS` / `TABLE4SHADOW` 是否覆盖上述字段。
5. 判断是否需要 shadow：优先直接映射；只有 direct mapping 无法表达 PK、vector ownership、引用查找、重建视图时才定义 shadow。
6. 检查 `edadb_idb_init.cpp` 中表初始化是否和 schema、write/read 启用范围一致。
7. 检查 `DefReadEdadb::createDbByDef()` 是否只禁用了已由 EDADB 完整恢复的 DEF callbacks。
8. 用 demo roundtrip、DB 表内容和关键对象数量验证。

## Order / Index Policy

对 iDB root list，默认目标是保持原始 DEF parser append 顺序。iEDA 原始 parser 通常按 DEF 出现顺序 append，writer 再按 vector 当前顺序输出。

- 不要为了稳定输出对 iDB list 做 name sort；这会改变原始 DEF statement order。
- 先区分使用方式：对象是通过 name lookup 找到，还是通过 vector traversal / index 使用。
- 如果代码使用 `front()`、`operator[]`、row id、按 vector 遍历生成内部 id，则 root list 顺序更重要。
- 如果对象主要通过 name lookup 引用，EDA 语义通常不依赖顺序，但 DEF roundtrip 仍需要保持 insertion order。
- 如果 EDADB API/DB backend 不能明确保证 `insertVector()` / `readAll` 顺序稳定，root list 必须补显式 `_order_sd`。

## Current Progress

| Area | Demo Status | Notes |
| --- | --- | --- |
| Design / Units / BusBitChars | EDADB active | Direct `IdbDesign`; only DEF-visible fields are persisted. |
| Die | EDADB active | Shadow root plus ordered point vector. |
| Row | EDADB active | `Shadow<IdbRow>`; `_name_sd` identity, `_order_sd` root order, site cloned from LEF. |
| TrackGrid | EDADB active | `primary_key` identity, `_order_sd` root order, layer names rebuild LEF/routing references. |
| GCellGrid | EDADB active | `primary_key` identity, `_order_sd` root order, DEF four-field view. |
| Region | EDADB active | `_name_sd` identity, `_order_sd` root order, boundary vector preserved. |
| Slot | EDADB active | `primary_key` identity, `_order_sd` root order, rectangle vector preserved. |
| Via / Instance / Pin / Blockage / Group / Fill / SpecialNet / Net | DEF fallback | EDADB adapter code is not active in `demo`; original DEF read/write path is preserved. |

## Class Review Index

- `01_idb_design.md`: `IdbDesign`, `IdbUnits`, `IdbBusBitChars` for `VERSION`, `BUSBITCHARS`, `DESIGN`, `UNITS`.
- `02_idb_die.md`: `IdbDie` for `DIEAREA`, including point vector persistence and bounding-box rebuild.
- `03_idb_row.md`: `IdbRow` for `ROW`, including site clone/rebuild, origin, DO/BY, STEP, and bbox recomputation.
- `04_idb_track_grid.md`: `IdbTrackGrid` for `TRACKS`, including track fields, layer-name references, and routing-layer backlink rebuild.
- `05_idb_gcell_grid.md`: `IdbGCellGrid` for `GCELLGRID`, including direct four-field mapping and empty-list adapter semantics.
- `10_idb_region.md`: `IdbRegion` for `REGIONS`, including name/type and boundary rectangle vector persistence.
- `11_idb_slot.md`: `IdbSlot` for `SLOTS`, including layer name, rectangle vector, and explicit root order.
- `todo.md`: demo branch active/fallback scope and remaining work.

## Suggested Next Steps

1. Keep `demo` branch focused on progress reporting and stable test evidence.
2. Resume unfinished classes one by one on the development branch: Via, Instance, Pin, Blockage, Group, Fill, SpecialNet, Net.
3. For each new class: update schema/read/write, disable only its matching DEF callbacks, extend SQL assertions, run demo/regression, then commit.
