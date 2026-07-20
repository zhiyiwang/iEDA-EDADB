# IdbBlockage EDADB Adapter Review

## Scope

`IdbBlockage` 对应 DEF 的 `BLOCKAGES` section。

- Write: `DefWrite::write_blockage()`
- Read: `blockageCallback()` / `DefRead::parse_blockage()`
- EDADB Write: `DefWriteEdadb::writeIdbBlockage()`，见 `src/database/manager/builder/def_builder/def_write_edadb.cpp:426`
- EDADB Read: `DefReadEdadb::readIdbBlockage()`，见 `src/database/manager/builder/def_builder/def_read_edadb.cpp:745`

本文件按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 的约束检查：

- DEF section 映射：`BLOCKAGES` section。
- iEDA root container：`IdbBlockageList::_blockage_list`。
- root-vector order 等级：Level D，当前未发现点工具依赖 blockage root index/order。
- EDADB 结论：root order 不作为语义约束；当前分支不保存 root order 字段，也不做 ordered read。rect nested vector 仍通过 `_vec_idx` 保序。

## Original Write Semantics

原始 `DefWrite::write_blockage()` 按 `IdbBlockageList` 顺序输出：

- routing blockage: layer name、optional `PUSHDOWN`、optional `EXCEPTPGNET`、optional component name、rect list。
- placement blockage: `PLACEMENT`、optional `PUSHDOWN`、optional component name、rect list。

原始 writer 不输出：

- routing `SLOTS` / `FILLS`。
- routing spacing / design rule width。
- placement soft / partial density。
- polygon。

空 blockage list 时，原始 writer 返回 `kDbFail`，但 top-level DEF writer 不把它当作整体失败。

## Original Read Semantics

原始 `DefRead::parse_blockage()`：

- `hasLayer()` 时创建 `IdbRoutingBlockage`，设置 layer name，并按 LEF layer name 查找 `IdbLayer*`。
- 无 layer 时创建 `IdbPlacementBlockage`。
- 读取 pushdown、exceptpgnet、component name 和 rectangle list。
- component name 通过 `IdbInstanceList::find_instance()` 恢复 instance pointer。
- parser 还读取 slots/fills/spacing/effective width/soft/partial，但这些字段当前原始 writer 不输出。

## EDADB Schema

当前 schema：

```cpp
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbBlockage>, "iBlockageSD",
                 (primary_key, _instance_name_sd, _is_pushdown_sd,
                  _type_sd, _layer_name_sd, _is_except_pgnet_sd),
                 (_rect_list_sd));

TABLE4SHADOW(idb::IdbRect);
TABLE4CLASS(edadb::Shadow<idb::IdbRect>, "IdbRectSD",
            (_vec_idx, _lx_sd, _ly_sd, _hx_sd, _hy_sd));
```

保存字段覆盖当前 DEF writer 实际输出的 blockage type、layer/component 引用、flags 和 rectangle vector。

Schema / init 代码位置：

- `iBlockageSD` root table macro: `src/database/edadb/idb/edadb_idb_schema.h:103`
- `IdbRectSD` child table macro: `src/database/edadb/idb/edadb_idb_schema.h:70`
- `Shadow<IdbRect>` PK disabled: `src/database/edadb/idb/edadb_idb_init.cpp:30`
- `Shadow<IdbBlockage>` PK uses EDADB default `true`; `primary_key` is a synthetic root identity because `BLOCKAGES` records have no DEF name.
- Table registration: `src/database/edadb/idb/edadb_idb_init.cpp:89`
- Blockage shadow definition: `src/database/edadb/idb/shadow/shadow_idb_blockage.h:19`

Primary-key audit:

- `Shadow<IdbBlockage>::primary_key` is identity only; it is not vector order.
- `Shadow<IdbBlockage>` intentionally has no `_order_sd`; Level D root semantics do not require preserving original append order.
- `Shadow<IdbRect>` has no root identity; its `_vec_idx` preserves rect vector order under each blockage.

## Original DEF Write/Read Roundtrip Mapping

### Original DEF Write Flow

| 原始 `DefWrite` 执行顺序 | EDADB write / `toShadow` 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. `write_blockage()` 对 null/empty list 失败，输出 count 并遍历 root vector，见 `def_write.cpp:588-603` | `writeIdbBlockage()` 对 null 失败、empty 成功；shadow synthetic `primary_key` 只表示 anonymous identity，不保存 root order，见 `def_write_edadb.cpp:426-455` | `BLOCKAGES <N>` / `IdbBlockageList::_blockage_list` / `iBlockageSD.primary_key` |
| 2. routing 分支输出 `LAYER`，然后条件输出 `PUSHDOWN/EXCEPTPGNET/COMPONENT`，见 `def_write.cpp:604-619` | `_type_sd` 标记派生类，保存 `_layer_name_sd/_is_pushdown_sd/_is_except_pgnet_sd/_instance_name_sd`，见 `shadow_idb_blockage.h:31-46` | routing tags / `IdbRoutingBlockage` fields / corresponding blockage shadow fields |
| 3. routing 分支按 rect vector 顺序输出 geometry，见 `def_write.cpp:621-623` | `_rect_list_sd` 保存 rect values，registered rect shadow index 保存 child order，见 `shadow_idb_blockage.h:37-40` | `RECT` / `IdbBlockage::_rect_list` / `_rect_list_sd` → `IdbRectSD` |
| 4. placement 分支输出 `PLACEMENT`，然后条件输出 `PUSHDOWN/COMPONENT`，见 `def_write.cpp:624-635` | 同一 shadow 用 `_type_sd` 和 common fields 表示 placement subtype | `PLACEMENT/PUSHDOWN/COMPONENT` / `IdbPlacementBlockage` fields / `_type_sd/_is_pushdown_sd/_instance_name_sd` |
| 5. placement 分支按 rect vector 输出 geometry，见 `def_write.cpp:637-639` | 同一 `_rect_list_sd` child storage | `RECT` / placement rect list / `_rect_list_sd` |
| 6. writer 没有输出 parser 支持的 routing `SLOTS/FILLS/SPACING/DESIGNRULEWIDTH` 或 placement `SOFT/PARTIAL` | 当前 shadow 也不存储这些 parser-only fields | parser-only blockage tags / slots/fills/min-spacing/effective-width/soft/density / 无 EDADB 字段 |

### Original DEF Read Flow

| 原始 `DefRead` 执行顺序 | EDADB read / `fromShadow` 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. `parse_blockage()` 用 `hasLayer()` 区分 routing/placement；routing 创建后按 name lookup layer，见 `def_read.cpp:1955-1970` | `readIdbBlockage()` 根据 `_type_sd` 调用对应 list factory；routing 按 `_layer_name_sd` lookup layer，见 `def_read_edadb.cpp:745-785` | subtype/LAYER / blockage derived type + layer ref / `_type_sd/_layer_name_sd` |
| 2. routing parser 依次恢复 `SLOTS/FILLS/PUSHDOWN/EXCEPTPGNET`，见 `def_read.cpp:1972-1986` | shadow 只恢复 `PUSHDOWN/EXCEPTPGNET`；`SLOTS/FILLS` 无存储，见 `shadow_idb_blockage.h:49-65` | routing flags / routing blockage fields / `_is_pushdown_sd/_is_except_pgnet_sd`; slots/fills missing |
| 3. routing parser 条件恢复 COMPONENT pointer/name、SPACING、DESIGNRULEWIDTH，见 `def_read.cpp:1988-2000` | builder 按 `_instance_name_sd` lookup；当前 lookup 失败直接返回 false，比原始 parser 更严格；spacing/width 未存储，见 `def_read_edadb.cpp:791-799` | `COMPONENT/SPACING/DESIGNRULEWIDTH` / instance ref, spacing, width / `_instance_name_sd`; spacing/width missing |
| 4. routing parser 按 DEF 顺序 append rects，见 `def_read.cpp:2002-2004` | `fromShadow()` 按 child order `add_rect()`，见 `shadow_idb_blockage.h:55-59` | `RECT` / routing rect list / `_rect_list_sd` |
| 5. placement parser 创建 subtype，依次恢复 `SOFT/PARTIAL/COMPONENT`，见 `def_read.cpp:2008-2024` | factory 创建 placement subtype；shadow 恢复 common pushdown/instance fields，但 `SOFT/PARTIAL` 未存储 | placement tags / soft/density/instance / `_instance_name_sd`; soft/density missing |
| 6. placement parser 按 DEF 顺序 append rects，见 `def_read.cpp:2026-2028` | 共用 `_rect_list_sd` 恢复 | `RECT` / placement rect list / `_rect_list_sd` |

## Child Storage View

`IdbBlockage` 是 `BLOCKAGES` root，当前子节点/引用处理如下：

- `_rect_list_sd`：`vector<IdbRect>` child，使用 `Shadow<IdbRect>` 保存 rect 几何和 child order。
- `Shadow<IdbRect>` 保存 `_vec_idx/_lx_sd/_ly_sd/_hx_sd/_hy_sd`，用 `_vec_idx` 恢复 rectangle vector 的原始顺序。
- `_instance`：不作为 child 存库；保存 `_instance_name_sd`，read 时通过 `IdbInstanceList::find_instance()` 恢复。
- `_layer`：不作为 child 存库；routing blockage 保存 `_layer_name_sd`，read 时通过 `IdbLayers::find_layer()` 恢复。

因此 `Shadow<IdbBlockage>` 是必要的，但原因不是 root order：

- `IdbBlockage` 是 polymorphic base，真实对象由 `IdbBlockageList::add_blockage_routing()` 或 `add_blockage_placement()` 初始化成不同子类。
- EDADB root table 需要一个统一的 storage view，所以 shadow 用 `_type_sd` 保存 routing/placement 类型。
- `toShadow()` 根据 `obj->get_type()` 读取共同字段；若为 `IdbRoutingBlockage`，再 `dynamic_cast` 读取 `_layer_name_sd` 和 `_is_except_pgnet_sd`。
- `readIdbBlockage()` 先根据 `_type_sd` 调用对应的 list factory 创建正确子类，再调用 `fromShadow()` 回填共同字段和 routing-only 字段。
- layer / instance pointer 不直接存入 DB；read 阶段分别通过 `_layer_name_sd` 和 `_instance_name_sd` lookup 恢复。
- `primary_key` 只是 anonymous `BLOCKAGES` root record identity；当前没有 `_order_sd`，不承担 root list 顺序语义。

## EDADB Write Path

当前 `writeIdbBlockage()`：

- 从 `design->get_blockage_list()` 取得 root vector。
- 空列表返回 `kDbSuccess`，避免 EDADB dispatcher 中断整个写流程。
- 构造 `Shadow<IdbBlockage>` pointer vector。
- 写入 `primary_key`、type、pushdown、instance name、routing layer name、exceptpgnet 和 rect vector。
- 使用 `edadb::insertVector<Shadow<IdbBlockage>>()` 写入。

这与原始 writer 输出字段一致；空列表返回值是 adapter 层为 dispatcher 做的语义调整。因为 `BLOCKAGES` 是 Level D root list，当前不额外保存 root append order。

## EDADB Read Path

当前 `readIdbBlockage()`：

- `blockage_list->reset()` 清空旧数据。
- 使用 EDADB read-all 读取 root records，不指定 root order；Level D root order 只由 normalized diff 处理。
- 根据 `_type_sd` 创建 routing 或 placement blockage。
- `fromShadow()` 恢复 pushdown、instance name、routing layer name、exceptpgnet 和 rect vector。
- routing blockage 通过 `_layer_name_sd` 查找并恢复 `IdbLayer*`。
- instance pointer 通过 `_instance_name_sd` 查找并恢复。
- `createDbByDef()` 不注册 blockage callback，避免 DEF 文本重复创建 blockage。

读取顺序在 instance 之后，因此 component reference 可以通过 instance name 查找。

## Computed Fields

这些字段不入库或不作为当前 roundtrip 语义：

- `_instance` pointer：由 instance name 查找重建。
- routing `_layer` pointer：由 layer name 查找重建。
- slots/fills/spacing/effective width/soft/partial：原始 parser 会读取，但当前 writer 不输出；当前 EDADB adapter 按 writer-visible 语义保存。
- polygon：原始 parser 也未实现。

## Order / Index

`IdbBlockageList` 是 Level D root list，不要求保持原始 append 顺序。

依据：

- 原始 `parse_blockage()` 按 DEF 出现顺序 append。
- 原始 `write_blockage()` 按 `blockage_list->get_blockage_list()` 当前顺序输出。
- `def-ieda-mapping-and-order.md` 中未发现点工具依赖 blockage root index/order；iPDN 会遍历 blockage，但后续按 overlap edge 排序处理。
- blockage 没有稳定 name identity，因此当前 shadow 用 synthetic `primary_key` 表达 root identity。
- 当前不保存 `_order_sd`，避免为 Level D root list 维护非必要 order 字段。
- raw DEF diff 若只因 Level D blockage root record 顺序变化失败，应由 normalized diff 按 stable signature 处理。

当前状态：root identity 和 root order 已分离；Level D 不强制 ordered read，nested rect order 仍由 `_vec_idx` 保证。

## Tests

- demo `sky130_gcd` 覆盖空列表路径：`writeIdbBlockage insert blockage_count=0`，`readIdbBlockage restored blockage_count=0`。
- `src/database/edadb/test/run_idb_roundtrip_regression.sh` 的 `aux_optional` case 覆盖 routing/placement blockage，并检查 `iBlockageSD` count、type、layer name、pushdown、exceptpgnet 和 rect `_vec_idx`。
- normalized diff 覆盖 Level D `BLOCKAGES` root order-only differences；nested rect order 不排序。

## Risks / TODO

- 当前 EDADB adapter 不保存原始 writer 不输出的 blockage fields；如果后续要保持 parser state 而不是 DEF writer-visible state，需要同步扩展 writer、schema、read/write 和测试。
- 未保存的 parser-supported fields 包括 routing `SLOTS/FILLS/SPACING/DESIGNRULEWIDTH` 和 placement `SOFT/PARTIAL`。
- component name 查找依赖 instance 已先从 EDADB 恢复。
- component lookup 失败时，EDADB read 返回失败；原始 parser 会设置可能为 null 的 pointer 后继续，错误策略并不一致。
- 原始 writer 对空 blockage list 返回失败，EDADB writer 对空 vector 返回成功。
