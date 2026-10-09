# 无P2版本：SQLite开销分析

## 1. 版本、输入与方法

本轮从iEDA `27d1ceefd` / core `494ce79` 出发，仅增加观测代码。保留P1父键索引、P3/P4事务合并、P5流式Shadow；**P2已从源码移除，不是关闭开关**。milestone不移动。旧工作区源码和结果保存在服务器 `/tmp/edadb-p2-before-rollback/`，不混入以下数据。

| 输入 | DEF大小（B） | DB大小（B） | 表数 / 数据行数 |
| --- | ---: | ---: | ---: |
| Sky130 GCD filler | 726,155 | 2,936,832 | 41 / 34,358 |
| routed压力fixture | 6,305,169 | 46,981,120 | 41 / 449,214 |

压力fixture在已有routed输入中复制1,000个Net路由，检验查询放大，不代表新的物理有效设计。

- 隔离完整Release构建，GCC 10、O3/LTO，32路编译；回归8路并行。正式计时与构建/回归分离，固定CPU2串行执行。
- 沿用[Tcl入口](../../../../../scripts/edadb/performance/benchmark.tcl#L18)。LEF加载、写前DEF解析、读后验证在计时外；热OS缓存，每次新进程/连接。
- 计时器OFF/ON各预热1次、正式5次，交替顺序。OFF/ON仅表示我们自己的阶段计时器，不是SQLite参数或优化开关。
- 正式计时不加载探针；SQL计数、CPU采样、I/O诊断分别运行。实际文件库配置为DELETE/FULL、FK ON、cache_size=-2000、page_size=4096、mmap_size=0。
- SQLite为Ubuntu `3.37.2-2ubuntu0.8`，符号匹配实际库Build-ID，没有替换SQLite或关闭同步。

证据：[manifest](../../test/sqlite_cost/results/profile-only/manifest.json)、[生产源码差异](../../test/sqlite_cost/results/profile-only/production.patch)、[环境](../../test/sqlite_cost/results/profile-only/environment.json)、[配置](../../test/sqlite_cost/results/profile-only/sqlite-config.tsv)。生产源码、全部core头文件和测试脚本保存哈希，提交后按内容核对。

## 2. 读取：重复查询仍是重点

完整命令采用OFF组5次中位数；内部阶段采用ON组5次均值，**两者不能直接相减**。单位ms。

| 读取指标 | filler | 压力fixture |
| --- | ---: | ---: |
| 完整Tcl读取，OFF中位数 | 103.668 | 1347.806 |
| init：连接、表映射，ON均值 | 0.436 | 0.434 |
| read data：查询、Shadow恢复、iDB组装及清理，ON均值 | 105.617 | 1292.925 |
| 其中Net family | 80.080 | 1275.485 |
| Net占read data | 75.82% | 98.65% |

read data不含init；Net是子项，不再与data相加。[原始时间](../../test/sqlite_cost/results/profile-only/samples.tsv)、[统计](../../test/sqlite_cost/results/profile-only/summary.tsv)。

| 查询工作量 | filler | 压力fixture |
| --- | ---: | ---: |
| 全部SELECT执行次数 | 30,548 | 448,548 |
| Net后代SELECT次数，不含Net根查询 | 29,699 | 447,699 |
| SELECT VM指令数 | 917,870 | 11,707,534 |

Point、ViaRef及其他Net子表均按父键`SEARCH … USING INDEX`。根表全读仍有SCAN，Instance、Net、Pin、Row、Slot各有一次排序；不同于“每个父对象扫描整个子表”。[实际SQL](../../test/sqlite_cost/results/profile-only/sql-statements.tsv)、[查询计划/VM](../../test/sqlite_cost/results/profile-only/query-plans.json)。

压力Net读取只prepare 8次，却执行447,700次查询（含1次根查询），447,700次reset/clear及1,331,358次父键bind。**不是重复prepare，而是反复绑定、索引查询、取值和收尾。** [API计数](../../test/sqlite_cost/results/profile-only/api-counts.tsv)。

独立perf中，read data用户态CPU样本共301 / 3,546个：

| 调用链 | filler | 压力fixture |
| --- | ---: | ---: |
| step及内部VM/B-tree执行 | 50.17% | 64.24% |
| column取列 | 10.63% | 7.47% |
| bind | 2.33% | 2.48% |
| reset/clear | 2.66% | 3.67% |

其余包含框架、分配、对象恢复及未归类路径；内联影响归类，不能认为可见fromShadow样本少就意味着重建免费。[CPU分类](../../test/sqlite_cost/results/profile-only/cpu-categories.tsv)、[热点符号](../../test/sqlite_cost/results/profile-only/cpu-symbols.tsv)。CPU比例不是墙钟比例，不换算为独占毫秒。

**读取takeaway：** P1已解决关键子表无索引；当前证据支持下一步对照验证P2减少重复查询。尚未测得可省掉的毫秒数，不能把全部Net时间归为N+1。应保持完整对象和顺序一致，再比较查询次数、read data及RSS。

## 3. 写入：执行与提交等待分别分析

完整命令采用OFF中位数；内部阶段采用ON均值，单位ms。create与write data分开；schema COMMIT属于create，数据COMMIT属于write data。

| 写入指标 | filler | 压力fixture |
| --- | ---: | ---: |
| 完整Tcl写入，OFF中位数 | 391.170 | 2549.340 |
| init：连接、主键映射 | 0.176 | 0.188 |
| create：建表、索引及schema事务 | 88.746 | 88.029 |
| 其中建表/索引执行 | 4.966 | 5.616 |
| 其中schema COMMIT | 83.776 | 82.408 |
| write data：转换、插入、清理及数据事务 | 309.131 | 2485.087 |
| 其中数据BEGIN | 0.010 | 0.008 |
| 其中数据COMMIT | 144.180 | 688.638 |
| 同样本data减BEGIN/COMMIT | 164.940 | 1796.441 |
| 其中Net family（转换＋插入＋清理） | 150.384 | 1784.789 |

数据COMMIT占write data的46.64% / 27.71%。data减事务调用仍含页写出、日志及可能的同步，不是纯转换时间。

| 工作量 | filler | 压力fixture |
| --- | ---: | ---: |
| INSERT执行次数 | 34,358 | 449,214 |
| INSERT VM指令数 | 1,255,177 | 16,865,137 |
| 数据BEGIN / COMMIT次数 | 1 / 1 | 1 / 1 |
| 数据COMMIT前缓存溢出写页次数 | 250 | 11,039 |
| 数据COMMIT中写页次数 | 436 | 439 |

另有一次schema事务，41条CREATE TABLE和13条显式CREATE INDEX。页写出计数不是物理磁盘IO次数。[事务SQL](../../test/sqlite_cost/results/profile-only/exec-sql.tsv)、[缓存计数](../../test/sqlite_cost/results/profile-only/cache-stages.tsv)。

独立perf中，write data用户态CPU样本共351 / 5,252个：step占72.93% / 81.09%，bind占4.84% / 3.87%，reset/clear占3.99% / 3.43%。热点包括VM、B-tree定位/插入、记录比较；不能把step全称为调用开销或FK检查，也不能把剩余时间全归给Shadow。

压力用例独立I/O诊断：

| 位置 | fdatasync对象与次数 | 调用累计ms |
| --- | --- | ---: |
| schema COMMIT | journal×2、目录×1、DB文件×1 | 107.232 |
| Net插入中、数据COMMIT之前 | journal×2、目录×1 | 63.317 |
| 数据COMMIT | journal×2、DB文件×1 | 698.573 |

另一份边界CPU诊断中，压力COMMIT墙钟884.840 ms，用户态＋系统CPU仅8.389 ms，结合同步调用说明等待不可忽略。它们是**独立诊断运行**，不能替换正式COMMIT的688.638 ms或与其相减。[I/O](../../test/sqlite_cost/results/profile-only/io-stages.tsv)、[CPU/墙钟](../../test/sqlite_cost/results/profile-only/diagnostic-cpu-time.tsv)。

**写入takeaway：** P3/P4已合并事务；剩余代价包括逐行SQL执行和提交等待。下一步分别验证缓存容量、同事务多行INSERT，保持FULL/FK及对象语义，比较data＋COMMIT整体与内存。本轮未实施这些优化。

## 4. 代码与计时位置

| 位置 | 含义 |
| --- | --- |
| [Session](../../idb/edadb_stage_timing.h#L44) / [DetailTimer](../../idb/edadb_stage_timing.h#L124) | 进入作用域读时钟，析构累计，命令结束统一输出；行循环不读时钟 |
| [schema观测](../../idb/edadb_idb_init.cpp#L135) | 分开记录schema BEGIN、建表/索引、COMMIT |
| [逐父读取](../../core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h#L565) | core未修改；每个父对象绑定键并执行子查询 |
| [取列/NULL](../../core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h#L76) | 保留原有字段及NULL语义 |
| [插入收尾](../../core/include/edadb/backend/sqlite/DbTableOpInsert4Sqlite.h#L215) / [resetForReuse](../../core/include/edadb/backend/sqlite/DbStatement4Sqlite.h#L144) | 保留每行step、clear/reset及父子写入顺序 |

匹配SQLite源码目录见environment：`src/vdbeapi.c` → `sqlite3VdbeExec` → `src/btree.c`；写出与同步在`src/pager.c`、`src/os_unix.c`。实际采样到的符号见cpu-symbols.tsv，不以源码存在某函数就断言其耗时。

## 5. 有效性与下一步

- 新构建15/15回归通过；插入失败、转换失败均数据整体回滚，保留schema，同进程重试成功。
- 40次严格DEF/数据库往返检查通过；正式40条命令时间、114组各5次；逻辑dump与冻结流式Shadow结果一致。
- 12份perf采样无记录到的丢样。计数器精确次数、计时器及脚本防覆盖测试通过。[总审计](../../test/sqlite_cost/results/profile-only/audit.json)。
- OFF→ON完整命令中位数：filler读103.668→108.471 ms、写391.170→388.788 ms；压力读1347.806→1296.556 ms、写2549.340→2567.949 ms。差异含波动，负值不是观测加速，也不能据此证明所有观测成本的严格上界。
- 尚未做完全无新增观测源码的同批构建对照，未覆盖cold及更多真实大设计。第三方LEF/DEF既有LTO类型警告仍存在，本轮未修改其实现。

**下一步建议先讨论P2对照**：当前有大量使用索引的重复child查询，不同于缺索引或写入FK约束检查。尚不能宣布P2能节省多少时间。本次提交不包含P2，不更改milestone。

阅读顺序：[takeaway](sqlite-cost-takeaway.md) → 本报告 → [实现与复现命令](../../test/sqlite_cost/readme.md)。
