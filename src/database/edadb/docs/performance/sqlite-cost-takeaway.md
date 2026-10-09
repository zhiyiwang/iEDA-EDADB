# 无P2 profiling：takeaway

保留P1/P3/P4/P5，P2从源码移除；重新构建、验证、测量，不沿用旧P2关闭批次。[完整结果](sqlite-cost-results.md)。

## 读取

- 压力用例read data平均 **1292.925 ms**，Net占 **98.65%**。
- **448,548次SELECT**，其中 **447,699次Net后代查询**；关键子表已有父键索引，不是反复全表扫描。
- Net只prepare 8次，重复的是父键绑定、索引查找、取列和收尾；step占read data用户态CPU样本 **64.24%**，不是墙钟占比。
- **建议下一步验证P2减少查询次数**，不是先取消FK或继续加同类索引。尚未量化可省掉的毫秒数，不能把全部Net时间称为N+1成本。

## 写入

- create平均 **88.029 ms**，与write data **2485.087 ms**分开；数据COMMIT是data子项，平均 **688.638 ms（27.71%）**。
- **449,214次INSERT**；step占write data用户态CPU样本 **81.09%**，含VM、B-tree及记录比较，不是纯调用开销。
- 提交前有 **11,039次缓存溢出写页**；I/O诊断显示插入期间及COMMIT均有同步。移出COMMIT不能排除全部I/O。
- **后续分别验证缓存容量、多行INSERT**，保持同步/约束保障，比较data＋COMMIT与内存；本轮未实施。

## 验证与边界

15用例、两类失败整体回滚及重试、40次严格往返均通过；40条正式命令时间、114组各5次，12份perf采样无丢样。[审计](../../test/sqlite_cost/results/profile-only/audit.json)。

结论限于热OS缓存的filler和合成routed压力fixture；没有精确拆出N+1、FK或对象重建的独占毫秒，也没有宣称已快于原始iEDA。milestone不动。
