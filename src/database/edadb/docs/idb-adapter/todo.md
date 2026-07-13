# EDADB iDB Adapter TODO

## No-Sort ABCD Experiment

`edadb-idb-dev/no-sort-abcd` 以 `b7d5a72c4` 为 baseline，刻意让 A/B/C/D 四级 design root list 全部不保存顺序：

- root schema 不定义 `_order_sd`；read path 不使用 root `ORDER BY`。
- name 或 synthetic `primary_key` 仍负责 object identity 和 child ownership。
- nested vector 不参与本实验，继续使用 EDADB child-vector index、`_vec_idx` 或局部 pin-ref `_order_sd` 保序。
- DEF raw diff 失败时，normalizer 可重排 A/B/C/D 完整 root record；record 内部内容不排序。

当前已确认无 root `_order_sd`：`IdbRowList`、`IdbTrackGridList`、`IdbGCellGridList`、`IdbViaList`、`IdbInstanceList`、`IdbPins`、`IdbBlockageList`、`IdbRegionList`、`IdbSlotList`、`IdbGroupList`、`IdbFillList`、`IdbSpecialNetList`、`IdbNetList`。

后续实验：

- 使用 `PRAGMA reverse_unordered_selects=ON` 强制扰动无序查询。
- 分别运行依赖 Row/Pin/Instance/Net 的点工具，比较错误、物理实现和中间状态差异。
- 与 `edadb-idb-dev/sort-abc-no-sort-d` 在同一 fixture、seed 和工具版本下对比。
