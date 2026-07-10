# IdbPin EDADB Adapter Review

## Scope

`IdbPin` 对应 DEF 的 `PINS` section，只处理 design IO pins。

- Write: `DefWrite::write_pin()` at `src/database/manager/builder/def_builder/def_write.cpp:517`
- Read: `DefRead::parse_pin_number()` / `DefRead::parse_pin()` at `src/database/manager/builder/def_builder/def_read.cpp:1543` and `src/database/manager/builder/def_builder/def_read.cpp:1573`
- EDADB Write: `DefWriteEdadb::writeIdbPin()` at `src/database/manager/builder/def_builder/def_write_edadb.cpp:360`
- EDADB Read: `DefReadEdadb::readIdbPin()` at `src/database/manager/builder/def_builder/def_read_edadb.cpp:808`

本文件按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 的约束检查：

- DEF section 映射：`PINS` section。
- iEDA root container：`IdbPins::_pin_list`。
- root-vector order 等级：Level B，`IdbPins::_pin_list` 需要显式保留 append order，因为 iFP IO placement 会按 pin list 顺序分配物理位置。
- root identity 约束：IO pin name 是 DEF-visible identity，当前 `_pin_name_sd` 是 EDADB root PK；禁止用 vector order index 作为 PK。
- nested vector 约束：term port vector、port layer-shape vector、layer-shape rect vector 都是 pin 内部几何语义，必须随 root pin 保持原始顺序。

## Original Write Semantics

原始 `DefWrite::write_pin()` 输出：

- pin count: `design->get_io_pin_list()->get_pin_num()`
- pin name and net name
- optional `+ SPECIAL`
- direction and use, from `IdbTerm`
- if term has explicit ports or pin is special:
  - write `+ PORT`
  - per-port layer shapes and rects
  - per-port placement status, coordinate, orient
- otherwise:
  - write pin-level layer shapes and rects from term ports
  - optional pin-level placement status, location, orient

## Original Read Semantics

原始 `DefRead::parse_pin()`：

- `parse_pin_number()` reserve IO pin vector。
- 按 DEF 出现顺序 `pin_list->add_pin_list(trimEscape(pinName))`。
- 保存 net name、orient、IO flag。
- 创建 `IdbTerm`，保存 direction/use/special。
- `numPorts() > 0`:
  - 创建 `IdbPort`，保存 orient、placement status、coordinate。
  - 保存 port layer shape 和 relative rect。
  - 调用 `pin->set_port_layer_shape()` 生成 pin-level absolute layer shapes。
- 无 explicit port:
  - 创建一个 implicit port，保存 layer shape 和 relative rect。
  - 计算 term average position 和 term bounding box。
  - 若有 placement，则设置 pin location、average coordinate、pin bounding box。

## EDADB Schema

当前 root schema：

```cpp
TABLE4CLASS(edadb::Shadow<idb::IdbPin>, "iPinSD",
            (_pin_name_sd, _order_sd, _net_name_sd, _io_term_sd,
             _average_coordinate_sd, _location_sd, _orient_sd,
             _is_io_pin_sd, _is_special_net_sd, _layer_num_sd));
```

相关 nested / inline storage schema：

```cpp
TABLE4SHADOW(idb::IdbCoordinate<int32_t>);
TABLE4CLASS(edadb::Shadow<idb::IdbCoordinate<int32_t>>,
            "iCoordSD", (_vec_idx, _x_sd, _y_sd));

TABLE4SHADOW(idb::IdbRect);
TABLE4CLASS(edadb::Shadow<idb::IdbRect>, "IdbRectSD",
            (_vec_idx, _lx_sd, _ly_sd, _hx_sd, _hy_sd));

TABLE4SHADOW_WVEC(idb::IdbLayerShape);
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbLayerShape>,
                  "iLayerShapeSD", (_layer_name_sd, _type_sd),
                  (_rect_list_sd));

TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbPort>, "iPortSD",
                  (primary_key, _class_sd, _orient_sd,
                   _placement_status_sd, _coordinate_sd),
                  (_layer_shape_list_sd));

TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbTerm>, "iTermSD",
                  (_name_sd, _direction_sd, _type_sd, _shape_sd,
                   _placement_status_sd, _has_port_sd,
                   _is_special_net_sd, _is_instance_sd),
                  (_port_list_sd));
```

`iPinSD` 不是完整 `IdbPin` object dump，而是 `PINS` 的 DEF storage view。它依赖以下 store views：

- `Shadow<IdbTerm>` / `iTermSD`：`_io_term_sd` 的 inline child，保存 direction/use/shape/special/has-port，并拥有 port vector。
- `Shadow<IdbPort>` / `iPortSD`：term port vector child，保存 port class、orient、placement status、coordinate，并拥有 layer-shape vector。
- `Shadow<IdbLayerShape>` / `iLayerShapeSD`：port layer-shape vector child，保存 layer name、shape type，并拥有 rect vector。
- `Shadow<IdbRect>` / `IdbRectSD`：layer-shape rect vector child，保存 DEF-relative rect，使用 `_vec_idx` 保持 rect append order。
- `Shadow<IdbCoordinate<int32_t>>` / `iCoordSD`：保存 pin average coordinate、pin location、port coordinate；scalar coordinate 不使用 `_vec_idx` 表达 root order。
- `IdbLayer`：不作为 child table 存储；layer shape 只保存 layer name，read 时从 LEF layer list lookup。
- `IdbNet` / `IdbSpecialNet`：不作为 child table 存储；这里只保存 `_net_name_sd` 和 special flag，后续 net/special-net adapter 按 pin name/link name 恢复 pointer。

Schema / init 代码位置：

- `iCoordSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:34`
- `IdbRectSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:70`
- `iLayerShapeSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:74`
- `iPortSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:94`
- `iTermSD` table macro: `src/database/edadb/idb/edadb_idb_schema.h:97`
- `iPinSD` root table macro: `src/database/edadb/idb/edadb_idb_schema.h:100`
- `Shadow<IdbPin>` PK uses EDADB default `true`; `_pin_name_sd` is the first table column and root identity.
- `Shadow<IdbTerm>` is stored inline in `iPinSD`; it is not registered as an independent root table.
- `Shadow<IdbPort>` owns `_layer_shape_list_sd`, so it uses explicit `primary_key` in the child table.
- `Shadow<IdbLayerShape>` owns `_rect_list_sd` and uses layer name as child identity under its parent port.
- `Shadow<IdbRect>` PK is disabled in `src/database/edadb/idb/edadb_idb_init.cpp:30`; rect order is represented by `_vec_idx`.
- Table registration: `src/database/edadb/idb/edadb_idb_init.cpp:88`
- Pin shadow definition: `src/database/edadb/idb/shadow/shadow_idb_pin.h:18`
- Term shadow definition: `src/database/edadb/idb/shadow/shadow_idb_term.h:15`
- Port shadow definition: `src/database/edadb/idb/shadow/shadow_idb_port.h:18`

Primary-key audit:

- `Shadow<IdbPin>` 保留默认 primary-key 行为，因为 `PINS` root record 有天然 DEF identity：pin name。
- `_order_sd` 只表达 `IdbPins::_pin_list` append order，不作为 identity。
- `Shadow<IdbPort>::primary_key` 是 nested vector-owner identity，用于挂接 port 的 layer-shape child rows；它不表示 DEF root identity。
- `Shadow<IdbLayerShape>` 在同一 port 下用 layer name 作为 child identity；rect child rows 用 `IdbRectSD::_vec_idx` 保序。
- `Shadow<IdbTerm>` 是 pin 的 inline value view；它不单独建 root table。

## Original DEF Write/Read Roundtrip Mapping

### Original DEF Write Flow

| 原始 `DefWrite` 执行顺序 | EDADB write / `toShadow` 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. `write_pin()` 输出 pin count，再按 `IdbPins::_pin_list` 顺序遍历，见 `def_write.cpp:517-528` | `writeIdbPin()` 按 index 保存 `_order_sd`；empty vector 同样成功，见 `def_write_edadb.cpp:360-389` | `PINS <N>`, pin order / `IdbPins::_pin_list` / `iPinSD._order_sd` |
| 2. 输出 pin name、net name、optional SPECIAL、DIRECTION，见 `def_write.cpp:529-534` | pin shadow 保存 name/net/io/special，term shadow 保存 direction/special，见 `shadow_idb_pin.h:29-47`, `shadow_idb_term.h:25-33` | `- <pin> + NET ... + SPECIAL + DIRECTION` / `IdbPin::_pin_name/_net_name`, `IdbTerm::_direction/_is_special_net` / `_pin_name_sd/_net_name_sd/_io_term_sd` |
| 3. term use 非空时输出 `USE`，见 `def_write.cpp:536-540` | term shadow 保存 `_type_sd`，见 `shadow_idb_term.h:28` | `+ USE` / `IdbTerm::_type` / `_io_term_sd._type_sd` |
| 4. `term->is_port_exist()` 或 `pin->is_special_net_pin()` 为真时进入 explicit PORT 分支，按 port vector 输出 `+ PORT`，见 `def_write.cpp:542-545` | term/port shadows 保存 `_has_port_sd` 和有序 `_port_list_sd`，见 `shadow_idb_term.h:31,35-41` | `+ PORT` / `IdbTerm::_has_port/_port_list` / `_io_term_sd._has_port_sd/_port_list_sd` |
| 5. explicit PORT 中按 layer-shape/rect vector 输出 layer 和 relative rect，见 `def_write.cpp:546-553` | port → layer-shape → rect shadow 树保存 layer name/type 和 rect order，见 `shadow_idb_port.h:61-76`, `shadow_idb_layer_shape.h:57-66` | `+ LAYER ... (xl yl)(xh yh)` / `IdbPort::_layer_shape_list`, `IdbLayerShape::_rect_list` / `_layer_shape_list_sd`, `_rect_list_sd` |
| 6. explicit port placed 时输出 port status、coordinate、orient，见 `def_write.cpp:554-556` | port shadow 保存 `_placement_status_sd/_coordinate_sd/_orient_sd`，见 `shadow_idb_port.h:61-68` | port placement / `IdbPort::_placement_status/_coordinate/_orient` / corresponding port shadow fields |
| 7. 否则进入 legacy no-PORT 分支：输出 term-port layer/rect，term placed 时输出 term status、pin location、pin orient，见 `def_write.cpp:560-575` | 仍使用 term/port/layer-shape tree；pin shadow 另存 `_location_sd/_orient_sd`，term shadow 存 placement status | legacy `LAYER` + placement / `IdbTerm::_placement_status/_port_list`, `IdbPin::_location/_orient` / `_io_term_sd`, `_location_sd`, `_orient_sd` |

### Original DEF Read Flow

| 原始 `DefRead` 执行顺序 | EDADB read / `fromShadow` 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. pin-count callback 初始化 list，`parse_pin()` trim name、append pin，恢复 net name/orient/IO flag，见 `def_read.cpp:1543-1549,1573-1601` | `readIdbPin()` reset list，按 `_order_sd` 读取，`pin.fromShadow()` 恢复 name/net/orient/io/location，见 `def_read_edadb.cpp:797-837`, `shadow_idb_pin.h:50-69` | pin root/header / `IdbPin::_pin_name/_net_name/_orient/_is_io_pin` / root pin shadow fields |
| 2. parser 创建 term，设 term name，条件恢复 DIRECTION/USE/SPECIAL，见 `def_read.cpp:1603-1615` | term shadow `fromShadow()` 恢复 name/direction/type/special/has-port/status，见 `shadow_idb_term.h:44-57` | `DIRECTION/USE/SPECIAL` / `IdbTerm` fields / `_io_term_sd` scalar fields |
| 3. `numPorts()>0` 时设 has-port，按 port 顺序创建 port、恢复 orient，见 `def_read.cpp:1617-1623` | builder 按 `_port_list_sd` 顺序创建 port，port shadow 恢复 orient/status/coordinate，见 `def_read_edadb.cpp:848-850`, `shadow_idb_port.h:79-90` | explicit `PORT` / `IdbTerm::_port_list`, `IdbPort` scalars / `_port_list_sd` |
| 4. 每个 port 按 layer order lookup LEF layer、创建 rect，见 `def_read.cpp:1624-1632` | builder 按 layer-shape child order调 `fromShadow()`，按 layer name lookup 并恢复 rect，见 `def_read_edadb.cpp:852-857`, `shadow_idb_layer_shape.h:71-88` | `LAYER/RECT` / `IdbLayerShape`, `IdbRect` / layer-shape/rect shadow fields |
| 5. explicit port 有 placement 时恢复 port status/coordinate，第一个 port 同步 term status，见 `def_read.cpp:1634-1666` | port/term shadow 已恢复这些 scalars；几何恢复后调 `port->set_io_bounding_box()`，见 `def_read_edadb.cpp:872-875` | explicit port placement / port + term status/coordinate / port/term shadow fields |
| 6. explicit ports 完成后调 `pin->set_port_layer_shape()` 生成 pin absolute geometry，见 `def_read.cpp:1669` | `_has_port_sd` 为 true 时调同一几何计算，见 `def_read_edadb.cpp:877-879` | computed absolute layer shapes/bbox / `IdbPin` derived geometry / 不需要独立 DB 字段 |
| 7. no-PORT 分支创建一个 port，按 pin layer 恢复 layer/rect，同时统计 bbox 和中点和，见 `def_read.cpp:1671-1707` | builder 恢复同一 child tree，当 `_has_port_sd` 为 false 时同样统计 rect bbox/中点，见 `def_read_edadb.cpp:859-869` | legacy layer geometry / term port/layer shapes / `_port_list_sd/_layer_shape_list_sd` |
| 8. layer 数非零时计算 term average/bbox；pin 有 placement 时设 term status、pin location/absolute average/bbox，见 `def_read.cpp:1709-1735` | builder 重算 term average/bbox 并调 `pin->set_bounding_box()`，见 `def_read_edadb.cpp:879-885`；`_average_coordinate_sd` 虽被存储/恢复，仍会被此逻辑重算 | computed average/bbox + placement / term/pin derived fields, `_location` / geometry child fields, `_location_sd` |

审计结论：`_layer_num_sd` 当前写入 schema 但 `readIdbPin()` 不使用；root `_is_special_net_sd` 也未在 `Shadow<IdbPin>::fromShadow()` 恢复。DEF `+ SPECIAL` 依靠 term shadow 的 `_is_special_net_sd` 恢复，因此当前 writer 的 `pin special || term special` 仍可输出 SPECIAL，但 root flag 本身不是完整 roundtrip。

## Child Storage View

`IdbPin` 是 `PINS` root，当前子节点按 DEF 语义分层保存：

- `_io_term_sd`：`Shadow<IdbTerm>` child，保存 term name、direction、use、special、has-port、placement status 和 port vector。
- `_port_list_sd`：`Shadow<IdbPort>` vector child，保存 port class、orient、placement status、coordinate 和 layer shape vector。
- `_layer_shape_list_sd`：`Shadow<IdbLayerShape>` vector child，保存 layer name/type 和 `Shadow<IdbRect>` rect vector；rect vector 用 `_vec_idx` 保序。
- `_average_coordinate_sd` / `_location_sd`：coordinate value fields，保存 pin-level 位置/平均坐标。

不直接保存原始 `IdbPin::_layer_shape_list`：它是由 term/port relative geometry 和 placement 计算得到的 absolute geometry cache。原始 DEF writer/read 的语义根在 `IdbTerm -> IdbPort -> IdbLayerShape`，所以 EDADB 保存这个层次，并在 read path 调用 `set_port_layer_shape()` 或重算 bbox。

也不保存 `_net` / `_special_net` pointer：这里只保存 net name，后续 net/special-net adapter 按 pin name 连接对象引用。

## EDADB Write Path

当前 `writeIdbPin()`：

- Code: `src/database/manager/builder/def_builder/def_write_edadb.cpp:360`
- Enabled by chip writer: `src/database/manager/builder/def_builder/def_write_edadb.cpp:97`

- 从 `design->get_io_pin_list()` 获取 IO pin vector。
- 空列表返回成功，兼容 EDADB framework。
- 按 vector 顺序构造 `Shadow<IdbPin>`，第 `idx` 个写 `_order_sd = idx`。
- 保存 pin name、net name、term shadow、location、average coordinate、orient、IO/special flags 和 layer count。

## EDADB Read Path

当前 `readIdbPin()`：

- Code: `src/database/manager/builder/def_builder/def_read_edadb.cpp:808`
- Enabled by EDADB read flow: `src/database/manager/builder/def_builder/def_read_edadb.cpp:217`

- reset 当前 IO pin list，避免 DEF callback 重复创建。
- 使用 `ORDER BY "_order_sd"` 读取 `iPinSD`，恢复 IO pin root list 原始顺序。
- `fromShadow()` 恢复 pin name、net name、term、location、average coordinate、orient、IO flag。
- 逐个重建 term ports、layer shapes 和 rects。
- 如果 term has port，调用 `pin->set_port_layer_shape()` 重建 pin-level absolute shapes。
- 如果没有 explicit port，则按原始 `parse_pin()` 逻辑重算 term average position、term bounding box 和 pin bounding box。

`createDbByDef()` 使用 `defrUnsetPinCbk()` / `defrUnsetPinEndCbk()` / `defrUnsetStartPinsCbk()` 清掉 DEF pin callbacks，因此 `PINS` 只来自 EDADB。

## Computed Fields

不直接从 DB root 字段读取、而在 read path 重建：

- pin-level absolute `_layer_shape_list`
- pin bounding box
- implicit-port term average position / term bounding box
- net/special-net pointer：由后续 net/special-net adapter 按 pin name 连接

## Order / Index

`IdbPins` root order 需要保持：

- 原始 parser 按 DEF `PINS` 出现顺序 append。
- 原始 writer 按 `pin_list->get_pin_list()` 当前顺序输出。
- net/special-net 后续按 pin name 查找，但 DEF diff 和流程稳定性仍依赖 root vector 顺序。

当前已实现：`_pin_name_sd` 作为 identity，`_order_sd` 作为 root list order，read path 显式 `ORDER BY "_order_sd"`。

## Tests

当前回归覆盖：

- `iPinSD` count。
- sample pin 的 name、net name、direction、use、has-port、location、layer count。
- IO pin root order prefix。
- port/layer/rect child row count。
- aux optional fixture 将 `clk` 改成 explicit `+ PORT` 和 `+ SPECIAL`，并验证 `iPinSD._io_term_sd__has_port_sd`、`iPinSD._io_term_sd__is_special_net_sd`、port placement、layer name、rect geometry。
- `writeIdbPin` / `readIdbPin` 日志。
- demo DEF roundtrip diff clean。

## Risks / TODO

- 当前已覆盖无 explicit `+ PORT` 的默认 pin，以及一个 explicit `+ PORT` / `+ SPECIAL` pin。
- 多 PORT、多 LAYER、多 rect、未放置 pin 等边界仍可继续扩展 fixture。
- `_layer_num_sd` 当前被写入但 `readIdbPin()` 不使用；真实 geometry 来自 term/port/layer-shape nested rows，应评估是否删除该冗余字段。
- root `_is_special_net_sd` 未由 `Shadow<IdbPin>::fromShadow()` 恢复；当前 DEF `+ SPECIAL` 依靠 term shadow 恢复，但 root flag 本身不是完整 roundtrip。
- `IdbTerm::_shape/_is_instance` 与 `IdbPort::_class` 被 EDADB 保存，但原始 `parse_pin()` 不从 `PINS` section 设置这些字段，需要确认是否属于 adapter 应保存的状态。
