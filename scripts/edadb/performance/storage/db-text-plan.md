# SQLite／DuckDB／RocksDB与文本：独立实验交接计划

## 1. 目标和状态

目标：用相同逻辑记录，完整写入、完整读取并恢复相同字段，验证哪些数据库及API在什么条件下比文本快。分别报告读和写，不预设胜者，不以COUNT、投影少数字段或计时外编码替代完整工作。

**状态：历史设计方案，不是当前执行状态或验收报告。** 独立目录已完成部分三库及Parquet平面实验；当前实测入口为`/home/zhiyiwang/cs/db/sqlite/bench/results-brief.md`与`final-report.md`，版本/配置以其manifest为准。下文矩阵包含尚未全部实施的扩展，不能因已有结果就宣称整份计划完成。旧iEDA仓库SQLite结果不与独立批次相减，外部源码和结果也不随本仓库提交。

## 2. 目录与获取代码

在cherry13的`/home/zhiyiwang/cs/db`使用三个数据库目录；已有sqlite目录及旧源码必须保留：

```text
/home/zhiyiwang/cs/db/
├── sqlite/   sources/<version>/  build/<version>/  bench/  results/<run-id>/
├── duckdb/   sources/<version>/  build/<version>/  bench/  results/<run-id>/
└── rocksdb/  sources/<version>/  build/<version>/  bench/  results/<run-id>/
```

统一Record、生成器、文本实现、校验器、runner放`sqlite/bench/common/`，另外两库引用这一份，不复制分叉；总报告放`sqlite/bench/report.md`，各库bench内只放后端实现及入口。路径为组织方案，sqlite目录名不表示公共测试逻辑依赖SQLite。

- SQLite：检查已有`/home/zhiyiwang/cs/db/sqlite`；需要新版本时从[官方下载](https://www.sqlite.org/download.html)获取amalgamation/source，固定版本和SHA256，不覆盖旧目录。
- DuckDB：从[官方仓库](https://github.com/duckdb/duckdb)固定release tag并解析commit；使用稳定C API，源码或匹配平台的官方库二选一并记录。
- RocksDB：从[官方仓库](https://github.com/facebook/rocksdb)固定release tag/commit，Release构建；记录压缩库、编译选项及实际链接库。
- 不跟随浮动main。报告记录版本、源码/二进制哈希、编译器、优化参数、CPU/NUMA、内存、文件系统、磁盘与RAID、线程配置。
- 新目录不在当前iEDA可写范围时，按执行环境申请权限；不要以复制/删除旧仓库绕过限制。构建结果及大数据不纳入iEDA Git。

## 3. 相同逻辑schema，不伪称相同物理schema

```text
Record:
  name, master_name: UTF-8字符串
  source, status, orient, x, y: int32
  record_order: int64
```

SQLite与DuckDB使用同样的列名、顺序和数值范围：

```sql
CREATE TABLE component (
  name TEXT, master_name TEXT, source INTEGER, status INTEGER,
  orient INTEGER, x INTEGER, y INTEGER, record_order BIGINT
);
```

不加业务PK/FK、额外索引或NULL值，保留各引擎自己的存储机制。SQLite INTEGER与DuckDB INTEGER类型实现不同，记录这一点，不强求物理字节相同。

RocksDB是键值库，没有SQL表：key为record_order的8字节大端非负整数；value编码其余7字段：两字符串各用固定宽度长度前缀＋UTF-8字节，五整数使用固定字节序int32。禁止直接memcpy C++结构体。读取必须解码value并从key恢复record_order。**编码、分配、复制均计入写；解码与最终Record保存计入读。** 不增加业务name索引；key唯一是RocksDB模型的必要差异，不能声称与无PK SQL表物理等价。

文本为同样8字段的空格分隔、换行结尾；首批生成字符串不含空格/换行。以后扩展任意字符串必须定义统一转义及校验，不能悄悄改变文本工作量。

数据规模：先0、1、9、10、11、99、100、101条验证，再1万、100万条性能；资源允许再1000万条，作为独立规模。首批沿用历史生成规则：name=U加7位序号、master_name=bench_cell、source/status/orient=2/3/1、坐标按序号生成、record_order=i。报告同时记录行数、逻辑编码字节数、文本字节数、全部DB/WAL/SST等文件字节数，不能用另一批的43.7MB替代实测。

第二批再加入不同字符串长度和低重复度内容，避免固定字符串压缩优势被推广到任意数据。压缩开关必须记录；默认配置与禁用压缩对照分开，不把压缩算法差异叫API开销。

## 4. 实验A：单数据集完整写入与读取

### A1. 写入方法

| 后端 | 基本路径 | 优化路径 | 需要记录 |
| --- | --- | --- | --- |
| 文本 | 缓冲ofstream逐行格式化输出 | 有界缓冲＋to_chars格式化＋块写出 | 缓冲大小、flush/fsync、实际输出字节一致 |
| SQLite | prepared单行INSERT、TRANSIENT绑定 | 每条INSERT 10/100行；再独立比较安全STATIC | prepare/执行次数、参数上限、尾批、事务数 |
| DuckDB | prepared逐行INSERT，显式事务 | Appender逐行接口；再比较Data Chunk追加 | chunk大小、填充和字符串复制、flush/close/commit |
| RocksDB | Put | WriteBatch 10/100/1000条及有界字节预算 | 编码、批数、WAL/sync、memtable、后台工作 |

SQL两库先固定一笔完整数据事务；RocksDB小WriteBatch仅保证各批原子性，**不等于整个数据集一笔原子事务**。所以此主矩阵只能称吞吐/快照生成比较，不能声称原子性完全一致。若要统一原子单元，另测每100条提交一批：SQL事务100条、RocksDB WriteBatch100条；文本仅是快照输出，不能冒充通用事务数据库。

不能将RocksDB全量百万行WriteBatch作为唯一最快路径而不报告其内存；也不能只测DuckDB逐行INSERT而忽略其批量接口。优化文本必须参与最终胜负判断，避免仅击败较慢的iostream实现就宣称优于文本。

### A2. 读取方法

| 后端 | 完整读取 |
| --- | --- |
| 文本 | 逐行解析8字段；另测相同内容的有界块解析版本 |
| SQLite | 一次SELECT全8列，持续step、column取值并恢复Record |
| DuckDB | 查询执行与结果获取都计时；按chunk访问向量，逐记录恢复同样8字段，不只统计chunk行数 |
| RocksDB | 一个Iterator从首条推进到末尾，解码key/value成Record，检查iterator status |

正式循环每条都做相同字段摘要消费，不保存整套结果、不跳过字符串。禁止在计时前执行DuckDB查询/物化结果再只计fetch；禁止提前解码RocksDB。

提供两个明确的输出协议，分别排名：
1. **无序全量恢复**：内容全部取得，行顺序不要求；使用顺序无关摘要，完整验证独立检查位置覆盖及重复。SQL不加ORDER BY。
2. **按record_order恢复**：SQL显式ORDER BY record_order，包含排序代价；文本按生成顺序读取，RocksDB按大端key迭代。不能依赖SQLite隐式rowid或DuckDB偶然返回顺序作为通用保证。

如使用record_order作为位置直接填入数组，则三个后端及文本都遵守同一协议，并把数组分配/写入计时与内存计入，另立实验，不替换上面流式扫描。

## 5. 计时与持久性：决定比较是否成立

统一边界：

```text
生成输入、准备路径                         计时外
open/config                               init
创建表/列族/文件                          create
开始事务（适用时）                        begin
准备writer、编码/格式化、完整写入          write data
flush appender/buffer、commit/WAL同步       finish（不与data重复）
关闭                                      close
write complete = begin + data + finish + close（按每个样本求和）

新进程打开已有数据                        read init
执行查询/创建迭代器、取得全部数据、解码消费 read data
销毁结果、关闭                            read close
```

若API把flush包含在写调用内则留在data，明确记录，不人为挪到计时外。最终比较以完整write complete为主、data为诊断；init/create另列，同时给从创建到完成的端到端时间。结果中各阶段中位数不能相加代替总时间中位数。

两种持久性模式分表：
- **非同步确认**：文本flush到OS；RocksDB WAL开启、sync=false；SQL数据库仅使用有官方支持并已验证的非同步设置。DuckDB若无对应设置，不强行归入等价组。不把WAL关闭结果混入主表。
- **持久化确认**：文本flush＋fsync（新文件发布还需处理目录同步）；SQLite保留FULL并提交；DuckDB保留持久化事务提交；RocksDB WAL开启且按明确边界sync。API返回的持久化保证与故障模型须逐库核对，未做掉电测试不能宣称实证掉电安全。

RocksDB写入返回后可能仍有memtable flush/compaction；DuckDB可能还有checkpoint工作。必须另报后台完成/物化阶段和最终文件大小：记录同步flush、必要checkpoint或等待任务的时间及调用，不直接把某库强制compact而另库不做维护称为等价。主结果同时给“确认完成”与“后台工作状态”，追加多轮持续写入检验是否只是把成本推迟。

## 6. 实验B：三集合交织（第二阶段）

先完成A再做B。三SQL表t0/t1/t2各10万条；RocksDB对应同一DB的三个column family；文本三个文件。同8字段，source=2/3/4，无父子关系。

逐条切换、每10条切换、按集合完成；固定批量API粒度再改变切换粒度，不把两者一起改变。写比较固定读；读比较固定写。每集合游标/statement复用，不每次切换重新查询。三列族不能等价为三个独立数据库，需记录共享缓存及后台资源。

## 7. 正确性、运行与统计

- 共用一套C++生成器/Record/文本/消费/验证逻辑；不使用Python逐行读写数据库，Python仅调度。各后端连接同一轮输入指纹。
- 校验全部字段、记录数、唯一序号、空集、尾批、重复写行为、字符串边界和错误返回。吞吐测试只使用唯一键；另明确RocksDB重复Put覆盖而SQL无PK可能新增，不能将两者混测。
- warm读取：生成后关闭，新进程读，统一预热策略。独立做冷缓存时需授权并验证缓存清理方法；重开连接不等于cold。
- 单前台线程和统一资源预算为首组；DuckDB内部线程、RocksDB后台线程必须记录/约束，不声称只绑调用线程就等于引擎单线程。多核最大吞吐另表测试，不能和单线程文本直接解释为架构收益。
- 编译与小型正确性可并行；正式样本串行、轮换顺序、预热1次至少正式5次，疑似小收益增加重复。定时心跳，按实际进度更新ETA。
- 保存逐次时间、median/min/max、配对差值、DB÷文本耗时比、records/s、峰值RSS、CPU时间、磁盘字节；计数器和perf独立采集，不插入高频计时。
- 每个候选必须同时通过完整内容验证与复开恢复。两组中位数轻微不同且配对波动跨零，只报告不确定，不宣布胜出。

## 8. 交付与执行顺序

1. 检查已有目录和资源，确定三库版本与链接方式；保存源码身份及完整配置，避免重复下载SQLite。
2. 实现公共Record、两种文本路径及正确性框架，再接入三库，先验证计时边界和持久性说明。
3. A先跑1万条smoke，100万条正式单线程文件库；先给完整读写结果，再解释data/finish/后台成本。
4. 验证原生批量API、稳定性与持续写，之后再加B和大规模/字符串/并行扩展，避免一次膨胀矩阵。
5. 输出总report.md：环境→schema→写结果→读结果→正确性→归因→限制；每库results保存manifest、samples.tsv、summary、audit。结论精确写“哪个模式/数据量/API比哪种文本快多少”，允许没有数据库胜出。

官方接口依据：[SQLite step](https://www.sqlite.org/c3ref/step.html)、[DuckDB Appender与Data Chunk](https://duckdb.org/docs/current/clients/c/appender)、[RocksDB WriteBatch、同步与Iterator](https://github.com/facebook/rocksdb/wiki/Basic-Operations)。版本相关API、线程及持久性细节必须在实际固定版本时再核对。
