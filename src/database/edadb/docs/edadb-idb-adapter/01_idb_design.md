# IdbDesign EDADB Adapter Review

## Scope

`IdbDesign` 在 DEF 中对应这些原始 writer/parser：

- Write: `write_version()`, `write_busbit_char()`, `write_design()`, `write_units()`
- Read: `designCallback()` / `parse_design()`, `unitsCallback()` / `parse_units()`, `busBitCharsCallBack()` / `parse_bus_bit_chars()`

它覆盖 DEF 的：

- `VERSION`
- `BUSBITCHARS`
- `DESIGN`
- `UNITS DISTANCE MICRONS`

`DIVIDERCHAR` 目前由原始 writer 固定输出 `/`，不是 `IdbDesign` 成员，当前不进入 EDADB `IdbDesign` schema。

## Original Write Semantics

原始 `DefWrite` 输出字段：

- `write_version()`：读取 `IdbDesign::_version`；为空时输出默认 `5.8`。
- `write_busbit_char()`：读取 `IdbDesign::_bus_bit_chars->_left_delimiter/_right_delimiter`。
- `write_design()`：读取 `IdbDesign::_design_name`。
- `write_units()`：读取 `IdbDesign::_units->_micron_dbu`；如果 DEF units 无效，则使用 `layout->_units->_micron_dbu`。

因此 EDADB 需要保存或规范化以下数据：

- `IdbDesign::_design_name`
- `IdbDesign::_version`
- `IdbUnits::_micron_dbu`
- `IdbBusBitChars::_left_delimiter`
- `IdbBusBitChars::_right_delimiter`

`IdbUnits` 的其它成员不是当前 DEF writer 输出内容；保存在 schema 中不影响 roundtrip，但不是 DEF 语义必需字段。

## Original Read Semantics

原始 `DefRead` 重建逻辑：

- `parse_design(name)`：复用 active `IdbDesign`，只设置 `_design_name`。
- `parse_units(microns)`：复用 active `IdbDesign::_units`，只设置 `_micron_dbu`；同时和 LEF DBU 比较，不一致只 warning。
- `parse_bus_bit_chars(chars)`：new 一个 `IdbBusBitChars`，设置左右 delimiter，删除旧 `_bus_bit_chars`，再挂到 `IdbDesign`。

## EDADB Schema

当前 schema：

- `TABLE4CLASS(idb::IdbUnits, "iUnits", (..., _micron_dbu, ...))`
- `TABLE4CLASS(idb::IdbBusBitChars, "iBusBitChars", (_left_delimiter, _right_delimiter))`
- `TABLE4CLASS(idb::IdbDesign, "iDesign", (_design_name, _version, _units, _bus_bit_chars))`

当前采用 direct class mapping，不需要 `Shadow<IdbDesign>`。

## EDADB Write Path

当前 `writeIdbDesign()` 已贴近原始 writer：

- 检查 active `design`。
- 按 `write_units()` 语义计算 `micron_dbu`：优先 DEF units，否则 LEF units。
- 如果 DEF units 无效但 LEF units 有效，会把 active `def_units->_micron_dbu` 规范化成实际 DEF 输出值。
- 检查 `bus_bit_chars` 非空。
- 直接 `insertObject<IdbDesign>(design)`。

需要注意：当前 schema 会连同 `IdbUnits` 其它非 DEF 输出字段一起存入 DB。若未来想更严格贴近 DEF 语义，可只保存 `_micron_dbu`。

## EDADB Read Path

当前 `readIdbDesign()`：

- 从 EDADB 读取一个 `IdbDesign got` 作为安全缓冲。
- 设置 active `design->_design_name` 和 `_version`。
- 复用 active `design->_units`，只从 `got._units` 复制 `_micron_dbu`，并保持和原始 `parse_units()` 一样的 LEF DBU warning 语义。
- 删除 active `design->_bus_bit_chars`，直接接管 EDADB 读出的 `got._bus_bit_chars`，并清空 `got._bus_bit_chars`。

## Computed Fields

当前 `IdbDesign` 组没有复杂几何计算字段。

读回时需要保持的计算/校验语义：

- `UNITS` 应该和原始 `parse_units()` 一样，可与 LEF DBU 比较并 warning。
- 如果 `VERSION` 为空，DEF writer 输出时仍会 canonicalize 为 `5.8`。
- `DIVIDERCHAR` 是固定 writer 输出，不应作为 `IdbDesign` DB 字段处理。

## Order / Index

`IdbDesign` 是 singleton root object，不存在 `vector<IdbDesign>` root list 顺序问题。

依据：

- 原始 DEF read/write 只恢复一个 active `IdbDesign`。
- `IdbUnits` 和 `IdbBusBitChars` 是 inline/member object，不是 DEF statement list。

当前状态：已满足，不需要 `_order`。

## Risks / TODO

`readIdbDesign()` 已按原始 read 语义收敛：

- `parse_units()` 复用 active `design->_units`，当前 EDADB read 也复用该对象并只恢复 `_micron_dbu`。
- `parse_bus_bit_chars()` 删除旧对象并替换；当前 EDADB read 直接接管 EDADB 读出的新对象，语义相同且代码更直观。
- `IdbDesign::~IdbDesign()` 当前删除 `_bus_bit_chars`，但没有删除 `_units`；因此禁止在 EDADB read 中替换 active `_units` pointer。

后续仍需确认：

- `IdbUnits` schema 当前保存了 DEF writer 不输出的其它 units 字段；如果要更严格贴近 DEF，可收敛 schema。
