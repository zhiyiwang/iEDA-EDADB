# IdbTrackGrid EDADB Adapter Review

## Scope

`IdbTrackGrid` 对应 DEF 的 `TRACKS` statements。

- Write: `DefWrite::write_track_grid()`
- Read: `trackGridCallback()` / `DefRead::parse_track_grid()`
- EDADB Write: `DefWriteEdadb::writeIdbTrackGrid()`
- EDADB Read: `DefReadEdadb::readIdbTrackGrid()`

本文件按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 的约束检查：

- DEF section 映射：`TRACKS` statements。
- root-vector order 等级：Level D，当前没有发现点工具依赖 `IdbTrackGridList::_track_grid_list` 的 root index/order。
- nested vector 约束：`_layer_name_vec_sd` 是 track grid 内部的 layer-name list，应随 root record 保持原始顺序，不参与 D-level root sort。

## Original Write Semantics

原始 `DefWrite::write_track_grid()` 对每个 track grid 输出：

- direction: `track->_track->_direction`
- start: `track->_track->_start`
- DO count: `track->_track_num`
- STEP pitch: `track->_track->_pitch`
- layer names: `track->_layer_list[*]->_name`

不直接输出：

- `IdbTrack::_width`。
- 完整 `IdbLayer` 对象内容。
- routing layer 上的反向 `_track_grid_list`，它是 read 后重建的引用关系。

## Original Read Semantics

原始 `DefRead::parse_track_grid()`：

- 在 `layout->get_track_grid_list()` 中创建 `IdbTrackGrid`。
- 由 `def_track->macro()[0]` 设置 direction：`X` -> `kDirectionX`，否则 `kDirectionY`。
- 设置 start、pitch、track number。
- 按 DEF layer name 在 LEF `layout->get_layers()` 中查找 `IdbLayer`。
- 找到 layer 时加入 `track_grid->_layer_list`；如果是 routing layer，再把 track grid 挂到 `IdbLayerRouting::_track_grid_list`。
- 找不到 layer 时只打印 `Track Grid Error : no layer exist...`，不返回失败。

## EDADB Schema

当前 schema：

```cpp
TABLE4CLASS(idb::IdbTrack, "iTrack", (_start, _direction, _pitch));
TABLE4SHADOW_WVEC(idb::IdbTrackGrid);
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbTrackGrid>, "iTrackGridSD", (primary_key, _order_sd, _track_num_sd, _track_sd), (_layer_name_vec_sd));
```

Schema / init 代码位置：

- `IdbTrack` table macro: `src/database/edadb/idb/edadb_idb_schema.h:54`
- `TABLE4SHADOW_WVEC(idb::IdbTrackGrid)`: `src/database/edadb/idb/edadb_idb_schema.h:57`
- `iTrackGridSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:58`
- Primary-key setup: `src/database/edadb/idb/edadb_idb_init.cpp:21`
- Table registration: `src/database/edadb/idb/edadb_idb_init.cpp:82`

保存字段覆盖原始 DEF writer 需要的内容：direction、start、pitch、track number、layer name vector。

Schema 与 order/index 约束的关系：

- 依据 `src/database/edadb/docs/def-ieda-mapping-and-order.md`，`TRACKS` 映射到 `IdbTrackGridList::_track_grid_list`，等级为 Level D。
- Level D 的含义是当前未发现点工具依赖 root vector index/order；normalized diff 可以按 stable key 排序 `TRACKS` root records。
- 当前 adapter 仍保存 `_order_sd` 并按它读回，这是为了严格 raw DEF roundtrip 和可重复输出；它比 Level D 最低要求更严格。
- `primary_key` 是 root identity；`_order_sd` 只表达原始 append 顺序，禁止用 vector order index 作为 PK。
- `_layer_name_vec_sd` 是 nested vector，必须保持该 `TRACKS` record 内部 layer name 顺序。

Primary-key 约束：

- `initPrimKeys()` 显式关闭 `IdbTrack` 的 primary-key 行为；`IdbTrack` 是 `Shadow<IdbTrackGrid>` 内部 inline 标量对象。
- `initPrimKeys()` 没有关闭 `Shadow<IdbTrackGrid>` 的 primary-key 行为；`primary_key` 是 `iTrackGridSD` root identity。
- `initReadDb()` / `initWriteDb()` 都先调用 `initPrimKeys()`，再调用 `initAllTables()`，因此 read/write 的 table metadata 一致。

## Field Mapping To Original DEF Flow

以下按 EDADB shadow 域列出它对应的原始 DEF read/write 代码位置。

- Root identity / order: `primary_key`, `_order_sd`
  - Write source: `DefWrite::write_track_grid()` 按 `IdbTrackGridList` 顺序输出 `TRACKS`，见 `src/database/manager/builder/def_builder/def_write.cpp:362-388`。
  - Read source: `trackGridCallback()` / `parse_track_grid()` 按 DEF 出现顺序创建 track grid，见 `src/database/manager/builder/def_builder/def_read.cpp:731-787`。

- Track axis fields: `_track_sd`, `_track_num_sd`
  - Write source: `write_track_grid()` 输出 direction/start/track count/pitch，见 `src/database/manager/builder/def_builder/def_write.cpp:362-388`。
  - Read source: `parse_track_grid()` 设置 direction/start/num/pitch，见 `src/database/manager/builder/def_builder/def_read.cpp:749-787`。

- Layer refs: `_layer_name_vec_sd`
  - Write source: `write_track_grid()` 输出 `LAYER` 后的 layer name list，见 `src/database/manager/builder/def_builder/def_write.cpp:362-388`。
  - Read source: `parse_track_grid()` 按 DEF layer list lookup LEF layer，并维护 routing layer track-grid back link，见 `src/database/manager/builder/def_builder/def_read.cpp:749-787`。

## Child Storage View

`IdbTrackGrid` 是 `TRACKS` root，当前子节点分两类：

- `_track_sd`：direct `IdbTrack` inline member，只保存 `_start/_direction/_pitch`；`IdbTrack` 是纯 DEF 标量对象，不需要 shadow。
- `_layer_name_vec_sd`：primitive string vector child，保存 DEF 中 layer name 列表和顺序。

不直接持久化 `vector<IdbLayer*> _layer_list`：`IdbLayer` 属于 LEF/layout，track grid 只应保存 layer name，read 时按 name 查找全局 `IdbLayer` 并重建 routing layer 反向引用。

## Why TrackGrid Shadow

当前需要 `Shadow<IdbTrackGrid>`：

- `IdbTrackGrid` 没有天然 name/ID，需要 `primary_key` 作为 root record 标识。
- `IdbTrackGridList` 的顺序不是对象身份，不能把 vector index 作为 PK；因此用 `primary_key` 做 root identity，`_order_sd` 单独保存 list order。
- `_layer_list` 是 `vector<IdbLayer*>`，属于对 LEF layer 的非 owning 引用；DB 中应保存 layer name，而不是持久化完整 layer 对象或裸指针。
- read 时必须用 layer name 回查当前 layout 的 LEF layer，并重建 routing layer 到 track grid 的反向引用。

如果未来 EDADB 支持稳定的 `IdbLayer*` name-reference 隐式转换，可以重新评估是否去掉 shadow；当前 direct mapping 不合适。

## EDADB Write Path

当前 `writeIdbTrackGrid()`：

- Code: `src/database/manager/builder/def_builder/def_write_edadb.cpp:237`
- Track-grid vector access: `src/database/manager/builder/def_builder/def_write_edadb.cpp:246`
- Shadow conversion: `src/database/manager/builder/def_builder/def_write_edadb.cpp:248`
- EDADB insert: `src/database/manager/builder/def_builder/def_write_edadb.cpp:256`
- `Shadow<IdbTrackGrid>::toShadow()`: `src/database/edadb/idb/shadow/shadow_idb_track_grid.h:20`

- 从 `layout->get_track_grid_list()` 取得 track grid list。
- 对每个 `IdbTrackGrid` 调用 `Shadow<IdbTrackGrid>::toShadow()`。
- `toShadow()` 保存 `_order_sd`、track number、`IdbTrack` 的 DEF 字段、layer name vector。
- 使用 `edadb::insertVector<Shadow<IdbTrackGrid>>()` 写入。

这与原始 writer 输出字段一致。

## EDADB Read Path

当前 `readIdbTrackGrid()`：

- Code: `src/database/manager/builder/def_builder/def_read_edadb.cpp:374`
- Reset active track grids: `src/database/manager/builder/def_builder/def_read_edadb.cpp:383`
- Ordered query: `src/database/manager/builder/def_builder/def_read_edadb.cpp:385`
- EDADB read loop: `src/database/manager/builder/def_builder/def_read_edadb.cpp:393`
- Shadow restore: `src/database/manager/builder/def_builder/def_read_edadb.cpp:405`
- Layer lookup / back link rebuild: `src/database/manager/builder/def_builder/def_read_edadb.cpp:407`
- `Shadow<IdbTrackGrid>::fromShadow()`: `src/database/edadb/idb/shadow/shadow_idb_track_grid.h:35`

- `track_grid_list->reset()` 清空旧 track grid。
- 通过 `ORDER BY "_order_sd"` 读取 root records，恢复 `IdbTrackGridList` 原始 append 顺序。
- 循环读取 `Shadow<IdbTrackGrid>`。
- `fromShadow()` 恢复 track number 和 track 基本字段。
- 对 `_layer_name_vec_sd` 中每个 layer name 回查 LEF layer。
- 当前在 `DefReadEdadb::readIdbTrackGrid()` 中使用局部 `layout->get_layers()->find_layer()`，贴近原始 `parse_track_grid()`。
- 如果后续把 layer lookup 下沉到 `Shadow<IdbTrackGrid>::fromShadow()`，应使用 `idb::edadb_adapter::EdadbIdbHelper::findIdbLayerByName()` 获取全局 `IdbLayer`。
- 使用 helper 前必须确保 `EdadbIdbHelper::setIdbDefService(_def_service)` 已设置；当前其他 shadow 如 `IdbLayerShape` / via master 已按这个模式使用。
- 找到 layer 时，加入 track grid，并同步挂到 routing layer 的 `_track_grid_list`。
- 找不到 layer 时按原始 parser 语义打印错误并继续。

## Computed Fields

这些字段不需要作为 TrackGrid DB 字段直接保存：

- routing layer 反向 `_track_grid_list`：读回时由 layer name 查找并重新挂接。
- 完整 `IdbLayer` 内容：由 LEF read 提供。
- `IdbTrack::_width`：原始 DEF `TRACKS` writer/read 不处理。

## Order / Index

`IdbTrackGridList` 在 iEDA 点工具语义上是 Level D，但当前 adapter 仍保存并恢复原始 append 顺序。

依据：

- 原始 `parse_track_grid()` 按 DEF 出现顺序 append track grid。
- DEF writer 会按 `track_grid_list` 当前顺序输出，因此严格 raw text roundtrip 需要稳定 root order。
- `def-ieda-mapping-and-order.md` 中记录：iFP/iRT 会遍历或重建 track grid，但未发现 root index/front/order-derived ID 依赖。
- 当前 shadow 用 `primary_key` 作为 root identity，用 `_order_sd` 保存 list order；禁止把 vector order index 当作 PK。
- read path 已显式按 `_order_sd` 恢复，不依赖 EDADB/SQLite read-all 物理顺序。
- `_layer_name_vec_sd` 是 layer name vector，必须保持 DEF 中 layer name 的原始顺序。

当前状态：已实现。root identity 和 root order 已分离，`primary_key` 不表达 vector order。

对 normalized diff 的影响：

- `TRACKS` 是 Level D root list；如果 raw diff 只因为不同 `TRACKS` root record 顺序失败，normalized diff 可以按 stable key 排序后通过。
- 排序单位必须是完整 `TRACKS` record；record 内部 layer-name list 不排序。
- 如果 direction/start/DO/STEP/layer-name list 内容不同，normalized diff 必须失败。

## Risks / TODO

- `Shadow<IdbTrackGrid>` 依赖 LEF 已先读入，否则 layer name 无法解析。
- 当前 layer lookup 留在 `readIdbTrackGrid()`，不是 `fromShadow()`；这样更接近原始 parser，也避免 shadow 隐式依赖全局 helper。
- 当前 missing-layer 行为已对齐原始 parser：打印并继续；如果后续希望 EDADB 对 DB/LEF mismatch 更严格，可以改成 adapter 层可配置策略。
