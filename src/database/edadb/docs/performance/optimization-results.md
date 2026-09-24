# 原始iEDA与EDADB存取性能对比

本页保留流式Shadow修改前、Demo版本的事务优化结果。后续新增修改及同批前后对照见[流式Shadow实验](stream-shadow.md)，不将新旧批次混为一个实验。

## 1. 比较版本与测试方法

| 版本 | 源码标识 | 本轮用途 |
| --- | --- | --- |
| 原始iEDA，无EDADB集成 | `0074352412f6a4a8c88c13739946cdf5004f25c0` | 原生DEF基线 |
| 未优化iEDA+EDADB，已有测量 | iEDA `62504bd9d`；core `be6bbdd` | 未加父键索引及事务合并；Release，profiling与SQL trace关闭 |
| 当前iEDA+EDADB | iEDA `4ba761281`＋工作区修改；core `1c4857c` | 同一程序分别执行DEF文本路径和EDADB数据库路径 |

当前EDADB增加了**子表父键索引、全部建表共用一次事务、全部design数据写入共用一次事务**；另移除了参考DEF扫描。不包含后续流式Shadow及叶子批量读取。

原始iEDA取自EDADB项目初始化提交`638335686`之前，源码保持干净，不回移集成后的DEF/iDB修改。当前两仓库仍在`edadb-performance-optimization-dev`，没有切换或修改生产代码。未优化EDADB采用已有测量的准确提交；**它不是配对milestone `d1e3a2125/d6656f0`的实测值**，不以其他提交冒充milestone。

- 输入：Sky130 GCD [iPL_filler_result.def](../../../../../scripts/design/sky130_gcd/result/iPL_filler_result.def)，**726,155 B**。不是单表合成数据。
- 两程序均使用GCC 10、Release `-O3`、LTO；原始版本以`-j40`重新构建，构建结束后才测试。
- 使用同一[benchmark.tcl](../../../../../scripts/edadb/performance/benchmark.tcl)：命令前后读取`clock microseconds`，换算为ms。LEF加载、写入前准备iDB、读取后输出DEF校验均在计时外。
- **本轮仅热缓存**：每次进程前预读输入，读库前预读DB。固定CPU 2，串行执行；每个版本样本前等待5秒。两版本交替先后顺序，预热1次、正式5次。
- 正式总时间关闭新增C++阶段计时器（`EDADB_STAGE_TIMING=0`，OFF）；另跑相同优化二进制的阶段观测组（`=1`，ON）。这不是优化开关，原生DEF一直使用Tcl计时。
- 审计通过：6次跨版本原生DEF严格相等、12次EDADB恢复DEF严格相等、12个数据库完整性及FK检查。正式30条命令记录、70条阶段及派生记录。

## 2. 总表：先读后写

**单位ms，5次中位数，阶段计时OFF。** 横向比较完整命令，不把建表混入后面的data阶段。

各列的执行路线：

- **原始iEDA：DEF**：接入EDADB前的`007435241`程序；读执行`def_init`（DEF文本→iDB），写执行`def_save`（iDB→DEF文本）。这是正式比较基线。
- **未优化EDADB**：上述`62504bd9d/be6bbdd`程序执行`edadb_read/edadb_write`；取已有热缓存5次中位数。读取仍含参考DEF扫描，写入包含初始化、建表及分组数据事务。
- **当前程序：DEF**：当前iEDA+EDADB程序，仍执行`def_init/def_save`，**不调用EDADB读写API、不读写SQLite数据库**。仅用于检查集成前后的原生DEF路径耗时是否接近，不替代原始基线。
- **当前程序：EDADB**：与上一列同一二进制，改为调用`edadb_read`（DB→adapter恢复iDB）或`edadb_write`（iDB→adapter转换→DB）。完整读取含初始化与对象恢复；完整写入含初始化、建表、数据写入及提交。

每个读、写命令分别计时，均排除LEF加载。**比较原始DEF、未优化EDADB与当前EDADB；当前程序的DEF路径仅作辅助核查。**

| 完整命令 | 原始iEDA：DEF | 未优化EDADB（已有批次） | 当前程序：EDADB（本轮） | 当前EDADB耗时/原始DEF耗时 |
| --- | ---: | ---: | ---: | ---: |
| 读取 | **47.484** | **11931.378** | **106.132** | **2.24倍** |
| 写入 | **6.855** | **2277.177** | **371.532** | **54.20倍** |

### 优化前后：已有数据的比较

| 完整命令 | 当前本轮相对未优化已有批次 | 已有同批对照：未优化→当前实现 | 同批收益 |
| --- | --- | --- | --- |
| 读取 | 耗时减少99.11%，约112.42倍加速 | 11931.378→108.873 ms | 耗时减少99.09%，约109.59倍加速 |
| 写入 | 耗时减少83.68%，约6.13倍加速 | 2277.177→419.340 ms | 耗时减少81.59%，约5.43倍加速 |

两批使用相同输入、Tcl计时边界、Release配置、CPU绑定和热缓存方法；优化二进制SHA256也相同。**左列是跨批次现有数值比较，右列是已有同批交替对照，判断优化收益优先看右列。** 当前写入371.532与419.340 ms的区别不是新增代码收益；具体波动原因未定位。收益包含全部修改及移除参考DEF扫描，不能拆作各项修改的独占贡献。

未优化数据来源：[原始计时](../../../../../scripts/edadb/performance/baseline-verification/samples.tsv)、[统计](../../../../../scripts/edadb/performance/baseline-verification/summary.tsv)（`baseline/warm`）、[版本与方法核查](baseline-verification.md)。**没有对应的未优化init/create/data实测值，因此不列其阶段分解，也不推算。**

### 原生路径辅助核查及波动

- 当前集成程序的DEF路径：读取47.274 ms、写入6.884 ms，不走EDADB；仅作辅助核查。

- 原始DEF读取范围47.455–47.904 ms；写入6.845–7.631 ms。
- EDADB读取范围101.143–113.768 ms；写入355.422–492.735 ms，写入波动明显，不能将某次差值直接归因于具体函数。
- 原始与集成程序的DEF中位数相差：读约−0.44%，写约+0.42%。本轮未出现历史约71 ms的原生读时间，也没有用集成程序代替原始基线。
- **结论：当前EDADB仍慢于原始DEF。** 完整数据库写入包含建表和事务提交，原生DEF不承担同样的数据库工作；这是真实应用操作差异，不是“纯格式转换”的差距。

## 3. 读取：内部时间在哪里

以下为**ON观测组5次均值**，百分比以该组完整Tcl命令均值为分母。只在同一组内部拆分，不能从上表OFF中位数减去这些值。

| 读取阶段 | 包含工作 | 均值ms | 占完整读取 |
| --- | --- | ---: | ---: |
| init | helper、连接配置、表映射 | 0.433 | 0.41% |
| data | 查询、恢复对象图、组装iDB | **102.967** | **97.19%** |
| Session内其他 | 未归入init/data的作用域内工作 | 0.000365 | <0.01% |
| Session外差额 | 外层builder工作、命令返回及计时报表等 | 2.544 | 2.40% |
| **完整Tcl读取** | 上述互斥部分合计 | **105.944** | **100%** |

读取不建表，也不再扫描参考DEF。data排除init，但**包含对象恢复**，并非只测SQLite查询。原生DEF未额外拆分init/data，因此其47.484 ms仍只标记为完整命令，不冒称为纯解析时间。

**结论：当前读开销主要位于查询＋对象恢复及组装的合计阶段，不在连接初始化。** 本轮不能进一步给出SQLite、Shadow、对象分配分别耗时多少。

## 4. 写入：内部时间在哪里

同样使用ON组5次均值；建表独立，数据事务仍计入data。

| 写入阶段 | 包含工作 | 均值ms | 占完整写入 |
| --- | --- | ---: | ---: |
| init | 连接配置、主键映射 | 0.181 | 0.05% |
| create | 建表、建索引、建表事务提交 | **89.754** | **24.29%** |
| data | 数据BEGIN、Shadow转换、遍历/INSERT、COMMIT、临时对象清理 | **279.471** | **75.64%** |
| Session内其他 | 未归入上述阶段的作用域内工作 | 0.000636 | <0.01% |
| Session外差额 | 命令外围及计时报表等 | 0.088 | 0.02% |
| **完整Tcl写入** | 上述互斥部分合计 | **369.493** | **100%** |

**data不含init/create；data中的BEGIN/COMMIT未单独计时。** 转换、bind/step/reset/finalize与提交仍是一个合计阶段，不能把279.471 ms全部称为SQLite执行或磁盘同步时间。

**结论：写入主要成本在数据阶段，其次是建表；只去掉建表不能解释或消除与原生DEF的全部差距。** 下一步若继续定位，应在保持语义不变的前提下拆分数据事务提交与转换/存取，而不是根据总差值猜测。

## 5. 优化快在哪里：整体与每一步

**整体结论：读主要受益于子表父键索引，写主要受益于两类事务合并。** 已有同批完整命令对照：读11931.378→108.873 ms，减少99.09%；写2277.177→419.340 ms，减少81.59%。最新复测106.132/371.532 ms见总表，不与历史各步拼接成同一次实验。

### 5.1 读取：避免反复扫描子表

- **改了什么**：为缺少对应索引的子表建立完整parent-FK复合索引。FK约束与查找索引不是同一件事；这里加的是查找路径，不是取消FK检查。
- **少做了什么**：Net的Point（14,256行）、ViaRef（3,716行）原先按父键查找时执行全表SCAN；建立索引后改为SEARCH，只定位匹配父对象的记录。原来反复检查不相关行的工作大幅减少。
- **没有少做什么**：仍逐父对象发起查询，查询次数没有因此减少；取字段、恢复对象、组装iDB仍需执行。本次不是批量读取或N+1查询次数优化。

历史逐版本实验的完整读取，热缓存5次中位数，单位ms：

| 新增修改 | 前→后 | 观测变化 | 如何解释 |
| --- | ---: | --- | --- |
| 子表父键索引 | 19164.606→169.480 | 减少99.12%，约113.08倍加速 | 结合SCAN→SEARCH证据，支持其为主要读优化 |
| 全部建表共用事务 | 169.480→167.552 | 减少1.14% | 不修改读路径，不能称为确定的读收益 |
| 全部design写入共用事务 | 167.552→172.396 | 增加2.89% | 不修改读路径，属于回归观测，不说明事务合并使读必然变慢 |

这些是历史相邻版本结果，不是当前分支新增的单项隔离测试；历史完整读取还含参考DEF扫描。因此不将169.480→本轮106.132 ms全部解释成事务优化收益，也不推算扫描本身用了多少时间。

### 5.2 写入：减少重复事务提交，不减少INSERT条数

历史相邻版本的完整写入，热缓存5次中位数，单位ms：

| 新增修改 | 前→后 | 观测变化 | 增加或减少的工作 |
| --- | ---: | --- | --- |
| 子表父键索引 | 2011.603→2224.625 | 增加213.022 ms，约10.59% | 额外建索引，并在插入时维护索引；以写入和空间成本换读取收益 |
| 全部建表共用事务 | 2224.625→1131.619 | 减少1093.006 ms，约49.13% | 本用例15个schema事务合成1个，减少14次提交边界 |
| 全部design写入共用事务 | 1131.619→378.269 | 减少753.350 ms，约66.57% | 本用例10个非空root写事务合成1个，减少9次提交边界 |

**batch create**仍逐条执行相同的CREATE TABLE/INDEX，只在外层统一BEGIN/COMMIT。**batch commit write**仍逐条插入相同数据，只将各root的提交合并；它不是多行`INSERT VALUES(...),(...)`，也不是减少字符串绑定次数。

减少提交边界避免了重复的事务收尾，以及这些提交所需的日志处理、页写出和同步工作；**本实验没有单独统计sync次数或耗时，不能把全部净收益称为sync时间，也不是每次COMMIT必定对应一次sync。** 数据提交仍保留，不是关闭持久化。索引造成的213.022 ms增量也包括缓存、页数及运行波动影响，不是单独测得的索引函数耗时。

### 5.3 当前实现的阶段证据：建表与写数据分别下降

下面使用已有粗粒度计时的两批**ON/warm阶段中位数**：仅有父键索引→再合并两类事务；两版均已移除参考DEF扫描。不是未优化版本的阶段数据，也不是本轮OFF总时间的分解。

| 阶段 | 仅父键索引ms | 再合并两类事务ms | 观测变化 |
| --- | ---: | ---: | --- |
| 读init | 0.421 | 0.427 | 约+0.006 ms |
| 读data：查询与对象恢复 | 106.392 | 101.324 | 约−4.76%；未修改读路径，不归为确定收益 |
| 写init | 0.173 | 0.179 | 约+0.006 ms |
| 写create：建表、索引、建表提交 | 982.339 | 89.526 | **减少892.813 ms，约90.89%** |
| 写data：转换、INSERT、数据提交、清理 | 797.298 | 259.063 | **减少538.236 ms，约67.51%** |

该分布支持“建表事务合并降低create、design事务合并降低write data”的解释，但两项修改一起启用，不能视为当前代码的逐项隔离测量。BEGIN、COMMIT、转换、SQLite API的各自毫秒数尚未拆出，不编造分项。

依据：[历史相邻版本及源码修改](optimization-history.md#5-每个p增加了什么收益依据是什么)、[历史原始计时](../../../../../scripts/edadb/performance/p_optimization_samples.tsv)、[仅父键索引阶段统计](../../../../../scripts/edadb/performance/p1-stage-timing/summary.tsv)、[事务合并后阶段统计](../../../../../scripts/edadb/performance/p3-p4/summary.tsv)。阶段变化用未舍入值计算，不将不同批次、不同统计口径的节省毫秒数相加。

## 6. 计时范围与扰动

```text
Tcl命令开始
    C++ Session开始
        init作用域：进入记时，离开自动累计
        create作用域：仅写入时，单独累计
        data作用域：进入记时，离开自动累计
    Session结束，计算other，再输出统计
Tcl命令结束
```

- `Session command = init + create + data + other`；逐样本验证相等。Session外差额为同次`Tcl总时间 − Session总时间`，是派生余量，不是独立函数计时。
- 实现采用`steady_clock`与RAII，仅阶段边界取时，不逐行计时；正确性检查和统计分析在性能计时外。
- ON完整命令中位数：读106.614 ms，写370.984 ms；相对OFF为+0.45%、−0.15%。未观察到明显总时间增加，**不代表计时绝对零开销**；写入波动及组间环境差异仍存在。
- 明细使用均值便于加和；总对比使用中位数降低极端样本影响。所有值均保留原始单位，展示值四舍五入。
- 位置：[Session/ScopedTimer](../../idb/edadb_stage_timing.h)、[连接与建表](../../idb/edadb_idb_init.cpp)、[读取](../../../manager/builder/def_builder/def_read_edadb.cpp)、[写入](../../../manager/builder/def_builder/def_write_edadb.cpp)。详细边界见[计时说明](stage-timing.md)。

## 7. 原始数据与复核入口

- [运行脚本](../../../../../scripts/edadb/performance/native-baseline/run.sh)、[统计与审计脚本](../../../../../scripts/edadb/performance/native-baseline/summarize.py)。
- [命令原始计时](../../../../../scripts/edadb/performance/native-baseline/samples.tsv)、[阶段原始计时](../../../../../scripts/edadb/performance/native-baseline/stages.tsv)、[统计汇总](../../../../../scripts/edadb/performance/native-baseline/summary.tsv)、[审计结果](../../../../../scripts/edadb/performance/native-baseline/audit.json)、[二进制/输入/Tcl哈希](../../../../../scripts/edadb/performance/native-baseline/manifest.sha256)。
- 本机完整日志与DB：`/tmp/iedadb_native_original_validation`；小型TSV及审计文件已留存在仓库，临时二进制与DB未纳入。
- 原始程序：`/tmp/ieda_native_007435241_bin/iEDA`；当前程序：仓库内`bin-release/iEDA`。源码HEAD不足以标识当前未提交修改，实际被测二进制以manifest SHA256为准。

复跑需先构建对应版本，选择不存在的输出目录；本脚本仅测试热缓存，统计脚本要求正式5次：

```bash
OUT_DIR=/tmp/iedadb_native_repeat PERF_RUNS=5 \
  bash scripts/edadb/performance/native-baseline/run.sh \
  /tmp/ieda_native_007435241_bin/iEDA bin-release/iEDA \
  scripts/design/sky130_gcd/result/iPL_filler_result.def
python3 scripts/edadb/performance/native-baseline/summarize.py /tmp/iedadb_native_repeat
```
