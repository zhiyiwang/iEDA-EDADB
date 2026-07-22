# IdbGroup EDADB Adapter Review

## Scope And Constraints

`IdbGroup` 对应 DEF `GROUPS`，root container 是 `IdbGroupList::_group_list`。

- 原始 write：`DefWrite::write_group()`，`src/database/manager/builder/def_builder/def_write.cpp:1106`
- 原始 read：`DefRead::parse_group_name/member/group()`，`src/database/manager/builder/def_builder/def_read.cpp:2238`, `src/database/manager/builder/def_builder/def_read.cpp:2253`, `src/database/manager/builder/def_builder/def_read.cpp:2288`
- EDADB write：`DefWriteEdadb::writeIdbGroup()`，`src/database/manager/builder/def_builder/def_write_edadb.cpp:559`
- EDADB read：`DefReadEdadb::readIdbGroup()`，`src/database/manager/builder/def_builder/def_read_edadb.cpp:591`

按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 检查：

- Root order 是 Level D，不保存 `_order_sd`，不依赖 SQLite row order。
- Group name 是 natural identity，并作为 table 第一列/默认 PK。
- Member vector 是 root 内的 nested vector，必须保持 parser 展开后的顺序。

## Schema And Primary Key

```cpp
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbGroup>, "iGroupSD",
                 (_group_name_sd, _region_name_sd),
                 (_instance_name_vec_sd));
```

- Group schema：`src/database/edadb/idb/edadb_idb_schema.h:112`
- Group root registration：`src/database/edadb/idb/edadb_idb_init.cpp:89`
- Group shadow：`src/database/edadb/idb/shadow/shadow_idb_group.h:16-77`

Primary-key audit：`_group_name_sd` 足以表达 root identity；不增加 synthetic PK，也不把 member index 当 PK。Primitive string vector 的 EDADB child index只负责 member order。

## Why Shadow Is Required

- `_region` 和 member `IdbInstance*` 都是 non-owning runtime references，DB 应保存 name，而不是地址。
- 原始 parser 可接受 exact name 或 regex pattern；iDB 最终只保留展开、去重后的 instance pointer list。EDADB 保存这个 parser-built storage view，不保存原始 pattern 文本。
- `fromShadow()` 按 name 恢复 active Region/Instance pointers，并保持 member vector order。

## Original DEF Write Mapping

| Original writer brace/order | DEF output | EDADB correspondence | Stored source |
| --- | --- | --- | --- |
| list checks/count, `def_write.cpp:1108-1120` | `GROUPS <N>` | `writeIdbGroup()` reads and converts the complete root vector, `def_write_edadb.cpp:559-595` | root rows; no root order column |
| root name, `def_write.cpp:1122-1123` | `- <group_name>` | `toShadow()` stores `_group_name_sd`, `shadow_idb_group.h:22-28` | group name / root PK |
| member loop, `def_write.cpp:1125-1127` | expanded instance names | `toShadow()` stores ordered instance names, `shadow_idb_group.h:29-35` | `_instance_name_vec_sd` |
| region output, `def_write.cpp:1129` | `+ REGION <name>` | `toShadow()` requires a non-null region and stores its name, `shadow_idb_group.h:23-28` | `_region_name_sd` |
| terminators, `def_write.cpp:1131-1134` | `;`, `END GROUPS` | rebuilt structurally | no column |

## Original DEF Read Mapping

| Original parser brace/order | EDADB correspondence | Source / rebuild |
| --- | --- | --- |
| create current group, `parse_group_name()`, `def_read.cpp:2238-2250` | builder creates root by `_group_name_sd`, `def_read_edadb.cpp:621`; `fromShadow()` synchronizes the name, `shadow_idb_group.h:39-44` | root name |
| exact-name then regex expansion with deduplication, `parse_group_member()`, `def_read.cpp:2253-2285` | EDADB reads the already-expanded ordered name vector; `fromShadow()` performs exact lookup and pointer deduplication, `shadow_idb_group.h:51-59` | final member list, not original regex |
| finalize group and region lookup, `parse_group()`, `def_read.cpp:2288-2304` | `fromShadow()` resolves `_region_name_sd` and sets the active pointer, `shadow_idb_group.h:45-49` | region name → `IdbRegion*` |
| property TODO, `def_read.cpp:2306-2308` | no schema field | no implemented iDB state |

## EDADB Paths And Order

- Write conversion/insert：`def_write_edadb.cpp:580-595`；standard `toShadow()`：`shadow_idb_group.h:22-37`。
- Read reset/query/restore：`def_read_edadb.cpp:604-628`；standard `fromShadow()`：`shadow_idb_group.h:39-61`。
- Missing Region or Instance is a hard conversion failure; the active list is reset, `def_read_edadb.cpp:615-625`。
- Root order is not preserved；member order is preserved by the primitive vector child index。

## Tests And Remaining Work

- `aux_optional` checks non-empty group, root schema without `_order_sd`, region name and ordered members。
- Original writer unconditionally dereferences `group->get_region()`；therefore current adapter rejects a group without Region instead of persisting a state that native writer cannot emit safely。
- Regex source text and properties are not persisted because they are absent from the final implemented iDB state。
