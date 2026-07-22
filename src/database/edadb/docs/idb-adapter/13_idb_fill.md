# IdbFill EDADB Adapter Review

## Scope And Constraints

`IdbFill` 对应 DEF `FILLS`，root container 是 `IdbFillList::_fill_list`。

- 原始 write：`DefWrite::write_fill()`，`src/database/manager/builder/def_builder/def_write.cpp:1140`
- 原始 read：`DefRead::parse_fill()`，`src/database/manager/builder/def_builder/def_read.cpp:2352`
- EDADB write：`DefWriteEdadb::writeIdbFill()`，`src/database/manager/builder/def_builder/def_write_edadb.cpp:609`
- EDADB read：`DefReadEdadb::readIdbFill()`，`src/database/manager/builder/def_builder/def_read_edadb.cpp:635`

按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 检查：

- Root order 是 Level D，不保存 `_order_sd`。
- Fill 是 anonymous layer/via union，layer/via name 都不保证唯一；synthetic `primary_key` 只表达 root identity。
- Layer rect vector 和 Via coordinate vector 必须保持 owner 内顺序。

## Schema And Primary Key

```cpp
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbFill>, "iFillSD",
                 (primary_key, _type_sd, _layer_name_sd, _via_name_sd),
                 (_rect_list_sd, _coordinate_list_sd));
```

- Fill schema：`src/database/edadb/idb/edadb_idb_schema.h:115`
- Rect/Coordinate schemas：`src/database/edadb/idb/edadb_idb_schema.h:69-70`, `src/database/edadb/idb/edadb_idb_schema.h:33-34`
- Coordinate/Rect shadow PK disabled：`src/database/edadb/idb/edadb_idb_init.cpp:26-27`
- Fill root registration：`src/database/edadb/idb/edadb_idb_init.cpp:90`
- Fill shadow：`src/database/edadb/idb/shadow/shadow_idb_fill.h:16-119`

`primary_key` 不能使用 layer/via name：同一 layer/via 可以出现多个 fill records。Child vector index 只表达 nested order，不作为 root PK。

## Why Shadow Is Required

- `_type_sd` 区分 layer/via 两个互斥分支。
- `IdbLayer*`、`IdbVia*` 是运行时引用；DB 保存 name，read 时按原始 parser 规则 lookup。
- Via fill 必须 clone lookup 到的 Via，再挂到 Fill；Layer fill 直接绑定 global layer。
- Root shadow 已完整表达两个分支，因此不再需要独立 `Shadow<IdbFillLayer>` / `Shadow<IdbFillVia>`。

## Original DEF Write Mapping

| Original writer brace/order | DEF output | EDADB correspondence | Stored source |
| --- | --- | --- | --- |
| list checks/count, `def_write.cpp:1142-1154` | `FILLS <N>` | `writeIdbFill()` reads active roots, `def_write_edadb.cpp:609-645` | root rows; no root order column |
| layer validity/name, `def_write.cpp:1156-1162` | `- LAYER <name>` or skip invalid record | `toShadow()` accepts only valid layer branch and stores layer name, `shadow_idb_fill.h:35-45` | `_type_sd`, `_layer_name_sd` |
| layer rect loop, `def_write.cpp:1164-1168` | repeated `RECT` | `toShadow()` copies ordered rects, `shadow_idb_fill.h:46-52` | `_rect_list_sd` |
| via validity/name, `def_write.cpp:1169-1174` | `- VIA <name>` or skip invalid record | `toShadow()` accepts only valid via branch and stores via name, `shadow_idb_fill.h:53-57` | `_type_sd`, `_via_name_sd` |
| via coordinate loop, `def_write.cpp:1176-1180` | repeated points | `toShadow()` copies ordered coordinates, `shadow_idb_fill.h:58-64` | `_coordinate_list_sd` |
| section terminator, `def_write.cpp:1184` | `END FILLS` | rebuilt structurally | no column |

## Original DEF Read Mapping

| Original parser brace/order | EDADB correspondence | Source / rebuild |
| --- | --- | --- |
| layer lookup/create, `def_read.cpp:2363-2366` | `fromShadow()` resolves global layer and binds it to the layer-fill child, `shadow_idb_fill.h:76-82` | `_layer_name_sd` → `IdbLayer*` |
| layer rect loop, `def_read.cpp:2367-2369` | append ordered stored rects, `shadow_idb_fill.h:83-88` | `_rect_list_sd` |
| via DEF→LEF lookup and clone, `def_read.cpp:2376-2385` | helper performs the same lookup order; `fromShadow()` clones and binds the Via, `shadow_idb_fill.h:89-98` | `_via_name_sd` → cloned `IdbVia*` |
| via point loops, `def_read.cpp:2386-2391` | append ordered stored coordinates, `shadow_idb_fill.h:99-104` | `_coordinate_list_sd` |
| polygon TODO, `def_read.cpp:2371-2372` | no schema field | no implemented iDB state |

## EDADB Paths And Order

- Write conversion/insert：`def_write_edadb.cpp:630-645`；standard `toShadow()`：`shadow_idb_fill.h:35-69`。
- Read reset/query/restore：`def_read_edadb.cpp:648-673`；standard `fromShadow()`：`shadow_idb_fill.h:71-109`。
- Invalid type/reference is rejected during conversion; it is not stored as an empty-name shadow。
- Read resets the list before restore and on failure：`def_read_edadb.cpp:648`, `def_read_edadb.cpp:658-668`。
- Root order is not preserved；nested rect/coordinate order is preserved by child vector indices。

## Tests And Remaining Work

- `aux_optional` covers both layer and via fills and checks names, types, child counts and nested order。
- The original parser calls `via->clone()` before checking `via`; current adapter checks lookup success first, preventing a null dereference while preserving valid-input semantics。
- The original writer declares the full list count but skips invalid root records. The adapter instead rejects invalid roots, avoiding a database state that cannot produce a consistent section count。
- Polygon remains unsupported until original iEDA creates corresponding state。
