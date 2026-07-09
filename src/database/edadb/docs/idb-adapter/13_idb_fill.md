# IdbFill EDADB Adapter Review

## Scope

`IdbFill` 对应 DEF 的 `FILLS` section。

- Write: `DefWrite::write_fill()`
- Read: `fillsCallback()` / `fillCallback()` / `DefRead::parse_fill()`
- EDADB Write: `DefWriteEdadb::writeIdbFill()`
- EDADB Read: `DefReadEdadb::readIdbFill()`

本文件按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 的约束检查：

- DEF section 映射：`FILLS` section。
- iEDA root container：`IdbFillList::_fill_list`。
- root-vector order 等级：Level D，当前没有发现点工具依赖 `IdbFillList::_fill_list` 的 root index/order。
- anonymous/root identity 约束：fill record 没有 DEF-visible name，layer/via name 也不唯一；因此需要 adapter shadow 提供 `primary_key`。
- nested vector 约束：layer fill rect vector、via fill coordinate vector 是 fill record 内部几何语义，必须随 root record 保持原始顺序，不参与 D-level root sort。

## Original Write Semantics

原始 `DefWrite::write_fill()` 按 `IdbFillList` 顺序输出：

- layer fill: `LAYER <layer_name>` 和 rect list。
- via fill: `VIA <via_name>` 和 coordinate list。

原始 writer 不输出 polygon，也不会输出缺少 layer/via pointer 的 fill record。

空 fill list 时，原始 writer 返回 `kDbFail`，但 top-level DEF writer 不把它当作整体失败。

## Original Read Semantics

原始 `DefRead::parse_fill()`：

- `hasLayer()` 时按 layer name 从 LEF `IdbLayers` 查找 `IdbLayer*`，调用 `fill_list->add_fill_layer(layer)`，再按 DEF 顺序加入 rect。
- `hasVia()` 时优先从 DEF via list 查找 via，找不到再从 LEF via list 查找，clone via 后调用 `fill_list->add_fill_via(via_new)`，再按 DEF 顺序加入 coordinate。
- polygon 当前仍是 TBD，不进入 iDB 状态。

## EDADB Schema

当前 schema：

```cpp
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbFill>, "iFillSD",
                 (primary_key, _type_sd, _layer_name_sd, _via_name_sd),
                 (_rect_list_sd, _coordinate_list_sd));
```

Schema / init 代码位置：

- `iFillSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:115`
- Primary-key setup: `src/database/edadb/idb/edadb_idb_init.cpp:21`
- Table registration: `src/database/edadb/idb/edadb_idb_init.cpp:93`
- Shadow definition: `src/database/edadb/idb/shadow/shadow_idb_fill.h:15`

保存字段覆盖当前 DEF writer/read 需要的 fill type、layer/via name、rect vector 和 coordinate vector。

Schema 与 order/index 约束的关系：

- 依据 `src/database/edadb/docs/def-ieda-mapping-and-order.md`，`FILLS` 映射到 `IdbFillList::_fill_list`，等级为 Level D。
- 当前 adapter 不保存 `_order_sd`，read path 不指定 root order；root order-only 文本差异由 normalized diff 处理。
- `primary_key` 是 anonymous fill root identity，只用于挂接 child vectors，不表达 vector order。
- `_rect_list_sd` 和 `_coordinate_list_sd` 是 fill 内部 child vectors，EDADB/shadow 机制使用 vector index 保存其原始顺序。

Primary-key audit:

- `initPrimKeys()` 没有关闭 `Shadow<IdbFill>` 的 primary-key 行为；`primary_key` 是 table 第一列和 root identity。
- `Shadow<IdbRect>` 和 `Shadow<IdbCoordinate<int32_t>>` 的 PK 已关闭；它们是 child vector element storage view，通过 `_vec_idx`/child vector index 表达顺序。
- 不定义 `_order_sd`；`FILLS` root order 是 Level D，不作为 iEDA 点工具语义约束。
- `initReadDb()` / `initWriteDb()` 都先调用 `initPrimKeys()`，再调用 `initAllTables()`，因此 read/write 的 table metadata 一致。

## Field Mapping To Original DEF Flow

以下按 EDADB shadow 域列出它对应的原始 DEF read/write 代码位置。

- Root identity: `primary_key`
  - Write source: `DefWrite::write_fill()` 按 fill list 顺序输出 fill records，见 `src/database/manager/builder/def_builder/def_write.cpp:1140-1188`。
  - Read source: `fillsCallback()` / `parse_fill_number()` reserve list，`fillCallback()` / `parse_fill()` 按 DEF 出现顺序创建 fill，见 `src/database/manager/builder/def_builder/def_read.cpp:2313-2396`。

- Fill type and refs: `_type_sd`, `_layer_name_sd`, `_via_name_sd`
  - Write source: `write_fill()` 区分 layer fill 和 via fill，并输出 layer/via name，见 `src/database/manager/builder/def_builder/def_write.cpp:1140-1188`。
  - Read source: `parse_fill()` 读取 layer/via fill 类型，并按 layer/via name lookup，见 `src/database/manager/builder/def_builder/def_read.cpp:2352-2396`。

- Fill geometry: `_rect_list_sd`, `_coordinate_list_sd`
  - Write source: `write_fill()` 输出 layer fill rects 或 via fill coordinates，见 `src/database/manager/builder/def_builder/def_write.cpp:1140-1188`。
  - Read source: `parse_fill()` 读取 rect list 或 coordinate list，见 `src/database/manager/builder/def_builder/def_read.cpp:2352-2396`。

## Child Storage View

`IdbFill` 是 `FILLS` root，当前子节点按 type flatten 到同一张 root shadow 表：

- layer fill 使用 `_layer_name_sd` 和 `_rect_list_sd`。
- via fill 使用 `_via_name_sd` 和 `_coordinate_list_sd`。
- `_rect_list_sd` 使用 `Shadow<IdbRect>` child rows；`_vec_idx` 保存并恢复 rect 顺序。
- `_coordinate_list_sd` 使用 `Shadow<IdbCoordinate<int32_t>>` child rows，保存 coordinate 顺序。

不直接保存 `IdbLayer*` 或 `IdbVia*`：它们是运行时引用。read 时按 name lookup layer/via；via 会按原始 parser 语义 clone 后挂到 fill。

`Shadow<IdbFillLayer>` / `Shadow<IdbFillVia>` 不再定义 active storage view；root `Shadow<IdbFill>` 已经表达 DEF-visible storage view，避免多一层无用 root table。

## Why Fill Shadow

当前需要 `Shadow<IdbFill>`：

- `IdbFill` 是 polymorphic-like wrapper：实际语义分 layer fill 和 via fill。
- `IdbFill` 没有天然 name/ID，不能用 layer/via name 当 root identity。
- `IdbFillList` 是 Level D root list；当前不保存 root append order，也不使用 vector order index。
- shadow 用 `primary_key` 作为 root identity。
- shadow 用 name + vector child 替代运行时 pointer 和 owning geometry list。

## EDADB Write Path

当前 `writeIdbFill()`：

- Code: `src/database/manager/builder/def_builder/def_write_edadb.cpp:561`
- Fill vector access: `src/database/manager/builder/def_builder/def_write_edadb.cpp:574`
- Empty-list return: `src/database/manager/builder/def_builder/def_write_edadb.cpp:578`
- Shadow construction: `src/database/manager/builder/def_builder/def_write_edadb.cpp:584`
- EDADB insert: `src/database/manager/builder/def_builder/def_write_edadb.cpp:590`

- 从 `design->get_fill_list()` 取得 fill vector。
- 空列表返回 `kDbSuccess`，避免 EDADB dispatcher 中断整个写流程。
- 非空时构造 `Shadow<IdbFill>` pointer vector。
- 写入 type、layer/via name、rect vector 或 coordinate vector。
- 使用 `edadb::insertVector<Shadow<IdbFill>>()` 写入。

这与原始 DEF 输出字段一致；空列表返回值是 adapter 层为 dispatcher 做的语义调整。

## EDADB Read Path

当前 `readIdbFill()`：

- Code: `src/database/manager/builder/def_builder/def_read_edadb.cpp:642`
- EDADB read op: `src/database/manager/builder/def_builder/def_read_edadb.cpp:659`
- EDADB read loop: `src/database/manager/builder/def_builder/def_read_edadb.cpp:662`
- Layer lookup / add layer fill: `src/database/manager/builder/def_builder/def_read_edadb.cpp:675`
- Via lookup / add via fill: `src/database/manager/builder/def_builder/def_read_edadb.cpp:688`

- 使用 EDADB read-all 读取 root records，不指定 root order。
- layer fill 按 `_layer_name_sd` 查找 LEF layer，调用 `add_fill_layer(layer)`，再恢复 rect list。
- via fill 优先按 `_via_name_sd` 查找 DEF via，找不到再查 LEF via，clone 后调用 `add_fill_via(via_new)`，再恢复 coordinate list。
- `createDbByDef()` 不注册 fill callback，避免 DEF 文本重复创建 fill。

读取顺序在 via 之后，因此 DEF via list 已可用于 fill via lookup。

## Computed Fields

这些字段不直接入库：

- `IdbLayer*`：由 layer name 查找恢复。
- `IdbVia*`：由 via name 查找并 clone 恢复。
- polygon：原始 parser 未实现。

## Order / Index

`IdbFillList` 不强制保持 root 顺序；rect vector 和 coordinate vector 需要保持顺序。

依据：

- 原始 `parse_fill()` 按 DEF 出现顺序 append fill。
- 原始 `write_fill()` 按 `fill_list->get_fill_list()` 当前顺序输出。
- layer fill rect 和 via fill coordinate 都按 vector 当前顺序输出。
- `src/operation` 中当前未发现点工具依赖 `IdbFillList::_fill_list` root index/order。
- 因此 `FILLS` root order 在点工具语义上是 Level D；当前 shadow 用 `primary_key` 作为 root identity，不保存 root list order。
- child vector 顺序由 EDADB vector child 机制保存。
- read path 不依赖 EDADB/SQLite read-all 物理顺序表达语义；root-order-only 文本差异由 normalized diff 处理。

当前状态：已实现。root identity 和 root order 已分离；root order 不保存，rect/coordinate child vector order 已回归验证。

对 normalized diff 的影响：

- `FILLS` 是 Level D root list；如果 raw diff 只因为不同 fill root record 顺序失败，normalized diff 可以按 layer/via/geometry signature 排序后通过。
- 排序单位必须是完整 fill record；record 内部 rect/coordinate vector 不排序。
- 如果 fill type、layer/via name 或 geometry 内容不同，normalized diff 必须失败。

## Tests

- demo `sky130_gcd` 覆盖空列表路径：`writeIdbFill insert fill_count=0`，`readIdbFill restored fill_count=0`。
- `src/database/edadb/test/run_idb_roundtrip_regression.sh` 的 `aux_optional` case 覆盖 layer fill 和 via fill，并检查 `iFillSD` count、确认没有 `_order_sd` column、type、layer/via name、rect count 和 coordinate count。

## Risks / TODO

- 当前只覆盖原始 parser/writer 已实现的 rect/coordinate 语义；polygon 后续如果实现，需要同步扩展 schema/read/write/test。
- Via fill read 会 clone via；这与原始 parser 一致，但也意味着 fill 持有的是 cloned via object，不是 via list 中的原始 pointer。
