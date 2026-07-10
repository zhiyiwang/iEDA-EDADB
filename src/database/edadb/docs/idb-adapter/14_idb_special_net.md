# IdbSpecialNet EDADB Adapter Review

## Scope

`IdbSpecialNet` 对应 DEF 的 `SPECIALNETS` section。

- Write: `DefWrite::write_special_net()` / `write_specialnet_wire*()`
- Read: `specialNetCallback()` / `DefRead::parse_special_net()` / `parse_pdn()`
- EDADB Write: `DefWriteEdadb::writeIdbSpecialNet()`
- EDADB Read: `DefReadEdadb::readIdbSpecialNet()`

本文件按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 的约束检查：

- DEF section 映射：`SPECIALNETS` section。
- iEDA root container：`IdbSpecialNetList::_net_list`。
- root-vector order 等级：Level D，当前点工具按 PDN net name 查找，不依赖 `IdbSpecialNetList::_net_list` root index/order。
- root identity 约束：special net name 是 DEF-visible identity，当前 `_net_name_sd` 是 EDADB root PK；不使用 vector order index 作为 PK。
- nested vector 约束：pin-string / IO pin / instance pin ref / wire / segment / point vectors 都是 net record 内部语义，必须随 root record 保持原始顺序，不参与 D-level root sort。

## Original Write Semantics

原始 `DefWrite::write_special_net()` 按 `IdbSpecialNetList` 顺序输出：

- net name。
- pin refs：优先输出 `pin_string_list`；否则输出 IO pin refs 和 instance pin refs。
- `USE`，由 `connect_type` 输出。
- optional `SOURCE`、`ORIGINAL`、`WEIGHT`。
- special wire list，按 wire vector 顺序输出。
- wire segment：按 segment vector 顺序输出 point/via/rect 三类路径。
  - `segment->is_via()`：输出 layer、width、shape、point 和 via name。
  - `segment->is_rect()`：输出 shape、layer 和 rect；当前 writer 使用现有 iEDA 输出逻辑，不在 adapter 中修正。
  - 其它：输出 layer、width、shape 和两点线段。

原始 writer 对 shield wire 当前直接返回 `kDbFail`，不会输出 shield wire 后续内容；caller 不检查这个局部返回值。当前 adapter 保存 shield name，但不扩大原始 writer 的文本输出能力。

## Original Read Semantics

原始 `DefRead::parse_special_net()`：

- 只有 `USE` 被识别为 PDN 时进入 `parse_pdn()`；如果 use 是 regular net，则转入 `parse_net()`。
- `parse_pdn()` 按 DEF 出现顺序 `special_net_list->add_net(name)`。
- 保存 use/source/original/weight。
- 连接关系按 DEF connection 顺序处理：
  - `*` instance 表示 pin string，加入 `pin_string_list`。
  - `PIN` instance 表示 IO pin，按 pin name 查找并设置 special net pointer。
  - 其它 instance/pin pair 按 instance name 和 term pin name 查找，并设置 special net pointer。
- 如果存在 pin string，则调用 `get_pin_list_by_names()` 补回 instance pin list 和 instance list。注意：原始 parser 本身允许逐条解析混合 connection token；当前 adapter 按原始 writer 可输出语义处理，pin string 非空时不额外保存 resolved IO/instance refs。
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
                 (_net_name_sd, _original_net_name_sd,
                  _connect_type_sd, _source_type_sd, _weight_sd),
                 (_pin_string_list_sd, _io_pin_name_list_sd,
                  _instance_pin_list_sd, _wire_list_sd));
```

Schema / init 代码位置：

- `iSpecPinRef` table macro: `src/database/edadb/idb/edadb_idb_schema.h:118`
- `iSpecWireSegSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:119`
- `iSpecWireSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:122`
- `iSpecNetSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:124`
- Root table registration: `src/database/edadb/idb/edadb_idb_init.cpp:93`
- Shadow definitions: `src/database/edadb/idb/shadow/shadow_idb_special_net.h:14`

保存字段覆盖当前 DEF writer/read 需要的 net header、pin refs、wire list、segment list、points/via/rect。
`SpecialNetPinRef` / `Shadow<IdbSpecialWireSegment>` / `Shadow<IdbSpecialWire>` 只作为 `Shadow<IdbSpecialNet>` 的 vector child storage view 保留 schema metadata，不单独 root init；child tables 由 `Shadow<IdbSpecialNet>` 的 schema tree 创建和读写。

连接字段必须遵守原始 writer 的互斥输出语义：

- 如果 `_pin_string_list_sd` 非空，只保存 pin-string 形式，不保存由 pin string 推导出的 resolved IO/instance refs。
- 如果 `_pin_string_list_sd` 为空，才保存 `_io_pin_name_list_sd` 和 `_instance_pin_list_sd`。

Schema 与 order/index 约束的关系：

- 依据 `src/database/edadb/docs/def-ieda-mapping-and-order.md`，`SPECIALNETS` 映射到 `IdbSpecialNetList::_net_list`，等级为 Level D。
- 当前 adapter 不保存 root `_order_sd`，read path 不指定 root order；root order-only 文本差异由 normalized diff 处理。
- `_net_name_sd` 是 special net root identity；它不表达 vector order。
- `SpecialNetPinRef::_order_sd` 仍保留，因为它是 instance pin ref nested vector 的 local identity/order，不是 special-net root order。
- wire / segment / point child vector 顺序由 EDADB vector child 机制保存。

Primary-key audit:

- `Shadow<IdbSpecialNet>` 使用 `_net_name_sd` 作为 root identity。
- `Shadow<IdbSpecialWire>` / `Shadow<IdbSpecialWireSegment>` 使用 `primary_key` 作为 nested vector-owner identity，用于挂接 segment / point child rows。
- `SpecialNetPinRef::_order_sd` 是 child table local PK/order；EDADB child table PK 会组合 ancestor FK + local PK，因此它只在 owning special-net record 内表达 pin-ref 顺序，不是 special-net root PK。
- `Shadow<IdbCoordinate<int32_t>>` 和 `Shadow<IdbRect>` 的 PK 已关闭；它们是 child vector element / inline value storage view。

## Class-by-Class Roundtrip Mapping

阅读这个表时按从左到右理解：

- `DEF/LEF field + iDB member`：当前行对应的 DEF/LEF 域名，以及 iEDA 中被读写的成员对象。
- `iDB write`：原始 `DefWrite` 从 iDB 对象输出 DEF 文本的语义。
- `toShadow`：EDADB 从 iDB 对象抽取的存储视图。
- `iDB DEF read`：原始 `DefRead` 从 DEF 文本重建 iDB 对象的语义。
- `fromShadow`：EDADB 从存储视图恢复 iDB 对象的语义。
- `Review conclusion`：当前实现与原始 DEF read/write 的一致性或差异。

表格行顺序按原始 `DefWrite` 输出 DEF 的顺序组织；如果原始 `DefRead` 的解析顺序不同，则在 `iDB DEF read` 列写对应恢复位置。

### `Shadow<idb::IdbSpecialNet>`

| DEF/LEF field + iDB member | iDB write | toShadow | iDB DEF read | fromShadow | Review conclusion |
| --- | --- | --- | --- | --- | --- |
| `SPECIALNETS - <net_name>`；`IdbSpecialNet::_net_name` | 输出 net name，见 `src/database/manager/builder/def_builder/def_write.cpp:777-778` | 保存 `_net_name_sd`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:227` | `parse_pdn()` 调 `net_list->add_net(def_net->name())`，见 `src/database/manager/builder/def_builder/def_read.cpp:1314-1315` | `readIdbSpecialNet()` 用 `_net_name_sd` 创建 root object，见 `src/database/manager/builder/def_builder/def_read_edadb.cpp:989` | root identity 一致；Level D root order 不保存。 |
| connection refs；`IdbSpecialNet::_pin_string_list/_io_pin_list/_instance_pin_list/_instance_list` | 按互斥规则输出 connection refs：pin string 非空时只输出 `( * pin )`，否则输出 `( PIN pin )` 和 `( inst pin )`，见 `src/database/manager/builder/def_builder/def_write.cpp:780-792` | 保存 `_pin_string_list_sd`；仅当其为空时保存 `_io_pin_name_list_sd/_instance_pin_list_sd`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:233-249` | `parse_pdn()` 逐 connection 分发 `*` / `PIN` / instance 三分支，见 `src/database/manager/builder/def_builder/def_read.cpp:1338-1365` | `fromShadow()` 先恢复 pin strings；若非空则调用 `get_pin_list_by_names()`，否则恢复 IO/instance refs，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:267-295` | 当前贴近原始 writer 的互斥输出语义；不是保存任意混合 DEF connection token。 |
| `+ USE`；`IdbSpecialNet::_connect_type` | 输出 `USE`，见 `src/database/manager/builder/def_builder/def_write.cpp:796-797` | 保存 `_connect_type_sd`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:229` | `parse_pdn()` 用 `hasUse()` 恢复 `connect_type`，见 `src/database/manager/builder/def_builder/def_read.cpp:1322-1324` | `fromShadow()` 调 `set_connect_type()`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:263` | 直接 roundtrip。 |
| `+ SOURCE`；`IdbSpecialNet::_source_type` | 条件输出 `SOURCE`，见 `src/database/manager/builder/def_builder/def_write.cpp:799-802` | 保存 `_source_type_sd`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:230` | `parse_pdn()` 用 `hasSource()` 恢复 `source_type`，见 `src/database/manager/builder/def_builder/def_read.cpp:1326-1328` | `fromShadow()` 调 `set_source_type()`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:264` | 直接 roundtrip。 |
| `+ ORIGINAL`；`IdbSpecialNet::_original_net_name` | 条件输出 `ORIGINAL`，见 `src/database/manager/builder/def_builder/def_write.cpp:804-807` | 保存 `_original_net_name_sd`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:228` | `parse_pdn()` 用 `hasOriginal()` 恢复 original name，见 `src/database/manager/builder/def_builder/def_read.cpp:1334-1336` | `fromShadow()` 调 `set_original_net_name()`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:262` | 直接 roundtrip。 |
| `+ WEIGHT`；`IdbSpecialNet::_weight` | 条件输出 `WEIGHT`，见 `src/database/manager/builder/def_builder/def_write.cpp:809-810` | 保存 `_weight_sd`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:231` | `parse_pdn()` 用 `hasWeight()` 恢复 weight，见 `src/database/manager/builder/def_builder/def_read.cpp:1330-1332` | `fromShadow()` 调 `set_weight()`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:265` | 直接 roundtrip。 |
| special wire records；`IdbSpecialNet::_wire_list` | 遍历 special wire list 并调用 `write_specialnet_wire()`，见 `src/database/manager/builder/def_builder/def_write.cpp:813-815` | 保存 `_wire_list_sd`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:252-257` | `parse_pdn()` 取 `net->get_wire_list()` 后调用 `parse_pdn_wire()` 和 `parse_pdn_rects()`，见 `src/database/manager/builder/def_builder/def_read.cpp:1373-1375` | `fromShadow()` 遍历 `_wire_list_sd`，创建 `IdbSpecialWire` 并交给 wire shadow，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:298-307` | wire child vector 顺序由 EDADB child vector 保存。 |

### Connection Branch Mapping

`parse_pdn()` 的 connection restore 是逐条 DEF connection 分发；当前 EDADB storage 是从 iDB writer 可输出状态抽取的 shadow view。1343/1351 分支在 EDADB 中有对应实现，但被原始 writer 的互斥规则包住。

| DEF/LEF field + iDB member | iDB write | toShadow | iDB DEF read | fromShadow | Review conclusion |
| --- | --- | --- | --- | --- | --- |
| `( * pin )`；`IdbSpecialNet::_pin_string_list` | `pin_string_list` 非空时输出 `( * pin )`，见 `src/database/manager/builder/def_builder/def_write.cpp:780-783` | 保存 `_pin_string_list_sd`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:233-235` | `instance == "*"` 时 `add_pin_string()`，见 `src/database/manager/builder/def_builder/def_read.cpp:1341-1342` | `restorePinStringConnection()` 调 `add_pin_string()`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:324-325` | `*` branch 直接对应。 |
| `( PIN pin )`；`IdbSpecialNet::_io_pin_list` + `IdbPin::_special_net` | `pin_string_list` 为空时输出 `( PIN pin )`，见 `src/database/manager/builder/def_builder/def_write.cpp:784-787` | 仅在 `_pin_string_list_sd` 为空时保存 `_io_pin_name_list_sd`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:237-240` | `instance == "PIN"` 时 lookup IO pin、`add_io_pin()`、`pin->set_special_net()`，见 `src/database/manager/builder/def_builder/def_read.cpp:1343-1350` | `restoreIoPinConnection()` lookup IO pin、`add_io_pin()`、`set_special_net()`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:281-285` 和 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:328-340` | `PIN` branch 已实现；只在没有 pin strings 时启用。 |
| `( inst pin )`；`IdbSpecialNet::_instance_list/_instance_pin_list` + `IdbPin::_special_net` | `pin_string_list` 为空时输出 `( inst pin )`，见 `src/database/manager/builder/def_builder/def_write.cpp:789-791` | 仅在 `_pin_string_list_sd` 为空时保存 `_instance_pin_list_sd`；`_order_sd` 保存 nested pin-ref order，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:242-249` | 普通 instance name 分支 lookup instance、`add_instance()`、lookup term pin、`add_instance_pin()`、`set_special_net()`，见 `src/database/manager/builder/def_builder/def_read.cpp:1351-1365` | 按 `_order_sd` 排序后 `restoreInstancePinConnection()`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:287-295` 和 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:343-360` | instance/pin branch 已实现；只在没有 pin strings 时启用。 |
| pin-string derived refs；`IdbSpecialNet::_instance_pin_list/_instance_list` | writer 不输出 pin-string 推导出的 resolved refs | 不保存 pin-string 推导出的 resolved refs | 如果 `pin_string_list` 非空，`parse_pdn()` 调 `get_pin_list_by_names()` 补回 instance pin list 和 instance list，见 `src/database/manager/builder/def_builder/def_read.cpp:1368-1371` | pin strings 非空时调用同一个 `get_pin_list_by_names()`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:272-279` | resolved refs 是运行时计算状态，不是显式 DB 字段。 |

如果未来目标改为保存任意混合 connection DEF token，需要新增统一的 ordered connection record，例如 `type/order/instance_name/pin_name`，再按 `parse_pdn()` 的三分支逐条恢复；当前实现的目标是匹配 iEDA 原始 writer 可输出的 roundtrip 语义。

### `idb::edadb_adapter::SpecialNetPinRef`

| DEF/LEF field + iDB member | iDB write | toShadow | iDB DEF read | fromShadow | Review conclusion |
| --- | --- | --- | --- | --- | --- |
| `( inst pin )` ref element；`IdbPin::_instance/_term/_pin_name` | `DefWrite::write_special_net()` 按 `special_net->get_instance_pin_list()->get_pin_list()` 当前顺序输出 `( inst pin )`，见 `src/database/manager/builder/def_builder/def_write.cpp:789-791` | `SpecialNetPinRef` 保存 `instance_name`、`pin_name`、`_order_sd`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:242-248` | `parse_pdn()` 普通 instance branch 按 DEF connection 出现顺序 append instance/pin refs，见 `src/database/manager/builder/def_builder/def_read.cpp:1351-1365` | `fromShadow()` 按 `_order_sd` 排序后恢复 instance/pin refs，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:287-295` | `_order_sd` 是 child table local PK/order；不是 special-net root order，也不是 root PK。 |

### `Shadow<idb::IdbSpecialWire>`

| DEF/LEF field + iDB member | iDB write | toShadow | iDB DEF read | fromShadow | Review conclusion |
| --- | --- | --- | --- | --- | --- |
| routing statement `+ ROUTED/+ FIXED/+ COVER`；`IdbSpecialWire::_wire_state` | `write_specialnet_wire()` 输出非 shield wire state，见 `src/database/manager/builder/def_builder/def_write.cpp:744-755` | 保存 `_wire_state_sd`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:174` | `parse_pdn_wire()` 创建 wire 并 `set_wire_state()`，见 `src/database/manager/builder/def_builder/def_read.cpp:1394-1397`；`parse_pdn_rects()` 对 rect wire 设置 route status，见 `src/database/manager/builder/def_builder/def_read.cpp:1486-1489` | `Shadow<IdbSpecialWire>::fromShadow()` 恢复 `_wire_state_sd`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:185-186` | 非 shield wire state 直接 roundtrip。 |
| `+ SHIELD <net>`；`IdbSpecialWire::_shield_name` | shield wire 当前在 writer 中直接返回 `kDbFail`，不会输出 shield path 内容，见 `src/database/manager/builder/def_builder/def_write.cpp:748-753` | 保存 `_shield_name_sd`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:175` | `parse_pdn_wire()` / `parse_pdn_rects()` 对 shield wire 保存 shield name，见 `src/database/manager/builder/def_builder/def_read.cpp:1398-1400` 和 `src/database/manager/builder/def_builder/def_read.cpp:1490-1492` | `fromShadow()` 恢复 `_shield_name_sd`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:187` | adapter 保存 iDB 状态；完整 shield 文本 roundtrip 仍受原始 writer 限制。 |
| wire segment list；`IdbSpecialWire::_segment_list` | 按 segment vector 顺序调用 `write_specialnet_wire_segment()`，见 `src/database/manager/builder/def_builder/def_write.cpp:755-764` | 保存 `_segment_list_sd`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:177-182` | `parse_pdn_wire()` 每个 path 创建 segment，见 `src/database/manager/builder/def_builder/def_read.cpp:1402-1406`；`parse_pdn_rects()` 每个 rect 创建一个 segment，见 `src/database/manager/builder/def_builder/def_read.cpp:1486-1488` | `fromShadow()` `init()` 后按 `_segment_list_sd` 顺序创建 segment，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:188-195` | segment nested order 已保存。 |

### `Shadow<idb::IdbSpecialWireSegment>`

| DEF/LEF field + iDB member | iDB write | toShadow | iDB DEF read | fromShadow | Review conclusion |
| --- | --- | --- | --- | --- | --- |
| via segment：`LAYER/WIDTH/SHAPE/POINT/VIA`；`_is_via/_layer/_route_width/_shape_type/_point_list/_via` | `write_specialnet_wire_segment()` 先判断 `is_via()` 并调用 `write_specialnet_wire_segment_via()`，输出 layer、width、shape、point、via name，见 `src/database/manager/builder/def_builder/def_write.cpp:677-710` 和 `src/database/manager/builder/def_builder/def_write.cpp:731-734` | `toShadow()` 在 `_is_via_sd` 分支保存 layer name、route width、via name、point list；同时保存原始 parser 可能读入的 style，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:51-64` | `parse_pdn_wire()` 通过 `DEFIPATH_LAYER/WIDTH/SHAPE/STYLE/POINT/VIA` 恢复 path segment，见 `src/database/manager/builder/def_builder/def_read.cpp:1412-1455` | `fromShadow()` 恢复 flag/width/style/shape/points/layer，并按 DEF via、LEF via lookup 后 `copy_via()`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:88-137` | via branch 按 writer 分支恢复 writer 会输出的字段；`STYLE` 是 read-supported iDB 状态，当前 special-net writer 不输出。 |
| rect segment：`SHAPE/LAYER/RECT`；`_is_rect/_shape_type/_layer/_delta_rect` | `write_specialnet_wire_segment()` 第二分支判断 `is_rect()` 并调用 `write_specialnet_wire_segment_rect()`，输出 shape、layer、rect，见 `src/database/manager/builder/def_builder/def_write.cpp:712-728` 和 `src/database/manager/builder/def_builder/def_write.cpp:735-736` | `toShadow()` 先保存 `_shape_type_sd`，再在 `_is_rect_sd` 分支保存 layer name 和 delta rect，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:49` 和 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:65-71` | `parse_pdn_rects()` 创建 rect wire/segment，恢复 route status、shape、layer、rect，并设置 `is_rect`，见 `src/database/manager/builder/def_builder/def_read.cpp:1486-1508` | `fromShadow()` 恢复 `is_rect`、shape、layer、delta rect，并调用 `set_bounding_box()`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:88-112` 和 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:137` | adapter 保存/恢复 iDB rect；最终文本仍经过原始 writer 的 rect 输出逻辑。 |
| point segment：`LAYER/WIDTH/SHAPE/POINT`；`_layer/_route_width/_shape_type/_point_list` | `write_specialnet_wire_segment()` fallback 到 `write_specialnet_wire_segment_points()`，输出 layer、width、shape、point list，见 `src/database/manager/builder/def_builder/def_write.cpp:651-675` 和 `src/database/manager/builder/def_builder/def_write.cpp:737-738` | `toShadow()` 在 points 分支保存 layer name、route width、point list；同时保存原始 parser 可能读入的 style，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:72-85` | `parse_pdn_wire()` 通过 `DEFIPATH_LAYER/WIDTH/SHAPE/STYLE/POINT` 恢复普通 path segment，见 `src/database/manager/builder/def_builder/def_read.cpp:1412-1455` | `fromShadow()` 恢复 width/style/shape/points/layer，并调用 `set_bounding_box()`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:88-112` 和 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:137` | points branch 对应 writer 会输出的字段；`STYLE` 是 read-supported iDB 状态，当前 special-net writer 不输出；point order 由 child vector 保持。 |
| computed bbox；`IdbSpecialWireSegment::_bounding_box` | writer 不输出 bounding box | 不保存 bounding box | 原始 parser 在 path/rect segment 末尾调用 `set_bounding_box()`，见 `src/database/manager/builder/def_builder/def_read.cpp:1474` 和 `src/database/manager/builder/def_builder/def_read.cpp:1508` | `fromShadow()` 在 geometry/layer/via 恢复后调用 `set_bounding_box()`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:137` | bounding box 是 computed field；rect bbox 也按当前 iEDA `set_bounding_box()` 实现执行，adapter 不另写派生 bbox。 |

### Child Value Views

| DEF/LEF field + iDB member | iDB write | toShadow | iDB DEF read | fromShadow | Review conclusion |
| --- | --- | --- | --- | --- | --- |
| point `( x y )`；`IdbCoordinate<int32_t>::_x/_y` | point coordinates 作为 special wire path 的一部分输出，见 `src/database/manager/builder/def_builder/def_write.cpp:651-710` | `_point_list_sd` 使用 `IdbCoordinate<int32_t>` child vector，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:61-64` | `DEFIPATH_POINT` 读取 x/y，见 `src/database/manager/builder/def_builder/def_read.cpp:1439-1445` | segment `fromShadow()` 调 `add_point(x, y)`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:95-97` | coordinate 是 child value；顺序由 vector child 保存。 |
| rect `( xl yl ) ( xh yh )`；`IdbRect::_lx/_ly/_hx/_hy` | rect coordinates 作为 special net rect 输出，见 `src/database/manager/builder/def_builder/def_write.cpp:712-728` | `_delta_rect_sd` 使用 `IdbRect` value，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:69-70` | `parse_pdn_rects()` 读取 `xl/yl/xh/yh`，见 `src/database/manager/builder/def_builder/def_read.cpp:1494-1501` | segment `fromShadow()` 调 `set_delta_rect()`，见 `src/database/edadb/idb/shadow/shadow_idb_special_net.h:99-102` | rect 是 value storage view；adapter 不保存派生 bbox，也不改变原始 writer 的 rect 文本输出逻辑。 |

## Child Storage View

`IdbSpecialNet` 是 `SPECIALNETS` root，当前子节点/引用处理如下：

- `_pin_string_list_sd`：primitive string vector，保存 `( * pin )` 形式和顺序；非空时作为唯一连接存储视图。
- `_io_pin_name_list_sd`：primitive string vector，保存 IO pin names；仅在没有 pin string 时启用，read 时按 name 查找 `IdbPin*`。
- `_instance_pin_list_sd`：`SpecialNetPinRef` vector，保存 instance name、pin name 和 pin ref order；仅在没有 pin string 时启用。
- `_wire_list_sd`：`Shadow<IdbSpecialWire>` vector，保存 wire order。
- `_segment_list_sd`：`Shadow<IdbSpecialWireSegment>` vector，保存 segment order。
- `_point_list_sd`：coordinate child vector，保存 path point order。

不直接保存 `IdbPin*`、`IdbInstance*`、`IdbLayer*`、`IdbVia*`：这些都是运行时引用。read 时按 name lookup pin/instance/layer/via，via 按原始 parser 语义 copy 后设置 coordinate。
这些 lookup 和 child object 重建逻辑集中在标准单参 `fromShadow()`：`Shadow<IdbSpecialNet>::fromShadow()`、`Shadow<IdbSpecialWire>::fromShadow()`、`Shadow<IdbSpecialWireSegment>::fromShadow()`；运行时上下文通过 `idb::edadb_adapter::EdadbIdbHelper` 获取，`DefReadEdadb::readIdbSpecialNet()` 只负责 cursor 读取、创建 root net、调用 `fromShadow()` 和计数日志。

不保存 `IdbSpecialNetEdgeSegmenArray`：这是后续 routing/PDN 分析用的派生 edge structure，不是 DEF `SPECIALNETS` 直接读写字段。

## Why SpecialNet Shadow

当前需要 `Shadow<IdbSpecialNet>`：

- root identity 是 net name，`_net_name_sd` 作为 PK。
- `IdbSpecialNetList` 是 Level D root list；当前不保存 root append order，也不使用 vector order index。
- pin/layer/via/instance 都是 name reference，不应直接持久化 runtime pointers。
- wire/segment/point 是多层 vector child，需要明确 storage view 和顺序。

## EDADB Write Path

当前 `writeIdbSpecialNet()`：

- Code: `src/database/manager/builder/def_builder/def_write_edadb.cpp:604`
- Special-net vector access: `src/database/manager/builder/def_builder/def_write_edadb.cpp:617`
- Empty-list return: `src/database/manager/builder/def_builder/def_write_edadb.cpp:622`
- Shadow construction: `src/database/manager/builder/def_builder/def_write_edadb.cpp:628`
- EDADB insert: `src/database/manager/builder/def_builder/def_write_edadb.cpp:634`

- 从 `design->get_special_net_list()` 取得 special net vector。
- 空列表返回 `kDbSuccess`，避免 EDADB dispatcher 中断整个写流程。
- 非空时构造 `Shadow<IdbSpecialNet>` pointer vector。
- 写入 net header、pin refs、wire/segment/point nested vectors。
- pin refs 按 `DefWrite::write_special_net()` 的分支写入：`pin_string_list` 非空时只写 pin strings；否则写 IO pin refs 和 instance pin refs。
- segment shadow 按 `DefWrite::write_specialnet_wire_segment()` 的分支保存：via/rect/point 三类由 `_is_via_sd`、`_is_rect_sd` 和对应字段决定。
- 使用 `edadb::insertVector<Shadow<IdbSpecialNet>>()` 写入。

这与原始 DEF writer 输出字段一致；空列表返回值是 adapter 层为 dispatcher 做的语义调整。

## EDADB Read Path

当前 `readIdbSpecialNet()`：

- Code: `src/database/manager/builder/def_builder/def_read_edadb.cpp:960`
- EDADB read op: `src/database/manager/builder/def_builder/def_read_edadb.cpp:973`
- EDADB read loop: `src/database/manager/builder/def_builder/def_read_edadb.cpp:976`
- Add special net: `src/database/manager/builder/def_builder/def_read_edadb.cpp:989`
- Root `fromShadow()` dispatch: `src/database/manager/builder/def_builder/def_read_edadb.cpp:996`
- Pin-string / pin-ref rebuild: `src/database/edadb/idb/shadow/shadow_idb_special_net.h:267`
- Wire/segment rebuild: `src/database/edadb/idb/shadow/shadow_idb_special_net.h:299`

- 使用 EDADB read-all 读取 root records，不指定 root order。
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

`IdbSpecialNetList` 不强制保持 root 顺序；pin refs、wire list、segment list、point list 都需要保持顺序。

依据：

- 原始 `parse_pdn()` 按 DEF 出现顺序 append special net。
- 原始 `write_special_net()` 按 `special_net_list->get_net_list()` 当前顺序输出。
- 原始 writer 也按 pin/wire/segment/point vector 当前顺序输出。
- `src/operation/iPDN` / `iPNP` 对 PDN nets 按 name 查找，当前未发现点工具依赖 `IdbSpecialNetList::_net_list` root index/order。
- 因此 `SPECIALNETS` root order 在点工具语义上是 Level D；当前 root shadow 用 `_net_name_sd` 作为 identity，不保存 root list order。
- instance pin refs 使用 `SpecialNetPinRef::_order_sd` 保存 pin ref order；该字段也是 child table local PK，和 ancestor FK 共同定位 child row。
- wire/segment/point child vector order 由 EDADB vector child 机制保存。
- read path 不依赖 EDADB/SQLite read-all 物理顺序表达语义；root-order-only 文本差异由 normalized diff 处理。

当前状态：已实现。root identity 和 root order 已分离；root order 不保存，child vector order 已由 regression 覆盖。

对 normalized diff 的影响：

- `SPECIALNETS` 是 Level D root list；如果 raw diff 只因为不同 special-net root record 顺序失败，normalized diff 可以按 special net name 排序后通过。
- 排序单位必须是完整 special net record；record 内部 pin/wire/segment/point vectors 不排序。
- 如果 special net header、pin refs 或 routing geometry 内容不同，normalized diff 必须失败。

## Tests

- demo `sky130_gcd` 覆盖非空 special net：`writeIdbSpecialNet insert special_net_count=2 segment_count=639`，`readIdbSpecialNet restored special_net_count=2 segment_count=639`。
- `src/database/edadb/test/run_idb_roundtrip_regression.sh` 使用 TDD 扩展 special-net 覆盖：
  - RED 证据：新增 explicit pin refs / rect segment 断言后，旧 fixture 失败在 `aux_optional special net explicit pin refs: expected '3|1|1', got '6|0|0'`。
  - GREEN 证据：补充 fixture 后，`OUT_DIR=/tmp/iedadb_specialnet_full2 bash src/database/edadb/test/run_idb_roundtrip_regression.sh` 通过。
- default case 检查 root schema、default header、pin-string-only refs、wire/segment/point child counts、via/point dispatch：
  - no root order column：`src/database/edadb/test/run_idb_roundtrip_regression.sh:142`
  - pin-string refs：`src/database/edadb/test/run_idb_roundtrip_regression.sh:156`
  - via/point dispatch 且 no rect：`src/database/edadb/test/run_idb_roundtrip_regression.sh:158`
- `aux_optional` fixture 明确制造 SPECIALNETS 分支：
  - VDD optional `SOURCE/ORIGINAL/WEIGHT`：`src/database/edadb/test/run_idb_roundtrip_regression.sh:287`
  - VSS explicit `( PIN clk ) ( ctrl/_34_ A )`：`src/database/edadb/test/run_idb_roundtrip_regression.sh:294`
  - VSS `+ ROUTED + RECT met1 ...`：`src/database/edadb/test/run_idb_roundtrip_regression.sh:299`
- `aux_optional` SQL 检查 explicit IO pin、instance pin、RECT segment：
  - child counts / dispatch：`src/database/edadb/test/run_idb_roundtrip_regression.sh:146`
  - optional fields：`src/database/edadb/test/run_idb_roundtrip_regression.sh:210`
  - explicit pin refs：`src/database/edadb/test/run_idb_roundtrip_regression.sh:212`
  - rect segment coordinates：`src/database/edadb/test/run_idb_roundtrip_regression.sh:220`
- `routed_irt` case 间接覆盖 routed database 与 regular net 共存场景。

当前动态覆盖矩阵：

- Header：default `USE/weight=0`、optional `SOURCE/ORIGINAL/WEIGHT` 均已覆盖。
- Connection：pin-string `( * pin )`、explicit IO `( PIN pin )`、explicit instance `( inst pin )` 均已覆盖。
- Segment dispatch：via、rect、point 三分支均已覆盖。
- Rebuild：EDADB read 后 raw DEF diff clean，证明 direct DEF parse/write 与 EDADB parse/write 在当前 fixture 上文本一致。
- 未覆盖：shield wire 的完整文本 roundtrip；这是原始 writer 当前也不完整支持的分支，保留 TODO。

## Risks / TODO

- 原始 writer 的 rect segment 文本输出使用当前 iEDA 逻辑，见 `src/database/manager/builder/def_builder/def_write.cpp:724-726`；adapter 保存/恢复的是 iDB rect value，不单独修正 writer 文本输出行为。
- `STYLE` 当前由 original DEF parser 读取、由 EDADB 保存/恢复，但原始 special-net writer 不输出；这属于 iDB 状态保真，不属于当前 raw DEF 文本保真。
- Shield wire 当前原始 writer 不完整支持；EDADB 保存 `_shield_name_sd`，但是否可完整文本 roundtrip 仍取决于原始 writer。
- 如果后续支持 more DEF path tokens，如 mask/viarotation/viadata，需要同步扩展 schema/read/write/test。
