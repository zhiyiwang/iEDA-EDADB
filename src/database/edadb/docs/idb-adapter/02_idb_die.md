# IdbDie EDADB Adapter Review

## Scope And Constraints

`IdbDie` 对应 DEF singleton `DIEAREA`：

- Root：`IdbLayout::_die`
- DEF source：`IdbDie::_points`
- Nested element：`IdbCoordinate<int32_t>::x/y`

本实现按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 检查：

- `IdbDie` 是 Level D singleton，不存在 root vector，不保存 `_order_sd`。
- `IdbDie::_points` 是 deeper nested vector，点序属于 polygon 语义，必须保存。
- Bounding box、`_polygon` 和 `_area` 是 read 后派生状态，不进入 EDADB。

## EDADB Schema

```cpp
TABLE4SHADOW(idb::IdbCoordinate<int32_t>);
TABLE4CLASS(edadb::Shadow<idb::IdbCoordinate<int32_t>>, "iCoordSD",
            (_vec_idx, _x_sd, _y_sd));

TABLE4SHADOW_WVEC(idb::IdbDie);
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbDie>, "iDieSD",
                 (primary_key), (points_sd));
```

代码位置：

- Schema：`src/database/edadb/idb/edadb_idb_schema.h:32-38`
- Coordinate PK disabled：`src/database/edadb/idb/edadb_idb_init.cpp:21-26`
- Die root registration：`src/database/edadb/idb/edadb_idb_init.cpp:69-77`
- Shadow：`src/database/edadb/idb/shadow/shadow_idb_die.h:14-64`

Primary-key / order 结论：

- `IdbDie` 没有 DEF name/ID，但 `points_sd` child table 需要 owner identity，因此保留 synthetic `primary_key`。
- DEF 只允许一个 active die，owner key 固定为 `1`；它不是 order index。
- Coordinate 是 nested vector element，不需要独立 identity，因此关闭 PK。
- `_vec_idx` 只恢复 point vector 顺序，不能作为 PK。

## Why Shadow Is Required

Direct `IdbDie` mapping 无法同时表达：

- singleton die 没有可用作 child owner 的原始成员；
- 只保存 DEF-visible `_points`，排除 bbox/area/polygon cache；
- 每个 coordinate 需要额外 `_vec_idx`，而原始 `IdbCoordinate` 没有该成员。

因此 `Shadow<IdbDie>` 提供 root owner，`Shadow<IdbCoordinate<int32_t>>` 提供有序 point storage view。这不是为了 root 排序。

## Original DEF Write Mapping

| Original writer brace | DEF output | EDADB correspondence | Stored source |
| --- | --- | --- | --- |
| `write_die()` 获取 `layout->get_die()` 并检查 null，`def_write.cpp:340-348` | `DIEAREA` root | `writeIdbDie()` 获取同一 singleton，`def_write_edadb.cpp:192-198` | `iDieSD.primary_key=1` 仅作 child owner |
| 写出 `DIEAREA` 前缀，`def_write.cpp:350` | `DIEAREA ` | `Shadow<IdbDie>::toShadow()` 建立 non-owning point view，`shadow_idb_die.h:22-39` | `points_sd` |
| 按 `die->get_points()` 顺序写每个 `(x y)`，`def_write.cpp:352-354` | `( x y )` | EDADB 遍历 vector，并把 index 传给 coordinate `toShadow()` | `_vec_idx/_x_sd/_y_sd` |
| 写 terminator，`def_write.cpp:356-359` | `;` | bbox、area、polygon 不入库 | 无 derived columns |

Adapter 会检查 `toShadow()` 返回值并传播失败，随后插入一个 die shadow：
`src/database/manager/builder/def_builder/def_write_edadb.cpp:200-212`。

## Original DEF Read Mapping

| Original parser brace | EDADB correspondence | Source / synchronization / calculation |
| --- | --- | --- |
| `dieAreaCallback()` 检查 input/type 后调用 `parse_die()`，`def_read.cpp:691-707` | `readIdbDie()` 检查 active die，读取一个 `iDieSD` row，`def_read_edadb.cpp:293-307` | Root singleton lookup |
| `parse_die()` 取得 `defiPoints`，按输入顺序循环 `add_point(x,y)`，`def_read.cpp:709-724` | EDADB 根据 coordinate shadow 返回的 `_vec_idx` 放回 vector slot；`fromShadow()` 再按 vector 顺序调用 `add_point(point)`，`shadow_idb_die.h:42-56` | DB source：x/y/order；`add_point()` 同步重建 `_polygon` |
| points 完成后调用 `set_bounding_box()`，`def_read.cpp:726` | builder 在 `fromShadow()` 后显式调用同一函数，`def_read_edadb.cpp:309-314` | bbox 从 points 重新计算，不存储 |

读取 DB 成功前不清空 active die；只有 root row 完整读出后才 `reset()`，避免查询失败先破坏旧 points。

## Child Storage And Ownership

- Write：`points_sd` non-owning 引用 active die points；insert 同步完成，不深拷贝。
- EDADB：对每个 coordinate 调用标准 `toShadow(point, &idx)`，保存 `_vec_idx/x/y`。
- Read：EDADB 分配 coordinate，并通过标准 `fromShadow(point, &idx)` 按 `_vec_idx` 放入 `points_sd`。
- `Shadow<IdbDie>::fromShadow()` 检查 active points 为空及 child 非空，再把 pointer ownership 转移给 active die；随后清空 `points_sd`，避免重复释放。
- `add_point()` 同步 append `_points/_polygon`；builder 最后重建 bbox。

## Validation

回归位置：`src/database/edadb/test/run_idb_roundtrip_regression.sh`。

- Polygon fixture：六点 L-shaped `DIEAREA`，`run_idb_roundtrip_regression.sh:317-327`。
- DB perturbation：按 `_vec_idx DESC` 重新插入 child rows，使 SQLite 实际 fetch 顺序成为 `5,4,3,2,1,0`，`run_idb_roundtrip_regression.sh:532-541`。
- SQL assertions：固定 owner key、逆序物理 fetch、按 `_vec_idx` 的完整坐标序列，以及 `_vec_idx` 非 PK，`run_idb_roundtrip_regression.sh:283-295`。
- 最终比较 direct DEF roundtrip 与 EDADB roundtrip，证明完整 parent block 随 nested points 正确重建。

验证结果：

- `cmake --build build -j40 --target db_edadb def_builder iEDA`：通过。
- `OUT_DIR=/tmp/iedadb_die_converged bash src/database/edadb/test/run_idb_roundtrip_regression.sh`：全部 case 通过。
- Polygon 输出保持：`(0,0) → (149960,0) → (149960,75064) → (75000,75064) → (75000,150128) → (0,150128)`。

## Known Limitation

同一进程连续调用两次 `edadb_read` 当前会在 `EdadbIdbHelper::setIdbDefService()` 被拒绝，因此 repeated-read lifecycle 未纳入本类测试。另需注意原始 `IdbDie::reset()` 只清 `_points`，不清 `_polygon/_area`；如果未来允许同一 layout 重复 EDADB restore，需要先统一该 lifecycle 与 derived-cache reset 语义。

## Conclusion

当前 Die adapter 保存最小 DEF storage view：一个固定 owner row 和有序 coordinate children；root 无 order，nested `_vec_idx` 必需，bbox/polygon/area 按原始 parser 路径重建。
