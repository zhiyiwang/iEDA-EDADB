# SQLite为什么比简单文本慢：归因对照结果

**状态：本轮归因对照完成，计时、计数和独立perf审计通过；已冻结。** 主表来自同一宿主机批次，不与旧批次或沙箱计时相减。

## 1. 方法与验收

- 1,000,000条8字段Component，无声明PK/FK/额外索引，仍有rowid表B-tree；生成规则与baseline一致。
- A：内存DB、MEMORY日志、OFF同步；B-batch：文件DB、实际DELETE/FULL。两者均一笔数据事务。
- 普通用户宿主机、CPU 0、Release g++-10 -O3、系统SQLite 3.37.2；正式串行5次，完整正确性运行兼预热，样本前及读写间等待1秒。
- write data：prepare → 全部bind/step/reset → finalize；批量组包含SQL构造。create、BEGIN、COMMIT、close单列。文本写包含格式化与flush，不要求fsync。
- read data：prepare → 全部step/取列/保存/消费 → finalize。A同连接first-read；B预读文件后新连接，属于OS-warm。
- **60组正确性、120条正式时间、24组统计（各5次）、10次独立计数通过。** 正确性独立验证0/1/8/101/1,000,000条，101覆盖批量尾部。正式循环无行内计时或计数查询。
- 运行归档二进制；[审计程序](../audit_causal.py)复算时间、核对哈希、实际SQL、计数和source ID。

时间单位ms，表中为中位数。delta先按同轮相减再取中位数，不一定等于表中中位数之差。N=1,000,000时，ms净差数值等于ns/条净差，不是函数独占时间。

比例口径：**相对文本耗时倍数 = SQLite data中位数 / 文本data中位数**；**对照净差比例 = 配对delta中位数 / SQLite data中位数**。只在相同读写方向比较，不将读写合计作分母。后者不是内部函数独占占比，不能相加凑成100%。

## 2. 写入：语句执行粒度是主要已验证因素

| 路线 | 改变什么 | A write ms | B-batch write ms |
| --- | --- | ---: | ---: |
| text | 格式化8字段，缓冲顺序写文本 | 541.514 | 538.721 |
| sqlite | 每行一次INSERT；字符串TRANSIENT绑定 | 1202.036 | 1264.825 |
| static | 仅字符串改STATIC，输入存活至finalize | 1072.860 | 1128.495 |
| batch10 | TRANSIENT不变，每条INSERT写10行 | 749.262 | 826.318 |
| batch100 | TRANSIENT不变，每条INSERT写100行 | 693.469 | 776.539 |

| 同轮净差 | A ms（min–max） | B-batch ms（min–max） |
| --- | ---: | ---: |
| sqlite − text | 660.522（649.803–666.988） | 725.751（723.731–733.761） |
| sqlite − static | 129.176（29.191–132.030） | 137.387（106.195–142.484） |
| sqlite − batch10 | 452.752（448.190–455.691） | 434.747（430.599–451.613） |
| sqlite − batch100 | 507.203（367.130–512.901） | 485.230（445.465–500.486） |

**写入Takeaway**：SQLite不仅输出数据，还要绑定参数、执行VM并维护表页面；本例最大的已验证写侧因素是逐条语句执行粒度，绑定策略也有贡献。

| 写入比例 | A内存 | B-batch文件 |
| --- | ---: | ---: |
| SQLite / 文本耗时 | 2.22倍（多121.98%） | 2.35倍（多134.78%） |
| 100行INSERT的配对净收益 / SQLite write data | 42.20% | 38.36% |
| STATIC绑定的配对净收益 / SQLite write data | 10.75% | 10.86% |

后两行是两项独立对照，不能相加，也不能分别称为step、memcpy独占占比。所有配对轮次方向一致；A/static、A/batch100有高值样本，全部保留，不猜测其来源。

文本只需格式化并顺序输出；SQLite还要绑定Mem参数槽、执行VM、编码record并维护rowid B-tree/Pager。多行组仍绑定8,000,000个字段、插入全部行，仅减少statement执行/重置及语句收尾周期，同时改变VM程序。

因此不能把507 ms全叫step入口成本；STATIC差值也不是memcpy独占时间。两项对照未测交互，**收益不能直接相加**。batch100仍慢于文本，剩余绑定、编码、B-tree/Pager成本未逐项分时。

### 建表与提交单列

| 配置/路线 | create ms | BEGIN ms | COMMIT ms |
| --- | ---: | ---: | ---: |
| A/sqlite | 0.169 | 0.001 | 1.520 |
| A/batch100 | 0.174 | 0.001 | 1.525 |
| B-batch/sqlite | 63.649 | 0.005 | 452.036 |
| B-batch/batch100 | 76.504 | 0.009 | 460.845 |

这些不属于write data。B提交有明显独立成本，但A也慢，不能以磁盘sync解释全部data差距；COMMIT不是纯sync计时，write data本身也可能写文件。

## 3. 读取：取列与应用物化不可忽略

| 路线 | 实际工作 | A read ms | B-batch read ms |
| --- | --- | ---: | ---: |
| text | 解析、保存8字段并消费 | 473.431 | 472.831 |
| sqlite | SELECT取8列、保存并消费 | 579.705 | 614.171 |
| step-only（诊断） | 同一SELECT，仅step至DONE并计数 | 290.800 | 327.231 |

| 同轮净差 | A ms（min–max） | B-batch ms（min–max） |
| --- | ---: | ---: |
| sqlite − text | 107.131（98.193–110.298） | 139.280（111.020–143.695） |
| sqlite − step-only | 290.343（287.195–290.747） | 284.808（271.219–291.842） |

**读取Takeaway**：SQLite读取的成本不仅在执行查询，还在把结果取出并保存成应用自己的数据。

| 读取比例 | A内存 | B-batch文件 |
| --- | ---: | ---: |
| SQLite / 文本耗时 | 1.22倍（多22.45%） | 1.30倍（多29.89%） |
| 去掉取列、保存和消费的配对净差 / SQLite read data | 50.08% | 46.37% |

- **取列**：通过column API取得SQLite当前行的字段；**应用物化**：将字段赋值、复制到C++ Record。本测试复用Record，不逐行新增对象或向vector追加。
- 1,000,000条记录中，去掉取列、保存及摘要消费，A/B分别净减少290.343/284.808 ms，证明这条客户端处理路径成本显著，不能只分析step。
- 这是整条路径的净影响，不是字符串复制或某个API的独占耗时；文本也承担解析和保存成本，不能把该净差全算成SQLite相对文本的额外开销。
- 只step并计数没有向应用交付完整字段，因此是诊断对照，不能作为等价读取的加速方案。

上述约46%–50%是消融净差相对完整读取的比例，不是精确的函数分时；VM在step-only中仍解码8列并返回ROW。

本实验没有EDADB、adapter，也没有额外逐列column_type检查；不要与sqlite-vs-edadb中的NULL检查成本混淆。

static/batch组读取代码未改变，重复读取仅作控制，不宣称新的读取优化。原始结果全部保留。

## 4. 计数与源码证据

| 操作 | RUN | VM_STEP |
| --- | ---: | ---: |
| 单行INSERT | 1,000,000 | 16,000,000 |
| 10行INSERT | 100,000 | 14,800,000 |
| 100行INSERT | 10,000 | 14,080,000 |
| 完整SELECT | 1 | 10,000,006 |
| step-only SELECT | 1 | 10,000,006 |

A/B一致；SORT/AUTOINDEX/REPREPARE均0，SELECT的FULLSCAN_STEP为999,999。全读需要SCAN，**不是N+1或缺索引问题**；prepare时规划查询，不是每行重新优化。

批量EXPLAIN显示：单行程序每次执行OpenWrite→NewRowid→MakeRecord→Insert→Halt；10/100行程序通过Yield循环生成记录，OpenWrite和最终Halt在整批外，NewRowid/MakeRecord/Insert仍逐行执行。静态程序变长（16/102/912条指令），但减少反复进入/退出执行周期。实际VM_STEP只减少约12%，A写时间却减少约42%，说明指令种类及执行/收尾路径的成本不同，不能按VM步数均摊毫秒数。

B的标准/batch10/batch100写入均有10,243次CACHE_WRITE及10,243次CACHE_SPILL，COMMIT另写434页；A均为0。WRITE与SPILL重叠，不能相加，也不等于sync次数。写入收益发生在写页计数相同的情况下，支持语句组织/执行成本解释，而非少写数据。

源码根目录：`/home/zhiyiwang/cs/db/sqlite/ubuntu-3.37.2-2ubuntu0.7/src/`；版本/source ID与系统库核对，未重编译SQLite。

| 阶段 | 本地源码位置 | 实际工作 |
| --- | --- | --- |
| prepare/规划 | prepare.c:882；select.c:6877 | 编译SQL、选择路径，仅prepare时进行 |
| 绑定 | vdbeapi.c:1385；vdbemem.c:1143 | TRANSIENT准备存储并复制；STATIC借用 |
| VM | vdbeapi.c:716、:761；vdbe.c:3078、:5242 | 执行MakeRecord、Insert等 |
| 存储 | btree.c:8801；pager.c:6148 | rowid表插入、页面及事务状态 |
| 收尾 | vdbeaux.c:3019 | 游标/语句状态；显式事务内不逐行COMMIT |
| 读VM | vdbe.c:2634、:1511、:5930 | Column解码、ResultRow、Next |
| 取列 | vdbeapi.c:1067 | 结果槽访问、类型/API状态、互斥管理 |

完整调用链及官方依据见[源码说明](sqlite_source.md)。源码说明机制、计数说明工作量、对照说明净影响；不编造未测函数的独占毫秒数。

## 5. 独立CPU采样

24次采样及24次无perf控制通过，零丢样；499 Hz用户态cycles，仅门控data阶段。使用同profile二进制、同权限环境控制，阶段中位数观测差异为−3.91%至+1.99%，均未超过预设5%阈值。B读为负差值，不是perf使程序加速的证据；差值含运行波动，不宣称准确扣除了采样开销。

### 写入CPU路径

| 路径 | A样本占比 | B-batch样本占比 |
| --- | ---: | ---: |
| sqlite3_step | 63.62% | 62.77% |
| bind_record | 32.84% | 32.94% |
| sqlite3_reset | 2.00% | 3.30% |

step是最大CPU路径，绑定次之，与多行INSERT及STATIC对照相互支持。A中VdbeExec为60.78%、VdbeHalt为14.27%、BtreeInsert为7.64%，都是嵌套Children占比，不能再加到step或相互相加当总时间。

### 读取CPU路径

| 路径 | A样本占比 | B-batch样本占比 |
| --- | ---: | ---: |
| sqlite3_step | 55.07% | 51.16% |
| fetch_record | 43.44% | 46.39% |

这支持查询推进与客户端取列/保存都重要，与step-only消融一致；没有证明某一mutex发生线程竞争。样本比例不是墙钟分项，不能乘正式ms冒充独占耗时。

**核心结论**：本例慢的不是每行重新优化SQL，也不能一概归为磁盘sync。写侧是逐条语句执行/收尾加绑定、记录及页处理；其中执行粒度与绑定策略已通过控制实验验证。读侧是VM扫描/解码加通用取列和应用物化，后半段已通过消融验证。具体函数的独占时间、B的sync等待仍未量化，不凑出一张虚假的完整分时表。

## 6. 阅读、运行和证据

1. [readme](../readme.md)：构建、运行及审计；[方案](test_plan.md)：配置、数据与边界。
2. [实现](../benchmark.cpp#L196)：批量及step-only；[写计时](../benchmark.cpp#L345)、[读计时](../benchmark.cpp#L355)。
3. 正式宿主机批：[报告](../../../../../../../../../../../../tmp/iedadb_sqlite_text_causal_host/report.md)、[原始时间](../../../../../../../../../../../../tmp/iedadb_sqlite_text_causal_host/samples.tsv)、[统计](../../../../../../../../../../../../tmp/iedadb_sqlite_text_causal_host/summary.json)、[配对delta](../../../../../../../../../../../../tmp/iedadb_sqlite_text_causal_host/deltas.json)。
4. [计数](../../../../../../../../../../../../tmp/iedadb_sqlite_text_causal_host/counters.tsv)、[正确性](../../../../../../../../../../../../tmp/iedadb_sqlite_text_causal_host/checks.json)、[审计](../../../../../../../../../../../../tmp/iedadb_sqlite_text_causal_host/causal_audit.json)、[manifest](../../../../../../../../../../../../tmp/iedadb_sqlite_text_causal_host/manifest.json)、[批量EXPLAIN](../../../../../../../../../../../../tmp/iedadb_sqlite_text_causal_host/batch_explain.json)。
5. 独立perf：[热点](../../../../../../../../../../../../tmp/iedadb_sqlite_text_causal_perf/summary.json)、[控制差值](../../../../../../../../../../../../tmp/iedadb_sqlite_text_causal_perf/overhead.json)、[审计](../../../../../../../../../../../../tmp/iedadb_sqlite_text_causal_perf/audit.json)。

`/tmp/iedadb_sqlite_text_causal_full`是前置沙箱验收，运行期间还发生构建路径替换，整批不作为正式性能依据；宿主机改为运行归档二进制且哈希审计通过。旧批`/tmp/iedadb_sqlite_text_counters_full`及`/tmp/iedadb_sqlite_text_counters_perf`只保留历史证据，不混算。服务器/tmp产物不随Git或文档复制。

## 7. 补充：SELECT *与显式全部列

独立批次，1,000,000条相同8字段；只改SELECT文本，取列/保存/消费完全相同。Release -O3、宿主机CPU 0，A/B各5轮，轮换执行顺序。16组全字段检查、20个正式样本及4次计数诊断通过；不与第2–5节跨批相减。

### 完整读取：未证明稳定速度差异

| 配置 | 显式8列 read ms | SELECT * read ms | 同轮star−explicit中位数 ms | 配对范围 ms |
| --- | ---: | ---: | ---: | ---: |
| A | 576.885 | 569.779 | +0.042 | −12.293 ～ +8.624 |
| B-batch | 614.883 | 608.335 | −11.225 | −13.687 ～ +54.787 |

两条主时间是各自中位数；delta是同轮相减再取中位数，不能直接拿前两列相减替代。配对差值均跨零，B/star有一次663.514 ms高值，完整保留。**这5轮不能证明SELECT *或显式全部列有稳定的完整读取优势。**

### prepare/finalize：小差异，不是逐行成本

同一已加载schema的连接上，预热一次后连续10,000次prepare→finalize，整段计时；不执行查询，无逐次时钟。

| 配置 | 显式列10,000次 ms | SELECT * 10,000次 ms | 每次显式列 μs | 每次SELECT * μs | 配对净差 μs/次 |
| --- | ---: | ---: | ---: | ---: | ---: |
| A | 109.724 | 106.877 | 10.972 | 10.688 | −0.281 |
| B-batch | 108.739 | 103.751 | 10.874 | 10.375 | −0.499 |

本批prepare/finalize循环中star五轮均略快；含finalize、循环和正常错误检查，不是纯prepare耗时。未单独测词法解析与星号展开，不能把差值指定给其中某个函数。完整读取只prepare一次，不能把这项微秒差乘以1,000,000行。

### 为什么执行阶段没有少做工作

两种SQL的完整EXPLAIN字节码逐项相同；RUN均1、VM_STEP均10,000,006、FULLSCAN_STEP均999,999，SORT/AUTOINDEX/REPREPARE均0。正确性检查确认返回的字段和顺序一致。

SQLite在准备阶段将星号展开为结果列列表。本机对应`src/select.c:5451`的selectExpander，`:5582`附近处理TK_ASTERISK；不是每读一行再展开。官方依据：[SELECT结果列](https://www.sqlite.org/lang_select.html#resultset)、[3.37.2源码](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/select.c#L5451)。

**Takeaway**：当前固定schema、相同8列的查询，星号与显式全部列最终执行同样的程序。它不是SQLite比文本慢的核心原因；也不能由此推断“少读几列没有收益”或任意schema下SELECT *都等价。

实现：[select_projection.cpp](../select_projection.cpp)、[run_select.py](../run_select.py)；运行见[README](../readme.md#补充select-与显式全部列)。原始证据：[samples](../../../../../../../../../../../../tmp/iedadb_select_host/samples.tsv)、[summary](../../../../../../../../../../../../tmp/iedadb_select_host/summary.json)、[delta](../../../../../../../../../../../../tmp/iedadb_select_host/deltas.json)、[audit](../../../../../../../../../../../../tmp/iedadb_select_host/audit.json)、[manifest](../../../../../../../../../../../../tmp/iedadb_select_host/manifest.json)。字节码/实际配置/计数在该目录各样本log中；每个样本各自产生的DB在验证后删除。
