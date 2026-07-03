# IdbVia EDADB Adapter Review

## Scope

`IdbVia` 对应 DEF 的 `VIAS` section。

- Write: `DefWrite::write_via()`
- Read: `viaBeginCallback()` / `viaCallback()` / `DefRead::parse_via_num()` / `DefRead::parse_via()`
- EDADB Write: `DefWriteEdadb::writeIdbVia()`
- EDADB Read: `DefReadEdadb::readIdbVia()`

本文件按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 的约束检查：

- DEF section 映射：`VIAS` section。
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

- `iViaMasterGenerateSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:66`
- `IdbRectSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:69`
- `iLayerShapeSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:73`
- `iViaMasterSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:77`
- `iVia` root table macro: `src/database/edadb/idb/edadb_idb_schema.h:81`
- Primary-key setup: `src/database/edadb/idb/edadb_idb_init.cpp:21`
- `Shadow<IdbViaMasterGenerate>` PK disabled: `src/database/edadb/idb/edadb_idb_init.cpp:31`
- `Shadow<IdbViaMaster>` PK uses EDADB default `true`; no explicit `initPrimKeys()` override.
- Table registration: `src/database/edadb/idb/edadb_idb_init.cpp:86`
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

## Field Mapping To Original DEF Flow

以下按 EDADB direct/shadow 域列出它对应的原始 DEF read/write 代码位置。

- Via identity: `IdbVia::_name`
  - Write source: `DefWrite::write_via()` 输出每个 DEF via name，见 `src/database/manager/builder/def_builder/def_write.cpp:390-435`。
  - Read source: `viaBeginCallback()` / `parse_via_num()` reserve via list，`viaCallback()` / `parse_via()` 创建 DEF via，见 `src/database/manager/builder/def_builder/def_read.cpp:1759-1935`。

- Via generate rule: via master generate fields
  - Write source: `write_via()` 对 generate via 输出 `VIARULE`、cut size、cut rows/cols、enclosure、spacing、row/col offset、origin、pattern 等字段，见 `src/database/manager/builder/def_builder/def_write.cpp:390-435`。
  - Read source: `parse_via()` 读取 generated via rule/cut size/layers/spacing/enclosure/offset/origin/pattern，见 `src/database/manager/builder/def_builder/def_read.cpp:1800-1935`。

- Via layer-shape rects: layer name and rect list
  - Write source: `write_via()` 对 fixed via 输出 layer + rect geometry，见 `src/database/manager/builder/def_builder/def_write.cpp:390-435`。
  - Read source: `parse_via()` 读取 via layer rects 并绑定 LEF layer，见 `src/database/manager/builder/def_builder/def_read.cpp:1800-1935`。

- Runtime layer/via-rule pointers: computed by lookup
  - Write source: DEF writer 输出 name/geometry，不输出 raw pointers，见 `src/database/manager/builder/def_builder/def_write.cpp:390-435`。
  - Read source: 原始 parser 通过 layout via rules/layers lookup 重建 pointer 关系，见 `src/database/manager/builder/def_builder/def_read.cpp:1800-1935`。

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

- Code: `src/database/manager/builder/def_builder/def_write_edadb.cpp:288`
- Via vector access: `src/database/manager/builder/def_builder/def_write_edadb.cpp:301`
- Empty-list return: `src/database/manager/builder/def_builder/def_write_edadb.cpp:305`
- EDADB direct insert: `src/database/manager/builder/def_builder/def_write_edadb.cpp:309`

- 从 `design->get_via_list()` 取 root via vector。
- 空列表返回成功，兼容 top-level EDADB framework。
- 使用 `edadb::insertVector<idb::IdbVia>()` 写 direct root object。
- nested via master 写入时自动走 `Shadow<IdbViaMaster>` / `Shadow<IdbViaMasterGenerate>` / `Shadow<IdbLayerShape>`。

这保持了新版 EDADB API 的设计：root `IdbVia` 直接存，内部 pointer/member storage view 由 EDADB shadow specialization 处理。

## EDADB Read Path

当前 `readIdbVia()`：

- Code: `src/database/manager/builder/def_builder/def_read_edadb.cpp:466`
- Helper setup: `src/database/manager/builder/def_builder/def_read_edadb.cpp:467`
- EDADB read op: `src/database/manager/builder/def_builder/def_read_edadb.cpp:487`
- EDADB read loop: `src/database/manager/builder/def_builder/def_read_edadb.cpp:489`
- Add to active list: `src/database/manager/builder/def_builder/def_read_edadb.cpp:502`

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

- 原始 writer 不输出 fixed via；如果输入 DEF 使用 fixed via，当前 iEDA 原生 writer 和 EDADB adapter 都需要单独 review。
- 原始 writer 不输出 `ORIGIN` / `OFFSET`，但 parser 和 EDADB shadow 会保存并参与 geometry 重建；这有利于内部几何一致性，但 DEF 文本输出仍跟随原始 writer。
- 若需要强保证 root via list order，不能用 order index 做 PK；应另行设计 identity + order 的存储方案。
