# EDADB iDB Adapter TODO

## Demo Branch Scope

`demo` 分支只启用以下 EDADB read/write：

- `IdbDesign`
- `IdbDie`
- `IdbRow`
- `IdbTrackGrid`
- `IdbGCellGrid`
- `IdbRegion`
- `IdbSlot`

其余类在本分支保持原始 DEF fallback：

- `IdbVia`
- `IdbInstance`
- `IdbPin`
- `IdbBlockage`
- `IdbGroup`
- `IdbFill`
- `IdbSpecialNet`
- `IdbNet`

## Root List Order

已启用类的 root order 状态：

- `IdbDesign`: singleton，无 root list order。
- `IdbDie`: singleton；nested point order 已由 `Shadow<IdbCoordinate<int32_t>>::_vec_idx` 处理。
- `IdbRowList`: root order 已由 `Shadow<IdbRow>::_order_sd` 和 ordered read 保证。
- `IdbTrackGridList`: root identity 使用 `primary_key`，root order 已由 `Shadow<IdbTrackGrid>::_order_sd` 和 ordered read 保证。
- `IdbGCellGridList`: root identity 使用 `primary_key`，root order 已由 `Shadow<IdbGCellGrid>::_order_sd` 和 ordered read 保证。
- `IdbRegionList`: root identity 使用 `Shadow<IdbRegion>::_name_sd`，root order 已由 `_order_sd` 和 ordered read 保证。
- `IdbSlotList`: root identity 使用 `primary_key`，root order 已由 `Shadow<IdbSlot>::_order_sd` 和 ordered read 保证。

## Remaining Work

后续开发分支继续逐类恢复：

1. `IdbVia`
2. `IdbInstance`
3. `IdbPin`
4. `IdbBlockage`
5. `IdbGroup`
6. `IdbFill`
7. `IdbSpecialNet`
8. `IdbNet`

每恢复一个类，都要重新做原始 `DefWrite` / `DefRead` 对齐、schema/init 同步、DEF callback 切换、SQL 验证和文档更新。
