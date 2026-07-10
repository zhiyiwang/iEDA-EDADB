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

## Original DEF Write/Read Roundtrip Mapping

下面两张表分别以原始 `DefWrite` 和 `DefRead` 的实际执行顺序为主线，不再按 shadow class 分组。第三列统一标明 DEF tag、iDB 成员以及 EDADB 存储域。表中简写文件分别是：

- `def_write.cpp`：`src/database/manager/builder/def_builder/def_write.cpp`
- `def_read.cpp`：`src/database/manager/builder/def_builder/def_read.cpp`
- `def_write_edadb.cpp`：`src/database/manager/builder/def_builder/def_write_edadb.cpp`
- `def_read_edadb.cpp`：`src/database/manager/builder/def_builder/def_read_edadb.cpp`
- `shadow_idb_special_net.h`：`src/database/edadb/idb/shadow/shadow_idb_special_net.h`

### Original DEF Write Flow

| 原始 `DefWrite` 执行顺序 | EDADB write / `toShadow` 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. `write_special_net()` 检查 list 并输出 section count，见 `def_write.cpp:769-775` | `writeIdbSpecialNet()` 读取 root vector 并 `insertVector()`，见 `def_write_edadb.cpp:611-634`；count 由 vector/table row count 推导，不单独存储。当前差异：原始 writer 对空 list 返回 `kDbFail`，EDADB writer 对空 vector 返回成功 | `SPECIALNETS <N>` / `IdbSpecialNetList::_net_list` / 无独立 count 字段 |
| 2. 遍历 special net 并输出 name，见 `def_write.cpp:777-778` | `Shadow<IdbSpecialNet>::toShadow()` 保存 `_net_name_sd`，见 `shadow_idb_special_net.h:226-227` | `- <net_name>` / `IdbSpecialNet::_net_name` / `_net_name_sd` |
| 3. pin-string list 非空时输出 `( * pin )`，见 `def_write.cpp:780-783` | 保存 `_pin_string_list_sd`，且不保存其他 connection view，见 `shadow_idb_special_net.h:233-237` | connection `*` / `IdbSpecialNet::_pin_string_list` / `_pin_string_list_sd` |
| 4. pin-string list 为空时输出 `( PIN pin )`，见 `def_write.cpp:784-787` | 按 pin name 保存 `_io_pin_name_list_sd`，见 `shadow_idb_special_net.h:237-240` | connection `PIN` / `IdbSpecialNet::_io_pin_list`, `IdbPin::_pin_name` / `_io_pin_name_list_sd` |
| 5. 继续输出 `( instance pin )`，见 `def_write.cpp:789-791` | 保存 `SpecialNetPinRef{_order_sd, instance_name, pin_name}`，见 `shadow_idb_special_net.h:242-248` | instance connection / `IdbSpecialNet::_instance_pin_list`, `IdbPin::_instance`, `IdbPin::_pin_name` / `_instance_pin_list_sd` |
| 6. 无条件输出 `USE`，见 `def_write.cpp:796-797` | 保存 `_connect_type_sd`，见 `shadow_idb_special_net.h:229` | `+ USE` / `IdbSpecialNet::_connect_type` / `_connect_type_sd` |
| 7. source type 有效时输出 `SOURCE`，见 `def_write.cpp:799-802` | 保存 `_source_type_sd`，见 `shadow_idb_special_net.h:230` | `+ SOURCE` / `IdbSpecialNet::_source_type` / `_source_type_sd` |
| 8. original name 非空时输出 `ORIGINAL`，见 `def_write.cpp:804-807` | 保存 `_original_net_name_sd`，见 `shadow_idb_special_net.h:228` | `+ ORIGINAL` / `IdbSpecialNet::_original_net_name` / `_original_net_name_sd` |
| 9. weight 非零时输出 `WEIGHT`，见 `def_write.cpp:809-810` | 保存 `_weight_sd`，见 `shadow_idb_special_net.h:231` | `+ WEIGHT` / `IdbSpecialNet::_weight` / `_weight_sd` |
| 10. 按 wire vector 顺序调用 `write_specialnet_wire()`，见 `def_write.cpp:813-815` | 按 child vector 顺序构造 `_wire_list_sd`，见 `shadow_idb_special_net.h:252-257` | routing statement / `IdbSpecialNet::_wire_list` / `_wire_list_sd` |
| 11. `write_specialnet_wire()` 生成 wire state；shield 直接返回 `kDbFail`，见 `def_write.cpp:744-755` | 保存 `_wire_state_sd` 和 `_shield_name_sd`，见 `shadow_idb_special_net.h:173-175`；shield name 是 parser-supported iDB 状态，但原始 writer 当前不输出完整 shield path | `+ ROUTED`, `+ FIXED`, `+ COVER`, `+ SHIELD` / `IdbSpecialWire::_wire_state`, `_shield_name` / `_wire_state_sd`, `_shield_name_sd` |
| 12. 按 segment vector 顺序调用 `write_specialnet_wire_segment()`，首段使用 wire state，后续段使用 `NEW`，见 `def_write.cpp:757-762` | 按 child vector 顺序构造 `_segment_list_sd`，见 `shadow_idb_special_net.h:177-182` | `NEW` path / `IdbSpecialWire::_segment_list` / `_segment_list_sd` |
| 13. segment dispatch 依次判断 via、rect、point fallback，见 `def_write.cpp:731-738` | `toShadow()` 保存 `_is_via_sd/_is_rect_sd`，并按相同三分支抽取字段，见 `shadow_idb_special_net.h:46-85` | segment kind / `IdbSpecialWireSegment::_is_via`, `_is_rect` / `_is_via_sd`, `_is_rect_sd` |
| 14. via branch 输出 layer、width、shape、point(s)、via name：point 数等于 `_POINT_MAX_` 时输出前两点，否则只输出 start point，见 `def_write.cpp:677-710` | 保存 layer/via name、width、shape和完整 point vector，见 `shadow_idb_special_net.h:49-64`；EDADB 保留的 iDB point state 可多于原始 writer 实际输出 | `LAYER WIDTH SHAPE POINT VIA` / `_layer`, `_route_width`, `_shape_type`, `_point_list`, `_via` / `_layer_name_sd`, `_route_width_sd`, `_shape_type_sd`, `_point_list_sd`, `_via_name_sd` |
| 15. rect branch 输出 shape、layer、rect，见 `def_write.cpp:712-728` | 保存 shape、layer name 和 `_delta_rect_sd`，见 `shadow_idb_special_net.h:49,65-71` | `SHAPE LAYER RECT` / `_shape_type`, `_layer`, `_delta_rect` / `_shape_type_sd`, `_layer_name_sd`, `_delta_rect_sd` |
| 16. point branch 要求至少 `_POINT_MAX_` 个 point，但只输出 start/second point，见 `def_write.cpp:651-675` | 保存 layer name、width、shape和完整 point vector，见 `shadow_idb_special_net.h:49,72-85`；EDADB 保留的 iDB point state 可多于原始 writer 实际输出 | `LAYER WIDTH SHAPE POINT` / `_layer`, `_route_width`, `_shape_type`, `_point_list` / `_layer_name_sd`, `_route_width_sd`, `_shape_type_sd`, `_point_list_sd` |
| 17. 输出 net terminator 和 section terminator，见 `def_write.cpp:817-820` | 根据 root/child vector 边界重建，不存储文本终止符 | `;`, `END SPECIALNETS` / 无 iDB 成员 / 无 EDADB 字段 |

### Original DEF Read Flow

| 原始 `DefRead` 执行顺序 | EDADB read / `fromShadow` 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. `parse_special_net()` 按 `USE` 判断 PDN 或 regular net，见 `def_read.cpp:1286-1303` | `readIdbSpecialNet()` 只读 `Shadow<IdbSpecialNet>`；regular-net state 由 `readIdbNet()` / `Shadow<IdbNet>` 处理 | `USE` / `defiNet::use()` / special-net 与 net 两条 schema path |
| 2. `parse_pdn()` 获取 IO pin、instance 和 special-net list，然后按 name `add_net()`，见 `def_read.cpp:1306-1319` | `readIdbSpecialNet()` 获取 `IdbSpecialNetList`，用 `_net_name_sd` 创建 root net，见 `def_read_edadb.cpp:960-995` | `- <net_name>` / `IdbSpecialNetList::_net_list`, `IdbSpecialNet::_net_name` / `_net_name_sd` |
| 3. `hasUse()` 时恢复 connect type，见 `def_read.cpp:1322-1324` | `fromShadow()` 调 `set_connect_type(_connect_type_sd)`，见 `shadow_idb_special_net.h:260-265` | `+ USE` / `IdbSpecialNet::_connect_type` / `_connect_type_sd` |
| 4. `hasSource()` 时恢复 source type，见 `def_read.cpp:1326-1328` | `fromShadow()` 调 `set_source_type(_source_type_sd)`，见 `shadow_idb_special_net.h:264` | `+ SOURCE` / `IdbSpecialNet::_source_type` / `_source_type_sd` |
| 5. `hasWeight()` 时恢复 weight，见 `def_read.cpp:1330-1332` | `fromShadow()` 调 `set_weight(_weight_sd)`，见 `shadow_idb_special_net.h:265` | `+ WEIGHT` / `IdbSpecialNet::_weight` / `_weight_sd` |
| 6. `hasOriginal()` 时恢复 original name，见 `def_read.cpp:1334-1336` | `fromShadow()` 调 `set_original_net_name(_original_net_name_sd)`，见 `shadow_idb_special_net.h:262` | `+ ORIGINAL` / `IdbSpecialNet::_original_net_name` / `_original_net_name_sd` |
| 7. connection loop 遇到 `*` 时 `add_pin_string()`，见 `def_read.cpp:1338-1342` | 遍历 `_pin_string_list_sd` 并调 `restorePinStringConnection()`，见 `shadow_idb_special_net.h:267-270,323-326` | `( * pin )` / `IdbSpecialNet::_pin_string_list` / `_pin_string_list_sd` |
| 8. connection loop 遇到 `PIN` 时 lookup IO pin、append，并设置 pin back-reference，见 `def_read.cpp:1343-1350` | `restoreIoPinConnection()` 通过 helper lookup pin，然后 `add_io_pin()` 和 `set_special_net()`，见 `shadow_idb_special_net.h:281-285,328-340` | `( PIN pin )` / `IdbSpecialNet::_io_pin_list`, `IdbPin::_special_net` / `_io_pin_name_list_sd` |
| 9. 其他 connection 作为 instance name，lookup instance/pin、append 并设置 back-reference，见 `def_read.cpp:1351-1365` | 按 `_order_sd` 排序 `SpecialNetPinRef`，再调 `restoreInstancePinConnection()`，见 `shadow_idb_special_net.h:288-295,343-360` | `( instance pin )` / `IdbSpecialNet::_instance_list`, `_instance_pin_list`, `IdbPin::_special_net` / `_instance_pin_list_sd` |
| 10. pin-string list 非空时，调 `get_pin_list_by_names()` 计算 instance/pin refs，见 `def_read.cpp:1368-1371` | pin strings 非空时调用同一 iDB helper 逻辑，见 `shadow_idb_special_net.h:272-279`；推导结果不重复存储 | derived connection / `IdbSpecialNet::_instance_list`, `_instance_pin_list` / 由 `_pin_string_list_sd` 计算 |
| 11. `parse_pdn_wire()` 按 DEF wire 顺序创建 wire，恢复 wire state 和 shield name，见 `def_read.cpp:1388-1400` | `Shadow<IdbSpecialWire>::fromShadow()` 恢复 `_wire_state_sd/_shield_name_sd`，见 `shadow_idb_special_net.h:185-188` | `ROUTED/FIXED/COVER/SHIELD` / `IdbSpecialWire::_wire_state`, `_shield_name` / `_wire_state_sd`, `_shield_name_sd` |
| 12. 按 path 顺序创建 segment，然后在 `while` 中按 DEF path token 的实际出现顺序 traverse，见 `def_read.cpp:1402-1411` | wire `fromShadow()` 按 `_segment_list_sd` 顺序 `add_segment()`；segment `fromShadow()` 从已解析的 storage view 恢复最终 iDB 状态，见 `shadow_idb_special_net.h:88-137,188-195` | path / `IdbSpecialWire::_segment_list` / `_segment_list_sd` |
| 13a. token 为 `DEFIPATH_LAYER` 时按 name lookup LEF layer，见 `def_read.cpp:1412-1414` | segment `fromShadow()` 通过 helper 按 `_layer_name_sd` lookup 并 `set_layer()`，见 `shadow_idb_special_net.h:104-112` | `LAYER` / `IdbSpecialWireSegment::_layer` / `_layer_name_sd` |
| 13b. token 为 `DEFIPATH_VIA` 时设置 via flag，先查 DEF via 再查 LEF via，`copy_via()` 后设 coordinate，见 `def_read.cpp:1416-1431` | 使用 `_is_via_sd/_via_name_sd`，按相同 DEF→LEF 顺序 lookup、copy 并设 coordinate，见 `shadow_idb_special_net.h:114-135` | `VIA` / `IdbSpecialWireSegment::_is_via`, `_via` / `_is_via_sd`, `_via_name_sd` |
| 13c. token 为 `DEFIPATH_WIDTH` 时恢复 route width，见 `def_read.cpp:1435-1437` | `set_route_width(_route_width_sd)`，见 `shadow_idb_special_net.h:89` | `WIDTH` / `IdbSpecialWireSegment::_route_width` / `_route_width_sd` |
| 13d. token 为 `DEFIPATH_POINT` 时按 token 顺序 `add_point(x,y)`，见 `def_read.cpp:1439-1444` | 按 `_point_list_sd` child vector 顺序 `add_point()`，见 `shadow_idb_special_net.h:95-97` | `POINT` / `IdbSpecialWireSegment::_point_list`, `IdbCoordinate::_x/_y` / `_point_list_sd` |
| 13e. token 为 `DEFIPATH_SHAPE` 时恢复 shape，见 `def_read.cpp:1449-1451` | `set_shape_type(_shape_type_sd)`，见 `shadow_idb_special_net.h:91` | `SHAPE` / `IdbSpecialWireSegment::_shape_type` / `_shape_type_sd` |
| 13f. token 为 `DEFIPATH_STYLE` 时恢复 style，见 `def_read.cpp:1453-1455` | `set_style(_style_sd)`，见 `shadow_idb_special_net.h:90`；原始 special-net writer 当前不输出 style | `STYLE` / `IdbSpecialWireSegment::_style` / `_style_sd` |
| 13g. `VIAROTATION/FLUSHPOINT/TAPERRULE/VIADATA/RECT/VIRTUALPOINT/MASK/VIAMASK` 在当前 parser 中均忽略，见 `def_read.cpp:1433-1468` | 不存储这些 token；与原始 parser 的最终 iDB 状态一致 | parser-ignored path tags / 无 iDB 成员更新 / 无 EDADB 字段 |
| 14. path token 处理完成后计算 bbox，见 `def_read.cpp:1472-1474` | segment `fromShadow()` 在 layer/via/geometry 恢复后调 `set_bounding_box()`，见 `shadow_idb_special_net.h:137` | computed bbox / `IdbSpecialWireSegment::_bounding_box` / 不存储，读时计算 |
| 15. `parse_pdn_rects()` 为每个 rect 创建 wire/segment，恢复 route status 和 shield name，见 `def_read.cpp:1480-1492` | `_wire_list_sd/_segment_list_sd` 保存 owner/order；wire `fromShadow()` 恢复 state/shield | special-net `RECT` record / `IdbSpecialWire`, `IdbSpecialWireSegment` / `_wire_list_sd`, `_segment_list_sd`, `_wire_state_sd`, `_shield_name_sd` |
| 16. rect parser 读取 layer、coordinates、shape，设置 rect flag/delta rect，见 `def_read.cpp:1494-1506` | segment `fromShadow()` 恢复 `_is_rect_sd/_shape_type_sd/_layer_name_sd/_delta_rect_sd`，见 `shadow_idb_special_net.h:91-112` | `LAYER RECT SHAPE` / `_is_rect`, `_shape_type`, `_layer`, `_delta_rect` / `_is_rect_sd`, `_shape_type_sd`, `_layer_name_sd`, `_delta_rect_sd` |
| 17. rect geometry 完成后计算 bbox，见 `def_read.cpp:1508` | 共用 segment `fromShadow()` 末尾的 `set_bounding_box()`，见 `shadow_idb_special_net.h:137` | computed bbox / `IdbSpecialWireSegment::_bounding_box` / 不存储，读时计算 |

当前 EDADB connection storage 刻意匹配原始 writer 的互斥输出语义：`pin_string_list` 非空时只保存 pin strings，否则保存 IO/instance refs。它不用来保存任意混合的 DEF connection token；pin-string 推导出的 resolved refs 是 read-time computed state。

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

- 空 `IdbSpecialNetList` 的返回语义尚未完全对齐：原始 `write_special_net()` 返回 `kDbFail`，当前 `writeIdbSpecialNet()` 对空 vector 返回成功。
- EDADB 保存完整 `_point_list_sd`，但原始 point writer 只输出 start/second point，via writer 根据 point count 输出一点或两点；超过两点的 segment 需要专门 fixture 确认期望语义。
- 原始 writer 的 rect segment 文本输出使用当前 iEDA 逻辑，见 `src/database/manager/builder/def_builder/def_write.cpp:724-726`；adapter 保存/恢复的是 iDB rect value，不单独修正 writer 文本输出行为。
- `STYLE` 当前由 original DEF parser 读取、由 EDADB 保存/恢复，但原始 special-net writer 不输出；这属于 iDB 状态保真，不属于当前 raw DEF 文本保真。
- Shield wire 当前原始 writer 不完整支持；EDADB 保存 `_shield_name_sd`，但是否可完整文本 roundtrip 仍取决于原始 writer。
- 如果后续支持 more DEF path tokens，如 mask/viarotation/viadata，需要同步扩展 schema/read/write/test。
