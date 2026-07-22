# IdbSpecialNet EDADB Adapter Review

## Scope And Constraints

`IdbSpecialNet` 对应由 `DefRead::parse_special_net()` 判定为 PDN 的 DEF `SPECIALNETS` records；非 PDN `USE` 会转入 `parse_net()`，不属于本 storage view。

- 原始 write：`DefWrite::write_special_net()`，`src/database/manager/builder/def_builder/def_write.cpp:767`
- 原始 read dispatch：`DefRead::parse_special_net()`，`src/database/manager/builder/def_builder/def_read.cpp:1286`
- 原始 PDN read：`DefRead::parse_pdn()` / `parse_pdn_wire()` / `parse_pdn_rects()`，`src/database/manager/builder/def_builder/def_read.cpp:1306`, `src/database/manager/builder/def_builder/def_read.cpp:1388`, `src/database/manager/builder/def_builder/def_read.cpp:1480`
- EDADB write：`DefWriteEdadb::writeIdbSpecialNet()`，`src/database/manager/builder/def_builder/def_write_edadb.cpp:659`
- EDADB read：`DefReadEdadb::readIdbSpecialNet()`，`src/database/manager/builder/def_builder/def_read_edadb.cpp:845`

按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 检查：

- `IdbSpecialNetList::_net_list` root order 是 Level D；不保存 `_order_sd`。
- Root name 是 natural identity/PK。
- Connection、wire、segment、point 等 nested vectors 必须保持 parser 构建后的 owner-local order。

## Schema And Primary Key

```cpp
TABLE4CLASS(idb::edadb_adapter::SpecialNetPinRef, "iSpecPinRef",
            (_order_sd, instance_name, pin_name));
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSpecialWireSegment>, "iSpecWireSegSD",
                 (primary_key, _vec_idx, _layer_name_sd, _via_name_sd,
                  _route_width_sd, _shape_type_sd, _is_via_sd,
                  _is_rect_sd, _delta_rect_sd),
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

- Schema：`src/database/edadb/idb/edadb_idb_schema.h:118-126`
- Root registration：`src/database/edadb/idb/edadb_idb_init.cpp:91`
- `SpecialNetPinRef` PK disabled：`src/database/edadb/idb/edadb_idb_init.cpp:31`
- Segment/Wire/Net shadows：`src/database/edadb/idb/shadow/shadow_idb_special_net.h:29`, `src/database/edadb/idb/shadow/shadow_idb_special_net.h:164`, `src/database/edadb/idb/shadow/shadow_idb_special_net.h:235`

Primary-key audit：

- `_net_name_sd` 是 root identity；不需要额外 synthetic root PK。
- Wire/Segment 的 `primary_key` 只用于挂接 nested children；`_vec_idx` 单独表达 owner-local order。
- `SpecialNetPinRef::_order_sd` 只表达 instance-pin order，不是 PK，因此显式关闭其 PK。
- Point vector 通过 `Shadow<IdbCoordinate<int32_t>>::_vec_idx` 保序。

## Why Shadows Are Required

- Pin/Instance/Layer/Via 都是 runtime references，DB 保存 names，read 时通过 helper lookup。
- Connection 有三种分支：`(* pin)` pin-string、`(PIN pin)` IO pin、`(instance pin)` instance pin；必须保存 branch-specific storage view。
- Wire/segment 是多层 owned vectors，segment 又有 point/via/rect 三种 writer dispatch。
- Parser 会计算 Via copy/coordinate 和 segment bbox；这些 derived states 不直接持久化，由 `fromShadow()` 重建。

## Original DEF Write Mapping

| Original writer brace/order | DEF output | EDADB `toShadow()` correspondence | Stored source |
| --- | --- | --- | --- |
| section/list checks, `def_write.cpp:769-775` | `SPECIALNETS <N>` | `writeIdbSpecialNet()` gets all roots, `def_write_edadb.cpp:659-696` | root rows |
| root name, `def_write.cpp:777-778` | `- <net_name>` | Net shadow stores `_net_name_sd`, `shadow_idb_special_net.h:250-255` | root name / PK |
| pin-string branch, `def_write.cpp:780-783` | `(* pin)` | copy `_pin_string_list_sd`, `shadow_idb_special_net.h:261-263` | pin-string vector |
| explicit IO/instance branch, `def_write.cpp:784-792` | `(PIN pin)` then `(instance pin)` | only when pin-string list is empty, save ordered IO names and `SpecialNetPinRef`, `shadow_idb_special_net.h:265-283` | IO names; instance/pin names + child order |
| required USE and optional SOURCE/ORIGINAL/WEIGHT, `def_write.cpp:796-811` | header clauses | store connect/source/original/weight, `shadow_idb_special_net.h:255-259` | root scalar fields |
| wire loop, `def_write.cpp:813-815`; state/segment loop, `def_write.cpp:744-764` | wiring state, then first segment/header and `NEW` | Net/Wire shadows preserve wire and segment vectors using `_vec_idx`, `shadow_idb_special_net.h:179-198`, `shadow_idb_special_net.h:286-295` | wire state/shield name and ordered segments |
| segment dispatch, `def_write.cpp:731-739` | via → rect → points | Segment `toShadow()` follows the same branch order, `shadow_idb_special_net.h:46-91` | `_is_via_sd`, `_is_rect_sd` and branch fields |
| point branch, `def_write.cpp:651-675` | layer, width, shape, first/second points | store layer/width/shape and point vector, `shadow_idb_special_net.h:79-90` | point branch fields |
| via branch, `def_write.cpp:677-710` | layer, width, shape, one/two points, via name | store the same fields and via name, `shadow_idb_special_net.h:56-70` | via branch fields |
| rect branch, `def_write.cpp:712-729` | shape, layer, delta rect | store shape/layer/delta rect, `shadow_idb_special_net.h:71-77` | rect branch fields |

`STYLE` is deliberately absent：original parser sets it at `def_read.cpp:1453-1455`, but original special-net writer never emits it. The adapter stores the native writer-visible canonical state, not parser-only hidden state.

## Original DEF Read Mapping

| Original parser brace/order | EDADB `fromShadow()` correspondence | Source / calculated state |
| --- | --- | --- |
| `USE` dispatch, `parse_special_net()`, `def_read.cpp:1286-1304` | this adapter represents only records dispatched to `parse_pdn()`; regular-use records belong to `Shadow<IdbNet>` | root type selection |
| create root and restore USE/SOURCE/WEIGHT/ORIGINAL, `def_read.cpp:1308-1336` | builder creates root by name, `def_read_edadb.cpp:877`; Net shadow restores scalar fields, `shadow_idb_special_net.h:299-307` | root fields |
| `io_name == "*"`, `def_read.cpp:1338-1342` | restore pin strings, then derive instance pins with `get_pin_list_by_names()`, `shadow_idb_special_net.h:309-321` | pin-string source + computed instance membership |
| `io_name == "PIN"`, `def_read.cpp:1343-1350` | lookup IO pin, add it and set `pin->_special_net`, `shadow_idb_special_net.h:323-328`, `shadow_idb_special_net.h:372-384` | IO name → pointer/back-reference |
| instance branch, `def_read.cpp:1351-1365` | sort pin refs by `_order_sd`, lookup instance/pin, add both and set back-reference, `shadow_idb_special_net.h:330-337`, `shadow_idb_special_net.h:387-405` | instance/pin names → pointers |
| pin-string expansion, `def_read.cpp:1368-1371` | same `get_pin_list_by_names()` call, `shadow_idb_special_net.h:314-321` | derived lists, not extra columns |
| wire/path construction, `def_read.cpp:1388-1475` | Net/Wire shadows sort by `_vec_idx`, create wires/segments and restore fields, `shadow_idb_special_net.h:340-352`, `shadow_idb_special_net.h:200-221`, `shadow_idb_special_net.h:95-145` | ordered wire/segment structure |
| LAYER/VIA/WIDTH/POINT/SHAPE tokens, `def_read.cpp:1412-1451` | Segment shadow resolves layer/via names, restores width/shape/points and copies Via, `shadow_idb_special_net.h:102-140` | source fields + runtime references |
| segment bbox, `def_read.cpp:1474` | `fromShadow()` calls `set_bounding_box()`, `shadow_idb_special_net.h:143` | derived bbox |
| standalone RECT records, `def_read.cpp:1486-1508` | stored as one Wire + one rect Segment; restore layer/shape/delta rect and bbox through the same shadows | parser-equivalent grouped storage view |

## EDADB Paths And Order

- Write conversion/insert：`def_write_edadb.cpp:681-696`；root `toShadow()`：`shadow_idb_special_net.h:250-297`。
- Read reset/query/root creation：`def_read_edadb.cpp:858-885`；root `fromShadow()`：`shadow_idb_special_net.h:299-355`。
- Read resets the active list on read/conversion failure：`def_read_edadb.cpp:870-888`。
- Root order is not preserved；all nested vectors preserve owner-local order through primitive-vector indices, `SpecialNetPinRef::_order_sd`, or Wire/Segment `_vec_idx`。

## Tests And Remaining Work

- Regression covers pin-string, explicit IO pin, explicit instance pin, optional header fields, and point/via/rect segment dispatch。
- DB child rows are perturbed in targeted tests; final raw DEF equality verifies nested order restoration。
- Shield wire remains limited by original `write_specialnet_wire()` returning failure for `kShield`, `def_write.cpp:748-751`。
- Original rect writer currently emits `high_x` twice at `def_write.cpp:724-726`; adapter preserves the iDB rect value and does not hide or patch this native writer behavior。
- More than two points and parser-only route tokens require separate expectation definitions because the native writer serializes only its supported subset。
