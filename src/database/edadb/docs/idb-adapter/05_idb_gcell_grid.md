# IdbGCellGrid EDADB Adapter Review

## Scope

`IdbGCellGrid` 对应 DEF 的 `GCELLGRID` statements。

- Write: `DefWrite::write_gcell_grid()`
- Read: `gcellGridCallback()` / `DefRead::parse_gcell_grid()`
- EDADB Write: `DefWriteEdadb::writeIdbGCellGrid()`
- EDADB Read: `DefReadEdadb::readIdbGCellGrid()`

本文件按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 的约束检查：

- DEF section 映射：`GCELLGRID` statements。
- iEDA root container：`IdbGCellGridList::_gcelll_grid_list`。
- root-vector order 等级：Level D，当前没有发现点工具依赖 `IdbGCellGridList::_gcelll_grid_list` 的 root index/order。
- nested vector 约束：`IdbGCellGrid` 当前没有持久化 nested vector。

## Original Write Semantics

原始 `DefWrite::write_gcell_grid()` 对每个 gcell grid 输出：

- direction: `gcell_grid->_direction`，`kDirectionX` 输出 `X`，否则输出 `Y`
- start: `gcell_grid->_start`
- DO count: `gcell_grid->_num`
- STEP space: `gcell_grid->_space`

如果列表为空，原始 writer 打印 `No GCELLGRID...` 并返回 `kDbFail`；但原始 top-level writer 没检查这个返回值，所以空列表不是整体 DEF 写失败条件。

## Original Read Semantics

原始 `DefRead::parse_gcell_grid()`：

- 在 `layout->get_gcell_grid_list()` 中创建 `IdbGCellGrid`。
- 由 `def_grid->macro()[0]` 设置 direction：`X` -> `kDirectionX`，否则 `kDirectionY`。
- 设置 num、start、space。
- 不依赖 LEF layer，不重建额外引用关系。

## EDADB Schema

当前 schema：

```cpp
TABLE4CLASS(idb::IdbGCellGrid, "iGCellGrid", (_direction, _start, _num, _space));
```

Schema / init 代码位置：

- `iGCellGrid` direct table macro: `src/database/edadb/idb/edadb_idb_schema.h:62`
- Primary-key setup: `src/database/edadb/idb/edadb_idb_init.cpp:21`
- Table registration: `src/database/edadb/idb/edadb_idb_init.cpp:84`

保存字段正好覆盖原始 DEF writer/read 需要的四个字段。

Schema 与 order/index 约束的关系：

- 依据 `src/database/edadb/docs/def-ieda-mapping-and-order.md`，`GCELLGRID` 映射到 `IdbGCellGridList::_gcelll_grid_list`，等级为 Level D。
- Level D 的含义是当前未发现点工具依赖 root vector index/order；normalized diff 可以按 stable key 排序 `GCELLGRID` root records。
- 当前 adapter 不保存 `_order_sd`；如果 DB 读回顺序不同，测试应通过 Level-D normalized diff 判断语义一致性。
- 当前 adapter 不定义 synthetic `primary_key`；`IdbGCellGrid` 没有 child rows 或 owner relationship 需要 root PK。

Primary-key audit:

- `initPrimKeys()` 显式关闭 `idb::IdbGCellGrid` 的 primary-key 行为；direct table 允许多条无 PK rows。
- `initReadDb()` / `initWriteDb()` 都先调用 `initPrimKeys()`，再调用 `initAllTables()`，因此 read/write 的 table metadata 一致。

## Original DEF Write/Read Roundtrip Mapping

### Original DEF Write Flow

| 原始 `DefWrite` 执行顺序 | EDADB write 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. `write_gcell_grid()` 检查 list；null 或 empty 都返回 `kDbFail`，见 `def_write.cpp:1013-1025` | `writeIdbGCellGrid()` 对 null 失败，但 empty vector 返回成功，见 `def_write_edadb.cpp:264-277` | `GCELLGRID` root/count / `IdbGCellGridList::_gcell_grid_list` / root rows，无 count/order 字段 |
| 2. 按 root vector 遍历，输出 direction、start、DO count、STEP space，见 `def_write.cpp:1029-1034` | direct `insertVector<IdbGCellGrid>()` 写入四个 scalar，见 `def_write_edadb.cpp:272-280` | `GCELLGRID <dir> <start> DO <num> STEP <space>` / `_direction/_start/_num/_space` / `iGCellGrid._direction/_start/_num/_space` |

### Original DEF Read Flow

| 原始 `DefRead` 执行顺序 | EDADB read 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. `gcellGridCallback()` 校验 input 后调 `parse_gcell_grid()`，见 `def_read.cpp:2034-2050` | `readIdbGCellGrid()` 清空 active list，然后逐行 direct read，见 `def_read_edadb.cpp:432-447` | `GCELLGRID` root / `IdbGCellGridList` / `iGCellGrid` |
| 2. parser append grid，按 macro 恢复 direction，再设 num/start/space，见 `def_read.cpp:2059-2070` | EDADB 已将四个 scalar 直接读入新 `IdbGCellGrid`，builder 只 append，见 `def_read_edadb.cpp:446-459` | direction/start/DO/STEP / `_direction/_start/_num/_space` / `iGCellGrid._direction/_start/_num/_space` |

## Child Storage View

`IdbGCellGrid` 是 `GCELLGRID` root，没有持久化子节点：

- direction/start/num/space 全部是 root scalar。
- 没有 LEF layer pointer、geometry vector 或 computed child 需要保存。

因此不需要 child storage view，也不需要 shadow。

## Why Direct Mapping

当前使用 direct `IdbGCellGrid` mapping：

- `IdbGCellGrid` 没有 vector child 或 owner-child rows。
- 没有 `IdbLayer*` / via rule / master 等非 owning 引用需要 name lookup。
- 当前只需要保存 DEF 四个标量字段，原始类成员正好匹配。
- Level D 下 root order 不是 iEDA 点工具语义必需，因此不额外保存 `_order_sd`。
- 没有 child rows 需要 owner key，因此不额外定义 `primary_key`。
- 旧的 dormant `shadow_idb_gcell_grid.h` 已删除，避免保留未启用 shadow 造成误导。

## EDADB Write Path

当前 `writeIdbGCellGrid()`：

- Code: `src/database/manager/builder/def_builder/def_write_edadb.cpp:264`
- GCell-grid vector access: `src/database/manager/builder/def_builder/def_write_edadb.cpp:272`
- Empty-list return: `src/database/manager/builder/def_builder/def_write_edadb.cpp:276`
- EDADB direct insert: `src/database/manager/builder/def_builder/def_write_edadb.cpp:280`

- 从 `layout->get_gcell_grid_list()` 取得 list。
- 空列表时直接返回 `kDbSuccess`，避免 `writeChip2Edadb()` 因原始 writer 的局部 `kDbFail` 语义中断整个 EDADB 写流程。
- 非空时直接 `edadb::insertVector<IdbGCellGrid>(gcell_grid_vec)` 写入。
- direct table 保存 direction、start、num、space。

这与原始输出字段一致；空列表返回值是 adapter 层必要调整，因为 EDADB write dispatcher 会检查每个 `writeIdbXXX()` 的返回值。

## EDADB Read Path

当前 `readIdbGCellGrid()`：

- Code: `src/database/manager/builder/def_builder/def_read_edadb.cpp:431`
- Clear active gcell grids: `src/database/manager/builder/def_builder/def_read_edadb.cpp:439`
- EDADB read op: `src/database/manager/builder/def_builder/def_read_edadb.cpp:441`
- EDADB read loop: `src/database/manager/builder/def_builder/def_read_edadb.cpp:444`
- Add to active list: `src/database/manager/builder/def_builder/def_read_edadb.cpp:457`

- `gcell_grid_list->clear()` 清空旧数据。
- 使用 `makeReadAllOp<IdbGCellGrid>()` 循环读取 direct rows。
- 直接加入 layout 的 gcell grid list。

这和原始 parser 的对象重建语义一致：没有额外 computed field 或外部引用要恢复。

## Computed Fields

`IdbGCellGrid` 当前没有 read 后计算字段：

- direction/start/num/space 全部来自 DEF/EDADB。
- 不需要查 LEF layer。
- 不需要反向引用重建。

## Order / Index

`IdbGCellGridList` 在 iEDA 点工具语义上是 Level D；当前 adapter 不保存 root order。

依据：

- 原始 parser 按 DEF 出现顺序 append。
- 原始 writer 按 list 当前顺序输出，因此 raw text roundtrip 可能受 DB 读回顺序影响。
- `def-ieda-mapping-and-order.md` 中记录：iRT 会遍历或重建 GCell grid，但未发现 root index/front/order-derived ID 依赖。
- 因为 root order 没有点工具语义依赖，当前不引入 `_order_sd`；如果 raw diff 只因 Level-D root order 变化失败，应使用 normalized diff。

当前状态：已实现 direct no-shadow/no-order mapping。

对 normalized diff 的影响：

- `GCELLGRID` 是 Level D root list；如果 raw diff 只因为不同 `GCELLGRID` root record 顺序失败，normalized diff 可以按 stable key 排序后通过。
- 排序单位必须是完整 `GCELLGRID` record；当前没有 nested vector 需要随 record 之外单独处理。
- 如果 direction/start/DO/STEP 内容不同，normalized diff 必须失败。

## Tests

- demo `sky130_gcd` 当前没有 `GCELLGRID`，覆盖空列表路径：`writeIdbGCellGrid insert gcell_grid_count=0`，`readIdbGCellGrid restored gcell_grid_count=0`。
- `src/database/edadb/test/run_idb_roundtrip_regression.sh` 中 routed case 覆盖非空路径，并检查 `iGCellGrid` count 和 direction/start/num/space 字段组合；不检查 `_order_sd`。
- 2026-07-03 验证 direct no-shadow/no-order 改动：
  - Build: `cmake --build build -j40 --target db_edadb def_builder iEDA` passed。
  - Demo: `cd bin && bash /home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/demo/demo.sh 2>&1 | tee run.out` passed，default DEF raw diff clean。
  - Regression: `OUT_DIR=/tmp/iedadb_gcell_direct_regression bash src/database/edadb/test/run_idb_roundtrip_regression.sh` passed。
  - Routed SQL: `iGCellGrid` count is `6`; sorted direct fields are `1:0:2:3600;1:3600:43:3360;1:144720:2:5240;2:0:2:3600;2:3600:43:3360;2:144720:2:5408`。
  - Normalizer unit: `bash src/database/edadb/test/test_normalize_def_for_diff.sh` passed。

## Risks / TODO

- demo 只覆盖空列表，正向字段持久化应以 regression 的 routed case 为准。
- 原始 writer 对空 gcell-grid list 返回失败，EDADB writer 对空 vector 返回成功。
- direct no-PK rows 允许重复字段组合；如果未来需要 update/delete 单条 `GCELLGRID` record，再重新评估是否需要 synthetic identity。
