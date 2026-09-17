# 测试汇总：baseline测试


## 1. 项目与资料位置

项目：`../../../../..`。
实验目录：`..`。

| 内容 | 文件相对路径链接 |
| --- | --- |
| 计划与配置 | [baseline/readme.md](readme.md)、[sqlite-reference/config.md](../sqlite-reference/config.md) |
| SQLite机制 | [sqlite-reference/runtime.md](../sqlite-reference/runtime.md) |
| 实现与计时位置 | [benchmark/implementation.md](../benchmark/implementation.md) |
| C++代码与对象定义 | [benchmark/stream_benchmark.cpp](../benchmark/stream_benchmark.cpp)、[benchmark/benchmark_support.h](../benchmark/benchmark_support.h) |
| 数据生成与调度 | [baseline/fixtures.py](fixtures.py)、[baseline/run.py](run.py) |
| 实测依据 | [baseline/results.md](results.md) |

实验归档提交为`4f49ce07b`，当前`prof-test`包含该提交；测量时版本和归档版本的关系见完整报告，不将当前HEAD冒充测量时HEAD。

## 2. 对比设计与数据

| 路线 | write | read | 比较时必须区分 |
| --- | --- | --- | --- |
| 原生iEDA | DefWrite输出DEF | DefRead解析并恢复完整iDB | 主基线 |
| C++文本 | 格式化输出8字段 | 解析到复用Record | 简化文本，不是完整DEF parser |
| SQLite直接 | prepare、bind/step/reset、finalize | SELECT取8列到复用Record | 简化数据库表示，不恢复完整iDB |
| EDADB直接 | makeInsertOp、循环insert | makeReadAllOp、循环readNext | 同SQLite schema的映射成本 |
| adapter | iDB→Shadow→EDADB | EDADB→完整iDB | 最接近实际替代DEF存取的路径 |

direct的component表：name、master_name两个TEXT；source、status、orient、x、y五个INTEGER；record_order一个BIGINT。无显式PK/FK/索引。
adapter保留应用schema：41张表，Instance表19列、name主键、按_order_sd排序及引用重建。两者不是完全相同的对象工作量。

**约束成本取决于具体schema，不取决于是否叫SQLite路线。**本单表实验的SQLite/EDADB直接路线没有声明主外键；
adapter的实际表具有主键等应用结构，相关数据库工作仍由SQLite执行，不是EDADB独有的成本。
另外的主外键两表实验才为两条API配置相同约束来对照；不能混用两组实验的schema解释时间差。
无显式主键不意味着没有SQLite表存储结构与页管理成本；也不能仅因连接开启foreign_keys就认定表中存在外键。

### 测试对象与数据库schema

下面是实际测试对象的存储字段；辅助比较函数省略。SQLite直接路线和EDADB直接路线共用此对象。

```cpp
struct ComponentRecord {
    std::string name, master_name;
    int32_t source, status, orient, x, y;
    int64_t record_order;
};

TABLE4CLASS(ComponentRecord, "component",
    (name, master_name, source, status, orient, x, y, record_order));
```

EDADB测试入口另设`Cpp2SqlTypeTrait<ComponentRecord>::hasPrimKey = false`，不将name自动当主键。
对象和映射见[benchmark_support.h:111](../benchmark/benchmark_support.h#L111)，
关闭主键见[stream_benchmark.cpp:171](../benchmark/stream_benchmark.cpp#L171)。

SQLite直接路线实际DDL（EDADB生成同字段/类型的表）：

```sql
CREATE TABLE component (
    name TEXT,
    master_name TEXT,
    source INTEGER,
    status INTEGER,
    orient INTEGER,
    x INTEGER,
    y INTEGER,
    record_order BIGINT
);
```

无显式PRIMARY KEY、FOREIGN KEY、二级索引或NOT NULL约束。record_order是普通数据列，不要求SELECT排序。
建表源码：[stream_benchmark.cpp:188](../benchmark/stream_benchmark.cpp#L188)。

| 字段 | C++类型 / SQL类型 | 语义及DEF对应 |
| --- | --- | --- |
| name | string / TEXT | Component实例名 |
| master_name | string / TEXT | 引用的cell master名 |
| source | int32_t / INTEGER | SOURCE，测试为DIST |
| status | int32_t / INTEGER | 放置状态，测试为PLACED |
| orient | int32_t / INTEGER | 朝向，测试为N |
| x、y | 各int32_t / INTEGER | 放置坐标，单位为DEF数据库单位 |
| record_order | int64_t / BIGINT | 原始记录序号；在DEF中用记录位置表达，不是独立DEF tag |

**为什么用这张表：**既有字符串又有整数，同时能映射实际COMPONENT字段，便于比较格式处理和基础存取；
先不引入主外键、子表和复杂约束，减少变量。它不是全部iDB对象的schema。

adapter不使用上面的简化TABLE4CLASS，而使用`Shadow<IdbInstance>`的实际映射。
其schema见[edadb_idb_schema.h:91](../../../../../src/database/edadb/idb/edadb_idb_schema.h#L91)。
因此“逻辑输入同源”不等于“五条路线执行相同SQL、存储相同列数”。

### 数据生成与同一条记录的表示

合成数据：name为U加7位补零序号，master_name=bench_cell；source=2、status=3、orient=1；
x=(index%1000)*100，y=(index/1000)*100，record_order=index。输入对象、LEF及canonical DEF在计时外准备。

例如index=1，三种表示分别是：

```text
Record / 简单文本的字段顺序：
U0000001 bench_cell 2 3 1 100 0 1

DEF生成器输出：
- U0000001 bench_cell + SOURCE DIST + PLACED ( 100 0 ) N ;

SQLite绑定值（name至record_order）：
('U0000001', 'bench_cell', 2, 3, 1, 100, 0, 1)
```

DEF还包含VERSION、DESIGN、UNITS、DIEAREA等必要上下文；最小LEF定义bench_cell。
生成器见[fixtures.py:15](fixtures.py#L15)，
C++对象生成见[benchmark_support.h:133](../benchmark/benchmark_support.h#L133)。

| Component数量 | 输入DEF大小（bytes） |
| ---: | ---: |
| 1,000 | 72,098 |
| 10,000 | 737,091 |
| 100,000 | 7,477,012 |
| 1,000,000 | 75,776,213 |

1,000,000条的简单文本为44,664,890 bytes；SQLite B-batch DB为43,732,992 bytes；adapter DB为75,517,952 bytes。

## 3. 测量口径

### 实验过程
```text
核验Release构建 → 生成输入与最小LEF/DEF（计时外）
→ 独立正确性检查（包含0、1、8条边界数据）
→ 每个路线/配置/规模预热1次
→ 正式串行运行5次，分别记录写入与读取阶段
→ 从日志提取计时 → 统计均值、中位数、min/max → 审计
```

输入不是通过另一条路线在计时内临时转换而来，避免将输入准备误算为被测读写成本。
以下括号表示分别计时的阶段，不重复相加：

```text
SQLite/EDADB批量写：
[打开/配置] → [建表] → [BEGIN] → [prepare/构造op → N条写入 → finalize/销毁op]
→ [COMMIT] → [关闭；内存库留到读后关闭]

SQLite/EDADB读：
准备缓存 → [文件库重开连接] → [prepare/reader → 逐行取字段并消费 → finalize/销毁reader]
→ [关闭]

原生iEDA：
计时外准备LEF/iDB → [DefWrite写DEF]
计时外独立准备读侧LEF → [DefRead读DEF并恢复iDB]

adapter：
计时外准备LEF/iDB → [initWriteDb，含建表] → [writeChip2Edadb，含内部事务] → [关闭]
计时外准备读侧LEF → [initReadDb] → [createDbByEdadb，恢复iDB] → [关闭]
```

直接SQLite只prepare一次并复用INSERT；读执行无参数的完整SELECT，不逐行prepare：

```sql
INSERT INTO component VALUES(?,?,?,?,?,?,?,?);
SELECT name,master_name,source,status,orient,x,y,record_order FROM component;
```

实际读写见[stream_benchmark.cpp:49](../benchmark/stream_benchmark.cpp#L49)。
直接读取两字符串复制到复用Record、消费整数和字符串长度，不累积结果vector；完整字段校验在独立check流程。

### 公共条件

- g++-10 Release -O3/-DNDEBUG、SQL trace关闭；40逻辑CPU、125 GiB RAM。
- 预热1次、正式5次；性能串行，样本前及读写间等待5秒。文件为OS-warm新连接，内存为同连接first/repeat。
- steady_clock墙钟差，单位ms；每阶段记录边界，不逐行细分。prepare至finalize计入完整数据阶段。
- init、create、BEGIN、write data、COMMIT、read data、close分别统计；LEF加载不计入。
- native的open/close在API内；adapter的create包含于init，内部提交包含于write，不能假装已经单拆。
- 正确性完整比较不混入正式计时；必要的错误检查、Record字符串复制和摘要消费仍有成本，不声称零扰动。
- 99组正确性检查、540条计时、108组统计审计通过，整批约78分钟；性能耗时与整批耗时不是同一个指标。

## 4. SQLite配置与运行机制

### 存储与journal选项

文件路径打开文件库；`:memory:`将整个数据库置于内存，连接关闭后消失；空文件名可建立临时库，可能写磁盘（本实验未使用）。
**`PRAGMA journal_mode=MEMORY`中的MEMORY修饰的是回滚日志（journal），不是数据库本体（DB）。DB的位置由打开数据库时的路径决定。**

| 打开数据库的方式 | journal_mode | DB本体在哪里 | journal在哪里 |
| --- | --- | --- | --- |
| `sqlite3_open("test.db", &db)` | MEMORY | 文件test.db；运行时仍有页缓存 | 内存 |
| `sqlite3_open(":memory:", &db)` | MEMORY | 内存，不创建DB文件 | 内存 |
| `sqlite3_open("test.db", &db)` | DELETE | 文件test.db；运行时仍有页缓存 | 回滚日志文件，提交时删除 |

**本实验A组是第二行：DB和journal都在内存。**原因分别是打开路径为`:memory:`和journal_mode为MEMORY；不能只由journal_mode推断DB位置。
[官方：In-Memory Databases](https://www.sqlite.org/inmemorydb.html)。

| journal_mode | 区别 |
| --- | --- |
| DELETE | 回滚日志，提交时删除；常规默认 |
| TRUNCATE | 回滚日志，提交时截为零长度 |
| PERSIST | 回滚日志，提交时清零头部、保留文件 |
| MEMORY | 回滚日志仅在内存；文件库崩溃恢复能力受损 |
| WAL | 写前日志替代回滚日志 |
| OFF | 禁用回滚日志，破坏正常回滚/原子提交保障 |

同一数据库的这些模式不是同时启用；内存数据库仅支持MEMORY/OFF。设置后应读回实际值。
[官方：journal_mode](https://www.sqlite.org/pragma.html#pragma_journal_mode)。

Rollback保存修改前页面，再写DB文件；WAL追加修改后页面与提交边界，之后checkpoint才写DB文件。
因此WAL成功提交不等于当场写DB文件，已同步的WAL可用于恢复。
[官方：WAL原理](https://www.sqlite.org/wal.html#how_wal_works)。本次没有测WAL，不能声称其更快。

### 事务与sync：不同层次

| 机制 | 控制什么 | 不等于什么 |
| --- | --- | --- |
| Transaction/COMMIT | 哪组修改一起提交，结束事务 | 不等于固定一次文件写或一次sync |
| Journal/WAL | 恢复所需旧页面/新页面 | 不是SQL文本备份，不等于同步 |
| Sync | 请求文件按恢复协议持久化 | 不决定一组有多少INSERT |

自动提交模式默认开启，BEGIN关闭，成功COMMIT/ROLLBACK恢复。
本实验不写BEGIN时，每条独立INSERT仍有隐式事务；B-batch将所有INSERT置于一组事务。
BEGIN默认DEFERRED推迟到访问时开始事务；IMMEDIATE立即尝试写事务；EXCLUSIVE在非WAL下还排斥其他读者，WAL下同IMMEDIATE。
[官方：Transaction](https://www.sqlite.org/lang_transaction.html)、[自动提交状态](https://www.sqlite.org/c3ref/get_autocommit.html)。

```text
批量：BEGIN → prepare → 多次bind/step/reset → finalize → COMMIT
逐条：prepare → 每条bind/step/reset及隐式事务结束 → finalize
```

step执行SQL时可能触发日志、页管理、文件写出。DELETE的简化恢复顺序是：
旧页写journal → 按需同步journal → 写DB文件 → 按需同步DB文件 → 删除journal完成提交。
缓存不足时可能在COMMIT前写出脏页；将外层COMMIT移出计时，不等于消除事务与I/O成本。
[官方：Atomic Commit及Cache Spill](https://www.sqlite.org/atomiccommit.html)。

SQLite Pager/WAL决定何时触发sync，后端经VFS xSync调用OS同步设施；粒度是文件同步请求，不是行。
xSync是同步调用，返回前等待底层结果；实际持久化依赖OS、文件系统与设备兑现承诺。
这里持久化指进入非易失存储，不只是交给OS缓存；也不必已经写DB文件（WAL可以承载持久化内容）。
[官方：VFS xSync](https://www.sqlite.org/c3ref/io_methods.html)、[底层假设](https://www.sqlite.org/atomiccommit.html#hardware_assumptions)。

| synchronous | 策略及区别 |
| --- | --- |
| OFF=0 | 不请求同步，仍可写文件；掉电有风险 |
| NORMAL=1 | 减少同步；WAL通常提交不sync、checkpoint节点同步，掉电可丢最近提交 |
| FULL=2 | 更强同步；WAL每次提交额外同步；rollback掉电持久性仍依赖文件系统 |
| EXTRA=3 | FULL基础上，DELETE提交后额外同步日志所在目录；WAL同FULL |

它不是简单定时频率，而是不同协议节点的同步策略。**一个事务可以没有、一次或多次sync，不能按事务数推算同步次数。**
[官方：synchronous与保障矩阵](https://www.sqlite.org/pragma.html#pragma_synchronous)。

### 实验实际配置与输出

下面是实际读回的PRAGMA，不是只列请求值。SQLite版本3.37.2。

| 参数 | A | A-no-journal | B-default | B-batch |
| --- | --- | --- | --- | --- |
| DB | :memory: | :memory: | 文件 | 文件 |
| journal_mode | memory | off | delete | delete |
| synchronous | 0 | 0 | 2 | 2 |
| cache_size | -8192 | -8192 | -2000 | -2000 |
| temp_store | 2 | 2 | 0 | 0 |
| foreign_keys | 1 | 1 | 0 | 0 |
| page_size（bytes） | 4096 | 4096 | 4096 | 4096 |
| 数据事务 | 整批 | 整批 | 逐条隐式 | 整批 |
| 规模 | 全部 | 全部 | 仅1,000条 | 全部 |

cache_size负值表示KiB预算；temp_store=2请求内存，0沿用构建默认。文件mmap_size读回0，内存未返回该项，不伪填0。
B-default/B-batch仅数据事务不同；A/B同时改变多项配置，不是纯磁盘差值。
adapter保持应用设置，FK=1，不冒充direct的默认配置。A是低文件I/O参考，不是已证明最快。

[print_config源码:34](../benchmark/stream_benchmark.cpp#L34)输出如下制表符分隔的B配置记录：

```text
CONFIG  journal_mode  delete
CONFIG  synchronous  2
CONFIG  cache_size   -2000
CONFIG  version      3.37.2
```

另有temp_store、mmap_size（如适用）、foreign_keys、page_size、encoding、source_id。
原始日志和audit.json中的configs保留实际值；其文件引用在本文中索引。

会议现场可直接展开[配置与统计审计audit.json](../../../../../../../../../../../tmp/iedadb_stream_full_20260910/audit.json)查看configs；
展开[输入大小与哈希datasets.json](../../../../../../../../../../../tmp/iedadb_stream_full_20260910/datasets.json)查看数据文件证据。
这些原始证据位于cherry13的仓库外/tmp，相对链接仅在原服务器目录布局下可用；复制本文不会携带原始证据。链接中的数字是原始目录标识，不是性能计时单位。

## 5. 结果：1,000,000条

数据来源：[每次计时samples.tsv](../../../../../../../../../../../tmp/iedadb_stream_full_20260910/samples.tsv)、
[均值/中位数/min/max summary.tsv](../../../../../../../../../../../tmp/iedadb_stream_full_20260910/summary.tsv)、
[完整统计report.md](../../../../../../../../../../../tmp/iedadb_stream_full_20260910/report.md)。正文摘取同一个完整批次，未拼接其他结果。

1,000,000条，5次中位数，单位ms；文件为warm，内存取first-read。
仅列绝对时间，不列speedup；写与写、读与读分别讨论。
**先读计时口径，再看写入表、读取表和结论。每项阶段结果只在对应表格出现一次；建表和提交不是另一轮实验。**

### 5.1 计时口径

| 指标 | 包含什么 | 不包含什么 |
| --- | --- | --- |
| create | SQLite直接：执行建表BEGIN、CREATE TABLE、建表COMMIT；EDADB直接：createTable调用全程，包括框架建表处理及其事务 | 连接init、业务记录INSERT、数据事务COMMIT |
| write data | prepare/op构造、全部记录绑定及INSERT、reset、finalize/op销毁 | direct的BEGIN和最后COMMIT；adapter内部提交未排除 |
| 数据事务COMMIT | 全部INSERT结束后，执行显式COMMIT直到返回的时间 | 之前INSERT阶段已发生的工作；不是全部磁盘写入或纯sync时间 |
| write complete | 每个样本的data BEGIN + write data + data COMMIT；write data包含prepare/op构造、字段绑定与全部INSERT、reset、finalize/op销毁 | init、create、close、输入生成、LEF加载、校验、预热及等待 |
| read data | prepare SELECT/reader构造、遍历全部结果、取字段到复用Record并消费、finalize/reader销毁 | 连接init/close、缓存准备、repeat预读、独立正确性校验；不重复建表 |

两条direct的实际边界见[stream_benchmark.cpp:188](../benchmark/stream_benchmark.cpp#L188)，
complete计算见[stream_benchmark.cpp:26](../benchmark/stream_benchmark.cpp#L26)。

**写入：以SQLite B-batch为例。**Tx表示`steady_clock::now()`取得的时间点，差值换算为ms；EDADB直接路线用op构造/销毁对应prepare/finalize。
源码对每个阶段独立记录起止点。下面按执行顺序纵向展示；括号给出该阶段的计算式。

```text
输入准备（计时外）
T0：开始init
    打开/配置连接
T1：结束init                         （init = T1 - T0）

配置输出及检查（计时外）

T2：开始create
    建表BEGIN → CREATE TABLE → 建表COMMIT
T3：结束create                       （create = T3 - T2）

──────── 数据写入的三个阶段 ────────
T4：开始数据事务
    执行BEGIN
T5：BEGIN返回                        （begin = T5 - T4）

T6：开始写数据
    prepare INSERT
    1,000,000条记录：逐条bind → step → reset
    finalize
T7：结束写数据                       （write data = T7 - T6）

T8：开始提交
    执行COMMIT，等待返回
T9：提交返回                         （commit = T9 - T8）
──────────────────────────────────

等待（计时外）

T10：开始close
     关闭文件库连接
T11：结束close                       （close = T11 - T10）

write data     = T7 - T6
write complete = (T5 - T4) + (T7 - T6) + (T9 - T8)
               不包含create/init/close
```
**write data与write complete为什么不同？**对于这里的显式批量事务：
- `write data`回答“执行全部INSERT要多久”：prepare至finalize完成时，外层事务还没有执行最后的COMMIT。
- `write complete`回答“开始事务、执行全部INSERT并完成提交，三个阶段合计多久”：在data上再加BEGIN和COMMIT。它不是另一次实验，也不是含建表的全流程时间。
- 所以比较数据操作成本用data；比较完成一批写入的成本用complete。两者都不是纯格式转换时间，提交成功的持久性保障还取决于配置，不能仅凭名称判断。

**读取：以SQLite B-batch的文件OS-warm、新连接读取为例。**Rx是另一组独立时间点，不与写入的Tx相减。

```text
准备文件缓存（计时外）
R0：开始read init
    打开/配置读连接
R1：结束read init                    （read init = R1 - R0）

R2：开始读数据
    prepare SELECT
    循环：step返回ROW → column取8字段 → 赋值/消费Record
    step返回DONE → finalize
R3：结束读数据                       （read data = R3 - R2）

独立check运行的完整校验（若启用，不计入read data）

R4：开始read close
    关闭连接
R5：结束read close                   （read close = R5 - R4）

read data = R3 - R2
当前程序的read complete = read data（没有额外的显式BEGIN/COMMIT阶段）
```

内存库复用写连接，不重新打开，最终在读取后关闭；repeat组的预读也在R2之前，不进入正式读取计时。
读取过程见[stream_benchmark.cpp:204](../benchmark/stream_benchmark.cpp#L204)。

**write complete与read data不是同一种阶段名称，不能直接混称为“纯读写时间”。**应区分下面两层口径：

| 比较层次 | 写侧 | 读侧 | 如何理解 |
| --- | --- | --- | --- |
| 数据操作阶段 | write data | read data | 都包含prepare至finalize及字段处理；写侧另列显式事务边界。不是无事务、无I/O的纯计算时间 |
| 数据操作完成合计 | write complete = begin + data + commit | read complete = read data | 本程序读取未设置额外显式事务边界，所以两者数值相同；不表示读写具有相同的持久化工作 |

以上合计均不是端到端进程时间，不含单列的init/create/close。读取表只保留read data，不再增加数值相同的read complete列。
跨路线仍按写对写、读对读比较；原生iEDA的open/close在API内，adapter内部事务在data内，这些边界差异仍需保留说明，不能仅靠改名宣称完全对等。

注意有两组不同的事务：建表事务已包含在create，数据事务才用于write complete。
**write complete已包含数据事务COMMIT，不能再加一次提交时间；create不在complete中。**

adapter例外：init混合打开/注册/建表，不能说全是create；write data是writeChip2Edadb全程，
包含转换、写库、内部各事务提交与临时Shadow清理，不是测试外面额外再执行一次COMMIT。
B-default也没有单独外层COMMIT，其逐条隐式提交已在data内，因此complete=data，不表示没有提交。

create不进入数据读写比较。计入direct提交后，SQLite/EDADB B-batch完整数据写入仍慢于native。
文本/DEF没有等价fsync保障，因此该比较也不是同掉电持久性对照。
分项中位数不直接相加冒充完整中位数；占比先逐样本计算再汇总。

### 5.2 写入阶段

按执行顺序列出阶段，单位ms；最后一列是合计，不是额外阶段。建表、数据提交单列，均不混入direct的write data。

| 实现 | init | create | BEGIN | write data | COMMIT | close | write complete |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| C++文本 | 0.109 | — | — | 748.833 | — | 0.021 | 748.833 |
| 原生iEDA | 包含于data | — | — | 702.185 | — | 包含于data | 702.185 |
| SQLite A | 0.124 | 0.160 | 0.002 | 1510.430 | 1.408 | — | 1511.874 |
| SQLite A-no-journal | 0.128 | 0.108 | 0.002 | 1515.544 | 1.406 | — | 1516.931 |
| SQLite B-batch | 0.118 | 78.141 | 0.010 | 1597.901 | 715.792 | 0.430 | 2316.542 |
| EDADB A | 0.129 | 0.188 | 0.002 | 1603.358 | 13.598 | — | 1616.961 |
| EDADB A-no-journal | 0.137 | 0.128 | 0.002 | 1603.744 | 13.485 | — | 1617.187 |
| EDADB B-batch | 0.134 | 74.585 | 0.011 | 1685.538 | 714.206 | 0.383 | 2391.578 |
| adapter | 1415.042 | 包含于init | 包含于data | 5813.067 | 包含于data | 0.288 | 5813.067 |

- complete=data+适用的BEGIN/COMMIT，不含init/create/close。B-default提交已在data中，不能填0冒充没有提交。
- adapter init包含打开、注册和建表，**没有独立测得create**；adapter data包含内部family事务。不能将它与direct排除提交的data作纯转换成本比较。
- 内存库写后保持同一连接，write close为—，最终关闭计在read close。native文件打开关闭位于API内，原始字段的0不是免费操作。
- B-batch完整数据写入中，COMMIT占比中位数为SQLite **31.01%**、EDADB **29.86%**；先按每个样本计算，再取中位数。
- 若以已测write阶段总和init+create+begin+data+commit+close为分母，SQLite B-batch：data约66.71%、COMMIT29.99%、create3.27%；adapter：混合init约19.80%、data约80.20%。这不是完整进程时间分布，计时外LEF/输入准备没有包含。

### 5.3 读取阶段

按执行顺序列出阶段，单位ms；min–max用于查看5次读取样本的波动。

| 实现 | read init | read data | read close | read data min–max |
| --- | ---: | ---: | ---: | ---: |
| C++文本 | 0.026 | 622.220 | 0.009 | 620.219–628.212 |
| 原生iEDA | 包含于data | 4843.321 | 包含于data | 4811.415–4849.885 |
| SQLite A | — | 825.183 | 5.757 | 819.951–831.917 |
| SQLite A-no-journal | — | 835.204 | 5.738 | 828.342–838.996 |
| SQLite B-batch | 0.138 | 844.419 | 0.256 | 842.235–858.069 |
| EDADB A | — | 1095.802 | 4.271 | 1095.417–1135.463 |
| EDADB A-no-journal | — | 1097.338 | 4.419 | 1095.735–1098.549 |
| EDADB B-batch | 0.176 | 1141.248 | 0.253 | 1127.029–1143.620 |
| adapter | 0.163 | 4730.975 | 0.185 | 4712.844–4781.578 |

内存配置取first-read；repeat数据在完整统计文件。读取表不重复计算write阶段的建表。
adapter read仅createDbByEdadb，不扫描参考DEF；恢复完整iDB。direct读取只覆盖同一个Record，不能把两者差值全算成封装成本。

### 5.4 如何解读

- **写入**：本例原生iEDA的write data最短；SQLite/EDADB即使排除建表和外层提交，也没有比原生写DEF更快。计入B-batch的数据提交后，差距进一步增大。
- **读取**：C++文本与direct路线快于原生iEDA，但只解析或消费简化Record，不恢复完整iDB，不能据此宣称完整DEF加载获得同等加速。
- **同条件API对照**：相同schema和配置下，EDADB直接读写均比手写SQLite慢；本批次只证明整条路径的净差异，额外操作验证见[SQLite与EDADB对照结果](../sqlite-vs-edadb/results.md)。
- **adapter**：读取中位数接近原生iEDA，写入较慢；其schema、对象恢复和事务工作量不同，不能将总差值全归因于adapter代码。
- **配置比较**：A与B-batch同时改变存储、日志、同步等参数，时间差表示整套配置的差异，不是独立磁盘I/O耗时；A-no-journal没有显示出稳定的整体优势。

## 6. 规模趋势

每格为write data / read data中位数，单位ms。direct取B-batch；adapter write含内部提交，其余direct write不含外层提交。

| 条数 | C++文本 | 原生iEDA | SQLite | EDADB直接 | adapter |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1,000 | 1.062 / 0.810 | 0.976 / 12.492 | 3.255 / 1.957 | 3.481 / 2.651 | 176.385 / 13.234 |
| 10,000 | 9.116 / 7.411 | 6.611 / 49.898 | 31.174 / 18.026 | 30.704 / 24.319 | 260.318 / 76.639 |
| 100,000 | 73.496 / 60.565 | 66.812 / 461.563 | 230.626 / 117.859 | 225.686 / 143.631 | 850.670 / 472.102 |


1,000,000条的对应数据统一见第5节写入/读取表；此处仅列较小规模，避免重复维护同一结果。

## 7. 运行证据与边界

原完整批次约78分钟，99组正确性、540条计时、108组各5次，退出码0、审计PASS。编译为g++-10 Release -O3/-DNDEBUG、trace关闭；测量版本以[manifest](../../../../../../../../../../../tmp/iedadb_stream_full_20260910/manifest.json)为准，源码归档提交不覆盖原始测量版本。

[原始样本](../../../../../../../../../../../tmp/iedadb_stream_full_20260910/samples.tsv)、[统计](../../../../../../../../../../../tmp/iedadb_stream_full_20260910/summary.tsv)、[完整报告](../../../../../../../../../../../tmp/iedadb_stream_full_20260910/report.md)、[正确性](../../../../../../../../../../../tmp/iedadb_stream_full_20260910/checks.json)、[审计](../../../../../../../../../../../tmp/iedadb_stream_full_20260910/audit.json)、[编译参数](../../../../../../../../../../../tmp/iedadb_stream_full_20260910/compile_flags.txt)、[链接参数](../../../../../../../../../../../tmp/iedadb_stream_full_20260910/link_command.txt)、[源码快照](../../../../../../../../../../../tmp/iedadb_stream_full_20260910/source)。/tmp不是永久备份；需要保留证据时另行归档，不将生成DB和二进制提交Git。

COMMIT占比及其口径见第5.2节，不代表纯sync时间。链接时存在LEF/DEF类型ODR警告，未修改该生产依赖，不宣称已解决。
