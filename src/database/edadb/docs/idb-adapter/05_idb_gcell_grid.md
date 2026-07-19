# IdbGCellGrid EDADB Adapter Review

## Scope And Constraints

`IdbGCellGrid` 对应 DEF `GCELLGRID` statement：

- Root container：`IdbLayout::_gcell_grid_list -> IdbGCellGridList::_gcelll_grid_list`
- DEF source：direction、start、`DO` count、`STEP` space
- Nested/computed state：无

本实现按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 检查：

- root order 等级为 Level D；未发现点工具依赖 root index/front/order-derived ID。
- 四个 scalar 与原始 class 成员完全对应。
- 没有 child、pointer reference、polymorphism 或 derived field，不引入 shadow。

## EDADB Schema

```cpp
TABLE4CLASS(idb::IdbGCellGrid, "iGCellGrid",
            (_direction, _start, _num, _space));
```

代码位置：

- Schema：`src/database/edadb/idb/edadb_idb_schema.h:61-62`
- PK disabled：`src/database/edadb/idb/edadb_idb_init.cpp:21-25`
- Root registration：`src/database/edadb/idb/edadb_idb_init.cpp:69-80`

Primary-key / order 结论：

- direct rows 没有 child owner relationship，也没有 update/delete identity 需求。
- `initPrimKeys()` 关闭 `IdbGCellGrid` PK；不会误把 `_direction` 当 PK。
- Level D 不增加 `_order_sd`，不引入 synthetic PK。
- 重复 scalar 组合在当前 append/read-all roundtrip 模型中合法。

## Why Direct Mapping

Direct mapping 已能表达完整 DEF storage view：

- `_direction/_start/_num/_space` 全部来自 DEF，setter 只是直接赋值。
- 没有 non-owning reference 需要 name conversion。
- 没有 nested vector 需要 owner identity 或 index。
- 没有 parser 计算出的 cache/geometry 需要排除。

增加 shadow 或 PK 只会引入没有语义用途的字段，因此不采用。

## Original DEF Write Mapping

| Original writer brace | DEF output | EDADB correspondence | Stored source |
| --- | --- | --- | --- |
| 获取 list，null 时失败，`def_write.cpp:1013-1020` | `GCELLGRID` collection | `writeIdbGCellGrid()` 做同一 null 检查，`def_write_edadb.cpp:277-285` | `iGCellGrid` rows |
| empty 时局部返回失败，`def_write.cpp:1022-1025` | 不输出 statement | adapter 返回成功，`def_write_edadb.cpp:286-291`，因为原始 `writeChip()` 忽略该局部返回值，而 EDADB dispatcher 会检查返回值 | zero rows |
| 按 list 遍历并输出四个 scalar，`def_write.cpp:1029-1034` | `GCELLGRID dir start DO num STEP space` | direct `insertVector<IdbGCellGrid>()`，`def_write_edadb.cpp:293-296` | `_direction/_start/_num/_space` |

空列表差异只发生在局部 return code；整体 DEF/EDADB 写流程语义相同：都允许没有
`GCELLGRID` statement 的设计继续完成。

## Original DEF Read Mapping

| Original parser brace | EDADB correspondence | Source / synchronization / calculation |
| --- | --- | --- |
| callback 校验并调用 parser，`def_read.cpp:2034-2050` | `readIdbGCellGrid()` 创建 direct read op，`def_read_edadb.cpp:406-421` | root query；无 `ORDER BY` |
| append 新 grid，`def_read.cpp:2059-2061` | EDADB 分配并填充对象，成功后 append，`def_read_edadb.cpp:420-433` | ownership handoff |
| `X` 映射为 `kDirectionX`，其它映射为 `kDirectionY`，`def_read.cpp:2062-2066` | DB 保存 writer 产生的 enum，direct read 恢复同一 `_direction` | `_direction` |
| 设置 num/start/space，`def_read.cpp:2068-2070` | direct read 恢复相同三个 scalar | `_num/_start/_space` |

原始 setters 没有额外同步或计算，因此 direct member restore 与 parser 结果一致。

## Root Order

GCellGrid root order 是 Level D：

- iRT 是唯一 `src/operation` root consumer；它先 clear list，再按 X/Y axis 数据重建，
  不读取原 list index：`src/operation/iRT/interface/RTInterface.cpp:1316-1338`。
- 未发现点工具通过 root `[index]`、`front()` 或 order-derived ID 消费该 list。

因此 read-all 不指定 root order；不同物理 DB 顺序允许产生不同 statement 顺序，最终由
Level-D normalized diff 按 direction/start/count/space stable key 比较。

## Validation

回归位置：`src/database/edadb/test/run_idb_roundtrip_regression.sh`。

- default sky130 fixture 覆盖 empty path：`iGCellGrid` count 为 `0`。
- routed/grid fixture 覆盖六条非空记录及全部四个 scalar：`run_idb_roundtrip_regression.sh:243-245`。
- `grid_branches` 逆置 `iGCellGrid` 物理 row order，检查逆序 fetch 和 sorted semantic fields，并确认无 PK：`run_idb_roundtrip_regression.sh:281-286`。
- normalizer 允许 `GCELLGRID` root reorder：`test_normalize_def_for_diff.sh:77-110`、`test_normalize_def_for_diff.sh:203-206`。
- statement stable key 实现：`normalize_def_for_diff.py:55-60`。

验证命令：

- `cmake --build build -j40 --target db_edadb def_builder iEDA`
- `bash src/database/edadb/test/test_normalize_def_for_diff.sh`
- `OUT_DIR=/tmp/iedadb_grid_convergence bash src/database/edadb/test/run_idb_roundtrip_regression.sh`

验证结果：目标编译、normalizer 单测和完整 regression 全部通过；`grid_branches`
将六条 `iGCellGrid` 物理逆序后，direct 与 EDADB DEF 通过 Level-D normalized diff，
四个 scalar 的 sorted semantic set 完全一致。

## Conclusion

GCellGrid adapter 保持最小 direct mapping：只保存原始 DEF 的四个 scalar，无 shadow、无 PK、
无 root order 字段；empty/non-empty path 与原始 top-level writer 语义一致，Level-D root reorder
由 normalized diff 验证。
