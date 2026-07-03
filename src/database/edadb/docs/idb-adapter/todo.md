# EDADB iDB Adapter TODO

## Demo Branch Scope

`demo/20260703` only enables EDADB read/write for:

- `IdbDesign` / `IdbUnits` / `IdbBusBitChars`
- `IdbDie`
- `IdbRow`
- `IdbTrackGrid`
- `IdbGCellGrid`
- `IdbVia`
- `IdbRegion`
- `IdbSlot`

The following DEF object families are intentionally not persisted through EDADB in this branch and are restored from DEF text callbacks:

- `IdbInstance`
- `IdbPin`
- `IdbBlockage`
- `IdbGroup`
- `IdbFill`
- `IdbSpecialNet`
- `IdbNet`

## Root List Order

Enabled and covered in this branch:

- `IdbDesign`: singleton; no root list order.
- `IdbDie`: singleton; nested point order uses `Shadow<IdbCoordinate<int32_t>>::_vec_idx`.
- `IdbRowList`: root order uses `Shadow<IdbRow>::_order_sd` and ordered read.
- `IdbTrackGridList`: root order uses `Shadow<IdbTrackGrid>::_order_sd` and ordered read.
- `IdbGCellGridList`: Level D; direct no-shadow/no-order; normalized diff handles root order-only differences.
- `IdbViaList`: Level D; direct root identity uses `IdbVia::_name`; no root order field.
- `IdbRegionList`: Level D; direct no-shadow/no-order; boundary rect vector order uses `Shadow<IdbRect>::_vec_idx`.
- `IdbSlotList`: root order uses `Shadow<IdbSlot>::_order_sd`; rect vector order uses `Shadow<IdbRect>::_vec_idx`.
