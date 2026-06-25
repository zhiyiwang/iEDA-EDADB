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

## Class Review Index

- `01_idb_design.md`: `IdbDesign`, `IdbUnits`, `IdbBusBitChars` for `VERSION`, `BUSBITCHARS`, `DESIGN`, `UNITS`.
- `02_idb_die.md`: `IdbDie` for `DIEAREA`, including point vector persistence and bounding-box rebuild.
- `03_idb_row.md`: `IdbRow` for `ROW`, including site clone/rebuild, origin, DO/BY, STEP, and bbox recomputation.
