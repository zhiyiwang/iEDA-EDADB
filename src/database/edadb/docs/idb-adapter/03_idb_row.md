# IdbRow EDADB Adapter Review

## Scope And Constraints

`IdbRow` 对应 DEF `ROW` statement：

- Root container：`IdbLayout::_rows -> IdbRows::_row_list`
- DEF source：name、site name/orient、origin、`DO/BY/STEP`
- Rebuilt state：row-local site、row orient、bounding box

本实现按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 检查：

- `IdbRows::_row_list` 是 Level B；canonical adapter 应显式恢复 root append 顺序。
- 本实验分支故意不保存 Level-B root order，用于验证去除 tuple order 后的行为；这是已知语义偏离，不改变 nested-order 规则。
- `IdbRow` 没有需要持久化的 owning nested vector。
- LEF site 完整对象和 row bounding box 不是 DEF source，不进入 EDADB。

## EDADB Schema

```cpp
TABLE4SHADOW(idb::IdbRow);
TABLE4CLASS(edadb::Shadow<idb::IdbRow>, "iRow",
            (_name_sd, _site_name_sd, _site_orient_sd,
             _origin_x_sd, _origin_y_sd, _row_num_x_sd, _row_num_y_sd,
             _step_x_sd, _step_y_sd));
```

代码位置：

- Dormant `IdbSite` mapping：`src/database/edadb/idb/edadb_idb_schema.h:41-47`
- Row schema：`src/database/edadb/idb/edadb_idb_schema.h:49-51`
- PK audit：`src/database/edadb/idb/edadb_idb_init.cpp:21-31`
- Root table registration：`src/database/edadb/idb/edadb_idb_init.cpp:69-78`
- Shadow implementation：`src/database/edadb/idb/shadow/shadow_idb_row.h:15-72`

Primary-key / order 结论：

- `_name_sd` 是自然 identity 和 `iRow` primary key。
- `iRow` 不包含 `_order_sd`；DB 不保存 `IdbRows::_row_list` index。
- `initPrimKeys()` 不覆盖 Row 的默认 root PK 行为。
- `IdbSite` mapping 未启用，`initAllTables()` 不创建 `iSite`。

## Why Shadow Is Required

Direct `IdbRow` mapping 不适合当前存储视图：

- DEF 只引用 site name/orient，不应持久化完整 LEF `IdbSite`。
- origin 可直接 flatten 为 x/y；bbox 必须在 read 后计算。

因此 shadow 用于保存最小 DEF source 和 site name reference；本分支不利用 shadow 增加 root order。

## Original DEF Write Mapping

原始入口为 `DefWrite::writeChip()` 中的 `write_row()`，见
`src/database/manager/builder/def_builder/def_write.cpp:205-214`。

| Original writer brace | DEF output | EDADB correspondence | Stored source |
| --- | --- | --- | --- |
| 获取 `layout->get_rows()` 并检查，`def_write.cpp:437-444` | `ROW` collection | `writeIdbRow()` 取得同一 root list，`def_write_edadb.cpp:217-225` | `iRow` rows |
| 按 `get_row_list()` 当前顺序遍历，`def_write.cpp:446` | root record order | 遍历同一 vector 并调用标准 `toShadow(obj)`，`def_write_edadb.cpp:226-234` | root order 不入库 |
| 从 row-local site 取 name/orient，`def_write.cpp:447-451` | `ROW name site ... orient` | `toShadow()` 取 `get_site()->get_name()/get_orient()`，`shadow_idb_row.h:25-27` | `_name_sd/_site_name_sd/_site_orient_sd` |
| 输出 origin 与 `DO/BY/STEP`，`def_write.cpp:449-451` | `x y DO nx BY ny STEP sx sy` | `toShadow()` 保存 origin、row num、step，`shadow_idb_row.h:28-33` | `_origin_*_sd/_row_num_*_sd/_step_*_sd` |

`writeIdbRow()` 检查每次 shadow 转换并传播失败，再 batch insert：
`src/database/manager/builder/def_builder/def_write_edadb.cpp:228-242`。

## Original DEF Read Mapping

原始 callback 在 `src/database/manager/builder/def_builder/def_read.cpp:789-805`，
实际对象重建位于 `parse_row()`：

| Original parser brace | EDADB correspondence | Source / synchronization / calculation |
| --- | --- | --- |
| append 新 row，`def_read.cpp:814-816` | builder reset active rows并执行无 `ORDER BY` 的 root query，`def_read_edadb.cpp:321-345` | DB source：root rows；root order 不恢复 |
| 设置 name 和 origin，`def_read.cpp:818-819` | `fromShadow()` 设置相同字段，`shadow_idb_row.h:56-57` | DB source：name/origin |
| 按 site name lookup、clone，并设置 site/row orient，`def_read.cpp:821-826` | `fromShadow()` 通过全局 helper 取得 layout sites，lookup/clone 后执行相同同步，`shadow_idb_row.h:44-60` | DB source：site name/orient；LEF source：site geometry/properties |
| `hasDo()` / `hasDoStep()` 时设置 row num 和 step，`def_read.cpp:828-835` | `fromShadow()` 恢复 writer 已保存的四个 scalar，`shadow_idb_row.h:61-64` | DB source：`DO/BY/STEP` canonical writer view |
| 最后调用 `set_bounding_box()`，`def_read.cpp:837` | `fromShadow()` 最后调用同一函数，`shadow_idb_row.h:66` | derived bbox，不存储 |
| callback 返回后 row 已在 list 中 | builder 仅检查 `fromShadow()`，然后 append row，`def_read_edadb.cpp:349-358` | builder 只负责编排和 ownership handoff |

原始 parser 的 `DO/BY/STEP` 有 presence branch，但原始 writer 总是输出这些字段。
EDADB 保存的是 writer 的 canonical output view，因此 read 时无条件恢复四个 scalar；当前 schema
不表达“tag 缺失”状态，但 paired write/read 语义与原始 writer → parser 一致。

## Site Ownership And Computed State

- `toShadow()` 只保存 site name/orient，不保存 width、height、class、symmetry 等 LEF 属性。
- `fromShadow()` 使用 `EdadbIdbHelper::getIdbLayout()` 获取当前 active LEF layout。
- `sites->add_site_list(name)` 与原始 `parse_row()` 一致；正常流程复用已读入的 LEF site。
- `clone()` 创建 row-local site，`IdbRow::set_site()` 接管并删除构造时的默认 site，见 `IdbRow.h:69-76`。
- row `_orient` 从 cloned site orient 同步，不单独存储。
- bbox 使用 origin、`row_num_x`、`step_x` 和 site height 计算，见 `IdbRow.cpp:86-92`。

## Order / Index

Canonical 约束认为 `IdbRows::_row_list` 必须保持原始 append 顺序：

- iTO 直接读取 `[0]` 的 site width/height：`src/operation/iTO/source/data_manager/data_manager.cpp:78-89`。
- iPDN 用遍历 index 的奇偶决定 row 上生成 power 还是 ground follow-pin，重排会改变物理连接分配：`src/operation/iPDN/source/module/pdn_plan/pdn_plan.cpp:195-215`。
- iFP 把 vector index 保存为 tapcell region index，后续按奇偶选择 cell master；重排会改变 tapcell 分配：`src/operation/iFP/source/module/tap_cell/tapcell.cpp:85-88`、`tapcell.cpp:126-135`、`tapcell.cpp:240-265`。
- iPNP 用偶/奇 index 分别选择 VDD/VSS rows，并直接索引对应坐标：`src/operation/iPNP/source/module/synthesis/PowerRouter.cpp:301-328`。

本实验分支实现：

- Write 不保存 root index；schema 无 `_order_sd`。
- Read 使用无 `ORDER BY` 的 `makeReadAllOp()`，见 `def_read_edadb.cpp:329-336`。
- `rows->add_row_list(row)` 仍按 DB 返回顺序 `emplace_back`，见 `def_read_edadb.cpp:345-354`。
- `_name_sd` 仍只表达 identity/PK；所有对象字段和计算语义不变。
- DEF 比较仅对完整 Row root record 做 ABCD-mode 归一化；不存在 nested row vector 被排序。

## Validation

回归位置：`src/database/edadb/test/run_idb_roundtrip_regression.sh`。

- SQL 断言 `iRow` 不存在 `_order_sd`，`_name_sd` 仍为 PK。
- 测试按 `rowid DESC` 重插 `iRow`，证明 read 不依赖原插入顺序。
- 每个 case 先保留 raw diff，再用 ABCD root normalizer 比较完整 Row records；record 内部不归一化。
- Slot 及所有 nested `_vec_idx` / child `_order_sd` 测试保持不变。

验证结果：

- `cmake --build build -j40 --target db_edadb def_builder iEDA`：通过。
- `OUT_DIR=/tmp/iedadb_row_convergence bash src/database/edadb/test/run_idb_roundtrip_regression.sh`：全部 case 通过。
- sky130 fixture：39 rows；DB 物理顺序逆置后 raw root order 可不同，ABCD normalized DEF 等价。

## Conclusion

当前实验 Row adapter 以 `_name_sd` 表达 identity，不存 Level-B root order；site 仍按 name 从
LEF clone，row orient 与 bbox 按原始 `parse_row()` 顺序重建。该分支用于量化 root order
缺失的影响，不代表 canonical Level-B 建议被取消。
