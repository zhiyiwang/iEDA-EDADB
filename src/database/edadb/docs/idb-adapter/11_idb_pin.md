# IdbPin EDADB Adapter Review

## Scope

`IdbPin` 对应 DEF 的 `PINS` section，只处理 design IO pins。

- Write: `DefWrite::write_pin()`
- Read: `pinBeginCallback()` / `pinCallback()` / `DefRead::parse_pin_number()` / `DefRead::parse_pin()`
- EDADB Write: `DefWriteEdadb::writeIdbPin()`
- EDADB Read: `DefReadEdadb::readIdbPin()`

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

## EDADB Write Path

当前 `writeIdbPin()`：

- 从 `design->get_io_pin_list()` 获取 IO pin vector。
- 空列表返回成功，兼容 EDADB framework。
- 按 vector 顺序构造 `Shadow<IdbPin>`，第 `idx` 个写 `_order_sd = idx`。
- 保存 pin name、net name、term shadow、location、average coordinate、orient、IO/special flags 和 layer count。

## EDADB Read Path

当前 `readIdbPin()`：

- reset 当前 IO pin list，避免 DEF callback 重复创建。
- 使用 `ORDER BY "_order_sd"` 读取 `iPinSD`，恢复 IO pin root list 原始顺序。
- `fromShadow()` 恢复 pin name、net name、term、location、average coordinate、orient、IO flag。
- 逐个重建 term ports、layer shapes 和 rects。
- 如果 term has port，调用 `pin->set_port_layer_shape()` 重建 pin-level absolute shapes。
- 如果没有 explicit port，则按原始 `parse_pin()` 逻辑重算 term average position、term bounding box 和 pin bounding box。

`createDbByDef()` 已禁用 pin callback，因此 `PINS` 只来自 EDADB。

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
- `writeIdbPin` / `readIdbPin` 日志。
- demo DEF roundtrip diff clean。

## Risks / TODO

- 当前测试主要覆盖无 explicit `+ PORT` 的 pin；special pin / explicit port 已由代码路径支持，但还需要更小 fixture 做边界覆盖。
- `_layer_num_sd` 是辅助校验字段，真实 geometry 仍来自 term/port/layer-shape nested rows。
