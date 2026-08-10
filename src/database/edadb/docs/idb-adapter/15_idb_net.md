# IdbNet EDADB Adapter Review

## Scope And Constraint Check

`IdbNet` 对应 DEF `NETS`。此外，`SPECIALNETS` 中 `USE SIGNAL/CLOCK` 的 record 也会被原始 `DefRead::parse_special_net()` 转发到 `DefRead::parse_net()`，最终加入同一个 `IdbNetList`。

- 原始 write：`DefWrite::write_net()`，`src/database/manager/builder/def_builder/def_write.cpp:827-903`。
- 原始 wire write：`DefWrite::write_net_wire()` / `write_net_wire_segment*()`，`src/database/manager/builder/def_builder/def_write.cpp:905-1006`。
- 原始 read：`DefRead::parse_net()`，`src/database/manager/builder/def_builder/def_read.cpp:1028-1240`。
- `SPECIALNETS` dispatch：`DefRead::parse_special_net()`，`src/database/manager/builder/def_builder/def_read.cpp:1286-1304`。
- EDADB write：`DefWriteEdadb::writeIdbNet()`，`src/database/manager/builder/def_builder/def_write_edadb.cpp:710-759`。
- EDADB read：`DefReadEdadb::readIdbNet()`，`src/database/manager/builder/def_builder/def_read_edadb.cpp:930-1012`。

按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 检查：

- Root container 是 `IdbNetList::_net_list/_net_map`，Level A。
- `IdbNetList::add_net()` 按 append 顺序分配 `_id`，同时写入 vector/map，见 `src/database/data/design/db_design/IdbNet.cpp:308-330`；canonical adapter 因此应恢复 root append order。
- 本实验分支故意不保存 Level-A root order；`_net_name_sd` 仍是 natural identity/PK。
- IO pins、instance pins、wires、segments、points、Via references 和 virtual-point markers 都是 nested ordered state。

## EDADB Schema And Primary Key

```cpp
TABLE4CLASS(idb::edadb_adapter::NetPinRef, "iNetPinRef",
            (_order_sd, instance_name, pin_name));
TABLE4CLASS(idb::edadb_adapter::RegularWireViaRef, "iRegViaRef",
            (_order_sd, _via_name_sd, _point_index_sd));
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbRegularWireSegment>, "iRegWireSegSD",
                 (primary_key, _vec_idx, _layer_name_sd,
                  _is_via_sd, _is_rect_sd, _delta_rect_sd),
                 (_via_ref_list_sd, _virtual_point_index_list_sd,
                  _point_list_sd));
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbRegularWire>, "iRegWireSD",
                 (primary_key, _vec_idx, _wire_state_sd, _shield_name_sd),
                 (_segment_list_sd));
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbNet>, "iNetSD",
                 (_net_name_sd, _original_net_name_sd,
                  _connect_type_sd, _source_type_sd, _weight_sd,
                  _xtalk_sd, _fix_bump_sd, _frequency_sd),
                 (_io_pin_name_list_sd, _instance_pin_list_sd,
                  _wire_list_sd));
```

- Schema：`src/database/edadb/idb/edadb_idb_schema.h:133-143`。
- Root table registration：`src/database/edadb/idb/edadb_idb_init.cpp:93`；nested tables 由 root `TABLE4CLASS_WVEC` 递归创建。
- `NetPinRef` / `RegularWireViaRef` PK disabled：`src/database/edadb/idb/edadb_idb_init.cpp:32-33`。
- Segment/Wire/Net shadows：`src/database/edadb/idb/shadow/shadow_idb_net.h:36-256`、`src/database/edadb/idb/shadow/shadow_idb_net.h:258-348`、`src/database/edadb/idb/shadow/shadow_idb_net.h:350-542`。

Primary-key audit：

- Root `_net_name_sd` 是 identity；root schema 不包含 `_order_sd`。
- Wire/Segment synthetic `primary_key` 只关联下一层 child rows；`_vec_idx` 单独恢复 owner-local order。
- `NetPinRef::_order_sd`、`RegularWireViaRef::_order_sd` 都是 child order，不是 identity，所以显式关闭 PK。
- Point 使用 `Shadow<IdbCoordinate<int32_t>>::_vec_idx` 恢复顺序；delta rect 通过已注册的 `Shadow<IdbRect>` 存储。

## Why Shadows Are Required

- Pin、Instance、Layer、Via 都是运行时 pointer；DB 保存稳定 name/reference，read 时从 active design/LEF lookup。
- Net/Wire/Segment 是多层 owned vectors，需要 owner identity 和 owner-local order。
- Segment 需要同时表达 parser 保留的 layer、完整 points、virtual-point membership、多个 Via token、Via 对应 point 和 delta rect；原始 C++ pointer graph 不适合直接持久化。
- Pin back-reference、Via clone/coordinate 和 root `_id` 是 read 时按当前 DB root 返回顺序重建的 derived state，不作为数据库列。

## Stored Source And Recomputed State

直接保存 DEF parser 写入 active iDB 的 source state：

- Root：name、USE、SOURCE、ORIGINAL、WEIGHT、XTALK、FIXEDBUMP、FREQUENCY。
- Connections：ordered IO pin names；ordered instance/pin name pairs。
- Wire：wire state；仅 SHIELD branch 保存 shield net name。
- Segment：layer name、rect/via flags、delta rect、完整 point vector、所有 virtual-point indices、所有 Via names/order 以及每个 Via 的 parser-derived point index。

read 时重新 lookup 或计算：

- `_net_name_sd` 按 DB root 返回顺序调用 `IdbNetList::add_net()`，重建 vector/map 和 append-derived `_id`；本实验不保证 `_id` 与原 vector index 一致。
- IO pin、instance、instance pin、layer、DEF/LEF Via pointers。
- `setPinNet` back-reference policy。
- Via clone，以及由对应 point 重新设置的 Via coordinate。

原始 `parse_net()` 明确忽略 `VIAROTATION`、`WIDTH`、`SHAPE`、`STYLE`、`TAPERRULE`、`VIADATA`、`MASK`、`VIAMASK`，且 `FLUSHPOINT` 的 extension 未进入 iDB；这些 token 不建列。

## Original DEF Write Roundtrip Mapping

| Original `DefWrite` execution order | DEF field / iDB member | EDADB correspondence |
| --- | --- | --- |
| 获取 list、检查为空并输出 count，`def_write.cpp:829-841` | `IdbNetList::_net_list` / `NETS <N>` | `writeIdbNet()` 获取同一 list，逐 root 转 shadow 后 batch insert，`def_write_edadb.cpp:711-758` |
| root vector loop 和 name，`def_write.cpp:843-846` | `- <net_name>` / `IdbNet::_net_name` | `_net_name_sd`，`shadow_idb_net.h:362-370`；root order 不入库 |
| IO connection loop，`def_write.cpp:848-851` | `( PIN <pin> )` | ordered `_io_pin_name_list_sd`，`shadow_idb_net.h:373-379` |
| instance-pin loop，`def_write.cpp:853-855` | `(<instance> <pin>)` | ordered `NetPinRef`，`shadow_idb_net.h:381-391` |
| optional USE，`def_write.cpp:859-862` | `_connect_type` | `_connect_type_sd`，`shadow_idb_net.h:393-400` |
| optional SOURCE，`def_write.cpp:864-867` | `_source_type` | `_source_type_sd`，`shadow_idb_net.h:393-400` |
| optional ORIGINAL，`def_write.cpp:869-872` | `_original_net_name` | `_original_net_name_sd`，`shadow_idb_net.h:393-400` |
| optional WEIGHT / XTALK / FIXEDBUMP / FREQUENCY，`def_write.cpp:874-888` | matching root scalar members | `_weight_sd/_xtalk_sd/_fix_bump_sd/_frequency_sd`，`shadow_idb_net.h:393-400` |
| wire loop，`def_write.cpp:890-894` | `IdbRegularWireList::_wire_list` | ordered `_wire_list_sd`，`shadow_idb_net.h:402-412` |
| wire state/SHIELD 和 segment loop，`def_write.cpp:905-923` | ROUTED/FIXED/COVER/SHIELD；shield name；segments | `_wire_state_sd/_shield_name_sd` 和 ordered `_segment_list_sd`，`shadow_idb_net.h:270-292` |
| segment dispatch rect → via → points，`def_write.cpp:926-937` | writer-selected branch | Segment shadow 保存 parser-built branch flags 和对应 source state，`shadow_idb_net.h:63-117` |
| rect branch，`def_write.cpp:994-1005` | layer、start point、delta rect | `_layer_name_sd/_point_list_sd/_delta_rect_sd`，`shadow_idb_net.h:55-85` |
| via branch，`def_write.cpp:965-991` | layer、1/2 points、writer 只输出 first Via | 保存完整 points 和所有 ordered Via refs，`shadow_idb_net.h:75-109`；native writer 继续 canonicalize 为 first Via |
| points branch，`def_write.cpp:942-962` | layer、first/second point；second point 可标 `VIRTUAL` | 保存完整 points 和所有 virtual indices，`shadow_idb_net.h:75-85`；native writer 继续只输出其支持的前两个 points |

原始 writer 的输出能力小于 parser-built iDB state：一个 segment 可由 parser 保留多个 Via 和多个 virtual points，但 writer 只使用 first Via、first/second point 和 second-point virtual flag。EDADB 保存完整 parser state；最终 DEF 仍由同一个 native writer 生成 canonical output。

## Original DEF Read Roundtrip Mapping

| Original `DefRead` execution order | DEF field / rebuilt member | EDADB correspondence |
| --- | --- | --- |
| `SPECIALNETS` USE dispatch，`def_read.cpp:1293-1302` | SIGNAL/CLOCK 调用同一 `parse_net()` | 这类 row 与普通 `NETS` row 一起存入 `iNetSD`；`readIdbNet()` 统一恢复 |
| section count 初始化，`def_read.cpp:987-1007` | `IdbNetList::init(def_num)` | DB root row count驱动读取；不保存 reserve/capacity |
| trim name 并 `add_net()`，`def_read.cpp:1035-1053` | root identity；vector/map/ID | builder 使用无 `ORDER BY` 的 `makeReadAllOp()`，并按 DB 返回顺序调用 `add_net(_net_name_sd)`，`def_read_edadb.cpp:943-975` |
| USE/SOURCE/WEIGHT/XTALK/FIXEDBUMP/FREQUENCY/ORIGINAL，`def_read.cpp:1055-1081` | root scalar state | `fromShadow()` 按 parser setter 顺序恢复，`shadow_idb_net.h:423-430` |
| 根据 connection count 创建 `setPinNet` policy，`def_read.cpp:1083-1092` | computed pin back-reference rule | 从两个 stored connection vectors 的总数重新计算同一 lambda，`shadow_idb_net.h:438-448` |
| IO connection branch，`def_read.cpp:1094-1106` | IO pin lookup、append、conditional back-reference | name lookup 后 `add_io_pin()` + `set_pin_net()`，`shadow_idb_net.h:450-458` |
| instance connection branch，`def_read.cpp:1107-1121` | instance lookup、instance-list append、term pin lookup、append/back-reference | 按 `_order_sd` 恢复 refs，并执行同一 lookup/append/policy，`shadow_idb_net.h:460-478` |
| wire/path owner construction，`def_read.cpp:1124-1143` | wire state、SHIELD、ordered segment objects | root/wire shadows 按 `_vec_idx` 重建 wires/segments，`shadow_idb_net.h:295-325`、`shadow_idb_net.h:480-494` |
| `DEFIPATH_LAYER`，`def_read.cpp:1144-1147` | layer name + active `IdbLayer*` | 保存 name，read 时 global lookup 并 set name/pointer，`shadow_idb_net.h:130-138` |
| `DEFIPATH_VIA`，`def_read.cpp:1149-1170` | mark via；DEF→LEF lookup；clone；coordinate = current point end | 每个 Via 保存 name/order/对应 point index；read 时逐个 lookup、clone、以该 point 重设 coordinate，`shadow_idb_net.h:171-201` |
| ignored VIAROTATION/WIDTH，`def_read.cpp:1172-1175` | no resulting iDB state | no columns |
| `DEFIPATH_POINT`，`def_read.cpp:1176-1182` | ordered normal point | `_point_list_sd` + point `_vec_idx`，`shadow_idb_net.h:140-158` |
| `DEFIPATH_FLUSHPOINT`，`def_read.cpp:1184-1193` | x/y 作为 normal point；extension discarded | x/y 进入 `_point_list_sd`；不保存 discarded extension |
| `DEFIPATH_VIRTUALPOINT`，`def_read.cpp:1194-1199` | ordered point + virtual membership | point vector 加 `_virtual_point_index_list_sd`，`shadow_idb_net.h:140-158` |
| ignored SHAPE/STYLE/TAPERRULE/VIADATA，`def_read.cpp:1200-1207` | no resulting iDB state | no columns |
| `DEFIPATH_RECT`，`def_read.cpp:1208-1217` | rect flag + delta rect | `_is_rect_sd/_delta_rect_sd`，`shadow_idb_net.h:160-169` |
| ignored MASK/VIAMASK/default，`def_read.cpp:1218-1223` | no resulting iDB state | no columns |

Segment `fromShadow()` 不尝试恢复原始 token interleaving；它恢复 parser 最终构建的 storage view：先重建 ordered points/virtual membership，再恢复 rect，最后按 Via order clone Via 并使用所记录的 point index 重建 coordinate。该结果与原始 parser 最终对象状态一致。

## Order And Canonicalization

- Root `IdbNetList::_net_list` 不保存 `_order_sd`，也不使用 root `ORDER BY`；这是本实验分支对 Level-A canonical 约束的有意偏离。
- IO pin primitive vector、instance refs、wires、segments、points、Via refs、virtual-point indices 都保存 owner-local order；read 不依赖 SQLite physical row order。
- ABCD normalizer 只移动完整 NET root record；同一 Net 内 connection/wire/segment/point/Via 顺序不归一化。
- EDADB 保存 parser-built complete state。原始 writer 只输出支持的 canonical subset，因此 raw EDADB output 与 direct iDB output比较，而不是与包含 parser-only state 的原始输入强制逐字相同。

## EDADB Write Read Path

- Write call：`writeChip2Edadb()` 调用 `writeIdbNet()`，`src/database/manager/builder/def_builder/def_write_edadb.cpp:118-119`。
- Write conversion：root validation `def_write_edadb.cpp:711-730` → standard `toShadow()` `def_write_edadb.cpp:732-745` → batch insert/cleanup `def_write_edadb.cpp:747-758`。
- Read call：`createDbByEdadb()` 在前置 Design/Via/Instance/Pin 等引用对象恢复后调用 `readIdbNet()`，`src/database/manager/builder/def_builder/def_read_edadb.cpp:212-226`。
- Read path：reset + unordered query `def_read_edadb.cpp:930-955` → cursor read `def_read_edadb.cpp:953-965` → `add_net()` `def_read_edadb.cpp:967-973` → standard `fromShadow()` `def_read_edadb.cpp:975-979`。
- `createDbByDef()` 不注册已由 EDADB 恢复的 callbacks，避免 DEF text 再次创建 Net，`src/database/manager/builder/def_builder/def_read_edadb.cpp:60-76`。
- 任一 cursor/root/restore failure 都 reset active `IdbNetList`，避免留下部分状态，`def_read_edadb.cpp:976-995`。

## Test Coverage

`routed_irt` 和 `net_branches` 覆盖：

- 677/678 个 Net roots、root `_order_sd` absence、IO/instance connections 和 optional header fields。
- ROUTED/FIXED/COVER/NOSHIELD wire states。
- points、rect、Via segments，以及完整的 multi-Via/multi-VIRTUAL parser state。
- `SPECIALNETS USE SIGNAL` → `IdbNet` dispatch。
- 反转 Net root physical order并允许完整 root record 重排；同时反转 connection、wire、segment、point、Via-ref、virtual-index child tables，验证所有 nested order 仍显式恢复。
- SQL 验证 parser-only state；read log 验证 Via/virtual/multi-Via counts；最终 DEF 与 direct iDB canonical output 一致。

Fixture、SQL 和 physical-order perturbation 分别位于 `src/database/edadb/test/run_idb_roundtrip_regression.sh:728`、`src/database/edadb/test/run_idb_roundtrip_regression.sh:337-498`、`src/database/edadb/test/run_idb_roundtrip_regression.sh:1059`。

定向测试：

```bash
OUT_DIR=/tmp/iedadb_net_convergence \
EDADB_TEST_JOBS=1 \
bash src/database/edadb/test/run_idb_roundtrip_regression.sh routed_irt net_branches
```

完整回归使用多进程 case 并发：

```bash
EDADB_TEST_JOBS=8 bash src/database/edadb/test/run_idb_roundtrip_regression.sh
```

## Remaining Native Limitations

- Regular `+ SHIELD <name>` 存在 writer branch，但当前 native DEF parser 不接受该 regular-NETS syntax；adapter 保留 shield state，不宣称该 raw text branch 已 roundtrip 覆盖。
- Native writer 只输出 first Via、first/second point 和 second-point virtual marker；EDADB 已完整保存 parser state，但 canonical DEF 仍受 native writer 限制。
- `FLUSHPOINT` extension 以及 parser 明确忽略的 route tokens 在原始 iDB 中不存在，adapter 不猜测恢复。
