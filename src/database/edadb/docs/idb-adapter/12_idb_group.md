# IdbGroup EDADB Adapter Review

## Scope

`IdbGroup` 对应 DEF 的 `GROUPS` section。

- Write: `DefWrite::write_group()`
- Read: `groupCallback()` / `groupNameCallback()` / `groupMemberCallback()` / `DefRead::parse_group*()`
- EDADB Write: `DefWriteEdadb::writeIdbGroup()`
- EDADB Read: `DefReadEdadb::readIdbGroup()`

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
- `parse_group_member()`：按 DEF member 顺序查找 exact instance；如果不是 exact name，则按 regex 扫描 instance list，并避免重复加入。
- `parse_group()`：设置 region 引用，最后清空 `_cur_group`。

property 当前仍是 TBD，不进入 iDB 状态。

## EDADB Schema

当前 schema：

```cpp
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbGroup>, "iGroupSD", (_group_name_sd, _order_sd, _region_name_sd), (_instance_name_vec_sd));
```

保存字段覆盖原始 DEF writer/read 需要的 group name、region name 和 member instance names。

## Field Mapping To Original DEF Flow

以下按 EDADB shadow 域列出它对应的原始 DEF read/write 代码位置。

- Group identity / root order: `_group_name_sd`, `_order_sd`
  - Write source: `DefWrite::write_group()` 按 group list 顺序输出 group name，见 `src/database/manager/builder/def_builder/def_write.cpp:1106-1138`。
  - Read source: `groupNameCallback()` / `parse_group_name()` 创建 group，见 `src/database/manager/builder/def_builder/def_read.cpp:2176-2192` 和 `src/database/manager/builder/def_builder/def_read.cpp:2238-2251`。

- Member instance patterns: `_instance_name_vec_sd`
  - Write source: `write_group()` 输出 member instance list，见 `src/database/manager/builder/def_builder/def_write.cpp:1106-1138`。
  - Read source: `groupMemberCallback()` / `parse_group_member()` 保存 member pattern，见 `src/database/manager/builder/def_builder/def_read.cpp:2194-2236` 和 `src/database/manager/builder/def_builder/def_read.cpp:2253-2286`。

- Region ref: `_region_name_sd`
  - Write source: `write_group()` 输出 group region constraint，见 `src/database/manager/builder/def_builder/def_write.cpp:1106-1138`。
  - Read source: `groupCallback()` / `parse_group()` 读取 region name 并关联 region，见 `src/database/manager/builder/def_builder/def_read.cpp:2158-2174` 和 `src/database/manager/builder/def_builder/def_read.cpp:2288-2311`。

## Child Storage View

`IdbGroup` 是 `GROUPS` root，当前子节点是 instance name vector：

- `_instance_name_vec_sd`：primitive string vector child，保存 group member instance names 和 member 顺序。

不直接保存 `IdbInstance*` vector，也不保存 `IdbRegion*`：这些都是 non-owning references。DB 中保存 instance/region name，read 时通过 `IdbInstanceList::find_instance()` 和 `IdbRegionList::find_region()` 恢复。

## Why Group Shadow

当前需要 `Shadow<IdbGroup>`：

- `IdbGroup` 的 root identity 是 `_group_name`，因此 `_group_name_sd` 作为 PK。
- `IdbGroupList` 需要恢复 DEF append 顺序，但不能用 vector order index 作为 PK。
- `_order_sd` 单独保存 list order。
- `_region` 是 non-owning reference，DB 中保存 region name，read 时通过 `IdbRegionList::find_region()` 重建。
- `_instance_list` 是 instance references，DB 中保存 instance name vector，read 时通过 `IdbInstanceList::find_instance()` 重建。

## EDADB Write Path

当前 `writeIdbGroup()`：

- 从 `design->get_group_list()` 取得 group vector。
- 空列表返回 `kDbSuccess`，避免 EDADB dispatcher 中断整个写流程。
- 非空时按 list 顺序构造 `Shadow<IdbGroup>` pointer vector。
- `toShadow()` 保存 `_group_name_sd`、`_order_sd`、`_region_name_sd` 和 `_instance_name_vec_sd`。
- 使用 `edadb::insertVector<Shadow<IdbGroup>>()` 写入。

这与原始 DEF 输出字段一致；空列表返回值是 adapter 层为 dispatcher 做的语义调整。

## EDADB Read Path

当前 `readIdbGroup()`：

- 通过 `ORDER BY "_order_sd"` 读取 root records，恢复 `IdbGroupList` 原始 append 顺序。
- `group_list->add_group(group_name)` 创建 group。
- `fromShadow()` 恢复 group name。
- 通过 region name 查找并设置 region pointer。
- 按 `_instance_name_vec_sd` 顺序查找 instance 并加入 group instance list。
- `createDbByDef()` 不注册 group callbacks，避免 DEF 文本重复创建 group。

## Computed Fields

这些字段不直接入库：

- `_region` pointer：由 `_region_name_sd` 查找后重建。
- `_instance_list` pointer list：由 `_instance_name_vec_sd` 查找后重建。
- regex/group member pattern：原始 iDB 最终只保存匹配后的 instance list，不保存 pattern。
- property：原始 parser 未实现，不进入 EDADB schema。

## Order / Index

`IdbGroupList` 和 group member vector 都需要保持顺序。

依据：

- 原始 `parse_group_name()` 按 DEF 出现顺序 append group。
- 原始 `write_group()` 按 `group_list->get_group_list()` 当前顺序输出 group。
- 原始 `write_group()` 也按 group instance list 当前顺序输出 members。
- 当前 shadow 用 `_group_name_sd` 作为 root identity，用 `_order_sd` 保存 root list order。
- member vector 顺序由 EDADB primitive vector child table 的 `__edadb_vec_idx` 保存。
- read path 已显式按 `_order_sd` 恢复 root list，不依赖 EDADB/SQLite read-all 物理顺序。

当前状态：已实现。root identity 和 root order 已分离，member vector order 已回归验证。

## Tests

- demo `sky130_gcd` 覆盖空列表路径：`writeIdbGroup insert group_count=0`，`readIdbGroup restored group_count=0`。
- `src/database/edadb/test/run_idb_roundtrip_regression.sh` 的 `aux_optional` case 覆盖非空 group，并检查 `iGroupSD` count、`_order_sd`、region name 和 member order。

## Risks / TODO

- EDADB read 只恢复最终 instance list，不恢复原始 regex pattern；这与当前 iDB 状态一致，但不能反推出原始 DEF pattern 文本。
- 如果未来原始 parser 支持 group property，需要同步扩展 schema 和 read/write。
