# IdbBlockage EDADB Adapter Review

## Scope And Constraints

`IdbBlockage` 对应 DEF `BLOCKAGES`，root container 是 `IdbBlockageList::_blockage_list`。

- 原始 write：`DefWrite::write_blockage()`，`src/database/manager/builder/def_builder/def_write.cpp:588`
- 原始 read：`DefRead::parse_blockage()`，`src/database/manager/builder/def_builder/def_read.cpp:1955`
- EDADB write：`DefWriteEdadb::writeIdbBlockage()`，`src/database/manager/builder/def_builder/def_write_edadb.cpp:430`
- EDADB read：`DefReadEdadb::readIdbBlockage()`，`src/database/manager/builder/def_builder/def_read_edadb.cpp:790`

按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 检查：

- Root order 是 Level D；不保存 `_order_sd`，不依赖 SQLite root-row 顺序。
- Blockage record 无 name；synthetic `primary_key` 只表示 anonymous root identity。
- Nested `_rect_list` 必须保序；`Shadow<IdbRect>::_vec_idx` 保存 child index。

## Why Shadow Is Required

`IdbBlockage` 是 polymorphic base，DEF record 对应 `IdbRoutingBlockage` 或 `IdbPlacementBlockage`：

- `_type_sd` 驱动 builder 创建正确派生类。
- Layer/instance 是 non-owning pointer；DB 保存 name，`fromShadow()` 从 active layout/design lookup。
- `_rect_list_sd` 是 owned child storage view。
- Routing 与 placement 拥有不同 DEF source fields，direct base-class mapping 无法完整表达。

## EDADB Schema

```cpp
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbBlockage>, "iBlockageSD",
                 (primary_key, _instance_name_sd, _is_pushdown_sd, _type_sd,
                  _layer_name_sd, _min_spacing_sd, _effective_width_sd,
                  _is_slots_sd, _is_fills_sd, _is_except_pgnet_sd,
                  _is_soft_sd, _max_density_sd),
                 (_rect_list_sd));
```

- Schema：`src/database/edadb/idb/edadb_idb_schema.h:103`
- Shadow：`src/database/edadb/idb/shadow/shadow_idb_blockage.h:22`
- Table registration：`src/database/edadb/idb/edadb_idb_init.cpp:86`
- Nested Rect shadow PK disabled：`src/database/edadb/idb/edadb_idb_init.cpp:27`

Persisted fields：

- Common：type、instance name、pushdown、ordered rects。
- Routing：layer name、slots、fills、except-pg-net、minimum spacing、effective width。
- Placement：soft、maximum density。
- Not persisted：resolved pointers、polygon、`_is_partial`。原始 parser 未设置 `_is_partial`，不能由 adapter 猜测。

## Original DEF Write Mapping

| Original writer brace/order | DEF output | EDADB correspondence | Stored source |
| --- | --- | --- | --- |
| list checks/root loop，`def_write.cpp:590-603` | `BLOCKAGES <N>` | `writeIdbBlockage()` batch-converts/inserts shadows，`def_write_edadb.cpp:430-477` | root rows，无 root order |
| routing branch，`def_write.cpp:604-619` | `LAYER`，optional `PUSHDOWN/EXCEPTPGNET/COMPONENT` | `toShadow()` routing branch，`shadow_idb_blockage.h:62-72` | type、layer、pushdown、except-pg-net、instance name |
| routing rect loop，`def_write.cpp:621-623` | repeated `RECT` | common rect copy，`shadow_idb_blockage.h:54-59` | `_rect_list_sd` + `_vec_idx` |
| placement branch，`def_write.cpp:624-635` | `PLACEMENT`，optional `PUSHDOWN/COMPONENT` | common fields plus placement branch，`shadow_idb_blockage.h:73-79` | type、pushdown、instance name |
| placement rect loop，`def_write.cpp:637-639` | repeated `RECT` | common rect copy | `_rect_list_sd` + `_vec_idx` |

`toShadow()` 同时保存下表中的 parser-only source fields；原始 writer 不输出它们，不能只靠最终 DEF diff 验证。

## Original DEF Read Mapping

| Original parser brace/order | EDADB correspondence | Source / rebuild |
| --- | --- | --- |
| active lists，`def_read.cpp:1961-1965` | builder 获取并 reset active blockage list，`def_read_edadb.cpp:790-805` | allocation context |
| routing factory/layer，`def_read.cpp:1967-1970` | builder 按 `_type_sd` 创建 routing subtype；`fromShadow()` 按 layer name lookup，`def_read_edadb.cpp:819-830`、`shadow_idb_blockage.h:91-98` | DEF `LAYER` + non-owning pointer rebuild |
| `hasSlots()`，`def_read.cpp:1972-1974` | `set_slots(_is_slots_sd)`，`shadow_idb_blockage.h:99` | parser-only DEF `SLOTS` source |
| `hasFills()`，`def_read.cpp:1976-1978` | `set_fills(_is_fills_sd)`，`shadow_idb_blockage.h:100` | parser-only DEF `FILLS` source |
| `hasPushdown()`，`def_read.cpp:1980-1982` | `set_pushdown(_is_pushdown_sd)`，`shadow_idb_blockage.h:101` | DEF `PUSHDOWN` source |
| `hasExceptpgnet()`，`def_read.cpp:1984-1986` | `set_except_pgnet(_is_except_pgnet_sd)`，`shadow_idb_blockage.h:102` | DEF `EXCEPTPGNET` source |
| routing component，`def_read.cpp:1988-1992` | restore name then instance lookup，`shadow_idb_blockage.h:103-106` | DEF `COMPONENT`; lookup may remain null like original parser |
| routing spacing，`def_read.cpp:1994-1996` | `set_min_spacing(_min_spacing_sd)`，`shadow_idb_blockage.h:107` | parser-only DEF `SPACING` source |
| routing width，`def_read.cpp:1998-2000` | `set_effective_width(_effective_width_sd)`，`shadow_idb_blockage.h:108` | parser-only DEF `DESIGNRULEWIDTH` source |
| routing rect loop，`def_read.cpp:2002-2004` | append ordered rect children，`shadow_idb_blockage.h:129-132` | repeated `RECT` source |
| placement factory，`def_read.cpp:2008-2010` | builder 按 `_type_sd` 创建 placement subtype | placement root allocation |
| `hasSoft()`，`def_read.cpp:2012-2014` | `set_soft(_is_soft_sd)`，`shadow_idb_blockage.h:116` | parser-only DEF `SOFT` source |
| `hasPartial()`，`def_read.cpp:2016-2018` | `set_max_density(_max_density_sd)`，`shadow_idb_blockage.h:117` | parser-only DEF `PARTIAL <density>`；原 parser 不设置 `_is_partial` |
| placement component，`def_read.cpp:2020-2024` | restore name then instance lookup，`shadow_idb_blockage.h:118-121` | DEF `COMPONENT` source |
| placement rect loop，`def_read.cpp:2026-2028` | append ordered rect children，`shadow_idb_blockage.h:129-132` | repeated `RECT` source |

## Known Native Writer/Parser Differences

- Writer 不输出 routing `SLOTS/FILLS/SPACING/DESIGNRULEWIDTH` 或 placement `SOFT/PARTIAL`；adapter 仍保存这些 parser source fields。
- Writer 能输出 placement `PUSHDOWN`，但 parser placement branch 不读取它。Adapter 保存 active `_is_pushdown`，但无法从已被 parser 丢弃的 DEF token 推断 true。
- Parser 对 `PARTIAL` 只调用 `set_max_density()`，不设置 `_is_partial`；schema 不增加虚假的 `_is_partial_sd`。
- DEF grammar 还支持 `MASK`，但当前 `parse_blockage()`、iDB class 和 writer 都不保留它；schema 不推测增加 `_mask_sd`。
- Polygon 在原始 parser 中仍为 TBD，不进入 schema。

## Order And Tests

- `aux_optional` fixture 使用 7 条合法 records，覆盖 routing slots/fills/pushdown/except/component/spacing/width 和 placement soft/partial/component/pushdown。
- SQL 检查所有 persisted source fields，并断言 schema 不存在 `_is_partial_sd` / `_mask_sd`。
- Regression 反转 Rect child table 的物理 row order，验证 `_vec_idx` 恢复 nested order。
- `readIdbBlockage()` debug state 证明 parser-only fields 已由 `fromShadow()` 写回 active iDB。
- Direct DEF 与 EDADB DEF 完全一致；writer-omitted fields 的正确性由 SQL + read-state 日志证明。

已验证命令：

```bash
OUT_DIR=/tmp/iedadb_blockage_fields EDADB_TEST_JOBS=1 \
  bash src/database/edadb/test/run_idb_roundtrip_regression.sh aux_optional
```
