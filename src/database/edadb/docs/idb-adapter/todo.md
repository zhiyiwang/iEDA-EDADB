# Demo Adapter TODO

`demo/20260713` 固定为汇报快照，不在此分支继续实现 fallback families。

未启用 EDADB adapter：

- Pin
- Fill
- SpecialNet
- Net

这些对象由原始 DEF callbacks 读取，并由原始 `DefWrite` 输出。后续开发应回到 `edadb-idb-dev/no-sort-abcd` 或新的开发分支。

保留的实验计划：使用 `PRAGMA reverse_unordered_selects=ON` 扰动无序查询，验证 root order 策略；该测试不在本 demo 中实现。
