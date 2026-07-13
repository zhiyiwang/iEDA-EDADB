# IdbVia EDADB Adapter Review

## Scope

`IdbVia` 对应 DEF 的 `VIAS` section。

- Write: `DefWrite::write_via()`
- Read: `viaBeginCallback()` / `viaCallback()` / `DefRead::parse_via_num()` / `DefRead::parse_via()`
- EDADB Write: `DefWriteEdadb::writeIdbVia()`
- EDADB Read: `DefReadEdadb::readIdbVia()`

本文件按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 的约束检查：

- DEF section 映射：`VIAS` section。
- iEDA root container：`IdbVias::_via_list`。
- root-vector order 等级：Level D，当前点工具主要按 via name 查找 `IdbVias::_via_list`，没有发现依赖 root index/order。
- root identity 约束：via name 是 DEF-visible identity，当前 direct `IdbVia::_name` 是 EDADB root PK；禁止用 vector order index 作为 PK。
- nested vector 约束：fixed via 的 layer-shape vector 和 rect vector 是 via 内部几何语义，必须随 root via 保持原始顺序，不参与 D-level root sort。

## Original Write Semantics

原始 `DefWrite::write_via()` 输出：

- via count: `design->get_via_list()->get_num_via()`
- via name: `IdbVia::_name`
- generated via fields: via rule, cut size, bottom/cut/top layer names, cut spacing, enclosure, row/col
- optional pattern: `IdbViaMasterGenerate::_patttern`

当前原始 writer 只输出 `via_master->is_generate()` 的 via；fixed via parse 逻辑存在，但 writer 没有对应输出分支。

## Original Read Semantics

原始 `DefRead::parse_via()`：

- `parse_via_num()` 只 reserve `IdbVias` vector。
- `add_via(def_via->name())` 按 DEF 出现顺序加入 design via list。
- generated via:
  - 通过 via rule name 从 LEF `IdbViaRuleList` 查找 rule。
  - 通过 layer name 从 LEF `IdbLayers` 查找 bottom/cut/top layer。
  - 保存 cut size、spacing、enclosure、origin、offset、row/col、pattern。
  - 根据 row/col、spacing、origin、pattern 计算 cut rect list 和 cut bounding box。
  - 调用 `set_via_shape()` 计算 bottom/cut/top layer shapes。
- fixed via:
  - 从 DEF layer rect 重建 `IdbViaMasterFixed`。
  - 根据 cut layer rect 计算 cut rect，再调用 `set_via_shape()`。

## EDADB Schema

当前 schema：

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

Schema / init 代码位置：

- `iViaMasterGenerateSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:58`
- `IdbRectSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:62`
- `iLayerShapeSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:66`
- `iViaMasterSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:70`
- `iVia` root table macro: `src/database/edadb/idb/edadb_idb_schema.h:73`
- Primary-key setup: `src/database/edadb/idb/edadb_idb_init.cpp:21`
- `Shadow<IdbViaMasterGenerate>` PK disabled: `src/database/edadb/idb/edadb_idb_init.cpp:31`
- `Shadow<IdbViaMaster>` PK uses EDADB default `true`; no explicit `initPrimKeys()` override.
- Table registration: `src/database/edadb/idb/edadb_idb_init.cpp:85`
- Via master shadow definition: `src/database/edadb/idb/shadow/shadow_idb_via_master.h:22`
- Layer shape shadow definition: `src/database/edadb/idb/shadow/shadow_idb_layer_shape.h:18`
- Rect shadow definition: `src/database/edadb/idb/shadow/shadow_idb_geometry.h:45`

`IdbVia` 不定义 root shadow：root identity 是 `_name`，`_master_instance` 内部由 EDADB 的 `Shadow<IdbViaMaster>` 隐式转换。

Schema 与 order/index 约束的关系：

- 依据 `src/database/edadb/docs/def-ieda-mapping-and-order.md`，`VIAS` 映射到 `IdbVias::_via_list`，等级为 Level D。
- 当前 adapter 不保存 root `_order_sd`；如果 DB 读回顺序不同，测试应通过 Level-D normalized diff 判断语义一致性。
- `IdbVia::_name` 是 direct table 第一列和 root identity；它不表达 vector order。
- fixed via 的 `fixed_layer_shape_list_sd` 是 nested vector，由 `TABLE4CLASS_WVEC` child table 保存；layer shape 内的 rect vector 使用 `Shadow<IdbRect>::_vec_idx` 保存 nested order。

Primary-key audit:

- `initPrimKeys()` 没有关闭 `IdbVia` 的 primary-key 行为；`iVia` 使用 `_name` 作为 root PK。
- `initPrimKeys()` 关闭 `Shadow<IdbRect>` 的 primary-key 行为，rect 作为 vector child 依赖 parent FK + `_vec_idx` 表达 child order。
- `Shadow<IdbViaMaster>` 保留 EDADB 默认 PK 行为，因为它 owns `fixed_layer_shape_list_sd`；fixed via 的 layer-shape child rows 需要稳定 parent row。
- `Shadow<IdbLayerShape>` 保留默认 PK 行为，因为它 owns `_rect_list_sd`；rect child rows 需要稳定 parent row。
- `Shadow<IdbViaMasterGenerate>` 当前是 `Shadow<IdbViaMaster>::_master_generate_sd` 的 nested scalar value view，不作为 root/vector table 单独使用；因此 `initPrimKeys()` 显式关闭其 primary key，避免把 `_rule_name_sd` 误当成独立 via-generate identity。
- `initReadDb()` / `initWriteDb()` 都先调用 `initPrimKeys()`，再调用 `initAllTables()`，因此 read/write 的 table metadata 一致。

## Original DEF Write/Read Roundtrip Mapping

### Original DEF Write Flow

| 原始 `DefWrite` 执行顺序 | EDADB write / `toShadow` 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. `write_via()` 对 null/empty list 失败，然后用全部 via 数输出 section count，见 `def_write.cpp:390-404` | `writeIdbVia()` 对 null 失败、empty 成功，且 direct 插入全部 via，见 `def_write_edadb.cpp:276-297` | `VIAS <N>` / `IdbVias::_via_list` / `iVia` rows，无独立 count 字段 |
| 2. 遍历 via，但只对 `via_master->is_generate()` 输出 record，fixed via 被跳过，见 `def_write.cpp:406-428` | direct `IdbVia` root 会同时存储 generated/fixed master；`Shadow<IdbViaMaster>` 用 `_type_sd` 分支，见 `shadow_idb_via_master.h:165-175` | `- <via_name>` / `IdbVia::_name/_master_instance` / `iVia._name/_master_instance` → `iViaMasterSD._type_sd` |
| 3. generated branch 输出 `VIARULE/CUTSIZE/LAYERS/CUTSPACING/ENCLOSURE/ROWCOL`，见 `def_write.cpp:409-420` | generated shadow 保存对应 scalars 和 rule/layer names，见 `shadow_idb_via_master.h:28-49` | generated via fields / `IdbViaMasterGenerate` rule、cut、layer、spacing、enclosure、row/col members / `_rule_name_sd`, cut/layer/spacing/enclosure/row-col shadow fields |
| 4. pattern 非空时输出 `PATTERN`，见 `def_write.cpp:422-424` | 保存 pattern string，见 `shadow_idb_via_master.h:50` | `+ PATTERN` / `IdbViaMasterGenerate::_pattern` / `_pattern_name_sd` |
| 5. 当前 writer 不输出 generated `ORIGIN/OFFSET`，也不输出 fixed layer/rect records | EDADB 仍保存 parser-supported origin/offset，并用 `fixed_layer_shape_list_sd` 保存 fixed via，见 `shadow_idb_via_master.h:40-45,171-174` | parser-only `ORIGIN/OFFSET`; fixed `LAYER RECT` / generated offsets, `IdbViaMaster::_master_fixed_list` / offset fields, `fixed_layer_shape_list_sd` |

### Original DEF Read Flow

| 原始 `DefRead` 执行顺序 | EDADB read / `fromShadow` 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. `viaBeginCallback()` 调 `parse_via_num()` 预分配 via list，见 `def_read.cpp:1759-1779` | EDADB 不存 section count；`readIdbVia()` 按 table rows 读取 | `VIAS <N>` / `IdbVias` capacity / 无 EDADB count 字段 |
| 2. `parse_via()` 按 name 创建 via/master，见 `def_read.cpp:1800-1813` | direct read 构造 `IdbVia`，EDADB 对 `_master_instance` 自动使用 via-master shadow，然后 builder append，见 `def_read_edadb.cpp:469-497` | via name/root / `IdbVia::_name/_master_instance` / `iVia`, `iViaMasterSD` |
| 3. `hasViaRule()` 分支恢复 rule name/pointer、cut size、三层 layer pointer、spacing、enclosure，见 `def_read.cpp:1815-1841` | generated `fromShadow()` 按 name lookup via rule 和 layers，再恢复 scalars，见 `shadow_idb_via_master.h:59-98` | `VIARULE/CUTSIZE/LAYERS/CUTSPACING/ENCLOSURE` / generated master fields/runtime refs / generated shadow scalar/name fields |
| 4. parser 条件恢复 `ORIGIN/OFFSET`，`ROWCOL` 缺省为 1x1，条件恢复 `PATTERN`，见 `def_read.cpp:1843-1873` | generated shadow 直接恢复 stored origin/offset/row-col/pattern，见 `shadow_idb_via_master.h:68-75,100-101` | `ORIGIN/OFFSET/ROWCOL/PATTERN` / generated master fields / corresponding `_sd` fields |
| 5. parser 用 cut size/spacing/origin/row-col/pattern 重建 cut rect、cut bbox 和 via shape，见 `def_read.cpp:1875-1900` | generated `fromShadow()` 执行同类循环与计算，via-master `fromShadow()` 再 `set_via_shape()`，见 `shadow_idb_via_master.h:104-125,183-188` | computed generated geometry / cut rect/bbox/via shape / 不存储，读时重建 |
| 6. fixed branch 按 DEF layer/rect 创建 fixed master，lookup layer，计算 cut bbox 并 `set_via_shape()`，见 `def_read.cpp:1901-1931` | `fixed_layer_shape_list_sd` 恢复 layer/rect vectors，通过 layer name lookup 创建 fixed master，重算 cut bbox/shape，见 `shadow_idb_via_master.h:191-231`, `shadow_idb_layer_shape.h:71-88` | fixed `LAYER RECT` / fixed masters、layer pointers、rects、computed shape / `fixed_layer_shape_list_sd` → `iLayerShapeSD`/`IdbRectSD` |

已验证的不一致：原始 writer 的 section count 使用全部 via 数，却只输出 generated records；fixed via 存在时可产生 count/record 不一致。EDADB 可以保存并恢复 fixed via iDB 状态，但最终经过当前原始 writer 仍不能输出 fixed via DEF record。

## Child Storage View

`IdbVia` 是 `VIAS` root，root 本身 direct mapping，但子节点必须使用 storage view：

- `_master_instance`：通过 `Shadow<IdbViaMaster>` 存储，不直接 dump 原始 `IdbViaMaster`。
- generated via：`Shadow<IdbViaMasterGenerate>` 保存 via rule name、cut size/spacing/enclosure、row/col、origin/offset、bottom/cut/top layer name、pattern string。
- fixed via：`Shadow<IdbViaMaster>` 的 `fixed_layer_shape_list_sd` 保存 `Shadow<IdbLayerShape>` vector。
- layer shape：`Shadow<IdbLayerShape>` 保存 layer name、shape type 和 `Shadow<IdbRect>` rect vector；rect vector 用 `_vec_idx` 保序。

这些 child shadow 是必要的：原始 via master 中包含 LEF layer/rule 指针和由 `set_via_shape()` 计算出的 shape cache。DB 应保存 DEF-visible name/scalar/rect 语义，read 时再按 name lookup LEF layer/rule 并重建几何。

Nested member 说明：

- `Shadow<IdbViaMasterGenerate>` 不对应 DEF root record，也不对应 `IdbVias::_via_list` 的元素；它只是 generated via master 的 scalar value view。
- `_rule_name_sd` 是 lookup key，用来在 read 阶段通过 `EdadbIdbHelper::findIdbViaRuleGenerateByName()` 找 LEF via rule；它不是 EDADB root PK 语义。
- `_layer_bottom_name_sd/_layer_cut_name_sd/_layer_top_name_sd` 是 layer lookup key，用来恢复 runtime layer pointer。
- `_pattern_name_sd` 保存 DEF `+ PATTERN` 字符串；read 阶段通过 `set_patttern()` 重建 pattern object。
- cut rect list、cut bounding box、bottom/cut/top layer shape 不直接保存为 generate shadow 字段，而是由 `fromShadow()` 按原始 `parse_via()` 的 row/col、spacing、origin、pattern 逻辑重算。
- `Shadow<IdbViaMaster>` 是 `_master_instance` 的 storage view：generate via 走 `_master_generate_sd`，fixed via 走 `fixed_layer_shape_list_sd`。
- fixed via 的 `fixed_layer_shape_list_sd` 是 vector child；其 layer-shape 顺序由 EDADB vector child index 保证，layer-shape 内部 rect 顺序由 `IdbRectSD::_vec_idx` 保证，不由 generate shadow 处理。
- `Shadow<IdbLayerShape>` 保存 layer name 而不是 raw `IdbLayer*`；read 阶段通过 `EdadbIdbHelper::findIdbLayerByName()` 恢复 pointer。

## EDADB Write Path

当前 `writeIdbVia()`：

- Code: `src/database/manager/builder/def_builder/def_write_edadb.cpp:276`
- Via vector access: `src/database/manager/builder/def_builder/def_write_edadb.cpp:289`
- Empty-list return: `src/database/manager/builder/def_builder/def_write_edadb.cpp:293`
- EDADB direct insert: `src/database/manager/builder/def_builder/def_write_edadb.cpp:297`

- 从 `design->get_via_list()` 取 root via vector。
- 空列表返回成功，兼容 top-level EDADB framework。
- 使用 `edadb::insertVector<idb::IdbVia>()` 写 direct root object。
- nested via master 写入时自动走 `Shadow<IdbViaMaster>` / `Shadow<IdbViaMasterGenerate>` / `Shadow<IdbLayerShape>`。

这保持了新版 EDADB API 的设计：root `IdbVia` 直接存，内部 pointer/member storage view 由 EDADB shadow specialization 处理。

## EDADB Read Path

当前 `readIdbVia()`：

- Code: `src/database/manager/builder/def_builder/def_read_edadb.cpp:469`
- EDADB read op: `src/database/manager/builder/def_builder/def_read_edadb.cpp:482`
- EDADB read loop: `src/database/manager/builder/def_builder/def_read_edadb.cpp:484`
- Add to active list: `src/database/manager/builder/def_builder/def_read_edadb.cpp:497`

- 先设置 `EdadbIdbHelper` 的 active `IdbDefService`，供 shadow fromShadow 查找 LEF layer 和 via rule。
- `makeReadAllOp<idb::IdbVia>()` 读取 root vias。
- 将读出的 `IdbVia*` 逐个 `via_list->add_via(via_inst)` 加回 design。
- `Shadow<IdbViaMasterGenerate>::fromShadow()` 负责查找 LEF rule/layer，并重新计算 cut rect、cut bounding box 和 via shape。
- `Shadow<IdbViaMaster>::fromShadow()` 负责 fixed via 的 layer shape、cut rect 和 via shape 重建。

## Computed Fields

不直接作为 root via 字段保存、但读回时重建：

- generated via 的 `_rule_generate`、layer pointers：通过 helper 从 LEF DB 按 name 查找。
- generated via 的 cut rect list / cut bounding box：按原始 `parse_via()` 公式重算。
- generated via 的 bottom/cut/top layer shape：调用 `set_via_shape()` 重算。
- fixed via 的 layer pointers：由 `Shadow<IdbLayerShape>` 按 layer name 查找。
- fixed via 的 cut rect / layer shapes：由 `Shadow<IdbViaMaster>::fromShadow()` 重算。

## Order / Index

`IdbVias` 是 vector，但当前不为 root via 增加 `_order_sd`：

- 后续 net/fill/route 使用 via 时主要通过 `find_via(name)` 查找：DEF read net path 在 `src/database/manager/builder/def_builder/def_read.cpp:1152` / `src/database/manager/builder/def_builder/def_read.cpp:1419` 按 via name 查找；fill 在 `src/database/manager/builder/def_builder/def_read.cpp:2377` 按 via name 查找。
- 点工具侧也按 name 使用：iPDN 在 `src/operation/iPDN/source/module/pdn_via/pdn_via.cpp:46` 查找 via，iPNP 在 `src/operation/iPNP/source/module/synthesis/PowerVia.cpp:126` 查找 via，iRT 在 `src/operation/iRT/interface/RTInterface.cpp:1580` 先查 LEF via 再查 DEF via。
- `find_via(size_t index)` 当前没有发现实际调用点。
- root via identity 是 name；不要用 vector index 作为 PK，也不要为了排序额外定义 root `Shadow<IdbVia>`。
- 当前 demo 和 regression 会通过 raw DEF diff 捕获现有测试设计中的 via 输出顺序问题；如果只发生 Level-D root via order 差异，应由 normalized diff 按 via name 判断语义等价。

如果未来发现某个流程依赖 `IdbVias` root vector 原始顺序，应优先讨论是否引入 root order 存储；在当前约束下不新增 `Shadow<IdbVia>`。

对 normalized diff 的影响：

- `VIAS` 是 Level D root list；如果 raw diff 只因为不同 via root record 顺序失败，normalized diff 可以按 via name 排序后通过。
- 排序单位必须是完整 via record；record 内部 layer-shape/rect nested vector 不排序。
- 如果 via name、generated via scalar、layer name 或 geometry 内容不同，normalized diff 必须失败。

## Tests

当前回归覆盖：

- `iVia` count。
- via name set。
- generated via 的 rule name、cut size、row/col、bottom/cut/top layer name。
- `writeIdbVia` / `readIdbVia` 日志。
- demo DEF roundtrip diff clean。

## Risks / TODO

- 原始 writer 的 section count 使用全部 via 数量，但只输出 generated via；存在 declared count 大于实际 record 数的问题。EDADB 会存储并恢复 fixed/generated 两类 root via，但最终 DEF 仍受原始 writer 限制。
- 原始 writer 不输出 `ORIGIN` / `OFFSET`，但 parser 和 EDADB shadow 会保存并参与 geometry 重建；这有利于内部几何一致性，但 DEF 文本输出仍跟随原始 writer。
- 原始 writer 对空 via list 返回失败，EDADB writer 对空 vector 返回成功。
- 若需要强保证 root via list order，不能用 order index 做 PK；应另行设计 identity + order 的存储方案。
