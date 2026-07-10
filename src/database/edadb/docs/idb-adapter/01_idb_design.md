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

本文件按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 的约束检查：

- DEF section 映射：`VERSION`、`BUSBITCHARS`、`DESIGN`、`UNITS`。
- iEDA root container：无 root list；使用 active singleton `IdbDesign` 及 inline children `IdbUnits` / `IdbBusBitChars`。
- root-vector order 等级：Level D，但原因是 singleton fields，没有 root list 可排序。
- normalized diff：不应对 `IdbDesign` 做 record sort；只要求 singleton 字段值一致。

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

Schema / init 代码位置：

- `IdbUnits` table macro: `src/database/edadb/idb/edadb_idb_schema.h:23`
- `IdbBusBitChars` table macro: `src/database/edadb/idb/edadb_idb_schema.h:26`
- `IdbDesign` table macro: `src/database/edadb/idb/edadb_idb_schema.h:29`
- Primary-key setup: `src/database/edadb/idb/edadb_idb_init.cpp:21`
- Table registration: `src/database/edadb/idb/edadb_idb_init.cpp:80`

当前采用 direct class mapping，不需要 `Shadow<IdbDesign>`。

Schema 与新 order/index 约束的关系：

- `IdbDesign` 是 singleton root object，不需要 `_order_sd`。
- `IdbUnits` 和 `IdbBusBitChars` 是 inline singleton value object，不属于 root list。
- 当前 direct mapping 覆盖 DEF-visible singleton 字段；不涉及 ABCD 中的 root-vector reorder 问题。

Primary-key audit:

- `initPrimKeys()` 显式关闭 `IdbUnits` 和 `IdbBusBitChars` 的 primary-key 行为，因为它们作为 `IdbDesign` 的 inline child/value object 使用。
- `initPrimKeys()` 没有关闭 `IdbDesign` 的 primary-key 行为；`IdbDesign` 是 root singleton table，按 EDADB 默认 root table key 规则处理。
- `initReadDb()` / `initWriteDb()` 都先调用 `initPrimKeys()`，再调用 `initAllTables()`，因此 read/write 的 table metadata 一致。

## Original DEF Write/Read Roundtrip Mapping

下面两张表以当前源码的原始 `DefWrite` / `DefRead` 执行顺序为主线；第三列统一记录 DEF 域、iDB 成员和 EDADB 字段。

### Original DEF Write Flow

| 原始 `DefWrite` 执行顺序 | EDADB write 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. `writeChip()` 先调 `write_version()`；`_version` 为空时文本 fallback 为 `5.8`，见 `def_write.cpp:205-210,270-279` | `writeIdbDesign()` 直接存储 active `_version`，见 `def_write_edadb.cpp:145-180`；空值不在 DB 中 canonicalize，后续 DEF writer 仍会输出 `5.8` | `VERSION` / `IdbDesign::_version` / `iDesign._version` |
| 2. `write_divider_char()` 固定输出 `/`，见 `def_write.cpp:208,281-285` | 不存储；该值不是 `IdbDesign` 成员 | `DIVIDERCHAR` / 无 iDB 成员 / 无 EDADB 字段 |
| 3. `write_busbit_char()` 检查 pointer 并输出左右 delimiter，见 `def_write.cpp:209,288-301` | `writeIdbDesign()` 检查 `_bus_bit_chars`，由 direct nested mapping 写入，见 `def_write_edadb.cpp:170-180` | `BUSBITCHARS` / `IdbDesign::_bus_bit_chars`, `IdbBusBitChars::_left_delimiter/_right_delimiter` / `iDesign._bus_bit_chars` → `iBusBitChars` |
| 4. `write_design()` 输出 design name，见 `def_write.cpp:210,308-316` | direct mapping 写入 `_design_name` | `DESIGN` / `IdbDesign::_design_name` / `iDesign._design_name` |
| 5. `write_units()` 优先 DEF DBU，非正值时 fallback 到 LEF DBU，见 `def_write.cpp:211,318-338` | `writeIdbDesign()` 使用同一 fallback，并将选中值写回 active DEF units 后插入，见 `def_write_edadb.cpp:152-180` | `UNITS DISTANCE MICRONS` / `IdbUnits::_micron_dbu`, `IdbLayout::_units` fallback / `iUnits._micron_dbu` |

### Original DEF Read Flow

DEF callback 的实际触发顺序由输入 DEF tag 顺序决定；下表按 iEDA `writeChip()` 生成的 header 顺序对应。

| 原始 `DefRead` 执行顺序 | EDADB read 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. `versionCallback()` 调 `parse_version()`，直接设置 active design version，见 `def_read.cpp:619-638` | `readIdbDesign()` 从 temporary `got` 拷贝 version，见 `def_read_edadb.cpp:241-258` | `VERSION` / `IdbDesign::_version` / `iDesign._version` |
| 2. `busBitCharsCallBack()` 校验长度为 2，`parse_bus_bit_chars()` 新建 value object、删除旧 pointer 并挂接，见 `def_read.cpp:2398-2434` | `readIdbDesign()` 检查 `got` child，删除 active child 后转移 pointer ownership，见 `def_read_edadb.cpp:276-283` | `BUSBITCHARS` / `IdbDesign::_bus_bit_chars` / `iDesign._bus_bit_chars` ← `iBusBitChars` |
| 3. `designCallback()` 调 `parse_design()` 设置 name，见 `def_read.cpp:640-659` | 从 `got` 拷贝 `_design_name`，见 `def_read_edadb.cpp:251-258` | `DESIGN` / `IdbDesign::_design_name` / `iDesign._design_name` |
| 4. `unitsCallback()` 调 `parse_units()`，与 LEF DBU 比较后复用 active `IdbUnits` 设置 DBU，见 `def_read.cpp:661-689` | 执行同样 LEF DBU warning 检查，将 `got` DBU 写入 active units，再释放 temporary child，见 `def_read_edadb.cpp:260-274` | `UNITS DISTANCE MICRONS` / `IdbUnits::_micron_dbu` / `iUnits._micron_dbu` |

## Child Storage View

`IdbDesign` 是 DEF design header 的 root，当前只有两个持久化子节点：

- `IdbUnits`：direct member，通过 `IdbDesign::_units` inline 写入；当前 DEF 语义只需要 `_micron_dbu`，其它 units 字段是原始类附带字段。
- `IdbBusBitChars`：direct member，通过 `IdbDesign::_bus_bit_chars` inline 写入；只保存左右 delimiter。

这两个子节点不需要 shadow：它们是 singleton value object，不引用 LEF/layout 对象，也没有 vector child order。`readIdbDesign()` 仍按原始 parser 语义处理 ownership：`_units` 复用 active object，`_bus_bit_chars` 替换 active pointer。

## EDADB Write Path

当前 `writeIdbDesign()` 已贴近原始 writer：

- Code: `src/database/manager/builder/def_builder/def_write_edadb.cpp:145`
- EDADB insert: `src/database/manager/builder/def_builder/def_write_edadb.cpp:180`

- 检查 active `design`。
- 按 `write_units()` 语义计算 `micron_dbu`：优先 DEF units，否则 LEF units。
- 如果 DEF units 无效但 LEF units 有效，会把 active `def_units->_micron_dbu` 规范化成实际 DEF 输出值。
- 检查 `bus_bit_chars` 非空。
- 直接 `insertObject<IdbDesign>(design)`。

需要注意：当前 schema 会连同 `IdbUnits` 其它非 DEF 输出字段一起存入 DB。若未来想更严格贴近 DEF 语义，可只保存 `_micron_dbu`。

## EDADB Read Path

当前 `readIdbDesign()`：

- Code: `src/database/manager/builder/def_builder/def_read_edadb.cpp:236`
- EDADB read op: `src/database/manager/builder/def_builder/def_read_edadb.cpp:237`
- Active design restore: `src/database/manager/builder/def_builder/def_read_edadb.cpp:252`

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

按 `def-ieda-mapping-and-order.md` 的等级定义：

- Level: D。
- 具体含义：不是“可排序 root list”，而是“无 root vector / singleton data”。
- 测试要求：比较 `_design_name`、`_version`、`_units->_micron_dbu`、`_bus_bit_chars` 左右 delimiter 的值；不能通过排序处理 Design 差异。

依据：

- 原始 DEF read/write 只恢复一个 active `IdbDesign`。
- `IdbUnits` 和 `IdbBusBitChars` 是 inline/member object，不是 DEF statement list。
- 点工具没有 `vector<IdbDesign>` order/index 使用点。

当前状态：已满足，不需要 `_order`。

对 normalized diff 的影响：

- `VERSION`、`BUSBITCHARS`、`DESIGN`、`UNITS` 是 singleton statements。
- 如果这些字段不同，normalized diff 必须失败。
- D-level root record 排序规则不适用于本类，因为没有可排序 root records。

## Risks / TODO

`readIdbDesign()` 已按原始 read 语义收敛：

- `parse_units()` 复用 active `design->_units`，当前 EDADB read 也复用该对象并只恢复 `_micron_dbu`。
- `parse_bus_bit_chars()` 删除旧对象并替换；当前 EDADB read 直接接管 EDADB 读出的新对象，语义相同且代码更直观。
- `IdbDesign::~IdbDesign()` 当前删除 `_bus_bit_chars`，但没有删除 `_units`；因此禁止在 EDADB read 中替换 active `_units` pointer。

后续仍需确认：

- `IdbUnits` schema 当前保存了 DEF writer 不输出的其它 units 字段；如果要更严格贴近 DEF，可收敛 schema。
- 原始 `write_version()` 在 version 为空时输出 `5.8`，EDADB 当前保存 raw `_version`；因此最终 DEF 可一致，但 DB 中恢复后的 iDB version 仍可能为空。
