# IdbVia EDADB Adapter Review

## Scope

`IdbVia` 对应 DEF 的 `VIAS` section。

- Write: `DefWrite::write_via()`
- Read: `viaBeginCallback()` / `viaCallback()` / `DefRead::parse_via_num()` / `DefRead::parse_via()`
- EDADB Write: `DefWriteEdadb::writeIdbVia()`
- EDADB Read: `DefReadEdadb::readIdbVia()`

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
```

`IdbVia` 不定义 root shadow：root identity 是 `_name`，`_master_instance` 内部由 EDADB 的 `Shadow<IdbViaMaster>` 隐式转换。

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
- layer shape：`Shadow<IdbLayerShape>` 保存 layer name、shape type 和 direct `IdbRect` rect vector。

这些 child shadow 是必要的：原始 via master 中包含 LEF layer/rule 指针和由 `set_via_shape()` 计算出的 shape cache。DB 应保存 DEF-visible name/scalar/rect 语义，read 时再按 name lookup LEF layer/rule 并重建几何。

## EDADB Write Path

当前 `writeIdbVia()`：

- 从 `design->get_via_list()` 取 root via vector。
- 空列表返回成功，兼容 top-level EDADB framework。
- 使用 `edadb::insertVector<idb::IdbVia>()` 写 direct root object。
- nested via master 写入时自动走 `Shadow<IdbViaMaster>` / `Shadow<IdbViaMasterGenerate>` / `Shadow<IdbLayerShape>`。

这保持了新版 EDADB API 的设计：root `IdbVia` 直接存，内部 pointer/member storage view 由 EDADB shadow specialization 处理。

## EDADB Read Path

当前 `readIdbVia()`：

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

- 后续 net/fill/route 使用 via 时主要通过 `find_via(name)` 查找。
- `find_via(size_t index)` 目前没有发现实际调用点。
- root via identity 是 name；不要用 vector index 作为 PK。
- 当前 demo 和 regression 会通过 DEF diff 捕获现有测试设计中的 via 输出顺序问题。

如果未来发现某个流程依赖 `IdbVias` root vector 原始顺序，应优先讨论是否引入 root order 存储；在当前约束下不新增 `Shadow<IdbVia>`。

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
