# SQLite查询计划与执行计数机制

本文仅整理SQLite官方文档与源码。源码链接固定到 **3.37.2**；其他版本应查对应实现。

## 1. 执行计划与统计

| 工具 | 官方定义及范围 | 依据 |
| --- | --- | --- |
| EXPLAIN | 返回执行SQL所使用的虚拟机指令，而不是执行普通SQL并返回其结果 | [文档](https://www.sqlite.org/lang_explain.html) |
| EXPLAIN QUERY PLAN | 返回访问策略，包括SCAN、SEARCH、索引使用及临时排序结构；输出格式可能随版本改变 | [文档](https://www.sqlite.org/eqp.html) |
| sqlite3_stmt_status | 取得或重置prepared statement的执行计数 | [接口](https://www.sqlite.org/c3ref/stmt_status.html) |
| sqlite3_db_status | 取得数据库连接的状态与统计；部分参数支持重置，须检查返回码 | [接口](https://www.sqlite.org/c3ref/db_status.html) |
| sqlite3_stmt_scanstatus | 取得查询循环的预测与实测统计；需要SQLITE_ENABLE_STMT_SCANSTATUS编译选项 | [接口与条件](https://www.sqlite.org/c3ref/stmt_scanstatus.html) |

### 语句计数

- `VM_STEP`：执行的虚拟机操作数；超过2,147,483,647时该计数的返回值未定义。
- `RUN`：语句开始执行的次数；一次执行可以包含多次step。
- `FULLSCAN_STEP`：全表扫描时前进的步数。
- `SORT`：排序操作次数；`AUTOINDEX`：加入自动创建的临时索引的行数。
- `REPREPARE`：因schema或影响计划的绑定参数变化而重新生成语句的次数。

这些是操作计数，不是耗时。官方：[计数定义](https://www.sqlite.org/c3ref/c_stmtstatus_counter.html)。
3.37.2的普通计数读取位于[src/vdbeapi.c:1728](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbeapi.c#L1728)：读取`aCounter[op]`，按resetFlag清零；MEMUSED使用另一处理分支。
VM累加与归并见[src/vdbe.c:784](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbe.c#L784)及[src/vdbe.c:8434](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbe.c#L8434)。

### 连接缓存统计

- `CACHE_HIT / CACHE_MISS`：Pager缓存命中/未命中次数。
- `CACHE_WRITE`：写出的脏缓存页数量；WAL模式写入WAL文件，rollback模式写入DB文件，不包括回滚或恢复所写的页面。
- `CACHE_SPILL`：事务中因页缓存溢出而写出的脏页数量。

这些指标统计缓存访问或页数，不提供sync次数及耗时。官方：[参数定义](https://www.sqlite.org/c3ref/c_dbstatus_options.html)。
3.37.2通过连接关联的Pager取得这些计数，见[src/status.c:353](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/status.c#L353)。

## 2. 语句计时：SQLITE_TRACE_PROFILE

通过`sqlite3_trace_v2()`注册PROFILE事件，语句结束时回调提供近似执行时间，单位为纳秒。
旧`sqlite3_profile()`已弃用，官方建议使用trace_v2。[事件定义](https://www.sqlite.org/c3ref/c_trace.html)、[旧接口说明](https://www.sqlite.org/c3ref/profile.html)

### 3.37.2的计时边界

```text
sqlite3Step首次开始执行：记录startTime
    执行语句；SELECT可多次返回ROW
语句结束：读取结束时间，计算间隔，调用PROFILE回调
```

开始位置为[src/vdbeapi.c:693](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbeapi.c#L693)，结束及回调为[src/vdbeapi.c:61](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbeapi.c#L61)。
此实现记录起止时间差，不是累计各次step内部耗时；首次step前的prepare/bind不在区间内，返回ROW后至再次step之间的调用方时间也位于起止区间内。

### 纳秒单位不等于纳秒分辨率

3.37.2源码计算：

```c
iElapse = (iNow - p->startTime)*1000000;
```

Unix VFS的[src/os_unix.c:6747](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/os_unix.c#L6747)按毫秒生成时间值，包括将微秒部分除以1000。
因此这里把毫秒差换算为纳秒输出，并未获得纳秒精度。官方也明确区分profile的纳秒单位和毫秒分辨率：[说明](https://www.sqlite.org/c3ref/profile.html)。

## 3. 版本与编译条件

scanstatus只有在启用`SQLITE_ENABLE_STMT_SCANSTATUS`时才提供；3.37.2实现受对应条件编译保护，见[src/vdbeapi.c:2004](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbeapi.c#L2004)。
官方接口文档可能包含较新版本新增能力，应以目标版本的头文件、编译选项和源码为准。[官方编译选项](https://www.sqlite.org/compile.html#enable_stmt_scanstatus)

## 4. 优化器统计与执行计数不是同一种统计

| 信息 | 何时产生 | 用途 |
| --- | --- | --- |
| ANALYZE统计 | ANALYZE检查表/索引，写入sqlite_stat系列表 | 为查询规划提供数据分布信息 |
| EXPLAIN QUERY PLAN | 规划语句时生成 | 展示选择了哪种访问方式，不是实际运行计数 |
| statement/connection计数 | VM或Pager执行时累加 | 描述这次执行实际发生的工作 |

读取stmt_status不会自动执行ANALYZE，也不会自动替当前查询建立永久索引。ANALYZE会改变供后续规划使用的统计，不能将其当作只读的运行计数查询。[ANALYZE](https://www.sqlite.org/lang_analyze.html)、[查询计划](https://www.sqlite.org/eqp.html)

## 5. 计数何时增加、怎样取出

### VM语句计数：statement级

```text
prepare → 得到statement及内部计数存储
step执行VM → 对应事件发生时递增计数
    返回ROW → 应用取列 → 再次step（仍属同一次执行）
    返回DONE → 本次执行结束
sqlite3_stmt_status(statement, 指标, resetFlag) → 取得累计值
reset后复用 → 继续累加，除非明确清零
finalize → statement销毁，不能再取其计数
```

3.37.2中：
- 每执行一条VM指令，局部nVmStep增加；退出VM时累加进VM_STEP。[vdbe.c:784](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbe.c#L784)、[vdbe.c:8434](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbe.c#L8434)
- Init指令增加RUN，**不是每次返回ROW都增加RUN**。[vdbe.c:8226](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbe.c#L8226)
- 游标推进及排序等指令在相应位置增加计数，不是应用每处理一行手动加计数。[Next路径:5958](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbe.c#L5958)、[排序路径:5814](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbe.c#L5814)

普通指标的读取示意（statement仍有效）：

```c
int vm_steps = sqlite3_stmt_status(statement, SQLITE_STMTSTATUS_VM_STEP, 0);
int runs = sqlite3_stmt_status(statement, SQLITE_STMTSTATUS_RUN, 0);
int scans = sqlite3_stmt_status(statement, SQLITE_STMTSTATUS_FULLSCAN_STEP, 0);
```

resetFlag=0只读；非0返回旧累计值并清零对应计数，MEMUSED等非计数指标有例外。不同statement的计数不自动汇总；同一statement多次执行则会累计。[官方接口](https://www.sqlite.org/c3ref/stmt_status.html)

### Pager缓存计数：connection级

sqlite3_db_status取得一个连接关联Pager的统计，而不是某一个statement的独占统计。3.37.2的status.c枚举连接关联的数据库并调用sqlite3PagerCacheStat。[status.c:353](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/status.c#L353)、[pager.c:6789](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/pager.c#L6789)

```text
读取缓存计数before
执行目标操作
读取同一缓存计数after
增量 = after - before
```

前后不得夹入其他需要归属到不同阶段的SQL；否则增量会包含它们的活动。对CACHE_HIT/MISS/WRITE/SPILL取pCur，pHiwtr在这些指标上为0；其他指标可能采用不同含义，应查各自定义并检查API返回码。不能把所有db_status参数的清零语义一概而论。[官方连接参数](https://www.sqlite.org/c3ref/c_dbstatus_options.html)

## 6. 读取与写入怎样解读计数

### 读取查询

- RUN：一条SELECT返回许多ROW，通常仍是一次执行，不是返回行数次执行。
- FULLSCAN_STEP：扫描前进次数，不是返回行数；有过滤时可扫描很多行、只返回少量行。
- SORT/AUTOINDEX：排序次数/临时自动索引填入行数；不是排序行数/自动索引个数。AUTOINDEX也不是PRIMARY KEY创建的sqlite_autoindex。
- EXPLAIN QUERY PLAN的SCAN也可能按索引顺序遍历全部记录；SEARCH表示访问子集，不能仅凭名称判定哪个更快。[官方计划解释](https://www.sqlite.org/eqp.html)

### 写入语句

- 复用单行INSERT执行多次，每次开始执行增加RUN；VM_STEP累计所有执行的内部工作，不等于插入行数。
- CACHE_WRITE反映写页，CACHE_SPILL反映事务中缓存压力导致的脏页写出；INSERT期间与COMMIT期间的增量可能不同。
- 页可能多次写出；写页计数不是唯一页数，也不能乘页大小后直接当作全部I/O字节数，日志等工作需另外区分。它不提供sync耗时。[官方缓存计数](https://www.sqlite.org/c3ref/c_dbstatus_options.html)

**边界：这些计数解释工作量，不是耗时拆分。**VM_STEP多不代表各操作等价耗时；计数更少也不直接保证wall time更短。PROFILE计时的边界与精度见第2节，不能与执行计数混为一项。
