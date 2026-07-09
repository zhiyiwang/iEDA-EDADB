# IdbRow EDADB Adapter Review

## Scope

`IdbRow` 对应 DEF 的 `ROW` statements。

- Write: `DefWrite::write_row()`
- Read: `rowCallback()` / `DefRead::parse_row()`
- EDADB Write: `DefWriteEdadb::writeIdbRow()`
- EDADB Read: `DefReadEdadb::readIdbRow()`

本文件按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 的约束检查：

- DEF section 映射：`ROW` statements。
- iEDA root container：`IdbRows::_row_list`。
- root-vector order 等级：Level B，`IdbRows::_row_list` 的 append 顺序会影响物理语义，需要显式保存和恢复。
- nested vector 约束：`IdbRow` 当前 adapter 没有持久化 owning nested vector；site/bbox 都在 read 阶段重建。

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
TABLE4SHADOW(idb::IdbRow);
TABLE4CLASS(edadb::Shadow<idb::IdbRow>, "iRow", (_name_sd, _order_sd, _site_name_sd, _site_orient_sd, _origin_x_sd, _origin_y_sd, _row_num_x_sd, _row_num_y_sd, _step_x_sd, _step_y_sd));
```

Schema / init 代码位置：

- Dormant `IdbSite` table macro: `src/database/edadb/idb/edadb_idb_schema.h:45`
- `TABLE4SHADOW(idb::IdbRow)`: `src/database/edadb/idb/edadb_idb_schema.h:50`
- `iRow` table macro: `src/database/edadb/idb/edadb_idb_schema.h:51`
- Primary-key setup: `src/database/edadb/idb/edadb_idb_init.cpp:21`
- Table registration: `src/database/edadb/idb/edadb_idb_init.cpp:82`

当前采用 `Shadow<IdbRow>`，只保存 DEF row 语义字段和 root list order。

Schema 与 order/index 约束的关系：

- 依据 `src/database/edadb/docs/def-ieda-mapping-and-order.md`，`ROWS` 映射到 `IdbRows::_row_list`，等级为 Level B。
- Level B 的含义是 root vector 顺序会影响物理语义；因此 `ROW` 不能被 normalized diff 排序，也不能依赖 DB 物理读出顺序。
- `_name_sd` 是 `iRow` 的 primary key，表达 row identity；不要用 `_order_sd` 做 PK。
- `_order_sd` 只表达 `IdbRows::_row_list` append 顺序，读回时用于 `ORDER BY "_order_sd"`。
- `IdbSite` table macro 当前休眠；row adapter 不注册/创建 `iSite` 表，也不通过 `iSite` 持久化 row site。

Primary-key audit:

- `initPrimKeys()` 没有关闭 `Shadow<IdbRow>` 的 primary-key 行为；`iRow` 使用 table macro 的第一列 `_name_sd` 作为 root identity。
- `initPrimKeys()` 没有专门处理 `_order_sd`；它不是 PK，只是 order key。
- `initReadDb()` / `initWriteDb()` 都先调用 `initPrimKeys()`，再调用 `initAllTables()`，因此 read/write 的 table metadata 一致。

## Field Mapping To Original DEF Flow

以下按 EDADB shadow 域列出它对应的原始 DEF read/write 代码位置。

- Row identity / root order: `_name_sd`, `_order_sd`
  - Write source: `DefWrite::write_row()` 按 `IdbRowList` 顺序输出 row name，见 `src/database/manager/builder/def_builder/def_write.cpp:437-458`。
  - Read source: `rowCallback()` / `parse_row()` 按 DEF 出现顺序创建 row，见 `src/database/manager/builder/def_builder/def_read.cpp:789-841`。

- Site name and orient: `_site_name_sd`, `_site_orient_sd`
  - Write source: `write_row()` 输出 site name 和 row orient，见 `src/database/manager/builder/def_builder/def_write.cpp:437-458`。
  - Read source: `parse_row()` 从 LEF site clone row-local site，并设置 orient，见 `src/database/manager/builder/def_builder/def_read.cpp:807-841`。

- Origin / DO-BY / STEP: `_origin_x_sd`, `_origin_y_sd`, `_row_num_x_sd`, `_row_num_y_sd`, `_step_x_sd`, `_step_y_sd`
  - Write source: `write_row()` 输出 origin、DO/BY、STEP，见 `src/database/manager/builder/def_builder/def_write.cpp:437-458`。
  - Read source: `parse_row()` 设置 origin、row num、step，见 `src/database/manager/builder/def_builder/def_read.cpp:807-841`。

- Row bounding box: computed from site/origin/count/step
  - Write source: DEF writer 不直接输出 bbox，只输出 ROW scalar fields，见 `src/database/manager/builder/def_builder/def_write.cpp:437-458`。
  - Read source: `parse_row()` 恢复 row fields 后调用 `row->set_bounding_box()`，见 `src/database/manager/builder/def_builder/def_read.cpp:807-841`。

## Child Storage View

`IdbRow` 是 `ROW` root，当前没有持久化 owning child object：

- site 不作为 `IdbSite` child 存库；只保存 `_site_name_sd` 和 `_site_orient_sd`。
- original coordinate 被 flatten 成 `_origin_x_sd/_origin_y_sd`，不单独建 coordinate child。
- row bbox、row-local site clone 都是 read 阶段重建结果。

schema 文件中保留但休眠 `TABLE4CLASS(idb::IdbSite, ...)`；当前 row adapter 不写/读 `iSite` 表。row site 语义必须从 LEF site 按 name clone 后设置 orient，贴近原始 `parse_row()`。

## Why Row Shadow

当前需要 `Shadow<IdbRow>`：

- `IdbRowList` root 顺序需要显式保存，不能依赖 DB 物理顺序。
- DEF row 语义只需要 site name、site orient、origin、DO/BY、STEP，不需要保存完整 `IdbSite`。
- shadow 使用 `_name_sd` 作为 `iRow` primary key，表达 row identity。
- shadow 使用 `_order_sd` 作为 order key，表达 `IdbRowList` append 顺序。
- shadow 用纯标量字段表达 DEF row，避免保存完整 `IdbSite` 对象。

## EDADB Write Path

当前 `writeIdbRow()`：

- Code: `src/database/manager/builder/def_builder/def_write_edadb.cpp:210`
- Row vector access: `src/database/manager/builder/def_builder/def_write_edadb.cpp:218`
- Shadow conversion: `src/database/manager/builder/def_builder/def_write_edadb.cpp:221`
- EDADB insert: `src/database/manager/builder/def_builder/def_write_edadb.cpp:229`
- `Shadow<IdbRow>::toShadow()`: `src/database/edadb/idb/shadow/shadow_idb_row.h:17`

- 从 `layout->get_rows()` 取得 row list。
- 按 `row_vec` 当前顺序构造 `Shadow<IdbRow>`。
- 第 `idx` 个 row 写入 `_order_sd = idx`。
- 使用 `edadb::insertVector<Shadow<IdbRow>>(row_sd_vec)` 写入。

这覆盖了原始 writer 需要的 row name、site name/orient、origin、DO/BY、STEP 字段。

## EDADB Read Path

当前 `readIdbRow()`：

- Code: `src/database/manager/builder/def_builder/def_read_edadb.cpp:319`
- Reset active rows: `src/database/manager/builder/def_builder/def_read_edadb.cpp:328`
- Ordered query: `src/database/manager/builder/def_builder/def_read_edadb.cpp:330`
- EDADB read loop: `src/database/manager/builder/def_builder/def_read_edadb.cpp:337`
- Shadow restore: `src/database/manager/builder/def_builder/def_read_edadb.cpp:349`
- LEF site clone: `src/database/manager/builder/def_builder/def_read_edadb.cpp:351`
- Rebuild bounding box: `src/database/manager/builder/def_builder/def_read_edadb.cpp:363`
- `Shadow<IdbRow>::fromShadow()`: `src/database/edadb/idb/shadow/shadow_idb_row.h:38`

- `rows->reset()` 清空旧 row。
- 使用 `ORDER BY "_order_sd"` 循环读取 `Shadow<IdbRow>`。
- 从 shadow 中取出 site name 和 site orient。
- 按原始 `parse_row()` 语义调用 `sites->add_site_list(site_name)` 获取/创建 LEF site。
- clone site，设置 orient，并挂回 row。
- 设置 row name、origin、DO/BY、STEP、orient，调用 `set_bounding_box()`。
- 加入 `rows`。

这和原始 `parse_row()` 的对象重建语义保持一致：最终 row site 是 layout site 的 clone，而不是 EDADB 持久化的完整 site 对象。

## Computed Fields

这些字段不需要作为 row DB 字段直接保存：

- row bounding box：读回后由 `set_bounding_box()` 计算。
- site LEF geometry/class/symmetry：正常流程从 LEF layout site 获得。

计算方式：

- `set_bounding_box()` 使用 original coordinate、`row_num_x`、`step_x` 和 row site height 计算 bbox。

## Order / Index

`IdbRowList` 需要保持原始 append 顺序，且不应该按 name 排序。

依据：

- 原始 `parse_row()` 按 DEF 出现顺序 append row。
- 原始 `write_row()` 按 `rows->get_row_list()` 当前顺序输出。
- iEDA 后续代码存在 `rows->get_row_list().front()` / `rows->get_row_list()[0]` 获取 site width/height/orient。
- iPL wrapper 按 row vector 遍历顺序分配 row id；部分 placer/legalizer 再按 row index 使用。

当前状态：已显式实现 root order。写入保存 `_order_sd`，读回使用 `ORDER BY "_order_sd"`，不依赖 EDADB/SQLite root table 物理顺序。

对 normalized diff 的影响：

- `ROW` 属于不能排序规避差异的 root list；测试应保持 row root record 顺序。
- 如果 `ROW` 顺序、name、site、origin、orient、DO/BY、STEP 任一项不同，normalized diff 必须失败。
- deeper nested vector 规则当前不涉及 `IdbRow`，因为 adapter 没有持久化 row 的 owning nested vector。

## Risks / TODO

当前实现总体贴近原始 DEF read/write 语义，但需要注意：

- `IdbRow::_orient` 不单独从 active row 读取；shadow 保存 `_site_orient_sd`，读回后同步设置 row site orient 和 row orient。
- `iRow` 表名保持不变，但列名已从 direct mapping 切换为 shadow 字段。
