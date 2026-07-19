# IdbInstance EDADB Adapter Review

## Scope And Constraints

`IdbInstance` 对应 DEF `COMPONENTS`，root container 是 `IdbInstanceList::_instance_list`。

- 原始 write：`src/database/manager/builder/def_builder/def_write.cpp:460`
- 原始 read：`src/database/manager/builder/def_builder/def_read.cpp:857`、`src/database/manager/builder/def_builder/def_read.cpp:884`
- EDADB write/read：`src/database/manager/builder/def_builder/def_write_edadb.cpp:330`、`src/database/manager/builder/def_builder/def_read_edadb.cpp:710`
- Order level：按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 为 Level C。iPL 按 vector append order 分配 instance IDs，固定随机种子下重排会改变 placement 状态。
- Root identity：instance name；`_name_sd` 是 PK，`_order_sd` 只保存 list order。

## Original DEF Write/Read Roundtrip Mapping

### Original DEF Write Flow

| 原始 `DefWrite` 顺序 | EDADB write / `toShadow()` 对应 | DEF 域 / iDB 成员 |
| --- | --- | --- |
| 校验 list/count，输出 `COMPONENTS <N>` 并遍历 vector，见 `def_write.cpp:462-476` | `writeIdbInstance()` 取同一 vector，按 index 构造 shadow batch，见 `def_write_edadb.cpp:331-366` | section count / `IdbInstanceList::_instance_list` / `_order_sd` |
| escape name，输出 master 与可选 `SOURCE`，见 `def_write.cpp:477-484` | 保存 `_name_sd/_cell_master_name_sd/_type_sd`，见 `shadow_idb_instance.h:38-51` | component name/master/source |
| `has_placed()` 时输出 status、coordinate、orient，见 `def_write.cpp:486-491` | 保存 `_status_sd/_coordinate_sd/_orient_sd` | placement statement |
| 可选输出 `HALO [SOFT] left bottom right top`，见 `def_write.cpp:493-499` | `_halo_sd` 保存 `IdbHalo` scalar child | halo source fields |
| 可选输出 `ROUTEHALO distance bottom top`，见 `def_write.cpp:501-506` | `Shadow<IdbRouteHalo>` 保存 distance 与 layer names，见 `shadow_idb_halo.h:17-28` | route-halo source fields |
| 当前 writer 不输出 `WEIGHT/REGION` | EDADB 仍保存 `_weight_sd/_region_name_sd`，因为它们是 parser-supported active iDB state | parser-only source fields |

### Original DEF Read Flow

| 原始 `DefRead` 顺序 | EDADB read / `fromShadow()` 对应 | DEF 域 / iDB 成员 |
| --- | --- | --- |
| `parse_component_number()` reserve vector，见 `def_read.cpp:857-863` | 不保存 count；reset 后按 `_order_sd` query，见 `def_read_edadb.cpp:723-734` | `COMPONENTS <N>` / capacity/order |
| lookup LEF master，trim name，append instance，调用 `set_cell_master()` 重建 pins，见 `def_read.cpp:891-918` | helper 按 `_cell_master_name_sd` lookup；`fromShadow()` 设置 name/master，见 `shadow_idb_instance.h:75-89` | name/master reference |
| 设置 status、orient 和 optional source，见 `def_read.cpp:919-924` | 设置 status；`set_orient(..., false)` 避免提前计算；设置 type，见 `shadow_idb_instance.h:89-92` | placement/source |
| 条件设置 weight，见 `def_read.cpp:926-928` | 恢复 `_weight_sd` | `WEIGHT` |
| 条件 lookup region，设置 instance ref 和 region backlink，见 `def_read.cpp:930-936` | helper lookup name，恢复同一双向关系，见 `shadow_idb_instance.h:94-101` | `REGION` |
| 条件创建并设置 halo，见 `def_read.cpp:938-948` | 将 EDADB 读出的 optional halo child 转交 instance ownership，见 `shadow_idb_instance.h:103-106` | `HALO` |
| 条件创建 route halo，并按 name lookup layers，见 `def_read.cpp:950-955` | route-halo shadow 恢复 distance/layers，见 `shadow_idb_instance.h:108-113`、`shadow_idb_halo.h:31-39` | `ROUTEHALO` |
| 最后设置 coordinate，触发 bbox、pin coordinate、halo coordinate、obs boxes，见 `def_read.cpp:957` | 最后调用 `set_coodinate()`，见 `shadow_idb_instance.h:115-117` | placement x/y + derived state |

## Schema And Shadow Audit

```cpp
TABLE4CLASS(edadb::Shadow<idb::IdbInstance>, "iInstSD",
            (_name_sd, _order_sd, _type_sd, _status_sd, _orient_sd,
             _weight_sd, _cell_master_name_sd, _coordinate_sd,
             _halo_sd, _route_halo_sd, _region_name_sd));
TABLE4CLASS(idb::IdbHalo, "iHalo", (...));
TABLE4CLASS(edadb::Shadow<idb::IdbRouteHalo>, "iRouteHaloSD", (...));
```

- Schema：`src/database/edadb/idb/edadb_idb_schema.h:84-91`
- PK setup：`src/database/edadb/idb/edadb_idb_init.cpp:21-30`
- Root registration：`src/database/edadb/idb/edadb_idb_init.cpp:82`
- Instance shadow：`src/database/edadb/idb/shadow/shadow_idb_instance.h:20-141`
- Route-halo shadow：`src/database/edadb/idb/shadow/shadow_idb_halo.h:15-46`

Shadow 必要性：

- `_cell_master`、`_region`、route-halo layers 是 non-owning references；DB 保存 names，read 时从 active LEF/design lookup。
- `_pin_list`、bbox、pin coordinates、halo coordinates、obs boxes 是 setter 构建或派生状态，不入库。
- `_coordinate_sd` 保存 DEF placement value；`_halo_sd` 是纯 scalar optional child；route halo 因 layer pointers 使用 shadow。

Primary-key audit：

- `_name_sd` 是 root PK；`_order_sd` 不是 PK。
- `IdbHalo` 和 `Shadow<IdbRouteHalo>` 都是 optional inline child，没有独立 identity，`hasPrimKey=false`。
- coordinate child 是 placement value，不承担 root identity。

## EDADB Paths

Write：

- `writeIdbInstance()` 检查每次 `toShadow()`，再使用一次 batch insert，见 `def_write_edadb.cpp:330-377`。
- 当前实现先构造完整 shadow vector，DB 侧仍是 single transaction/prepared operation；百万级 instance 若临时内存成为问题，再改 streaming batch，不能退回逐个 transaction。

Read：

- `readIdbInstance()` 只负责 reset、ordered cursor、错误处理和 append，见 `def_read_edadb.cpp:710-767`。
- master/region/layer lookup、backlink 和派生状态重建都在标准 `fromShadow(IdbInstance*, uint32_t*)` 内；没有自定义 shadow 接口。
- `EdadbIdbHelper` 提供 active design/layout lookup，见 `src/database/edadb/idb/edadb_idb_helper.h:42-169`。

## Stored And Computed State

Stored DEF/parser state：

- name、master name、source type、status、orient、placement coordinate
- weight、region name
- halo soft/extensions
- route-halo distance 与 bottom/top layer names
- root `_order_sd`

Read-time computed/rebuilt state：

- cell-master pin list
- region-instance backlink
- instance bbox、pin coordinates/bboxes、halo coordinate、obs boxes
- later net/group/blockage references by instance name

## Tests

`src/database/edadb/test/run_idb_roundtrip_regression.sh` 覆盖：

- 1458 instances 的 count、sample name/master/status/orient/coordinate。
- `WEIGHT/REGION` optional fields 与 region backlink 路径。
- `instance_branches` fixture 覆盖 soft HALO、四边 extension、ROUTEHALO distance/layer refs。
- 将 `iInstSD` 物理行逆序后，`ORDER BY _order_sd` 恢复原 append order，最终 DEF 与 direct path 一致。
- write/read logs 与 default/aux/routed regressions。

## Known Native Writer Differences

- 原始 writer 不输出 parser 支持的 `WEIGHT/REGION`；必须用 DB SQL/read-state 测试，不能只看最终 DEF。
- 原始 writer 对 soft halo 输出字面量 `[SOFT]`，见 `def_write.cpp:496`；该输出无法被当前 DEF parser 二次解析。专项测试验证 direct 与 EDADB 输出一致，但不把这项原生问题误判为 adapter 问题。
- 原始 writer 对空 component list 返回失败，EDADB 空 batch 返回成功。
