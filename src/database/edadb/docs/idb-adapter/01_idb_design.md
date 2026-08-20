# IdbDesign EDADB Adapter Review

## Scope And Constraints

`IdbDesign` 对应 DEF header singleton：

- `VERSION` → `IdbDesign::_version`
- `BUSBITCHARS` → `IdbDesign::_bus_bit_chars`
- `DESIGN` → `IdbDesign::_design_name`
- `UNITS DISTANCE MICRONS` → `IdbDesign::_units->_micron_dbu`

本实现按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 检查：

- iEDA 使用一个 active `IdbDesign`，不存在 root list 或 vector index。
- 等级为 Level D singleton；不增加 `_order_sd`，也不对这些 header statement 做 normalized sort。
- `DIVIDERCHAR` 由原始 writer 固定输出 `/`，没有对应 `IdbDesign` 成员，因此不进入 EDADB。
- 仅保存原始 DEF read/write 使用的字段；`IdbUnits` 的 LEF-only 单位字段不进入 schema。

## EDADB Schema

```cpp
TABLE4CLASS(idb::IdbUnits, "iUnits", (_micron_dbu));
TABLE4CLASS(idb::IdbBusBitChars, "iBusBitChars",
            (_left_delimiter, _right_delimiter));
TABLE4CLASS(idb::IdbDesign, "iDesign",
            (_design_name, _version, _units, _bus_bit_chars));
```

代码位置：

- Schema：`src/database/edadb/idb/edadb_idb_schema.h:22-29`
- Inline child PK disabled：`src/database/edadb/idb/edadb_idb_init.cpp:21-23`
- Root table registration：`src/database/edadb/idb/edadb_idb_init.cpp:69-76`

存储模型：

- `IdbDesign` 使用 direct mapping；没有引入 `Shadow<IdbDesign>` 的必要。
- `_design_name` 是自然 root identity；不增加 synthetic PK。
- `IdbUnits`、`IdbBusBitChars` 是 inline value view，不是独立 root identity，因此关闭 PK。
- SQLite 的 `iDesign` 中实际展开为 `_units__micron_dbu` 和 `_bus_bit_chars__*` 列；`initAllTables()` 只注册 `IdbDesign` root。

## Original DEF Write Mapping

下表严格按 `DefWrite::writeChip()` 的执行顺序组织。

| Original writer brace | DEF output | EDADB correspondence | Stored source |
| --- | --- | --- | --- |
| `write_version()`：空字符串输出默认 `5.8`，`def_write.cpp:270-279` | `VERSION value ;` | `writeIdbDesign()` 在 insert 前把空 version 规范化为 `5.8`，`def_write_edadb.cpp:145-154` | `iDesign._version` |
| `write_divider_char()` 固定输出 `/`，`def_write.cpp:281-285` | `DIVIDERCHAR "/" ;` | 不存储 | 无 iDB/EDADB 字段 |
| `write_busbit_char()` 检查 pointer 并输出左右 delimiter，`def_write.cpp:288-301` | `BUSBITCHARS "xy" ;` | direct nested mapping；adapter 同样拒绝 null，`def_write_edadb.cpp:174-177` | `iDesign._bus_bit_chars__left_delimiter/right_delimiter` |
| `write_design()` 输出 name，`def_write.cpp:308-316` | `DESIGN name ;` | direct mapping | `iDesign._design_name` |
| `write_units()` 优先 DEF DBU，非正值时使用 LEF DBU，`def_write.cpp:318-337` | `UNITS DISTANCE MICRONS n ;` | 使用同一 fallback，并把最终 DBU 写回 active DEF units，`def_write_edadb.cpp:156-172` | `iDesign._units__micron_dbu` |

`writeChip2Edadb()` 在对象写入流程中首先调用 `writeIdbDesign()`：
`src/database/manager/builder/def_builder/def_write_edadb.cpp:75-78`。最终 insert 位于
`src/database/manager/builder/def_builder/def_write_edadb.cpp:184-187`。

## Original DEF Read Mapping

DEF callback 的触发顺序由输入 tag 决定；表中按原始 writer 的 header 顺序列出。

| Original parser brace | EDADB correspondence | Source / synchronization / calculation |
| --- | --- | --- |
| `versionCallback()` → `parse_version()`，直接设置 active version，`def_read.cpp:619-638` | 从 temporary `got` 拷贝到 active design，`def_read_edadb.cpp:239-256` | DB source：`iDesign._version` |
| `busBitCharsCallBack()` 校验长度；`parse_bus_bit_chars()` new child、删除旧 child、挂接新 child，`def_read.cpp:2398-2434` | EDADB 已分配 `got` child；检查非空后删除 active child、转移 pointer，并清空 `got` pointer，`def_read_edadb.cpp:274-281` | DB source：两个 delimiter；ownership 同原 parser 的 replace 语义 |
| `designCallback()` → `parse_design()`，设置 active name，`def_read.cpp:640-659` | 从 `got` 拷贝 name，`def_read_edadb.cpp:249-256` | DB source：`iDesign._design_name` |
| `unitsCallback()` → `parse_units()`，比较 LEF DBU、warning、复用 active units 设置 DBU，`def_read.cpp:661-688` | 同样比较 LEF DBU；只复制 `_micron_dbu` 到 active units，释放 temporary units，`def_read_edadb.cpp:258-272` | DB source：micron DBU；LEF comparison 重新计算 |

`createDbByEdadb()` 首先调用 `readIdbDesign()`：
`src/database/manager/builder/def_builder/def_read_edadb.cpp:208-213`。对应 DEF callbacks 不注册，
因此这四个 active fields 只由 EDADB 恢复：
`src/database/manager/builder/def_builder/def_read_edadb.cpp:70-76`。

## Ownership And Canonicalization

- `readIdbDesign()` 使用 temporary `got`，避免 EDADB 直接把 null inline pointer 覆盖 active object。
- `_units` 按原始 `parse_units()` 复用 active object，只复制 `_micron_dbu`；不替换 pointer。
- `_bus_bit_chars` 按原始 `parse_bus_bit_chars()` 替换 active child；EDADB 已创建新 child，因此直接转移 pointer。
- 空 `VERSION` 在写 DB 前规范化为原始 writer 实际输出的 `5.8`。
- 无有效 DEF DBU 时，写 DB 前规范化为原始 writer 采用的 LEF DBU。

## Validation

回归位置：`src/database/edadb/test/run_idb_roundtrip_regression.sh`。

- `check_design_sql()`：验证 name/version/DBU/delimiter，并确认七个非 DEF `IdbUnits` 列不存在，`run_idb_roundtrip_regression.sh:272-280`。
- `design_fields`：输入 `VERSION 5.7`、`BUSBITCHARS "{}"`、`DESIGN gcd_design`、`UNITS 2000`，验证 DB 与输出 DEF，fixture 生成位于 `run_idb_roundtrip_regression.sh:283-293`。
- `design_fallback`：移除 VERSION/BUSBITCHARS/UNITS，验证 iDB 默认 `5.8`、`[]` 和 LEF DBU `1000` 被写入 EDADB，fixture 生成位于 `run_idb_roundtrip_regression.sh:295-301`。
- 每个 case 均比较 direct DEF roundtrip 与 EDADB roundtrip；主流程位于 `run_idb_roundtrip_regression.sh:506-534`。

本类 acceptance evidence：`design_fields` DB 值为 `gcd_design|5.7|2000|{|}`，`design_fallback` DB 值为 `gcd|5.8|1000|[|]`。全局命令和 suite 结果只维护在 `../adapter-testing.md`。

## Conclusion

当前 Design adapter 已收敛为最小 DEF storage view：direct root mapping、两个 inline value child、无 shadow、无 order 字段、无额外 units 列；write/read 的 fallback、ownership 和 LEF DBU 校验与原始 DEF 流程对应。
