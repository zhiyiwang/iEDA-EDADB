# IdbInstance EDADB Adapter Review

## Scope

`IdbInstance` 对应 DEF 的 `COMPONENTS` section。

- Write: `DefWrite::write_component()` at `src/database/manager/builder/def_builder/def_write.cpp:460`
- Read: `DefRead::parse_component_number()` / `DefRead::parse_component()` at `src/database/manager/builder/def_builder/def_read.cpp:857` and `src/database/manager/builder/def_builder/def_read.cpp:884`
- EDADB Write: `DefWriteEdadb::writeIdbInstance()` at `src/database/manager/builder/def_builder/def_write_edadb.cpp:317`
- EDADB Read: `DefReadEdadb::readIdbInstance()` at `src/database/manager/builder/def_builder/def_read_edadb.cpp:727`

本文件按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 的约束检查：

- DEF section 映射：`COMPONENTS` section。
- root-vector order 等级：`IdbInstanceList::_instance_list` 需要显式保留 append order。
- root identity 约束：component instance name 是 DEF-visible identity，当前 `_name_sd` 是 EDADB root PK；禁止用 vector order index 作为 PK。
- nested vector 约束：`IdbInstance` 本身不直接持久化 pin list / obs box list；这些由 cell master、placement 和后续 pin/net adapter 重建。

## Original Write Semantics

原始 `DefWrite::write_component()` 输出：

- component count: `instance_list->get_num()`
- instance name: `IdbInstance::_name`，写 DEF 前用 `ieda::Str::addBackslash()`
- cell master name: `instance->get_cell_master()->get_name()`
- optional source: `+ SOURCE ...`，当 `_type != kNone`
- placement status / coordinate / orient：当 `instance->has_placed()`
- optional halo: `+ HALO [SOFT] left bottom right top`
- optional route halo: `+ ROUTEHALO distance bottom_layer top_layer`

原始 writer 不输出 weight 和 region，虽然原始 parser 会读取它们。

对 EDADB adapter 来说，这个差异不能只按 writer 输出裁剪字段：`readIdbInstance()` 会替代原始 `parse_component()`，因此 DB 字段选择必须以 DEF read 解析后需要重建的 iDB 状态为准。结论是：

- writer 输出字段必须保存：name、master、source type、placement、halo、route halo。
- parser 会写入 iDB、且不能从其它上下文可靠重建的字段也要保存：weight、region name。
- writer 不输出但 parser 支持的字段，需要在文档中标明 “read-semantics field”，避免误以为它们来自 `write_component()` 文本输出。

## Original Read Semantics

原始 `DefRead::parse_component()`：

- `parse_component_number()` reserve instance vector。
- 按 DEF 出现顺序 `instance_list->add_instance(trimEscape(def_component->id()))`。
- 通过 component master name 从 LEF `IdbCellMasterList` 查找 master，并调用 `set_cell_master()` 初始化 instance pins。
- 设置 placement status、orient、source type、weight。
- 如果有 region name，则按 name 查找 region，设置 instance region，并将 instance 加入 region。
- 如果有 halo / route halo，则创建对应对象并设置字段。
- 设置 coordinate，并触发 bounding box、pin coordinate、halo coordinate、obs box 等计算。

## EDADB Schema

当前 schema：

```cpp
TABLE4CLASS(edadb::Shadow<idb::IdbInstance>, "iInstSD",
            (_name_sd, _order_sd, _type_sd, _status_sd, _orient_sd,
             _weight_sd, _cell_master_name_sd, _coordinate_sd,
             _halo_sd, _route_halo_sd, _region_name_sd));
```

字段选择依据是 “DEF read 解析后 iDB 应恢复什么”，而不是只保存 “当前 writer 会输出什么”：

- `_name_sd` / `_cell_master_name_sd` / `_type_sd` / `_status_sd` / `_orient_sd` / `_coordinate_sd` / `_halo_sd` / `_route_halo_sd`：同时对应原始 writer 输出和 parser 读取。
- `_weight_sd` / `_region_name_sd`：当前原始 writer 不输出，但原始 parser 会读取并写入 `IdbInstance`；EDADB read 替代 parser 后必须能恢复这些状态。
- `_order_sd`：不属于 DEF component 字段本身，用于恢复 `IdbInstanceList` append 顺序。

Schema / init 代码位置：

- `iHalo` table macro: `src/database/edadb/idb/edadb_idb_schema.h:85`
- `iRouteHaloSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:88`
- `iInstSD` root table macro: `src/database/edadb/idb/edadb_idb_schema.h:91`
- `IdbHalo` PK disabled: `src/database/edadb/idb/edadb_idb_init.cpp:32`
- `Shadow<IdbRouteHalo>` PK disabled: `src/database/edadb/idb/edadb_idb_init.cpp:33`
- `Shadow<IdbInstance>` PK uses EDADB default `true`; `_name_sd` is the first table column and root identity.
- Table registration: `src/database/edadb/idb/edadb_idb_init.cpp:87`
- Instance shadow definition: `src/database/edadb/idb/shadow/shadow_idb_instance.h:19`
- Route-halo shadow definition: `src/database/edadb/idb/shadow/shadow_idb_halo.h:12`

使用 `Shadow<IdbInstance>` 而不是 direct `IdbInstance`：

- root identity 是 `_name_sd`。
- `_order_sd` 保存 `IdbInstanceList` append 顺序。
- `_cell_master` / `_region` / route halo layers 通过 name lookup 重建，避免直接持久化运行时指针。

Primary-key audit:

- `Shadow<IdbInstance>` 保留默认 primary-key 行为，因为 `COMPONENTS` root record 有天然 DEF identity：instance name。
- `_order_sd` 只表达 `IdbInstanceList` append order，不作为 identity，也不能替代 `_name_sd`。
- `IdbHalo` 是 instance 的 optional nested scalar child，没有独立 root/list identity，因此在 `initPrimKeys()` 中关闭 PK。
- `Shadow<IdbRouteHalo>` 是 route halo 的 nested scalar storage view，没有独立 root/list identity，因此在 `initPrimKeys()` 中关闭 PK。
- `_coordinate_sd` 使用已有 coordinate shadow/storage view；它是 placement value child，不是 instance root identity。

## Field Mapping To Original DEF Flow

以下按 EDADB shadow 域列出它对应的原始 DEF read/write 代码位置。

- Instance identity / root order: `_name_sd`, `_order_sd`
  - Write source: `DefWrite::write_component()` 按 instance list 顺序输出 component name，见 `src/database/manager/builder/def_builder/def_write.cpp:460-515`。
  - Read source: `componentNumberCallback()` / `parse_component_number()` reserve list，`componentsCallback()` / `parse_component()` 按 DEF 出现顺序创建 instance，见 `src/database/manager/builder/def_builder/def_read.cpp:843-971`。

- Master cell: `_cell_master_name_sd`
  - Write source: `write_component()` 输出 cell master name，见 `src/database/manager/builder/def_builder/def_write.cpp:460-515`。
  - Read source: `parse_component()` 按 master name 从 layout cell master list lookup，并创建 instance pins，见 `src/database/manager/builder/def_builder/def_read.cpp:884-971`。

- Placement: `_status_sd`, `_orient_sd`, `_coordinate_sd`
  - Write source: `write_component()` 输出 placement status、coordinate、orient，见 `src/database/manager/builder/def_builder/def_write.cpp:460-515`。
  - Read source: `parse_component()` 读取 placement status/location/orient 并更新 bbox/pins，见 `src/database/manager/builder/def_builder/def_read.cpp:884-971`。

- Optional component properties: `_type_sd`, `_weight_sd`, `_halo_sd`, `_route_halo_sd`, `_region_name_sd`
  - Write source: `write_component()` 输出 source type、halo、route halo；不输出 weight / region，见 `src/database/manager/builder/def_builder/def_write.cpp:480`、`src/database/manager/builder/def_builder/def_write.cpp:493`、`src/database/manager/builder/def_builder/def_write.cpp:501`。
  - Read source: `parse_component()` 读取 source type、weight、region、halo、route halo，并按 region/layer name lookup，见 `src/database/manager/builder/def_builder/def_read.cpp:922`、`src/database/manager/builder/def_builder/def_read.cpp:926`、`src/database/manager/builder/def_builder/def_read.cpp:930`、`src/database/manager/builder/def_builder/def_read.cpp:938`、`src/database/manager/builder/def_builder/def_read.cpp:950`。
  - DB decision: `_weight_sd` 和 `_region_name_sd` 属于 read-semantics fields；它们不是当前 writer 文本输出字段，但为了让 EDADB read 后的 active iDB 等价于原始 DEF parser 结果，仍需要进入 DB。

- Computed bbox/pin geometry
  - Write source: DEF writer 不直接输出 instance bbox 或 pin absolute geometry，见 `src/database/manager/builder/def_builder/def_write.cpp:460-515`。
  - Read source: 原始 `parse_component()` 设置 placement 后重建 bbox 和 pin geometry，见 `src/database/manager/builder/def_builder/def_read.cpp:884-971`。

## Child Storage View

`IdbInstance` 是 `COMPONENTS` root，当前子节点/引用处理如下：

- `_coordinate_sd`：coordinate value child，保存 placement x/y。
- `_halo_sd`：direct `IdbHalo` child；halo 只包含 left/right/top/bottom/soft 纯 DEF 字段，不需要 shadow。
- `_route_halo_sd`：`Shadow<IdbRouteHalo>` child；route halo 的 bottom/top layer 是 LEF layer pointer，DB 中保存 layer name。
- `_cell_master`：不作为 child 存库；保存 `_cell_master_name_sd`，read 时从 LEF `IdbCellMasterList` 查找并调用 `set_cell_master()` 重建 pin list。
- `_region`：不作为 child 存库；保存 `_region_name_sd`，read 时查找 region 并补回 region-instance 关系。
- `_pin_list` / obs boxes / bbox：不入库，由 `set_cell_master()`、coordinate/orient 设置和 iDB 计算流程重建。

因此 `Shadow<IdbInstance>` 是必要的：它把 DEF component 语义从 iEDA 运行时对象图中拆出来，避免把 cell master、region、layer 等 non-owning pointers 当成 DB ownership。

## EDADB Write Path

当前 `writeIdbInstance()`：

- Code: `src/database/manager/builder/def_builder/def_write_edadb.cpp:317`
- Enabled by chip writer: `src/database/manager/builder/def_builder/def_write_edadb.cpp:94`

- 从 `design->get_instance_list()` 获取 instance vector。
- 空列表返回成功，兼容 EDADB framework。
- 按 vector 顺序构造 `Shadow<IdbInstance>`，第 `idx` 个写 `_order_sd = idx`。
- 写入 `_name_sd`、cell master name、type/status/orient/weight、coordinate、halo、route halo、region name。

这覆盖了原始 writer 输出字段，并保存 parser 语义字段 weight/region。原因是 EDADB read 会替代 `parse_component()`：如果 DB 不保存 weight/region，EDADB read 后的 active iDB 就无法完整等价于原始 DEF read 解析结果。

## EDADB Read Path

当前 `readIdbInstance()`：

- Code: `src/database/manager/builder/def_builder/def_read_edadb.cpp:727`
- Enabled by EDADB read flow: `src/database/manager/builder/def_builder/def_read_edadb.cpp:216`

- reset 当前 instance list，避免 DEF 文本 callback 与 EDADB 读回重复。
- 用 `ORDER BY "_order_sd"` 读取 `iInstSD`，恢复 `IdbInstanceList` 原始 append 顺序。
- 通过 `_cell_master_name_sd` 查找 LEF master，调用 `set_cell_master()` 重建 pin list。
- `fromShadow()` 恢复 name、type、status、orient、weight、coordinate、halo。
- route halo layer 和 region 通过 name lookup 恢复。
- 最后 `instance_list->add_instance(inst)`，保持 map 和 vector 同步。

`createDbByDef()` 使用 `defrUnsetComponentCbk()` / `defrUnsetComponentStartCbk()` / `defrUnsetComponentEndCbk()` 清掉 DEF component callbacks，因此 `COMPONENTS` 只来自 EDADB。

## Computed Fields

不直接按 DEF 字段入库、但读回时由 iDB 计算：

- instance bounding box
- instance pin coordinates / pin bounding boxes
- halo coordinate
- obs box list
- net/group/blockage 到 instance 的引用，由后续 adapter 通过 instance name 查找恢复

## Order / Index

`IdbInstanceList` 顺序需要保持：

- 原始 parser 按 DEF `COMPONENTS` 出现顺序 append。
- 原始 writer 按 `get_instance_list()` 当前顺序输出。
- 多个工具会遍历 instance vector；顺序变化虽然通常不改变逻辑 identity，但会影响文本 diff 和部分流程稳定性。

当前已实现：`_name_sd` 作为 identity，`_order_sd` 作为 root list order，read path 显式 `ORDER BY "_order_sd"`。

## Tests

当前回归覆盖：

- `iInstSD` count。
- sample instance 的 name、cell master、status、orient、coordinate。
- default fixture 中 sample instance 的 `_weight_sd=-1`、`_region_name_sd=''`，保持 iEDA 未设置 `WEIGHT` 时的默认状态。
- aux optional fixture 给 `ctrl/_34_` 注入 DEF `+ WEIGHT 13` / `+ REGION test_region`，并用 SQL 验证 `iInstSD._weight_sd` / `iInstSD._region_name_sd` 已写入 DB。
- `IdbInstanceList` 前缀顺序。
- `writeIdbInstance` / `readIdbInstance` 日志。
- demo DEF roundtrip diff clean。

## Risks / TODO

- 当前 sky130 demo 主要覆盖 placed instance。
- halo / route halo 仍需要专门 optional fixture 继续扩展覆盖。
