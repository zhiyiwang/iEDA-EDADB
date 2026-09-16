# 首轮NULL与绑定对照：结果归档

仅用于追溯首轮两个独立批次；当前汇报请看[当前结果](../results.md)，不要混合计算delta。

内容范围：SQLite增加NULL检查、绑定清理对照及单条成本分析。原始五路线比较见[baseline/results.md](../../baseline/results.md)。

## EDADB为何比直接SQLite慢：补充对照

**读取主要差在逐列NULL检查；写入部分差在每行清除参数绑定。剩余差距尚未独立拆分，不能全部归为遍历。**

条件：1,000,000条8字段记录，Release `-O3`；各组正确性检查1次、串行正式5次，以下为中位数（ms）。A为内存库，B-batch为文件库（读取为warm）。读写来自两个独立批次，各自在同批比较，不替换前面的五路线结果。16组正确性检查、80条计时均通过。

计时只含 **prepare → 数据操作循环 → finalize**；建表、BEGIN、COMMIT、打开关闭单列，不计入下表。正确性比较不进入正式计时。

### 读取：每列多一次NULL检查

原SQLite直接取列；EDADB先判断NULL再取值，共增加8,000,000次检查。调用链：
[DbTableOpSelect4Sqlite.h:76](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h#L76)
→ [DbStatement4Sqlite.h:348：sqlite3_column_type](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbStatement4Sqlite.h#L348)。

| 路线 | A读取 | B-batch读取 |
| --- | ---: | ---: |
| 原SQLite | 813.820 | 837.073 |
| SQLite增加相同NULL检查 | 1065.636 | 1095.355 |
| EDADB | 1102.421 | 1138.790 |

**增加检查净增252–258ms，约相当于EDADB总差距的86%–87%；剩余37–43ms。** 两边都复制字符串、复用Record，不存在EDADB独有的结果vector扩容。

检查具有语义用途：NULL不同于零值，NULL文本返回空指针，见[SQLite官方说明](https://www.sqlite.org/c3ref/column_blob.html)。本例保证非NULL，不代表通用框架可以直接删除检查。

### 写入：每行多一次参数清理

| 差异 | 直接SQLite | EDADB及源码 |
| --- | --- | --- |
| 参数清理 | 每行仅reset，下轮重新绑定全部参数 | [Insert:230](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpInsert4Sqlite.h#L230) → [resetForReuse:131](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbStatement4Sqlite.h#L131) → [clear_bindings:108](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbStatement4Sqlite.h#L108)，然后reset |
| 字符串绑定 | 2次bind_text | [bind_text64:242](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbStatement4Sqlite.h#L242)，两边均为TRANSIENT |
| 整数绑定 | 6次bind_int64 | [5次bind_int:184](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbStatement4Sqlite.h#L184)＋[1次bind_int64:207](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbStatement4Sqlite.h#L207) |

原SQLite每轮重绑8个参数；EDADB另外清理一次，共增加1,000,000次clear。它不是读端的NULL类型检查。[官方说明：reset保留绑定，clear将绑定置NULL](https://www.sqlite.org/c3ref/clear_bindings.html)。

| 路线 | A写入 | B-batch写入 |
| --- | ---: | ---: |
| 原SQLite | 1518.122 | 1582.017 |
| SQLite仅增加clear | 1573.522 | 1654.170 |
| SQLite仅对齐绑定API | 1514.694 | 1603.269 |
| SQLite对齐绑定API＋clear | 1576.684 | 1661.871 |
| EDADB | 1635.148 | 1713.397 |

- **仅增加clear净增55–72ms；匹配绑定API后，clear仍净增59–62ms。**
- 仅改变绑定路径为−3.428/+21.252ms，没有稳定的大幅影响；不能认定text64必然更慢。
- 全部对齐后仍剩52–58ms。文件组有波动，不能将净差值当作函数独占时间。
- create、COMMIT等分项保留在[原始计时](../../../../../../../../../../../../tmp/iedadb_write_control/samples.tsv)，不混入write比较。

### 未拆分的额外工作

EDADB还有[读操作器状态检查](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h#L397)、[写操作器状态管理](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpInsert4Sqlite.h#L101)、[通用成员遍历](../../../../../../src/database/edadb/core/include/edadb/DbObjectTraverser.h#L139)、[逐成员绑定及返回检查](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpInsert4Sqlite.h#L126)。这些是剩余成本候选，未分别计时；模板代码可能被优化。两边都有step、取值和字符串复制；本例没有adapter、Shadow或N+1。

### 每条记录与指令估算

N=1,000,000时，**整批差值1ms = 每条1ns**：
- 读NULL检查：每条约252–258ns，平均每次检查约31–32ns。
- 写clear：匹配绑定后每条约59–62ns，一次处理8个参数。
- 假设CPU固定2.30GHz，约60ns对应138周期；指令数还需实测IPC，不能由时间直接推出。以上均为摊销净差值，不是单次函数计时。

### 代码与原始证据

| 对照 | 测试实现／脚本 | 原始时间／统计 |
| --- | --- | --- |
| 读NULL | [读取循环:101](../../benchmark/stream_benchmark.cpp#L101)、[sqlite_vs_edadb/run_read.py](../run_read.py#L1) | [samples.tsv](../../../../../../../../../../../../tmp/iedadb_null_check/samples.tsv)、[summary.json](../../../../../../../../../../../../tmp/iedadb_null_check/summary.json) |
| 写绑定与clear | [写入循环:67](../../benchmark/stream_benchmark.cpp#L67)、[sqlite_vs_edadb/run_write.py](../run_write.py#L1) | [samples.tsv](../../../../../../../../../../../../tmp/iedadb_write_control/samples.tsv)、[summary.json](../../../../../../../../../../../../tmp/iedadb_write_control/summary.json)、[checks.json](../../../../../../../../../../../../tmp/iedadb_write_control/checks.json) |

脚本内含运行命令；结果目录保留当次源码、编译参数和日志。仅扩展测试程序，core和adapter未修改。下一步若省略检查或清理，必须另验NULL、部分绑定和失败后复用，不能仅凭性能结果删除安全处理。


## 复现入口

先按[构建说明](../../benchmark/implementation.md)生成可执行文件；两个实验顺序运行，不并行竞争资源。输出目录必须不存在：

```bash
cd "$(git rev-parse --show-toplevel)"
python3 scripts/edadb/performance/sqlite-baseline/sqlite_vs_edadb/run_read.py \
  /tmp/iedadb_benchmark_build/stream_benchmark /tmp/iedadb_read_control
python3 scripts/edadb/performance/sqlite-baseline/sqlite_vs_edadb/run_write.py \
  /tmp/iedadb_benchmark_build/stream_benchmark /tmp/iedadb_write_control_new
```

默认保持1,000,000条和5次正式采样。快速验证可追加`--count 1000 --runs 1 --settle 0`并使用新目录；这只验证迁移后的运行，不替代历史性能结果。

输出`samples.tsv`、`summary.json`、`checks.json`、逐次日志、源码快照和编译参数。原始完整对照的结论与证据保留于本页，不重新维护多份时间表。
