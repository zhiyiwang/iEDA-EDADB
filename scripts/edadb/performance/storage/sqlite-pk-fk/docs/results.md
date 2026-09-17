# 主外键实验结果

## 1. 数据口径

以下全部使用本轮完整复测批次，不混入此前数字：10,000个Component、100,000个Pin，Release -O3，预热1次、正式5次串行，采样流程约104分钟（包含等待及预热）。88组正确性检查、275条计时（55组×5次）和55个文件库完整审计通过。

本次以迁移后的源码重新构建并完整运行。schema及参数见[方案](test_plan.md)，计时与源码见[实现](implementation.md)。各表单位ms，均为中位数；memory读取选repeat，disk为OS-warm新连接。first结果、均值及min/max保留在原始统计中。

## 2. 写数据

仅write阶段，不含init、create、BEGIN、外层COMMIT、close。

| 存储 | API | pk | index | none |
| --- | --- | ---: | ---: | ---: |
| memory | sqlite | 363.732 | 361.378 | 253.541 |
| memory | edadb | 353.075 | 351.115 | 239.728 |
| disk | sqlite | 419.516 | 419.262 | 317.914 |
| disk | edadb | 414.834 | 410.731 | 306.821 |

- index在保存Pin记录之外，还维护(父名称, Pin名称)索引；pk还要求组合唯一。none只是不维护child索引，仍有父PK及FK检查。
- SQLite disk相对none，index净增101.348 ms（31.88%），pk净增101.602 ms（31.96%）。是整批写入路径净差，不是索引函数独占时间。
- pk与index写入排序并不稳定，不能据小差值量化唯一性检查时间。SQLite成功路径还存在错误字符串构造成本，EDADB的小幅写入优势不能解释为“负的框架开销”。

### 建表与提交单列

| 存储 | API | schema | create | COMMIT |
| --- | --- | --- | ---: | ---: |
| memory | sqlite | pk | 0.271 | 0.059 |
| memory | sqlite | index | 0.321 | 0.056 |
| memory | sqlite | none | 0.251 | 0.033 |
| memory | edadb | pk | 0.269 | 0.055 |
| memory | edadb | index | 0.324 | 0.056 |
| memory | edadb | none | 0.251 | 0.033 |
| disk | sqlite | pk | 75.142 | 162.164 |
| disk | sqlite | index | 69.593 | 142.705 |
| disk | sqlite | none | 70.318 | 125.279 |
| disk | edadb | pk | 75.471 | 152.357 |
| disk | edadb | index | 65.943 | 144.637 |
| disk | edadb | none | 92.402 | 132.530 |

文件库SQLite graph三种schema的5个正式DB大小各自一致：pk/index为6,164,480 B，none为3,551,232 B，增加2,613,248 B。这是整个文件大小之差，不等于索引有效载荷的精确大小。

create包含两表、必要索引和建表事务；COMMIT为数据事务提交调用。write_complete按每条样本的BEGIN+write+COMMIT统计，不把各阶段中位数相加冒称完整中位数。

## 3. 关联读取

两条API都恢复父对象及其pins；无ORDER BY。子statement复用，但每个父对象仍执行一次子SELECT。

| 存储 | API | pk | index | none |
| --- | --- | ---: | ---: | ---: |
| memory | sqlite | 107.434 | 107.745 | 120111.246 |
| memory | edadb | 119.146 | 119.268 | 121837.746 |
| disk | sqlite | 150.274 | 149.124 | 129745.899 |
| disk | edadb | 162.000 | 161.644 | 127462.238 |

实际子查询与check中的计划：

```sql
SELECT "pin_name", "direction", "x", "y", "component_name"
FROM "component_pins_instance_pin" WHERE "component_name" = ?;
```

- pk：SEARCH，使用主键唯一索引的component_name前缀。
- index：SEARCH，使用普通复合索引的component_name前缀。
- none：SCAN，每个父查询都扫描子表。

**约120–130秒是完整恢复10,000个父及其100,000个子对象的时间，不是一次子查询，也不是顺序读一遍子表。**

100父check用trace验证101次SELECT；主数据沿相同循环预期10,001次SELECT，正式计时不开trace。none按循环和计划估算约需10,000×100,000=1,000,000,000次候选行检查（未在正式计时中采集逐行计数），索引组定位每父10条子记录。**索引降低每次查询成本，没有消除N+1。**

本批有索引读取约107–162 ms，无索引约120–130秒；memory也慢，说明文件I/O不是该现象的必要条件。尚未拆分CPU、缓存及各函数的独立耗时。

### 波动与API差值

| schema | 存储 | SQLite读取min–max（ms） | EDADB读取min–max（ms） |
| --- | --- | ---: | ---: |
| pk | memory | 106.984–108.181 | 118.695–119.772 |
| pk | disk | 143.512–150.972 | 124.712–162.783 |
| index | memory | 106.959–109.142 | 118.359–119.577 |
| index | disk | 142.707–150.158 | 124.739–161.864 |

- 同轮配对EDADB−SQLite的memory读取差值，中位数为pk 11.680 ms、index 11.400 ms；5次均为正。它包含通用映射、NULL检查、取列及对象构造等净差，不等于某个函数的独立耗时。
- disk有约125 ms的较快EDADB样本，同轮差值范围跨过0；保留全部数据，不猜测是频率、调度或缓存导致。约12 ms的中位差不能当作稳定固定成本。
- SQLite官方对[SCAN/SEARCH](https://www.sqlite.org/eqp.html#table_and_index_scans)的定义与检查日志一致；性能数字来自本轮实测。

## 4. 补充对照

### FK检查：PRAGMA foreign_keys=OFF与ON

比较是否启用SQLite外键约束检查。两组都保留相同的FOREIGN KEY定义、父表主键及子表复合主键（schema=pk），使用相同数据和显式批量事务；只改变连接上的设置：

- **OFF组**：运行参数为`--foreign-keys off`，执行`PRAGMA foreign_keys=OFF`，关闭外键约束检查；不是删除外键列、外键定义或索引。
- **ON组**：运行参数为`--foreign-keys on`，执行`PRAGMA foreign_keys=ON`，启用外键约束检查；本例插入子记录时需验证引用的父记录存在。
- 设置在连接打开后、事务开始前执行，并读回实际值：OFF为0，ON为1。前面的主矩阵全部使用ON；这里单独增加OFF作为对照。

以下仅比较**写数据阶段**，单位ms、中位数，不含init、建表、BEGIN、COMMIT及close。输入均为合法父子关系，测的是成功写入时启用检查的净影响。

例如写入Pin的component_name='U0000001'：ON时SQLite检查父表是否存在该name，缺失则本例的即时外键约束使INSERT失败；OFF时不因缺失父记录而拒绝该INSERT，但主键唯一性和NOT NULL约束仍生效。正式计时全部先写父再写子，不使用非法数据；缺失父记录的拒绝行为在独立check中验证。

| API | 存储 | foreign_keys=OFF（关闭检查） | foreign_keys=ON（启用检查） |
| --- | --- | ---: | ---: |
| sqlite | memory | 292.387 | 363.732 |
| sqlite | disk | 346.530 | 419.516 |
| edadb | memory | 279.991 | 353.075 |
| edadb | disk | 343.153 | 414.834 |

DDL与索引不变，差值表示成功插入的FK设置净影响，不是逐函数计时。文件样本存在波动，例如SQLite disk FK OFF写入最大514.578 ms，未删除该样本。普通SELECT的读取差值不能当作逐行外键校验成本。

**这张表不比较read阶段。** 程序也运行并记录对应的读取，但普通SELECT不会因为ON而逐行重新校验父子引用；两组仍使用相同WHERE查询和对象恢复流程。完整外键检查使用独立的PRAGMA foreign_key_check，不进入正式读写计时。

实现位置：[连接配置](../pk_fk_benchmark.cpp#L175)、[write计时边界](../pk_fk_benchmark.cpp#L468)。官方依据：[启用外键检查及连接/事务边界](https://www.sqlite.org/foreignkeys.html#fk_enable)、[即时与延迟约束](https://www.sqlite.org/foreignkeys.html#fk_deferred)、[foreign_key_check](https://www.sqlite.org/pragma.html#pragma_foreign_key_check)。

### flat：SQLite两次全表读取

| schema | memory repeat | disk warm |
| --- | ---: | ---: |
| pk | 80.612 | 119.630 |
| index | 80.871 | 120.520 |
| none | 80.673 | 117.264 |

flat只消费字段，不分组恢复父子对象。none顺序读取约81–117 ms，说明无索引全表顺序读取不必慢，但不能直接与graph相减得到N+1或对象重建的独立成本。

## 5. 结论与后续

- 当前最明确的结论：child索引用写入和空间成本换取关联查找效率；PK还提供组合唯一性。
- 本轮已完成完整复测，主要趋势与此前一致；正式结论只使用本批5次样本，不混批。主要流程一致适合比较schema，但不足以把API净差全部称为框架遍历成本。
- 下一步先审阅结果，再决定独立sqlite-rowid实验。若分析EDADB细小差距，先对齐API及测试包装器；若分析批量读，需保持相同对象恢复语义。
- 父子比例扩展、cold及真正批量对象恢复未运行；不推断所有设计或所有插入顺序都具有相同效果。

## 6. 原始证据

本轮产物：[原始计时](../../../../../../../../../../../../tmp/iedadb_pk_fk_full_review_20260917_run/samples.tsv)、[统计](../../../../../../../../../../../../tmp/iedadb_pk_fk_full_review_20260917_run/summary.tsv)、[完整报告](../../../../../../../../../../../../tmp/iedadb_pk_fk_full_review_20260917_run/report.md)、[正确性与DDL/EQP](../../../../../../../../../../../../tmp/iedadb_pk_fk_full_review_20260917_run/checks.json)、[计时审计](../../../../../../../../../../../../tmp/iedadb_pk_fk_full_review_20260917_run/audit.json)、[文件库审计](../../../../../../../../../../../../tmp/iedadb_pk_fk_full_review_20260917_run/database_audit.json)、[版本与配置](../../../../../../../../../../../../tmp/iedadb_pk_fk_full_review_20260917_run/manifest.json)。

本页数字来自上述summary.tsv；55个文件库及源码快照保留在同一产物目录。原始文件不随仓库提交，链接只在对应服务器布局下有效。实际配置：memory为内存DB、journal=MEMORY、synchronous=OFF、cache_size=-8192；disk为文件DB、本机构建默认journal=DELETE、synchronous=FULL、cache_size=-2000。两组均使用显式批量数据事务；同时改变多项参数，不能把A/B差值全部归因于磁盘。此前批次保留在原产物目录，不参与本页统计。

运行命令统一见[readme](../readme.md#如何运行)。
