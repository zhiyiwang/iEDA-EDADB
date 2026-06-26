# EDADB iDB Adapter TODO

## Root List Order

目标：EDADB read/write 后，iDB root list 中 `t1, t2, t3` 的顺序应与原始 DEF parser append 顺序一致。不要用 name sort 代替原始顺序。

当前需要后续补充或确认：

| Class / Root List | Why Order Matters | Current State | TODO |
| --- | --- | --- | --- |

已满足：

- `IdbDesign`: singleton，无 root list order。
- `IdbDie`: singleton；nested point order 已由 `Shadow<IdbCoordinate<int32_t>>::_vec_idx` 处理。
- `IdbRowList`: root order 已由 `Shadow<IdbRow>::_order_sd` 和 ordered read 保证。
- `IdbTrackGridList`: root identity 使用 `primary_key`，root order 已由 `Shadow<IdbTrackGrid>::_order_sd` 和 ordered read 保证。
- `IdbGCellGridList`: root identity 使用 `primary_key`，root order 已由 `Shadow<IdbGCellGrid>::_order_sd` 和 ordered read 保证。
- `IdbRegionList`: root identity 使用 `Shadow<IdbRegion>::_name_sd`，root order 已由 `_order_sd` 和 ordered read 保证。
