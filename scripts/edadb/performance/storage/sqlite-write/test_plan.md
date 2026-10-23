# 测试计划

状态：保留早期写专项及后续A/B补测方法，均已有结果，不是新增待运行任务。A/B指实验类别，内存/文件指配置。历史脚本的A/B-batch配置标识不重命名。代码与证据对应见[readme](readme.md)。

## 目标

分别量化SQLite与文本完整读写差距、多行INSERT净收益、三表写入和读取切换的净影响。不测试EDADB，也不以减少持久性保障冒充等价加速。以下开头为早期写专项，后两节定义单表完整读写及三表交织补测。

## 数据和矩阵

单表100万条8字段Component，沿用baseline生成器：name、master_name、source、status、orient、x、y、record_order。数据预生成，不计入时间。四路线为文本、单行INSERT、10行INSERT、100行INSERT；SQLite都使用TRANSIENT绑定和一笔事务。三表各10万条相同8字段，无PK/FK或额外索引，仍有rowid表树。

三表顺序：逐行t0/t1/t2轮换；连续10行再切表；每表完成后再切表。三组都用单行INSERT，三个statement各prepare一次。相同循环结构仅改变block长度。因而净差也可能含循环调度及输入访问局部性的变化，不将全部差值称为SQLite内部切换成本。

```sql
CREATE TABLE component(name TEXT,master_name TEXT,source INTEGER,status INTEGER,
                       orient INTEGER,x INTEGER,y INTEGER,record_order BIGINT);
INSERT INTO component VALUES(?,?,?,?,?,?,?,?);
INSERT INTO component VALUES(?,?,?,?,?,?,?,?),(?,?,?,?,?,?,?,?); -- 示意两行，实际10/100组
```

三表采用相同DDL，表名为t0、t1、t2；各自INSERT为8个参数。不执行多表JOIN，不反复prepare，不逐条提交，不使用STATIC。尾部不足整批时复用旧函数的单行尾部路径。

## 配置和时间

memory：:memory:、MEMORY日志、OFF同步、8MiB缓存、temp_store=MEMORY、mmap=0、FK ON（无声明FK）。disk：新文件、DELETE/FULL，其他参数实读输出，不擅自把默认缓存改为内存配置。两配置分别分析，不以它们的差值单独估算I/O。

单独记录open/config、建表事务、数据BEGIN、write data、COMMIT、close。write data包括SQL构造/prepare、全部绑定/step/reset、finalize；文本包括格式化/输出/flush。内存DB为便于验证保持连接，close标记-1，不虚构关闭时间；文件库在提交后计时关闭，再只读打开校验。文本flush不是fsync。没有全局清空OS缓存，不声称cold或掉电测试。

0/1/8/101条小规模检查，各组一次全量预热；正式5轮轮换顺序、串行，间隔0.5秒。所有样本在计时外逐字段校验，包括三张表和最终行数，不只检查摘要。

## 诊断与验收

正式时间不读取额外计数器；独立运行读取RUN、VM_STEP、REPREPARE及阶段缓存计数。以期望INSERT次数校验RUN。计数是工作量，不是时间。perf权限先验证，不更改全局sysctl；不可用时明确缺口，不编造热点。

主结果报告5轮中位数、范围和同轮配对差值。逐行/分组的行数与每表顺序相同；比较单表SQLite与文本时不混入三表数字。数据量为100万时，净差1ms等于1ns/条；三表30万条按总记录数换算。差值包含正常检查和调度，不是函数独占耗时。

## 完整读写补测

上一轮没有读取性能对照，读取只作计时外正确性校验。本轮单表文本/SQLite单行/十行/百行在同批重新测写和读，不从上一轮拼接时间。配置A对应memory，B-batch对应disk；使用冻结独立程序原入口，文件库默认参数必须实读确认DELETE/FULL。所有路线100万条同样8字段，4路线×2配置×5轮=40进程、80条读写阶段时间；0/1/8/101/100万条共40组全字段检查（全量检查兼预热），SQLite另跑6进程独立计数。正式进程间、写读之间均等待0.5秒。

| 阶段 | 文本 | SQLite |
| --- | --- | --- |
| 写data | 格式化8字段、顺序输出、flush | SQL构造/prepare、全部bind/step/reset、finalize |
| 写事务 | 无显式事务，不fsync | BEGIN、COMMIT单列，create另列 |
| 读init | 打开输入文件 | 文件库新连接；内存库复用连接，不虚构init |
| 读data | 解析8字段、赋值到复用Record、消费摘要 | prepare完整8列SELECT、step/取列、赋值到复用Record、相同摘要、finalize |
| 读close | 关闭输入流 | 关闭数据库连接；内存库也在此关闭 |

SQL为`SELECT name,master_name,source,status,orient,x,y,record_order FROM component`，固定新库的全表扫描。预热核验本次输入的顺序；不能由此宣称无ORDER BY对任意查询保证业务顺序。批量仅改写入SQL，读取代码完全相同，读取结果用于控制及对照，不预设批量写能让读取加速。

文本与文件SQLite均计时外预读文件，属于OS-warm；SQLite文件读取使用新连接，内存A是写后同连接首次读。配置差异明确保留，不称冷读，也不将内存SQLite与文本文件说成存储介质相同。正式读取不逐字段对照expected数组，只算相同消费摘要；完整验证及同长度内容损坏拒绝在独立check运行。不使用step-only代表完整读取。

新证据在results目录以rw-前缀保存，旧写入样本及perf不覆盖。本次未重跑读取perf，不把上一轮仅写入的采样当作本轮读热点。

## 三表／三文件交织补测

仅原生SQLite与C++文本。每表／文件100,000条、相同8字段，三份合计300,000条；source加表序号，避免互换表数据仍通过。SQLite为三张无显式PK/FK/索引的rowid表，文本为三个独立文件。配置沿用memory与disk，文本均为文件。

| 切换粒度 | 写／读顺序 |
| --- | --- |
| 1 | A一条、B一条、C一条，重复100,000轮 |
| 10 | A十条、B十条、C十条，重复10,000轮 |
| all | A全部、B全部、C全部 |

五个(write_block, read_block)组合为(1,all)、(10,all)、(all,all)、(all,1)、(all,10)。分析写切换只用前三组；分析读切换只用后三组。不同时改变两项再归因。这里10表示连续调用同一statement十次，**不是多行INSERT**；始终单线程、同一连接、一笔数据事务。

每张表prepare一次并保留statement。写入SQL为`INSERT INTO t0 VALUES(?,?,?,?,?,?,?,?)`（t1/t2同构，值按schema字段顺序绑定）；读取为`SELECT name,master_name,source,status,orient,x,y,record_order FROM t0 ORDER BY rowid`（另两表同构）。按调度推进三个游标，完整取出8字段并计算相同摘要；并非每次切换重新执行SELECT。

write data包含prepare、全部bind/step/reset、finalize；文本包含格式化、输出、flush。read data包含prepare、step/取列或文本解析、Record保存、摘要消费及SQLite finalize。连接、建表、BEGIN、COMMIT、close分别记录。文件计时外预读，内存沿用写连接；不测cold。文本不fsync，不与SQLite FULL宣称等价持久性。

20个配置组合，各做0/1/8/101/100,000条逐字段检查，最后一个兼预热；正式轮换5次，计时后额外完整逐字段验证，不把验证计入data。SQLite read close发生在验证读取之后。独立10组计数核验write RUN=300,000、read RUN=3、SORT/REPREPARE=0；不在正式循环读时钟或计数。报告中位数、范围及同轮差值；证据用interleave-前缀保留，不覆盖旧批次。
