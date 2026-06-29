# IdbBlockage EDADB Adapter Review

## Scope

`IdbBlockage` 对应 DEF 的 `BLOCKAGES` section。

- Write: `DefWrite::write_blockage()`
- Read: `blockageCallback()` / `DefRead::parse_blockage()`
- EDADB Write: `DefWriteEdadb::writeIdbBlockage()`
- EDADB Read: `DefReadEdadb::readIdbBlockage()`

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
                 (primary_key, _order_sd, _instance_name_sd, _is_pushdown_sd,
                  _type_sd, _layer_name_sd, _is_except_pgnet_sd),
                 (_rect_list_sd));
```

保存字段覆盖当前 DEF writer 实际输出的 blockage type、layer/component 引用、flags 和 rectangle vector。

## Field Mapping To Original DEF Flow

以下按 EDADB shadow 域列出它对应的原始 DEF read/write 代码位置。

- Root identity / order: `primary_key`, `_order_sd`
  - Write source: `DefWrite::write_blockage()` 按 blockage list 顺序输出 blockage records，见 `src/database/manager/builder/def_builder/def_write.cpp:588-648`。
  - Read source: `blockageCallback()` / `parse_blockage()` 按 DEF 出现顺序创建 blockage，见 `src/database/manager/builder/def_builder/def_read.cpp:1937-2032`。

- Blockage type and flags: `_type_sd`, `_is_pushdown_sd`, `_is_except_pgnet_sd`
  - Write source: `write_blockage()` 区分 routing/placement blockage，并输出 pushdown/except pgnet 等 flags，见 `src/database/manager/builder/def_builder/def_write.cpp:588-648`。
  - Read source: `parse_blockage()` 读取 routing/placement 类型和 flags，见 `src/database/manager/builder/def_builder/def_read.cpp:1955-2032`。

- Layer and instance refs: `_layer_name_sd`, `_instance_name_sd`
  - Write source: `write_blockage()` 输出 routing layer 或 placement component refs，见 `src/database/manager/builder/def_builder/def_write.cpp:588-648`。
  - Read source: `parse_blockage()` 按 layer/instance name lookup 并设置引用，见 `src/database/manager/builder/def_builder/def_read.cpp:1955-2032`。

- Rect geometry: `_rect_list_sd`
  - Write source: `write_blockage()` 输出 blockage rect list，见 `src/database/manager/builder/def_builder/def_write.cpp:588-648`。
  - Read source: `parse_blockage()` 读取 rects 并加入 blockage，见 `src/database/manager/builder/def_builder/def_read.cpp:1955-2032`。

## Child Storage View

`IdbBlockage` 是 `BLOCKAGES` root，当前子节点/引用处理如下：

- `_rect_list_sd`：`vector<IdbRect>` child，保存 rect 几何和 child order；`IdbRect` 是纯标量，不需要 shadow。
- `_instance`：不作为 child 存库；保存 `_instance_name_sd`，read 时通过 `IdbInstanceList::find_instance()` 恢复。
- `_layer`：不作为 child 存库；routing blockage 保存 `_layer_name_sd`，read 时通过 `IdbLayers::find_layer()` 恢复。

因此 `Shadow<IdbBlockage>` 是必要的：原始 `IdbBlockage` 是 polymorphic base，实际对象分 routing/placement 两类；shadow 用 `_type_sd` 表达类型，并用 name reference 替代运行时 pointer。

## EDADB Write Path

当前 `writeIdbBlockage()`：

- 从 `design->get_blockage_list()` 取得 root vector。
- 空列表返回 `kDbSuccess`，避免 EDADB dispatcher 中断整个写流程。
- 按 vector 顺序构造 `Shadow<IdbBlockage>` pointer vector。
- 写入 `primary_key`、`_order_sd`、type、pushdown、instance name、routing layer name、exceptpgnet 和 rect vector。
- 使用 `edadb::insertVector<Shadow<IdbBlockage>>()` 写入。

这与原始 writer 输出字段一致；空列表返回值是 adapter 层为 dispatcher 做的语义调整。

## EDADB Read Path

当前 `readIdbBlockage()`：

- `blockage_list->reset()` 清空旧数据。
- 通过 `ORDER BY "_order_sd"` 读取 root records，恢复 `IdbBlockageList` 原始 append 顺序。
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

`IdbBlockageList` 需要保持原始 append 顺序。

依据：

- 原始 `parse_blockage()` 按 DEF 出现顺序 append。
- 原始 `write_blockage()` 按 `blockage_list->get_blockage_list()` 当前顺序输出。
- blockage 没有稳定 name identity，主要依赖 vector traversal 和 DEF statement order。
- 当前 shadow 用 `primary_key` 作为 root identity，用 `_order_sd` 保存 list order。
- read path 已显式按 `_order_sd` 恢复，不依赖 EDADB/SQLite read-all 物理顺序。

当前状态：已实现。root identity 和 root order 已分离，`primary_key` 不表达 vector order。

## Tests

- demo `sky130_gcd` 覆盖空列表路径：`writeIdbBlockage insert blockage_count=0`，`readIdbBlockage restored blockage_count=0`。
- `src/database/edadb/test/run_idb_roundtrip_regression.sh` 的 `aux_optional` case 覆盖 routing/placement blockage，并检查 `iBlockageSD` count、`_order_sd`、type、layer name、pushdown 和 exceptpgnet。

## Risks / TODO

- 当前 EDADB adapter 不保存原始 writer 不输出的 blockage fields；如果后续要保持 parser state 而不是 DEF writer-visible state，需要同步扩展 writer、schema、read/write 和测试。
- component name 查找依赖 instance 已先从 EDADB 恢复。
