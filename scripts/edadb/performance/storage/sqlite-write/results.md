# SQLite与文本：A单表粒度、B三表交织

状态：本提交保存实测与归档审计，提交前验证见[readme](readme.md)。未push、未建立新milestone。本页不覆盖旧冻结实验，不包含外部DuckDB/Parquet新结果；此次验证不替换正式性能数据。

**A改变每条INSERT包含的记录数；B改变三个表之间的读写切换顺序。这是两套实验，不是同一个“batch”。** 两套实验都与文本比较，读写分别报告。本页仅汇总已有测量，不新增或混合批次。

| 实验 | 要回答的问题 | 数据规模 | 改变的因素 |
| --- | --- | --- | --- |
| A：单表INSERT粒度与读取对照 | 多行INSERT是否更快？其生成的数据读取是否不同？与文本差多少？ | 单表100万条，对照单文本文件100万行 | 每条INSERT写1／10／100行；读取方式不变 |
| B：三表交织 | 每条、每10条、按表完成的调度是否影响读写？与文本差多少？ | 三表各10万条，对照三个文本文件各10万行 | 单行INSERT不变；分别改变写切换或读切换粒度 |

**命名约定：A/B只表示实验类型。存储配置统一写“内存／文件”，不再用A或B-batch命名配置。** 原始证据中的旧配置标识保留，避免改写历史数据。

## 共用schema与统计口径

### 数据schema与规模

实际单表DDL如下；三表实验仅将表名替换为t0、t1、t2，字段定义完全一致：

```sql
CREATE TABLE component (
  name TEXT, master_name TEXT,
  source INTEGER, status INTEGER, orient INTEGER,
  x INTEGER, y INTEGER, record_order BIGINT
);
```

未声明PRIMARY KEY、UNIQUE、FOREIGN KEY或额外索引；不是WITHOUT ROWID表。record_order只是普通数据列，不是索引。生成数据均非NULL。

| 数据字段 | 生成规则，i从0开始 |
| --- | --- |
| name | U加7位补零序号，如U0000000 |
| master_name | 固定bench_cell |
| source / status / orient | 单表为2 / 3 / 1；三表source分别为2、3、4 |
| x / y | (i % 1000) × 100 / (i / 1000整数除法) × 100 |
| record_order | i，C++端使用int64_t |

| 实验 | SQLite数据量 | 文本数据量 |
| --- | --- | --- |
| 单表INSERT粒度与完整读取 | 1表×1,000,000条，共8,000,000个字段值 | 1文件×1,000,000行，同序8字段 |
| 三表交织写入与读取 | 3表×100,000条，共300,000条、2,400,000个字段值 | 3文件×100,000行，同表对应同文件 |

文本每行按schema顺序以空格分隔、换行结束；不是DEF格式。这里的数据量指逻辑记录数，不是数据库文件字节数。两类实验总记录数不同，不能直接拿总耗时相减。三表source不同用于发现表之间的数据错配，不涉及关联查询。


- 内存配置：SQLite MEMORY日志、OFF同步；文件配置：SQLite DELETE日志、FULL同步。两种配置的文本参照都使用普通文件，不是内存文本。
- GCC 11.4、Release -O3、SQLite 3.37.2；固定CPU，正式串行轮换5次。所有表格单位ms，取中位数；耗时比＝SQLite÷对应文本，越小越好。
- 写data：SQLite包含SQL准备、绑定、step/reset及finalize；文本包含格式化、输出、flush。不包含init、create、BEGIN、COMMIT、close。
- 读data：完整读取8字段、保存至复用Record、消费相同摘要；SQLite还包含prepare/finalize。不用COUNT或step-only代替完整读取。
- 文件在计时外预读，SQLite文件库新开读连接；内存库沿用写连接首次读取。属于warm场景，不代表cold。
- 文本flush不等于fsync，不声称与SQLite FULL有相同持久性。两套实验数据量及批次不同，总时间不跨实验相减。

## A. 单表INSERT粒度与读取对照

### A.1 方法：改变单条INSERT的行数，不改变读取方式

单表component共1,000,000条，对照一个同内容文本文件。SQLite每组均一笔数据事务；所有参数使用相同绑定策略。

| SQLite写法 | INSERT执行次数 | 数据事务数 | 随后如何读取 |
| --- | ---: | ---: | --- |
| INSERT ... VALUES (8个参数) | 1,000,000 | 1 | 一次SELECT，逐行step并取全部8列 |
| INSERT ... VALUES (8个参数), ... 共10组 | 100,000 | 1 | 同上 |
| INSERT ... VALUES (8个参数), ... 共100组 | 10,000 | 1 | 同上 |

读取SQL为`SELECT name,master_name,source,status,orient,x,y,record_order FROM component`。三组均只执行一次SELECT，持续step读取同一游标，不逐行或每10行重新查询。读取表中的“单行／十行／百行”仅表示数据库的写入方式，读取方法相同。

下表只比较read data和write data；连接、建表及提交不在比较范围内，原始阶段时间保留在[统计文件](results/rw-summary.json)。

40组全字段检查（含全量预热）、40个正式进程共80条读写时间、6个独立计数进程通过。正式读取仅做摘要消费，完整字段检查在独立check进程中；见[审计](results/rw-audit.json)。

### A.2 写入结果

单位ms，5次中位数；耗时比＝SQLite写入中位数÷同组文本写入中位数，越小越好。

| 实现 | 内存配置写入 | 相对文本耗时比 | 文件配置写入 | 相对文本耗时比 |
| --- | ---: | ---: | ---: | ---: |
| C++文本 | 738.134 | 1.000 | 739.137 | 1.000 |
| SQLite单行INSERT | 1505.569 | 2.040 | 1565.399 | 2.118 |
| SQLite十行INSERT | 1009.162 | 1.367 | 1070.581 | 1.448 |
| SQLite百行INSERT | 936.963 | 1.269 | 1003.515 | 1.358 |

单行SQLite相对文本，同轮净差中位数为 **+767.435／+828.459 ms**；百行后仍为 **+198.414／+264.885 ms**。100万条下，这分别等于每条约767.435／828.459 ns和198.414／264.885 ns净差，不是某个函数的独占代价。

**写入takeaway：批量后明显接近文本，但仍慢约27%／36%。** 事务均为一笔，收益不是减少COMMIT次数。计数确认单行/十行/百行RUN分别为100万/10万/1万，VM_STEP为1600万/1480万/1408万；字段绑定与记录数不变。此补测没有分解剩余198～265ms，也没有测试STATIC组合，不能把剩余差距全部归给字符串复制。

### A.3 读取结果

单位ms，5次中位数；耗时比以同组文本**读取**为分母，不使用写入时间。

| 数据来源／读取实现 | 内存配置读取 | 相对文本耗时比 | 文件配置读取 | 相对文本耗时比 |
| --- | ---: | ---: | ---: | ---: |
| C++文本解析全部字段 | 583.620 | 1.000 | 583.794 | 1.000 |
| SQLite：单行INSERT生成的库 | 790.186 | 1.354 | 833.808 | 1.428 |
| SQLite：十行INSERT生成的库 | 789.008 | 1.352 | 822.994 | 1.410 |
| SQLite：百行INSERT生成的库 | 794.260 | 1.361 | 832.164 | 1.425 |

单行组SQLite相对文本，同轮净差中位数为 **+203.413／+249.462 ms**，每条约203.413／249.462 ns。配对差值范围分别为+198.892～+232.357ms、+244.880～+253.006ms，5轮均慢于文本。

三条SQLite路线读取均为1次SELECT、10,000,006个VM步、999,999个FULLSCAN_STEP，SORT/AUTOINDEX/REPREPARE均0，结果摘要一致。它们不是三种读取优化，而是写入方式不同后的相同读取控制。

**读取takeaway：SQLite完整读取仍比文本慢约35%～43%；改善写入语句粒度不能直接消除读取成本。** SQLite的VM取行/解码、column接口及应用字段保存仍要执行；文本也需解析和保存。本批证明总差距与工作量，没有重新采样读取CPU，也没有逐项测出这些环节各占多少ms。上一轮仅写入的perf不作为本轮读取热点证据。


## B. 三个表不同交织的实验

### B.1 方法：仅改变表之间的切换顺序

**Schema：同一个SQLite数据库中的三张独立表t0、t1、t2，均为8字段，彼此没有主外键关联。** 实际DDL如下：

```sql
CREATE TABLE t0 (
  name TEXT, master_name TEXT, source INTEGER, status INTEGER,
  orient INTEGER, x INTEGER, y INTEGER, record_order BIGINT
);
CREATE TABLE t1 (
  name TEXT, master_name TEXT, source INTEGER, status INTEGER,
  orient INTEGER, x INTEGER, y INTEGER, record_order BIGINT
);
CREATE TABLE t2 (
  name TEXT, master_name TEXT, source INTEGER, status INTEGER,
  orient INTEGER, x INTEGER, y INTEGER, record_order BIGINT
);
```

- 三表均无显式PRIMARY KEY、FOREIGN KEY、UNIQUE、NOT NULL或额外索引；各自仍有隐式rowid及表B-tree。record_order只是普通字段。
- 每表100,000条，总计300,000条。字段生成规则相同，仅source分别为2、3、4，用于识别表间错配；生成值均非NULL。
- 文本对照为三个独立文件，各100,000行，每行按上述列顺序保存8字段。表t0/t1/t2分别对应文件0/1/2。
- 写入共用一个连接、一笔数据事务，三条单行INSERT分别prepare并复用；读取各表使用一次完整SELECT并ORDER BY rowid，持续推进三个游标。没有JOIN或逐父子查询。

因此本实验只检验三张独立表之间的交织调度，**不包含索引维护、FK检查或父子对象恢复的额外成本**；不能直接推广到adapter的关联表读写。

本节是独立批次：每表100,000条，三表总计300,000条；不是上面单表100万条。三文件文本具有相同数据及调度。每条切换为ABC循环，每10条为A×10、B×10、C×10循环，按表为A全部→B全部→C全部。全部为单行INSERT，**十条连续执行不等于一条十行INSERT**。

20种组合各5次正式运行，共200条读写时间；100组预检查/预热、100个正式进程和10个计数进程均通过完整字段验证。计时后验证不进入data；正式样本按轮次交替顺序，串行固定CPU。以下单位ms，5次中位数；连接、建表、BEGIN、COMMIT不包含在写data中，详见[阶段及范围](results/interleave-summary.json)。

### B.2 写入结果

固定读取按表完成；SQLite三张表共用一次数据事务，文本同时打开三个文件。

| 切换粒度 | 文本（内存组） | SQLite内存 | SQLite/文本 | 文本（文件组） | SQLite文件 | SQLite/文本 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 每条切换 | 225.977 | 446.381 | 1.975 | 224.148 | 516.180 | 2.303 |
| 每10条切换 | 222.617 | 445.441 | 2.001 | 222.677 | 533.091 | 2.394 |
| 按表／文件完成 | 220.363 | 442.957 | 2.010 | 220.205 | 511.399 | 2.322 |

**写入takeaway：本轮不支持“交织是SQLite慢的主要原因”。** 按表写相比每条切换，中位数下降0.77%（内存）／0.93%（文件）；同轮差值分别为−3.424ms（−10.756～+2.103）、−3.889ms（−8.931～+10.357），均有正有负。不能把不足1%的变化认定为稳定收益。文件每10条组反而较慢，差值也跨零，没有证据归因于特定SQLite内部机制。

三种写法都是300,000次INSERT执行、相同绑定数，不像多行INSERT减少statement执行次数。文本按文件完成比逐条切换快约2.48%／1.76%；SQLite仍约为对应文本的1.98～2.39倍。文本flush与SQLite FULL持久性不同，COMMIT仍单列，不用它解释本表全部差距。

### B.3 读取结果

统一使用按表写入生成的数据；三个SELECT各执行一次，游标持续step，并完整取出8字段；文本对应三个输入流，不是只计数。

| 切换粒度 | 文本（内存组） | SQLite内存 | SQLite/文本 | 文本（文件组） | SQLite文件 | SQLite/文本 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 每条切换 | 173.829 | 240.967 | 1.386 | 174.037 | 271.179 | 1.558 |
| 每10条切换 | 172.489 | 237.657 | 1.378 | 172.158 | 274.237 | 1.593 |
| 按表／文件完成 | 171.536 | 236.606 | 1.379 | 171.396 | 274.427 | 1.601 |

**读取takeaway：减少游标切换不是消除SQLite与文本差距的主要手段。** 内存按表读比逐条切换中位数低1.81%，同轮净差−3.571ms（−4.645～−2.339），5轮一致改善；文件组没有一致方向，同轮差值−2.865ms（−5.020～+5.109）。配对差值中位数不等于两组中位数之差。SQLite仍约为文本的1.38～1.60倍。

所有SQLite读组均RUN=3，SORT/REPREPARE=0；写组RUN=300,000，SORT/REPREPARE=0。切换游标并没有重新发起查询。本批未重新采样CPU，不将微小变化猜测为缓存或B-tree的独占开销；也不能推广到带索引/FK、更多表或cold场景。

证据：[原始200条计时](results/interleave-samples.tsv)、[独立配对复算](results/interleave-review.json)、[运行审计](results/interleave-audit.json)、[计数](results/interleave-counters.tsv)、[配置](results/interleave-config.json)、[源码指纹](results/interleave-manifest.json)。源码及复现入口见[readme](readme.md)；原始日志在服务器`/tmp/sqlite-interleaved-run-01`，生成数据验证后删除。旧写入批次不覆盖，不跨批相减。

### B.4 为什么每10条切换有时更慢？

首先区分表格：**533.091ms是文件SQLite写入，不是读取**；文件SQLite每10条读取为274.237ms，逐条读取为271.179ms。

| 文件SQLite对照：每10条减逐条 | 两组中位数之差 | 同轮差值中位数 | 同轮差值范围 |
| --- | ---: | ---: | ---: |
| 写入 | +16.911ms（+3.28%） | +14.659ms | −2.048～+56.113ms |
| 读取 | +3.058ms（+1.13%） | +0.630ms | −4.640～+5.212ms |

两组差值都跨零，不能认定每10条必然更慢，也不能反过来保证更快。内存每10条读取的中位数反而更低（237.657 vs 240.967ms）。已确认各组记录数、语句执行次数一致且没有SQL排序；本轮没有证据证明文件组的变化由某项缓存、I/O或SQLite内部机制导致，不猜测具体原因。若要将这点小差异作为优化依据，应再增加配对重复次数并独立诊断，不把诊断开销加入正式计时。

## 阅读与复现

先看实验A，再看实验B，各自分别阅读写入与读取结果，再看[计时定义](test_plan.md#完整读写补测)及[运行方法](readme.md)。

- [原始80条时间](results/rw-samples.tsv)、[统计及范围](results/rw-summary.json)、[配对净差与耗时比](results/rw-comparisons.json)。
- [计数](results/rw-counters.tsv)、[实际配置](results/rw-config.json)、[源码及二进制指纹](results/rw-manifest.json)。
- [补测运行器](run_read_write.py)、[独立审计](audit_read_write.py)；底层复用冻结的[原生SQLite/text实现](../sqlite-vs-text/benchmark.cpp)，不修改其源码。
- 服务器原始日志在`/tmp/sqlite-read-write-run-01`，库及文本验证后删除；上轮写入证据保留，不重写归属。
