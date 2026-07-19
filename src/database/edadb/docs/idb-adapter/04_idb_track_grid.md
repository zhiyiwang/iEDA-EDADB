# IdbTrackGrid EDADB Adapter Review

## Scope And Constraints

`IdbTrackGrid` 对应 DEF `TRACKS` statement：

- Root container：`IdbLayout::_track_grid_list -> IdbTrackGridList::_track_grid_list`
- DEF source：direction、start、`DO` count、`STEP` pitch、ordered layer-name list
- Rebuilt reference：`IdbLayer*` 以及 routing layer 的 track-grid backlink

本实现按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 检查：

- root order 等级为 Level D；点工具不使用 root index/front/order-derived ID。
- `_layer_list` 是 deeper nested vector，statement 内 layer-name 顺序必须保留。
- `IdbTrack::_width` 未被原始 DEF writer/parser处理，不进入 EDADB。

## EDADB Schema

```cpp
TABLE4CLASS(idb::IdbTrack, "iTrack", (_start, _direction, _pitch));

TABLE4SHADOW_WVEC(idb::IdbTrackGrid);
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbTrackGrid>, "iTrackGridSD",
                 (primary_key, _track_num_sd, _track_sd),
                 (_layer_name_vec_sd));
```

代码位置：

- Schema：`src/database/edadb/idb/edadb_idb_schema.h:53-58`
- PK setup：`src/database/edadb/idb/edadb_idb_init.cpp:21-25`
- Root registration：`src/database/edadb/idb/edadb_idb_init.cpp:69-80`
- Shadow：`src/database/edadb/idb/shadow/shadow_idb_track_grid.h:16-83`

Primary-key / order 结论：

- `IdbTrackGrid` 是 anonymous root，且 layer-name vector child 需要 owner identity，因此保留 synthetic `primary_key`。
- `primary_key` 只链接 root/child，不表达 DEF root order。
- Level D 不增加 `_order_sd`，也不把 vector index 当 PK。
- `IdbTrack` 是 inline scalar view，关闭其独立 PK。
- primitive layer-name child 使用 `(owner primary_key, __edadb_vec_idx)` 定位元素。

## Why Shadow Is Required

当前不能使用 direct root mapping：

- 原始 `_layer_list` 是 non-owning `vector<IdbLayer*>`，不能把 LEF layer 对象或 pointer 存入 DEF DB。
- DB 应保存 layer name，read 时在当前 LEF layout 中重新 lookup。
- layer-name child table 需要 anonymous TrackGrid root 的 owner key。

因此 shadow 只提供 `primary_key`、DEF scalar view 和 layer-name vector；它不是为 root 排序而定义。

## Original DEF Write Mapping

| Original writer brace | DEF output | EDADB correspondence | Stored source |
| --- | --- | --- | --- |
| 获取 `layout->get_track_grid_list()` 并检查，`def_write.cpp:362-369` | `TRACKS` collection | `writeIdbTrackGrid()` 取得同一 list，`def_write_edadb.cpp:247-256` | `iTrackGridSD` roots |
| 按 root vector 遍历，`def_write.cpp:371` | statement sequence | Level D 不存 root order；每个 shadow 生成 owner `primary_key` | identity only |
| 输出 direction/start/DO/STEP，`def_write.cpp:372-375` | `TRACKS dir start DO num STEP pitch` | `toShadow()` 只复制 track number、direction、start、pitch，`shadow_idb_track_grid.h:26-29` | `_track_num_sd/_track_sd` |
| 按 `_layer_list` 顺序输出 layer names，`def_write.cpp:377-383` | `LAYER name...` | `toShadow()` 按当前 vector 顺序保存 name，`shadow_idb_track_grid.h:30-36` | `_layer_name_vec_sd` |

`writeIdbTrackGrid()` 检查每次标准 `toShadow()` 并传播失败，再 batch insert：
`src/database/manager/builder/def_builder/def_write_edadb.cpp:255-271`。

## Original DEF Read Mapping

| Original parser brace | EDADB correspondence | Source / synchronization / calculation |
| --- | --- | --- |
| callback 校验并调用 parser，`def_read.cpp:731-746` | `readIdbTrackGrid()` 读取 shadow roots，`def_read_edadb.cpp:366-388` | root query；无 `ORDER BY` |
| append grid，取得 inline track，`def_read.cpp:757-760` | builder new grid；仅在标准 `fromShadow()` 成功后 append，`def_read_edadb.cpp:390-398` | ownership handoff |
| 设置 direction/start/pitch/count，`def_read.cpp:762-769` | `fromShadow()` 调用相同类别 setters，`shadow_idb_track_grid.h:50-53` | DB source scalars |
| 按 layer name lookup 并 append，`def_read.cpp:771-776` | `fromShadow()` 通过全局 helper 的 active layout 查找 layer，`shadow_idb_track_grid.h:45-62` | DB name reference → LEF pointer |
| routing layer 增加 backlink，`def_read.cpp:777-780` | `fromShadow()` 执行同一 `add_track_grid()`，`shadow_idb_track_grid.h:63-69` | derived backlink，不存储 |
| layer 不存在时记录错误并继续，`def_read.cpp:781-783` | `fromShadow()` 保持同一行为，`shadow_idb_track_grid.h:56-60` | parser-compatible mismatch policy |

Builder 不再访问 shadow fields 或重建 layer；只负责 query、对象分配、失败传播和 append。

## Child Storage And Order

- `_track_sd` 是 inline `IdbTrack`，只映射 DEF 使用的 `_start/_direction/_pitch`。
- `_layer_name_vec_sd` 是 primitive string vector child；不存储完整 `IdbLayer`。
- EDADB insert 自动写 `__edadb_vec_idx`。
- EDADB read 使用 `__edadb_vec_idx` 放回目标 vector slot，不依赖 SQLite fetch order，见
  `src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h:545-574`。
- read 后按 name 重建 `_layer_list`，并同步 routing-layer backlink。

## Root Order

TrackGrid root order 是 Level D：

- iFP 只按调用顺序新建 X/Y grids，没有读取既有 root index：`src/operation/iFP/source/module/init_design/init_design.cpp:114-143`。
- iRT 先 reset，再由 routing-layer/axis 数据重建 grids：`src/operation/iRT/interface/RTInterface.cpp:1271-1313`。
- 未发现点工具通过 root `[index]`、`front()` 或 order-derived ID 消费该 list。

因此 read 使用无 `ORDER BY` 的 root query；root 顺序差异由 Level-D normalized diff 处理。
排序单位是完整 `TRACKS` statement，statement 内 layer list 不排序。

## Validation

回归位置：`src/database/edadb/test/run_idb_roundtrip_regression.sh`。

- `grid_branches` fixture 把一个 `TRACKS` 扩展为 `LAYER met5 met4`：`run_idb_roundtrip_regression.sh:365-380`。
- 测试逆置 TrackGrid root 物理顺序和 layer child 物理顺序：`run_idb_roundtrip_regression.sh:607-623`。
- SQL 检查无 `_order_sd`、root 逆序、child 物理 `1:met4,0:met5`，以及逻辑顺序 `0:met5,1:met4`：`run_idb_roundtrip_regression.sh:265-280`。
- normalizer 明确允许 `TRACKS` root reorder，但拒绝 nested layer reorder：`test_normalize_def_for_diff.sh:77-110`、`test_normalize_def_for_diff.sh:203-206`。

验证命令：

- `cmake --build build -j40 --target db_edadb def_builder iEDA`
- `bash src/database/edadb/test/test_normalize_def_for_diff.sh`
- `OUT_DIR=/tmp/iedadb_grid_convergence bash src/database/edadb/test/run_idb_roundtrip_regression.sh`

验证结果：目标编译、normalizer 单测和完整 regression 全部通过；`grid_branches`
在 root/child 物理逆序后由 normalized diff 判定语义一致，nested layer names 仍恢复为
`met5, met4`。

## Conclusion

TrackGrid adapter 保存最小 DEF view：synthetic PK 只服务 anonymous root/child ownership，
root 不保序；direction/start/count/pitch 直接恢复，ordered layer names 通过 primitive vector index
恢复，再由标准 `fromShadow()` 重建 LEF layer references 和 routing backlinks。
