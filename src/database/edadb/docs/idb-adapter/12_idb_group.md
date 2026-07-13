# IdbGroup EDADB Adapter Review

## Scope

`IdbGroup` 对应 DEF 的 `GROUPS` section。

- Write: `DefWrite::write_group()`
- Read: `groupCallback()` / `groupNameCallback()` / `groupMemberCallback()` / `DefRead::parse_group*()`
- EDADB Write: `DefWriteEdadb::writeIdbGroup()`
- EDADB Read: `DefReadEdadb::readIdbGroup()`

本文件按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 的约束检查：

- DEF section 映射：`GROUPS` section。
- iEDA root container：`IdbGroupList::_group_list`。
- root-vector order 等级：Level D，当前没有发现点工具依赖 `IdbGroupList::_group_list` 的 root index/order。
- root identity 约束：group name 是 DEF-visible identity，当前用 `_group_name_sd` 作为 EDADB root PK；不使用 vector order index 作为 PK。
- nested vector 约束：group member vector 是 group record 内部成员顺序，必须随 root record 保持原始顺序，不参与 D-level root sort。

## Original Write Semantics

原始 `DefWrite::write_group()` 输出：

- group count: `group_list->get_num()`
- group name: `group->_group_name`
- member instance names: `group->_instance_list`
- region name: `group->_region->_name`

空 group list 时，原始 writer 返回 `kDbFail`，但 top-level DEF writer 不把它当作整体失败。

## Original Read Semantics

原始 group parser 分三步：

- `parse_group_name()`：按 DEF 出现顺序 `group_list->add_group(group_name)`，并设置 `_cur_group`。
- `parse_group_member()`：按 DEF member 顺序查找 exact instance；如果不是 exact name，则按 regex 扫描 instance list，并避免重复加入；iDB 最终保存的是展开后的 instance list，不保存原始 pattern 文本。
- `parse_group()`：设置 region 引用，最后清空 `_cur_group`。

property 当前仍是 TBD，不进入 iDB 状态。

## EDADB Schema

当前 schema：

```cpp
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbGroup>, "iGroupSD", (_group_name_sd, _region_name_sd), (_instance_name_vec_sd));
```

Schema / init 代码位置：

- `iGroupSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:95`
- Primary-key setup: `src/database/edadb/idb/edadb_idb_init.cpp:21`
- Table registration: `src/database/edadb/idb/edadb_idb_init.cpp:90`
- Shadow definition: `src/database/edadb/idb/shadow/shadow_idb_group.h:15`

保存字段覆盖原始 DEF writer/read 需要的 group name、region name 和 member instance names。

Schema 与 order/index 约束的关系：

- 依据 `src/database/edadb/docs/def-ieda-mapping-and-order.md`，`GROUPS` 映射到 `IdbGroupList::_group_list`，等级为 Level D。
- Level D 的含义是当前未发现点工具依赖 root vector index/order；normalized diff 可以按 group name 排序 `GROUPS` root records。
- 当前 adapter 不保存 `_order_sd`，read path 不指定 root order；root order-only 文本差异由 normalized diff 处理。
- `_instance_name_vec_sd` 是 group 内部 primitive string vector，EDADB primitive vector child table 使用 `__edadb_vec_idx` 保存 member order。

Primary-key audit:

- `initPrimKeys()` 没有关闭 `Shadow<IdbGroup>` 的 primary-key 行为；`_group_name_sd` 是 table 第一列和 root identity。
- 不定义 `_order_sd`；`GROUPS` root order 是 Level D，不作为 iEDA 点工具语义约束。
- `initReadDb()` / `initWriteDb()` 都先调用 `initPrimKeys()`，再调用 `initAllTables()`，因此 read/write 的 table metadata 一致。

## Original DEF Write/Read Roundtrip Mapping

### Original DEF Write Flow

| 原始 `DefWrite` 执行顺序 | EDADB write / `toShadow` 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. `write_group()` 检查 list；空 list 返回失败；输出 section count，见 `def_write.cpp:1108-1120` | `writeIdbGroup()` 检查 list；空 vector 返回成功；构造 group shadows 后 batch insert，见 `def_write_edadb.cpp:463-492` | `GROUPS <N>` / `IdbGroupList::_group_list` / `iGroupSD` row count |
| 2. 按 group vector 遍历并输出 group name，见 `def_write.cpp:1122-1123` | `toShadow()` 保存 `_group_name_sd`，见 `shadow_idb_group.h:20-24` | `- <group_name>` / `IdbGroup::_group_name` / `_group_name_sd` |
| 3. 按当前 expanded instance vector 顺序输出 member names，见 `def_write.cpp:1125-1127` | `toShadow()` 保存有序 `_instance_name_vec_sd`，见 `shadow_idb_group.h:28-31` | group members / `IdbGroup::_instance_list` 中的 `IdbInstance::_name` / `_instance_name_vec_sd` |
| 4. 无条件解引用 region 并输出 region name，见 `def_write.cpp:1129` | region 非空时才保存 `_region_name_sd`，见 `shadow_idb_group.h:25-27`；因此 EDADB 可存“无 region group”，但原始 writer 后续输出该状态会空指针解引用 | `+ REGION` / `IdbGroup::_region` / `_region_name_sd` |
| 5. 输出 record/section terminator，见 `def_write.cpp:1131-1134` | 由 row/vector 边界重建，不存文本终止符 | `;`, `END GROUPS` / 无 iDB 成员 / 无 EDADB 字段 |

### Original DEF Read Flow

| 原始 `DefRead` 执行顺序 | EDADB read / `fromShadow` 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. `groupNameCallback()` 调 `parse_group_name()`，按 name 创建 `_cur_group`，见 `def_read.cpp:2176-2192,2238-2250` | `readIdbGroup()` 从 `_group_name_sd` 创建 group；`fromShadow()` 同步 name，见 `def_read_edadb.cpp:584-616`, `shadow_idb_group.h:34-40` | group name / `IdbGroup::_group_name` / `_group_name_sd` |
| 2. 每个 `groupMemberCallback()` 调 `parse_group_member()`：先 exact-name lookup，再把 pattern 当 regex 遍历全部 instances，且去重，见 `def_read.cpp:2194-2209,2253-2285` | EDADB 不保存原始 pattern；保存 parser 展开后的最终 instance-name vector，read 时按 name lookup 并 append，见 `def_read_edadb.cpp:621-626` | member pattern → expanded members / `IdbGroup::_instance_list` / `_instance_name_vec_sd` |
| 3. 最终 `groupCallback()` 调 `parse_group()`，复用 `_cur_group` 或按 name 新建 group，见 `def_read.cpp:2158-2173,2288-2300` | EDADB 每个 shadow 对应一个已展开的 final group；不需要 callback 临时状态 | finalized group root / `IdbGroupList::_group_list` / one `iGroupSD` row |
| 4. 有 region name 时按 name lookup 并设置 region pointer，随后清空 `_cur_group`，见 `def_read.cpp:2301-2304` | `_region_name_sd` 非空时执行同样 lookup；空 name 保持 null，见 `def_read_edadb.cpp:617-619` | `+ REGION` / `IdbGroup::_region` / `_region_name_sd` |
| 5. component-name pattern/property 分支仍为 TODO，见 `def_read.cpp:2306-2308` | schema 不保存 property；pattern 已被 canonicalize 为 expanded instance names | property/original pattern / 无已实现 property 成员；最终 member vector / 无 property 字段；`_instance_name_vec_sd` |

## Child Storage View

`IdbGroup` 是 `GROUPS` root，当前子节点是 instance name vector：

- `_instance_name_vec_sd`：primitive string vector child，保存 group member instance names 和 member 顺序。

不直接保存 `IdbInstance*` vector，也不保存 `IdbRegion*`：这些都是 non-owning references。DB 中保存 instance/region name，read 时通过 `IdbInstanceList::find_instance()` 和 `IdbRegionList::find_region()` 恢复。

## Why Group Shadow

当前需要 `Shadow<IdbGroup>`：

- `IdbGroup` 的 root identity 是 `_group_name`，因此 `_group_name_sd` 作为 PK。
- `IdbGroupList` 是 Level D root list；当前不保存 root append order，也不使用 vector order index。
- `_region` 是 non-owning reference，DB 中保存 region name，read 时通过 `IdbRegionList::find_region()` 重建。
- `_instance_list` 是 instance references，DB 中保存 instance name vector，read 时通过 `IdbInstanceList::find_instance()` 重建。

## EDADB Write Path

当前 `writeIdbGroup()`：

- Code: `src/database/manager/builder/def_builder/def_write_edadb.cpp:463`
- Group vector access: `src/database/manager/builder/def_builder/def_write_edadb.cpp:476`
- Empty-list return: `src/database/manager/builder/def_builder/def_write_edadb.cpp:480`
- Shadow construction: `src/database/manager/builder/def_builder/def_write_edadb.cpp:484`
- EDADB insert: `src/database/manager/builder/def_builder/def_write_edadb.cpp:492`

- 从 `design->get_group_list()` 取得 group vector。
- 空列表返回 `kDbSuccess`，避免 EDADB dispatcher 中断整个写流程。
- 非空时构造 `Shadow<IdbGroup>` pointer vector。
- `toShadow()` 保存 `_group_name_sd`、`_region_name_sd` 和 `_instance_name_vec_sd`。
- 使用 `edadb::insertVector<Shadow<IdbGroup>>()` 写入。

这与原始 DEF 输出字段一致；空列表返回值是 adapter 层为 dispatcher 做的语义调整。

## EDADB Read Path

当前 `readIdbGroup()`：

- Code: `src/database/manager/builder/def_builder/def_read_edadb.cpp:584`
- EDADB read op: `src/database/manager/builder/def_builder/def_read_edadb.cpp:599`
- EDADB read loop: `src/database/manager/builder/def_builder/def_read_edadb.cpp:602`
- Add to active list: `src/database/manager/builder/def_builder/def_read_edadb.cpp:615`
- Region lookup: `src/database/manager/builder/def_builder/def_read_edadb.cpp:617`
- Instance lookup: `src/database/manager/builder/def_builder/def_read_edadb.cpp:621`

- 使用 EDADB read-all 读取 root records，不指定 root order。
- `group_list->add_group(group_name)` 创建 group。
- `fromShadow()` 恢复 group name。
- 通过 region name 查找并设置 region pointer；空 region name 保持 nullptr，贴近原始 `parse_group()` 的 optional region 判断。
- 按 `_instance_name_vec_sd` 顺序查找 instance 并加入 group instance list。
- `createDbByDef()` 不注册 group callbacks，避免 DEF 文本重复创建 group。

## Computed Fields

这些字段不直接入库：

- `_region` pointer：由 `_region_name_sd` 查找后重建。
- `_instance_list` pointer list：由 `_instance_name_vec_sd` 查找后重建。
- regex/group member pattern：原始 iDB 最终只保存匹配后的 instance list，不保存 pattern。
- property：原始 parser 未实现，不进入 EDADB schema。

## Order / Index

`IdbGroupList` 不强制保持 root 顺序；group member vector 需要保持顺序。

依据：

- 原始 `parse_group_name()` 按 DEF 出现顺序 append group。
- 原始 `write_group()` 按 `group_list->get_group_list()` 当前顺序输出 group。
- 原始 `write_group()` 也按 group instance list 当前顺序输出 members。
- `src/operation/iPL` 中的 `Group` 是 placer 自己的 topology group，不是 `IdbGroupList` root group；当前未发现点工具依赖 `IdbGroupList::_group_list` index。
- 因此 `GROUPS` root order 在点工具语义上是 Level D；当前 shadow 用 `_group_name_sd` 作为 root identity，不保存 root list order。
- member vector 顺序由 EDADB primitive vector child table 的 `__edadb_vec_idx` 保存。
- read path 不依赖 EDADB/SQLite read-all 物理顺序表达语义；root-order-only 文本差异由 normalized diff 处理。

当前状态：已实现。root identity 和 root order 已分离；root order 不保存，member vector order 已回归验证。

对 normalized diff 的影响：

- `GROUPS` 是 Level D root list；如果 raw diff 只因为不同 group root record 顺序失败，normalized diff 可以按 group name 排序后通过。
- 排序单位必须是完整 group record；record 内部 member instance vector 不排序。
- 如果 group name、region name 或 member instance 内容不同，normalized diff 必须失败。

## Tests

- demo `sky130_gcd` 覆盖空列表路径：`writeIdbGroup insert group_count=0`，`readIdbGroup restored group_count=0`。
- `src/database/edadb/test/run_idb_roundtrip_regression.sh` 的 `aux_optional` case 覆盖非空 group，并检查 `iGroupSD` count、确认没有 `_order_sd` column、region name 和 member order。

## Risks / TODO

- EDADB read 只恢复最终 instance list，不恢复原始 regex pattern；这与当前 iDB 状态一致，但不能反推出原始 DEF pattern 文本。
- 原始 writer 无条件解引用 `group->get_region()`；parser/EDADB 可产生 region 为 null 的 group，写 DEF 时存在空指针风险。
- 原始 writer 对空 group list 返回失败，EDADB writer 对空 vector 返回成功。
- `readIdbGroup()` 不清空现有 list；找不到的 instance name 会被跳过而不是报错。
- 如果未来原始 parser 支持 group property，需要同步扩展 schema 和 read/write。
