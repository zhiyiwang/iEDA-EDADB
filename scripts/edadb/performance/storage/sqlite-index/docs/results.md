# SQLite单表主键与索引：实测结果

数据：1,000,000条Component、8字段；每组5次，单位ms，表格使用中位数。
检查：48组小规模正确性、80个正式样本、16组独立全量逐字段/约束/结构诊断通过。
整批运行时长：20.9分钟。正式样本串行，Release -O3，系统SQLite；无逐行计时。

## 结论

- **none不是“没有B-tree”，而是“没有额外索引”。** SQLite仍以隐式rowid为键，用一棵表B-tree存储整行；本轮schema根页与dbstat已确认该结构。INSERT未指定rowid，因此SQLite自动分配rowid，并编码8字段、维护表树及页面。按rowid可直接定位，但本轮点查条件是name，没有name索引就要扫描；全读则遍历全部记录。因此none的写入时间仍包含表自身的存储组织成本，不能视作纯文本复制或零索引维护成本。依据：[SQLite rowid表](https://www.sqlite.org/rowidtable.html)、[表的存储格式](https://www.sqlite.org/fileformat.html#representation_of_sql_tables)。
- **额外索引确实增加写入和空间成本。** 文件库index相对none的同轮写入差值中位数为+1,079.387 ms；dbstat确认表本身都占10,676页，普通name索引另占4,759页（19,492,864 B）。差值不是索引某个函数的独占时间。
- **name查找收益与全读收益不同。** 文件库100次name点查从13,268.658 ms变为2.239 ms；无索引诊断实测99,999,900次FULLSCAN_STEP。全读仍取全部1,000,000行，各布局约790–808 ms，并没有对应的巨大加速。
- **TEXT主键不等于没有额外索引。** 实测text-pk有表树和自动唯一索引树；改为WITHOUT ROWID后仅一棵表树，文件库写中位数2,743.657 → 2,310.086 ms，空间63,225,856 → 46,137,344 B。结果支持本例采用不同布局可减少总成本，不是把差值全部算成“删除索引的函数时间”。
- **整数主键成为rowid别名有实际区别。** integer-unique有两棵树，integer-rowid一棵；文件库写2,584.098 → 1,793.381 ms，空间57,421,824 → 40,706,048 B。差值包含自动分配rowid、记录编码及索引维护等变化。
- **WITHOUT ROWID并非总更快。** 相同INTEGER主键下，文件库integer-without-rowid写2,154.715 ms，反而慢于integer-rowid的1,793.381 ms；两者都只有一棵树，说明不能仅凭树数量预测性能。
- **唯一性小差值不能过度归因。** unique与text-pk均为24,000,000个INSERT VM步骤，文件库同轮写差值中位数仅+1.054 ms，范围跨零；没有证据证明两种声明有稳定速度差异。VM步骤不是CPU指令：TEXT WITHOUT ROWID的VM步数更多，但本例写入更快。

范围：单表、固定8字段、递增键插入、warm读取。未测FK、UPDATE、随机插入或cold；没有修改SQLite、EDADB、adapter，也未测试掉电恢复。建表/提交单列，不计入上述write。内存配置与文件配置同时改变存储、日志及缓存，不能把差值全部归因于磁盘。

配置实测：SQLite 3.37.2；A为memory/MEMORY/OFF、cache_size=-8192；B-batch为文件/DELETE/FULL、cache_size=-2000；page_size均4096 B。两组均显式批量事务。CPU固定0，服务器Xeon E5-2650 v3，正式进程不并行。

各实现主键、额外索引数和读写操作见[测试计划](test_plan.md)；代码及运行入口见[readme](../readme.md)。

## 与已有baseline的对应关系

**只有本轮none与baseline的SQLite直接路线属于相同的无额外索引场景；不是严格同代码、同DDL复测。** 其余7种主键/索引变体，以及100次点查，均为新增实验。baseline的EDADB或adapter不作为本轮SQLite索引组的等价参照。

同为1,000,000条、8字段、5次中位数；写入都包含prepare → bind/step/reset → finalize，**均排除init/create/BEGIN/COMMIT/close**。不是本轮把建表或提交混进write导致数值变大。

| 写入参照 | baseline write ms | 本轮none write ms | 跨批中位数差 ms | 相对变化 |
| --- | ---: | ---: | ---: | ---: |
| SQLite A（baseline结果主表所用first批次） | 1510.430 | 1563.863 | +53.433 | +3.54% |
| SQLite B-batch | 1597.901 | 1624.210 | +26.309 | +1.65% |

这些只是两个批次中位数的数值对照，**不是配对实验，也不是NOT NULL或某个API的独占成本**。不能用本轮index减历史baseline来计算索引代价；索引代价使用第3节的同批index−none。

已核对的区别：

| 项目 | baseline历史实现 | 本轮实现 | 对比较的影响 |
| --- | --- | --- | --- |
| name定义 | TEXT，可为NULL | TEXT NOT NULL | 约束不同；正式输入虽然都非NULL，不能视为完全相同DDL |
| 整数绑定 | 6个整数均用sqlite3_bind_int64，通过数组循环绑定 | 5个int32字段用sqlite3_bind_int，record_order用bind_int64，逐字段调用 | 存入的值相同，但C API及C++执行路径不同 |
| INSERT文本 | INSERT INTO component VALUES(...) | 显式列出8个字段后VALUES(...) | 同样写入8字段，prepare的SQL文本不同 |
| 编译与程序 | g++-10、Release -O3、LTO；五路线程序 | GCC 11.4、Release -O3、无LTO；独立SQLite程序 | 编译产物与布局不同，不能当同二进制复测 |
| 调度与等待 | 原runner未显式固定CPU；样本前及读写间等待5秒 | 固定CPU0；样本前等待2秒 | 均串行，但运行控制不完全相同 |
| 建表/事务 | 无显式PK/FK/index；批量数据事务，create与COMMIT单列 | none同样无显式PK/FK/index；同样单列 | 阶段边界一致；NOT NULL差异仍存在 |

读取另作比较，不能拿读的变化解释写的变化：
- A：baseline结果主表的825.183 ms是first读取，本轮808.029 ms是预读后读取。更接近的是baseline原始统计中的repeat **811.196 ms**，本轮少3.167 ms（约0.39%）。
- B-batch：baseline **844.419 ms**，本轮 **803.720 ms**；都为OS-warm新连接，但baseline通过文件缓存准备，本轮先执行一次完整SELECT再重开连接；读循环实现和编译也不同。不能直接称为4.82%的优化收益。
- 本轮正式读的逐字段验证由模板在编译期去除；历史代码的Consumer保留运行时verify判断。是否及多少被编译器消除没有单独测量，不据此分摊差值。

核对依据：[baseline结果](../../baseline/results.md)、[历史统计](../../../../../../../../../../../../tmp/iedadb_stream_full_20260910/summary.tsv)、[测量时源码快照](../../../../../../../../../../../../tmp/iedadb_stream_full_20260910/source/stream_benchmark.cpp)、[历史编译参数](../../../../../../../../../../../../tmp/iedadb_stream_full_20260910/compile_flags.txt)、[历史链接参数](../../../../../../../../../../../../tmp/iedadb_stream_full_20260910/link_command.txt)；本轮见[绑定与SQL实现](../benchmark.cpp)、[运行脚本](../run.py)。没有修改冻结的baseline，也没有为此重复运行实验。

## 1. 数据读写

write不含init/create/BEGIN/COMMIT/close；scan取全8列；point为100次命中点查总时间。
TEXT组按name点查，整数组按record_order点查；两组之间不可把键类型差值称为rowid独占开销。
A为内存连接预热读取；B-batch为OS-warm文件、新连接读取，不是cold。

| 配置 | 实现 | write data | 全读scan | 100次point |
| --- | --- | ---: | ---: | ---: |
| A | none（对应baseline无索引场景，见上节差异） | 1563.863 | 808.029 | 12106.583 |
| A | index | 2589.313 | 796.736 | 0.392 |
| A | unique | 2657.203 | 796.008 | 0.378 |
| A | text-pk | 2625.799 | 795.160 | 0.379 |
| A | text-pk-without-rowid | 2198.285 | 804.976 | 0.297 |
| A | integer-unique | 2486.852 | 795.814 | 0.348 |
| A | integer-rowid | 1705.485 | 799.812 | 0.237 |
| A | integer-without-rowid | 2094.109 | 793.494 | 0.264 |
| B-batch | none（对应baseline无索引场景，见上节差异） | 1624.210 | 803.720 | 13268.658 |
| B-batch | index | 2697.484 | 803.637 | 2.239 |
| B-batch | unique | 2736.137 | 801.993 | 2.190 |
| B-batch | text-pk | 2743.657 | 806.901 | 2.206 |
| B-batch | text-pk-without-rowid | 2310.086 | 796.620 | 1.913 |
| B-batch | integer-unique | 2584.098 | 805.457 | 2.064 |
| B-batch | integer-rowid | 1793.381 | 793.835 | 1.528 |
| B-batch | integer-without-rowid | 2154.715 | 790.752 | 1.816 |

## 2. 建表、提交与空间

create包含建表事务及空表索引创建；commit仅为数据事务COMMIT调用。它不是全部I/O或纯sync耗时。
空间为page_count×page_size；文件库对应DB文件页空间，内存库不是进程RSS。

| 配置 | 实现 | create ms | commit ms | DB页空间 B |
| --- | --- | ---: | ---: | ---: |
| A | none | 0.159 | 1.476 | 43,732,992 |
| A | index | 0.234 | 19.479 | 63,225,856 |
| A | unique | 0.229 | 19.424 | 63,225,856 |
| A | text-pk | 0.185 | 19.280 | 63,225,856 |
| A | text-pk-without-rowid | 0.181 | 1.523 | 46,137,344 |
| A | integer-unique | 0.228 | 17.893 | 57,421,824 |
| A | integer-rowid | 0.171 | 1.350 | 40,706,048 |
| A | integer-without-rowid | 0.172 | 1.540 | 46,137,344 |
| B-batch | none | 85.059 | 617.838 | 43,732,992 |
| B-batch | index | 72.328 | 727.285 | 63,225,856 |
| B-batch | unique | 82.298 | 870.267 | 63,225,856 |
| B-batch | text-pk | 65.168 | 914.423 | 63,225,856 |
| B-batch | text-pk-without-rowid | 63.633 | 664.635 | 46,137,344 |
| B-batch | integer-unique | 74.810 | 796.302 | 57,421,824 |
| B-batch | integer-rowid | 92.541 | 589.923 | 40,706,048 |
| B-batch | integer-without-rowid | 74.177 | 525.883 | 46,137,344 |

## 3. 同轮写入差值

先逐轮相减，再取中位数；不是两个中位数相减。正值表示前者更慢。差值包含布局、约束、缓存等净变化，不是某函数独占耗时。

| 配置 | 前者 − 参照 | Δ中位数 ms | Δ最小–最大 ms | Δ ns/记录 |
| --- | --- | ---: | ---: | ---: |
| A | index − none | 1041.518 | 1007.625–1186.200 | 1041.5 |
| A | unique − index | 68.311 | -87.955–81.400 | 68.3 |
| A | text-pk − unique | -31.404 | -44.391–3.023 | -31.4 |
| A | text-pk-without-rowid − text-pk | -414.606 | -447.763–-387.563 | -414.6 |
| A | integer-rowid − integer-unique | -788.483 | -797.965–-767.257 | -788.5 |
| A | integer-without-rowid − integer-rowid | 391.437 | 362.862–405.125 | 391.4 |
| B-batch | index − none | 1079.387 | 1066.174–1110.335 | 1079.4 |
| B-batch | unique − index | 38.653 | 6.923–47.623 | 38.7 |
| B-batch | text-pk − unique | 1.054 | -35.720–35.070 | 1.1 |
| B-batch | text-pk-without-rowid − text-pk | -405.960 | -479.510–-395.170 | -406.0 |
| B-batch | integer-rowid − integer-unique | -792.433 | -975.293–-775.971 | -792.4 |
| B-batch | integer-without-rowid − integer-rowid | 385.128 | 332.488–409.573 | 385.1 |

## 4. 实际访问计划与计数

实际sqlite_schema根页与dbstat核验额外索引数：none=0、index=1、unique=1、text-pk=1、text-pk-without-rowid=0、integer-unique=1、integer-rowid=0、integer-without-rowid=0；各有一棵存整行的表树。这里的0不是“不维护B-tree”。

额外索引组的INSERT计划同时包含表插入和索引插入；唯一键组另有冲突检查。点查额外索引组出现SeekGE/DeferredSeek并取非索引列；INTEGER PRIMARY KEY使用SeekRowid；WITHOUT ROWID直接在PK组织的表查找。EXPLAIN是编译后的程序，不能把其中每个分支都当成实际执行次数，也不能用Insert/IdxInsert操作码数量推断树数量。

下表为独立A配置诊断；完整B配置、VM及树信息见diagnostics。计数不是毫秒或CPU指令数。

| 实现 | 点查EQP | INSERT VM步数 | 全读VM步数 | 100次点查FULLSCAN_STEP |
| --- | --- | ---: | ---: | ---: |
| none | SCAN component | 17,000,000 | 10,000,006 | 99,999,900 |
| index | SEARCH component USING INDEX component_name_idx (name=?) | 23,000,000 | 10,000,006 | 0 |
| unique | SEARCH component USING INDEX component_name_idx (name=?) | 24,000,000 | 10,000,006 | 0 |
| text-pk | SEARCH component USING INDEX sqlite_autoindex_component_1 (name=?) | 24,000,000 | 10,000,006 | 0 |
| text-pk-without-rowid | SEARCH component USING PRIMARY KEY (name=?) | 29,000,000 | 10,000,006 | 0 |
| integer-unique | SEARCH component USING INDEX component_order_idx (record_order=?) | 25,000,000 | 10,000,006 | 0 |
| integer-rowid | SEARCH component USING INTEGER PRIMARY KEY (rowid=?) | 20,000,000 | 10,000,006 | 0 |
| integer-without-rowid | SEARCH component USING PRIMARY KEY (record_order=?) | 30,000,000 | 10,000,006 | 0 |

## 5. 原始证据

- [原始阶段计时](../../../../../../../../../../../../tmp/iedadb_sqlite_index_full/samples.tsv)、[统计含min/max](../../../../../../../../../../../../tmp/iedadb_sqlite_index_full/summary.tsv)、[样本配置及空间](../../../../../../../../../../../../tmp/iedadb_sqlite_index_full/samples.json)。
- [正确性](../../../../../../../../../../../../tmp/iedadb_sqlite_index_full/checks.json)、[完整审计](../../../../../../../../../../../../tmp/iedadb_sqlite_index_full/audit.json)、[独立诊断](../../../../../../../../../../../../tmp/iedadb_sqlite_index_full/diagnostics.json)、[构建及硬件](../../../../../../../../../../../../tmp/iedadb_sqlite_index_full/manifest.json)。
- 正式timing和独立diagnostics的log分别保存；诊断TIME不用于统计。
