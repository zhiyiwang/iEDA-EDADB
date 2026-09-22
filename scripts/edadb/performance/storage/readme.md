# SQLite性能实验：入口

本目录按实验和共用资料组织，不按运行日期分组。

会议汇报先读[四组已冻结实验汇总](report.md)：方法、数据、计时边界、结果与实验之间的关系。

当前验收及提交范围见[performance交接](../handoff.md#本轮收尾与提交边界)；生产优化方案不属于本轮基线交付。

```text
storage/
├── readme.md
├── baseline/            五路线整体性能
│   ├── readme.md        计划、运行、接续事项
│   ├── results.md       基线结果与证据
│   ├── run.py
│   ├── audit.py
│   └── fixtures.py
├── sqlite-vs-edadb/     SQLite与EDADB额外操作对照
│   ├── readme.md        阅读入口与文件职责
│   ├── alignment.md     当前SQL/API对齐方法、源码对应与运行
│   ├── results.md       当前对齐实验结果与分析
│   ├── alignment.cpp    当前独立测试程序
│   ├── CMakeLists.txt
│   ├── run_alignment.py
│   ├── audit_alignment.py
│   ├── archive/initial_checks.md 首轮结果归档
│   ├── run_read.py
│   └── run_write.py
├── sqlite-reference/       共用SQLite参数与机制
│   ├── config.md
│   ├── runtime.md
│   ├── execution_flow.md SQL准备、VM执行、存储与返回
│   ├── btree_storage.md rowid、字符串主键、B-tree页面与缺页读取
│   └── execution_analysis.md 执行计划、统计接口及计时限制
├── sqlite-vs-text/      本轮归因、SELECT补充及perf审计完成；已冻结
│   ├── readme.md
│   └── docs/            test_plan.md、results.md、sqlite_source.md
├── sqlite-pk-fk/        两表PK/FK与索引实验
│   ├── readme.md        导航与运行命令
│   ├── docs/            test_plan.md、implementation.md、results.md
│   ├── pk_fk_benchmark.cpp
│   ├── run_pk_fk.py
│   └── audit_pk_fk.py
├── sqlite-index/        单表主键、索引与布局；完整测试通过，已冻结
│   ├── readme.md
│   ├── docs/            test_plan.md、results.md
│   ├── benchmark.cpp
│   ├── run.py
│   └── summarize.py
└── benchmark/           共用测试程序
    ├── implementation.md
    ├── stream_benchmark.cpp
    ├── benchmark_support.h
    └── CMakeLists.txt
```

## 阅读顺序

| 目的 | 顺序与要点 |
| --- | --- |
| 开会汇报 | [基线结果](baseline/results.md) → [额外开销结果](sqlite-vs-edadb/results.md)：先说谁快，再解释已验证原因；不要跨批次相减 |
| 理解测试 | [基线计划](baseline/readme.md)或[对照计划](sqlite-vs-edadb/readme.md) → [参数](sqlite-reference/config.md)：检查schema、规模、改变的变量与计时边界 |
| 审查代码 | [实现说明](benchmark/implementation.md) → [Record/schema](benchmark/benchmark_support.h#L111) → [main计时](benchmark/stream_benchmark.cpp#L138) → [读写API](benchmark/stream_benchmark.cpp#L49) |
| 运行与复核 | 对应实验readme → run脚本 → 输出目录的samples、统计、正确性/审计文件；先查正确性再看性能 |
| 查询SQLite机制 | [运行机制及官方链接](sqlite-reference/runtime.md)，不必作为必读前置 |
| 理解主外键实验 | [结果](sqlite-pk-fk/docs/results.md) → [方案](sqlite-pk-fk/docs/test_plan.md) → [实现](sqlite-pk-fk/docs/implementation.md)：先分开索引、FK检查和对象恢复 |
| 理解SQL如何执行 | [SQL到VM及存储流程](sqlite-reference/execution_flow.md)：先准备，再分别看写入、读取和底层页面 |
| 理解主键与页面 | [B-tree存储结构](sqlite-reference/btree_storage.md)：rowid不等于声明主键；区分rowid表与WITHOUT ROWID |
| 分析SQLite执行 | [执行分析接口及限制](sqlite-reference/execution_analysis.md) → [当前实验方案](sqlite-vs-text/readme.md)：先区分计划、计数、时间，再决定实验 |

## 源码职责

- [baseline/fixtures.py](baseline/fixtures.py)生成最小LEF和DEF；[generate()](benchmark/benchmark_support.h#L133)生成内存记录。
- [baseline/run.py](baseline/run.py)运行五路线，[baseline/audit.py](baseline/audit.py)核验原始时间、统计及输入哈希。
- [sqlite-vs-edadb/run_read.py](sqlite-vs-edadb/run_read.py)比较NULL检查，[sqlite-vs-edadb/run_write.py](sqlite-vs-edadb/run_write.py)比较绑定API和clear。
- [benchmark/CMakeLists.txt](benchmark/CMakeLists.txt)构建共用stream_benchmark；[对齐实验CMakeLists.txt](sqlite-vs-edadb/CMakeLists.txt)构建独立alignment。当前对齐实验的调用链见[alignment.md](sqlite-vs-edadb/alignment.md)，不移动core或adapter。

## 已有结果与保存规则

- 基线：99组正确性、540条计时；[完整结果](baseline/results.md)。
- 当前SQL/API对齐：40组正确性、100条计时；[结果](sqlite-vs-edadb/results.md)。首轮读写补充的16组正确性、80条计时仅见[归档](sqlite-vs-edadb/archive/initial_checks.md)，不混算。
- 对齐实验readme只做导航，方法/运行放alignment，数字/分析放results；共用参数只在sqlite-reference维护。冻结基线的文档分工不变。
- 输入、DB、二进制、日志、统计产物放仓库外；原始证据链接指向本机/tmp，不是永久备份。Python缓存不阅读、不提交。
- 目录迁移已通过Release -O3构建、76组小规模正确性检查及46条采样；仅验证入口与统计，不替代历史性能结果。C++计时实现未改动。
- 迁移验证证据：[基线审计](../../../../../../../../../../tmp/iedadb_reorg_baseline_smoke/audit.json)、[读对照检查](../../../../../../../../../../tmp/iedadb_reorg_read_smoke/checks.json)、[写对照检查](../../../../../../../../../../tmp/iedadb_reorg_write_smoke/checks.json)。构建仍有既有LEF/DEF依赖的ODR警告，未在本次目录整理中修改生产代码。
