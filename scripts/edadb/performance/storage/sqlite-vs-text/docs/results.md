# SQLite与文本：低扰动测试结果

## 1. 范围与验证

本页主数据来自新版独立计数实验，不与历史批次拼接。固定1,000,000条、8字段Component，无声明PK/FK/index；Release -O3，系统SQLite 3.37.2，普通宿主机CPU 0，串行5次，表中单位ms。

- 24组正确性检查、60条正式计时、12组统计和4次独立计数运行全部通过；另有1,000条的小规模验收。
- 正确性逐值比较及同长度字符串损坏检测，不进入正式样本。正式读取完整8字段并消费相同摘要。
- 正式模式无计数输出；反汇编确认两个非诊断operation实例无时钟或counter API调用。整段首尾计时仍有固定代价，不宣称零扰动。
- 输入、配置、边界见[方案](test_plan.md)；SQLite执行路径见[源码分析](sqlite_source.md)。

## 2. 写入：慢多少，确认了什么

write data含prepare、全部bind/step/reset及finalize，不含建表、外层BEGIN/COMMIT、连接开关。文本包含格式化、输出和flush，不要求fsync。

| 配置 | 实现 | 中位数 | min–max |
| --- | --- | ---: | ---: |
| A | text | 551.582 | 550.204–553.687 |
| A | sqlite | 1202.365 | 1199.327–1206.062 |
| A | static | 1060.039 | 1059.077–1066.422 |
| B-batch | text | 551.757 | 551.125–553.385 |
| B-batch | sqlite | 1277.074 | 1276.009–1283.948 |
| B-batch | static | 1128.054 | 1125.776–1149.463 |

`sqlite`使用TRANSIENT字符串绑定；`static`仅将两个字符串改为STATIC，输入一直有效至finalize。其余SQL、数据和事务不变；独立计数核实两组工作量相同。

| 同轮差值 | A（ms） | B-batch（ms） | 能说明什么 |
| --- | ---: | ---: | --- |
| SQLite−文本 | 652.089 | 724.686 | 当前存储路线的净差距，不是step独占时间 |
| TRANSIENT−STATIC | 141.076 | 150.233 | 绑定所有权策略的净影响，不是memcpy独占时间 |

差值先按同轮相减，再取中位数；不一定等于主表两个中位数相减。所有写差值5轮方向一致。N=1,000,000时，ms净差在数值上等于每条记录ns净差：例如A绑定策略约141.076 ns/条；不能据此推算CPU指令数。

**原因证据**：TRANSIENT在绑定时准备SQLite拥有的存储并复制字符串；STATIC借用输入。改变这一点能稳定减少写时间，证明绑定策略贡献了部分成本。但减少后SQLite仍慢于文本，不能把全部差距归给复制；VM、记录编码及B-tree/Pager工作仍保留。

### 建表和提交单列

| 配置/实现 | init | create | BEGIN | COMMIT | close |
| --- | ---: | ---: | ---: | ---: | ---: |
| A/sqlite | 0.217 | 0.175 | 0.001 | 1.480 | — |
| A/static | 0.221 | 0.171 | 0.001 | 1.437 | — |
| B-batch/sqlite | 0.208 | 71.577 | 0.010 | 691.311 | 0.387 |
| B-batch/static | 0.215 | 74.479 | 0.009 | 700.850 | 0.386 |

create包含建表事务；COMMIT只是数据事务提交调用的耗时，不是全部I/O或纯sync耗时。A保留连接用于读取，因此写close不适用。write complete=BEGIN+write data+COMMIT，逐样本计算，不能把不同阶段中位数之和冒称完整时间的中位数。

## 3. 读取：独立分析

read data含prepare、step、取8列、复用Record保存/消费、最终DONE及finalize。B先预读文件，再新建SQLite连接；OS-warm不等于SQLite页缓存已热。

| 配置 | 实现 | 中位数 | min–max |
| --- | --- | ---: | ---: |
| A | text | 474.249 | 474.151–474.541 |
| A | sqlite | 573.382 | 572.722–576.883 |
| A | static | 570.779 | 569.829–897.363 |
| B-batch | text | 474.695 | 474.247–475.160 |
| B-batch | sqlite | 610.702 | 610.063–616.738 |
| B-batch | static | 610.880 | 607.237–612.130 |

SQLite−文本的同轮中位差为A **98.841 ms（98.841 ns/条）**、B **136.199 ms（136.199 ns/条）**，5轮均为正。STATIC只改变写入，读取代码没有改变；A/static有一次897.363 ms波动，全部保留，不声称读性能得到优化，也不猜测该波动原因。

读取不仅执行step，还通过10次column API取8字段并复制两个字符串。当前没有删除fetch的对照，也没有各内部函数的独占毫秒数；CPU热点用于定位，不能代替因果计时。

## 4. 独立计数：实际做了多少工作

语句计数（A/B、TRANSIENT/STATIC均一致）：

| 计数 | INSERT | SELECT |
| --- | ---: | ---: |
| RUN | 1,000,000 | 1 |
| VM_STEP | 16,000,000 | 10,000,006 |
| FULLSCAN_STEP | 0 | 999,999 |
| REPREPARE / SORT / AUTOINDEX | 0 / 0 / 0 | 0 / 0 / 0 |

EXPLAIN对应写入Variable→NewRowid→MakeRecord→Insert→Halt，读取Column→ResultRow→Next。VM_STEP不是CPU指令数；本例全表读取的SCAN合理，不是N+1，也不是索引缺失的证明。

Pager计数（标准/STATIC均一致）：

| 阶段/计数 | A | B-batch |
| --- | ---: | ---: |
| create / CACHE_WRITE | 0 | 2 |
| write / CACHE_HIT | 2,954,125 | 2,954,125 |
| write / CACHE_WRITE | 0 | 10,243 |
| write / CACHE_SPILL | 0 | 10,243 |
| COMMIT / CACHE_WRITE | 0 | 434 |
| read / CACHE_HIT | 10,677 | 2 |
| read / CACHE_MISS | 0 | 10,677 |

B在提交前就有脏页写出；SPILL和WRITE存在重叠，不能相加。页写次数不是sync次数，MISS也不是物理磁盘次数。A数据与日志在内存，因此不能用磁盘sync解释A相对文本的差距。A/B还改变缓存、日志与同步配置，不能把两者时间差全称为磁盘成本。

计数读取有代价：普通statement计数是内存数组访问；缓存统计还涉及连接锁及各Pager汇总。本实验每个诊断进程读取40次缓存状态、12次语句状态，全部在正式样本结束后单独运行，不放入读写时间。详见[接口与源码](test_plan.md#6-如何实现统计且不污染时间)。

## 5. 独立CPU热点

本轮24次采样＋24次无perf控制通过，零丢样；每组3次，CPU 0，用户态cycles采样499Hz。相同profile二进制、相同sudo环境对照，观测到阶段中位时间增加0.85%–2.01%，均在5%阈值内。不是零扰动；下表只表示CPU样本比例，不能乘正式wall time作为函数毫秒数。

### 写入CPU热点

| 路径 | A | B-batch |
| --- | ---: | ---: |
| sqlite3_step | 59.53% | 61.07% |
| bind_record | 36.46% | 34.77% |
| sqlite3_reset | 2.83% | 3.19% |

A的step样本范围59.32%–63.24%。内部VdbeExec为57.33%、VdbeHalt为12.36%、BtreeInsert为8.27%，都是嵌套Children比例，不能再加到step上。证据支持“执行VM及记录/页处理是最大CPU路径，绑定同样重要”，不支持“全部差距都是step或sync”。

### 读取CPU热点

| 路径 | A | B-batch |
| --- | ---: | ---: |
| sqlite3_step | 51.79% | 48.54% |
| fetch_record（取列＋字符串赋值） | 44.69% | 50.42% |

A的step范围51.52%–54.65%。取列API不是直接访问C++字段，还涉及结果类型、状态及连接互斥管理。mutex函数有样本并不证明发生线程争用。剩余各步骤独占毫秒数未测，不用估计凑齐总时间。

## 6. 结论与边界

- 写：已量化绑定策略的净影响；剩余差距不能全部归为step，更不能分摊成未经测量的内部毫秒数。
- 读：SQLite推进查询和取列保存都需工作；本轮量化的是整个路线净差，而不是精确的step独占时间。
- Takeaway：普通rowid表的TEXT主键通常对应“表B-tree＋唯一索引B-tree”；WITHOUT ROWID用声明主键组织表。**本实验没有TEXT主键，不存在那份主键唯一索引，不能用两棵树维护解释当前差距。** [存储说明](../../sqlite-reference/btree_storage.md)
- 后续若继续验证每次step的固定成本，应单独设计等数据多行INSERT；若追问sync，则独立追踪系统调用，不修改正式基线，也不把跟踪时间当无扰动性能。

## 7. 结果在哪里

- 当前正式批：[report.md](../../../../../../../../../../../../tmp/iedadb_sqlite_text_counters_full/report.md)、[samples.tsv](../../../../../../../../../../../../tmp/iedadb_sqlite_text_counters_full/samples.tsv)、[summary.json](../../../../../../../../../../../../tmp/iedadb_sqlite_text_counters_full/summary.json)、[配对差值](../../../../../../../../../../../../tmp/iedadb_sqlite_text_counters_full/deltas.json)。
- 独立计数：[counters.tsv](../../../../../../../../../../../../tmp/iedadb_sqlite_text_counters_full/counters.tsv)；每行附原始日志名，配置及EXPLAIN/EQP在日志中。
- 验证：[checks.json](../../../../../../../../../../../../tmp/iedadb_sqlite_text_counters_full/checks.json)、[audit.json](../../../../../../../../../../../../tmp/iedadb_sqlite_text_counters_full/audit.json)、[实现审计](../../../../../../../../../../../../tmp/iedadb_sqlite_text_counters_full/implementation_audit.json)。
- 复现：[manifest.json](../../../../../../../../../../../../tmp/iedadb_sqlite_text_counters_full/manifest.json)、[源码快照](../../../../../../../../../../../../tmp/iedadb_sqlite_text_counters_full/source/)、[编译参数](../../../../../../../../../../../../tmp/iedadb_sqlite_text_counters_full/flags.make)；运行命令见[readme](../readme.md#如何运行和查看)。
- perf独立批：[目录](../../../../../../../../../../../../tmp/iedadb_sqlite_text_counters_perf/)，保留采样和无perf控制日志。
- 旧批次仍在服务器`/tmp/iedadb_sqlite_text_host_full`及`/tmp/iedadb_sqlite_text_perf_final`；本页不混算其时间，旧行内外推不再作为结论。

服务器产物不是仓库附件，复制仓库不会同时复制这些文件。
