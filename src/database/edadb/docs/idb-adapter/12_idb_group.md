# IdbGroup EDADB Adapter Review

## Scope And Constraint Check

`IdbGroup` 对应 DEF `GROUPS`，root container 是 `IdbGroupList::_group_list`。

- 原始 write：`DefWrite::write_group()`，`src/database/manager/builder/def_builder/def_write.cpp:1106-1138`
- 原始 read：`DefRead::parse_group_name/member/group()`，`src/database/manager/builder/def_builder/def_read.cpp:2238-2310`
- EDADB write：`DefWriteEdadb::writeIdbGroup()`，`src/database/manager/builder/def_builder/def_write_edadb.cpp:559-607`
- EDADB read：`DefReadEdadb::readIdbGroup()`，`src/database/manager/builder/def_builder/def_read_edadb.cpp:591-633`

按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 检查：

- `IdbGroupList` root order 是 Level D，不保存 `_order_sd`，不依赖 SQLite root row order。
- `_group_name_sd` 是 natural root identity，也是 `iGroupSD` 第一列/默认 PK。
- Group member list 是 nested vector；原始 writer 按该 vector 顺序输出，因此 EDADB 必须保留 parser 展开后的 member 顺序。

## EDADB Schema

```cpp
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbGroup>, "iGroupSD",
                 (_group_name_sd, _region_name_sd),
                 (_instance_name_vec_sd));
```

- Schema：`src/database/edadb/idb/edadb_idb_schema.h:116-117`
- Table init：`src/database/edadb/idb/edadb_idb_init.cpp:71-95`，Group registration 在 `:89`
- Shadow：`src/database/edadb/idb/shadow/shadow_idb_group.h:15-76`
- `initPrimKeys()` 不覆盖 Group：`Shadow<IdbGroup>` 保留默认 PK，第一列 `_group_name_sd` 作为 root identity。
- Root 由 builder 显式转换为 `Shadow<IdbGroup>`；当前路径不需要 `TABLE4SHADOW_WVEC(idb::IdbGroup)`。

| Field | Classification | Storage / rebuild |
| --- | --- | --- |
| `IdbGroup::_group_name` | DEF source + root identity | `_group_name_sd`；builder 用它创建 root |
| `IdbGroup::_instance_list` | parser-built storage view | 保存展开、去重后的 ordered instance-name vector；read 时 lookup active `IdbInstance*` |
| `IdbGroup::_region` | DEF source + non-owning reference | 保存 `_region_name_sd`；read 时 lookup active `IdbRegion*` |
| `DefRead::_cur_group` | transient parser state | 不入库；只用于串联原始三个 callbacks |
| 原始 member regex/pattern | parser-discarded text | 不入库；iDB 只保留展开后的 instance pointers |
| Group property/soft options | current iDB TODO | 原始 parser 未写入已实现的 `IdbGroup` state，不入库 |

## Why Shadow Is Required

- `_region` 和 member instances 都是 non-owning pointers，必须转换为稳定 name references。
- 原始 parser 会把 exact name 或 regex member 展开成最终 `IdbInstance*` list；shadow 保存该 parser-built view，而不是不可恢复的原始 pattern。
- Member vector 需要 child storage 和 index；direct pointer dump 无法表达引用重建及 nested order。

## Original DEF Write Mapping

| Original writer brace/order | DEF output | EDADB correspondence | Stored source |
| --- | --- | --- | --- |
| list/null/count checks，`def_write.cpp:1108-1120` | `GROUPS <N> ;` | `writeIdbGroup()` 获取完整 root vector，空 vector 直接成功，`def_write_edadb.cpp:560-578` | root row count |
| root loop/name，`def_write.cpp:1122-1123` | `- <group_name>` | builder 逐 root 调用 `toShadow()`，`def_write_edadb.cpp:580-595`；shadow 写 `_group_name_sd`，`shadow_idb_group.h:22-28` | `IdbGroup::_group_name` |
| member loop，`def_write.cpp:1125-1127` | ordered instance names | `toShadow()` 按 active member vector 顺序写 names，`shadow_idb_group.h:29-35` | `IdbGroup::_instance_list` 的 names |
| Region，`def_write.cpp:1129` | `+ REGION <name>` | `toShadow()` 要求 non-null Region 并写 `_region_name_sd`，`shadow_idb_group.h:23-28` | `IdbGroup::_region->get_name()` |
| record/section terminators，`def_write.cpp:1131-1134` | `;` / `END GROUPS` | 输出时由 `DefWrite` 结构化生成 | 不入库 |

## Original DEF Read Mapping

| Original parser brace/order | EDADB correspondence | Source / synchronization / calculation |
| --- | --- | --- |
| `parse_group_name()` 创建 `_cur_group`，`def_read.cpp:2238-2250` | `readIdbGroup()` 用 `_group_name_sd` 调用一次 `group_list->add_group()`，`def_read_edadb.cpp:608-622` | root allocation + name；builder 负责，不在 `fromShadow()` 重复 set name |
| `parse_group_member()` exact-name branch，`def_read.cpp:2253-2272` | `fromShadow()` 对每个已展开 name 做 exact lookup，`shadow_idb_group.h:44-48` | name → active `IdbInstance*` |
| `parse_group_member()` regex traversal/dedup，`def_read.cpp:2274-2285` | DB 已保存 parser 展开后的 names；`fromShadow()` 用 `hasInstance()` 去重后 append，`shadow_idb_group.h:49-52,63-70` | 保留 parser-built member vector，不重新执行 regex |
| `parse_group()` 复用同名 `_cur_group`，否则 fallback `add_group()`，`def_read.cpp:2294-2300` | 每个完整 `iGroupSD` row 只由 builder 创建一个 root；natural PK 保证一个 group name 对应一行 | `_cur_group` 是 split-callback 协调状态，完整 DB row 不需要持久化或模拟第二次 allocation |
| `parse_group()` Region lookup/set，`def_read.cpp:2301-2303` | member 恢复完成后，`fromShadow()` lookup Region 并 set，`shadow_idb_group.h:54-58` | `_region_name_sd` → active `IdbRegion*`；保持原 parser 的 member-before-Region 顺序 |
| `parse_group()` 清空 `_cur_group`，`def_read.cpp:2304` | 一条 shadow row 转换结束即完成该 root | 无持久字段 |
| property TODO，`def_read.cpp:2306-2308` | 无 schema field | 原始 iDB 未实现 |

## Split Callback Equivalence

原始 `DefRead` 将一个 Group record 拆成三类 callback：

1. `groupNameCallback()` → `parse_group_name()`：调用 `add_group(name)` 创建一次 `_cur_group`，`def_read.cpp:2176-2191,2238-2250`。
2. `groupMemberCallback()` → `parse_group_member()`：逐 member 向 `_cur_group` 添加 instance，`def_read.cpp:2194-2209,2253-2285`。`parse_group()` 本身不解析 instance。
3. `groupCallback()` → `parse_group()`：正常路径下名称相同，三目表达式复用 `_cur_group`，不再创建 Group；随后设置 Region 并清空临时状态，`def_read.cpp:2158-2173,2288-2304`。其中 fallback `add_group()` 只防御 `_cur_group` 缺失或名称不匹配。

EDADB 已把这三个 callbacks 的最终状态合并为一条 `iGroupSD` row。因此 `readIdbGroup()` 对该 row 调用一次 `add_group(_group_name_sd)`，再由 `fromShadow()` 恢复已展开的 member list 和 Region。两条路径在有效配对数据下都只创建一个 `IdbGroup`，并得到相同的 name、instances 和 Region；EDADB 不需要保存 `_cur_group`。

## EDADB Read/Write Paths

- Write：`writeIdbGroup()` 只负责 root vector、batch conversion/insert 和失败清理；字段转换在标准 `toShadow()` 中完成。
- Read order：`createDbByEdadb()` 先恢复 Region、Instance，再恢复 Group，`src/database/manager/builder/def_builder/def_read_edadb.cpp:218-223`。
- Read：`readIdbGroup()` reset list、读取完整 shadow row、按 name 创建一次 root，再调用标准 `fromShadow()`，`def_read_edadb.cpp:604-628`。
- `fromShadow()` 校验 builder 已设置的 group name，按 member → Region 顺序恢复引用。任何 stored reference lookup 失败都向上传播，builder reset 整个 Group list，避免保留部分对象。

## Order And Primary Key

- Root：Level D；不保存 `_order_sd`。`_group_name_sd` 只表达 identity，不表达顺序。
- Nested members：EDADB primitive-vector child table 使用内部 `__edadb_vec_idx` 恢复 vector 位置；该 index 不是 Group root PK。
- Regex expansion 顺序来自原始 `parse_group_member()` 遍历 active `IdbInstanceList` 的结果；EDADB 保存该最终顺序。

## Tests And Risks

- `aux_optional`：验证一个 Group root、name/Region、两个 ordered members，以及 member child table 物理反序后仍按 `__edadb_vec_idx` 恢复。
- `group_branches`：输入 regex `ctrl/_3[45]_` 后再重复写 `ctrl/_34_`，验证原始 parser 展开并去重为 `ctrl/_34_ ctrl/_35_`，EDADB 保存同一最终 storage view，输出还能再次通过原始 reader/writer。
- SQLite assertions：`src/database/edadb/test/run_idb_roundtrip_regression.sh:185-237`；fixture：`:651-661`；case checks：`:1017-1027`。
- 原始 writer 无条件解引用 `group->get_region()`；因此 `toShadow()` 同样拒绝 null Region。
- 对有效配对数据，parser 与 EDADB 的对象结果一致；若 DB 中保存的 Region/Instance name 在 active design 中不存在，adapter 按引用完整性错误处理，而不是静默生成不完整 Group。
