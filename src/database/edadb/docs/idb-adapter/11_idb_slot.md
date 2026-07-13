# IdbSlot EDADB Adapter Review

## Scope

`IdbSlot` 对应 DEF 的 `SLOTS` section。

- Write: `DefWrite::write_slot()`
- Read: `slotsCallback()` / `DefRead::parse_slot()`
- EDADB Write: `DefWriteEdadb::writeIdbSlot()`
- EDADB Read: `DefReadEdadb::readIdbSlot()`

本文件按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 的约束检查：

- DEF section 映射：`SLOTS` section。
- iEDA root container：`IdbSlotList::_slot_list`。
- root-vector order 等级：Level D；本 no-sort 实验分支不再把 raw roundtrip 当作例外，不保存 root append order。
- duplicate/root identity 约束：slot 没有 name，`_layer_name` 也不唯一；因此仍需要 adapter shadow 提供 `primary_key`。
- nested vector 约束：rectangle vector 是 slot 内部几何列表，必须随 root record 保持原始顺序，不参与 D-level root sort。

## Original Write Semantics

原始 `DefWrite::write_slot()` 输出：

- slot count: `slot_list->get_num()`
- layer name: `slot->_layer_name`
- rectangle list: `slot->_rect_list`

空 slot list 时，原始 writer 返回 `kDbFail`，但 top-level DEF writer 不把它当作整体失败。

## Original Read Semantics

原始 `DefRead::parse_slot()`：

- 通过 `slot_list->add_slot()` 创建 slot。
- 如果 `def_slot->hasLayer()`，设置 layer name。
- 按 DEF rectangle 顺序调用 `slot->add_rect(...)`。
- polygon 当前仍是 TBD，不进入 iDB 状态。

## EDADB Schema

当前 schema：

```cpp
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSlot>, "iSlotSD", (primary_key, _layer_name_sd), (_rect_list_sd));
```

Schema / init 代码位置：

- `iSlotSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:109`
- Primary-key setup: `src/database/edadb/idb/edadb_idb_init.cpp:21`
- Table registration: `src/database/edadb/idb/edadb_idb_init.cpp:91`
- Shadow definition: `src/database/edadb/idb/shadow/shadow_idb_slot.h:15`

保存字段覆盖原始 DEF writer/read 需要的 layer name 和 rectangle vector。

Schema 与 order/index 约束的关系：

- 依据 `src/database/edadb/docs/def-ieda-mapping-and-order.md`，`SLOTS` 映射到 `IdbSlotList::_slot_list`，等级为 Level D。
- `SLOTS` 是 anonymous root record；`primary_key` 只表达 DB identity，用来支持同 layer、多 rectangle-signature甚至重复 slot record。
- schema 不包含 `_order_sd`，read path 使用 read-all；root-order-only DEF 差异由 A/B/C/D normalizer 处理。
- `_rect_list_sd` 是 slot 内部 owned geometry vector，使用 `Shadow<IdbRect>::_vec_idx` 保存 nested order。

Primary-key audit:

- `IdbSlot` 原始类没有天然唯一 name；不能用 `_layer_name` 作为 PK，因为同一 layer 可以有多个 slot record。
- `initPrimKeys()` 没有关闭 `Shadow<IdbSlot>` 的 primary-key 行为；`primary_key` 是 table 第一列和 root identity。
- `initReadDb()` / `initWriteDb()` 都先调用 `initPrimKeys()`，再调用 `initAllTables()`，因此 read/write 的 table metadata 一致。

## Original DEF Write/Read Roundtrip Mapping

### Original DEF Write Flow

| 原始 `DefWrite` 执行顺序 | EDADB write / `toShadow` 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. `write_slot()` 检查 list；空 list 返回失败；输出 section count，见 `def_write.cpp:1076-1088` | `writeIdbSlot()` 检查 list；空 vector 返回成功；batch insert root shadows，但不保存 root index | `SLOTS <N>` / `IdbSlotList::_slot_list` / `iSlotSD` row count |
| 2. 按 slot vector 顺序输出匿名 root record 和 layer name，见 `def_write.cpp:1090-1091` | `toShadow()` 用 synthetic `primary_key` 标识匿名 root，并保存 `_layer_name_sd` | `- LAYER <name>` / `IdbSlot::_layer_name` / `primary_key`, `_layer_name_sd` |
| 3. 按 rect vector 顺序输出 geometry，见 `def_write.cpp:1093-1095` | `toShadow()` 按原顺序复制 `_rect_list_sd`；nested rect index 由 EDADB child-vector storage 保留，见 `shadow_idb_slot.h:31-34` | `RECT` / `IdbSlot::_rect_list` / `_rect_list_sd` |
| 4. 输出 record/section terminator，见 `def_write.cpp:1097-1100` | 由 root/child row 边界重建，不存文本终止符 | `;`, `END SLOTS` / 无 iDB 成员 / 无 EDADB 字段 |

### Original DEF Read Flow

| 原始 `DefRead` 执行顺序 | EDADB read / `fromShadow` 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. `slotsCallback()` 校验参数后调用 `parse_slot()`，后者无条件 `add_slot()`，见 `def_read.cpp:2117-2143` | `readIdbSlot()` read-all root rows，并逐条 `add_slot()`；不指定 root order | anonymous slot root / `IdbSlotList::_slot_list` / `primary_key` |
| 2. 有 `LAYER` 时恢复 layer name，见 `def_read.cpp:2145-2146` | `fromShadow()` 调 `set_layer_name(_layer_name_sd)`；与原 parser 一样不恢复 `IdbLayer*` pointer，见 `shadow_idb_slot.h:36-42` | `LAYER` / `IdbSlot::_layer_name` / `_layer_name_sd` |
| 3. 按 DEF rectangle 顺序调用 `add_rect()`，见 `def_read.cpp:2147-2149` | `fromShadow()` 按 `_rect_list_sd` child order 创建并复制 `IdbRect`，见 `shadow_idb_slot.h:44-48` | `RECT` / `IdbSlot::_rect_list` / `_rect_list_sd` |
| 4. polygon 分支仍为 TODO，见 `def_read.cpp:2152-2153` | schema 不保存 polygon，与原始 parser 最终 iDB 状态一致 | polygon / 无已实现 iDB 成员 / 无 EDADB 字段 |

## Child Storage View

`IdbSlot` 是 `SLOTS` root，当前持久化子节点是 rectangle vector：

- `_rect_list_sd`：`vector<IdbRect>`，使用 `Shadow<IdbRect>` child table。
- `Shadow<IdbRect>` 保存 `_vec_idx/_lx_sd/_ly_sd/_hx_sd/_hy_sd`；其中 `_vec_idx` 是 rectangle vector index，用于抵抗 DB child-row read order 变化。

不保存 `IdbLayer* _layer`：原始 `parse_slot()` 只设置 layer name，当前 adapter 也只保存 `_layer_name_sd`。如果未来原始 parser 恢复 layer pointer，再讨论是否用 layer name lookup 重建。

## Why Slot Shadow

当前仍需要 `Shadow<IdbSlot>`：

- `IdbSlot` 没有天然 name/ID，`_layer_name` 不保证唯一，direct `TABLE4CLASS_WVEC(idb::IdbSlot, ...)` 难以给 root record 和 child rect vector 提供稳定 identity。
- `primary_key` 是 EDADB root identity，解决 duplicate/anonymous slot record 存储问题。
- rectangle vector 是 owning child data，由 `TABLE4CLASS_WVEC` 发现 child rows，再由 `Shadow<IdbRect>::_vec_idx` 恢复 child order。

## EDADB Write Path

当前 `writeIdbSlot()`：

- Code: `src/database/manager/builder/def_builder/def_write_edadb.cpp:475`
- Slot vector access: `src/database/manager/builder/def_builder/def_write_edadb.cpp:488`
- Empty-list return: `src/database/manager/builder/def_builder/def_write_edadb.cpp:492`
- Shadow construction: `src/database/manager/builder/def_builder/def_write_edadb.cpp:498`
- EDADB insert: `src/database/manager/builder/def_builder/def_write_edadb.cpp:504`

- 从 `design->get_slot_list()` 取得 slot vector。
- 空列表返回 `kDbSuccess`，避免 EDADB dispatcher 中断整个写流程。
- 非空时构造 `Shadow<IdbSlot>` pointer vector。
- `toShadow()` 保存 `primary_key`、`_layer_name_sd` 和 `_rect_list_sd`，不保存 root index。
- 使用 `edadb::insertVector<Shadow<IdbSlot>>()` 写入。

这与原始 DEF 输出字段一致；空列表返回值是 adapter 层为 dispatcher 做的语义调整。

## EDADB Read Path

当前 `readIdbSlot()`：

- Code: `src/database/manager/builder/def_builder/def_read_edadb.cpp:545`
- EDADB read op: `src/database/manager/builder/def_builder/def_read_edadb.cpp:558`
- EDADB read loop: `src/database/manager/builder/def_builder/def_read_edadb.cpp:567`
- Add to active list: `src/database/manager/builder/def_builder/def_read_edadb.cpp:578`

- 使用 `makeReadAllOp<Shadow<IdbSlot>>()` 循环读取 root records，不指定 root order。
- `slot_list->add_slot()` 创建 slot。
- `fromShadow()` 恢复 layer name 和 rectangle list。
- `createDbByDef()` 不注册 slot callback，避免 DEF 文本重复创建 slot。

## Computed Fields

`IdbSlot` 当前没有 read 后计算字段：

- layer name 和 rectangles 全部来自 DEF/EDADB。
- `_layer` 指针原始 parser 也没有设置，当前不入库、不重建。
- polygon 尚未在原始 parser 中实现，不进入 EDADB schema。

## Order / Index

`IdbSlotList` 当前不保存 root order。

依据：

- 原始 `parse_slot()` 按 DEF 出现顺序 `add_slot()`。
- 原始 `write_slot()` 按 `slot_list->get_slot_list()` 当前顺序输出。
- `SLOTS` root record 没有 name；DB identity 由 synthetic `primary_key` 提供，但 identity 不等于 list order。
- `src/operation` 中没有发现 `IdbSlotList::_slot_list` 的点工具 root order/index 消费者。
- `src/platform/data_manager/idm_transform.cpp:335` 仅遍历 slot rectangles 做坐标变换；该遍历不依赖 slot root index。

当前状态：已实现 shadow identity；无 `_order_sd`，无 ordered root read。

对 normalized diff 的影响：

- normalized diff 的排序单位是完整 slot record；record 内部 rectangle vector 不排序并随 root record 整体移动。
- 如果 layer name 或 rectangle 内容不同，normalized diff 必须失败。

## Tests

- demo `sky130_gcd` 覆盖空列表路径：`writeIdbSlot insert slot_count=0`，`readIdbSlot restored slot_count=0`。
- `src/database/edadb/test/run_idb_roundtrip_regression.sh` 的 `aux_optional` case 覆盖非空 slot，并检查 `iSlotSD` count、确认不存在 `_order_sd`、layer name、rect `_vec_idx` 和 rectangle。

## Risks / TODO

- 当前 `_layer` 指针未恢复；这对 DEF SLOTS roundtrip 与原始 parser 一致。
- 原始 writer 对空 slot list 返回失败，EDADB writer 对空 vector 返回成功。
- `readIdbSlot()` 不清空现有 list，依赖“新 design、单次恢复”的调用前提。
- 若未来原始 DEF parser 支持 slot polygon，需要同步扩展 schema 和 read/write。
