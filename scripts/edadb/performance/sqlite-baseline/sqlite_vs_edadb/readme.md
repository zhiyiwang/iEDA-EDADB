# SQLite与EDADB：额外开销对照

## 计划

只比较直接SQLite API与EDADB API，不经过adapter。复用[基线的8字段Component](../baseline/readme.md)及[A/B-batch配置](../sqlite_params/config.md)，固定1,000,000条；每组独立正确性/预热1次、正式5次，串行并轮换顺序，write/read之间等待1秒。

| 实验 | 控制组 | 唯一关注的变化 |
| --- | --- | --- |
| 读 | 原SQLite、SQLite加NULL检查、EDADB | SQLite增加逐列类型检查，取值和消费不变 |
| 写 | 原SQLite、仅clear、仅匹配绑定API、同时匹配并clear、EDADB | 绑定方式与参数清理的2×2对照 |

- read：prepare SELECT → step/取列/赋值/消费 → finalize。
- write：prepare INSERT → bind/step/[clear]/reset → finalize；建表、BEGIN、COMMIT、打开关闭单列。
- 性能只消费字段与摘要，独立check才逐字段比较并检测同长度损坏。配置、校验、缓存准备不进入数据计时。
- A读是同连接first-read；B-batch读是文件OS-warm、新连接。不存在本实验的设备冷缓存结论。
- 只在同一批次内相减；测得的是净增量，不是函数独占耗时，不从墙钟反推指令数。
- 所有字段在本例均非NULL。当前通用读取跳过NULL非指针标量赋值，可能留下旧值；本实验不评价该行为是否适合所有应用。
- 对照只修改测试分支；EDADB core和adapter不变。两边均复制字符串、复用Record，无Shadow、子表或N+1。

## 运行

先按[构建说明](../benchmark/implementation.md)生成可执行文件；两个实验顺序运行，不并行竞争资源。输出目录必须不存在：

```bash
cd "$(git rev-parse --show-toplevel)"
python3 scripts/edadb/performance/sqlite-baseline/sqlite_vs_edadb/run_read.py \
  /tmp/iedadb_benchmark_build/stream_benchmark /tmp/iedadb_read_control
python3 scripts/edadb/performance/sqlite-baseline/sqlite_vs_edadb/run_write.py \
  /tmp/iedadb_benchmark_build/stream_benchmark /tmp/iedadb_write_control_new
```

默认保持1,000,000条和5次正式采样。快速验证可追加`--count 1000 --runs 1 --settle 0`并使用新目录；这只验证迁移后的运行，不替代历史性能结果。

输出`samples.tsv`、`summary.json`、`checks.json`、逐次日志、源码快照和编译参数。原始完整对照的结论与证据统一见[results.md](results.md)，不重新维护多份时间表。

## 实现入口与接续

[写入控制循环](../benchmark/stream_benchmark.cpp#L67)、[读取控制循环](../benchmark/stream_benchmark.cpp#L101)；共用调用链见[benchmark/implementation.md](../benchmark/implementation.md)。
`STREAM_NULL_CHECK`、`STREAM_MATCH_BINDINGS`、`STREAM_CLEAR_BINDINGS`按环境变量是否存在启用，设为0仍启用；runner负责先清除继承值再按组设置。

剩余成本尚未独立量化。任何去除NULL检查/clear的优化，必须先验证可空字段、部分绑定、失败后复用和字符串生命周期。是否采集阶段内instructions/cycles需另行讨论。
