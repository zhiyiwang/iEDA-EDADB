# IdbPin EDADB Adapter Review

## Scope

`IdbPin` 对应 DEF 的 `PINS` section，只处理 design IO pins。

- Write: `DefWrite::write_pin()` at `src/database/manager/builder/def_builder/def_write.cpp:517`
- Read: `DefRead::parse_pin_number()` / `DefRead::parse_pin()` at `src/database/manager/builder/def_builder/def_read.cpp:1543` and `src/database/manager/builder/def_builder/def_read.cpp:1573`
- EDADB Write: `DefWriteEdadb::writeIdbPin()` at `src/database/manager/builder/def_builder/def_write_edadb.cpp:360`
- EDADB Read: `DefReadEdadb::readIdbPin()` at `src/database/manager/builder/def_builder/def_read_edadb.cpp:812`

本文件按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 的约束检查：

- DEF section 映射：`PINS` section。
- root-vector order 等级：`IdbPins::_pin_list` 需要显式保留 append order。
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

当前 schema：

```cpp
TABLE4CLASS(edadb::Shadow<idb::IdbPin>, "iPinSD",
            (_pin_name_sd, _order_sd, _net_name_sd, _io_term_sd,
             _average_coordinate_sd, _location_sd, _orient_sd,
             _is_io_pin_sd, _is_special_net_sd, _layer_num_sd));
```

相关 nested shadow：

- `Shadow<IdbTerm>` 保存 direction/use/special/has-port 和 port vector。
- `Shadow<IdbPort>` 保存 port orient/status/coordinate 和 layer shape vector。
- `Shadow<IdbLayerShape>` 保存 layer name、shape type 和 rect vector。

Schema / init 代码位置：

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

## Field Mapping To Original DEF Flow

以下按 EDADB shadow 域列出它对应的原始 DEF read/write 代码位置。

- Pin identity / root order: `_pin_name_sd`, `_order_sd`
  - Write source: `DefWrite::write_pin()` 按 IO pin list 顺序输出 pin name，见 `src/database/manager/builder/def_builder/def_write.cpp:517-586`。
  - Read source: `pinsBeginCallback()` / `parse_pin_number()` reserve list，`pinCallback()` / `parse_pin()` 按 DEF 出现顺序创建 IO pin，见 `src/database/manager/builder/def_builder/def_read.cpp:1529-1746`。

- Net ref and pin flags: `_net_name_sd`, `_is_io_pin_sd`, `_is_special_net_sd`, `_io_term_sd._is_special_net_sd`
  - Write source: `write_pin()` 输出 pin net name 和 SPECIAL flag，见 `src/database/manager/builder/def_builder/def_write.cpp:517-586`。
  - Read source: `parse_pin()` 读取 net name、设置 IO pin，并读取 special flag，见 `src/database/manager/builder/def_builder/def_read.cpp:1573-1746`。
  - DB decision: DEF `+ SPECIAL` 对应 `IdbTerm::_is_special_net`，即 `iPinSD._io_term_sd__is_special_net_sd`；root `_is_special_net_sd` 只是 `IdbPin::_special_net` pointer 是否非空的 runtime 状态，不能单独代表 DEF `+ SPECIAL`。

- IO term fields: `_io_term_sd`
  - Write source: `write_pin()` 输出 direction/use，并根据 port/layer shape 输出 PORT 信息，见 `src/database/manager/builder/def_builder/def_write.cpp:517-586`。
  - Read source: `parse_pin()` 创建 `IdbTerm`，设置 direction/use/special/port，见 `src/database/manager/builder/def_builder/def_read.cpp:1573-1746`。

- Port placement: `_location_sd`, `_orient_sd`, `_average_coordinate_sd`
  - Write source: `write_pin()` 输出 port/pin placement status、location、orient，见 `src/database/manager/builder/def_builder/def_write.cpp:517-586`。
  - Read source: `parse_pin()` 读取 placement，并计算 pin/term bounding box，见 `src/database/manager/builder/def_builder/def_read.cpp:1573-1746`。

- Layer shapes and rects: port/layer-shape child shadows, `_layer_num_sd`
  - Write source: `write_pin()` 输出 layer name 和 rect geometry，见 `src/database/manager/builder/def_builder/def_write.cpp:517-586`。
  - Read source: `parse_pin()` 为每个 port layer 创建 `IdbLayerShape`，按 layer name lookup 并保存 rect，见 `src/database/manager/builder/def_builder/def_read.cpp:1573-1746`。

- Computed absolute geometry
  - Write source: DEF writer 输出 DEF-relative pin/port geometry，不输出 runtime bbox cache，见 `src/database/manager/builder/def_builder/def_write.cpp:517-586`。
  - Read source: `parse_pin()` 调用 term/pin bbox 计算逻辑，见 `src/database/manager/builder/def_builder/def_read.cpp:1573-1746`。

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

- Code: `src/database/manager/builder/def_builder/def_read_edadb.cpp:812`
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
- `_layer_num_sd` 是辅助校验字段，真实 geometry 仍来自 term/port/layer-shape nested rows。
