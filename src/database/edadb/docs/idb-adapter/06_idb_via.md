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
- nested vector 约束：fixed via 的 rect vector 是 DEF `RECT` 输入几何，必须通过 `_vec_idx` 恢复顺序。fixed layer-shape vector 当前不存 index，bottom/cut/top 语义通过 layer type/order 重建；两者都只随 root via 整体移动，不参与 D-level root sort。

## Original Write Semantics

原始 `DefWrite::write_via()` 输出：

- via count: `design->get_via_list()->get_num_via()`
- via name: `IdbVia::_name`
- generated via fields: via rule, cut size, bottom/cut/top layer names, cut spacing, enclosure, row/col
- optional pattern: `IdbViaMasterGenerate::_patttern`
- generated via 不直接输出 `IdbViaMasterGenerate::_cut_rect_list`；cut rectangles 是由上述 generated parameters 计算得到的派生几何。

当前原始 writer 只输出 `via_master->is_generate()` 的 via；`is_fix()` 没有对应的 `LAYER/RECT` 输出分支。因此 fixed via 的 rectangle vector 虽然能被原始 parser 读入和 EDADB 保存，但当前不能由原始 `DefWrite::write_via()` 写回 DEF。

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
  - 每个 `LAYER` 对应一个 `IdbViaMasterFixed::_layer_shape`，其 `IdbLayerShape::_rect_list` 保存该 layer 下按 DEF 出现顺序读入的 `RECT` records。
  - 根据 cut layer rect 计算 cut rect，再调用 `set_via_shape()`。

## Generated / Fixed Branch and Rectangle Mapping

`DefRead::parse_via()` 的 `if (def_via->hasViaRule()) ... else ...` 对应两种不同的 iDB 存储语义：

| DEF read branch | iDB source of truth | EDADB write | EDADB read/rebuild |
| --- | --- | --- | --- |
| `hasViaRule() == true` generated via，见 `def_read.cpp:1815-1900` | `IdbViaMasterGenerate` 的 rule/layer name、cut size/spacing、enclosure、row/col、origin/offset、pattern 是原始数据；`_cut_rect_list` 和 cut bbox 是派生几何 | `Shadow<IdbViaMasterGenerate>::toShadow()` 只写 generated scalar/name fields，不写 `_cut_rect_list`，见 `shadow_idb_via_master.h:28-50` | `fromShadow()` 先恢复 scalar/name fields，再按 row/col、cut size/spacing、origin、pattern 调用 `add_cut_rect()` 重建 `_cut_rect_list`，见 `shadow_idb_via_master.h:59-125`；之后 `set_via_shape()` 用该 vector 构造 cut layer shape |
| `hasViaRule() == false` fixed via，见 `def_read.cpp:1901-1931` | `IdbViaMaster::_master_fixed_list` 中每个 `IdbViaMasterFixed::_layer_shape`；真正的 rectangle vector 是 `IdbLayerShape::_rect_list` | `Shadow<IdbViaMaster>::toShadow()` 把每个 fixed master 的 layer shape 加入 `fixed_layer_shape_list_sd`，见 `shadow_idb_via_master.h:171-174`；`Shadow<IdbLayerShape>::toShadow()` 再把 `get_rect_list()` 复制到 `_rect_list_sd`，见 `shadow_idb_layer_shape.h:57-66` | EDADB 先读回 layer-shape/rect child rows；`Shadow<IdbLayerShape>::fromShadow()` 恢复 layer pointer 和 rect vector，见 `shadow_idb_layer_shape.h:71-88`；`Shadow<IdbViaMaster>::fromShadow()` 再通过 `add_fixed()` / `add_rect()` 重建 `_master_fixed_list`，并计算 cut bbox/via shape，见 `shadow_idb_via_master.h:191-231` |

两条路径没有在 EDADB 中互相转换 rectangle vector：

- generated source：generated parameters -> `fromShadow()` 计算 `IdbViaMasterGenerate::_cut_rect_list` -> `set_via_shape()` 将该 vector 复制到公共 `_layer_shape_cut`，见 `IdbViaMaster.cpp:433-470`。
- fixed source：`fixed_layer_shape_list_sd -> IdbLayerShape::_rect_list` -> `fromShadow()` 重建 `IdbViaMaster::_master_fixed_list` -> `set_via_shape()` 按 layer type/order 选出 bottom/cut/top fixed master，并把各自 rect 复制到公共 layer shapes，见 `IdbViaMaster.cpp:472-517,523-568`。
- 因此，`IdbViaMasterFixed::_layer_shape->_rect_list` 不是 `IdbViaMasterGenerate::_cut_rect_list`。它们只在 `set_via_shape()` 之后形成相同的公共消费接口：`_layer_shape_bottom/_layer_shape_cut/_layer_shape_top`。

`def_read.cpp:1876` 的

```cpp
vector<IdbRect*> cut_rect_list = master_generate->get_cut_rect_list();
```

只是一个未继续使用的 local vector copy。`shadow_idb_via_master.h:105` 也有同样的未使用 local copy。两条路径后续都通过 `add_cut_rect()` 直接写入成员 `IdbViaMasterGenerate::_cut_rect_list`；EDADB generated-via adapter 不应把 local variable 或派生 cut-rect vector 当作持久化源。

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
- `IdbRectSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:70`
- `iLayerShapeSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:74`
- `iViaMasterSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:78`
- `iVia` root table macro: `src/database/edadb/idb/edadb_idb_schema.h:81`
- Primary-key setup: `src/database/edadb/idb/edadb_idb_init.cpp:21`
- `Shadow<IdbViaMasterGenerate>` PK disabled: `src/database/edadb/idb/edadb_idb_init.cpp:28`
- `Shadow<IdbViaMaster>` PK uses EDADB default `true`; no explicit `initPrimKeys()` override.
- Table registration: `src/database/edadb/idb/edadb_idb_init.cpp:81`
- Via master shadow definition: `src/database/edadb/idb/shadow/shadow_idb_via_master.h:22`
- Layer shape shadow definition: `src/database/edadb/idb/shadow/shadow_idb_layer_shape.h:18`
- Rect shadow definition: `src/database/edadb/idb/shadow/shadow_idb_geometry.h:45`

`IdbVia` 不定义 root shadow：root identity 是 `_name`，`_master_instance` 内部由 EDADB 的 `Shadow<IdbViaMaster>` 隐式转换。

Schema 与 order/index 约束的关系：

- 依据 `src/database/edadb/docs/def-ieda-mapping-and-order.md`，`VIAS` 映射到 `IdbVias::_via_list`，等级为 Level D。
- 当前 adapter 不保存 root `_order_sd`；如果 DB 读回顺序不同，测试应通过 Level-D normalized diff 判断语义一致性。
- `IdbVia::_name` 是 direct table 第一列和 root identity；它不表达 vector order。
- fixed via 的 `fixed_layer_shape_list_sd` 是 nested vector，由 `TABLE4CLASS_WVEC` child table 保存。当前 `Shadow<IdbLayerShape>` 没有保存该 layer-shape vector 的 index；恢复后的 bottom/cut/top 语义由 layer type/order lookup 确定。layer shape 内部的 rect vector 则使用 `Shadow<IdbRect>::_vec_idx` 显式恢复 nested order。

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
| 1. `write_via()` 对 null/empty list 失败，然后用全部 via 数输出 section count，见 `def_write.cpp:390-404` | `writeIdbVia()` 对 null 失败、empty 成功，且 direct 插入全部 via，见 `def_write_edadb.cpp:288-309` | `VIAS <N>` / `IdbVias::_via_list` / `iVia` rows，无独立 count 字段 |
| 2. 遍历 via，但只对 `via_master->is_generate()` 输出 record，fixed via 被跳过，见 `def_write.cpp:406-428` | direct `IdbVia` root 会同时存储 generated/fixed master；`Shadow<IdbViaMaster>` 用 `_type_sd` 分支，见 `shadow_idb_via_master.h:165-175` | `- <via_name>` / `IdbVia::_name/_master_instance` / `iVia._name/_master_instance` → `iViaMasterSD._type_sd` |
| 3. generated branch 输出 `VIARULE/CUTSIZE/LAYERS/CUTSPACING/ENCLOSURE/ROWCOL`，见 `def_write.cpp:409-420` | generated shadow 保存对应 scalars 和 rule/layer names，见 `shadow_idb_via_master.h:28-49` | generated via fields / `IdbViaMasterGenerate` rule、cut、layer、spacing、enclosure、row/col members / `_rule_name_sd`, cut/layer/spacing/enclosure/row-col shadow fields |
| 4. pattern 非空时输出 `PATTERN`，见 `def_write.cpp:422-424` | 保存 pattern string，见 `shadow_idb_via_master.h:50` | `+ PATTERN` / `IdbViaMasterGenerate::_pattern` / `_pattern_name_sd` |
| 5. generated writer 不输出 `_cut_rect_list`；该 vector 由 generated parameters 派生 | EDADB 仅保存 generated parameters，不保存 `IdbViaMasterGenerate::_cut_rect_list`，见 `shadow_idb_via_master.h:28-50` | derived cut geometry / `IdbViaMasterGenerate::_cut_rect_list` / 无对应 EDADB 列或 child table |
| 6. 当前 writer 不输出 generated `ORIGIN/OFFSET`，也没有 fixed `LAYER/RECT` 分支 | EDADB 仍保存 parser-supported origin/offset；对 fixed via，保存 `fixed_layer_shape_list_sd -> _rect_list_sd -> IdbRectSD`，见 `shadow_idb_via_master.h:40-45,171-174`, `shadow_idb_layer_shape.h:57-66` | parser-only `ORIGIN/OFFSET`; fixed `LAYER RECT` / generated offsets, `IdbViaMasterFixed::_layer_shape->_rect_list` / offset fields, fixed layer-shape/rect child tables |

### Original DEF Read Flow

| 原始 `DefRead` 执行顺序 | EDADB read / `fromShadow` 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. `viaBeginCallback()` 调 `parse_via_num()` 预分配 via list，见 `def_read.cpp:1759-1779` | EDADB 不存 section count；`readIdbVia()` 按 table rows 读取 | `VIAS <N>` / `IdbVias` capacity / 无 EDADB count 字段 |
| 2. `parse_via()` 按 name 创建 via/master，见 `def_read.cpp:1800-1813` | direct read 构造 `IdbVia`，EDADB 对 `_master_instance` 自动使用 via-master shadow，然后 builder append，见 `def_read_edadb.cpp:467-496` | via name/root / `IdbVia::_name/_master_instance` / `iVia`, `iViaMasterSD` |
| 3. `hasViaRule()` 分支恢复 rule name/pointer、cut size、三层 layer pointer、spacing、enclosure，见 `def_read.cpp:1815-1841` | generated `fromShadow()` 按 name lookup via rule 和 layers，再恢复 scalars，见 `shadow_idb_via_master.h:59-98` | `VIARULE/CUTSIZE/LAYERS/CUTSPACING/ENCLOSURE` / generated master fields/runtime refs / generated shadow scalar/name fields |
| 4. parser 条件恢复 `ORIGIN/OFFSET`，`ROWCOL` 缺省为 1x1，条件恢复 `PATTERN`，见 `def_read.cpp:1843-1873` | generated shadow 直接恢复 stored origin/offset/row-col/pattern，见 `shadow_idb_via_master.h:68-75,100-101` | `ORIGIN/OFFSET/ROWCOL/PATTERN` / generated master fields / corresponding `_sd` fields |
| 5. generated branch 用 cut size/spacing/origin/row-col/pattern 调用 `add_cut_rect()` 重建 `IdbViaMasterGenerate::_cut_rect_list`，再计算 cut bbox 和 via shape，见 `def_read.cpp:1875-1900` | generated `fromShadow()` 执行同类循环与计算，via-master `fromShadow()` 再 `set_via_shape()`，见 `shadow_idb_via_master.h:104-125,183-188` | computed generated geometry / `IdbViaMasterGenerate::_cut_rect_list`, cut bbox, via shapes / 不存储，读时重建 |
| 6. fixed `else` branch 按每个 DEF `LAYER/RECT` 创建 `IdbViaMasterFixed`，把 rect 加入该 fixed master 的 `IdbLayerShape::_rect_list`，再计算 cut bbox 并 `set_via_shape()`，见 `def_read.cpp:1901-1931` | `fixed_layer_shape_list_sd` 恢复 layer shapes，`_rect_list_sd`/`IdbRectSD` 恢复每层 rect vector；之后通过 layer name lookup、`add_fixed()` 和 `add_rect()` 重建 iDB，见 `shadow_idb_layer_shape.h:71-88`, `shadow_idb_via_master.h:191-231` | fixed `LAYER RECT` / `IdbViaMaster::_master_fixed_list -> IdbViaMasterFixed::_layer_shape -> IdbLayerShape::_rect_list` / `fixed_layer_shape_list_sd -> _rect_list_sd -> IdbRectSD` |

已验证的不一致：原始 writer 的 section count 使用全部 via 数，却只输出 generated records；fixed via 存在时可产生 count/record 不一致。EDADB 可以保存并恢复 fixed via iDB 状态，但最终经过当前原始 writer 仍不能输出 fixed via DEF record。

## Child Storage View

`IdbVia` 是 `VIAS` root，root 本身 direct mapping，但子节点必须使用 storage view：

- `_master_instance`：通过 `Shadow<IdbViaMaster>` 存储，不直接 dump 原始 `IdbViaMaster`。
- generated via：`Shadow<IdbViaMasterGenerate>` 保存 via rule name、cut size/spacing/enclosure、row/col、origin/offset、bottom/cut/top layer name、pattern string。
- fixed via：`Shadow<IdbViaMaster>` 不额外定义 `Shadow<IdbViaMasterFixed>`，而是将 `_master_fixed_list` flatten 为 `fixed_layer_shape_list_sd`；其元素对应每个 fixed master 的 `_layer_shape`。
- layer shape：`Shadow<IdbLayerShape>` 保存 layer name、shape type 和 `_rect_list_sd`；这个 rect vector 就是 fixed DEF `LAYER/RECT` 分支需要持久化的 rectangle vector，元素通过 `Shadow<IdbRect>` 存储并用 `_vec_idx` 保序。

这些 child shadow 是必要的：原始 via master 中包含 LEF layer/rule 指针和由 `set_via_shape()` 计算出的 shape cache。DB 应保存 DEF-visible name/scalar/rect 语义，read 时再按 name lookup LEF layer/rule 并重建几何。

Nested member 说明：

- `Shadow<IdbViaMasterGenerate>` 不对应 DEF root record，也不对应 `IdbVias::_via_list` 的元素；它只是 generated via master 的 scalar value view。
- `_rule_name_sd` 是 lookup key，用来在 read 阶段通过 `EdadbIdbHelper::findIdbViaRuleGenerateByName()` 找 LEF via rule；它不是 EDADB root PK 语义。
- `_layer_bottom_name_sd/_layer_cut_name_sd/_layer_top_name_sd` 是 layer lookup key，用来恢复 runtime layer pointer。
- `_pattern_name_sd` 保存 DEF `+ PATTERN` 字符串；read 阶段通过 `set_patttern()` 重建 pattern object。
- cut rect list、cut bounding box、bottom/cut/top layer shape 不直接保存为 generate shadow 字段，而是由 `fromShadow()` 按原始 `parse_via()` 的 row/col、spacing、origin、pattern 逻辑重算。
- `Shadow<IdbViaMaster>` 是 `_master_instance` 的 storage view：generate via 走 `_master_generate_sd`，fixed via 走 `fixed_layer_shape_list_sd`。
- fixed via 的 `fixed_layer_shape_list_sd` 是 vector child，但当前 `Shadow<IdbLayerShape>` 不记录 vector index；恢复 bottom/cut/top 时依赖 layer type/order，而不依赖 layer-shape vector index。layer-shape 内部 rect 顺序由 `IdbRectSD::_vec_idx` 保证，不由 generate shadow 处理。
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

- Code: `src/database/manager/builder/def_builder/def_read_edadb.cpp:467`
- Helper setup before EDADB initialization: `src/database/manager/builder/def_builder/def_read_edadb.cpp:35`
- EDADB read op: `src/database/manager/builder/def_builder/def_read_edadb.cpp:480`
- EDADB read loop: `src/database/manager/builder/def_builder/def_read_edadb.cpp:482`
- Add to active list: `src/database/manager/builder/def_builder/def_read_edadb.cpp:495`

- `createDbFromEdadb()` 在调用 `initReadDb()` 和 `readIdbVia()` 前统一设置 `EdadbIdbHelper` 的 active `IdbDefService`，供 shadow `fromShadow()` 查找 LEF layer 和 via rule。
- `makeReadAllOp<idb::IdbVia>()` 读取 root vias。
- 将读出的 `IdbVia*` 逐个 `via_list->add_via(via_inst)` 加回 design。
- `Shadow<IdbViaMasterGenerate>::fromShadow()` 负责查找 LEF rule/layer，并重新计算 cut rect、cut bounding box 和 via shape。
- `Shadow<IdbViaMaster>::fromShadow()` 负责 fixed via 的 layer shape、cut rect 和 via shape 重建。

## Computed Fields

不直接作为 root via 字段保存、但读回时重建：

- generated via 的 `_rule_generate`、layer pointers：通过 helper 从 LEF DB 按 name 查找。
- generated via 的 `IdbViaMasterGenerate::_cut_rect_list` / cut bounding box：不入库，按原始 `parse_via()` 公式重算。
- generated via 的 bottom/cut/top layer shape：调用 `set_via_shape()` 重算。
- fixed via 的 layer pointers：由 `Shadow<IdbLayerShape>` 按 layer name 查找。
- fixed via 的 per-layer `IdbLayerShape::_rect_list`：从 EDADB `IdbRectSD` child rows 恢复，不是派生数据。
- fixed via 的 aggregate cut bbox 和 bottom/cut/top layer-shape cache：由 `Shadow<IdbViaMaster>::fromShadow()` 重算。

## Order / Index

`IdbVias` 是 vector，但当前不为 root via 增加 `_order_sd`：

- 后续 net/fill/route 使用 via 时主要通过 `find_via(name)` 查找：DEF read net path 在 `src/database/manager/builder/def_builder/def_read.cpp:1154` / `src/database/manager/builder/def_builder/def_read.cpp:1421` 按 via name 查找；fill 在 `src/database/manager/builder/def_builder/def_read.cpp:2379` 按 via name 查找。
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

标准 sky130 回归覆盖：

- `iVia` count。
- via name set。
- generated via 的 rule name、cut size、row/col、bottom/cut/top layer name。
- `writeIdbVia` / `readIdbVia` 日志。
- demo DEF roundtrip diff clean。

标准 sky130 fixture 的 via SQL 断言只覆盖 generated via。为验证 fixed 分支，本次使用同一套 `def2edadb_generic.tcl` / `edadb2def_generic.tcl` 测试框架加入一个定向 DEF via：

```def
- fixed_rect_probe
  + RECT met1 ( -120 -130 ) ( 120 130 )
  + RECT via ( -80 -80 ) ( -10 -10 )
  + RECT via ( 10 10 ) ( 80 80 )
  + RECT met2 ( -140 -150 ) ( 140 150 )
 ;
```

验证结果：

- `writeIdbVia insert via_count=5`，`readIdbVia restored via_count=5`。
- `iVia` 中 `fixed_rect_probe._master_instance__type_sd=2`，即 `kFixed`；四个 generated via 没有写 fixed layer/rect child rows。
- fixed layer child rows 为 `met1`、`met2`、`via`；rect child rows 分别为 `met1[0]=(-120,-130,120,130)`、`met2[0]=(-140,-150,140,150)`、`via[0]=(-80,-80,-10,-10)`、`via[1]=(10,10,80,80)`。
- GDB 在 `readIdbVia()` append 前检查恢复对象：`_master_fixed_list` 为 `met1(1 rect)`、`met2(1 rect)`、`via(2 rect)`；公共 shape 为 bottom=`met1(1 rect)`、cut=`via(2 rect)`、top=`met2(1 rect)`，坐标与输入完全一致。
- 输入 fixed layer record 顺序是 `met1, via, met2`，DB 读回后的 `_master_fixed_list` 顺序是 `met1, met2, via`。这是 child table 无 vector index 导致的顺序变化；`get_master_fixed()` 按 routing layer order 和 cut layer type 选择 bottom/cut/top，因此公共 shape 语义不受影响。cut layer 内两个 rect 通过 `_vec_idx=0,1` 保持原顺序。

该测试确认 EDADB 的 fixed rect 写入、读取、owner-layer 恢复和公共 via-shape 重建正确；但不能用最终 DEF 文本验证 fixed record，因为当前原始 `DefWrite::write_via()` 不输出 fixed `LAYER/RECT`。

## Risks / TODO

- 原始 writer 的 section count 使用全部 via 数量，但只输出 generated via；存在 declared count 大于实际 record 数的问题。EDADB 会存储并恢复 fixed/generated 两类 root via，但最终 DEF 仍受原始 writer 限制。
- generated via 的 `_cut_rect_list` 是 read-time 派生数据，fixed via 的 `IdbLayerShape::_rect_list` 是 DEF 输入数据；两者不能使用同一套持久化策略。
- fixed `fixed_layer_shape_list_sd` 当前未显式保存 layer-shape vector index；内部 rect vector 已由 `IdbRectSD::_vec_idx` 保序。如果未来原始 writer 增加 fixed `LAYER/RECT` 输出，需再确认 layer record 顺序是否需要显式保存。
- 原始 writer 不输出 `ORIGIN` / `OFFSET`，但 parser 和 EDADB shadow 会保存并参与 geometry 重建；这有利于内部几何一致性，但 DEF 文本输出仍跟随原始 writer。
- 原始 writer 对空 via list 返回失败，EDADB writer 对空 vector 返回成功。
- 若需要强保证 root via list order，不能用 order index 做 PK；应另行设计 identity + order 的存储方案。
