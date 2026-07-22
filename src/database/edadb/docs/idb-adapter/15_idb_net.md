# IdbNet EDADB Adapter Review

## Scope And Constraints

`IdbNet` corresponds to DEF `NETS`, with root container `IdbNetList::_net_list/_net_map`.

- 原始 write：`DefWrite::write_net()` / `write_net_wire*()`，`src/database/manager/builder/def_builder/def_write.cpp:827`, `src/database/manager/builder/def_builder/def_write.cpp:905`
- 原始 read：`DefRead::parse_net()`，`src/database/manager/builder/def_builder/def_read.cpp:1028`
- EDADB write：`DefWriteEdadb::writeIdbNet()`，`src/database/manager/builder/def_builder/def_write_edadb.cpp:710`
- EDADB read：`DefReadEdadb::readIdbNet()`，`src/database/manager/builder/def_builder/def_read_edadb.cpp:901`

按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 检查：

- Root order is Level A：`add_net()` assigns IDs in append order and downstream code may use ID as vector index。
- `_net_name_sd` is identity/PK；`_order_sd` separately preserves root append order。
- IO pins、instance pins、wires、segments and points are nested ordered data。

## Schema And Primary Key

```cpp
TABLE4CLASS(idb::edadb_adapter::NetPinRef, "iNetPinRef",
            (_order_sd, instance_name, pin_name));
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbRegularWireSegment>, "iRegWireSegSD",
                 (primary_key, _vec_idx, _layer_name_sd, _via_name_sd,
                  _is_via_sd, _is_rect_sd, _is_second_point_virtual_sd,
                  _delta_rect_sd),
                 (_point_list_sd));
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbRegularWire>, "iRegWireSD",
                 (primary_key, _vec_idx, _wire_state_sd, _shield_name_sd),
                 (_segment_list_sd));
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbNet>, "iNetSD",
                 (_net_name_sd, _order_sd, _original_net_name_sd,
                  _connect_type_sd, _source_type_sd, _weight_sd,
                  _xtalk_sd, _fix_bump_sd, _frequency_sd),
                 (_io_pin_name_list_sd, _instance_pin_list_sd,
                  _wire_list_sd));
```

- Schema：`src/database/edadb/idb/edadb_idb_schema.h:129-137`
- Root registration：`src/database/edadb/idb/edadb_idb_init.cpp:92`
- `NetPinRef` PK disabled：`src/database/edadb/idb/edadb_idb_init.cpp:32`
- Segment/Wire/Net shadows：`src/database/edadb/idb/shadow/shadow_idb_net.h:29`, `src/database/edadb/idb/shadow/shadow_idb_net.h:153`, `src/database/edadb/idb/shadow/shadow_idb_net.h:224`

Primary-key audit：

- `_net_name_sd` is root identity；`_order_sd` is order only。
- `NetPinRef::_order_sd` is child order, not PK。
- Wire/Segment need synthetic owner identity to attach children；their `_vec_idx` restores owner-local order。
- Point order uses `Shadow<IdbCoordinate<int32_t>>::_vec_idx`。

## Why Shadows Are Required

- Pin/Instance/Layer/Via pointers must be persisted as names and resolved against the active design/layout。
- Root order must be explicit so `add_net()` rebuilds vector/map/ID consistency。
- Wire/segment are nested owned structures with rect/via/point branches and computed Via coordinates。
- `fromShadow()` must reproduce the parser's `setPinNet` back-reference rule rather than persist cached pointers。

## Original DEF Write Mapping

| Original writer brace/order | DEF output | EDADB `toShadow()` correspondence | Stored source |
| --- | --- | --- | --- |
| list checks/count, `def_write.cpp:829-841` | `NETS <N>` | `writeIdbNet()` reads all roots, `def_write_edadb.cpp:710-747` | root rows |
| root name/vector iteration, `def_write.cpp:843-846` | `- <net_name>` | Net shadow stores name and supplied root index, `shadow_idb_net.h:239-245` | `_net_name_sd`, `_order_sd` |
| IO then instance connections, `def_write.cpp:848-855` | `(PIN pin)` then `(instance pin)` | store ordered IO names and `NetPinRef`, `shadow_idb_net.h:247-265` | connection child vectors |
| optional USE/SOURCE/ORIGINAL/WEIGHT/XTALK/FIXEDBUMP/FREQUENCY, `def_write.cpp:859-888` | optional header clauses | store the matching scalar fields, `shadow_idb_net.h:267-274` | root scalar fields |
| wire loop, `def_write.cpp:890-894`; wire state/segments, `def_write.cpp:905-923` | wiring state, first segment header and `NEW` | Net/Wire shadows preserve wire/segment vectors with `_vec_idx`, `shadow_idb_net.h:168-187`, `shadow_idb_net.h:276-286` | wire state/shield and ordered segments |
| segment dispatch, `def_write.cpp:926-937` | rect → via → points | Segment `toShadow()` preserves both parser flags but validates the writer-selected branch, `shadow_idb_net.h:46-81` | branch flags and source fields |
| points, `def_write.cpp:942-962` | layer, first/second point, optional second-point `VIRTUAL` | store layer, points and second-point virtual flag, `shadow_idb_net.h:71-80` | point branch fields |
| via, `def_write.cpp:965-991` | layer, one/two points, first via name | store layer, points and first via name, `shadow_idb_net.h:63-80` | via branch fields |
| rect, `def_write.cpp:994-1005` | layer, start point and delta rect | store layer, points and delta rect, `shadow_idb_net.h:55-80` | rect branch fields |

## Original DEF Read Mapping

| Original parser brace/order | EDADB `fromShadow()` correspondence | Source / calculated state |
| --- | --- | --- |
| section count/init, `def_read.cpp:987-1007` | no duplicated count column; root row count drives restore | list capacity is not persistent state |
| trim name and `add_net()`, `def_read.cpp:1042-1053` | ordered root query then `add_net(_net_name_sd)`, `def_read_edadb.cpp:916-945` | rebuild `_net_list`, `_net_map` and append-derived ID |
| optional header fields, `def_read.cpp:1055-1081` | restore in parser order, `shadow_idb_net.h:290-304` | root scalar fields |
| derive `setPinNet` policy from connection count, `def_read.cpp:1083-1092` | recompute the same policy from stored child counts, `shadow_idb_net.h:312-322` | computed back-reference rule |
| IO connection branch, `def_read.cpp:1094-1106` | lookup IO pin, add and apply `set_pin_net`, `shadow_idb_net.h:324-332` | IO name → pointer/back-reference |
| instance connection branch, `def_read.cpp:1107-1121` | sort refs by `_order_sd`, lookup instance/pin, add and apply policy, `shadow_idb_net.h:334-352` | instance/pin names → pointers |
| wire/path construction, `def_read.cpp:1124-1143` | sort wires/segments by `_vec_idx`, create objects and call nested `fromShadow()`, `shadow_idb_net.h:189-210`, `shadow_idb_net.h:354-363` | ordered nested structure |
| LAYER, `def_read.cpp:1144-1147` | restore layer name and lookup global layer, `shadow_idb_net.h:94-99` | source name + runtime pointer |
| VIA and copied-via coordinate, `def_read.cpp:1149-1170` | lookup Via, copy it and set coordinate from point end, `shadow_idb_net.h:120-131` | source via name + derived Via object |
| POINT/FLUSHPOINT/VIRTUALPOINT, `def_read.cpp:1176-1198` | restore ordered points and second-point virtual state, `shadow_idb_net.h:101-112` | point source; flush extension remains unsupported |
| RECT, `def_read.cpp:1208-1216` | restore rect flag and delta rect, `shadow_idb_net.h:114-118` | rect source fields |
| ignored route tokens, `def_read.cpp:1172-1175`, `def_read.cpp:1200-1207`, `def_read.cpp:1218-1223` | no columns | no resulting iDB state |

## EDADB Paths And Order

- Write conversion/insert：`def_write_edadb.cpp:732-747`；root `toShadow()`：`shadow_idb_net.h:239-288`。
- Read reset/ordered query：`def_read_edadb.cpp:914-920`；root create/restore：`def_read_edadb.cpp:923-954`；root `fromShadow()`：`shadow_idb_net.h:290-366`。
- Read resets the active list on read/conversion failure：`def_read_edadb.cpp:930-948`。
- Root uses `_order_sd + ORDER BY`；all nested vectors use primitive indices, `NetPinRef::_order_sd`, or Wire/Segment `_vec_idx`。

## Tests And Remaining Work

- Regression covers default and optional header fields, IO/instance connections, root order, routed wires, point/via/rect branches and virtual second point。
- Regular shield text remains constrained by native writer/parser grammar behavior；the adapter stores `_shield_name_sd` but does not claim unsupported raw text roundtrip。
- Native writer emits only a limited point subset for each segment branch. More-than-supported point state needs an explicit canonicalization rule and targeted fixture。
- Width/style/taper/mask/via-rotation tokens are not stored because original `parse_net()` does not retain them in iDB state。
