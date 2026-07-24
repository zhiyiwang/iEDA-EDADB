# IdbSpecialNet EDADB Adapter Review

## Scope And Constraint Check

`IdbSpecialNet` 只对应 DEF `SPECIALNETS` 中被 `DefRead::parse_special_net()` 判定为 POWER/GROUND 的 PDN records。SIGNAL/CLOCK records 虽然来自 `SPECIALNETS` section，但会进入 `parse_net()` 并加入 `IdbNetList`，由 `IdbNet` adapter 处理。

- 原始 write：`DefWrite::write_special_net()`，`src/database/manager/builder/def_builder/def_write.cpp:767-825`。
- 原始 callback：`DefRead::specialNetCallback()`，`src/database/manager/builder/def_builder/def_read.cpp:1268-1284`。
- 原始 read dispatch：`DefRead::parse_special_net()`，`src/database/manager/builder/def_builder/def_read.cpp:1286-1304`。
- 原始 PDN root read：`DefRead::parse_pdn()`，`src/database/manager/builder/def_builder/def_read.cpp:1306-1386`。
- 原始 PDN routed-wire read：`DefRead::parse_pdn_wire()`，`src/database/manager/builder/def_builder/def_read.cpp:1388-1478`。
- 原始 PDN standalone-rect read：`DefRead::parse_pdn_rects()`，`src/database/manager/builder/def_builder/def_read.cpp:1480-1512`。
- 原始 regular-net branch：`DefRead::parse_net()`，`src/database/manager/builder/def_builder/def_read.cpp:1028-1240`。
- EDADB write：`DefWriteEdadb::writeIdbSpecialNet()`，`src/database/manager/builder/def_builder/def_write_edadb.cpp:659-708`。
- EDADB read：`DefReadEdadb::readIdbSpecialNet()`，`src/database/manager/builder/def_builder/def_read_edadb.cpp:864-940`。

按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 检查：

- Root container 是 `IdbSpecialNetList::_net_list`，Level D；不保存 root `_order_sd`，read 不使用 `ORDER BY`。
- `_net_name_sd` 是 natural identity/PK。
- Connection、wire、segment、point 是 nested vectors，必须恢复 parser 构建后的 owner-local order。

## Original DEF Read Dispatch

`SPECIALNETS` 中每条 `defiNet` record 的入口和分支如下：

1. `specialNetCallback()` 检查 `def_net` 和 callback type，然后调用 `parse_special_net()`，见 `src/database/manager/builder/def_builder/def_read.cpp:1268-1283`。
2. `parse_special_net()` 先检查空指针，见 `src/database/manager/builder/def_builder/def_read.cpp:1286-1291`。
3. 仅当 record 存在 `+ USE` 时才分类，见 `src/database/manager/builder/def_builder/def_read.cpp:1293-1302`：
   - `USE POWER/GROUND`：`is_pdn()` 为 true，调用 `parse_pdn()`，创建 `IdbSpecialNet` 并加入 `IdbSpecialNetList`，见 `src/database/manager/builder/def_builder/def_read.cpp:1295-1297`。
   - `USE SIGNAL/CLOCK`：`is_net()` 为 true，调用已有的 `parse_net()`，创建 `IdbNet` 并加入 `IdbNetList`，见 `src/database/manager/builder/def_builder/def_read.cpp:1299-1301`。
4. 无 `USE` 或 `USE` 不属于上述四类时，函数直接返回成功，不创建 `IdbSpecialNet` 或 `IdbNet`，见 `src/database/manager/builder/def_builder/def_read.cpp:1302-1303`。

分类规则定义在 `src/database/data/design/IdbEnum.cpp:307-316`：SIGNAL/CLOCK 属于 regular net，POWER/GROUND 属于 PDN。本文后续 read mapping 只描述 `parse_pdn()` 分支；`parse_net()` 分支属于 `15_idb_net.md`。

原始 `specialNetCallback()` 在 `src/database/manager/builder/def_builder/def_read.cpp:1281-1283` 调用 `parse_special_net()` 后固定返回 `kDbSuccess`，没有继续传播 `parse_pdn()` / `parse_net()` 的失败状态；这是当前原始 iEDA 的 callback 行为，adapter 不在本阶段修改。

## EDADB Schema And Primary Key

```cpp
TABLE4CLASS(idb::edadb_adapter::SpecialNetPinRef, "iSpecPinRef",
            (_order_sd, instance_name, pin_name));
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSpecialWireSegment>, "iSpecWireSegSD",
                 (primary_key, _vec_idx, _layer_name_sd, _via_name_sd,
                  _route_width_sd, _shape_type_sd, _style_sd,
                  _is_via_sd, _is_rect_sd, _delta_rect_sd),
                 (_point_list_sd));
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSpecialWire>, "iSpecWireSD",
                 (primary_key, _vec_idx, _wire_state_sd, _shield_name_sd),
                 (_segment_list_sd));
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSpecialNet>, "iSpecNetSD",
                 (_net_name_sd, _original_net_name_sd, _connect_type_sd,
                  _source_type_sd, _weight_sd),
                 (_pin_string_list_sd, _io_pin_name_list_sd,
                  _instance_pin_list_sd, _wire_list_sd));
```

- Schema：`src/database/edadb/idb/edadb_idb_schema.h:122-131`。
- Root table registration：`src/database/edadb/idb/edadb_idb_init.cpp:91`；nested tables 由 root `TABLE4CLASS_WVEC` 递归创建。
- `SpecialNetPinRef` PK disabled：`src/database/edadb/idb/edadb_idb_init.cpp:31`。
- Segment/Wire/Net shadows：`src/database/edadb/idb/shadow/shadow_idb_special_net.h:29-202`、`src/database/edadb/idb/shadow/shadow_idb_special_net.h:205-291`、`src/database/edadb/idb/shadow/shadow_idb_special_net.h:294-492`。

Primary-key audit：

- Root 使用 `_net_name_sd`，不增加 synthetic PK。
- Wire/Segment 的 `primary_key` 只关联 nested children；`_vec_idx` 单独表达 owner-local order。
- `SpecialNetPinRef::_order_sd` 是 instance-pin child order，不是 identity。
- Point 使用 `Shadow<IdbCoordinate<int32_t>>::_vec_idx` 保序，不以 vector index 作为 PK。

## Why Shadows Are Required

- Pin、Instance、Layer、Via 是运行时 pointer；DB 保存 name，read 时通过 active iDB/LEF lookup 重建引用。
- Connection 有 `(* pin)`、`(PIN pin)`、`(instance pin)` 三种分支，需要 branch-specific storage view。
- Wire/Segment 是多层 owned vectors；Segment 又有 via、rect、points 三种互斥形态。
- Via clone/coordinate、pin back-reference、segment bbox 是 derived state，必须按原始 parser 重建，不能保存进程内对象地址。

## Stored Source And Recomputed State

直接保存 DEF parser 写入 iDB 的 source state：

- Root：name、USE、SOURCE、ORIGINAL、WEIGHT。
- Connections：pin strings，或 IO pin names + instance/pin names。
- Wire：wire state；SHIELD 时保存 shield net name。
- Segment：layer/via name、width、shape、STYLE、branch flags、delta rect、完整 point vector。

read 时重新计算或 lookup：

- Layer/Via pointers；Via clone 和 `via_new->set_coordinate(point_start)`。
- IO/instance pin pointers、instance list 和 `pin->set_special_net()` back-reference。
- Pin-string 对应的 instance/pin membership。
- Segment bounding box。

`VIAROTATION`、`FLUSHPOINT`、`TAPERRULE`、`VIADATA`、path `RECT`、`VIRTUALPOINT`、`MASK`、`VIAMASK` 在原始 `parse_pdn_wire()` 中未形成 iDB 状态，因此不建列。

## Original DEF Write Roundtrip Mapping

| Original `DefWrite` execution order | DEF field / iDB member | EDADB correspondence |
| --- | --- | --- |
| 获取 list、检查为空并输出 count，`def_write.cpp:769-775` | `IdbSpecialNetList::_net_list` / `SPECIALNETS <N>` | `writeIdbSpecialNet()` 获取同一 list，逐 root 转 shadow 后 batch insert，`def_write_edadb.cpp:660-707` |
| root loop 和 name，`def_write.cpp:777-778` | `- <net_name>` / `IdbSpecialNet::_net_name` | `_net_name_sd`，`shadow_idb_special_net.h:311-312` |
| `pin_string_list` 非空分支，`def_write.cpp:780-783` | repeated `(* pin)` | 只保存 `_pin_string_list_sd`，`shadow_idb_special_net.h:318-320` |
| 否则先 IO pin、后 instance pin，`def_write.cpp:784-792` | `(PIN pin)`；`(instance pin)` | 保存有序 IO names 和 `SpecialNetPinRef`，`shadow_idb_special_net.h:322-340` |
| 输出 USE，`def_write.cpp:796-797` | `+ USE` / `_connect_type` | `_connect_type_sd`，`shadow_idb_special_net.h:314` |
| 可选 SOURCE，`def_write.cpp:799-802` | `+ SOURCE` / `_source_type` | `_source_type_sd`，`shadow_idb_special_net.h:315` |
| 可选 ORIGINAL，`def_write.cpp:804-807` | `+ ORIGINAL` / `_original_net_name` | `_original_net_name_sd`，`shadow_idb_special_net.h:313` |
| 可选 WEIGHT，`def_write.cpp:809-811` | `+ WEIGHT` / `_weight` | `_weight_sd`，`shadow_idb_special_net.h:316` |
| wire loop，`def_write.cpp:813-815` | `IdbSpecialWireList::_wire_list` | 按当前 vector 顺序写 `_wire_list_sd`，`shadow_idb_special_net.h:343-352` |
| wire state 和 segment loop，`def_write.cpp:744-764` | `ROUTED/FIXED/COVER/SHIELD`；ordered segments | `_wire_state_sd`、SHIELD name、`_segment_list_sd` 和 `_vec_idx`，`shadow_idb_special_net.h:216-240` |
| dispatch via → rect → points，`def_write.cpp:731-739` | segment variant | `toShadow()` 使用相同分支顺序，`shadow_idb_special_net.h:47-95` |
| via branch，`def_write.cpp:677-710` | layer、width、shape、1/2 points、via name | 保存 layer/width/shape、完整 points、via name，`shadow_idb_special_net.h:55-71` |
| rect branch，`def_write.cpp:712-729` | shape、layer、delta rect | 保存 shape/layer/delta rect，`shadow_idb_special_net.h:72-78` |
| points branch，`def_write.cpp:651-675` | layer、width、shape、前两个 points | 保存 layer/width/shape 和完整 point vector，`shadow_idb_special_net.h:79-94` |

原始 writer 不输出 Segment `STYLE`，且 `kShield` 在 `write_specialnet_wire()` 中直接返回失败：`def_write.cpp:748-751`。Adapter 仍保存这两个 parser source states；恢复后继续交给同一个 native writer，因此 DEF 输出保持同一 canonical behavior，同时 EDADB 不丢失 active iDB 状态。

## Original DEF Read Roundtrip Mapping

| Original `DefRead` execution order | DEF field / rebuilt member | EDADB correspondence |
| --- | --- | --- |
| callback 转入 dispatch，`def_read.cpp:1268-1283` | 一条 `SPECIALNETS` `defiNet` record | EDADB 读 DB 时不经过 DEF callback；`readIdbSpecialNet()` 直接读取 PDN rows |
| `USE POWER/GROUND` dispatch，`def_read.cpp:1293-1297` | 选择 `IdbSpecialNet` / `IdbSpecialNetList` | `readIdbSpecialNet()` 只恢复该 PDN branch；SIGNAL/CLOCK branch 由 `readIdbNet()` 负责 |
| 获取 design/list 并 `add_net(name)`，`def_read.cpp:1308-1315` | root name，创建 `IdbSpecialNetList` member | builder 从 `_net_name_sd` 创建 root，`def_read_edadb.cpp:900-907` |
| USE/SOURCE/WEIGHT/ORIGINAL，`def_read.cpp:1322-1336` | root scalar state | `fromShadow()` 按 parser state 恢复，`shadow_idb_special_net.h:362-366` |
| `instance == "*"`，`def_read.cpp:1338-1342` | `add_pin_string()` | 恢复 pin strings，`shadow_idb_special_net.h:368-371`、`shadow_idb_special_net.h:443-445` |
| `instance == "PIN"`，`def_read.cpp:1343-1350` | IO pin lookup、append、back-reference | helper lookup 后 `add_io_pin()` + `set_special_net()`，`shadow_idb_special_net.h:382-387`、`shadow_idb_special_net.h:447-460` |
| instance-name branch，`def_read.cpp:1351-1365` | instance/pin lookup、append、back-reference | 按 `_order_sd` 恢复 refs，再执行同样的 lookup/append/back-reference，`shadow_idb_special_net.h:389-396`、`shadow_idb_special_net.h:462-480` |
| pin-string expansion，`def_read.cpp:1368-1371` | 计算 instance-pin 和 instance lists | 调用同一个 `get_pin_list_by_names()`，`shadow_idb_special_net.h:373-380` |
| 调用 routed-wire 与 standalone-rect parser，`def_read.cpp:1373-1375` | `IdbSpecialWireList` | root `fromShadow()` 重建同一个 wire list，`shadow_idb_special_net.h:399-411` |
| routed wire loop：state/SHIELD，`def_read.cpp:1394-1400` | wire state + shield name | Wire `fromShadow()` 恢复对应字段，`shadow_idb_special_net.h:243-257` |
| path/segment loop，`def_read.cpp:1402-1410` | ordered segment objects | Wire shadow 按 `_vec_idx` 排序并逐个 `add_segment()`，`shadow_idb_special_net.h:257-266` |
| LAYER，`def_read.cpp:1412-1414` | global `IdbLayer*` | name lookup 后 `set_layer()`，`shadow_idb_special_net.h:106-113` |
| VIA，`def_read.cpp:1416-1431` | DEF→LEF Via lookup、clone、coordinate | helper lookup、`copy_via()`、以 `point_start` 设置 coordinate，`shadow_idb_special_net.h:129-139` |
| WIDTH，`def_read.cpp:1435-1437` | route width | `_route_width_sd` → `set_route_width()`，`shadow_idb_special_net.h:120-121`、`shadow_idb_special_net.h:154-155` |
| POINT，`def_read.cpp:1439-1444` | ordered point vector | 按 point `_vec_idx` 读回后逐个 `add_point()`，`shadow_idb_special_net.h:123-128`、`shadow_idb_special_net.h:156-161` |
| SHAPE，`def_read.cpp:1449-1451` | wire shape | `_shape_type_sd` → `set_shape_type()`，`shadow_idb_special_net.h:112-113` |
| STYLE，`def_read.cpp:1453-1455` | parser-only style | `_style_sd` → `set_style()`，`shadow_idb_special_net.h:120-121`、`shadow_idb_special_net.h:154-155` |
| path 结束，`def_read.cpp:1474` | computed segment bbox | `fromShadow()` 调用 `set_bounding_box()`，`shadow_idb_special_net.h:164-166` |
| standalone RECT root loop，`def_read.cpp:1486-1492` | 每条 RECT 创建一个 wire + 一个 segment，恢复 state/shield | Wire shadow 重建 wire/segment owner 结构和 state/shield，`shadow_idb_special_net.h:243-266` |
| standalone RECT fields，`def_read.cpp:1494-1508` | layer、shape、delta rect、computed bbox | Segment shadow lookup layer、恢复 rect fields 并重算 bbox，`shadow_idb_special_net.h:106-113`、`shadow_idb_special_net.h:140-165` |

## Order And Canonicalization

- Root `IdbSpecialNetList::_net_list` 不保序；它是 Level D，点工具按 net name lookup。
- Pin strings、IO pin names、instance pin refs、wires、segments、points 都保留 owner-local order。
- 原始 parser 将每条 standalone RECT 聚合为一个 wire + 一个 segment；EDADB 保存该 iDB storage view，不尝试恢复 parser 已丢失的文本交错信息。
- 原始 points writer 只输出前两个 points，via writer 只输出其支持的一个或两个 points；EDADB 保存 parser 读入的完整 point vector，但最终 DEF 仍由 native writer canonicalize。
- 原始 rect writer 在 `def_write.cpp:724-726` 将 `high_x` 写了两次；adapter 保存真实 delta rect，并让 direct/EDADB 两条路径共同经过该 native behavior，不在 adapter 中隐藏修改。

## EDADB Write Read Path

- Write：list validation `def_write_edadb.cpp:660-679` → standard `toShadow()` `def_write_edadb.cpp:681-694` → batch insert/cleanup `def_write_edadb.cpp:696-707`。
- Read：reset list `def_read_edadb.cpp:871-879` → cursor read `def_read_edadb.cpp:886-898` → add root `def_read_edadb.cpp:900-907` → standard `fromShadow()` `def_read_edadb.cpp:908-912`。
- Root `fromShadow()`：header/connections `shadow_idb_special_net.h:356-397` → ordered wires `shadow_idb_special_net.h:399-413`。
- Cursor read failure resets the active list，`def_read_edadb.cpp:893-898`；root create/restore failure resets it at `def_read_edadb.cpp:900-911`。

## Test Coverage

`special_net_branches` fixture 和检查：

- Fixture：`src/database/edadb/test/run_idb_roundtrip_regression.sh:671-695`。
- SQLite/read-state assertions：`src/database/edadb/test/run_idb_roundtrip_regression.sh:297-336`。
- 物理 row order 扰动：`src/database/edadb/test/run_idb_roundtrip_regression.sh:950-1003`。

覆盖内容：

- pin-string、两个 IO pins、两个 instance pins 及其 nested order。
- point、two-point via、standalone rect 三种 segment branches。
- 三点 path 的完整 DB state 与 native writer 的 two-point canonical output。
- parser-only STYLE 和 SHIELD 的 DB 值及 EDADB read 后 active iDB state。
- 反转 root、connection、wire、segment、point tables 的物理 row order；root 只要求 normalized DEF 等价，nested vectors 必须按显式 index 恢复。

定向测试：

```bash
OUT_DIR=/tmp/iedadb_special_net_convergence \
EDADB_TEST_JOBS=1 \
bash src/database/edadb/test/run_idb_roundtrip_regression.sh special_net_branches
```

完整回归使用多进程 case 并发：

```bash
EDADB_TEST_JOBS=8 bash src/database/edadb/test/run_idb_roundtrip_regression.sh
```
