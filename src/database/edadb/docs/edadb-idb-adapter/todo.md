# EDADB iDB Adapter TODO

## Root List Order

目标：EDADB read/write 后，iDB root list 中 `t1, t2, t3` 的顺序应与原始 DEF parser append 顺序一致。不要用 name sort 代替原始顺序。

当前需要后续补充或确认：

| Class / Root List | Why Order Matters | Current State | TODO |
| --- | --- | --- | --- |
| `IdbGCellGridList` | DEF writer 按 vector 输出；routing grid 通过 vector 遍历使用。 | 未显式实现，依赖 EDADB read order。 | 增加 `_order` 或 shadow，并按 `_order` 恢复。 |
| `IdbRegionList` | references 用 name lookup，但 iPL wrapper 遍历时会产生内部 order/id；DEF writer 按 vector 输出。 | 未显式实现，依赖 EDADB read order。 | 增加 `_order`，不要按 region name 排序。 |

已满足：

- `IdbDesign`: singleton，无 root list order。
- `IdbDie`: singleton；nested point order 已由 `Shadow<IdbCoordinate<int32_t>>::_vec_idx` 处理。
- `IdbRowList`: root order 已由 `Shadow<IdbRow>::_order_sd` 和 ordered read 保证。
- `IdbTrackGridList`: root identity 使用 `primary_key`，root order 已由 `Shadow<IdbTrackGrid>::_order_sd` 和 ordered read 保证。
