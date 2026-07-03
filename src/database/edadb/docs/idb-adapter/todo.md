# EDADB iDB Adapter TODO

## Root List Order

目标：EDADB read/write 后，iDB root list 中 `t1, t2, t3` 的顺序应与原始 DEF parser append 顺序一致。不要用 name sort 代替原始顺序。

当前没有已知 root list order 遗留项。

已满足：

- `IdbDesign`: singleton，无 root list order。
- `IdbDie`: singleton；nested point order 已由 `Shadow<IdbCoordinate<int32_t>>::_vec_idx` 处理。
- `IdbRowList`: root order 已由 `Shadow<IdbRow>::_order_sd` 和 ordered read 保证。
- `IdbTrackGridList`: root identity 使用 `primary_key`，root order 已由 `Shadow<IdbTrackGrid>::_order_sd` 和 ordered read 保证。
- `IdbGCellGridList`: Level D，当前 direct no-shadow/no-order；root order-only diff 由 normalized diff 处理。
- `IdbViaList`: root identity 使用 `IdbVia::_name`；当前使用点主要按 name lookup，未发现 design via root vector index 语义，因此不新增 root shadow/order 字段。
- `IdbInstanceList`: root identity 使用 `Shadow<IdbInstance>::_name_sd`，root order 已由 `_order_sd` 和 ordered read 保证。
- `IdbPins` / IO pin list: root identity 使用 `Shadow<IdbPin>::_pin_name_sd`，root order 已由 `_order_sd` 和 ordered read 保证。
- `IdbBlockageList`: root identity 使用 `primary_key`，root order 已由 `Shadow<IdbBlockage>::_order_sd` 和 ordered read 保证；rect vector order 已由 `Shadow<IdbRect>::_vec_idx` 保证。
- `IdbRegionList`: Level D，当前 direct no-shadow/no-order；root order-only diff 由 normalized diff 处理。
- `IdbSlotList`: root identity 使用 `Shadow<IdbSlot>::primary_key`，root order 已由 `_order_sd` 和 ordered read 保证；rect vector order 已由 `Shadow<IdbRect>::_vec_idx` 保证。
- `IdbGroupList`: root identity 使用 `Shadow<IdbGroup>::_group_name_sd`，root order 已由 `_order_sd` 和 ordered read 保证；member vector order 已由 primitive vector `__edadb_vec_idx` 保证。
- `IdbFillList`: root identity 使用 `primary_key`，root order 已由 `Shadow<IdbFill>::_order_sd` 和 ordered read 保证；rect vector order 已由 `Shadow<IdbRect>::_vec_idx` 保证，coordinate vector order 已由 `Shadow<IdbCoordinate<int32_t>>::_vec_idx` 保证。
- `IdbSpecialNetList`: root identity 使用 `Shadow<IdbSpecialNet>::_net_name_sd`，root order 已由 `_order_sd` 和 ordered read 保证；pin/wire/segment/point vector order 已由 child vector 或 explicit pin ref order 保证。
- `IdbNetList`: root identity 使用 `Shadow<IdbNet>::_net_name_sd`，root order 已由 `_order_sd` 和 ordered read 保证；pin/wire/segment/point vector order 已由 child vector 或 explicit pin ref order 保证。
