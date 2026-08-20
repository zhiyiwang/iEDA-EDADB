# iDB Adapter Class Index

本目录只保存逐类 adapter 审计结论。统一规则、测试方法和全局状态不在这里重复。

## Canonical References

- [开发与审查规则](../adapter-development-rules.md)
- [DEF/iDB 映射与顺序策略](../def-ieda-mapping-and-order.md)
- [测试方法、命令与结果](../adapter-testing.md)
- [架构与代码阅读顺序](../EDADB_DEF_READ_WRITE_ONBOARDING.md)

## Class Review Order

1. [IdbDesign](01_idb_design.md)
2. [IdbDie](02_idb_die.md)
3. [IdbRow](03_idb_row.md)
4. [IdbTrackGrid](04_idb_track_grid.md)
5. [IdbGCellGrid](05_idb_gcell_grid.md)
6. [IdbVia](06_idb_via.md)
7. [IdbInstance](07_idb_instance.md)
8. [IdbPin](08_idb_pin.md)
9. [IdbBlockage](09_idb_blockage.md)
10. [IdbRegion](10_idb_region.md)
11. [IdbSlot](11_idb_slot.md)
12. [IdbGroup](12_idb_group.md)
13. [IdbFill](13_idb_fill.md)
14. [IdbSpecialNet](14_idb_special_net.md)

`demo/20260814` 启用上述 14 类的 EDADB read/write。普通 `IdbNet` 使用原始 DEF fallback，因此本 demo 不保留 Net adapter 文档。
