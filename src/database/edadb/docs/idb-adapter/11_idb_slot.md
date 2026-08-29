# IdbSlot EDADB Adapter Review

## Scope And Constraints

`IdbSlot` 对应 DEF `SLOTS`，root container 是 `IdbSlotList::_slot_list`。

- 原始 write：`DefWrite::write_slot()`，`src/database/manager/builder/def_builder/def_write.cpp:1074`
- 原始 read：`DefRead::parse_slot()`，`src/database/manager/builder/def_builder/def_read.cpp:2135`
- EDADB write：`DefWriteEdadb::writeIdbSlot()`，`src/database/manager/builder/def_builder/def_write_edadb.cpp:544`
- EDADB read：`DefReadEdadb::readIdbSlot()`，`src/database/manager/builder/def_builder/def_read_edadb.cpp:543`

按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 检查：

- 点工具语义是 Level D；当前作为 raw DEF roundtrip 例外保留 root append order。
- Slot 是 anonymous record，layer name 不唯一；`primary_key` 表示 root identity，`_order_sd` 单独表示 root order。
- nested rect vector 必须保序，使用 `Shadow<IdbRect>::_vec_idx`。

## Schema And Primary Key

```cpp
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSlot>, "iSlotSD",
                 (primary_key, _order_sd, _layer_name_sd),
                 (_rect_list_sd));
```

- Slot schema：`src/database/edadb/idb/edadb_idb_schema.h:109`
- Rect schema：`src/database/edadb/idb/edadb_idb_schema.h:69-70`
- Rect shadow PK disabled：`src/database/edadb/idb/edadb_idb_init.cpp:27`
- Slot root registration：`src/database/edadb/idb/edadb_idb_init.cpp:88`
- Slot shadow：`src/database/edadb/idb/shadow/shadow_idb_slot.h:15-67`

`primary_key` 不能替代 `_order_sd`：前者必须稳定区分同 layer、同 geometry 的重复 anonymous records；后者只表达 `IdbSlotList` 中的位置。

## Why Shadow Is Required

- 原始类没有天然唯一 name，direct mapping 无法可靠挂接多个 root 的 rect child rows。
- shadow 把 anonymous root identity、root order、layer name 和 owned rect vector 分开表达。
- 不保存 `IdbLayer*`：原始 `parse_slot()` 也只设置 layer name。

## Original DEF Write Mapping

| Original writer brace/order | DEF output | EDADB correspondence | Stored source |
| --- | --- | --- | --- |
| list checks and section count, `def_write.cpp:1076-1088` | `SLOTS <N>` | `writeIdbSlot()` gets the active vector and converts every root, `def_write_edadb.cpp:544-580` | root row count and `_order_sd` |
| root loop, `def_write.cpp:1090-1091` | `- LAYER <name>` | `toShadow()` stores root index and layer name, `shadow_idb_slot.h:24-30` | `primary_key`, `_order_sd`, `_layer_name_sd` |
| rect loop, `def_write.cpp:1093-1095` | repeated `RECT` | `toShadow()` copies the complete rect vector, `shadow_idb_slot.h:32-38` | `_rect_list_sd` + child `_vec_idx` |
| terminators, `def_write.cpp:1097-1100` | `;`, `END SLOTS` | rebuilt from root/child boundaries | no column |

## Original DEF Read Mapping

| Original parser brace/order | EDADB correspondence | Source / rebuild |
| --- | --- | --- |
| callback dispatch, `def_read.cpp:2117-2133`; create root, `def_read.cpp:2141-2143` | reset active list, query `ORDER BY "_order_sd"`, then `add_slot()`, `def_read_edadb.cpp:543-578` | root append order |
| `hasLayer()` branch, `def_read.cpp:2145-2146` | `fromShadow()` calls `set_layer_name()`, `shadow_idb_slot.h:42-50` | `_layer_name_sd` |
| rectangle loop, `def_read.cpp:2147-2149` | `fromShadow()` creates and copies each ordered rect, `shadow_idb_slot.h:52-55` | `_rect_list_sd` |
| polygon TODO, `def_read.cpp:2152-2153` | no schema field | no implemented iDB state |

## EDADB Paths And Order

- Write conversion and insert：`def_write_edadb.cpp:565-588`。
- Read ordered query：`def_read_edadb.cpp:558-562`；restore loop：`def_read_edadb.cpp:564-584`。
- Read resets the list before restore and again on failure：`def_read_edadb.cpp:556`, `def_read_edadb.cpp:571-580`。
- Root order：`_order_sd + ORDER BY`；nested rect order：`IdbRectSD._vec_idx`。

## Tests And Remaining Work

- `aux_optional` fixture checks non-empty Slot, root `_order_sd`, layer name, rect values and nested rect order。
- Empty-list path is covered by the canonical demo。
- Polygon remains unsupported because original `parse_slot()` does not create polygon state。
- If raw text order is no longer a requirement, Slot can be re-audited as pure Level D and `_order_sd` can be removed; `primary_key` must remain for anonymous root identity。
