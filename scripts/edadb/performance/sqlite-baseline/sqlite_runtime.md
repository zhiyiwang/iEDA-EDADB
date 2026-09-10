# SQLite事务、日志与同步运行机制

更新：2026-09-10。官方文档与源码阅读说明；不代表benchmark已做故障注入测试。
实验参数统一见[sqlite_config.md](sqlite_config.md)。

## 0. 写入成功、提交成功与持久化不是同一个边界

**ACID中的Durability确实要求已提交事务在故障后仍可恢复。**但SQLite允许显式降低持久性保障，
不能把所有synchronous配置的COMMIT成功都解释为完整的掉电持久化承诺。
也不能笼统说“SQLite提交成功却不持久化”：正常持久化配置正是为此执行同步协议。

| 所谓“成功” | 实际保证 |
| --- | --- |
| 普通文件write成功 | 写入已被系统接受；不等于同步I/O完成，数据可能仍在易失缓存 |
| 显式事务内INSERT的step返回DONE | 该SQL执行完，不代表整个事务提交；仍可ROLLBACK |
| COMMIT成功，WAL + NORMAL | 已逻辑提交，但断电/OS崩溃可丢失最近事务；官方明确允许牺牲持久性 |
| COMMIT成功，WAL + FULL | 提交前同步WAL，在底层正确实现同步的前提下提供持久性 |
| COMMIT成功，DELETE + EXTRA | 同步日志/DB文件及必要目录更新，提供更完整的掉电持久性保障 |

文件write语义见[POSIX write](https://pubs.opengroup.org/onlinepubs/9799919799/functions/write.html)；
语句与事务边界见[SQLite Transaction](https://www.sqlite.org/lang_transaction.html)。
SQLite官方[synchronous说明](https://www.sqlite.org/pragma.html#pragma_synchronous)明确说明：
WAL/NORMAL事务可在断电或系统崩溃后回滚，FULL增加每次提交的WAL同步；
DELETE/FULL最后提交的持久性依赖文件系统，EXTRA加强目录保障。OFF则还可能导致数据库损坏。

**“尚未写DB文件”也不等于“未持久化”。**WAL/FULL提交时，新页面和提交边界已同步到WAL，
即使尚未checkpoint，重开后仍能通过DB文件+WAL读取已提交状态。
这正是日志支持持久性的方式，不与数据库原理矛盾。[WAL工作原理](https://www.sqlite.org/wal.html)

例如：WAL/NORMAL下INSERT后COMMIT成功，若同步前断电，最新事务可能丢失；
换成WAL/FULL则提交路径等待WAL同步成功。此例说明配置语义，不是本benchmark故障注入结果。
进程崩溃不等于OS崩溃/掉电；不要将不同故障下的保证混用。

## 1. 概念与粒度

本文统一用语：**写DB文件**指写数据库本体（通常为`.db`文件），不包含写journal/WAL；
**写文件**可能只到OS缓存；**持久化**指获得掉电后仍可恢复的保障。三者不混用。

| 机制 | 作用与实际实现 | 粒度及关键边界 |
| --- | --- | --- |
| Transaction/COMMIT | SQL虚拟机控制事务状态，B-tree/Pager通过锁及所选日志协议提交/撤销 | 一个事务可含多条SQL；**提交不等于一次文件写入或一次sync** |
| Rollback journal | Pager保存旧页面，再覆盖DB文件；失败用旧页面恢复 | 页面；**启用同步时须先保护旧页，再覆盖DB文件** |
| WAL | Pager把新页交给WAL写为frames，提交frame标记边界，checkpoint回填 | frame含页头与一页内容；**提交时无需写DB文件** |
| Sync | Pager/WAL经VFS请求文件同步，OS与设备兑现持久化 | 文件请求，可覆盖多页；**write成功不等于掉电后仍在** |

名词：**page（页）**是SQLite管理文件的基本块；**脏页**是已修改但尚未写出的缓存页；
**frame**是WAL中的页记录；**checkpoint**是将WAL已提交页面写DB文件；
**持久化**指经存储栈保障后断电仍可保留，不仅是写入OS缓存。
下面的流程来自第6节所列SQLite实现，不是建议的新算法。

**日志恢复不是独立备份。**误删后已经提交的数据不能靠journal自动找回；
独立备份见[Online Backup API](https://www.sqlite.org/backup.html)。
rollback journal与WAL是替代协议，不是两份相同日志。

### 是否必须同时写journal和WAL？

**使用哪种协议由应用设置的`PRAGMA journal_mode`决定，不是SQLite根据INSERT/SELECT自动切换。**
设置作用于指定数据库（下面的`main`），不是指定某一张表。

| 当前journal_mode | 使用的协议 |
| --- | --- |
| DELETE / TRUNCATE / PERSIST / MEMORY | Rollback journal；区别是日志存放及提交收尾方式 |
| WAL | Write-Ahead Logging；新页写WAL，checkpoint再写DB文件 |
| OFF | 关闭正常回滚日志保障；不是WAL |

新建普通文件数据库、应用未改设置时，常规默认是**DELETE（Rollback）**，不是WAL。
纯内存`:memory:`通常是MEMORY日志，只允许MEMORY/OFF。
已切为WAL的文件会保留该模式，重新打开仍是WAL，因此“新连接”不等于“恢复默认DELETE”。
框架也可能在打开后自行设置模式；应检查实际连接，而不是根据文件名或SQL类型判断。

```sql
-- 查询当前状态：不修改配置
PRAGMA main.journal_mode;
PRAGMA main.synchronous;

-- 选择Rollback模式：成功时返回delete
PRAGMA main.journal_mode = DELETE;

-- 或选择WAL模式：成功时返回wal（二选一，不是依次同时启用）
PRAGMA main.journal_mode = WAL;
```

切换时不要有活动事务；检查错误码及返回的实际模式。锁冲突或VFS不支持等情况可能导致失败或模式不变。
C++通过SQLite API执行这些PRAGMA并读取结果；不需要重新编译SQLite。

**journal_mode与synchronous独立**：切换WAL不代表自动选择NORMAL。
常规同步默认FULL(2)，但编译选项可改变默认值，应用也可覆盖；每个连接都应读回确认。
默认自动提交开启；显式BEGIN省略类型时为DEFERRED。实验完整参数见[sqlite_config.md](sqlite_config.md)。
依据：[启用WAL及持久性](https://www.sqlite.org/wal.html#activating_and_configuring_wal_mode)、
[journal_mode](https://www.sqlite.org/pragma.html#pragma_journal_mode)、
[synchronous](https://www.sqlite.org/pragma.html#pragma_synchronous)。

**不需要。同一个数据库的正常事务恢复选择其中一种协议。**
DELETE/TRUNCATE/PERSIST/MEMORY属于rollback模式，WAL是另一模式；OFF取消正常日志保障。
WAL模式不再为同一次DB文件事务维护一份主rollback journal；内部SAVEPOINT/语句回滚仍可能使用
sub-journal，不能据此判断两套主日志同时提交。见[SQLite临时文件说明](https://www.sqlite.org/tempfiles.html)。

SQLite不是简单“复制整个数据库→更新→换文件”：rollback模式更新原DB文件页面，必须保留恢复所需旧页；
WAL模式先追加新页版本，之后才回填。只更新并写出新页，不能处理多页写到一半的失败。
例如两页需一起改变，第一页面已覆盖、第二页未写时断电，rollback靠旧页撤销，
WAL靠有效提交边界判断哪些页版本可见。实现分支见pager.c的pagerUseWal判断及第3/4节。

## 2. API与事务边界

### 自动提交与显式事务是什么？

**autocommit是SQLite正式的连接状态，默认开启**，不是我们benchmark发明的模式。
**隐式事务**是SQLite因数据库访问自动开启的事务；相关活动语句结束时自动提交。
**显式事务**由BEGIN开启，通常延续至COMMIT/ROLLBACK；BEGIN关闭自动提交。
它不是“每次bind自动提交”，也不是后台定时线程。下列逐条提交示例限定无其它活动语句。
官方：[sqlite3_get_autocommit](https://www.sqlite.org/c3ref/get_autocommit.html)。

```text
批量写：BEGIN → prepare INSERT → N次(bind → step → reset) → finalize → COMMIT
自动提交：prepare → 每条独立INSERT的bind/step/reset及隐式提交 → finalize
读：prepare SELECT → 循环(step返回ROW → column取列) → DONE → finalize
```

prepare编译，bind赋参数，step执行并可能访问页面/取锁/I/O。
没有独立sqlite3_fetch函数，取值用column接口；reset保留绑定，finalize销毁语句。
显式事务内reset/finalize不是COMMIT。[官方API流程](https://www.sqlite.org/quickstart.html)

### BEGIN的三个可选模式

语法为`BEGIN [DEFERRED | IMMEDIATE | EXCLUSIVE]`，只选一个，省略为DEFERRED。
它们控制**何时取得事务/锁**，不是journal模式，也不决定sync级别。

| 模式 | 执行BEGIN时 | 后续区别 |
| --- | --- | --- |
| DEFERRED | 关闭自动提交，暂不取得实际读/写事务 | 首次SELECT进入读；首次写或读升级时竞争写权限，可能BUSY |
| IMMEDIATE | 立即尝试写事务，竞争失败可BUSY | 非WAL下此时仍允许读者，后续写DB文件/提交可能需等读者 |
| EXCLUSIVE | 立即尝试写事务；非WAL还排除其它读者 | WAL下与IMMEDIATE相同；不是提前同步数据 |

- 同库多个读者、一个写者；读快照升级写可能BUSY。
- BEGIN不能嵌套，局部嵌套用SAVEPOINT。COMMIT因BUSY失败可保留事务，需检查后重试。
- IOERR/FULL/NOMEM等错误可能撤销语句或整个事务，不能假设统一结果。
- sqlite3_get_autocommit检查开关，sqlite3_txn_state检查NONE/READ/WRITE；
  BEGIN DEFERRED后可为autocommit=0、txn_state=NONE。

事务规则依据：[Transaction](https://www.sqlite.org/lang_transaction.html)。

### step会触发日志和同步吗？

**会触发执行路径，但不是每次step都写日志或sync。**

| 当前执行 | step内部可能发生什么 |
| --- | --- |
| 显式事务中的INSERT/UPDATE | 修改页、准备日志；缓存压力可触发spill：rollback写DB文件前处理journal同步，WAL可先写未提交frames |
| 自动提交的独立INSERT | 同时执行写入及隐式提交路径，可能包含提交同步 |
| 显式COMMIT语句 | 也是经step执行（sqlite3_exec内部同样执行语句），完成剩余提交工作；WAL还可能触发自动checkpoint |
| SELECT | 正常读取不产生写日志；首次访问若发现待恢复的hot journal，恢复路径可能写文件 |

prepare/bind不是普通INSERT实际修改数据的阶段；reset/finalize保证语句结束，
不能据此声称所有隐式提交都固定发生在某个API中。
可逐项核对的证据（源码固定3.46.1，不代表运行库版本）：
- SQL执行入口：[sqlite3_step官方接口](https://www.sqlite.org/c3ref/step.html)说明step执行已准备的语句，包括COMMIT。
- INSERT期间提前写出：[Atomic Commit §6.3](https://www.sqlite.org/atomiccommit.html)说明提交前cache spill；
  [pagerStress及其注释，pager.c:4325-4408](https://github.com/sqlite/sqlite/blob/version-3.46.1/src/pager.c#L4325)说明页缓存压力可写文件并同步journal。
- 提交路径：[sqlite3PagerCommitPhaseOne，pager.c:6101](https://github.com/sqlite/sqlite/blob/version-3.46.1/src/pager.c#L6101)处理页及同步；
  [WAL提交判断，wal.c:3903-3934](https://github.com/sqlite/sqlite/blob/version-3.46.1/src/wal.c#L3903)检查提交标记与同步flags，然后调用sqlite3OsSync。
- SELECT恢复例外：[Atomic Commit §4](https://www.sqlite.org/atomiccommit.html)说明访问带hot journal的库时先恢复；不是每次SELECT都产生写入。

这些证据共同支持“step执行的内部路径可能同步”，不是说step入口无条件调用sync。

## 3. Rollback journal执行步骤

普通单文件DELETE模式、启用同步时的实际协议顺序（省略设备特定优化）：

```text
取得写事务所需锁
→ 保存需要保护的旧页面到journal
→ 修改SQLite页缓存
→ 覆盖DB文件前按策略同步journal
→ 取得排他写权限，写新页面到DB文件
→ 按策略同步DB文件
→ 删除journal，提交生效
→ 释放相应锁
```

崩溃后若存在满足恢复条件的hot journal，后续访问恢复旧页面、撤销未完成事务。
新增页、空闲页等有优化，不能理解为每条INSERT都复制一次旧页面。
缓存压力可能在COMMIT前写出脏页，日志同步/I/O也可能出现在INSERT的step中。
这不是固定系统调用次数；设备能力和优化会改变具体路径。
**时机标记**：前半段可在INSERT的step中完成；剩余步骤由提交路径完成。
写journal/DB文件是文件写入，只有相应同步保障后才能称为持久化。
依据：[Atomic Commit §3、§4、§6.3](https://www.sqlite.org/atomiccommit.html)。

## 4. WAL执行步骤与checkpoint

```text
修改缓存页 → 新页面追加为WAL frames
→ 记录提交边界 → 按策略同步WAL → 提交返回

checkpoint：按策略同步WAL → 可回填页面写DB文件
→ 按策略同步DB文件 → 条件允许时复用/截断WAL
```

读取根据快照从WAL或DB文件取页；无有效提交边界的尾部不作为已提交内容。
checkpoint回填的是已提交数据，不是再次提交事务；活跃读者可能限制进度。
默认自动checkpoint阈值通常为1000页，达到阈值的提交可能承担其成本；
也能显式执行，最后连接关闭通常还有清理（受配置影响）。
一次checkpoint可处理多个事务，不是每次提交都写DB文件。
**时机标记**：INSERT的step可能只改缓存，也可能spill到WAL；提交路径写提交标记并按策略同步。
checkpoint可以在触发阈值的提交调用内，也可以由显式checkpoint调用执行。
依据：[WAL](https://www.sqlite.org/wal.html)。

## 5. 参数与时机

### journal_mode：恢复协议

| 值 | 含义 |
| --- | --- |
| DELETE | 删除回滚日志完成提交；常规文件默认 |
| TRUNCATE | 提交时截断回滚日志为0字节 |
| PERSIST | 提交时清零日志头使其失效，保留文件 |
| MEMORY | 回滚日志在内存；文件DB仍在磁盘，应用崩溃可能损坏DB |
| WAL | 新页写WAL，checkpoint回填 |
| OFF | 无正常回滚日志保障，语句失败也可能导致损坏 |

### synchronous：同步策略，不是定时频率

| 值 | 同步时机 |
| --- | --- |
| OFF(0) | 不请求文件同步屏障；事务、写文件仍存在 |
| NORMAL(1) | 减少同步；WAL主要在checkpoint前同步WAL、完成后同步DB及复用时同步WAL头，多数提交不单独sync |
| FULL(2) | rollback加强日志/DB文件同步；WAL每次提交增加WAL同步 |
| EXTRA(3) | FULL基础上，DELETE删除日志后同步目录；WAL与FULL相同 |

NORMAL/WAL保持一致性但断电可能丢失最近提交；rollback/NORMAL保障更弱。
FULL/DELETE最后提交的持久性依赖文件系统，EXTRA加强目录持久化。
保障均依赖底层正确兑现同步请求。内存库仅支持MEMORY/OFF日志；
WAL可跨连接保留，synchronous按连接核验且不能在事务中改。
依据：[官方PRAGMA](https://www.sqlite.org/pragma.html)。

### WAL维护

wal_autocheckpoint=N以页数设阈值，0关闭自动checkpoint，不是关闭事务。
wal_checkpoint模式：PASSIVE尽量推进不等待；FULL等待以尽量完整回填；
RESTART进一步等待可复用WAL；TRUNCATE进一步截断WAL到0字节。
这些模式不是同名的synchronous=FULL。
依据：[wal_checkpoint](https://www.sqlite.org/pragma.html#pragma_wal_checkpoint)。

## 6. 官方实现入口

### 第一部分：谁决定并触发sync？

应用触发SQL执行或checkpoint；**具体是否需要同步由Pager/WAL的协议和配置判断**，
不是OS替SQLite决定事务提交点，也不是由一个独立定时sync线程统一触发。

| 触发事件 | SQLite内部决策者 | 同步对象/原因 | 官方证据 |
| --- | --- | --- | --- |
| INSERT造成缓存压力 | pagerStress → rollback同步路径 | 覆盖DB文件前保护journal；不保证每次spill都新增一次sync | [源码及注释](https://github.com/sqlite/sqlite/blob/version-3.46.1/src/pager.c#L4325)、[§6.3](https://www.sqlite.org/atomiccommit.html) |
| rollback写事务提交 | CommitPhaseOne及日志收尾路径 | 按配置同步journal/DB文件；EXTRA还保护目录变更 | [Pager提交](https://github.com/sqlite/sqlite/blob/version-3.46.1/src/pager.c#L6101)、[§3.7、§3.10](https://www.sqlite.org/atomiccommit.html) |
| WAL写事务提交 | walFrames的isCommit与同步flags判断 | FULL/EXTRA下请求WAL同步 | [wal.c:3903-3934](https://github.com/sqlite/sqlite/blob/version-3.46.1/src/wal.c#L3903) |
| 自动/显式checkpoint及WAL复用 | WAL checkpoint/重用路径 | 按配置同步WAL、DB文件或WAL头 | [walCheckpoint](https://github.com/sqlite/sqlite/blob/version-3.46.1/src/wal.c#L2065)、[同步策略](https://www.sqlite.org/pragma.html#pragma_synchronous) |

是否实际产生请求还受OFF、内存库、页状态、VFS设备能力影响；以上不是固定sync计数表。

### 第二部分：请求如何由后端执行？

```text
应用执行SQL（sqlite3_step / sqlite3_exec）
  → Pager或WAL判断同步边界及synchronous配置
  → sqlite3OsSync(file, flags)
  → 该文件VFS的sqlite3_io_methods.xSync
  → OS/文件系统/设备提供持久化保障
```

应用通常设置PRAGMA，不直接调用SQLite内部sqlite3OsSync。
**journal/WAL决定记录什么，sync决定何时要求这些文件写入持久化**：两者不是替代关系。
例如rollback要先持久化旧页保护再覆盖DB文件，WAL/FULL要持久化提交所在WAL数据再返回。
同步不是复制数据库，也不是逐个字段刷盘；一个文件请求可覆盖多页。
目录同步保护删除journal等目录项变更，由VFS删除/目录处理路径完成，不应硬说全部走文件xSync。
具体使用fsync、fdatasync或其它系统接口依VFS/平台，本文件不指定未核验的系统调用次数。

同步时机由第5节策略控制：OFF不请求；NORMAL/WAL主要在checkpoint/复用边界；
FULL/WAL增加每次提交同步；EXTRA/DELETE还同步目录。
缓存spill、硬件优化等会改变次数，所以不能假设一个事务对应一个sync。
VFS接口依据：[sqlite3_io_methods](https://www.sqlite.org/c3ref/io_methods.html)。
Unix后端可继续阅读[os_unix.c中的unixSync/full_fsync](https://github.com/sqlite/sqlite/blob/version-3.46.1/src/os_unix.c)，
区分SQLite已经提出同步请求与平台如何实现请求。此处只给已核对的文件/符号，不猜平台条件分支或系统调用次数。

固定引用SQLite **3.46.1**官方源码镜像，方便复查；不代表benchmark链接该版本。
实际运行版本记录sqlite_source_id()；源码按函数定位，不能套用其它版本行号。

| 入口 | 实现职责 |
| --- | --- |
| [vdbe.c：OP_AutoCommit](https://github.com/sqlite/sqlite/blob/version-3.46.1/src/vdbe.c#L3774) | BEGIN/COMMIT相关自动提交状态控制；下层还有B-tree/Pager处理 |
| [pager.c：sqlite3PagerCommitPhaseOne](https://github.com/sqlite/sqlite/blob/version-3.46.1/src/pager.c#L6101) | 按日志模式处理待提交页和同步，使用syncJournal等内部逻辑 |
| [pager.c：sqlite3PagerCommitPhaseTwo](https://github.com/sqlite/sqlite/blob/version-3.46.1/src/pager.c#L6328) | 日志失效与事务收尾，不是第二次SQL COMMIT |
| [wal.c：sqlite3WalFrames](https://github.com/sqlite/sqlite/blob/version-3.46.1/src/wal.c#L3992) | 包装frame写入，区分普通写帧与提交 |
| [wal.c：walCheckpoint](https://github.com/sqlite/sqlite/blob/version-3.46.1/src/wal.c#L2065) | 根据读者状态回填，处理同步和复用 |
| [sqlite3_io_methods::xSync](https://www.sqlite.org/c3ref/io_methods.html) | VFS文件同步，具体系统调用依平台而异 |

文件写入可能先进入OS缓存；xSync请求底层持久化，不是同步C++对象或CPU缓存。
同步对象可以是journal、WAL、DB文件或目录，不是一次行级磁盘写。

## 7. 对性能计时的约束

- 一个事务可涉及零次、一次或多次sync；COMMIT耗时不等于全部sync耗时。
- 将COMMIT移出数据计时，未排除step中的日志、页管理、spill或同步。
- CREATE TABLE也是写事务，但建表和插入独立报告；数据阶段内部不做逐API计时。
- WAL持久化对比必须单列checkpoint/close，不能隐藏延后工作。
- 自动提交与批量事务分别标记；内存库、MEMORY journal、同步OFF不能混淆。
- 计时不证明故障恢复正确：本轮无断电、OS崩溃、磁盘错误注入结果。
