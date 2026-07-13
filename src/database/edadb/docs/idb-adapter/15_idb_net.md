# IdbNet EDADB Adapter Review

## Scope

`IdbNet` 对应 DEF `NETS` section。

- Original write: `DefWrite::write_net()`、`write_net_wire()`、`write_net_wire_segment*()`。
- Original read: `netBeginCallback()`、`netCallback()`、`DefRead::parse_net()`。
- EDADB write: `DefWriteEdadb::writeIdbNet()`。
- EDADB read: `DefReadEdadb::readIdbNet()`。

## Constraint Check

本文件按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 检查：

- DEF mapping：`NETS` → `IdbDesign::_net_list`、`IdbNetList::_net_list/_net_map`、`IdbNet`、`IdbRegularWire`。
- Root order level：A。
- 依据：`IdbNetList::add_net()` 按 append 顺序设置 `IdbNet::_id`；iDRC 把该 ID 保存为 `net_idx`，随后使用 `idb_net_list[net_idx]` 取回 net。只改变 vector 顺序会造成 ID 与对象错配。
- Experiment override：本 no-sort 分支只用 `_net_name_sd` 作为 identity/PK，不保存 root append order；read-all 后调用 `add_net()`，新 `_id` 只与本次 DB 返回顺序一致。
- Nested vectors：IO pin refs、instance pin refs、wire、segment、point 都按 order-sensitive 处理。

当前实现刻意不满足 Level-A root-order 要求，用于验证不保序对点工具和 roundtrip 的实际影响；nested order 仍满足约束。

## Original Write Semantics

原始 `DefWrite::write_net()` 输出：

- net name；
- IO pin refs 和 instance pin refs；
- optional `USE/SOURCE/ORIGINAL/WEIGHT/XTALK/FIXEDBUMP/FREQUENCY`；
- regular wire list；
- 每个 wire 的 wiring state 和有序 segment；
- segment 按 `rect → via → points` 分支输出 layer、point、via 或 delta rect。

## Original Read Semantics

原始 `DefRead::parse_net()`：

- 按 DEF append 顺序创建 net；
- 恢复 optional header fields；
- 按 name lookup IO pin、instance、instance pin，并执行原始 `setPinNet` back-reference 规则；
- 按 wire/path 顺序重建 wire 和 segment；
- layer 按 LEF name lookup；via 先查 DEF via，再查 LEF via；
- `POINT/FLUSHPOINT/VIRTUALPOINT/RECT` 更新 segment geometry；其余未实现 token 不改变最终 iDB 状态。

## EDADB Schema

Schema 定义见 `src/database/edadb/idb/edadb_idb_schema.h:128-137`：

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
                 (_net_name_sd, _original_net_name_sd,
                  _connect_type_sd, _source_type_sd, _weight_sd,
                  _xtalk_sd, _fix_bump_sd, _frequency_sd),
                 (_io_pin_name_list_sd, _instance_pin_list_sd,
                  _wire_list_sd));
```

Primary-key audit：

- `Shadow<IdbNet>::_net_name_sd` 是 root PK；schema 不包含 root order column。
- `NetPinRef::_order_sd` 是 owning net 内的 child-local PK/order。
- `Shadow<IdbRegularWire>` 和 `Shadow<IdbRegularWireSegment>` 使用 synthetic `primary_key` 标识 nested records，并保持构造顺序。
- point vector 使用 `Shadow<IdbCoordinate<int32_t>>::_vec_idx` 保持 nested point order。
- `initAllTables()` 只初始化 `Shadow<IdbNet>` root，见 `src/database/edadb/idb/edadb_idb_init.cpp:94`；child tables 由 root schema 递归创建/映射。

## Original DEF Write/Read Roundtrip Mapping

表中简写文件：

- `def_write.cpp`：`src/database/manager/builder/def_builder/def_write.cpp`
- `def_read.cpp`：`src/database/manager/builder/def_builder/def_read.cpp`
- `def_write_edadb.cpp`：`src/database/manager/builder/def_builder/def_write_edadb.cpp`
- `def_read_edadb.cpp`：`src/database/manager/builder/def_builder/def_read_edadb.cpp`
- `shadow_idb_net.h`：`src/database/edadb/idb/shadow/shadow_idb_net.h`

### Original DEF Write Flow

| 原始 `DefWrite` 执行顺序 | EDADB write / `toShadow` 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. `write_net()` 检查 list；空 list 返回失败；输出 section count，见 `def_write.cpp:829-841` | `writeIdbNet()` 检查 list；空 vector 返回成功；count 由 root rows 推导，见 `def_write_edadb.cpp:648-668` | `NETS <N>` / `IdbNetList::_net_list` / `iNetSD` row count |
| 2. 按 root vector 输出 escaped net name，见 `def_write.cpp:843-846` | `Shadow<IdbNet>::toShadow()` 保存 `_net_name_sd`；builder 不保存 root vector index | `- <net_name>` / `IdbNet::_net_name` / `_net_name_sd` |
| 3. 先按 IO pin vector 输出 `( PIN pin )`，见 `def_write.cpp:848-851` | 保存有序 primitive `_io_pin_name_list_sd`，见 `shadow_idb_net.h:209-212` | IO connection / `IdbNet::_io_pin_list` / `_io_pin_name_list_sd` |
| 4. 再按 instance-pin vector 输出 `( instance pin )`，见 `def_write.cpp:853-855` | 保存 `NetPinRef{_order_sd, instance_name, pin_name}`，见 `shadow_idb_net.h:214-220` | instance connection / `IdbNet::_instance_pin_list`, `IdbPin::_instance/_pin_name` / `_instance_pin_list_sd` |
| 5. connect type 有效时输出 `USE`，见 `def_write.cpp:859-862` | 保存 `_connect_type_sd`，见 `shadow_idb_net.h:223-224` | `+ USE` / `IdbNet::_connect_type` / `_connect_type_sd` |
| 6. source type 有效时输出 `SOURCE`，见 `def_write.cpp:864-867` | 保存 `_source_type_sd`，见 `shadow_idb_net.h:225` | `+ SOURCE` / `IdbNet::_source_type` / `_source_type_sd` |
| 7. original name 非空时输出 escaped `ORIGINAL`，见 `def_write.cpp:869-872` | 保存 `_original_net_name_sd`，见 `shadow_idb_net.h:226` | `+ ORIGINAL` / `IdbNet::_original_net_name` / `_original_net_name_sd` |
| 8. weight 非零时输出 `WEIGHT`，见 `def_write.cpp:874-876` | 保存 `_weight_sd`，见 `shadow_idb_net.h:227` | `+ WEIGHT` / `IdbNet::_weight` / `_weight_sd` |
| 9. xtalk 非零时输出 `XTALK`，见 `def_write.cpp:878-880` | 保存 `_xtalk_sd`，见 `shadow_idb_net.h:228` | `+ XTALK` / `IdbNet::_xtalk` / `_xtalk_sd` |
| 10. fix-bump 为真时输出 `FIXEDBUMP`，见 `def_write.cpp:882-884` | 保存 `_fix_bump_sd`，见 `shadow_idb_net.h:229` | `+ FIXEDBUMP` / `IdbNet::_fix_bump` / `_fix_bump_sd` |
| 11. frequency 大于零时输出 `FREQUENCY`，见 `def_write.cpp:886-888` | 保存 `_frequency_sd`，见 `shadow_idb_net.h:230` | `+ FREQUENCY` / `IdbNet::_frequency` / `_frequency_sd` |
| 12. 按 wire vector 调 `write_net_wire()`；首 segment 使用 wire state，后续使用 `NEW`，见 `def_write.cpp:890-894,905-923` | 构造有序 `_wire_list_sd/_segment_list_sd`，保存 `_wire_state_sd/_shield_name_sd`，见 `shadow_idb_net.h:153-161,232-238` | `ROUTED/FIXED/COVER/NOSHIELD`, `NEW` / `IdbRegularWire::_wire_state/_shield_name/_segment_list` / `_wire_state_sd/_shield_name_sd/_segment_list_sd` |
| 13. segment dispatch 顺序为 rect、via、points，见 `def_write.cpp:926-937` | segment shadow 保存 `_is_rect_sd/_is_via_sd`，并同时保留 parser 已建立的 rect/via state，见 `shadow_idb_net.h:46-60` | segment kind / `IdbRegularWireSegment::_is_rect/_is_via` / `_is_rect_sd/_is_via_sd` |
| 14. points branch 要求 layer 和至少两个 points，只输出 start/second；second 可为 `VIRTUAL`，见 `def_write.cpp:942-962` | 保存 layer name、完整 point vector和 `_is_second_point_virtual_sd`，见 `shadow_idb_net.h:47-68` | point path / `_layer/_point_list/_virtual_points` / `_layer_name_sd/_point_list_sd/_is_second_point_virtual_sd` |
| 15. via branch 输出 layer、一至两个 points和第一个 via name，见 `def_write.cpp:965-991` | 保存 layer name、完整 point vector和第一个 via name，见 `shadow_idb_net.h:47-68` | via path / `_layer/_point_list/_via_list` / `_layer_name_sd/_point_list_sd/_via_name_sd` |
| 16. rect branch 输出 layer、start point和 delta rect，见 `def_write.cpp:994-1005` | 保存 layer name、point vector和 `_delta_rect_sd`，见 `shadow_idb_net.h:47-68` | rect path / `_layer/_point_list/_delta_rect` / `_layer_name_sd/_point_list_sd/_delta_rect_sd` |
| 17. 输出 net/section terminator，见 `def_write.cpp:896-899` | 由 root/child row 边界重建，不存文本终止符 | `;`, `END NETS` / 无 iDB 成员 / 无 EDADB 字段 |

### Original DEF Read Flow

| 原始 `DefRead` 执行顺序 | EDADB read / `fromShadow` 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. `netBeginCallback()` 调 `parse_net_number()` reserve root list，见 `def_read.cpp:987-1007` | EDADB 不保存独立 count；read-all 逐 root row 读取 | `NETS <N>` / `IdbNetList` capacity / row count |
| 2. `parse_net()` trim escaped name 并 `add_net()`，由 append 顺序设置 `_id`，见 `def_read.cpp:1028-1053` | `readIdbNet()` 按 DB 返回顺序用 `_net_name_sd` 调 `add_net()`，不指定 root order | root name/ID / `IdbNet::_net_name/_id`, `IdbNetList::_net_list/_net_map` / `_net_name_sd` |
| 3. 依次恢复 `USE/SOURCE/WEIGHT/XTALK/FIXEDBUMP/FREQUENCY/ORIGINAL`，见 `def_read.cpp:1055-1081` | `Shadow<IdbNet>::fromShadow()` 按相同 parser 顺序恢复 fields，见 `shadow_idb_net.h:241-249` | optional net fields / `IdbNet` header members / corresponding root `_sd` fields |
| 4. 根据 connection count 创建 `setPinNet` 规则：单连接只填空 pointer，多连接直接覆盖，见 `def_read.cpp:1083-1092` | `fromShadow()` 使用保存的 IO/instance ref 总数执行同一规则，见 `shadow_idb_net.h:257-267` | pin back-reference policy / `IdbPin::_net` / 由 child rows 计算，不单独存储 |
| 5. connection 为 `PIN` 时按 name lookup IO pin，append 并设置 back-reference，见 `def_read.cpp:1094-1106` | 遍历 `_io_pin_name_list_sd`，执行同样 lookup/add/set，见 `shadow_idb_net.h:269-277` | `( PIN pin )` / `IdbNet::_io_pin_list`, `IdbPin::_net` / `_io_pin_name_list_sd` |
| 6. 其他 connection 按 instance name lookup，再按 term name lookup pin，append instance/pin并设置 back-reference，见 `def_read.cpp:1107-1121` | 按 `_order_sd` 排序 `NetPinRef` 后执行同样 lookup/add/set；缺失引用打印并继续，见 `shadow_idb_net.h:279-297` | `( instance pin )` / `IdbNet::_instance_list/_instance_pin_list`, `IdbPin::_net` / `_instance_pin_list_sd` |
| 7. 按 DEF wire 顺序创建 wire，恢复 wire state和 optional shield name，见 `def_read.cpp:1124-1135` | net `fromShadow()` 按 `_wire_list_sd` 创建 wire；wire `fromShadow()` 恢复 state/name，见 `shadow_idb_net.h:165-177,299-306` | regular wire / `IdbRegularWire::_wire_state/_shield_name` / `_wire_state_sd/_shield_name_sd` |
| 8. 按 path 顺序创建 segment并遍历 path tokens，见 `def_read.cpp:1136-1143` | wire `fromShadow()` 按 `_segment_list_sd` 创建 segment并调用 segment `fromShadow()`，见 `shadow_idb_net.h:170-176` | route path/order / `IdbRegularWire::_segment_list` / `_segment_list_sd` |
| 9. `DEFIPATH_LAYER` 保存 layer name并按 LEF name lookup，见 `def_read.cpp:1144-1147` | segment `fromShadow()` 保存 layer name并通过 helper lookup layer；lookup 结果可为 null，与原 parser 最终状态一致，见 `shadow_idb_net.h:74-79` | `LAYER` / `_layer_name/_layer` / `_layer_name_sd` |
| 10. `DEFIPATH_VIA` 设置 via flag，按 DEF→LEF 顺序 lookup、copy via，并用 end point设置 coordinate；via 未找到时打印并继续，见 `def_read.cpp:1149-1170` | 使用 `_is_via_sd/_via_name_sd` 执行相同 lookup/copy/coordinate 和 non-fatal missing-via 行为，见 `shadow_idb_net.h:99-124` | `VIA` / `_is_via/_via_list` / `_is_via_sd/_via_name_sd` |
| 11. `DEFIPATH_POINT` 按顺序 add point，见 `def_read.cpp:1176-1181` | 按 `_point_list_sd` child order add point，见 `shadow_idb_net.h:81-90` | point / `_point_list` / `_point_list_sd` |
| 12. `DEFIPATH_FLUSHPOINT` 当前忽略 extension，仅 add x/y，见 `def_read.cpp:1184-1192` | storage view只有 x/y，按普通 point 恢复；与原 parser 最终 iDB state一致 | flush point / `_point_list` / `_point_list_sd` |
| 13. `DEFIPATH_VIRTUALPOINT` 调 `add_virtual_point()`，见 `def_read.cpp:1194-1198` | `_is_second_point_virtual_sd` 为真时对第二点调用 `add_virtual_point()`，见 `shadow_idb_net.h:81-90` | `VIRTUAL` / `_point_list/_virtual_points` / `_point_list_sd/_is_second_point_virtual_sd` |
| 14. `DEFIPATH_RECT` 设置 rect flag和 delta rect，见 `def_read.cpp:1208-1216` | 恢复 `_is_rect_sd/_delta_rect_sd`，见 `shadow_idb_net.h:70-96` | `RECT` / `_is_rect/_delta_rect` / `_is_rect_sd/_delta_rect_sd` |
| 15. `VIAROTATION/WIDTH/SHAPE/STYLE/TAPERRULE/VIADATA/MASK/VIAMASK` 当前不更新 iDB，见 `def_read.cpp:1172-1175,1200-1207,1218-1223` | 不保存这些 token，与原 parser 的最终 iDB state一致 | parser-ignored route tags / 无成员更新 / 无 EDADB 字段 |

## Child Storage View

- `Shadow<IdbNet>`：root header、connection refs、wire vector；不保存 root order。
- `NetPinRef`：instance name、pin name和 owning-net-local order。
- `Shadow<IdbRegularWire>`：wire state、shield name和 segment vector。
- `Shadow<IdbRegularWireSegment>`：layer/via names、segment flags、virtual-second flag、delta rect和 point vector。
- `Shadow<IdbCoordinate<int32_t>>`：ordered point coordinates。

不保存 raw pin/instance/layer/via pointers；`fromShadow()` 通过 `EdadbIdbHelper` 按 name重建。

## Why Net Shadows Are Required

- root 需要 name identity；Level-A append order 在本实验分支刻意不保存。
- pin、instance、layer和via 是 non-owning/runtime references，必须转换为 names。
- regular wire包含多层 owned vectors，需要明确 owner和nested order。
- segment只保存 DEF/parser-visible state，不持久化运行时 pointer。

## EDADB Write Path

`writeIdbNet()` 位于 `src/database/manager/builder/def_builder/def_write_edadb.cpp:648`：

1. 获取 `IdbNetList`；
2. 遍历 root vector 构造 `Shadow<IdbNet>`，但不保存 root index；
3. `insertVector<Shadow<IdbNet>>()` 递归写入 root和所有 child tables。

## EDADB Read Path

`readIdbNet()` 位于 `src/database/manager/builder/def_builder/def_read_edadb.cpp:1011`：

1. 使用 read-all 读取 root shadows，不指定 root order；
2. 按 DB 返回顺序 `net_list->add_net(_net_name_sd)`，重建 list/map，并按该顺序重新分配 ID；
3. 只调用唯一标准接口 `Shadow<IdbNet>::fromShadow(IdbNet*)`；
4. Net shadow继续调用 wire/segment shadow的 `fromShadow()`；
5. builder只保留 cursor、root creation、错误传播和统计。

## Computed And Rebuilt State

- `IdbNet::_id`：由 read-all 返回顺序下的 `add_net()` 重新分配。
- `IdbNetList::_net_map`：由 `add_net()` 重建。
- `IdbPin::_net`：由 connection count和原始 `setPinNet` 规则重建。
- pin/instance/layer/via pointers：按 name lookup。
- copied via coordinate：`copy_via()` 后使用 segment end point设置。

## Order / Index

- Root `IdbNetList::_net_list`：Level A，但本实验分支不保序；schema 无 root `_order_sd`，read path 无 root `ORDER BY`。
- IO pin refs：primitive vector index保序。
- Instance pin refs：`NetPinRef::_order_sd` 保序。
- Wire和segment：nested synthetic PK/child query顺序保序。
- Points：`Shadow<IdbCoordinate>::_vec_idx` 保序。

## Tests

完整 regression：

```bash
OUT_DIR=/tmp/iedadb_net15_review bash src/database/edadb/test/run_idb_roundtrip_regression.sh
```

当前覆盖：

- `default_ipl`：675 regular nets、default header，并确认 root `_order_sd` column 不存在。
- `aux_optional`：`ORIGINAL/SOURCE/WEIGHT/XTALK/FIXEDBUMP/FREQUENCY`。
- `routed_irt`：677 nets、677 wires、8997 segments、14256 points、3716 via segments、22 rect segments和ordered instance refs。
- `net_branches`：真实 routed DEF派生 fixture，覆盖 `FIXED/COVER/NOSHIELD/VIRTUAL`，同时保持完整 via/rect/point regression。
- 四组 direct DEF roundtrip与EDADB roundtrip均通过 raw DEF diff。

## Risks / TODO

- 原始 `write_net()` 对空 list返回 `kDbFail`，当前 `writeIdbNet()` 对空 vector返回成功。
- Regular NETS grammar支持 `ROUTED/FIXED/COVER/NOSHIELD`；原始 `write_net_wire()` 的 `kShield` 分支会输出 parser拒绝的 `+ SHIELD <name>`，因此不能构造合法 raw roundtrip fixture。当前 adapter仍保存 `_shield_name_sd`，但不宣称 regular shield文本 roundtrip已支持。
- EDADB保存完整 point vector，但原始 points writer只输出 start/second，via writer只输出一至两个 points，rect writer只输出 start point；超过 writer可输出范围的point state需要单独定义期望。
- 当前只保存“第二点是否 virtual”，对应原始 writer只检查 second point的行为；如果要保存任意位置的 parser virtual-point state，需要改成ordered virtual flags。
- 原始 parser忽略 width/style/taper/mask/via-rotation等 route token；adapter保持相同最终 iDB state，不扩大语义。
