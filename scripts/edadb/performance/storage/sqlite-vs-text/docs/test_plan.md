# SQLite与文本：低扰动成本定位方案

状态：已实现低扰动版本；结果与验证状态统一见[results.md](results.md)。只修改本实验目录，不改SQLite库、EDADB或冻结基线。

## 1. 已知事实与要回答的问题

已有时间与CPU热点统一见[结果](results.md)，源码解释见[源码分析](sqlite_source.md)。本方案只定义新测试，不重复保存实测数字。目标是分别确定写入、读取的总差值及有证据支持的成本原因。

## 2. 固定比较条件

- 仍用baseline的8字段component：name、master_name、source、status、orient、x、y、record_order；无声明PK/FK/index，普通rowid表。
- N=1,000,000；name为U加7位序号，master_name=bench_cell，source/status/orient=2/3/1，x=(序号%1000)*100，y=(序号/1000)*100，record_order=序号。
- 数据生成在计时外。全部路线读取并消费8字段；主对照复用Record、复制字符串，不改成只取长度或COUNT(*)。
- 保留[参数配置](../../sqlite-reference/config.md)的A和B-batch；输出实际配置、SQLite版本、编译选项和库路径。A为内存库，不存在数据库文件sync；不是没有事务或页管理。
- 相同Release -O3二进制、主机执行环境、CPU亲和性和输入。性能串行；构建可并行。内存A同连接读取；B在计时外预读文件、新连接读取；文本明确预读。均不称cold。

```sql
CREATE TABLE component(name TEXT,master_name TEXT,source INTEGER,status INTEGER,
                       orient INTEGER,x INTEGER,y INTEGER,record_order BIGINT);
INSERT INTO component VALUES(?,?,?,?,?,?,?,?);
SELECT name,master_name,source,status,orient,x,y,record_order FROM component;
```

文本是格式化顺序文件，SQLite还维护数据库结构，语义保障不相同；比较回答当前两种表示的应用成本，不是证明一种存储对所有工作负载更快。

## 3. 三类运行：不把诊断放入正式计时

| 运行 | 如何实现 | 产物及限制 |
| --- | --- | --- |
| 正式时间 | 仅各阶段首尾读steady_clock；循环内无计时、统计查询或日志 | 阶段wall time，含该阶段阻塞；保留正常错误检查 |
| 独立计数 | 同SQL、数据、配置；阶段边界读取SQLite已有计数器 | 工作量，不提供bind/step的毫秒数 |
| 独立perf | 只采目标data阶段，另跑相同二进制无perf控制 | CPU热点；嵌套调用不相加，不能解释全部I/O等待 |

正确性独立运行0/1/8/1,000,000条，逐值验证、行数和顺序检查，不进入正式样本。正式读取保留相同的可观察消费摘要，保证实际取字段；消费本身有成本，因此所有路线同口径保留，不宣称零开销。

### 写入边界

```text
计时外：生成输入
init：open/config
create：建表事务（BEGIN、CREATE TABLE、COMMIT）
begin：数据BEGIN
write data开始
    prepare一次INSERT
    每条记录：bind 8字段 → step至DONE → reset
    finalize
write data结束
commit：数据COMMIT
close：关闭（A保留连接至读取完成）
```

文本write data为格式化、输出及flush，open/close单列。write complete=begin+write data+commit；不与read data混称对偶指标。SQLite的write data仍可能包含脏页写出、journal维护和事务工作；COMMIT单列不等于排除全部I/O。

### 读取边界

```text
计时外：缓存准备
read init：open/config（A复用连接，不虚构open耗时）
read data开始
    prepare一次SELECT
    循环：step返回ROW → 取8字段 → 复用Record → 消费
    最后step返回DONE → finalize
read data结束
read close：关闭连接
```

文本read data包含解析全部字段和同样消费。主表只比较write data与write data、read data与read data；create/commit另表展示。

## 4. 写入：原因、证据、验证

| 候选原因 | SQLite实际额外工作 | 如何验证 |
| --- | --- | --- |
| 参数绑定 | 把值交给参数槽；TRANSIENT复制两个字符串 | 保留标准TRANSIENT组，增加仅改STATIC的配对组；输入存活至finalize |
| 每条INSERT执行 | VM取参数、生成rowid、编码record、插入B-tree、语句收尾 | EXPLAIN、RUN/VM_STEP/REPREPARE；perf区分VM、B-tree、绑定热点 |
| 页管理与写出 | 维护脏页，可能在提交前溢出 | write与COMMIT分别读取CACHE_WRITE/SPILL增量；B不能仅凭时间就称sync慢 |

STATIC对照保持所有INSERT、事务和最终DB内容相同。差值是改变绑定生命周期后的净影响，包含缓存/分配连带效应，不是memcpy函数的独占时间。

先A后B复核。只有B出现明显写页/溢出且需要进一步区分时，再设计单独的缓存或同步单因素实验，不用A−B推算磁盘时间。必要时另跑系统调用诊断检查fsync/fdatasync；其耗时不纳入正式性能数据。

## 5. 读取：原因、证据、验证

| 候选原因 | SQLite实际工作 | 如何验证 |
| --- | --- | --- |
| step执行 | 扫描表、解码列、生成结果寄存器、返回ROW | EXPLAIN/EQP、RUN/VM_STEP/FULLSCAN_STEP；perf定位 |
| 取列与应用保存 | column API类型访问/转换、字符串复制、消费 | perf区分取列/复制与step；不得直接删掉fetch后与完整读取比较 |
| 缓存访问 | Pager命中或读页 | CACHE_HIT/MISS；miss不是物理磁盘读取次数 |

首轮不加入只step、SELECT少数字段或只检查长度的“加速组”：它们减少了实际工作，不能证明完整读取成本。若取列路径确为热点，再独立审批borrowed-view对照，并为文本定义相同结果语义；不混入本轮主对照。

## 6. 如何实现统计且不污染时间

- benchmark.cpp：移除sampled路径及prepare/finalize内层时钟；将现有operation末尾stmt_status查询移出正式模式。用编译期模板区分正式/统计路径，避免新增逐行模式判断。
- 统计运行：新prepare的statement计数初始为零；循环结束、finalize前读取RUN/VM_STEP/REPREPARE/FULLSCAN_STEP/SORT/AUTOINDEX。查询一次返回多行不是多次RUN；限定当前支持的计数和数据规模，检查负值及SQL不变量，不支持/异常不静默当零。
- db_status在目标阶段前后取值求差；create、write、commit、read独立快照，期间不执行配置输出/EXPLAIN等额外SQL。正确处理每个指标的current/highwater语义和错误码。
- run.py：分开correctness、timing、counters批次。每组1次预热、5次正式，轮换顺序，固定等待；原始样本、二进制/数据哈希、实际参数及统计全部保留在仓库外。
- run_perf.sh：保留阶段门控、相同二进制无perf控制；默认不在正式时间进程上采样。不增加SQLite源码插桩或SQL逐条trace。

官方依据：[语句统计](https://www.sqlite.org/c3ref/stmt_status.html)、[连接统计](https://www.sqlite.org/c3ref/c_dbstatus_options.html)、[执行架构](https://www.sqlite.org/arch.html)。源码及版本限制见[执行分析](../../sqlite-reference/execution_analysis.md)、[源码路径](sqlite_source.md)。

### SQLite已有计数器是什么意思，读取有什么代价？

这些计数由SQLite本身在执行VM及访问Pager时维护，不是本测试增加的逐行计数。`sqlite3_stmt_status`取某个statement的计数；`sqlite3_db_status`取连接下Pager等状态。它们不重新执行SELECT，也不通过扫描表计算次数。

- 3.37.2普通statement计数读取：访问`aCounter[op]`，可选择清零；本实验使用reset=0。是函数调用和内存访问，不是零成本，也不适用于MEMUSED这种特殊统计。
- 缓存计数读取：连接互斥锁、遍历attached database的Pager并汇总内存计数；成本与连接中数据库数有关，不随表行数逐行扫描。
- 不声称一次读取耗时多少ns。本测试每个阶段前后各取4个缓存计数、每个statement结束取6个语句计数；全部在独立进程进行。正式模式不调用这些接口，SQLite原本维护计数的成本则仍属于真实运行库成本。

源码：[vdbeapi.c:1728](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbeapi.c#L1728)、[status.c](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/status.c#L353)。实现：[缓存快照](../benchmark.cpp#L95)、[阶段边界](../benchmark.cpp#L107)、[语句循环及诊断模板](../benchmark.cpp#L190)。

## 7. 结果怎么解释、何时算完成

分别输出写、读两个结论表：text、SQLite标准、写入STATIC对照；报告中位数、min–max、每组样本和差值。独立计数表解释执行了多少工作，CPU热点表解释热点在哪里。

同一轮配对delta=SQLite阶段时间−text阶段时间；绑定delta=TRANSIENT−STATIC。除以N得到每条记录净代价，不能从ns猜测CPU指令数。样本差值方向不稳定时报告“未能分辨”，必要时增加配对轮次，不硬给因果结论。

完成要求：正确性全部通过；正式路径无行内诊断；计数符合SQL实际执行；perf有控制及扰动报告；每项结论标明实测差值、CPU占比或未量化候选。若perf改变阶段时间超过5%，降低采样频率复核，不能宣称5%以内等于无扰动。

**本方案能可靠回答总共慢多少、哪些路径最值得检查、改变特定因素的净差值。不能无扰动地精确拆出每个内部函数的wall time；没有数据支持的部分明确保留未知，不凑齐总时间。**
