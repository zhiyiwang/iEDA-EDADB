# IdbGCellGrid EDADB Adapter Review

## Scope

`IdbGCellGrid` 对应 DEF 的 `GCELLGRID` statements。

- Write: `DefWrite::write_gcell_grid()`
- Read: `gcellGridCallback()` / `DefRead::parse_gcell_grid()`
- EDADB Write: `DefWriteEdadb::writeIdbGCellGrid()`
- EDADB Read: `DefReadEdadb::readIdbGCellGrid()`

## Original Write Semantics

原始 `DefWrite::write_gcell_grid()` 对每个 gcell grid 输出：

- direction: `gcell_grid->_direction`，`kDirectionX` 输出 `X`，否则输出 `Y`
- start: `gcell_grid->_start`
- DO count: `gcell_grid->_num`
- STEP space: `gcell_grid->_space`

如果列表为空，原始 writer 打印 `No GCELLGRID...` 并返回 `kDbFail`；但原始 top-level writer 没检查这个返回值，所以空列表不是整体 DEF 写失败条件。

## Original Read Semantics

原始 `DefRead::parse_gcell_grid()`：

- 在 `layout->get_gcell_grid_list()` 中创建 `IdbGCellGrid`。
- 由 `def_grid->macro()[0]` 设置 direction：`X` -> `kDirectionX`，否则 `kDirectionY`。
- 设置 num、start、space。
- 不依赖 LEF layer，不重建额外引用关系。

## EDADB Schema

当前 schema：

```cpp
TABLE4CLASS(idb::IdbGCellGrid, "iGCellGrid", (_direction, _start, _num, _space));
```

保存字段正好覆盖原始 DEF writer/read 需要的四个字段。

## Why No GCellGrid Shadow

当前不需要 `Shadow<IdbGCellGrid>`：

- `IdbGCellGrid` 没有 vector child。
- 没有 `IdbLayer*` / via rule / master 等非 owning 引用需要 name lookup。
- direct mapping 能完整表达 `GCELLGRID` DEF 语义。
- `Cpp2SqlTypeTrait<IdbGCellGrid>::hasPrimKey = false` 后，可作为无主键简单记录表保存。

## EDADB Write Path

当前 `writeIdbGCellGrid()`：

- 从 `layout->get_gcell_grid_list()` 取得 list。
- 空列表时直接返回 `kDbSuccess`，避免 `writeChip2Edadb()` 因原始 writer 的局部 `kDbFail` 语义中断整个 EDADB 写流程。
- 非空时使用 `edadb::insertVector<IdbGCellGrid>()` 写入。

这与原始输出字段一致；空列表返回值是 adapter 层必要调整，因为 EDADB write dispatcher 会检查每个 `writeIdbXXX()` 的返回值。

## EDADB Read Path

当前 `readIdbGCellGrid()`：

- `gcell_grid_list->clear()` 清空旧数据。
- 循环读取 `IdbGCellGrid`。
- 直接加入 layout 的 gcell grid list。

这和原始 parser 的对象重建语义一致：没有额外 computed field 或外部引用要恢复。

## Computed Fields

`IdbGCellGrid` 当前没有 read 后计算字段：

- direction/start/num/space 全部来自 DEF/EDADB。
- 不需要查 LEF layer。
- 不需要反向引用重建。

## Order / Index

`IdbGCellGridList` 需要保持原始 append 顺序，且不应该按字段或方向排序。

依据：

- 原始 parser 按 DEF 出现顺序 append。
- 原始 writer 按 list 当前顺序输出。
- iEDA/iRT 主要通过 vector traversal 使用 GCell grid，没有 name lookup。

当前状态：未显式实现 root order；direct mapping 依赖 EDADB `insertVector()` / `readAll` 的读回顺序稳定。regression routed case 验证了当前 EDADB API 下非空路径顺序稳定；若未来 DB backend 不保证顺序，应给 `IdbGCellGrid` 增加 `_order` 或 shadow。

## Tests

- demo `sky130_gcd` 当前没有 `GCELLGRID`，覆盖空列表路径：`writeIdbGCellGrid insert gcell_grid_count=0`，`readIdbGCellGrid restored gcell_grid_count=0`。
- `src/database/edadb/test/run_idb_roundtrip_regression.sh` 中 routed case 覆盖非空路径，并检查 `iGCellGrid` count 和字段组合。

## Risks / TODO

- demo 只覆盖空列表，正向字段持久化应以 regression 的 routed case 为准。
- 若未来 EDADB 对无主键重复简单表引入顺序约束，需要确保 `iGCellGrid` 读写顺序稳定。
