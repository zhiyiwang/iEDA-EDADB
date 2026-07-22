# IdbBlockage EDADB Adapter Review

## Scope And Constraints

`IdbBlockage` 对应 DEF `BLOCKAGES`，root container 是 `IdbBlockageList::_blockage_list`。

- 原始 write：`DefWrite::write_blockage()`，`src/database/manager/builder/def_builder/def_write.cpp:588`
- 原始 read：`DefRead::parse_blockage()`，`src/database/manager/builder/def_builder/def_read.cpp:1955`
- EDADB write：`DefWriteEdadb::writeIdbBlockage()`，`src/database/manager/builder/def_builder/def_write_edadb.cpp:430`
- EDADB read：`DefReadEdadb::readIdbBlockage()`，`src/database/manager/builder/def_builder/def_read_edadb.cpp:790`

按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 检查：

- root order 等级为 Level D；不保存 `_order_sd`，也不依赖 SQLite root-row 返回顺序。
- blockage record 没有 DEF name；shadow 使用 synthetic `primary_key` 表示 anonymous root identity，不能把它当作 order。
- nested rectangle vector 必须保序；`Shadow<IdbRect>::_vec_idx` 保存 child index。

## Why Shadow Is Required

`Shadow<IdbBlockage>` 必要，但不是为了 root order：

- `IdbBlockage` 是 polymorphic base；DEF record 对应 `IdbRoutingBlockage` 或 `IdbPlacementBlockage`。
- `_type_sd` 是 branch discriminator；builder 先调用对应 list factory 创建正确派生类，再调用标准 `fromShadow()`。
- `_layer`、`_instance` 是 non-owning pointers；DB 保存 layer/instance name，`fromShadow()` 从 active layout/design lookup。
- `_rect_list_sd` 是 owned child storage view；EDADB 用 `IdbRectSD._vec_idx` 恢复 parser append 顺序。

builder 只负责 root cursor、派生类 factory 和错误处理；字段恢复、lookup、nested rebuild 都在 `fromShadow()`。

## EDADB Schema

```cpp
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbBlockage>, "iBlockageSD",
                 (primary_key, _instance_name_sd, _is_pushdown_sd,
                  _type_sd, _layer_name_sd, _is_except_pgnet_sd),
                 (_rect_list_sd));

TABLE4SHADOW(idb::IdbRect);
TABLE4CLASS(edadb::Shadow<idb::IdbRect>, "IdbRectSD",
            (_vec_idx, _lx_sd, _ly_sd, _hx_sd, _hy_sd));
```

- Blockage table macro：`src/database/edadb/idb/edadb_idb_schema.h:103`
- Rect table macro：`src/database/edadb/idb/edadb_idb_schema.h:69`
- Rect shadow PK disabled：`src/database/edadb/idb/edadb_idb_init.cpp:27`
- Blockage table registration：`src/database/edadb/idb/edadb_idb_init.cpp:86`
- Blockage shadow：`src/database/edadb/idb/shadow/shadow_idb_blockage.h:22`

Primary-key audit：

- `Shadow<IdbBlockage>::primary_key` 是 anonymous root identity，保留默认 PK 行为。
- `Shadow<IdbRect>` 是 owner 下的 nested element；`_vec_idx` 只表示 vector order，不作为全局 PK，因此关闭 shadow PK。

## Original DEF Write Mapping

| Original writer brace/order | DEF output | EDADB correspondence | Stored source |
| --- | --- | --- | --- |
| list/null/empty checks and root loop, `def_write.cpp:590-603` | `BLOCKAGES <N>` | `writeIdbBlockage()` reads the active list and inserts a shadow vector, `def_write_edadb.cpp:430-477` | root rows; no root order column |
| routing branch, `def_write.cpp:604-619` | `LAYER`, optional `PUSHDOWN`, `EXCEPTPGNET`, `COMPONENT` | `toShadow()` stores type/layer/routing flags; component name is stored only when `get_instance() != nullptr`, `shadow_idb_blockage.h:37-63` | `_type_sd`, `_layer_name_sd`, `_is_pushdown_sd`, `_is_except_pgnet_sd`, `_instance_name_sd` |
| routing rect loop, `def_write.cpp:621-623` | repeated `RECT` | `toShadow()` copies the complete rect vector, `shadow_idb_blockage.h:47-53` | `_rect_list_sd` + child `_vec_idx` |
| placement branch, `def_write.cpp:624-635` | `PLACEMENT`, optional `PUSHDOWN`, `COMPONENT` | `_type_sd` selects placement; component follows the same pointer-presence predicate | `_type_sd`, `_instance_name_sd`; placement pushdown is normalized as described below |
| placement rect loop, `def_write.cpp:637-639` | repeated `RECT` | same nested rect storage | `_rect_list_sd` + child `_vec_idx` |

## Original DEF Read Mapping

| Original parser brace/order | EDADB correspondence | Source / rebuild |
| --- | --- | --- |
| obtain active design/layout lists, `def_read.cpp:1961-1965` | helper already holds the active `IdbDefService`; builder obtains and resets the active blockage list, `def_read_edadb.cpp:791-805` | active list is empty before restore |
| routing factory and layer lookup, `def_read.cpp:1967-1970` | builder creates routing subtype from `_type_sd`, `def_read_edadb.cpp:819-823`; `fromShadow()` resolves `_layer_name_sd` through `EdadbIdbHelper`, `shadow_idb_blockage.h:78-92` | restore subtype, layer name and active `IdbLayer*` |
| routing flags, `def_read.cpp:1972-1986` | `fromShadow()` restores canonical writer-visible `PUSHDOWN/EXCEPTPGNET`, `shadow_idb_blockage.h:89-92` | slots/fills are parser-only hidden state; see native differences |
| routing component lookup, `def_read.cpp:1988-1992` | `fromShadow()` restores name and resolves active instance, `shadow_idb_blockage.h:74-103` | `_instance_name_sd` → `IdbInstanceList::find_instance()` |
| routing spacing/width, `def_read.cpp:1994-2000` | no EDADB columns | parser-only hidden state; see native differences |
| routing rect loop, `def_read.cpp:2002-2004` | `fromShadow()` appends EDADB-restored child vector, `shadow_idb_blockage.h:105-111` | `_rect_list_sd`, ordered by child `_vec_idx` |
| placement factory, `def_read.cpp:2008-2010` | builder creates placement subtype from `_type_sd` | placement root object |
| placement soft/partial/component, `def_read.cpp:2012-2024` | `fromShadow()` restores component reference; soft/partial have no EDADB columns | canonical component state; parser-only hidden soft/partial state omitted |
| placement rect loop, `def_read.cpp:2026-2028` | same nested rect rebuild | `_rect_list_sd`, ordered by child `_vec_idx` |

## Known Native Writer Differences

原始 writer/parser 并不完全对称：

- Writer 能输出 placement `PUSHDOWN`，但 placement parser branch 没有读取它。当前 adapter 采用“writer 输出后由原始 parser 重建”的 canonical view，因此 placement pushdown 不恢复。
- Parser 能读取 routing `SLOTS/FILLS/SPACING/DESIGNRULEWIDTH` 和 placement `SOFT/PARTIAL`，但当前 writer 不输出这些字段。
- 当前代码审计未发现点工具消费这些 parser-only hidden fields，因此它们不进入 schema；若将来出现语义消费者，必须增加 parser-only columns、read-state fixture 和 SQL assertions，不能只依赖 DEF diff。
- Polygon 在原始 parser 中仍是 TBD，不进入 schema。

## Write And Read Paths

Write：

- `writeIdbBlockage()`：`def_write_edadb.cpp:430-477`
- standard `toShadow(obj)`：`shadow_idb_blockage.h:32-67`
- component name 采用原始 writer 的 pointer predicate；避免保存“有 name 但 pointer 为空”的非输出状态。
- `insertVector<Shadow<IdbBlockage>>()` 写 root 和 nested rect rows。

Read：

- `readIdbBlockage()`：`def_read_edadb.cpp:790-842`
- reset：`def_read_edadb.cpp:803`
- subtype factory：`def_read_edadb.cpp:819-828`
- standard `fromShadow(obj)`：`shadow_idb_blockage.h:69-113`
- helper layer/instance lookup：`edadb_idb_helper.h:181`、`edadb_idb_helper.h:82`
- 任一 read/conversion 失败时再次 reset，避免留下 partial active list。
- `IdbBlockageList::reset()` 同时清空 vector 并归零 `_num`，`src/database/data/design/db_design/IdbBlockages.cpp:209`。

## Order And Tests

- Root `IdbBlockageList`：Level D，无 `_order_sd`；normalized diff 可重排完整 blockage records。
- Nested `_rect_list_sd`：必须保序；完整 record 移动时 rect vector 跟随 owner，record 内不排序。
- `aux_optional` fixture 覆盖 routing/placement、layer、component、routing flags，以及每类两个 rectangles。
- Regression 会把 SQLite child rows 按 `_vec_idx DESC` 物理重排；EDADB 仍按 child index 恢复，最终 DEF 与 direct baseline 完全一致。
- SQL 同时检查 root fields、ordered rect values 和物理 row order，避免“文本偶然相同”掩盖 DB 问题。

## Risks / TODO

- Component lookup 依赖 Instance 已先恢复；当前 `createDbByEdadb()` 顺序满足该依赖。
- Lookup 失败时 adapter 返回失败；这比原始 parser 保存 null pointer 后继续更严格。
- 原始 writer 对空 list 返回失败，而 EDADB dispatcher 将空 list 视为成功，避免中断其它 object families。
- Parser-only hidden fields 若出现真实点工具语义需求，需要扩展 schema 和 targeted read-state tests。
