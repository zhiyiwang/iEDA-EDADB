# EDADB iDB Adapter TODO

## Root List Order

目标：EDADB read/write 后，iDB root list 中 `t1, t2, t3` 的顺序应与原始 DEF parser append 顺序一致。不要用 name sort 代替原始顺序。

当前需要后续补充或确认：

| Class / Root List | Why Order Matters | Current State | TODO |
| --- | --- | --- | --- |
| `IdbBlockageList` | DEF writer 按 vector 输出；placement/routing blockage 遍历可能影响工具内部处理顺序。 | 已实现并回归覆盖，缺少详细 order audit 文档。 | 补 `07_idb_blockage.md`，确认是否已有 `_order_sd` 或需要新增。 |
| `IdbGroupList` | DEF writer 按 vector 输出；group member vector 顺序已测试。 | 已实现并回归覆盖，缺少详细 order audit 文档。 | 补 `09_idb_group.md`，确认 root order 和 member order。 |
| `IdbFillList` | DEF writer 按 vector 输出；fill layer/via children 有顺序语义。 | 已实现并回归覆盖，缺少详细 order audit 文档。 | 补 `10_idb_fill.md`，确认 root/child order。 |
| `IdbSpecialNetList` | DEF writer 按 vector 输出；pin refs/wires/segments 有顺序语义。 | 已实现并 routed/optional 回归覆盖，缺少详细 order audit 文档。 | 补 `11_idb_special_net.md`，确认 root/child order。 |
| `IdbNetList` | DEF writer 按 vector 输出；pin refs/wires/segments 有顺序语义。 | 已实现并 routed/optional 回归覆盖，缺少详细 order audit 文档。 | 补 `12_idb_net.md`，确认 root/child order。 |

已满足：

- `IdbDesign`: singleton，无 root list order。
- `IdbDie`: singleton；nested point order 已由 `Shadow<IdbCoordinate<int32_t>>::_vec_idx` 处理。
- `IdbRowList`: root order 已由 `Shadow<IdbRow>::_order_sd` 和 ordered read 保证。
- `IdbTrackGridList`: root identity 使用 `primary_key`，root order 已由 `Shadow<IdbTrackGrid>::_order_sd` 和 ordered read 保证。
- `IdbGCellGridList`: root identity 使用 `primary_key`，root order 已由 `Shadow<IdbGCellGrid>::_order_sd` 和 ordered read 保证。
- `IdbRegionList`: root identity 使用 `Shadow<IdbRegion>::_name_sd`，root order 已由 `_order_sd` 和 ordered read 保证。
- `IdbSlotList`: root identity 使用 `primary_key`，root order 已由 `Shadow<IdbSlot>::_order_sd` 和 ordered read 保证。
