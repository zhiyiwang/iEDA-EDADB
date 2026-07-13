# IdbTrackGrid EDADB Adapter Review

## Scope

`IdbTrackGrid` 对应 DEF 的 `TRACKS` statements。

- Write: `DefWrite::write_track_grid()`
- Read: `trackGridCallback()` / `DefRead::parse_track_grid()`
- EDADB Write: `DefWriteEdadb::writeIdbTrackGrid()`
- EDADB Read: `DefReadEdadb::readIdbTrackGrid()`

本文件按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 的约束检查：

- DEF section 映射：`TRACKS` statements。
- iEDA root container：`IdbTrackGridList::_track_grid_list`。
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
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbTrackGrid>, "iTrackGridSD", (primary_key, _track_num_sd, _track_sd), (_layer_name_vec_sd));
```

Schema / init 代码位置：

- `IdbTrack` table macro: `src/database/edadb/idb/edadb_idb_schema.h:46`
- `TABLE4SHADOW_WVEC(idb::IdbTrackGrid)`: `src/database/edadb/idb/edadb_idb_schema.h:49`
- `iTrackGridSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:50`
- Primary-key setup: `src/database/edadb/idb/edadb_idb_init.cpp:21`
- Table registration: `src/database/edadb/idb/edadb_idb_init.cpp:83`

保存字段覆盖原始 DEF writer 需要的内容：direction、start、pitch、track number、layer name vector。

Schema 与 order/index 约束的关系：

- 依据 `src/database/edadb/docs/def-ieda-mapping-and-order.md`，`TRACKS` 映射到 `IdbTrackGridList::_track_grid_list`，等级为 Level D。
- Level D 的含义是当前未发现点工具依赖 root vector index/order；normalized diff 可以按 stable key 排序 `TRACKS` root records。
- 当前 adapter 不保存 `_order_sd`；如果 DB 读回 root record 顺序不同，测试应通过 Level-D normalized diff 判断语义一致性。
- `primary_key` 是 root identity；它不表达 vector order。
- `_layer_name_vec_sd` 是 nested vector，必须保持该 `TRACKS` record 内部 layer name 顺序。

Primary-key audit:

- `initPrimKeys()` 显式关闭 `IdbTrack` 的 primary-key 行为；`IdbTrack` 是 `Shadow<IdbTrackGrid>` 内部 inline 标量对象。
- `initPrimKeys()` 没有关闭 `Shadow<IdbTrackGrid>` 的 primary-key 行为；`primary_key` 是 `iTrackGridSD` root identity。
- `initReadDb()` / `initWriteDb()` 都先调用 `initPrimKeys()`，再调用 `initAllTables()`，因此 read/write 的 table metadata 一致。

## Original DEF Write/Read Roundtrip Mapping

### Original DEF Write Flow

| 原始 `DefWrite` 执行顺序 | EDADB write / `toShadow` 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. `write_track_grid()` 按 root vector 遍历 anonymous `TRACKS` records，见 `def_write.cpp:362-371` | `writeIdbTrackGrid()` 按当前 vector 构造 shadow，但不存 root order；`primary_key` 只是 DB identity，见 `def_write_edadb.cpp:225-244`, `shadow_idb_track_grid.h:18-20` | `TRACKS` root / `IdbTrackGridList::_track_grid_list` / `iTrackGridSD.primary_key` |
| 2. 输出 direction、start、DO count、STEP pitch，见 `def_write.cpp:372-375` | `toShadow()` 保存 `_track_num_sd` 和 inline `_track_sd`，见 `shadow_idb_track_grid.h:20-25` | `TRACKS <dir> <start> DO <num> STEP <pitch>` / `IdbTrack::_direction/_start/_pitch`, `IdbTrackGrid::_track_num` / `_track_sd`, `_track_num_sd` |
| 3. 按 layer vector 顺序输出 layer names，见 `def_write.cpp:377-383` | 将 non-owning `IdbLayer*` 转为有序 `_layer_name_vec_sd`，见 `shadow_idb_track_grid.h:26-29` | `LAYER <names>` / `IdbTrackGrid::_layer_list` / `_layer_name_vec_sd` |

### Original DEF Read Flow

| 原始 `DefRead` 执行顺序 | EDADB read / `fromShadow` 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. `parse_track_grid()` append 新 grid 并获取 inline track，见 `def_read.cpp:749-760` | `readIdbTrackGrid()` reset list，无 `ORDER BY`地读 root row 并 append grid，见 `def_read_edadb.cpp:381-408` | `TRACKS` root / `IdbTrackGridList::_track_grid_list` / `iTrackGridSD` |
| 2. 从 DEF macro/x/xStep/xNum 恢复 direction、start、pitch、count，见 `def_read.cpp:762-769` | `fromShadow()` 恢复 `_track_sd/_track_num_sd`，见 `shadow_idb_track_grid.h:33-38` | axis fields / `IdbTrack`, `IdbTrackGrid::_track_num` / `_track_sd`, `_track_num_sd` |
| 3. 按 DEF layer list 查找 LEF layer，append reference；routing layer 增加 backlink，见 `def_read.cpp:771-784` | builder 按 `_layer_name_vec_sd` 执行同样 lookup、append 和 backlink 重建，见 `def_read_edadb.cpp:410-420` | `LAYER <names>` / `_layer_list`, `IdbLayerRouting::_track_grid_list` / `_layer_name_vec_sd` |

## Child Storage View

`IdbTrackGrid` 是 `TRACKS` root，当前子节点分两类：

- `_track_sd`：direct `IdbTrack` inline member，只保存 `_start/_direction/_pitch`；`IdbTrack` 是纯 DEF 标量对象，不需要 shadow。
- `_layer_name_vec_sd`：primitive string vector child，保存 DEF 中 layer name 列表和顺序。

不直接持久化 `vector<IdbLayer*> _layer_list`：`IdbLayer` 属于 LEF/layout，track grid 只应保存 layer name，read 时按 name 查找全局 `IdbLayer` 并重建 routing layer 反向引用。

## Why TrackGrid Shadow

当前需要 `Shadow<IdbTrackGrid>`：

- shadow 的必要性来自 `_layer_list` 的存储视图转换，而不是来自 root order。
- `IdbTrackGrid` 没有天然 name/ID，需要 `primary_key` 作为 root record 标识。
- `IdbTrackGridList` 的顺序不是对象身份，不能把 vector index 作为 PK；因此只用 `primary_key` 做 root identity。
- `_layer_list` 是 `vector<IdbLayer*>`，属于对 LEF layer 的非 owning 引用；DB 中应保存 layer name，而不是持久化完整 layer 对象或裸指针。
- read 时必须用 layer name 回查当前 layout 的 LEF layer，并重建 routing layer 到 track grid 的反向引用。

如果未来 EDADB 支持稳定的 `IdbLayer*` name-reference 隐式转换，可以重新评估是否去掉 shadow；当前 direct mapping 不合适。由于 `TRACKS` 是 Level D root list，当前不再保存 `_order_sd`。

## EDADB Write Path

当前 `writeIdbTrackGrid()`：

- Code: `src/database/manager/builder/def_builder/def_write_edadb.cpp:225`
- Track-grid vector access: `src/database/manager/builder/def_builder/def_write_edadb.cpp:234`
- Shadow conversion: `src/database/manager/builder/def_builder/def_write_edadb.cpp:236`
- EDADB insert: `src/database/manager/builder/def_builder/def_write_edadb.cpp:244`
- `Shadow<IdbTrackGrid>::toShadow()`: `src/database/edadb/idb/shadow/shadow_idb_track_grid.h:20`

- 从 `layout->get_track_grid_list()` 取得 track grid list。
- 对每个 `IdbTrackGrid` 调用 `Shadow<IdbTrackGrid>::toShadow()`。
- `toShadow()` 保存 track number、`IdbTrack` 的 DEF 字段、layer name vector。
- 使用 `edadb::insertVector<Shadow<IdbTrackGrid>>()` 写入。

这与原始 writer 输出字段一致。

## EDADB Read Path

当前 `readIdbTrackGrid()`：

- Code: `src/database/manager/builder/def_builder/def_read_edadb.cpp:381`
- Reset active track grids: `src/database/manager/builder/def_builder/def_read_edadb.cpp:390`
- EDADB read op: `src/database/manager/builder/def_builder/def_read_edadb.cpp:392`
- EDADB read loop: `src/database/manager/builder/def_builder/def_read_edadb.cpp:396`
- Shadow restore: `src/database/manager/builder/def_builder/def_read_edadb.cpp:408`
- Layer lookup / back link rebuild: `src/database/manager/builder/def_builder/def_read_edadb.cpp:410`
- `Shadow<IdbTrackGrid>::fromShadow()`: `src/database/edadb/idb/shadow/shadow_idb_track_grid.h:33`

- `track_grid_list->reset()` 清空旧 track grid。
- 使用 EDADB read-all 读取 root records，不指定 root order；Level D root order 只由 normalized diff 处理。
- 循环读取 `Shadow<IdbTrackGrid>`。
- `fromShadow()` 恢复 track number 和 track 基本字段。
- 对 `_layer_name_vec_sd` 中每个 layer name 回查 LEF layer。
- 当前在 `DefReadEdadb::readIdbTrackGrid()` 中使用局部 `layout->get_layers()->find_layer()`，贴近原始 `parse_track_grid()`。
- 如果后续把 layer lookup 下沉到 `Shadow<IdbTrackGrid>::fromShadow()`，应使用 `idb::edadb_adapter::EdadbIdbHelper::findIdbLayerByName()` 获取全局 `IdbLayer`。
- helper context 在 `DefReadEdadb::createDbFromEdadb()` 入口统一绑定；不要在单个 `readIdbXXX()` 中重复设置。
- 找到 layer 时，加入 track grid，并同步挂到 routing layer 的 `_track_grid_list`。
- 找不到 layer 时按原始 parser 语义打印错误并继续。

## Computed Fields

这些字段不需要作为 TrackGrid DB 字段直接保存：

- routing layer 反向 `_track_grid_list`：读回时由 layer name 查找并重新挂接。
- 完整 `IdbLayer` 内容：由 LEF read 提供。
- `IdbTrack::_width`：原始 DEF `TRACKS` writer/read 不处理。

## Order / Index

`IdbTrackGridList` 在 iEDA 点工具语义上是 Level D；当前 adapter 不保存 root order。

依据：

- 原始 `parse_track_grid()` 按 DEF 出现顺序 append track grid。
- DEF writer 会按 `track_grid_list` 当前顺序输出，因此 raw text roundtrip 可能受 DB 读回顺序影响。
- `def-ieda-mapping-and-order.md` 中记录：iFP/iRT 会遍历或重建 track grid，但未发现 root index/front/order-derived ID 依赖。
- 当前 shadow 用 `primary_key` 作为 root identity，不保存 `_order_sd`；禁止把 vector order index 当作 PK。
- 因为 root order 没有点工具语义依赖，当前不引入 `_order_sd`；如果 raw diff 只因 Level-D root order 变化失败，应使用 normalized diff。
- `_layer_name_vec_sd` 是 layer name vector，必须保持 DEF 中 layer name 的原始顺序。

当前状态：root identity 和 root order 已分离；Level D 不强制 ordered read，nested layer-name vector order 仍由 primitive vector `__edadb_vec_idx` 保证。

对 normalized diff 的影响：

- `TRACKS` 是 Level D root list；如果 raw diff 只因为不同 `TRACKS` root record 顺序失败，normalized diff 可以按 stable key 排序后通过。
- 排序单位必须是完整 `TRACKS` record；record 内部 layer-name list 不排序。
- 如果 direction/start/DO/STEP/layer-name list 内容不同，normalized diff 必须失败。

## Tests

- demo `sky130_gcd` 覆盖非空 track grid：`writeIdbTrackGrid insert track_grid_count=12`，`readIdbTrackGrid restored track_grid_count=12`。
- `src/database/edadb/test/run_idb_roundtrip_regression.sh` 检查 `iTrackGridSD` count、确认 root table 没有 `_order_sd` column、track direction/start/count/pitch，以及 primitive layer-name vector order。
- normalized diff 覆盖 Level D `TRACKS` root order-only differences；track record 内部 layer-name vector 不排序。

## Risks / TODO

- `Shadow<IdbTrackGrid>` 依赖 LEF 已先读入，否则 layer name 无法解析。
- 当前 layer lookup 留在 `readIdbTrackGrid()`，不是 `fromShadow()`；这样更接近原始 parser，也避免 shadow 隐式依赖全局 helper。
- 当前 missing-layer 行为已对齐原始 parser：打印并继续；如果后续希望 EDADB 对 DB/LEF mismatch 更严格，可以改成 adapter 层可配置策略。
