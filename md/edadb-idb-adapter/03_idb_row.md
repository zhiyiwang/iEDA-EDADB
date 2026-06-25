# IdbRow EDADB Adapter Review

## Scope

`IdbRow` 对应 DEF 的 `ROW` statements。

- Write: `DefWrite::write_row()`
- Read: `rowCallback()` / `DefRead::parse_row()`
- EDADB Write: `DefWriteEdadb::writeIdbRow()`
- EDADB Read: `DefReadEdadb::readIdbRow()`

## Original Write Semantics

原始 `DefWrite::write_row()` 对每个 row 输出：

- row name: `row->_name`
- site name: `row->_site->_name`
- origin: `row->_original_coordinate->_x/_y`
- orient: `row->_site->_orient`
- DO/BY: `row->_row_num_x`, `row->_row_num_y`
- STEP: `row->_step_x`, `row->_step_y`

不直接输出：

- row bounding box。
- site width/height/class/symmetry 等 LEF site 属性。
- row `_orient` 字段本身；writer 使用的是 `row->get_site()->get_orient()`。

## Original Read Semantics

原始 `DefRead::parse_row()`：

- `rows->add_row_list(nullptr)` 创建 active row。
- 设置 row name。
- 设置 original coordinate。
- 通过 `sites->add_site_list(def_row->macro())` 获取/创建 LEF site placeholder。
- clone site，按 DEF orient 设置 cloned row site orient。
- `row->set_site(row_site)`，并同步 `row->set_orient(row_site->get_orient())`。
- 如果 DEF 有 `DO/BY`，设置 row num；如果有 `STEP`，设置 step。
- 调用 `row->set_bounding_box()`。

## EDADB Schema

当前 schema：

```cpp
TABLE4CLASS(idb::IdbSite, "iSite", (_name, _width, _heigtht, _b_overlap, _site_class, _symmetry, _orient, _type));
TABLE4CLASS(idb::IdbRow, "iRow", (_name, _site, _original_coordinate, _row_num_x, _row_num_y, _step_x, _step_y));
```

当前采用 direct class mapping，不定义 `Shadow<IdbRow>`。

## Why No Row Shadow

当前不需要 `Shadow<IdbRow>`：

- `IdbRow::_name` 是天然 root key，可作为 `iRow` 记录标识。
- row 没有需要额外 owner PK 归属的 vector child。
- direct mapping 已能表达 row 的 DEF 输出字段和 site/origin 嵌套对象。

需要注意：schema 当前保存了完整 `IdbSite`，但 DEF row 语义只需要 site name 和 orient。完整 site 属性更多是 EDADB direct mapping 的副产物，不是 `ROW` roundtrip 的必要字段。

## EDADB Write Path

当前 `writeIdbRow()`：

- 从 `layout->get_rows()` 取得 row list。
- 使用 `edadb::insertVector<IdbRow>(row_vec)` 写入每个 row。

这覆盖了原始 writer 需要的 row name、site name/orient、origin、DO/BY、STEP 字段。

## EDADB Read Path

当前 `readIdbRow()`：

- `rows->reset()` 清空旧 row。
- 循环读取 `IdbRow`。
- 从 EDADB row 中取出 site name 和 site orient。
- 按原始 `parse_row()` 语义调用 `sites->add_site_list(site_name)` 获取/创建 LEF site。
- clone site，设置 orient，并挂回 row。
- 设置 row orient，调用 `set_bounding_box()`。
- 加入 `rows`。

这和原始 `parse_row()` 的对象重建语义保持一致：最终 row site 是 layout site 的 clone，而不是直接依赖 EDADB 读出的 site pointer 作为最终对象。

## Computed Fields

这些字段不需要作为 row DB 字段直接保存：

- row bounding box：读回后由 `set_bounding_box()` 计算。
- site LEF geometry/class/symmetry：正常流程从 LEF layout site 获得。

计算方式：

- `set_bounding_box()` 使用 original coordinate、`row_num_x`、`step_x` 和 row site height 计算 bbox。

## Risks / TODO

当前实现总体贴近原始 DEF read/write 语义，但有两个后续可优化点：

- `iSite` 当前保存完整 site 属性，超过 `ROW` DEF 语义所需；如果后续要更严格贴近 DEF，可用 row shadow 只保存 site name + orient。
- `IdbRow::_orient` 没在 schema 中单独保存；当前读回由 site orient 同步设置，符合原始 parser 语义。
