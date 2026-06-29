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
  - `segment->is_via()`：输出 layer、width、shape、point 和 via name。
  - `segment->is_rect()`：输出 shape、layer 和 rect。
  - 其它：输出 layer、width、shape 和两点线段。

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
- 如果存在 pin string，则调用 `get_pin_list_by_names()` 补回 instance pin list 和 instance list；此时 DEF 文本并没有显式 IO/instance pin refs。
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

连接字段必须遵守原始 writer/parser 的互斥语义：

- 如果 `_pin_string_list_sd` 非空，只保存 pin-string 形式，不保存 resolved IO/instance refs。
- 如果 `_pin_string_list_sd` 为空，才保存 `_io_pin_name_list_sd` 和 `_instance_pin_list_sd`。

## Field Mapping To Original DEF Flow

以下按 EDADB shadow 域列出它对应的原始 DEF read/write 代码位置。这里记录的是语义来源，不是 C++ object dump。

- Net identity / root order: `_net_name_sd`, `_order_sd`
  - Write source: `DefWrite::write_special_net()` 遍历 `special_net_list->get_net_list()` 并输出 net name，见 `src/database/manager/builder/def_builder/def_write.cpp:775-778`。
  - Read source: `DefRead::parse_pdn()` 按 DEF 出现顺序 `net_list->add_net(def_net->name())`，见 `src/database/manager/builder/def_builder/def_read.cpp:1315-1320`。
  - EDADB adapter: `_net_name_sd` 作为 identity，`_order_sd` 保存 root list 顺序；read 使用 `ORDER BY "_order_sd"`，见 `src/database/manager/builder/def_builder/def_read_edadb.cpp:1015-1019`。

- Net header: `_connect_type_sd`, `_source_type_sd`, `_original_net_name_sd`, `_weight_sd`
  - Write source: `USE/SOURCE/ORIGINAL/WEIGHT` 输出，见 `src/database/manager/builder/def_builder/def_write.cpp:796-810`。
  - Read source: `hasUse()/hasSource()/hasWeight()/hasOriginal()` 恢复字段，见 `src/database/manager/builder/def_builder/def_read.cpp:1322-1335`。
  - EDADB adapter: `Shadow<IdbSpecialNet>::toShadow()` 保存字段，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:132-137`；`readSpecialNet()` 恢复字段，见 `src/database/manager/builder/def_builder/def_read_edadb.cpp:1035-1039`。

- Pin-string refs: `_pin_string_list_sd`
  - Write source: `pin_string_list` 非空时输出 `( * pin )`，见 `src/database/manager/builder/def_builder/def_write.cpp:780-783`。
  - Read source: `instance == "*"` 时 `add_pin_string()`，随后 `get_pin_list_by_names()` 计算 instance pin refs，见 `src/database/manager/builder/def_builder/def_read.cpp:1338-1342` 和 `src/database/manager/builder/def_builder/def_read.cpp:1368-1371`。
  - EDADB adapter: `toShadow()` 保存 pin strings，且非空时不保存 resolved IO/instance refs，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:139-156`；read 时同样走 pin-string 分支，见 `src/database/manager/builder/def_builder/def_read_edadb.cpp:1041-1047`。

- Explicit IO pin refs: `_io_pin_name_list_sd`
  - Write source: 仅当没有 `pin_string_list` 时输出 `( PIN pin )`，见 `src/database/manager/builder/def_builder/def_write.cpp:784-787`。
  - Read source: `instance == "PIN"` 时按 pin name lookup，并设置 `pin->set_special_net(net)`，见 `src/database/manager/builder/def_builder/def_read.cpp:1343-1350`。
  - EDADB adapter: 仅在 `_pin_string_list_sd` 为空时保存/恢复，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:143-146` 和 `src/database/manager/builder/def_builder/def_read_edadb.cpp:1047-1054`。

- Explicit instance pin refs: `_instance_pin_list_sd`
  - Write source: 仅当没有 `pin_string_list` 时输出 `( inst pin )`，见 `src/database/manager/builder/def_builder/def_write.cpp:789-791`。
  - Read source: 普通 instance/pin pair 按 instance name 和 term pin name lookup，并设置 `pin->set_special_net(net)`，见 `src/database/manager/builder/def_builder/def_read.cpp:1351-1365`。
  - EDADB adapter: `SpecialNetPinRef` 保存 instance name、pin name 和 pin ref order，且仅在 `_pin_string_list_sd` 为空时启用，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:148-155`；read 时按 `_order_sd` 恢复，见 `src/database/manager/builder/def_builder/def_read_edadb.cpp:1056-1068`。

- Wire root: `Shadow<IdbSpecialWire>::_wire_state_sd`, `_shield_name_sd`, `_segment_list_sd`
  - Write source: `write_special_net()` 遍历 wire list 并调用 `write_specialnet_wire(wire)`，见 `src/database/manager/builder/def_builder/def_write.cpp:813-815`；`write_specialnet_wire()` 输出 wire state 并按 segment 顺序调用 segment writer，见 `src/database/manager/builder/def_builder/def_write.cpp:744-764`。
  - Read source: `parse_pdn_wire()` 为每个 DEF wire 创建 `IdbSpecialWire`，设置 wire state/shield name，并按 path 创建 segment，见 `src/database/manager/builder/def_builder/def_read.cpp:1394-1405`。
  - EDADB adapter: `Shadow<IdbSpecialWire>::toShadow()` 保存 wire state、shield name 和 segment vector，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:94-103`；read 时恢复 wire 并初始化 segment list，见 `src/database/manager/builder/def_builder/def_read_edadb.cpp:1071-1078`。

- Segment dispatch: `_is_via_sd`, `_is_rect_sd`
  - Write source: `write_specialnet_wire_segment()` 按 `is_via()` / `is_rect()` / points 三分支分发，见 `src/database/manager/builder/def_builder/def_write.cpp:731-739`。
  - Read source: DEF path 中 `DEFIPATH_VIA` 设置 `set_is_via(true)`，见 `src/database/manager/builder/def_builder/def_read.cpp:1416-1431`；DEF rectangles 在 `parse_pdn_rects()` 中设置 `set_is_rect(true)`，见 `src/database/manager/builder/def_builder/def_read.cpp:1486-1508`。
  - EDADB adapter: segment shadow 保存 `_is_via_sd/_is_rect_sd`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:49-50`；read 时恢复 flag，见 `src/database/manager/builder/def_builder/def_read_edadb.cpp:1080-1084`。

- Segment layer / width / shape / style: `_layer_name_sd`, `_route_width_sd`, `_shape_type_sd`, `_style_sd`
  - Write source: point/via segment 输出 layer、route width 和 optional shape，见 `src/database/manager/builder/def_builder/def_write.cpp:651-710`；rect segment 输出 shape/layer/rect，见 `src/database/manager/builder/def_builder/def_write.cpp:712-728`。
  - Read source: `DEFIPATH_LAYER/WIDTH/SHAPE/STYLE` 分别恢复 layer、route width、shape、style，见 `src/database/manager/builder/def_builder/def_read.cpp:1412-1455`；rect layer/shape 在 `parse_pdn_rects()` 中恢复，见 `src/database/manager/builder/def_builder/def_read.cpp:1494-1505`。
  - EDADB adapter: segment shadow 保存字段，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:44-48`；read 时按 layer name lookup，并恢复 width/shape/style，见 `src/database/manager/builder/def_builder/def_read_edadb.cpp:1080-1095`。

- Segment via: `_via_name_sd`
  - Write source: via segment 输出 via name，见 `src/database/manager/builder/def_builder/def_write.cpp:677-710`。
  - Read source: `DEFIPATH_VIA` 按 DEF via list、LEF via list lookup，`copy_via()` 后设置 coordinate，见 `src/database/manager/builder/def_builder/def_read.cpp:1416-1430`。
  - EDADB adapter: segment shadow 保存 via name，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:45`；read 时优先 DEF via、再 LEF via lookup，并 `copy_via()`，见 `src/database/manager/builder/def_builder/def_read_edadb.cpp:1106-1121`。

- Segment points: `_point_list_sd`
  - Write source: point/via segment 输出 point list 中的起点和可选第二点，见 `src/database/manager/builder/def_builder/def_write.cpp:651-710`。
  - Read source: `DEFIPATH_POINT` 调用 `segment->add_point(x, y)`，见 `src/database/manager/builder/def_builder/def_read.cpp:1439-1445`。
  - EDADB adapter: segment shadow 保存 point vector，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:56-59`；read 时顺序恢复 points，见 `src/database/manager/builder/def_builder/def_read_edadb.cpp:1097-1099`。

- Segment rect: `_delta_rect_sd`
  - Write source: rect segment 输出 `RECT` 坐标，见 `src/database/manager/builder/def_builder/def_write.cpp:712-728`。
  - Read source: `parse_pdn_rects()` 读取 `xl/yl/xh/yh` 并 `set_delta_rect()`，见 `src/database/manager/builder/def_builder/def_read.cpp:1494-1508`。
  - EDADB adapter: segment shadow 保存 delta rect，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:52-54`；read 时恢复 delta rect，见 `src/database/manager/builder/def_builder/def_read_edadb.cpp:1101-1104`。

- Computed geometry: segment bounding box
  - Write source: DEF writer 不直接输出 bounding box，只由 segment points/rect/via 间接决定。
  - Read source: 原始 parser 在 wire path 和 rect path 末尾调用 `segment->set_bounding_box()`，见 `src/database/manager/builder/def_builder/def_read.cpp:1474` 和 `src/database/manager/builder/def_builder/def_read.cpp:1508`。
  - EDADB adapter: 不保存 bounding box；read 末尾重新调用 `set_bounding_box()`，见 `src/database/manager/builder/def_builder/def_read_edadb.cpp:1124`。

## Child Storage View

`IdbSpecialNet` 是 `SPECIALNETS` root，当前子节点/引用处理如下：

- `_pin_string_list_sd`：primitive string vector，保存 `( * pin )` 形式和顺序；非空时作为唯一连接存储视图。
- `_io_pin_name_list_sd`：primitive string vector，保存 IO pin names；仅在没有 pin string 时启用，read 时按 name 查找 `IdbPin*`。
- `_instance_pin_list_sd`：`SpecialNetPinRef` vector，保存 instance name、pin name 和 pin ref order；仅在没有 pin string 时启用。
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
- pin refs 按 `DefWrite::write_special_net()` 的分支写入：`pin_string_list` 非空时只写 pin strings；否则写 IO pin refs 和 instance pin refs。
- segment shadow 按 `DefWrite::write_specialnet_wire_segment()` 的分支保存：via/rect/point 三类由 `_is_via_sd`、`_is_rect_sd` 和对应字段决定。
- 使用 `edadb::insertVector<Shadow<IdbSpecialNet>>()` 写入。

这与原始 DEF writer 输出字段一致；空列表返回值是 adapter 层为 dispatcher 做的语义调整。

## EDADB Read Path

当前 `readSpecialNet()`：

- 通过 `ORDER BY "_order_sd"` 读取 root records，恢复 `IdbSpecialNetList` 原始 append 顺序。
- 创建 special net，恢复 original/use/source/weight。
- 如果存在 pin string，只恢复 pin string，然后按原始 `parse_pdn()` 调用 `get_pin_list_by_names()` 补回 instance pin list 和 instance list。
- 如果不存在 pin string，才恢复 IO pin refs、instance pin refs，并设置 pin 的 special net pointer。
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
- pin-string resolved instance pin refs：由 `get_pin_list_by_names()` 根据当前 instance list 计算，不作为 DB 显式连接记录保存。
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
- `src/database/edadb/test/run_idb_roundtrip_regression.sh` 的 default case 检查 special net root order、header fields、pin string/wire/segment/point child counts、pin-string 与 resolved pin refs 的互斥、via/rect/point segment dispatch 类型。
- `aux_optional` case 检查 optional original/source/weight。
- `routed_irt` case 间接覆盖 routed database 与 regular net 共存场景。

## Risks / TODO

- 原始 writer 的 rect segment 输出中使用当前 iEDA 逻辑；adapter 不单独修正 writer 行为。
- Shield wire 当前原始 writer 不完整支持；EDADB 保存 `_shield_name_sd`，但是否可完整文本 roundtrip 仍取决于原始 writer。
- 如果后续支持 more DEF path tokens，如 mask/viarotation/viadata，需要同步扩展 schema/read/write/test。
