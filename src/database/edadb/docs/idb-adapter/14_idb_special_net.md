# IdbSpecialNet EDADB Adapter Review

## Scope

`IdbSpecialNet` 对应 DEF 的 `SPECIALNETS` section。

- Write: `DefWrite::write_special_net()` / `write_specialnet_wire*()`
- Read: `specialNetCallback()` / `DefRead::parse_special_net()` / `parse_pdn()`
- EDADB Write: `DefWriteEdadb::writeSpecialNet()`
- EDADB Read: `DefReadEdadb::readSpecialNet()`

## Original Write Semantics

原始 `DefWrite::write_special_net()` 按 `IdbSpecialNetList` 顺序输出：

- net name。
- pin refs：优先输出 `pin_string_list`；否则输出 IO pin refs 和 instance pin refs。
- `USE`，由 `connect_type` 输出。
- optional `SOURCE`、`ORIGINAL`、`WEIGHT`。
- special wire list，按 wire vector 顺序输出。
- wire segment：按 segment vector 顺序输出 point/via/rect 三类路径。

原始 writer 对 shield wire 当前直接返回 `kDbFail`，但 caller 不检查这个局部返回值；当前 adapter 不扩大该语义。

## Original Read Semantics

原始 `DefRead::parse_special_net()`：

- 只有 `USE` 被识别为 PDN 时进入 `parse_pdn()`；如果 use 是 regular net，则转入 `parse_net()`。
- `parse_pdn()` 按 DEF 出现顺序 `special_net_list->add_net(name)`。
- 保存 use/source/original/weight。
- 连接关系按 DEF connection 顺序处理：
  - `*` instance 表示 pin string，加入 `pin_string_list`。
  - `PIN` instance 表示 IO pin，按 pin name 查找并设置 special net pointer。
  - 其它 instance/pin pair 按 instance name 和 term pin name 查找，并设置 special net pointer。
- 如果存在 pin string，则调用 `get_pin_list_by_names()` 补回 instance pin list 和 instance list。
- `parse_pdn_wire()` 读取 routed/fixed/cover/shield wire、layer、via、width、point、shape、style。
- `parse_pdn_rects()` 读取 special net rect，作为 rect segment 重建。

## EDADB Schema

当前 schema：

```cpp
TABLE4CLASS(idb::edadb_adapter::SpecialNetPinRef, "iSpecPinRef",
            (_order_sd, instance_name, pin_name));
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSpecialWireSegment>, "iSpecWireSegSD",
                 (primary_key, _layer_name_sd, _via_name_sd, _route_width_sd,
                  _style_sd, _shape_type_sd, _is_via_sd, _is_rect_sd, _delta_rect_sd),
                 (_point_list_sd));
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSpecialWire>, "iSpecWireSD",
                 (primary_key, _wire_state_sd, _shield_name_sd), (_segment_list_sd));
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSpecialNet>, "iSpecNetSD",
                 (_net_name_sd, _order_sd, _original_net_name_sd,
                  _connect_type_sd, _source_type_sd, _weight_sd),
                 (_pin_string_list_sd, _io_pin_name_list_sd,
                  _instance_pin_list_sd, _wire_list_sd));
```

保存字段覆盖当前 DEF writer/read 需要的 net header、pin refs、wire list、segment list、points/via/rect。

## Child Storage View

`IdbSpecialNet` 是 `SPECIALNETS` root，当前子节点/引用处理如下：

- `_pin_string_list_sd`：primitive string vector，保存 `( * pin )` 形式和顺序。
- `_io_pin_name_list_sd`：primitive string vector，保存 IO pin names；read 时按 name 查找 `IdbPin*`。
- `_instance_pin_list_sd`：`SpecialNetPinRef` vector，保存 instance name、pin name 和 pin ref order。
- `_wire_list_sd`：`Shadow<IdbSpecialWire>` vector，保存 wire order。
- `_segment_list_sd`：`Shadow<IdbSpecialWireSegment>` vector，保存 segment order。
- `_point_list_sd`：coordinate child vector，保存 path point order。

不直接保存 `IdbPin*`、`IdbInstance*`、`IdbLayer*`、`IdbVia*`：这些都是运行时引用。read 时按 name lookup pin/instance/layer/via，via 按原始 parser 语义 copy 后设置 coordinate。

不保存 `IdbSpecialNetEdgeSegmenArray`：这是后续 routing/PDN 分析用的派生 edge structure，不是 DEF `SPECIALNETS` 直接读写字段。

## Why SpecialNet Shadow

当前需要 `Shadow<IdbSpecialNet>`：

- root identity 是 net name，`_net_name_sd` 作为 PK。
- `IdbSpecialNetList` 需要恢复 DEF append 顺序，不能用 vector order index 当 PK。
- `_order_sd` 单独保存 root list order。
- pin/layer/via/instance 都是 name reference，不应直接持久化 runtime pointers。
- wire/segment/point 是多层 vector child，需要明确 storage view 和顺序。

## EDADB Write Path

当前 `writeSpecialNet()`：

- 从 `design->get_special_net_list()` 取得 special net vector。
- 空列表返回 `kDbSuccess`，避免 EDADB dispatcher 中断整个写流程。
- 非空时按 list 顺序构造 `Shadow<IdbSpecialNet>` pointer vector。
- 写入 `_order_sd`、net header、pin refs、wire/segment/point nested vectors。
- 使用 `edadb::insertVector<Shadow<IdbSpecialNet>>()` 写入。

这与原始 DEF writer 输出字段一致；空列表返回值是 adapter 层为 dispatcher 做的语义调整。

## EDADB Read Path

当前 `readSpecialNet()`：

- 通过 `ORDER BY "_order_sd"` 读取 root records，恢复 `IdbSpecialNetList` 原始 append 顺序。
- 创建 special net，恢复 original/use/source/weight。
- 恢复 pin string、IO pin refs、instance pin refs，并设置 pin 的 special net pointer。
- 按 wire/segment/point vector 顺序重建 special wire。
- layer name 通过 LEF `IdbLayers` 查找。
- via name 优先查 DEF via list，找不到再查 LEF via list，并调用 `segment->copy_via()`。
- rect segment 恢复 delta rect；普通/via segment 恢复 points；最后调用 `set_bounding_box()`。
- `createDbByDef()` 不注册 special net callbacks，避免 DEF 文本重复创建 special net。

读取顺序在 via、instance、pin 之后，因此 pin/instance/via lookup 已具备上下文。

## Computed Fields

这些字段不直接入库：

- pin/instance/layer/via pointers：由 name lookup 重建。
- segment bounding box：由 points/rect/via 信息调用 `set_bounding_box()` 重建。
- copied via coordinate：由 `segment->copy_via()` 后设置 point start。
- edge segment array：后续按 special wire geometry 重新构建或由使用方维护。

## Order / Index

`IdbSpecialNetList`、pin refs、wire list、segment list、point list 都需要保持顺序。

依据：

- 原始 `parse_pdn()` 按 DEF 出现顺序 append special net。
- 原始 `write_special_net()` 按 `special_net_list->get_net_list()` 当前顺序输出。
- 原始 writer 也按 pin/wire/segment/point vector 当前顺序输出。
- 当前 root shadow 用 `_net_name_sd` 作为 identity，用 `_order_sd` 保存 root list order。
- instance pin refs 使用 `SpecialNetPinRef::_order_sd` 保存 pin ref order。
- wire/segment/point child vector order 由 EDADB vector child 机制保存。
- read path 已显式按 `_order_sd` 恢复 root list，不依赖 EDADB/SQLite read-all 物理顺序。

当前状态：已实现 root order；child vector order 已由 regression 覆盖。

## Tests

- demo `sky130_gcd` 覆盖非空 special net：`writeSpecialNet insert special_net_count=2 segment_count=639`，`readSpecialNet restored special_net_count=2 segment_count=639`。
- `src/database/edadb/test/run_idb_roundtrip_regression.sh` 的 default case 检查 special net root order、header fields、pin string/wire/segment/point child counts。
- `aux_optional` case 检查 optional original/source/weight。
- `routed_irt` case 间接覆盖 routed database 与 regular net 共存场景。

## Risks / TODO

- 原始 writer 的 rect segment 输出中使用当前 iEDA 逻辑；adapter 不单独修正 writer 行为。
- Shield wire 当前原始 writer 不完整支持；EDADB 保存 `_shield_name_sd`，但是否可完整文本 roundtrip 仍取决于原始 writer。
- 如果后续支持 more DEF path tokens，如 mask/viarotation/viadata，需要同步扩展 schema/read/write/test。
