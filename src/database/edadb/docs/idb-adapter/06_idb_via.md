# IdbVia EDADB Adapter Review

## Scope And Constraints

`IdbVia` 对应 DEF `VIAS`，root container 是 `IdbVias::_via_list`。

- 原始 write：`src/database/manager/builder/def_builder/def_write.cpp:390`
- 原始 read：`src/database/manager/builder/def_builder/def_read.cpp:1772`、`src/database/manager/builder/def_builder/def_read.cpp:1800`
- EDADB write/read：`src/database/manager/builder/def_builder/def_write_edadb.cpp:301`、`src/database/manager/builder/def_builder/def_read_edadb.cpp:441`
- Order level：按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 为 Level D。点工具按 via name 查找，没有 root index 语义证据。
- Root identity：`IdbVia::_name`；不增加 root shadow 或 `_order_sd`。
- Nested order：fixed via 的 layer-shape vector 和每个 shape 的 rect vector 属于 DEF 输入，必须分别保留 vector index。

## Original DEF Write/Read Roundtrip Mapping

### Original DEF Write Flow

| 原始 `DefWrite` 顺序 | EDADB 对应 | DEF 域 / iDB 成员 |
| --- | --- | --- |
| 校验 list/count，输出 `VIAS <N>`，见 `def_write.cpp:392-406` | `writeIdbVia()` 获取同一 vector 并 `insertVector<IdbVia>()`，见 `def_write_edadb.cpp:302-325` | section count / `IdbVias::_via_list` |
| 遍历 root via，只进入 `is_generate()` 分支，见 `def_write.cpp:406-410` | `IdbVia` direct store；`_master_instance` 隐式转为 `Shadow<IdbViaMaster>` | via name/type / `IdbVia::_name`、`IdbViaMaster::_type` |
| 输出 `VIARULE/CUTSIZE/LAYERS/CUTSPACING/ENCLOSURE/ROWCOL`，见 `def_write.cpp:412-420` | `Shadow<IdbViaMasterGenerate>::toShadow()` 保存 scalar 与 rule/layer names，见 `shadow_idb_via_master.h:28-54` | generated-via source fields |
| 可选输出 `PATTERN`，见 `def_write.cpp:422-424` | 保存 `_pattern_name_sd` | `PATTERN` / `IdbViaMasterGenerate::_patttern` |
| 原始 writer 没有 fixed `RECT` 分支，见 `def_write.cpp:409-428` | EDADB 仍保存 parser 已读入的 fixed layer/rect source state | fixed-via parser state |

### Original DEF Read Flow

| 原始 `DefRead` 顺序 | EDADB 对应 | DEF 域 / iDB 成员 |
| --- | --- | --- |
| `parse_via_num()` reserve vector，见 `def_read.cpp:1772-1779` | 不保存 section count；EDADB cursor 逐条读取并 append，见 `def_read_edadb.cpp:454-494` | `VIAS <N>` / capacity only |
| 按 name 创建 `IdbVia/IdbViaMaster`，见 `def_read.cpp:1806-1813` | direct `IdbVia` read 自动创建并恢复 `_master_instance` | via identity / `_name` |
| `hasViaRule()`：恢复 rule、layer refs、cut/spacing/enclosure，见 `def_read.cpp:1815-1841` | generated shadow 按 name 经 helper 查找 LEF rule/layers，见 `shadow_idb_via_master.h:63-103` | generated-via source fields |
| 可选恢复 `ORIGIN/OFFSET`，见 `def_read.cpp:1843-1855` | 保存并恢复 original/offset scalar fields | `ORIGIN/OFFSET` |
| 恢复默认或显式 `ROWCOL`、可选 `PATTERN`，见 `def_read.cpp:1862-1873` | 恢复 rows/cols/pattern | `ROWCOL/PATTERN` |
| 计算 cut rect、cut bbox 和公共 via shapes，见 `def_read.cpp:1878-1900` | `fromShadow()` 用相同公式重建 cut rect/bbox，parent 再调 `set_via_shape()`，见 `shadow_idb_via_master.h:108-129`、`shadow_idb_via_master.h:194-199` | derived geometry，不入库 |
| fixed branch 按 DEF `RECT` 创建 fixed master/layer/rect，见 `def_read.cpp:1901-1919` | layer-shape/rect children 读回后，master shadow 调 `add_fixed()/add_rect()`，见 `shadow_idb_via_master.h:205-237` | fixed layer/rect source fields |
| fixed branch 计算 cut bbox 与公共 via shapes，见 `def_read.cpp:1921-1931` | 按 cut-layer rect 计算同一结果并调用 `set_via_shape()` | derived geometry，不入库 |

## Schema And Shadow Audit

```cpp
TABLE4CLASS(idb::IdbVia, "iVia", (_name, _master_instance));
TABLE4SHADOW_WVEC(idb::IdbViaMaster);
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbViaMaster>, "iViaMasterSD", ...);
TABLE4CLASS(edadb::Shadow<idb::IdbViaMasterGenerate>, "iViaMasterGenerateSD", ...);
TABLE4SHADOW_WVEC(idb::IdbLayerShape);
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbLayerShape>, "iLayerShapeSD", ...);
TABLE4SHADOW(idb::IdbRect);
TABLE4CLASS(edadb::Shadow<idb::IdbRect>, "IdbRectSD", ...);
```

- Schema：`src/database/edadb/idb/edadb_idb_schema.h:65-81`
- PK setup：`src/database/edadb/idb/edadb_idb_init.cpp:21-30`
- Root registration：`src/database/edadb/idb/edadb_idb_init.cpp:81`

Shadow 必要性：

- `IdbVia` 不需要 root shadow：name 可直接作 identity，EDADB 会对 `_master_instance` 自动使用已注册的 `Shadow<IdbViaMaster>`。
- `IdbViaMaster` 需要 shadow：区分 generated/fixed union-like 分支，并把 fixed master 简化为实际 DEF storage view `vector<IdbLayerShape*>`。
- `IdbViaMasterGenerate` 需要 shadow：rule/layer 是 non-owning pointer，DB 保存 name；cut rect/bbox/公共 layer shapes 是派生数据，不保存。
- `IdbLayerShape` 需要 shadow：layer pointer 保存为 name，同时用独立 `primary_key` 区分同 owner 下同名 layer shape，用 `_vec_idx` 保序。
- `IdbRect` 需要 shadow：`_vec_idx` 只表达 nested vector order，不作为 PK。

Primary-key audit：

- `iVia._name` 是 root PK。
- `Shadow<IdbViaMasterGenerate>` 是 inline value，无独立 identity，`hasPrimKey=false`。
- `Shadow<IdbViaMaster>` 的内部 fixed vector 需要 owner chain；不为 `IdbVia` 增加额外 root key。
- `Shadow<IdbLayerShape>::primary_key` 是同一 owner 下 child identity；`_vec_idx` 仅排序。
- `Shadow<IdbRect>` 无独立 identity，`hasPrimKey=false`；禁止把 `_vec_idx` 当 PK。

## Generated And Fixed Geometry

- Generated via 的 source of truth 是 rule/layer names、cut size/spacing、enclosure、row/col、origin/offset、pattern。
- `IdbViaMasterGenerate::_cut_rect_list`、cut bbox、`IdbViaMaster::_layer_shape_*` 均由上述字段计算，不入库。
- Fixed via 的 source of truth 是 `IdbViaMaster::_master_fixed_list -> IdbLayerShape::_rect_list`；这些 rect 不是 generated cut rect。
- 两条分支只在 `set_via_shape()` 后形成统一的 bottom/cut/top layer-shape 消费接口。

## EDADB Paths

- Write：`writeIdbVia()` 直接 batch 写 root vector；nested direct-to-shadow 转换由 EDADB schema 递归完成，见 `def_write_edadb.cpp:301-328`。
- Read：`readIdbVia()` 只负责 cursor、错误处理和 `IdbVias::add_via()`；`469-491` 的代码仅在 debug 模式输出 fixed geometry 验证签名，nested rebuild 位于标准 `fromShadow()`，见 `def_read_edadb.cpp:441-499`。
- Generated shadow：`src/database/edadb/idb/shadow/shadow_idb_via_master.h:22-158`。
- Master/fixed shadow：`src/database/edadb/idb/shadow/shadow_idb_via_master.h:161-266`。
- Layer/rect shadow：`src/database/edadb/idb/shadow/shadow_idb_layer_shape.h:18-114`、`src/database/edadb/idb/shadow/shadow_idb_geometry.h:45-80`。

## Tests

`src/database/edadb/test/run_idb_roundtrip_regression.sh` 覆盖：

- sky130 四个 generated vias 的 count、name、rule、layer、cut size、row/col。
- `via_branches` fixture：generated `ORIGIN/OFFSET` 和 fixed via 三层 rect。
- fixed layer-shape/rect child table、nested `_vec_idx` 与重建路径。
- 逆序重建 `iVia` 物理行；Level-D normalized diff 允许 root 顺序变化，nested block 不归一化。

## Known Native Writer Differences

- `write_via()` section count 使用全部 vias，但只输出 generated records；存在 fixed via 时 count 与记录数不一致。
- `write_via()` 不输出 parser 支持的 fixed `RECT`、generated `ORIGIN/OFFSET`；这些字段只能通过 DB SQL 和 read-path 成功验证，不能依赖最终 DEF 文本覆盖。
- EDADB 保存的是 `parse_via()` 已构建的完整 source state；最终 DEF 输出仍严格跟随当前原始 writer 能力。
