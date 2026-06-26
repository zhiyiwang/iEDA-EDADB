# IdbTrackGrid EDADB Adapter Review

## Scope

`IdbTrackGrid` 对应 DEF 的 `TRACKS` statements。

- Write: `DefWrite::write_track_grid()`
- Read: `trackGridCallback()` / `DefRead::parse_track_grid()`
- EDADB Write: `DefWriteEdadb::writeIdbTrackGrid()`
- EDADB Read: `DefReadEdadb::readIdbTrackGrid()`

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

保存字段覆盖原始 DEF writer 需要的内容：direction、start、pitch、track number、layer name vector。

## Why TrackGrid Shadow

当前需要 `Shadow<IdbTrackGrid>`：

- `IdbTrackGrid` 没有天然 name/ID，需要 `primary_key` 作为 root record 标识。
- `IdbTrackGridList` 的顺序不是对象身份，不能把 vector index 作为 PK；因此用 `primary_key` 做 root identity，`_order_sd` 单独保存 list order。
- `_layer_list` 是 `vector<IdbLayer*>`，属于对 LEF layer 的非 owning 引用；DB 中应保存 layer name，而不是持久化完整 layer 对象或裸指针。
- read 时必须用 layer name 回查当前 layout 的 LEF layer，并重建 routing layer 到 track grid 的反向引用。

如果未来 EDADB 支持稳定的 `IdbLayer*` name-reference 隐式转换，可以重新评估是否去掉 shadow；当前 direct mapping 不合适。

## EDADB Write Path

当前 `writeIdbTrackGrid()`：

- 从 `layout->get_track_grid_list()` 取得 track grid list。
- 对每个 `IdbTrackGrid` 调用 `Shadow<IdbTrackGrid>::toShadow()`。
- `toShadow()` 保存 `_order_sd`、track number、`IdbTrack` 的 DEF 字段、layer name vector。
- 使用 `edadb::insertVector<Shadow<IdbTrackGrid>>()` 写入。

这与原始 writer 输出字段一致。

## EDADB Read Path

当前 `readIdbTrackGrid()`：

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

`IdbTrackGridList` 需要保持原始 append 顺序，且不应该按方向、layer 或 start 排序。

依据：

- 原始 `parse_track_grid()` 按 DEF 出现顺序 append track grid。
- DEF writer 会按 `track_grid_list` 当前顺序输出，因此严格文本 roundtrip 需要稳定 root order。
- iEDA/iRT 主要通过 vector traversal 和 layer back links 使用 track grid，没有 name lookup。
- 当前 shadow 用 `primary_key` 作为 root identity，用 `_order_sd` 保存 list order；禁止把 vector order index 当作 PK。
- read path 已显式按 `_order_sd` 恢复，不依赖 EDADB/SQLite read-all 物理顺序。
- `_layer_name_vec_sd` 是 layer name vector，必须保持 DEF 中 layer name 的原始顺序。

当前状态：已实现。root identity 和 root order 已分离，`primary_key` 不表达 vector order。

## Risks / TODO

- `Shadow<IdbTrackGrid>` 依赖 LEF 已先读入，否则 layer name 无法解析。
- 当前 layer lookup 留在 `readIdbTrackGrid()`，不是 `fromShadow()`；这样更接近原始 parser，也避免 shadow 隐式依赖全局 helper。
- 当前 missing-layer 行为已对齐原始 parser：打印并继续；如果后续希望 EDADB 对 DB/LEF mismatch 更严格，可以改成 adapter 层可配置策略。
