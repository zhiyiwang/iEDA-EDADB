# SQLite / EDADB性能测试

比较同一组8字段合成数据的五条读写路线。时间单位为ms，建表与数据读写分开统计。

## 阅读顺序

| 顺序 | 文件 | 内容 |
| --- | --- | --- |
| 1 | [test_plan.md](test_plan.md) | 数据、实验矩阵、计时与比较规则 |
| 2 | [sqlite_config.md](sqlite_config.md) | SQLite配置和事务控制组 |
| 3 | [implementation.md](implementation.md) | 五条路线的源码行号、伪代码、计时边界 |
| 4 | [experiment_report.md](experiment_report.md) | 编译运行命令、数据路径、实测结果与分析 |

源码从[stream_benchmark.cpp](stream_benchmark.cpp)开始；辅助函数见[benchmark_support.h](benchmark_support.h)，
数据生成见[fixtures.py](fixtures.py)，运行与统计见[run_stream.py](run_stream.py)，结果复核见[audit_stream.py](audit_stream.py)。
SQLite机制按需查[sqlite_runtime.md](sqlite_runtime.md)。

## 结果入口

本页全部结果来自同一个完整批次，包括adapter，不拼接专项数据。接续分析见[handoff.md](handoff.md)。

正式批次2026-09-10（19:42–21:00）：99组正确性检查通过，540条计时样本，108个分组各5次。

- [report.md：统计表及原始计时](/tmp/iedadb_stream_full_20260910/report.md)
- [samples.tsv：逐次时间和日志路径](/tmp/iedadb_stream_full_20260910/samples.tsv)
- [summary.tsv：均值、中位数、最小值、最大值](/tmp/iedadb_stream_full_20260910/summary.tsv)
- [datasets.json：输入路径、字节数、哈希](/tmp/iedadb_stream_full_20260910/datasets.json)
- [manifest.json：版本、硬件、二进制和源码哈希](/tmp/iedadb_stream_full_20260910/manifest.json)
- [checks.json：正确性](/tmp/iedadb_stream_full_20260910/checks.json) / [audit.json：结果审计](/tmp/iedadb_stream_full_20260910/audit.json)

实测值以manifest对应的二进制与source快照为准。Git只保存代码、配置说明、方法与实验结论；
可重新生成的输入、DB、日志和统计产物不提交。以上/tmp链接仅在运行机器有效，清理后需重新运行，
时间不会逐次完全相同。需要保留原始证据时另行备份整个输出目录；编译和运行命令只在实验报告维护。
