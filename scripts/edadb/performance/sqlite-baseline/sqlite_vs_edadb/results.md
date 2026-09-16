# SQL/API对齐实验：结果与分析

本页是**当前汇报结果**；[实验条件、组定义、SQL和源码对应](alignment.md)只在方法页维护。
首轮NULL/绑定对照保存在[归档](archive/initial_checks.md)，不是本页数据，不能跨批次相减。

## 数据口径与完成情况

- 1,000,000条8字段Component；直接SQLite与EDADB API，无adapter、Shadow或子表。
- A为内存库；B-batch为文件库OS-warm读取；完整配置见[参数说明](../sqlite_params/config.md)。
- Release -O3；正式串行5次。40组正确性检查、100条计时、20组统计通过审计。
- 下表为data中位数，单位ms；init/create/BEGIN/COMMIT/close另列，不混入读写data。
- 简写：raw=基本SQLite；checks=只加读NULL或写clear；other=只匹配其余SQL/API；aligned=全部匹配；edadb=原生框架。具体操作见方法页。

## 1. 读取结果

| 测试组（脚本名） | 读取操作概括 | A读data | B-batch读data |
| --- | --- | ---: | ---: |
| SQLite基本读取（raw） | 直接取8列，不检查NULL | 817.614 | 835.683 |
| SQLite仅加NULL检查（checks） | 基本读取＋逐列NULL检查 | 1063.591 | 1087.837 |
| SQLite仅匹配SQL/取列/收尾（other） | 匹配SELECT文本、字符串取列顺序、DONE清理；不加NULL检查 | 825.662 | 843.838 |
| SQLite全部匹配读取（aligned） | NULL检查＋SELECT/字符串顺序/DONE清理匹配 | 1079.313 | 1094.703 |
| EDADB原生API读取（edadb） | 使用EDADB reader，保留其完整框架处理 | 1073.911 | 1115.088 |

这里的读结束clear/reset只在整个SELECT结束时执行一次，**不是每条记录一次**。
手写路线的对齐SQL在计时外生成；EDADB仍在首次prepare内生成SQL，该成本保留在data中。

### 差值与结论

| 要分析的成本 | 算法 | A | B-batch |
| --- | --- | ---: | ---: |
| 基本读取增加NULL检查 | checks − raw | 245.977 | 252.153 |
| SQL/取列/收尾匹配后，再增加NULL检查 | aligned − other | 253.651 | 250.864 |
| 已有NULL检查，再匹配SQL/取列/收尾 | aligned − checks | 15.721 | 6.866 |
| Y：EDADB与全部匹配SQLite的差距 | edadb − aligned | -5.401 | 20.386 |

**结论：逐列NULL检查是读取差距的主要因素。**SQL/取列/收尾匹配的净影响较小；
Y不能称为遍历耗时。两种配置下aligned与EDADB样本范围均有重叠，
尤其不能把A的负差值解释为稳定加速。


## 2. 写入结果

| 测试组（脚本名） | 写入操作概括 | A写data | B-batch写data |
| --- | --- | ---: | ---: |
| SQLite基本写入（raw） | 手写INSERT；2次bind_text、6次bind_int64；step→reset | 1533.247 | 1608.843 |
| SQLite仅加清除绑定（checks） | 基本写入＋每行clear_bindings | 1590.237 | 1661.594 |
| SQLite仅匹配SQL/绑定API（other） | 匹配INSERT及text64/int/int64绑定，不增加clear | 1511.963 | 1593.447 |
| SQLite全部匹配写入（aligned） | 每行clear＋SQL/绑定API匹配 | 1583.346 | 1664.665 |
| EDADB原生API写入（edadb） | 使用EDADB writer，保留其完整框架处理 | 1630.426 | 1688.907 |

两边字符串绑定均使用TRANSIENT。手写对齐SQL计时外生成；EDADB自己的SQL生成仍计入data。

### 差值与结论

| 要分析的成本 | 算法 | A | B-batch |
| --- | --- | ---: | ---: |
| 原写入路径增加clear | checks − raw | 56.990 | 52.751 |
| SQL及绑定API对齐后增加clear | aligned − other | 71.384 | 71.218 |
| 已有clear，再对齐SQL及绑定API | aligned − checks | -6.891 | 3.072 |
| Y：全部对齐后剩余写入差距 | edadb − aligned | 47.080 | 24.242 |

**结论：clear是已验证的额外写入成本；其他SQL/绑定API对齐没有显示稳定的大幅增时。**
剩余Y约占EDADB写data的2.89%/1.44%，不能据此断言是哪段框架代码。
B-batch raw有1841.963ms高样本，小差值需结合min/max，而不是只看中位数。


## 3. 成本解释与证据边界

### 换算为每条记录的净成本

`每条delta（ns）= 整批delta（ms）×1,000,000 / N`；本批N=1,000,000，所以数值恰好相同。
下表是**整批中位数差的均摊**，不是每次API调用的独立计时。

| 对照因素 | A（ns/条） | B-batch（ns/条） | 每条增加的工作 |
| --- | ---: | ---: | --- |
| 读NULL：checks−raw | 245.977 | 252.153 | 8次column_type及条件判断 |
| 读NULL，已匹配：aligned−other | 253.651 | 250.864 | 同上 |
| 写clear：checks−raw | 56.990 | 52.751 | 1次clear_bindings，处理8个参数 |
| 写clear，已匹配：aligned−other | 71.384 | 71.218 | 同上 |
| 读剩余Y：edadb−aligned | -5.401 | 20.386 | 未拆分，不能当成遍历独占成本 |
| 写剩余Y：edadb−aligned | 47.080 | 24.242 | 未拆分，含固定准备成本的均摊 |

NULL净增量再除以8，约30.75–31.71ns/检查，包含调用、分支和对后续执行的影响。
没有测量CPU指令数，也不能仅靠主频把这些wall time准确换算成指令数。

### SQLite内部到底做了什么？

以下核对[SQLite 3.37.2官方源码vdbeapi.c](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbeapi.c)，不是给每个内部步骤分配猜测的时间：

- **column_type**（1130）：通过`columnMem`（1022）取得当前结果列，检查列范围；读取值类型，再经`columnMallocFailure`（1055）处理API错误状态。源码有连接mutex进入/退出路径；是否实际加锁取决于构建及连接配置，不能由此声称存在锁竞争。它不是直接在用户侧检查一个整数标志，也不重新查询DB。
- **clear_bindings**（140）：遍历statement的参数槽，调用`sqlite3VdbeMemRelease`释放需要释放的值资源，再置为NULL；有mutex路径。本例每行处理8个参数，但不代表8次实际堆释放。它改变后续重新绑定的资源生命周期，因此delta不是clear函数本体的独占计时。
- **reset**（120）：重置/回绕statement执行状态，不清空参数值。两条写入路线都有reset，额外的是clear，不是reset。[官方reset](https://www.sqlite.org/c3ref/reset.html)、[官方clear_bindings](https://www.sqlite.org/c3ref/clear_bindings.html)

这些实现说明额外工作**存在且不是零成本操作**；真正的量化证据是同条件增删NULL/clear的实验。
目前没有证据把其中多少ns划给mutex、内存释放或类型判断，不能把它们编成相加的耗时分布。

- 两行“增加NULL/clear”的差值是不同上下文下的两次对照，**不能一起加到总差距**。
- 读的SQL/取列/收尾匹配、写的SQL/绑定API匹配，分别是组合对照；差值不是某个API的独占耗时。源码更多不保证实际指令更多。
- 当前证据足以说明NULL/clear的重要性；尚未拆分setup/rows/teardown或traverser内部，暂不凭Y修改core。
- 冻结baseline、共用源码及生产API均未修改；本实验位于独立入口。


## 4. 下一步只验证尚不确定的差距

- NULL/clear已经有两种配置、两种上下文的对照，不必仅因小幅反向delta重跑全部baseline。
- 若要解释剩余Y，先拆固定prepare/收尾与逐行工作；若要解释other，分别控制SQL文本、绑定写法、取列顺序与DONE清理。
- 单因素复测交替执行顺序、增加重复次数及相同代码控制组。没有独立测量前，不把差值指认为遍历、mutex或某个API的独占成本。
- 这里只记录本实验的证据缺口，不维护跨项目TODO；主外键、adapter和N+1属于独立实验。

## 原始数据与复核

本批输出目录为`/tmp/iedadb_alignment_full`，仓库外产物不提交。以下为相对链接，在原服务器目录布局下可用：
- [samples.tsv](../../../../../../../../../../../tmp/iedadb_alignment_full/samples.tsv)：逐次阶段时间与对应原始日志。
- [summary.json](../../../../../../../../../../../tmp/iedadb_alignment_full/summary.json)、[deltas.json](../../../../../../../../../../../tmp/iedadb_alignment_full/deltas.json)：所有阶段均值/中位数/min/max和同批差值。
- [report.md](../../../../../../../../../../../tmp/iedadb_alignment_full/report.md)：可读汇总，init/create/BEGIN/COMMIT/close另列，不混入data。
- [audit.json](../../../../../../../../../../../tmp/iedadb_alignment_full/audit.json)、[checks.json](../../../../../../../../../../../tmp/iedadb_alignment_full/checks.json)：校验、SQL次数、统计与日志/哈希核对。
- [manifest.json](../../../../../../../../../../../tmp/iedadb_alignment_full/manifest.json)、[configs.json](../../../../../../../../../../../tmp/iedadb_alignment_full/configs.json)、[aligned.sql.txt](../../../../../../../../../../../tmp/iedadb_alignment_full/aligned.sql.txt)：源码/二进制哈希、硬件、实际配置和SQL。新增未提交代码由快照哈希标识，不能只用父仓库HEAD代表本次程序。
