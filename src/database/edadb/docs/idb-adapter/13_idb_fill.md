# IdbFill EDADB Adapter Review

## Scope And Constraint Check

`IdbFill` 对应 DEF `FILLS`，root container 是 `IdbFillList::_fill_list`：`src/database/data/design/db_design/IdbFill.h:131-155`。

- 原始 write：`DefWrite::write_fill()`，`src/database/manager/builder/def_builder/def_write.cpp:1140-1188`。
- 原始 read：`DefRead::parse_fill()`，`src/database/manager/builder/def_builder/def_read.cpp:2352-2396`。
- EDADB write：`DefWriteEdadb::writeIdbFill()`，`src/database/manager/builder/def_builder/def_write_edadb.cpp:609-657`。
- EDADB read：`DefReadEdadb::readIdbFill()`，`src/database/manager/builder/def_builder/def_read_edadb.cpp:635-678`。

按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 检查：

- Root order 为 Level D，不保存 `_order_sd`。iDM、GDS、JSON 仅遍历全部 Fill，不使用 root index：`src/platform/data_manager/idm_transform.cpp:343-344`、`src/database/manager/builder/gds_builder/gds_write.cpp:824-833`、`src/database/manager/builder/json_builder/json_write.cpp:918-927`。
- 同一 layer/via 可以出现多个 Fill root；root 需要 synthetic `primary_key` 区分 owner，不能用 layer/via name 或 nested `_vec_idx` 作为 PK。
- `IdbFillLayer::_rect_list` 和 `IdbFillVia::_coordinate_list` 是 DEF record 内部顺序，必须按 `_vec_idx` 恢复。

## EDADB Schema

```cpp
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbFill>, "iFillSD",
                 (primary_key, _type_sd, _layer_name_sd, _via_name_sd),
                 (_rect_list_sd, _coordinate_list_sd));
```

- Fill schema：`src/database/edadb/idb/edadb_idb_schema.h:119-120`。
- Coordinate shadow/schema：`src/database/edadb/idb/edadb_idb_schema.h:32-34`。
- Rect shadow/schema：`src/database/edadb/idb/edadb_idb_schema.h:68-70`。
- Coordinate/Rect nested shadows 无独立 PK：`src/database/edadb/idb/edadb_idb_init.cpp:26-27`。
- Fill root table 注册：`src/database/edadb/idb/edadb_idb_init.cpp:90`。
- Fill shadow：`src/database/edadb/idb/shadow/shadow_idb_fill.h:15-133`。

| Storage field | DEF/iDB source | Purpose |
| --- | --- | --- |
| `primary_key` | EDADB-only | 匿名 root identity 和两个 child table 的 owner key |
| `_type_sd` | `IdbFill::_type` | 区分互斥的 `LAYER` / `VIA` 分支 |
| `_layer_name_sd` | `IdbFillLayer::_layer->get_name()` | read 时 lookup 全局 LEF layer |
| `_via_name_sd` | `IdbFillVia::_via->get_name()` | read 时按 DEF-via→LEF-via lookup 并 clone |
| `_rect_list_sd` | `IdbFillLayer::_rect_list` | Layer Fill 的有序 `RECT` records |
| `_coordinate_list_sd` | `IdbFillVia::_coordinate_list` | Via Fill 的有序 placement points |

不保存运行时 layer/via pointer、cloned via 内部结构和 `_num_fill`；pointer 由 name lookup 重建，count 由 root list 得到。原始 `parse_fill()` 未建立 OPC、MASK 或 polygon iDB 状态，因此 adapter 也不增加对应列。

## Why Shadow Is Required

- `IdbFill` 是 layer/via union，需要 `_type_sd` 和互斥字段形成稳定存储视图。
- Layer/Via 是 non-owning/name-reference rebuild，不能持久化进程内 pointer。
- Root 同时拥有两个可选 nested vectors，需要 synthetic owner PK 关联 child rows。
- `IdbFillLayer` / `IdbFillVia` 只是运行时分支容器；root shadow 已 flatten 其 DEF source fields，因此不需要额外 child shadow。
- `TABLE4CLASS_WVEC` 根据 Rect/Coordinate 的已注册 shadow 自动转换 nested vectors；child `_vec_idx` 只恢复 owner 内顺序。

## Original DEF Write Roundtrip Mapping

| Original `DefWrite` execution order | DEF field / iDB member | EDADB correspondence |
| --- | --- | --- |
| 获取 list 并检查空值，`def_write.cpp:1142-1152` | `IdbDesign::_fill_list` | `writeIdbFill()` 获取同一 list；空 list 直接成功，`def_write_edadb.cpp:610-628` |
| 输出 section count，`def_write.cpp:1154` | `FILLS <N>` / list size | 每个 valid root 写一行 `iFillSD`；count 由 table rows 得到 |
| 进入 Layer 分支并检查 layer，`def_write.cpp:1156-1162` | `LAYER <name>` / `IdbFill::_type`, `IdbFillLayer::_layer` | `toShadow()` 保存 `_type_sd`、`_layer_name_sd`，`shadow_idb_fill.h:30-36` |
| 顺序输出 Layer rectangles，`def_write.cpp:1164-1166` | repeated `RECT` / `IdbFillLayer::_rect_list` | 顺序复制 `_rect_list_sd`，`shadow_idb_fill.h:37-43` |
| 进入 Via 分支并检查 via，`def_write.cpp:1169-1174` | `VIA <name>` / `IdbFill::_type`, `IdbFillVia::_via` | `toShadow()` 保存 `_type_sd`、`_via_name_sd`，`shadow_idb_fill.h:44-48` |
| 顺序输出 Via points，`def_write.cpp:1176-1178` | repeated `( x y )` / `IdbFillVia::_coordinate_list` | 顺序复制 `_coordinate_list_sd`，`shadow_idb_fill.h:49-55` |
| 输出 section terminator，`def_write.cpp:1184` | `END FILLS` | 由 DEF writer 根据恢复后的 list 生成 |

`toShadow()` 每次转换前清理旧分支字段和 child vectors，避免同一个 shadow 被重复用于不同类型时残留数据：`shadow_idb_fill.h:19-20`、`shadow_idb_fill.h:30`、`shadow_idb_fill.h:108-123`。

## Original DEF Read Roundtrip Mapping

| Original `DefRead` execution order | DEF field / rebuilt member | EDADB correspondence |
| --- | --- | --- |
| section count callback，`def_read.cpp:2313-2332` | `FILLS <N>`；当前 parser 不写入状态 | EDADB 不保存 count，read 后由 `IdbFillList` 统计 |
| 获取 design/layout/layer/fill list，`def_read.cpp:2358-2361` | rebuild context | builder 提供 active design；layer/via lookup 由 helper 使用同一 service |
| `hasLayer()`，lookup layer 并 `add_fill_layer()`，`def_read.cpp:2363-2366` | `LAYER <name>` → type + global `IdbLayer*` | `fromShadow()` 校验 inactive Via fields 为空，设置 type，按 name lookup 并绑定 layer，`shadow_idb_fill.h:67-76` |
| rectangle loop，`def_read.cpp:2367-2369` | ordered `IdbFillLayer::_rect_list` | 按 child `_vec_idx` 读回并调用 `add_rect()`，`shadow_idb_fill.h:77-82` |
| polygon TODO，`def_read.cpp:2371-2372` | 无已实现 iDB state | 无 EDADB column |
| `hasVia()`，先 DEF via 后 LEF via lookup，`def_read.cpp:2376-2382` | `VIA <name>` → referenced `IdbVia*` | helper 保持 DEF→LEF lookup；`fromShadow()` 校验 inactive Layer fields 为空并按 name lookup，`shadow_idb_fill.h:83-90` |
| clone via 并 `add_fill_via()`，`def_read.cpp:2383-2385` | Fill-owned cloned via | `fromShadow()` clone lookup 结果并绑定到预建 `IdbFillVia`，`shadow_idb_fill.h:91-95` |
| flatten `numViaPts()` / point loops，`def_read.cpp:2386-2391` | ordered `IdbFillVia::_coordinate_list` | 按 child `_vec_idx` 调用 `add_coordinate()`，`shadow_idb_fill.h:96-101` |

DEF grammar 中 Layer/Via 是互斥 record；`fromShadow()` 使用同样的 `if/else if` 分支，并拒绝 active 分支携带另一分支的 name/child rows，防止损坏的 DB row 被静默接受：`shadow_idb_fill.h:68-70`、`shadow_idb_fill.h:83-85`。

## EDADB Write Read Path

- Write：获取 list `def_write_edadb.cpp:610-628` → 每个 root 调用标准 `toShadow()` `def_write_edadb.cpp:630-643` → batch insert `def_write_edadb.cpp:645-654`。
- Read 调用顺序：Via 已恢复后再读 Fill，`def_read_edadb.cpp:217-224`。
- Read：reset list `def_read_edadb.cpp:642-650` → cursor 读取 shadow `def_read_edadb.cpp:652-662` → standard `fromShadow()` `def_read_edadb.cpp:664-670` → 成功后 append root `def_read_edadb.cpp:671-672`。
- Root query 不使用 `ORDER BY`；Level D root 可改变顺序。Rect/Coordinate child rows 由 EDADB 根据 `_vec_idx` 恢复。

## Native Behavior And Adapter Boundary

- 原始 writer 在 zero Fill 时返回失败，但 `write_chip()` 忽略 `write_fill()` 返回值：`def_write.cpp:223`。EDADB empty list 返回成功，以保持完整 flow 可继续执行。
- 原始 writer 先输出 list count，再跳过 invalid root；这可能产生 count 与 records 不一致。Adapter 对 invalid branch/reference 返回失败，不写入不可重建状态。
- 原始 Via parser 在 null check 前调用 `via->clone()`：`def_read.cpp:2379-2385`。Adapter 先确认 lookup 成功再 clone，仅修复无效输入的 null dereference，不改变合法输入语义。

## Test Coverage

`aux_optional` 构造 4 个 Fill roots：两个同名 Layer fills 和两个同名 Via fills，`src/database/edadb/test/run_idb_roundtrip_regression.sh:607-612`。

- 检查 4 个 distinct synthetic root PK、重复 name 不合并、两种 branch 的 inactive fields/children 为空：`run_idb_roundtrip_regression.sh:194`、`run_idb_roundtrip_regression.sh:238-247`。
- 每种 branch 分别覆盖一个 child 和两个 children，检查 owner 隔离及 `_vec_idx` 顺序：`run_idb_roundtrip_regression.sh:248-259`。
- 测试主动反转 root table 和两个 child tables 的物理 row order：`run_idb_roundtrip_regression.sh:836-853`。最终 root 只要求 normalized DEF 等价，nested vectors 必须恢复原顺序。
- 定向命令：`OUT_DIR=/tmp/iedadb_fill_convergence EDADB_TEST_JOBS=1 bash src/database/edadb/test/run_idb_roundtrip_regression.sh aux_optional`。
