# IdbRegion EDADB Adapter Review

## Scope And Constraints

`IdbRegion` 对应 DEF `REGIONS`，root container 是 `IdbRegionList::_region_list`。

- 原始 write：`DefWrite::write_region()`，`src/database/manager/builder/def_builder/def_write.cpp:1040`
- 原始 read：`DefRead::parse_region()`，`src/database/manager/builder/def_builder/def_read.cpp:2093`
- EDADB write：`DefWriteEdadb::writeIdbRegion()`，`src/database/manager/builder/def_builder/def_write_edadb.cpp:515`
- EDADB read：`DefReadEdadb::readIdbRegion()`，`src/database/manager/builder/def_builder/def_read_edadb.cpp:502`

按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 检查：

- root order 等级为 Level D；点工具通过 region name lookup 或遍历使用，没有 root index/order 语义依赖。
- root 不保存 `_order_sd`；`IdbRegion::_name` 是 natural identity 和 table PK。
- nested boundary rectangle vector 必须保序；完整 region record 重排时它随 owner 移动，内部不排序。

## Why Direct Mapping

Region 不需要 root shadow：

- DEF-visible fields 正好是 `_name`、`_type`、`_boudary_list`。
- `_name` 可直接作为 PK，不需要 synthetic identity。
- boundary 是 owned vector，可由 `TABLE4CLASS_WVEC` 和 registered `Shadow<IdbRect>` 表达。
- `_instance_list` 不是 `REGIONS` source field；它由 `COMPONENTS` 中的 region reference 在 Instance adapter 恢复。
- 没有需要 Region shadow 处理的 polymorphism 或 non-owning pointer lookup。

## EDADB Schema

```cpp
TABLE4CLASS_WVEC(idb::IdbRegion, "iRegion",
                 (_name, _type),
                 (_boudary_list));
```

- Region table macro：`src/database/edadb/idb/edadb_idb_schema.h:106`
- Rect table macro：`src/database/edadb/idb/edadb_idb_schema.h:69`
- Rect shadow PK disabled：`src/database/edadb/idb/edadb_idb_init.cpp:27`
- Region table registration：`src/database/edadb/idb/edadb_idb_init.cpp:87`

Primary-key audit：

- `IdbRegion::_name` 使用 EDADB 默认 PK 行为，表达 region identity，不表达 vector order。
- `Shadow<IdbRect>::_vec_idx` 是 owner 内 child order，不是全局 identity，因此 shadow PK 关闭。

## Original DEF Write Mapping

| Original writer brace/order | DEF output | EDADB correspondence | Stored source |
| --- | --- | --- | --- |
| list/null/empty checks and section count, `def_write.cpp:1042-1053` | `REGIONS <N>` | `writeIdbRegion()` reads active root vector, `def_write_edadb.cpp:515-535` | `iRegion` row count |
| root loop and name, `def_write.cpp:1055-1056` | `- <name>` | direct mapping stores `_name`; no root order column | `IdbRegion::_name` → `iRegion._name` |
| boundary loop, `def_write.cpp:1058-1060` | repeated rectangle pairs | `TABLE4CLASS_WVEC` stores the complete `_boudary_list` | child `IdbRectSD` rows + `_vec_idx` |
| type conversion/output, `def_write.cpp:1062-1063` | `+ TYPE FENCE/GUIDE` | direct mapping stores `_type` enum | `IdbRegion::_type` → `iRegion._type` |
| record/section terminators, `def_write.cpp:1065-1068` | `;`, `END REGIONS` | reconstructed from table/vector structure | no DB column |

## Original DEF Read Mapping

| Original parser brace/order | EDADB correspondence | Source / rebuild |
| --- | --- | --- |
| active design/list lookup and `add_region(name)`, `def_read.cpp:2099-2101` | `readIdbRegion()` obtains and resets active list, then EDADB direct-read creates and appends each complete `IdbRegion`, `def_read_edadb.cpp:502-535` | `_name` and root object |
| optional type, `def_read.cpp:2103-2105` | direct mapping restores `_type` | `iRegion._type` |
| rectangle loop, `def_read.cpp:2107-2109` | EDADB nested-vector reader restores boundary children by saved `_vec_idx` | `iRegion__boudary_list_IdbRectSD` |
| property TODO, `def_read.cpp:2111-2112` | no schema field | no implemented iDB state |

## Write And Read Paths

Write：

- `writeIdbRegion()`：`def_write_edadb.cpp:515-542`
- vector access：`def_write_edadb.cpp:528`
- direct insert：`def_write_edadb.cpp:536`
- Empty list is accepted by the EDADB dispatcher so other families can continue.

Read：

- `readIdbRegion()`：`def_read_edadb.cpp:502-541`
- reset active list：`def_read_edadb.cpp:515`
- direct read loop：`def_read_edadb.cpp:517-535`
- read failure resets the list again, avoiding partial active state：`def_read_edadb.cpp:527-531`
- `IdbRegionList::reset()`：`src/database/data/design/db_design/IdbRegion.cpp:130`

Read order places Region before Instance/Group, so those adapters can resolve region names and rebuild Region membership.

## Computed And Cross-Adapter State

- `_name`、`_type`、`_boudary_list` are DEF source fields and are persisted.
- `_instance_list` is not persisted in Region; Instance adapter restores the instance-to-region reference and reverse membership.
- Region property parsing is unimplemented in original iEDA, so it has no active state or EDADB column.
- Region has no post-read geometric cache/bbox rebuild step.

## Order And Point-Tool Evidence

Root `IdbRegionList` is Level D：

- iPL wraps regions by iteration, `src/operation/iPL/source/module/wrapper/IDBWrapper.cc:706`.
- Region membership is subsequently resolved by name map, for example `src/operation/iPL/source/module/detail_placer/DetailPlacer.cc:206` and `src/operation/iPL/source/module/detail_placer/database/DPLayout.cc:58`.
- Other region traversals accumulate geometry/area and do not consume root index, for example `src/operation/iPL/source/PlacerDB.cc:477` and `src/operation/iPL/source/module/filler/src/MapFiller.cpp:35`.
- iPL assigns an insertion-order `_region_id`, but the current audit found no algorithm consuming that ID; therefore the highest applicable level remains D.

Consequences：

- `iRegion` has no `_order_sd` and read uses no root `ORDER BY`.
- Normalized diff may sort complete `REGIONS` root records by name.
- `_boudary_list` remains nested-order-sensitive and is never normalized independently.

## Tests

- `sky130_gcd` covers empty Region handling.
- `aux_optional` contains one named FENCE region with two boundary rectangles.
- Regression physically reverses the SQLite boundary child rows, verifies their rowid order is reversed, then confirms EDADB restores `_vec_idx` order and final DEF exactly matches the direct baseline.
- SQL checks region count/name/type, both ordered boundary values, and physical child-row disorder.

## Risks / TODO

- Original writer rejects an empty list while EDADB dispatcher treats it as success.
- Root row order may differ because Region is Level D; raw order-only differences are handled by normalized diff.
- If point tools begin consuming insertion-derived region IDs, the order level must be re-audited before changing the adapter.
- If original parser implements region properties, schema/read/write/tests must be extended together.
