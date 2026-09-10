# SQLite实验参数

更新：2026-09-10。参数说明，非自动加载文件；新入口stream_benchmark.cpp按配置名执行，运行状态见[实验记录](experiment_report.md)。
运行机制见[sqlite_runtime.md](sqlite_runtime.md)，数据与计时见[TEST_PLAN](test_plan.md)。

## 候选配置与默认参考

**A是保留回滚能力的低开销候选，不是已证明的性能最优配置。**
B-default与B-batch仅改变数据事务边界；所有PRAGMA、SQLite构建及输入保持相同。

| 参数 | A：低开销候选 | B-default：默认参考 | B-batch：批量对照 |
| --- | --- | --- | --- |
| 打开目标 | `:memory:` | 新建独立文件 | 新建独立文件 |
| journal_mode | MEMORY | 实际默认，通常DELETE | 与B-default相同 |
| synchronous | OFF(0) | 实际默认，通常FULL(2) | 与B-default相同 |
| temp_store | MEMORY，核验构建是否允许 | 保留构建默认 | 与B-default相同 |
| 数据事务 | 一个显式BEGIN/COMMIT | 不显式BEGIN，独立INSERT隐式提交 | 一个显式BEGIN/COMMIT |
| cache_size | -8192（约8192 KiB预算） | 通常-2000，核验构建默认 | 与B-default相同 |
| mmap_size | 请求0，记录是否适用 | 构建/平台默认，读回 | 与B-default相同 |
| foreign_keys | ON | 通常OFF，可由编译选项改变 | 与B-default相同 |
| page_size | 记录实际值 | 通常4096字节，核验实际值 | 与B-default相同 |

另加**A-no-journal**：与A全部相同，仅journal_mode由MEMORY改为OFF。
A保留回滚作为主内存基线；A-no-journal只测成功路径的低开销，不提供同等恢复保障，未证明更快。
发生错误立即废弃并重建数据库，不能依赖ROLLBACK；计时外仍检查成功样本全部字段。
依据：[OFF模式限制](https://www.sqlite.org/pragma.html#pragma_journal_mode)。

默认受构建、VFS及库文件影响。B-default若偏离常规值，保留并报告；若强设DELETE/FULL，
应称“固定参考”，不是实测默认。依据：[PRAGMA](https://www.sqlite.org/pragma.html)、
[编译选项](https://www.sqlite.org/compile.html)。

两个参数不是同一维度：journal_mode选择恢复机制（DELETE为rollback journal提交时删除日志）；
synchronous选择该机制下的文件同步保障（FULL为同步级别2），不选择日志类型或事务大小。
“实际默认”是不写设置PRAGMA、读回当前库的值；“固定参考”是主动强设指定值，两者可能相同但不能预设。
B-default/B-batch名称只区分数据事务边界，具体计时见[test_plan.md](test_plan.md)。

## 为什么选择这些配置

- **A：避免数据库文件I/O的低开销参考。**以sqlite3_open(":memory:", &db)打开，
  SQLite不创建DB文件；MEMORY保留内存回滚日志，OFF不请求文件同步，批量事务减少重复提交。
  temp_store=MEMORY尽量避免中间结果临时文件；仍需核验SQLITE_TEMP_STORE编译设置。
  SQL执行、B-tree、分配、拷贝与事务管理仍存在，不能称为“零开销”或未经实测的最快配置。
- **B-default：默认使用成本。**新文件打开后不执行配置写入PRAGMA，也不显式BEGIN；
  只查询实际配置。它回答“不调参直接使用SQLite”的成本，不代表SQLite的最佳批量性能。
- **B-batch：隔离事务边界影响。**保持B-default所有实际参数，仅在全部数据INSERT外加BEGIN/COMMIT。
  差值反映批量事务收益，包含锁、日志及同步等变化，不能全部归因某一次sync。
- **A-no-journal：隔离回滚日志影响。**对比A看取消内存日志是否减少写入开销；不称为同保障优化。

A不持久化，关闭连接数据消失，不能替代文件存储。操作系统仍可能swap，测试应监控内存压力；
“不写DB文件”不是保证整台机器没有磁盘I/O。官方：[内存库](https://www.sqlite.org/inmemorydb.html)、
[临时文件](https://www.sqlite.org/tempfiles.html)、[自动提交](https://www.sqlite.org/c3ref/get_autocommit.html)。

## A的设置顺序与比较指标

打开`:memory:`后、建表/事务前执行：

```sql
PRAGMA main.journal_mode=MEMORY;
PRAGMA main.synchronous=OFF;
PRAGMA temp_store=MEMORY;
PRAGMA main.cache_size=-8192;
PRAGMA main.mmap_size=0;
PRAGMA foreign_keys=ON;
```

B-default只打开全新文件并查询配置，不调用现有config_sql，否则已经改变默认值。
B-batch同样不调用config_sql，读回配置必须与B-default逐项一致。
此处指SQLite-direct。EDADB connect会主动开启foreign_keys；新测试入口从同一SQLite库的
独立默认连接读取foreign_keys默认值，再在EDADB测试连接恢复该值（计入init），不修改EDADB生产代码。
因此两条direct的B配置仍相同；不把EDADB主动设置的ON误称SQLite构建默认。
两组使用独立新文件、相同schema创建流程、prepare一次及相同bind/step/reset循环；
仅数据INSERT外是否包BEGIN/COMMIT不同。所有batch组必须提交成功，建表另计。

- 数据阶段表比较write_data与read_data；init/create/close单列。不在逐行循环中做细粒度profiling。
- 另列完整数据写入成本：batch为BEGIN+write_data+COMMIT；default的隐式提交已经包含在write_data中。
  **不能直接把default含提交的时间与batch不含提交的时间称为同边界性能。**
- A与B-batch比较内存/文件整套配置效果，不称为纯I/O差值；A不能用于证明持久化性能胜过DEF。
- B事务收益必须用完整数据写入成本比较；报告比率B-default/B-batch及差值，不能用含提交对不含提交。
- B的事务边界差异只用于写入；读均使用相同完整SELECT流程，不额外给B-batch的读加BEGIN。
- 文件读区分OS-cold/OS-warm；内存读只分first-read/repeat，没有OS文件缓存cold组。

## 核验与实验边界

### 与未优化milestone的事务对应

已核对deliverable checkpoint的iEDA 93f3760（core gitlink 30771329），以及当前prof-test 7b661baa2/core 90a5fb249：
- 建表：createTable默认self_txn=true，每次root schema调用显式BEGIN/COMMIT。
- 写入：insertObject/insertVector默认self_txn=true，由EDADB内部显式BEGIN/COMMIT；
  一个root family的vector及其children共用该次事务，不是每条INSERT自动提交，也不是整个design一个事务。
- 读取：没有包住全部读取的显式BEGIN/COMMIT，使用SQLite隐式读事务；不把每个readNext视为一次提交，游标可持续有效。
- 因而B-batch才是单一COMPONENT family的事务粒度对照；B-default仅用于说明自动提交代价，不代表milestone写入。
  完整adapter仍按原family边界执行，不能擅自合并为整个design事务。
- 事务相同不保证所有PRAGMA相同；adapter实际同步/日志等必须记录后再声称配置一致。

源码：[def_write_edadb.cpp:366](../../../../src/database/manager/builder/def_builder/def_write_edadb.cpp#L366)调用insertVector；
core对应[runMaybeTransaction:119](../../../../src/database/edadb/core/include/edadb.h#L119)、
[createTable:319](../../../../src/database/edadb/core/include/edadb.h#L319)、
[insertVector:397](../../../../src/database/edadb/core/include/edadb.h#L397)。

- 在实际连接、事务开始前配置，检查返回码及实际值；内存库关闭即消失，读写需同连接。
- 保存sqlite_version()、sqlite_source_id()、PRAGMA compile_options和表中参数的实际值。
- 参数实现位于stream_benchmark.cpp；adapter保留原设置，单独记录，不冒充direct的A/B配置。
- A/B由stream_benchmark.cpp实现；实测完成状态和结果以实验记录为准，差值不能直接叫纯磁盘I/O时间。
- journal_mode=OFF仅限A-no-journal独立实验，不作为恢复能力基线；不存在关闭全部SQLite机制的参数。
- init/create、数据阶段、BEGIN/COMMIT、close分别报告；数据阶段比较不等于持久化性能比较。
