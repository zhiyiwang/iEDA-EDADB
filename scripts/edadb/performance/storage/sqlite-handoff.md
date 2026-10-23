# SQLite实验交接README

**范围：本文交接prof-test中的SQLite实验，不是三库最新结果报告。** 独立三库与Parquet已经有平面实测，入口为`/home/zhiyiwang/cs/db/sqlite/bench/results-brief.md`；[三库计划](db-text-plan.md)保留初始设计，未完成扩展仍需单独批准。后续EDADB接入见[DuckDB＋Parquet后端交接](duckdb-backend-handoff.md)，后端迁移尚未实现。

供新agent接续工作；这是现有状态与约束，不是授权执行新的优化、切分支或提交。先核实Git状态，再阅读结果并讨论下一轮单变量计划。

## 1. 工作位置与边界

- 服务器：cherry13；仓库：`/home/zhiyiwang/cs/arch/eda/iEDA-EDADB`。
- 当前分支：`prof-test`，基线HEAD `d1e3a21256063218d7cd0b62db8926e7a9633433`；core gitlink为`d6656f0`。接手时重新检查，不假设分支未变。
- 本工作区只保存原生SQLite C API与文本实验，不修改EDADB core、adapter或已冻结基线；独立三库工作在`/home/zhiyiwang/cs/db`，不混入该目录的历史测量。
- 新实验在`storage/sqlite-write/`，随本提交保存、尚未push；目录名包含write，但已补齐完整读取。
- 另有开发分支`edadb-performance-optimization-dev`保存未完成ORM工作（iEDA WIP `ad6800b72`，core `codex/net-table-read` WIP `21d5b62`），本轮不切换、不合并、不覆盖。
- 当前待提交范围及理由见文末“待提交清单”；本提交保存下述清单；未push、未新建milestone。构建目录、二进制和缓存不应提交，不要`git add .`。

```bash
cd /home/zhiyiwang/cs/arch/eda/iEDA-EDADB
git branch --show-current
git status --short
git ls-tree HEAD src/database/edadb/core
git -C src/database/edadb/core status --short
```

## 2. 阅读顺序与已有实验

| 顺序／入口 | 内容与状态 |
| --- | --- |
| 1. [最新A/B结果](sqlite-write/results.md) | A单表INSERT粒度与读取对照；B三表交织，当前工作重点 |
| 2. [方法](sqlite-write/test_plan.md)、[运行说明](sqlite-write/readme.md) | 数据、控制变量、计时、命令；readme保留早期写专项命令，勿误作最新A/B |
| 3. [总体总结](../experiment-summary.md) | iEDA/EDADB优化、SQLite实验之间的关系及跨批次限制 |
| [五路线](baseline/results.md) | 文本、原生iEDA、SQLite、纯EDADB、adapter；已冻结 |
| [纯EDADB对齐](sqlite-vs-edadb/results.md)、[操作定义](sqlite-vs-edadb/alignment.md) | NULL、clear、SQL与绑定API；已冻结 |
| [父子表](sqlite-pk-fk/docs/results.md) | 1万父＋10万子，索引与FK检查；已冻结 |
| [单表索引](sqlite-index/docs/results.md) | 100万条、8种主键/索引布局；已冻结 |
| [旧SQLite/text归因](sqlite-vs-text/docs/results.md) | STATIC、取列消融、SELECT写法、perf；已冻结，不是最新A/B批次 |
| [SQLite机制](sqlite-reference/execution_flow.md)、[分析工具](sqlite-reference/execution_analysis.md) | 官方链接、VM、存储、计数器；源码已在服务器`/home/zhiyiwang/cs/db/sqlite`，无需重复下载 |

冻结目录的AGENTS.md禁止未经同意修改代码、结果与结论；新实验放现有sqlite-write或获批的新兄弟目录。旧report.md有历史状态文字，不以它覆盖最新milestone或结果审计。

## 3. 数据与测量口径

```sql
CREATE TABLE component (
  name TEXT, master_name TEXT, source INTEGER, status INTEGER,
  orient INTEGER, x INTEGER, y INTEGER, record_order BIGINT
);
```

无显式PK/FK、唯一约束或额外索引，仍有隐式rowid表树。8字段为2字符串、5个int32、1个int64；输入均非NULL。name=U加7位补零序号，master_name=bench_cell，source/status/orient=2/3/1，x=(i%1000)×100，y=(i/1000整数除法)×100，record_order=i。文本同序空格分隔、每行一条，不是DEF。

| 实验 | 数据与改变因素 |
| --- | --- |
| A | 单表100万条／单文件100万行；每条INSERT含1、10、100行；随后读取方式完全相同 |
| B | 同库t0/t1/t2三张同schema独立表，各10万条，总30万条；对应三文件。source分别2/3/4，无父子关联。改变每1、10、全部记录后的表切换 |

**A/B表示实验，不表示配置。** 配置叫内存（MEMORY/OFF）或文件（DELETE/FULL），两组文本始终是文件。每个SQLite写入组一笔数据事务；B三张表也共用一笔。B的十次单行INSERT不等于A的一次十行INSERT。

- write data：prepare、全部bind/step/reset、finalize；文本含格式化、输出、flush。不含init/create/BEGIN/COMMIT/close。
- read data：完整8字段取值/解析、保存至复用Record及相同摘要消费；SQLite含prepare/finalize。一次SELECT后持续step，不逐行重新查询。
- 文件预读后新开SQLite连接；内存写后同连接首次读取，属于warm，不宣称cold或等价持久性。文本flush不是fsync。
- 输入生成和正确性验证在计时外。A正式进程只做摘要，完整字段由独立check验证；B正式计时后再做完整字段验证，read close发生在该验证之后。
- GCC 11.4、Release -O3、系统SQLite 3.37.2，固定CPU0，轮换顺序串行5次。服务器双路Xeon E5-2650 v3、40逻辑CPU、125GiB RAM。编译可并行；正式测试不得与编译或其他重负载争抢资源。

## 4. 当前结论：不能混淆读写

### A：单表粒度

时间ms，5次中位数；每格为内存配置／文件配置。

| 实现 | write data | read data |
| --- | --- | --- |
| 文本 | 738.134 / 739.137 | 583.620 / 583.794 |
| SQLite单行 | 1505.569 / 1565.399 | 790.186 / 833.808 |
| SQLite十行 | 1009.162 / 1070.581 | 789.008 / 822.994 |
| SQLite百行 | 936.963 / 1003.515 | 794.260 / 832.164 |

写：百行相对单行降低37.8%／35.9%，仍慢于文本26.9%／35.8%；不是减少事务提交。读：完整读取仍慢约35%～43%；十行/百行标签指生成数据库的写法，不是批量读取API。

### B：三表交织

| 切换粒度 | SQLite write（内存／文件） | SQLite read（内存／文件） |
| --- | --- | --- |
| 每1条 | 446.381 / 516.180 | 240.967 / 271.179 |
| 每10条 | 445.441 / 533.091 | 237.657 / 274.237 |
| 按表完成 | 442.957 / 511.399 | 236.606 / 274.427 |

文本对应写约220～226ms、读约171～174ms，精确逐组值见结果页。SQLite写为文本约1.98～2.39倍，读约1.38～1.60倍。按表写改善不足1%且配对差值跨零；内存按表读改善1.81%，文件无稳定改善。不支持交织是主要瓶颈，也不能猜测每10条较慢的内部原因。

五个(write_block,read_block)组合：(1,all)、(10,all)、(all,all)、(all,1)、(all,10)。写比较固定读，读比较固定写。所有写组RUN=300,000、读组RUN=3，SORT/REPREPARE=0。

### 其他已有结论，避免重复实验

- 纯EDADB对齐文件组：基本SQLite读835.683、写1608.843；完全操作对齐SQLite读1094.703、写1664.665；EDADB读1115.088、写1688.907。NULL检查解释主要读差距，逐行clear解释部分写差距；reset本来两边都有，API替换不等于额外调用。
- 父子无索引读取约120～130秒，有索引约107～162ms；索引减少每次扫描，不消除N+1。
- 索引增加写入/空间但显著改善点查，不同布局全扫描差别远小于点查；WITHOUT ROWID不是统一更快。
- 旧独立perf只用于其对应批次/阶段，CPU采样百分比不是独占墙钟ms。新A/B没有重新做读取perf。

## 5. 代码与证据定位

| 文件／结果前缀 | 作用 |
| --- | --- |
| sqlite-write/CMakeLists.txt | 独立构建，无需构建iEDA |
| sqlite-vs-text/benchmark.cpp | A直接复用的冻结实现；不要修改 |
| sqlite-write/run_read_write.py、audit_read_write.py | A矩阵、验证、独立复算；结果rw-* |
| sqlite-write/interleaved.cpp | B三表调度、完整取值与验证；只读复用冻结数据工具 |
| sqlite-write/run_interleaved.py、audit_interleaved.py | B矩阵与独立复算；结果interleave-* |
| sqlite-write/benchmark.cpp、run.py、audit.py | 早期写专项及三表写，保留的非前缀证据 |
| sqlite-write/profile.cpp、run_profile.py、audit_profile.py | 独立写CPU采样；结果profile-*，不等同正式计时 |

证据均在sqlite-write/results：samples为原始样本，summary为中位数/范围，comparisons或review为配对分析，counters为独立计数，config为读回配置，manifest为版本/源码/二进制哈希，audit为检查状态。

A验收40组字段检查、40个正式进程（80条时间）、6组计数；B验收100组预检查/预热、100个正式进程（200条时间）、10组计数。不要把不同单位相加称为统一测试次数。

服务器外部日志目录：`/tmp/sqlite-read-write-run-01`、`/tmp/sqlite-interleaved-run-01`；早期写与perf分别`/tmp/sqlite-write-run-01`、`/tmp/sqlite-write-profile-01`。/tmp可能被清理，仓库已保留精简证据，生成DB/文本验证后删除。复制给另一个agent时需带完整sqlite-write目录，构建还依赖相邻sqlite-vs-text/benchmark.cpp，不能仅复制单个cpp。

## 6. 复现最新A/B

在仓库根执行，输出目录必须不存在；下面名称仅示例，每轮换新名字：

```bash
cmake -S scripts/edadb/performance/storage/sqlite-write -B /tmp/sqlite-handoff-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/sqlite-handoff-build --target sqlite_read_write sqlite_interleaved -j 40
python3 scripts/edadb/performance/storage/sqlite-write/run_read_write.py --binary /tmp/sqlite-handoff-build/sqlite_read_write --output /tmp/sqlite-rw-new-run
python3 scripts/edadb/performance/storage/sqlite-write/audit_read_write.py /tmp/sqlite-rw-new-run
python3 scripts/edadb/performance/storage/sqlite-write/run_interleaved.py --binary /tmp/sqlite-handoff-build/sqlite_interleaved --output /tmp/sqlite-interleave-new-run
python3 scripts/edadb/performance/storage/sqlite-write/audit_interleaved.py /tmp/sqlite-interleave-new-run
```

审计若涉及源码哈希，应对照测量时版本；后续仅CMake新增目标也可能导致旧哈希不同，不修改旧manifest伪装一致。正式计时、计数、perf独立运行；perf权限先实查，不修改sysctl，授权诊断与普通计时环境不同则分别说明。

## 7. 接续计划与输出规范

1. 检查分支、脏文件、冻结边界和已有结果，先说明正在研究的具体差距，不立即重跑全部实验。
2. 写侧候选：多行INSERT与TRANSIENT/生命周期安全STATIC做组合对照；保持schema、数据、事务、持久性不变，分离两种因素。旧STATIC实验已存在，先读再决定新增什么。
3. 读侧候选：完整读取独立采样，区分VM/step、column、字符串保存与消费；不能用step-only或COUNT冒充完整读取加速。
4. 每轮先小规模及尾批正确性，再预热与串行正式性能；保存配置、源代码身份、原始样本、统计和审计。长任务定时心跳及更新ETA。
5. 输出继续按A/B实验分组，各组方法→写结果→读结果→takeaway；schema、记录量、数据边界必须明确。耗时比用SQLite÷文本；跨批不直接归因，不确定原因明确写待验证。
6. 本轮只获准交接整理；新测试方案先与用户确认。未获授权不commit/push、不移动milestone、不改ORM生产实现。已有未提交结果应先保存，不reset/clean覆盖。

## 8. 待提交清单与交付边界

以下是**prof-test同一工作区的文件组，不是多个分支**。本清单经用户授权随本提交保存；未push或新建milestone。

| 文件组 | 已做工作与结果 | 为什么保存／提交什么 |
| --- | --- | --- |
| sqlite-write最新A/B | A百行INSERT写改善约36%～38%，仍慢于文本；B写交织收益不稳定、内存按表读改善1.81% | 保存可复现对照；提交CMakeLists、interleaved.cpp、run_read_write.py/run_interleaved.py及相应audit脚本、results.md/readme.md/test_plan.md、rw-*和interleave-*证据 |
| sqlite-write早期写专项及perf | 70条写时间、12计数组；独立12采样＋12控制，无丢样。CPU比例不等于独占时间 | 保留结论来源；提交benchmark.cpp/profile.cpp、run.py/run_profile.py及audit脚本、write-results.md、无前缀和profile-*精简证据 |
| ../experiment-summary.md | 汇总端到端优化、五路线及SQLite单体；标明跨批次和工作量不同 | 会议及后续选型入口；只提交报告，不冒称包含其他分支生产代码 |
| 本文sqlite-handoff.md | 记录代码、结果、运行与后续要求 | 让新agent不重复实验、不覆盖冻结范围；提交交接文档 |
| db-text-plan.md | 三库最初方案，部分已在外部目录完成；扩展并非全部完成 | 保留设计与约束；标记历史方案，不充当最新实测证据 |
| duckdb-backend-handoff.md | 推荐core494ce79／iEDA1ec76562a起点；明确原生后端与Parquet快照分离 | 保存下一阶段设计；不计入已实现功能，不宣称完成迁移 |
| ../README.md、readme.md、根.gitignore | 导航、状态及精确放行 | 与对应文档一起保存，确保源码与JSON/TSV不会遗漏 |

### 明确不提交

- bin-profile/、bin-release/、build-profile/、build-release/、__pycache__、二进制、生成数据库/文本、原始perf大文件及临时日志。
- 未经用户同意的core/adapter修改（本轮没有）；不改冻结AGENTS范围的代码或结果。
- `/home/zhiyiwang/cs/db`外部项目不属于本仓库。本文仅引用其路径/报告；若要交付外部实验，需要另行保存其源码和原始证据。

### 提交前还欠什么

本次执行更新：四目标Release构建、105组小规模检查、4组profile全量控制通过；早期写/B/perf审计重新执行，A全部中位数及配对复算通过。A仅CMake哈希因后续新增目标不同，不改写旧manifest。以下保留验收流程供后续修订使用，不表示本次仍缺少上述验证。完整说明见[测试readme](sqlite-write/readme.md)。

归档audit/review的PASS是历史实验状态。本轮文档整理只核对状态、链接、样本数量和格式；不替代新构建、smoke和全部独立审计。真正提交前应：
1. 构建四个CMake目标并做空集、尾批、小规模字段验证；无代码变化不必默认全量性能重跑。
2. 复算归档数据，确认正文数字和manifest归属；后续新增CMake目标导致旧哈希变化时解释差异，不改旧manifest。
3. 检查精确暂存文件清单与大小，不使用git add .；同步“待提交”状态但不提前声称已push。

讨论建议：先保存已完成SQLite代码/证据及报告，再单独保存未来方案。若建立新milestone，只标记SQLite实验交付（如milestone/sqlite-text-experiments-v2），不移动旧产品milestone；core gitlink仍为d6656f0，无需造新的core提交或产品配对标签。具体拆分及标签等待用户决定。
