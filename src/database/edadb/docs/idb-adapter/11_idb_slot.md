# IdbSlot EDADB Adapter Review

## Scope

`IdbSlot` 对应 DEF 的 `SLOTS` section。

- Write: `DefWrite::write_slot()`
- Read: `slotsCallback()` / `DefRead::parse_slot()`
- EDADB Write: `DefWriteEdadb::writeIdbSlot()`
- EDADB Read: `DefReadEdadb::readIdbSlot()`

## Original Write Semantics

原始 `DefWrite::write_slot()` 输出：

- slot count: `slot_list->get_num()`
- layer name: `slot->_layer_name`
- rectangle list: `slot->_rect_list`

空 slot list 时，原始 writer 返回 `kDbFail`，但 top-level DEF writer 不把它当作整体失败。

## Original Read Semantics

原始 `DefRead::parse_slot()`：

- 通过 `slot_list->add_slot()` 按 DEF 出现顺序创建 slot。
- 如果 `def_slot->hasLayer()`，设置 layer name。
- 按 DEF rectangle 顺序调用 `slot->add_rect(...)`。
- polygon 当前仍是 TBD，不进入 iDB 状态。

## EDADB Schema

当前 schema：

```cpp
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSlot>, "iSlotSD", (primary_key, _order_sd, _layer_name_sd), (_rect_list_sd));
```

保存字段覆盖原始 DEF writer/read 需要的 layer name 和 rectangle vector。

## Field Mapping To Original DEF Flow

以下按 EDADB shadow 域列出它对应的原始 DEF read/write 代码位置。

- Root identity / order: `primary_key`, `_order_sd`
  - Write source: `DefWrite::write_slot()` 按 slot list 顺序输出 slot records，见 `src/database/manager/builder/def_builder/def_write.cpp:1074-1104`。
  - Read source: `slotsCallback()` / `parse_slot()` 按 DEF 出现顺序创建 slot，见 `src/database/manager/builder/def_builder/def_read.cpp:2117-2156`。

- Layer ref: `_layer_name_sd`
  - Write source: `write_slot()` 输出 slot layer name，见 `src/database/manager/builder/def_builder/def_write.cpp:1074-1104`。
  - Read source: `parse_slot()` 按 layer name lookup LEF layer，见 `src/database/manager/builder/def_builder/def_read.cpp:2135-2156`。

- Rect geometry: `_rect_list_sd`
  - Write source: `write_slot()` 输出 slot rectangle list，见 `src/database/manager/builder/def_builder/def_write.cpp:1074-1104`。
  - Read source: `parse_slot()` 读取 slot rects，见 `src/database/manager/builder/def_builder/def_read.cpp:2135-2156`。

## Child Storage View

`IdbSlot` 是 `SLOTS` root，当前持久化子节点是 rectangle vector：

- `_rect_list_sd`：`vector<IdbRect*>`，使用 direct `IdbRect` child table，并保持 child order。

不保存 `IdbLayer* _layer`：原始 `parse_slot()` 只设置 layer name，当前 adapter 也只保存 `_layer_name_sd`。如果未来原始 parser 恢复 layer pointer，再讨论是否用 layer name lookup 重建。

## Why Slot Shadow

当前需要 `Shadow<IdbSlot>`：

- `IdbSlot` 没有天然 name/ID，`_layer_name` 不保证唯一。
- 因此用 `primary_key` 作为 root record identity。
- `IdbSlotList` 需要恢复 DEF append 顺序，但不能用 vector order index 作为 PK。
- `_order_sd` 单独保存 list order。
- rectangle vector 是 owning child data，由 `TABLE4CLASS_WVEC` 保存并保持 child order。

## EDADB Write Path

当前 `writeIdbSlot()`：

- 从 `design->get_slot_list()` 取得 slot vector。
- 空列表返回 `kDbSuccess`，避免 EDADB dispatcher 中断整个写流程。
- 非空时按 list 顺序构造 `Shadow<IdbSlot>` pointer vector。
- `toShadow()` 保存 `primary_key`、`_order_sd`、`_layer_name_sd` 和 `_rect_list_sd`。
- 使用 `edadb::insertVector<Shadow<IdbSlot>>()` 写入。

这与原始 DEF 输出字段一致；空列表返回值是 adapter 层为 dispatcher 做的语义调整。

## EDADB Read Path

当前 `readIdbSlot()`：

- 通过 `ORDER BY "_order_sd"` 读取 root records，恢复 `IdbSlotList` 原始 append 顺序。
- 循环读取 `Shadow<IdbSlot>`。
- `slot_list->add_slot()` 创建 slot。
- `fromShadow()` 恢复 layer name 和 rectangle list。
- `createDbByDef()` 不注册 slot callback，避免 DEF 文本重复创建 slot。

## Computed Fields

`IdbSlot` 当前没有 read 后计算字段：

- layer name 和 rectangles 全部来自 DEF/EDADB。
- `_layer` 指针原始 parser 也没有设置，当前不入库、不重建。
- polygon 尚未在原始 parser 中实现，不进入 EDADB schema。

## Order / Index

`IdbSlotList` 需要保持原始 append 顺序，且不应该按 layer name 或 rectangle 坐标排序。

依据：

- 原始 `parse_slot()` 按 DEF 出现顺序 `add_slot()`。
- 原始 `write_slot()` 按 `slot_list->get_slot_list()` 当前顺序输出。
- slot 没有 name lookup 入口，主要依赖 vector traversal。
- 当前 shadow 用 `primary_key` 作为 root identity，用 `_order_sd` 保存 list order。
- read path 已显式按 `_order_sd` 恢复，不依赖 EDADB/SQLite read-all 物理顺序。

当前状态：已实现。root identity 和 root order 已分离，`primary_key` 不表达 vector order。

## Tests

- demo `sky130_gcd` 覆盖空列表路径：`writeIdbSlot insert slot_count=0`，`readIdbSlot restored slot_count=0`。
- `src/database/edadb/test/run_idb_roundtrip_regression.sh` 的 `aux_optional` case 覆盖非空 slot，并检查 `iSlotSD` count、`_order_sd`、layer name 和 rectangle。

## Risks / TODO

- 当前 `_layer` 指针未恢复；这对 DEF SLOTS roundtrip 与原始 parser 一致。
- 若未来原始 DEF parser 支持 slot polygon，需要同步扩展 schema 和 read/write。
