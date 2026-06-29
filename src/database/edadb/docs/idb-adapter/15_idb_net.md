# IdbNet EDADB Adapter Review

## Scope

`IdbNet` 对应 DEF 的 `NETS` section。

- Write: `DefWrite::write_net()` / `write_net_wire*()`
- Read: `netCallback()` / `DefRead::parse_net()`
- EDADB Write: `DefWriteEdadb::writeIdbNet()`
- EDADB Read: `DefReadEdadb::readIdbNet()`

## Original Write Semantics

原始 `DefWrite::write_net()` 按 `IdbNetList` 顺序输出：

- net name。
- IO pin refs：`( PIN pin )`。
- instance pin refs：`( inst pin )`。
- optional `USE`、`SOURCE`、`ORIGINAL`、`WEIGHT`、`XTALK`、`FIXEDBUMP`、`FREQUENCY`。
- regular wire list，按 wire vector 顺序输出。
- wire segment：按 segment vector 顺序输出 point/via/rect 三类路径。

## Original Read Semantics

原始 `DefRead::parse_net()`：

- 按 DEF 出现顺序 `net_list->add_net(name)`。
- 保存 use/source/original/weight/xtalk/fixedbump/frequency。
- 连接关系按 DEF connection 顺序处理：
  - `PIN` 表示 IO pin，按 pin name 查找并设置 net pointer。
  - 其它 instance/pin pair 按 instance name 和 term pin name 查找，并设置 net pointer。
- wire 读取 routed/fixed/cover/shield state、layer、via、point、virtual point、rect。

## EDADB Schema

当前 schema：

```cpp
TABLE4CLASS(idb::edadb_adapter::NetPinRef, "iNetPinRef",
            (_order_sd, instance_name, pin_name));
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbRegularWireSegment>, "iRegWireSegSD",
                 (primary_key, _layer_name_sd, _via_name_sd,
                  _is_via_sd, _is_rect_sd, _is_second_point_virtual_sd,
                  _delta_rect_sd),
                 (_point_list_sd));
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbRegularWire>, "iRegWireSD",
                 (primary_key, _wire_state_sd, _shield_name_sd),
                 (_segment_list_sd));
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbNet>, "iNetSD",
                 (_net_name_sd, _order_sd, _original_net_name_sd,
                  _connect_type_sd, _source_type_sd, _weight_sd,
                  _xtalk_sd, _fix_bump_sd, _frequency_sd),
                 (_io_pin_name_list_sd, _instance_pin_list_sd,
                  _wire_list_sd));
```

保存字段覆盖当前 DEF writer/read 需要的 net header、pin refs、wire list、segment list、points/via/rect。

## Field Mapping To Original DEF Flow

以下按 EDADB shadow 域列出它对应的原始 DEF read/write 代码位置。这里记录的是 regular `NETS` 的 DEF-visible 语义。

- Net identity / root order: `_net_name_sd`, `_order_sd`
  - Write source: `DefWrite::write_net()` 按 `IdbNetList` 顺序输出 net name，见 `src/database/manager/builder/def_builder/def_write.cpp:827-904`。
  - Read source: `netBeginCallback()` / `parse_net_number()` reserve list，`netCallback()` / `parse_net()` 按 DEF 出现顺序创建 net，见 `src/database/manager/builder/def_builder/def_read.cpp:987-1240`。

- Net header: `_connect_type_sd`, `_source_type_sd`, `_original_net_name_sd`, `_weight_sd`, `_xtalk_sd`, `_fix_bump_sd`, `_frequency_sd`
  - Write source: `write_net()` 输出 USE/SOURCE/ORIGINAL/WEIGHT/XTALK/FIXEDBUMP/FREQUENCY，见 `src/database/manager/builder/def_builder/def_write.cpp:852-889`。
  - Read source: `parse_net()` 读取对应 optional fields，见 `src/database/manager/builder/def_builder/def_read.cpp:1047-1077`。

- IO pin refs: `_io_pin_name_list_sd`
  - Write source: `write_net()` 输出 `( PIN pin )`，见 `src/database/manager/builder/def_builder/def_write.cpp:837-839`。
  - Read source: `parse_net()` 对 `PIN` connection 按 pin name lookup，并设置 net pointer，见 `src/database/manager/builder/def_builder/def_read.cpp:1089-1100`。

- Instance pin refs: `_instance_pin_list_sd`
  - Write source: `write_net()` 输出 `( inst pin )`，见 `src/database/manager/builder/def_builder/def_write.cpp:842-844`。
  - Read source: `parse_net()` 按 instance name 和 term pin name lookup，并设置 net pointer，见 `src/database/manager/builder/def_builder/def_read.cpp:1101-1116`。

- Regular wire root: `Shadow<IdbRegularWire>::_wire_state_sd`, `_shield_name_sd`, `_segment_list_sd`
  - Write source: `write_net()` 遍历 wire list 并调用 `write_net_wire()`，见 `src/database/manager/builder/def_builder/def_write.cpp:891-904`；`write_net_wire()` 输出 wire state 并按 segment 顺序输出，见 `src/database/manager/builder/def_builder/def_write.cpp:905-924`。
  - Read source: `parse_net()` 为每个 DEF wire 创建 `IdbRegularWire`，设置 wire state/shield name，并按 path 创建 segment，见 `src/database/manager/builder/def_builder/def_read.cpp:1119-1237`。

- Segment dispatch: `_is_via_sd`, `_is_rect_sd`, `_is_second_point_virtual_sd`
  - Write source: `write_net_wire_segment()` 按 rect/via/points 三分支分发，见 `src/database/manager/builder/def_builder/def_write.cpp:926-940`；points/via/rect writer 分别见 `src/database/manager/builder/def_builder/def_write.cpp:942-1011`。
  - Read source: `parse_net()` 在 `DEFIPATH_VIA`、`DEFIPATH_RECT`、`DEFIPATH_VIRTUALPOINT` 中设置对应状态，见 `src/database/manager/builder/def_builder/def_read.cpp:1147-1218`。

- Segment layer / via / points / rect: `_layer_name_sd`, `_via_name_sd`, `_point_list_sd`, `_delta_rect_sd`
  - Write source: point/via/rect segment writer 输出 layer、via、point、rect geometry，见 `src/database/manager/builder/def_builder/def_write.cpp:942-1011`。
  - Read source: `parse_net()` 读取 layer/via/point/virtual point/rect，并按 layer/via name lookup，见 `src/database/manager/builder/def_builder/def_read.cpp:1138-1218`。

- Runtime pointers and copied via geometry
  - Write source: DEF writer 输出 names/geometry，不输出 raw layer/via/pin/instance pointers，见 `src/database/manager/builder/def_builder/def_write.cpp:827-1011`。
  - Read source: 原始 `parse_net()` 通过 name lookup 重建 pin/instance/layer/via pointers，并 `copy_via()` 设置 coordinate，见 `src/database/manager/builder/def_builder/def_read.cpp:1081-1237`。

## Child Storage View

`IdbNet` 是 `NETS` root，当前子节点/引用处理如下：

- `_io_pin_name_list_sd`：primitive string vector，保存 IO pin names；read 时按 name 查找 `IdbPin*`。
- `_instance_pin_list_sd`：`NetPinRef` vector，保存 instance name、pin name 和 pin ref order。
- `_wire_list_sd`：`Shadow<IdbRegularWire>` vector，保存 wire order。
- `_segment_list_sd`：`Shadow<IdbRegularWireSegment>` vector，保存 segment order。
- `_point_list_sd`：coordinate child vector，保存 path point order。

不直接保存 `IdbPin*`、`IdbInstance*`、`IdbLayer*`、`IdbVia*`：这些是运行时引用。read 时按 name lookup pin/instance/layer/via，via 按原始 parser 语义 copy 后设置 coordinate。

## Why Net Shadow

当前需要 `Shadow<IdbNet>`：

- root identity 是 net name，`_net_name_sd` 作为 PK。
- `IdbNetList` 需要恢复 DEF append 顺序，不能用 vector order index 当 PK。
- `_order_sd` 单独保存 root list order。
- pin/layer/via/instance 都是 name reference，不应直接持久化 runtime pointers。
- wire/segment/point 是多层 vector child，需要明确 storage view 和顺序。

## EDADB Write Path

当前 `writeIdbNet()`：

- 从 `design->get_net_list()` 取得 regular net vector。
- 空列表返回 `kDbSuccess`，避免 EDADB dispatcher 中断整个写流程。
- 非空时按 list 顺序构造 `Shadow<IdbNet>` pointer vector。
- 写入 `_order_sd`、net header、pin refs、wire/segment/point nested vectors。
- 使用 `edadb::insertVector<Shadow<IdbNet>>()` 写入。

这与原始 DEF writer 输出字段一致；空列表返回值是 adapter 层为 dispatcher 做的语义调整。

## EDADB Read Path

当前 `readIdbNet()`：

- 通过 `ORDER BY "_order_sd"` 读取 root records，恢复 `IdbNetList` 原始 append 顺序。
- 创建 regular net，恢复 original/use/source/weight/xtalk/fixedbump/frequency。
- 恢复 IO pin refs、instance pin refs，并设置 pin 的 net pointer。
- 按 wire/segment/point vector 顺序重建 regular wire。
- layer name 通过 LEF `IdbLayers` 查找。
- via name 优先查 DEF via list，找不到再查 LEF via list，并调用 `segment->copy_via()`。
- rect segment 恢复 delta rect；普通/via segment 恢复 points。
- `createDbByDef()` 不注册 net callbacks，避免 DEF 文本重复创建 regular net。

读取顺序在 via、instance、pin 之后，因此 pin/instance/via lookup 已具备上下文。

## Computed Fields

这些字段不直接入库：

- pin/instance/layer/via pointers：由 name lookup 重建。
- copied via coordinate：由 `segment->copy_via()` 后设置 point end。
- path token 中当前原始 parser 忽略的 mask/viarotation/width/style/taperrule/viadata：当前 adapter 不扩大语义。

## Order / Index

`IdbNetList`、instance pin refs、wire list、segment list、point list 都需要保持顺序。

依据：

- 原始 `parse_net()` 按 DEF 出现顺序 append regular net。
- 原始 `write_net()` 按 `net_list->get_net_list()` 当前顺序输出。
- 原始 writer 也按 pin/wire/segment/point vector 当前顺序输出。
- 当前 root shadow 用 `_net_name_sd` 作为 identity，用 `_order_sd` 保存 root list order。
- instance pin refs 使用 `NetPinRef::_order_sd` 保存 pin ref order。
- wire/segment/point child vector order 由 EDADB vector child 机制保存。
- read path 已显式按 `_order_sd` 恢复 root list，不依赖 EDADB/SQLite read-all 物理顺序。

当前状态：已实现 root order；child vector order 已由 regression 覆盖。

## Tests

- demo `sky130_gcd` 覆盖 regular net count 和 DEF roundtrip。
- `src/database/edadb/test/run_idb_roundtrip_regression.sh` 的 default case 检查 regular net root order 和 default header fields。
- `aux_optional` case 检查 optional original/source/weight/xtalk/fixedbump/frequency。
- `routed_irt` case 检查 routed wire、segment、point counts，segment types，ordered instance pin refs 和 largest routed segment nets。

## Risks / TODO

- 原始 parser 对 mask/viarotation/width/style/taperrule/viadata 等 path token 当前没有完整保存；adapter 保持一致，不单独扩展。
- Shield wire 保存 `_shield_name_sd`，但复杂 shield 文本 roundtrip 仍取决于原始 writer/read 的支持范围。
