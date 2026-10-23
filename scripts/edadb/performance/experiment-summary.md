# iEDA＋EDADB实验总结

范围：本仓库历史iEDA/EDADB与SQLite实验汇总，随本提交保存。独立`/home/zhiyiwang/cs/db`已有更新的三库/Parquet平面实测，见该目录sqlite/bench/results-brief.md及[后端交接](storage/duckdb-backend-handoff.md)；不将它们与本文旧批次拼成同批排名。后端迁移仍是方案，非已交付实现。

## 1. 总结与阅读范围

**现有优化显著改善未优化EDADB，但已归档的完整设计读写结果尚未快于原始iEDA DEF。** 读取最明确的收益来自父键索引，写入来自事务合并；流式Shadow主要降低内存。SQLite单体实验进一步表明：多行INSERT值得优化，三表交织不是本次简单场景的主要瓶颈。

本文汇总已有证据，不是一次新的统一复测。正文按“端到端效果→优化实现→受控SQLite实验”组织，读写分开。时间默认ms、中位数；另有明确标记的均值。不同数据集、版本、编译器和计时边界的数字不能相减作归因，各项加速比不连乘。

| 实验层次 | 回答的问题 | 不能直接回答的问题 |
| --- | --- | --- |
| 完整DEF／EDADB | 同一设计保存、恢复需要多久 | 每个SQLite内部函数独占多少时间 |
| 五路线合成基线 | 文本、原生iEDA、SQLite、EDADB、adapter各承担多少工作 | 将adapter与简单Record差值全部视为框架开销 |
| API、索引、FK对照 | 同批改变一个因素的净影响 | 推广到所有schema、设计和缓存状态 |
| SQLite单表粒度／三表交织 | SQL执行粒度、切换频率是否影响性能 | 代替完整iDB正确性与端到端验收 |

## 2. 服务器与测量方法

| 项目 | 配置及适用范围 |
| --- | --- |
| 服务器 | cherry13，x86_64 |
| CPU | 双路Intel Xeon E5-2650 v3 @ 2.30GHz；每路10核、每核2线程，共40逻辑CPU |
| 内存 | 系统可见约125 GiB |
| 系统 | Ubuntu 22.04.3 LTS；SQLite写入实验manifest记录Linux 5.15.0-187-generic |
| 文件存储 | 当前/tmp位于ext4、/dev/sdb3；设备由PERC H330 Mini呈现。未保存完整物理盘/RAID缓存策略，不据此推断sync延迟 |
| 软件 | 系统SQLite 3.37.2；历史iEDA/五路线使用GCC 10 Release -O3；最新独立SQLite实验GCC 11.4 -O3。不是相同二进制 |
| 执行 | 性能样本串行、通常预热1次正式5次；新SQLite固定CPU0，P5对照固定CPU2，历史五路线未显式绑核 |
| 观测 | 正式耗时、计数诊断、perf采样分开；不逐条bind/step读取时钟 |

硬件与文件系统为本次整理时实查，历史容量和CPU型号与已有报告吻合；不将当前设备信息当作每个历史批次完整环境快照。新实验的编译器、源码哈希和动态库见[manifest](storage/sqlite-write/results/rw-manifest.json)。

**计时边界必须区分：**
- 完整设计：Tcl命令前后墙钟时间，LEF加载、写前输入准备、读后校验在外；EDADB完整写命令包含建表及提交。
- SQLite单体：write data为prepare→bind/step/reset→finalize；BEGIN/COMMIT、init/create/close另计。read data包含完整字段取得、保存和摘要消费。
- 优化版adapter的历史write data包含数据事务与清理，不能和排除COMMIT的SQLite单体write data当作同口径。
- 文件读取主要为warm；文本flush不是fsync。内存SQLite与文件文本不是相同介质，DEF输出与FULL同步数据库不是等价持久性工作量。

## 3. 数据集与schema

| 数据集 | 规模 | 用途 |
| --- | --- | --- |
| Sky130 GCD filler | iPL_filler_result.def，726,155 B；2,604 Component、56 Pin、677 Net、2 SpecialNet、4 Via | 原始iEDA与未优化／已优化EDADB完整操作 |
| routed压力fixture | 6,305,169 B；从iRT输入额外复制1,000个Net路由内容 | P5内存与后续读取压力；不是新的物理有效芯片 |
| 合成Component | 1千至100万条；主要对比100万条、8字段 | 五路线、API对齐、单表索引、SQLite与文本 |
| 父子图 | 10,000 Component＋100,000 Pin，每父10子 | 子表索引、FK检查、逐父查询 |
| 三表交织 | 3表各100,000条，总计300,000条 | SQLite三表与文本三文件调度 |

简单SQLite基线及最新粒度/交织实验使用：

```sql
CREATE TABLE component (
  name TEXT, master_name TEXT,
  source INTEGER, status INTEGER, orient INTEGER,
  x INTEGER, y INTEGER, record_order BIGINT
);
```

没有声明PK/FK或额外索引，但仍有rowid表B-tree。name为U加7位序号，master_name为bench_cell；source/status/orient为2/3/1；x=(i%1000)×100，y=(i/1000整数除法)×100，record_order=i。三表仅改表名及source=2/3/4，便于发现错配。文本同序8字段、空格分隔、每行一条，不是DEF。

主键/索引实验按变体改变DDL；真实adapter使用应用schema而非上述简化表。五路线adapter包含41张表，Instance为19列、名字主键，还需恢复引用及完整iDB，因此工作量不同。

## 4. 完整设计：原始iEDA、未优化EDADB、已优化EDADB

数据为同一filler设计。原始iEDA为接入前007435241；未优化EDADB为62504bd9d/core be6bbdd；已优化交付代码基于0da489453/core 494ce79，包含父键索引、两类事务合并及流式Shadow，不含历史P2。

**下表来自不同批次：原始及未优化为阶段计时OFF，已优化为P5验收ON。ON/OFF仅指我们添加的阶段计时，不是SQLite选项。** 已优化实测core为1c4857c，之后交付core合入默认关闭的SQL trace；未在最终标签重跑全矩阵。表用于已有结果概览，不是严格同批净收益。

### 4.1 读取

| 实现 | 完整read | 耗时÷原始iEDA |
| --- | ---: | ---: |
| 原始iEDA DEF | 47.484 | 1.00 |
| 未优化iEDA＋EDADB | 11931.378 | 251.27 |
| 已优化iEDA＋EDADB | 135.813 | 2.86 |

相对未优化约87.85倍加速、耗时降低98.86%，但仍慢于原始DEF。已优化内部init=0.460、read data=131.406；data包含查询、Shadow恢复、iDB组装与清理。没有未优化的同口径内部阶段数据，不补算。整体还包含移除参考DEF扫描等改变，不能全归给索引。

### 4.2 写入

| 实现 | 完整write | 耗时÷原始iEDA |
| --- | ---: | ---: |
| 原始iEDA DEF | 6.855 | 1.00 |
| 未优化iEDA＋EDADB | 2277.177 | 332.19 |
| 已优化iEDA＋EDADB | 400.614 | 58.44 |

相对未优化约5.68倍加速、耗时降低82.41%，但仍慢于原始DEF。已优化init=0.180、create=83.288、write data=317.389，后者含数据提交；各阶段中位数不保证相加等于总时间中位数。数据库还承担建表、索引、约束及提交，不能将58.44倍解释为纯序列化差距。

### 4.3 优化如何实现、分别改善什么

| 修改 | 实现与减少的工作 | 已有证据 | 限制 |
| --- | --- | --- | --- |
| P1：子表父键索引 | core按完整父FK生成索引；Point/ViaRef查找避免重复扫描整张子表 | 历史filler读19164.606→169.480，113.08倍加速；写2011.603→2224.625，增加10.59% | 查询次数未减少；索引增加写入成本 |
| P3：建表事务合并 | initWriteDb外层统一事务，内部建表不独立提交 | 历史完整写2224.625→1131.619，降低49.13%；本例15个schema事务变1个 | DDL数量不变，主要减少提交 |
| P4：design写事务合并 | def_write_edadb统一数据事务，各root失败整体回滚 | 历史完整写1131.619→378.269，降低66.57%；10个非空root事务变1个 | INSERT数量不变，不是多行INSERT |
| P5：流式Shadow | Instance/Pin/SpecialNet/Net writer复用InsertOp；逐root转换、写入、释放，替代整组Shadow缓存 | 同批压力峰值RSS302,304→273,432 KiB，减少28.20 MiB（9.55%）；write data2858.198→2876.312，增加0.63% | 主要是内存优化，未改读取；不宣称写加速 |

P3/P4另有阶段对照：create 982.339→89.526（降低90.89%），含提交的write data 797.298→259.063（降低67.51%）。它们属于另批阶段观测，不与第4节总时间相减，也不视为sync独占时间。

验收包括15/15功能回归（10个严格DEF、5个按既有规则规范化root顺序）、DB完整性/FK检查、插入失败及转换失败后的design整体回滚与同进程重试。故障注入二进制不参与性能测量，未覆盖所有异常或掉电恢复。

## 5. 五路线基线与SQLite开销解释

### 5.1 五路线：100万条合成Component

下表为历史文件配置，读取为warm；与第4节filler不是同一数据集。

| 路线 | write data | read data |
| --- | ---: | ---: |
| C++简单文本 | 748.833 | 622.220 |
| 原生iEDA DEF | 702.185 | 4843.321 |
| SQLite直接API | 1597.901 | 844.419 |
| EDADB直接API | 1685.538 | 1141.248 |
| adapter完整iDB | 5813.067 | 4730.975 |

SQLite/EDADB直接写不含外层提交；adapter写含内部事务，init另含建表；原生iEDA打开关闭在API内。简单Record读取复用对象，而native/adapter恢复完整iDB，不能将二者差值全部归为ORM开销。详见[冻结基线](storage/baseline/results.md)。

### 5.2 SQLite与EDADB对齐：哪些额外操作有证据

- **读：** 手写SQLite加入逐列NULL检查，100万条净差约246～254ms；匹配SQL文本、字符串取列顺序及结束清理后，EDADB剩余差距为−5.401／+20.386ms，样本范围重叠，不等于已测出的遍历独占成本。
- **写：** 在匹配SQL/绑定API后，逐行增加clear_bindings约多71ms；EDADB相对完全对齐SQLite仍多47.080／24.242ms，不能未经诊断归给某个框架函数。
- NULL检查和清绑定服务于通用语义，实验量化其成本，不代表可直接从通用API删除。证据见[API对齐结果](storage/sqlite-vs-edadb/results.md)。

### 5.3 索引与FK：读取收益与写入成本分别看

父子实验文件库，10,000父＋100,000子，读取恢复父子关联：

| 子表定义 | SQLite read | EDADB read |
| --- | ---: | ---: |
| 复合主键 | 150.274 | 162.000 |
| 普通复合索引 | 149.124 | 161.644 |
| 无额外子索引 | 129745.899 | 127462.238 |

无索引是每父一次子表全扫描，不是顺序扫描一次；加索引降低每次查询成本，但不消除N+1。内存同样出现约120秒，不能仅用磁盘解释。固定复合主键、仅切换FK检查时，SQLite文件写346.530→419.516ms，EDADB 343.153→414.834ms：这是合法数据写入的约束检查净影响，不是读阶段成本。[父子实验](storage/sqlite-pk-fk/docs/results.md)

单表100万条实验中：name普通索引增加文件写入净差约1079ms，却将100次name点查从13268.658降至2.239ms；全读仍约790～808ms。TEXT主键普通表有表树与额外唯一索引，WITHOUT ROWID改用主键组织记录，本例写2743.657→2310.086ms；但整数主键WITHOUT ROWID慢于整数rowid别名（2154.715 vs 1793.381ms）。**不是索引越少越好，也不是WITHOUT ROWID统一更快。** [索引与布局实验](storage/sqlite-index/docs/results.md)

## 6. 最新SQLite单体：实验A与实验B

### 6.1 A：单表INSERT粒度，100万条

| 写入方式 | 内存配置write | 文件配置write |
| --- | ---: | ---: |
| 文本文件 | 738.134 | 739.137 |
| SQLite每条INSERT写1行 | 1505.569 | 1565.399 |
| SQLite每条INSERT写10行 | 1009.162 | 1070.581 |
| SQLite每条INSERT写100行 | 936.963 | 1003.515 |

百行相对单行降低37.8%／35.9%，仍比文本慢26.9%／35.8%。事务都为一笔，收益不是减少COMMIT。执行次数100万→1万，VM步骤1600万→1408万，所有字段仍需绑定。

**读取单独看：** 文本583.620／583.794ms，单行写入SQLite库790.186／833.808ms，慢35.4%／42.8%。十行、百行写入生成的库读取接近。每表一次SELECT，持续step，不逐行重新查询；没有测试一次API返回十行。读取结果不能套用写入批量收益。

### 6.2 B：三表交织，总计30万条

Schema为同一数据库中的t0、t1、t2三张独立表，每张均为`(name TEXT, master_name TEXT, source INTEGER, status INTEGER, orient INTEGER, x INTEGER, y INTEGER, record_order BIGINT)`。无显式PK/FK、唯一约束或额外索引，各自保留隐式rowid表树；三表没有关联。每表10万条，source分别为2/3/4，其余按第3节生成；文本为对应的三个8字段文件。完整DDL见[实验B方法](storage/sqlite-write/results.md#b1-方法仅改变表之间的切换顺序)。

| 调度 | 内存SQLite write | 文件SQLite write | 内存SQLite read | 文件SQLite read |
| --- | ---: | ---: | ---: | ---: |
| 每条切换 | 446.381 | 516.180 | 240.967 | 271.179 |
| 每10条切换 | 445.441 | 533.091 | 237.657 | 274.237 |
| 按表完成 | 442.957 | 511.399 | 236.606 | 274.427 |

写对照固定读调度，读对照固定写调度；不是同时改变两者。三文件文本对应写约220～226ms、读约171～174ms。SQLite写约为文本1.98～2.39倍，读约1.38～1.60倍。

按表写比逐条切换仅改善0.77%／0.93%，配对差值跨零；内存按表读改善1.81%，文件读没有稳定收益。这里连续十次单行INSERT不等于十行VALUES；总写执行次数始终30万，读始终3次SELECT。**交织不是本场景主要差距来源。** 完整文本对照、范围及证据见[最新A/B结果](storage/sqlite-write/results.md)。

### 6.3 对SQLite内部成本的判断

内存SQLite也慢于简单文本，说明不能只归因于同步。已有独立写perf中，单行路线step调用链约59.56%、绑定约36.07%；百行后约38.00%／61.66%，是相对CPU采样比例，不是独占毫秒数，也不代表绑定绝对时间增加。计数器VM_STEP是虚拟机步骤，不是CPU指令。见[写入专项证据](storage/sqlite-write/write-results.md)。

证据支持减少语句执行及安全绑定复制的后续对照；不支持把余下时间全部归给step、索引或adapter。数据库仍需记录编码、VM执行、表存储和取列；要定量拆分必须继续受控诊断。新A/B没有重跑读取perf，旧写perf不能解释新读差距。

## 7. 后续读取原型：不混入已交付优化结论

另有Net-only整数身份／全表游标原型：保留旧DFS，用完整祖先整数ID、每表持续游标和有序merge，避免祖先JOIN、动态Row图与逐行父哈希查找。id表示身份，_vec_idx表示业务位置；不是同一概念。

其归档报告给出：相对现行已优化DFS，filler／压力read data均值降低4.81%／14.65%；但filler写data均值增加56.43%，压力写data中位数增加34.46%。这不是完整design对比，也不是“未优化EDADB→最终版本”，不能宣称已超过原始iEDA。原型写入回退需要先解释，不能只以查询次数少作为成功标准。

该工作已保存到开发分支WIP，当前prof-test未加载，不在本轮SQLite测试内。历史P2按Wire批读与该全表游标原型也不是同一实现，不将其结果混用。

## 8. 证据、版本和后续工作

本页在prof-test整理，未切换代码、未重新测试。当前工作树的权威入口：
1. [本文](experiment-summary.md)：总体结论与计时边界。
2. [五路线基线](storage/baseline/results.md) → [API对齐](storage/sqlite-vs-edadb/results.md)。
3. [父子索引/FK](storage/sqlite-pk-fk/docs/results.md) → [单表布局](storage/sqlite-index/docs/results.md)。
4. [最新SQLite A/B](storage/sqlite-write/results.md)及其原始计时、配置、审计链接。

优化资料在另一分支；为避免链接到当前分支不存在的文件，固定引用iEDA提交`ad6800b72091ebdca16a667a30cbd282321c37a4`内以下文件（该WIP包含历史报告，不代表所有测量运行于该提交）：

```bash
git show ad6800b72091ebdca16a667a30cbd282321c37a4:src/database/edadb/docs/performance/optimization-results.md
git show ad6800b72091ebdca16a667a30cbd282321c37a4:src/database/edadb/docs/performance/stream-shadow.md
git show ad6800b72091ebdca16a667a30cbd282321c37a4:src/database/edadb/docs/performance/root-stream-execution.md
```

它们分别记录第4节总表与历史P序列、P5独立对照及其原始TSV位置、后续Net原型。文档中旧“未commit”状态是当时测量说明；现有WIP保存不等于产品验收完成。不覆盖冻结报告中旧状态，本页按对应manifest和milestone说明解释其范围。

**接下来：** 写侧先做多行INSERT与生命周期安全的绑定策略组合；读侧独立测完整取值及对象恢复热点。将候选引入adapter后，必须统一版本、数据和计时开关，同批比较原始DEF、现行DFS及候选路径，同时验收完整正确性、更新/回滚语义与内存。当前没有统一最终版本全矩阵、cold或掉电持久性验证，不宣称覆盖。
