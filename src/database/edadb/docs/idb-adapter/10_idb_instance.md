# IdbInstance EDADB Adapter Review

## Scope

`IdbInstance` 对应 DEF 的 `COMPONENTS` section。

- Write: `DefWrite::write_component()`
- Read: `componentNumberCallback()` / `componentsCallback()` / `DefRead::parse_component_number()` / `DefRead::parse_component()`
- EDADB Write: `DefWriteEdadb::writeIdbInstance()`
- EDADB Read: `DefReadEdadb::readIdbInstance()`

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

使用 `Shadow<IdbInstance>` 而不是 direct `IdbInstance`：

- root identity 是 `_name_sd`。
- `_order_sd` 保存 `IdbInstanceList` append 顺序。
- `_cell_master` / `_region` / route halo layers 通过 name lookup 重建，避免直接持久化运行时指针。

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

- 从 `design->get_instance_list()` 获取 instance vector。
- 空列表返回成功，兼容 EDADB framework。
- 按 vector 顺序构造 `Shadow<IdbInstance>`，第 `idx` 个写 `_order_sd = idx`。
- 写入 `_name_sd`、cell master name、type/status/orient/weight、coordinate、halo、route halo、region name。

这覆盖了原始 writer 输出字段，并额外保存 parser 已支持的 weight/region，供后续对象关系重建。

## EDADB Read Path

当前 `readIdbInstance()`：

- reset 当前 instance list，避免 DEF 文本 callback 与 EDADB 读回重复。
- 用 `ORDER BY "_order_sd"` 读取 `iInstSD`，恢复 `IdbInstanceList` 原始 append 顺序。
- 通过 `_cell_master_name_sd` 查找 LEF master，调用 `set_cell_master()` 重建 pin list。
- `fromShadow()` 恢复 name、type、status、orient、weight、coordinate、halo。
- route halo layer 和 region 通过 name lookup 恢复。
- 最后 `instance_list->add_instance(inst)`，保持 map 和 vector 同步。

`createDbByDef()` 已禁用 component callbacks，因此 `COMPONENTS` 只来自 EDADB。

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
- `IdbInstanceList` 前缀顺序。
- `writeIdbInstance` / `readIdbInstance` 日志。
- demo DEF roundtrip diff clean。

## Risks / TODO

- 原始 writer 不输出 region/weight，但 EDADB 保存它们；这是为了保留 parser 已读入的语义和后续引用恢复。
- halo / route halo 目前依赖 optional fixture 继续扩展覆盖；默认 sky130 case主要覆盖 placed instance。
