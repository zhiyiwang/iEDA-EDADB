# iEDA+EDADB历史实验记录

本文保留历史命令口径及内部版本编号供审计，不作为当前阶段结果。当前对外结果见[优化实验结果](optimization-results.md)，基线为未优化iEDA+EDADB。

## 1. 先明确比较对象

- **原生iEDA**：`def_init`读取DEF，`def_save`写出DEF，是应用性能参照。
- **B0：未优化iEDA + EDADB**：加入EDADB存取，但尚未加入P系列优化。优化收益以该未优化版本为基线；原生iEDA仅作额外参照。
- 实际累积顺序：**B0 → P1 → P3 → P4 → P5 → P2**，不是按编号顺序执行。
- 本文汇总已有实测数据，本次仅重新测了P1，未重新运行全部历史版本。历史不同批次的数值变化不能全部归因于代码修改，尤其是写入提交和cold读取的波动。

### 版本对应

| 累计状态 | 新增修改 | iEDA commit | EDADB core gitlink |
| --- | --- | --- | --- |
| B0 | 尚无P优化，profiling关闭 | `62504bd9d` | `be6bbdd` |
| +P1 | 子表完整parent-FK索引 | `4ba761281` | `1c4857c` |
| +P3 | 合并建表事务 | `6990b7072` | `c2c92da` |
| +P4 | 合并design写事务 | `8d8550629` | `6d3718f` |
| +P5 | 逐root构造、写入、释放Shadow | `29808d61a` | `7066b01` |
| +P2 | Net叶子vector按Wire批量读取 | `9c376467b` | `ad0f820` |

当前开发分支为`edadb-performance-optimization-dev`，从P1配对提交创建，已有未提交的粗粒度计时代码；未引入后续P优化。下述结果仍属于历史版本，不是新增计时代码的结果。

### 每个相邻版本的代码差异

以下按上表相邻提交执行`git diff`核实，不把此前优化重复算作本阶段修改。链接打开当前工作区文件；后续版本的实现应按对应commit查看，不能用当前P1文件代替。

**B0：原始存取实现＋可关闭的profiling。** 本次测量关闭profiling，仍是逐父对象查询子表、每棵schema tree单独建表事务、每个root family单独写事务、完整root Shadow集合写入。它不是不含EDADB的原生路线。

**P1，相对B0：只修改core建表路径，不修改adapter生产代码。**
- [SqlStatement4Sqlite.h](../../core/include/edadb/backend/sqlite/SqlStatement4Sqlite.h)：新增`createParentForeignKeyIndexStatement()`。实际判断是parent FK有效且`hasPrimKey == false`；收集完整父FK链，生成非UNIQUE索引`<table>__edadb_parent_fk_idx`。不是动态审查所有已有索引。
- [DbTableOpCreate4Sqlite.h](../../core/include/edadb/backend/sqlite/DbTableOpCreate4Sqlite.h)：`createOneTable()`在CREATE TABLE成功后执行上述CREATE INDEX，失败返回错误；仍在原有建表遍历和事务内。
- core `test/DbChildForeignKeyIndex.cpp`：新增子FK索引测试。父仓库更新core gitlink；未合并事务、未改查询次数。

**P3，相对P1：合并建表事务。**
- [edadb_idb_init.cpp](../../idb/edadb_idb_init.cpp)：`initTable()`、`initAllTables()`增加并传递`self_txn`；`initWriteDb()`执行`BEGIN → initAllTables(true, false) → COMMIT`，建表或提交失败则ROLLBACK。
- core仅修改`test/DbFacadeTransactions.cpp`：验证外部schema事务提交及失败回滚；没有新增core生产API。

**P4，相对P3：合并数据事务，并传播失败。**
- [def_write_edadb.cpp](../../../manager/builder/def_builder/def_write_edadb.cpp)：`writeDb2Edadb()`在schema初始化之后开始design事务；保存各writer返回值，失败整体回滚，成功统一提交。
- 所有root writer的`insertObject()`/`insertVector()`传入`false`，关闭各自管理的事务；不是把schema和design合为一个事务。
- core仅扩展`test/DbFacadeTransactions.cpp`：两个root表一起提交；第二表重复键失败后，两表数据一起回滚。

**P5，相对P4：四类root Shadow流式写入。**
- 同一`def_write_edadb.cpp`中，`writeIdbInstance()`、`writeIdbPin()`、`writeIdbSpecialNet()`、`writeIdbNet()`从“new所有Shadow→vector→insertVector→统一delete”改为“创建可复用`makeInsertOp()`→逐root栈上Shadow→toShadow→insert→销毁”。保留对象顺序及P4外层事务，转换/插入失败返回错误。
- `edadb_idb_init.cpp`与core `SqlStatement4Sqlite.h`只补充前述事务/索引设计注释；本阶段core gitlink变化不代表新增core执行逻辑。
- adapter各root说明文档同步更新；新增压力fixture与RSS验证方法，不改持久化字段定义。

**P2，相对P5：显式启用叶子vector批读。**
- [def_read_edadb.cpp](../../../manager/builder/def_builder/def_read_edadb.cpp)：仅`readIdbNet()`启用`enableLeafBatchRead()`，其他root保持原路径。
- core `DbTableOperator.h`：增加并向child传播批读策略，以及每个root行开始时的缓存清理入口；`DbTableOpQueryGeneric4Sqlite.h`接入root行边界。
- core `SqlStatement4Sqlite.h`：新增`queryForeignKeyPrefixStatement()`，按祖先FK前缀查询，并返回直接父键用于分组；`DbForeignKeyBinder.h`新增`bindFirst()`绑定该前缀。
- core `DbTableOpSelect4Sqlite.h`：新增leaf batch cache、`readByForeignKeyPrefixBatch()`等；按Wire范围加载Point/ViaRef/VirtualPoint，按Segment键和vector序号恢复，失败清理暂存对象，切换root时清缓存。不是整库一次加载。
- core `DbProfiler.h`增加批加载/缓存命中计数；`test/DbTableOpSelect.cpp`与`test/DbProfiler.cpp`覆盖批读、跨root缓存、指针生命周期和计数。新增计数仅用于profiling，绝对性能表仍用OFF。

复核示例：父仓库执行`git diff 4ba761281 6990b7072 -- src/database`；core执行`git -C src/database/edadb/core diff 1c4857c c2c92da`。其余阶段替换为上表相邻commit即可。

## 2. 数据与计时方法

- 主数据：[Sky130 GCD iPL_filler_result.def](../../../../../scripts/design/sky130_gcd/result/iPL_filler_result.def)，本轮输入726,155 B，SHA256为`b2a8ade6f7e74f620491f56914a34d8fb89dd3efe957e4dc3ec4a42d50104c5e`。历史报告使用同名输入，本轮哈希不追溯证明历史文件内容。
- 历史P2/P5另用routed压力fixture：从Sky130 GCD `iRT_result.def`复制一条真实路由Net的routing body，生成1,000条额外Net；总计1,677个Net、约146,997个Segment。它不是filler，也不是物理设计质量验证用例。
- Release `-O3`，正式绝对时间使用profiling OFF；每组预热1次、正式5次、串行执行，样本间等待5秒。下表均为**中位数，单位ms**。
- 主表使用warm；cold仅通过文件级缓存回收建议准备，不能理解为所有硬件缓存均冷。cold原始数据也保留。
- **读**：原生`def_init`，对照完整`edadb_read`；LEF解析在外，EDADB参考DEF扫描、对象恢复在内。
- **写**：原生`def_save`，对照完整`edadb_write`；EDADB包含建表、索引、数据写入和提交，输入DEF加载在外。不是单独的INSERT循环时间，也不是同持久性保障的纯序列化比较。
- 正确性：读取后生成的DEF与原生DEF严格diff；验证不计入EDADB读时间。历史阶段的验收按各阶段报告记录，本轮P1独立通过12次diff。
- 入口：[run.sh](../../../../../scripts/edadb/performance/run.sh)、[benchmark.tcl](../../../../../scripts/edadb/performance/benchmark.tcl)。110条原始计时汇总见[p_optimization_samples.tsv](../../../../../scripts/edadb/performance/p_optimization_samples.tsv)，含批次、原始路径、cold/warm、每次微秒值。

### 测试框架与计时位置

```text
run.sh：每个样本依次启动3个独立iEDA进程
  native进程：LEF初始化 → [def_init计时] → [def_save计时]
  write进程： LEF初始化 → def_init准备对象 → [edadb_write计时]
  read进程：  LEF初始化 → [edadb_read计时] → def_save验证输出
run.sh：严格diff → 从日志提取EDADB_PERF行 → result.tsv
统计：分cold/warm、分读/写，取5次正式样本中位数；预热不入统计
```

- `run.sh:136`启动进程，`:157`组织样本，`:185`严格diff，`:199`收集四个时间，`:223`串行运行cold/warm。
- `benchmark.tcl:18`的`time_command()`：先记录`T0 = clock microseconds`，通过`uplevel`执行完整同步命令，返回后记录T1，输出`T1-T0`。打印和flush在结束取时之后，不逐行插入timer。
- `benchmark.tcl:39`加载LEF，早于任何计时；`:49–50`原生读写、`:59`EDADB写、`:68`EDADB读。计时单位us，表中除以1,000变成ms。
- 这是墙钟经过时间，包含命令内部CPU、等待和I/O，不是CPU时间。现有`clock microseconds`不是本实验另行验证的单调时钟；不能声称没有任何计时扰动。端点计时避免高频插桩，仍保留进程调度和系统负载波动。
- EDADB写入的建表/COMMIT包含在命令时间里；历史profiling可解释阶段，但不能将ON分项从OFF总时间相减。P5 RSS使用独立`/usr/bin/time`测进程峰值，不混入本表计时。

复现（先检出对应父子提交并构建Release、关闭profiling及SQL trace）：

```bash
PERF_WARMUPS=1 PERF_RUNS=5 PERF_SETTLE_SECONDS=5 \
  IEDA_BIN="$PWD/bin-release/iEDA" OUT_DIR=/tmp/iedadb_stage_compare \
  bash scripts/edadb/performance/run.sh \
    scripts/design/sky130_gcd/result/iPL_filler_result.def
```

每个版本使用不同OUT_DIR，避免覆盖。`result.tsv`为原始us，`cold/`、`warm/`保留native/write/read日志、DEF和DB。本轮另生成均值、中位数、最小最大值的`summary.tsv`。

**主指标：相对原生iEDA加速比S = 同批原生时间 / EDADB时间。** S>1才表示比原生快；S<1表示比原生慢。历史step benefit另用`1-本阶段/前阶段`说明耗时变化，不再称为“相对iEDA加速”。不同批次不能都除以同一个历史native时间。

## 3. 原始实验：问题有多大？

历史B0批次，filler、warm：

| 路线 | 读取ms | 写入ms |
| --- | ---: | ---: |
| 原生iEDA | 71.285 | 8.663 |
| 未优化iEDA + EDADB（B0） | 19164.606 | 2011.603 |

这就是起点：EDADB读取约为原生的269倍耗时，写入约232倍。后续需要分别解决读取访问路径与写入事务开销，不能把二者混成一个指标。

## 4. filler：各累计版本对比原生iEDA

主表每行使用该版本同批native作为分母基准；均为warm、5次中位数。加速比统一为原生时间/EDADB时间，小于1说明EDADB尚未超过原生。

### 4.1 读取

| 累计优化 | 同批原生iEDA ms | EDADB ms | 相对原生加速比S | 解释：EDADB耗时/原生 |
| --- | ---: | ---: | ---: | ---: |
| B0 | 71.285 | 19164.606 | 0.00372× | 268.84倍 |
| P1 | 70.667 | 169.480 | 0.41696× | 2.40倍 |
| P1 + P3 | 70.631 | 167.552 | 0.42155× | 2.37倍 |
| P1 + P3 + P4 | 74.189 | 172.396 | 0.43034× | 2.32倍 |
| P1 + P3 + P4 + P5 | 71.119 | 169.579 | 0.41939× | 2.38倍 |
| 全部P（最终验收） | 71.023 | 152.938 | 0.46439× | 2.15倍 |

**读取takeaway：从原始约269倍耗时降至最终约2.15倍耗时，但仍慢于原生。P1消除主要扫描开销；不能将P3/P4/P5的小幅读变化都归因于优化。**

### 4.2 写入

| 累计优化 | 同批原生iEDA ms | EDADB ms | 相对原生加速比S | 解释：EDADB耗时/原生 |
| --- | ---: | ---: | ---: | ---: |
| B0 | 8.663 | 2011.603 | 0.00431× | 232.21倍 |
| P1 | 8.633 | 2224.625 | 0.00388× | 257.69倍 |
| P1 + P3 | 8.675 | 1131.619 | 0.00767× | 130.45倍 |
| P1 + P3 + P4 | 8.741 | 378.269 | 0.02311× | 43.28倍 |
| P1 + P3 + P4 + P5 | 8.672 | 430.186 | 0.02016× | 49.61倍 |
| 全部P（最终验收） | 8.710 | 410.681 | 0.02121× | 47.15倍 |

**写入takeaway：从原始约232倍耗时降至最终约47倍耗时，仍慢于原生。主要收益来自P3/P4事务合并，P5不是写时间优化。**

历史P5首次filler写430.186 ms，后续复核395.792 ms；保留首次记录，不挑选最优批次。最终验收与阶段实验分属不同批次，step benefit及限制见下节。

## 5. 每个P增加了什么，收益依据是什么？

### 5.1 每一步的step benefit

**状态：每个P都做过历史性能评测；尚未完成当前统一环境下、同一数据集贯穿全部P的逐提交复测。** 本轮仅完成P1复测。下面使用历史5次warm中位数，正百分比表示耗时减少，负百分比表示耗时增加，不能保证每个P都改善读写时间。

| 新增优化 | 比较的数据集／前置状态 | 读：前→后 ms | 读耗时减少 | 写：前→后 ms | 写耗时减少 |
| --- | --- | ---: | ---: | ---: | ---: |
| P1 | filler，B0→P1 | 19164.606→169.480 | 99.12% | 2011.603→2224.625 | -10.59% |
| P3 | filler，P1→P1+P3 | 169.480→167.552 | 1.14% | 2224.625→1131.619 | 49.13% |
| P4 | filler，P1+P3→再加P4 | 167.552→172.396 | -2.89% | 1131.619→378.269 | 66.57% |
| P5 | filler，P1+P3+P4→再加P5 | 172.396→169.579 | 1.63% | 378.269→430.186 | -13.72% |
| P2 | routed压力，P1+P3+P4+P5→再加P2 | 1966.411→1473.071 | 25.09% | 2957.790→2785.886 | 5.81% |

- P3/P4/P5没有改读路径，小幅读时间变化不能直接称为优化收益；P2没有改写路径，5.81%也不是已证实的写入优化收益。
- P5不能包装成filler写入加速：此处测得变慢13.72%。它的主要收益是下述routed压力测试的内存下降；另一压力测试进程时长仅增加0.59%，两者测量范围和数据集不同，不能相互替代。
- P2最后一行不能接在filler时间上作差。第4节最终filler读152.938 ms来自最终验收，并不是同批P2前后实验。
- 若要求完整、统一的step benefit验收，还需对B0、P1、P1+P3、P1+P3+P4、再加P5、再加P2逐个重编译，在固定filler和固定routed压力输入上分别复测。每步输出读写中位数/范围、相对前步和B0的变化、严格DEF校验；P5另测峰值RSS。各版本都需重测，不能只把本轮P1插入历史链。

### 5.2 修改与收益证据

- **P1：父键查询索引。** 在没有生成主键覆盖的child表上，为完整parent-FK列链建立普通复合索引。本例产生13个索引，Net Point/ViaRef按父键查找从SCAN变为SEARCH；查询次数未减少。filler warm读从19164.606降至169.480 ms；DB从2,203,648增至2,936,832 B（+33.27%）。它以写入/空间成本换取查找速度。
- **P3：一个schema事务。** 原15棵root schema tree各自提交，改为全部建表/索引在一个事务内完成，减少14次提交边界；DDL和P1索引不变。相对P1，warm写减少49.13%。profiling-ON阶段证据为schema init 1328.322→93.294 ms；这是带仪器时间，不能从上表OFF总时间直接相减。
- **P4：一个design事务。** schema初始化后，原10个非空root family分别提交，改为全部数据一次提交，减少9次提交边界；失败时整个design回滚。相对P3，warm写减少66.57%。这既改变性能，也加强了失败原子性。
- **P5：流式Shadow。** Instance/Pin/SpecialNet/Net不再保留全部root的Shadow集合，而是复用insert operator，每次构造一个root Shadow，写完即释放。已提交历史报告的routed压力实验：写进程峰值302,184→273,356 KiB，减少28.15 MiB（9.54%）；进程总时长13.63→13.71 s（+0.59%）。这是进程RSS/总时长，不是Tcl命令读写时间，也不能填入filler时间表。原始临时目录后续被复测使用，这组具体内存数字以该提交内报告为依据。
- **P2：Wire范围叶子批量读取。** 保留P1索引，在Net读取中把Point/ViaRef/VirtualPoint从“每个Segment一次”改为“每个Wire范围一次，再按Segment和vector序号恢复”。不是整库BFS，也没有修改写入路径。

### P2独立增量：routed压力数据

以下“P2前”已经含P1、P3、P4、P5。此数据集没有这里可比的原始B0测量，因此不编造相对最初B0的累计提升。

| 操作／指标 | 加P2前 | 加P2后 | 增量效果 |
| --- | ---: | ---: | --- |
| warm EDADB读取ms | 1966.411 | 1473.071 | 减少25.09%，1.33×加速 |
| warm EDADB写入ms | 2957.790 | 2785.886 | 观测减少5.81%，非写路径优化结论 |
| Net child查询数 | 447699 | 11739 | 减少97.38% |

读查询减少不等于总读时间同比减少：仍需step、取列、恢复完整对象。写时间变化只作为回归观测。前后DB内容保持一致，未观察到峰值RSS回退。

## 6. 最终累计效果：历史最终验收对照

历史最终验收另重跑了未优化版本，所以其B0数值与第3节不同。**以下前后均采用该验收批次数据**，用于报告累计变化；主要加速比仍按同批原生计算。

| 操作 | 未优化EDADB ms | 全部P后EDADB ms | 优化前后耗时减少 | 优化版同批原生ms | 相对原生加速比S |
| --- | ---: | ---: | ---: | ---: | ---: |
| warm读取 | 18890.395 | 152.938 | 99.19% | 71.023 | 0.46439× |
| warm写入 | 2105.656 | 410.681 | 80.50% | 8.710 | 0.02121× |

**总takeaway：全部P之后，相对原生iEDA的读取加速比为0.46439×，写入为0.02121×，仍未达到1×，即仍慢于原生。** 作为优化进展的补充：相对未优化EDADB，读耗时减少99.19%、写耗时减少80.50%；这不是相对原生的收益。历史最终验收记录core 27/27、adapter 15类回归及36次性能roundtrip通过；这些不是本次重跑。

## 7. 本轮P1复测：独立保留，不替换历史链

本轮只编译并运行P1，cold/warm各5次，SQL trace/profiling关闭，12次DEF diff通过。

| 操作 | 缓存 | 原生iEDA ms | P1 EDADB ms | 相对原生加速比S |
| --- | --- | ---: | ---: | ---: |
| 读 | cold | 47.454 | 160.835 | 0.29505× |
| 读 | warm | 47.075 | 127.231 | 0.37000× |
| 写 | cold | 6.902 | 2409.798 | 0.00286× |
| 写 | warm | 6.835 | 2538.490 | 0.00269× |

本轮原生iEDA本身也比历史批次快，说明不能直接把本轮P1与历史P3/P4相减作严格增量归因。当前P1 warm读125.044–133.369 ms，写2078.391–2658.916 ms，写入波动明显。完整证据目录：`/tmp/iedadb_p1_retest_20260922/`，含`report.md`、`summary.tsv`、`manifest.txt`和索引计划；原始10条计时也已复制入本文TSV。

## 8. 数据追溯与阅读

1. 先读第3–4节：理解原始差距和逐阶段累计效果；读写分开。
2. 再读第5节：解释增量的原因，特别区分P5内存与P2查询数。
3. 第6节用于汇报最终结果；第7节用于本轮P1复测，二者不能混为同批。
4. [原始样本汇总](../../../../../scripts/edadb/performance/p_optimization_samples.tsv)：按`batch`筛选，每组cold/warm各5条，`time_unit=us`；`source_path`记录原始文件位置。

阶段实现及验收说明保存在`edadb-performance-optimization`分支的`scripts/edadb/performance/`下；当前P1检出没有后续报告，可用`git show edadb-performance-optimization:scripts/edadb/performance/<文件名>`查看，不必切分支：

| 阶段 | 已提交报告文件名 |
| --- | --- |
| P1 | `sky130_gcd_ipl_filler_p1_child_fk_index_20260829.md` |
| P3 | `sky130_gcd_ipl_filler_p3_schema_transaction_20260829.md` |
| P4 | `sky130_gcd_ipl_filler_p4_design_transaction_20260829.md` |
| P5 | `sky130_gcd_routed_p5_stream_shadow_20260829.md` |
| P2前复核 | `sky130_gcd_p2_reassessment_20260829.md` |
| P2 | `sky130_gcd_routed_p2_leaf_batch_20260829.md` |
| 最终验收 | `sky130_gcd_full_validation_20260830.md` |

本文未重新测量各P所有版本，也未量化P6对象重建剩余成本。若要得到当前机器条件下严格的逐P净收益，需要固定输入哈希及构建环境，逐提交重复同一测试；不能用已有跨批次结果替代。
