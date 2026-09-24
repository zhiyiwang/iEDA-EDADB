# Adapter粗粒度分段计时（从P1开始）

当前工作区已增加[P3/P4事务合并](transaction-batching.md)。本文源码位置对应当前实现；[已测P1结果](parent-index-results.md)仍仅代表合并前版本。

## 版本与范围

- iEDA与core均使用`edadb-performance-optimization-dev`分支，分别从P1 `4ba761281`、`1c4857c`创建。
- P1是历史iEDA `edadb-performance-optimization`、core `performance/optimization`分支上的祖先提交；新分支从这个历史点分出，当前仅移植P3/P4，不包含P5/P2。
- 本轮增加观测，并删除已停用的参考DEF扫描；core源码不改，gitlink仍是`1c4857c`。当前已按P3/P4分别合并schema事务和design数据事务。
- 原细粒度`EDADB_ENABLE_PROFILING`和SQL trace保持OFF。新环境变量`EDADB_STAGE_TIMING=1`开启分段，默认关闭。

## 先区分：P1优化改了什么，本轮计时又改了什么？

### 已提交的P1：只增加child父键索引

P1 core提交`1c4857c`修改两个生产头文件：

1. [SqlStatement4Sqlite.h:114](../../core/include/edadb/backend/sqlite/SqlStatement4Sqlite.h#L114)：新增`createParentForeignKeyIndexStatement()`。当parent FK有效且`hasPrimKey == false`时，收集完整祖先FK列，生成普通复合索引；其他情况返回空串。真实判断代码：

```cpp
if (!tableDef.parentFkc()->valid() || Cpp2SqlTypeTrait<T>::hasPrimKey) {
    return {};
}
```

2. [DbTableOpCreate4Sqlite.h:120](../../core/include/edadb/backend/sqlite/DbTableOpCreate4Sqlite.h#L120)：原来建表成功即返回，现在继续生成并执行该索引SQL，失败返回`-1`。真实新增代码：

```cpp
const std::string index_sql =
    SqlStatement<DbTableDefT>::createParentForeignKeyIndexStatement(*typed_table_def);
if (!index_sql.empty() && !DbManager::i().exec(index_sql)) {
    std::cerr << "DbTableOperatorImpl::createOneTable: create parent-FK index failed for table "
              << tableDef->getTableName() << std::endl;
    return -1;
}
```

因此P1是`CREATE TABLE → 额外CREATE INDEX`，没有改SELECT文本、事务边界、Shadow转换或对象遍历。新增索引的创建时间属于本轮`create`，后续INSERT维护索引的代价仍属于`write`。完整P1 diff见[optimization-code-changes.md](optimization-code-changes.md)。

### 本轮新增：adapter分段计时及移除参考扫描

本轮未提交的改动是：新增`edadb_stage_timing.h`，修改3个adapter源文件及读取头文件；core保持P1内容。计时代码本身不改变SQL或对象转换；P3/P4事务修改另见专项说明。另删除已注释的参考DEF调用及废弃createDbByDef实现；保留旧path参数兼容调用方，但不读取该文件。

| 修改位置 | 原来 | 现在 |
| --- | --- | --- |
| initWriteDb | 连接、初始化、建表连续执行 | 原顺序不变；连接初始化与建表分别包计时作用域 |
| writeDb2Edadb | 初始化后调用writer | 增加command Session；初始化后的writer分派包Data计时 |
| createDbFromEdadb | 初始化→恢复对象→参考DEF扫描 | 仅保留Init、Data计时，不再扫描参考DEF |

## 每个时间点具体放在哪里？

以下行号已按本轮本地源码核对。起点是构造`ScopedTimer`时的`Clock::now()`，终点是离开作用域、析构该对象时的`Clock::now()`；表中结束行表示触发析构的位置，不是新增了一行显式取时语句。

### 写入时间点

| 时间段 | 开始位置 | 结束位置 | 中间执行的代码 |
| --- | --- | --- | --- |
| write command | [def_write_edadb.cpp:52](../../../manager/builder/def_builder/def_write_edadb.cpp#L52)创建Session | 函数返回时销毁Session | 初始化、建表、全部writer及控制逻辑 |
| write init，W1−W0 | [edadb_idb_init.cpp:124](../../idb/edadb_idb_init.cpp#L124) | 第131行结束花括号 | initDatabase、initPrimKeys |
| create，W3−W2 | [edadb_idb_init.cpp:134](../../idb/edadb_idb_init.cpp#L134) | initWriteDb第156行return退出作用域 | initAllTables(true, false)，包括建表/索引及schema事务 |
| write，W5−W4 | [def_write_edadb.cpp:71](../../../manager/builder/def_builder/def_write_edadb.cpp#L71) | 第128行结束花括号 | switch调用writeChip2Edadb等writer，包含数据事务和转换/清理 |

成功路径示意，W编号用于解释，并非代码里的变量名：

```text
W0 → [连接/配置 + 主键映射初始化] → W1     init = W1-W0
W2 → [表定义 + 建表/索引 + schema事务] → W3 create = W3-W2
W4 → [Shadow转换 + 插入 + 数据事务 + 清理] → W5 write = W5-W4
```

**write不是W5−W0。** 它只测最后一段，因此不包含init/create；数据BEGIN/COMMIT已经位于这段内部，无需再加一次。

### 读取时间点

| 时间段 | 开始位置 | 结束位置 | 中间执行的代码 |
| --- | --- | --- | --- |
| read command | [def_read_edadb.cpp:52](../../../manager/builder/def_builder/def_read_edadb.cpp#L52)创建Session | 函数返回时销毁Session | 初始化、对象恢复及控制逻辑 |
| read init，R1−R0 | [def_read_edadb.cpp:61](../../../manager/builder/def_builder/def_read_edadb.cpp#L61) | 第77行结束花括号 | helper设置、initReadDb连接及表映射 |
| read，R3−R2 | [def_read_edadb.cpp:81](../../../manager/builder/def_builder/def_read_edadb.cpp#L81) | 第86行结束花括号 | createDbByEdadb：SQL读取、Shadow恢复、iDB分配与组装 |

```text
R0 → [helper + 连接/配置 + 表映射] → R1     init = R1-R0
R2 → [读取全部root并转换、组装iDB] → R3    read = R3-R2
```

**read不是R3−R0。** 只统计对象读取与恢复；不将转换/组装拆出来扣除，读取路径已无参考DEF扫描。当前没有单独的adapter_convert/rebuild数值，不能从该结果声称这些操作耗时多少。

## 实现原理：进入记录起点，退出自动累计

**在待测代码块开头创建计时对象作为桩点；离开代码块时，C++自动调用析构函数，计算并累计耗时。** 这就是RAII作用域计时，不需要手动调用停止函数。

示意（省略开关判断）：

```cpp
{
    ScopedTimer timer(Phase::Data); // 构造：start_ = Clock::now()
    // 原有读或写代码保持不变
} // 自动析构：elapsed_ns[Data] += now() - start_
```

- **阶段起点**：[ScopedTimer构造函数:70](../../idb/edadb_stage_timing.h#L70)保存单调时钟起点。
- **阶段终点**：[析构函数:76](../../idb/edadb_stage_timing.h#L76)计算结束减开始，转换成ns并累加到对应阶段。正常结束、提前return或正常异常栈展开都会触发；进程强制终止不保证触发。
- **命令总计时**：[Session构造:26](../../idb/edadb_stage_timing.h#L26)检查开关、清空本线程累计值并记录总起点；[析构:36](../../idb/edadb_stage_timing.h#L36)计算command，再统一打印结果。Session是本实验的计时对象，不是SQLite事务。
- **桩点位置**：写Session在[def_write_edadb.cpp:52](../../../manager/builder/def_builder/def_write_edadb.cpp#L52)，读Session在[def_read_edadb.cpp:52](../../../manager/builder/def_builder/def_read_edadb.cpp#L52)；各阶段位置见上方两张表。

```text
Session构造 → 阶段构造 → 原有代码 → 阶段析构累计 → 下一阶段 → Session析构汇总
              记录起点              自动计算差值               结束总计时后打印
```

仅在`EDADB_STAGE_TIMING=1`时启用。当前成功写命令有3个阶段，共8次取时；读命令有2个阶段，共6次取时（总计时2次、阶段各2次），不逐行计时。阶段不能重叠；失败路径虽有记录，但不纳入性能统计。

使用`steady_clock`测经过时间，包含CPU执行、等待和阶段内I/O。ns是输出单位，不代表纳秒精度；开启计时仍有扰动，须通过ON/OFF对照验证。

### 输出中的每一行是什么意思？

格式：`EDADB_STAGE <操作> <阶段> ns <数值>`，字段以TAB分隔。

| 操作列 | 阶段列 | 数值来源及含义 |
| --- | --- | --- |
| write | init / create / write | 对应Init / Create / Data作用域累计时间；write含数据提交及adapter转换 |
| read | init / read | 对应Init / Data作用域累计时间；read含对象组装 |
| read | create | 0，读取不建表 |
| write或read | command | S1−S0，C++ adapter入口总时间，包含各阶段但不含本次报告打印 |
| write或read | other | command减去三个阶段累计值，表示尚未单列的边界/控制开销，不是纯adapter开销 |

例如`EDADB_STAGE write write ns <数值>`中，第一个write表示“写命令”，第二个write表示“排除init/create后的数据写入阶段”，不是重复计时。

**加总只选一层：** `init + create + write/read + other = command`。不能再把command加进去，也不能把已含在write中的数据COMMIT再加一次。这一等式由other的差值定义保证，本身不能证明所有阶段划分正确；边界还需结合源码与功能测试核对。

当前没有独立的`begin_ms`、`commit_ms`、`adapter_convert_ms`或`adapter_rebuild_ms`：它们按约定包含在相关阶段内。Session的report不是这些内部操作的计时点。

Tcl的[time_command:18](../../../../../scripts/edadb/performance/benchmark.tcl#L18)计时更外层，包含报告打印；C++ command不含打印，二者边界不同。ns转换为ms需除以1,000,000。

## 计时口径汇总

| 操作 | 字段 | 包含 | 不包含 |
| --- | --- | --- | --- |
| write | init | initDatabase连接及配置、initPrimKeys | 建表、写数据 |
| write | create | initAllTables(true, false)：表定义构造、CREATE TABLE/INDEX、统一schema事务BEGIN/COMMIT | 数据写入 |
| write | write | 全部root转换、SQL准备及插入、统一design事务BEGIN/COMMIT、临时Shadow释放 | init/create |
| read | init | helper设置、initReadDb：连接配置、表定义映射 | 对象读取与组装 |
| read | read | createDbByEdadb：查询准备、readNext、Shadow转换、iDB对象分配及组装、临时对象清理 | init |

读取create为0；reference_scan已从计时枚举及输出删除。LEF解析仍由Tcl在命令计时前完成。数据BEGIN/COMMIT已包含在write，当前不另拆，以免复杂化；转换/组装与存取混合统计，不宣称这是纯SQLite时间。

`command = init + create + write/read + other`。阶段作用域互不嵌套；other包括阶段外控制逻辑与计时衔接开销，不是“尚未找到的SQLite成本”。close沿用原有生命周期，不移动关闭连接来改变执行顺序，本轮没有独立close时间。

## 修改文件与验证状态


这是相对P1提交的工作区修改，不是历史P1索引补丁；目前未commit/push。以下行号对应当前工作区源码，链接使用相对路径；删除项以函数名标识，不为已删除代码编造当前行号。

| 文件 | 状态 | 本轮具体变化 |
| --- | --- | --- |
| [edadb_stage_timing.h](../../idb/edadb_stage_timing.h) | 新增生产辅助头文件 | [12–20行](../../idb/edadb_stage_timing.h#L12)：阶段及累计状态；[26–49行](../../idb/edadb_stage_timing.h#L26)：Session开关、总计时和汇总；[56–59行](../../idb/edadb_stage_timing.h#L56)：输出；[70–80行](../../idb/edadb_stage_timing.h#L70)：阶段起止及自动累计 |
| [edadb_idb_init.cpp](../../idb/edadb_idb_init.cpp#L120) | 修改生产代码 | [124行](../../idb/edadb_idb_init.cpp#L124)：Init计时，131行结束；[134行](../../idb/edadb_idb_init.cpp#L134)：Create计时，156行return时结束；原root处理顺序不变，现由P3统一事务 |
| [def_write_edadb.cpp](../../../manager/builder/def_builder/def_write_edadb.cpp#L51) | 修改生产代码 | [52行](../../../manager/builder/def_builder/def_write_edadb.cpp#L52)：write Session；[71–128行](../../../manager/builder/def_builder/def_write_edadb.cpp#L71)：Data作用域，包含转换、数据事务与清理 |
| [def_read_edadb.cpp](../../../manager/builder/def_builder/def_read_edadb.cpp#L50) | 修改生产代码 | [52行](../../../manager/builder/def_builder/def_read_edadb.cpp#L52)：read Session；[61–77行](../../../manager/builder/def_builder/def_read_edadb.cpp#L61)：Init；[81–86行](../../../manager/builder/def_builder/def_read_edadb.cpp#L81)：Data；删除createDbByDef调用及整个实现 |
| [def_read_edadb.h](../../../manager/builder/def_builder/def_read_edadb.h) | 修改接口声明 | [21–28行](../../../manager/builder/def_builder/def_read_edadb.h#L21)：说明旧path仅兼容、不读取；删除createDbByDef声明 |
| [test_stage_timing.cpp](../../../../../scripts/edadb/performance/p1-stage-timing/test_stage_timing.cpp) | 新增测试 | [20–27行](../../../../../scripts/edadb/performance/p1-stage-timing/test_stage_timing.cpp#L20)：默认关闭；[29–35行](../../../../../scripts/edadb/performance/p1-stage-timing/test_stage_timing.cpp#L29)：跨命令重置、空阶段；[37–46行](../../../../../scripts/edadb/performance/p1-stage-timing/test_stage_timing.cpp#L37)：提前返回清理 |
| [统计脚本](../../../../../scripts/edadb/performance/p1-stage-timing/summarize.py)及[parent-index-results.md](parent-index-results.md) | 新增统计与结果 | 审计OFF/ON日志，输出原始样本、均值/中位数/范围及阶段占比 |
| [performance/readme.md](readme.md)与本文 | 新增正式文档 | 集中记录当前计时修改、边界、源码位置、运行方法及验证缺口 |
| [optimization-code-changes.md](optimization-code-changes.md) | 整理并移动文档 | 说明历史各P的代码差异，不把历史修改当成本轮新增 |
| [optimization-history.md](optimization-history.md) | 整理并移动文档 | 汇总历史实验与上轮P1复测，不含本轮新增计时结果 |
| [原始样本TSV](../../../../../scripts/edadb/performance/p_optimization_samples.tsv) | 新增汇总文件 | 110条已有计时记录，保留原始批次与来源，不是本轮新测数据 |
| [集成文档入口](../README.md)、[脚本目录入口](../../../../../scripts/edadb/performance/README.md) | 修改导航 | 指向唯一的集成性能文档目录 |
| [旧代码说明入口](../../../../../scripts/edadb/performance/optimization-code-changes.md)、[旧结果入口](../../../../../scripts/edadb/performance/optimization-history.md)、[旧分段说明入口](../../../../../scripts/edadb/performance/p1-stage-timing/readme.md) | 仅留导航 | 正文已移动，不保留重复的设计说明 |
| [.gitignore](../../../../../.gitignore) | 修改仓库配置 | [38–44行](../../../../../.gitignore#L38)：放行说明、测试源码和小型样本汇总，不放行构建产物 |

**EDADB core工作区没有源码修改。** 两仓库均在`edadb-performance-optimization-dev`，父仓库gitlink仍为P1 core `1c4857c`。`build-release/`等构建目录不属于应提交的源码修改。

以下验证表为P3/P4合并前的P1记录，不代表新修改已通过同一轮完整验收。

| 验证项 | P1结果 | 能证明什么 |
| --- | --- | --- |
| Release def_builder及db_edadb依赖构建 | 删除参考扫描后通过 | 当前adapter源码可编译 |
| 独立计时器测试、分项加总检查 | 通过 | 基础累计和状态清理符合测试预期 |
| 文档本地链接、git diff空白检查 | 通过 | 文档路径可用，diff无新增空白错误 |
| 完整iEDA可执行程序重新链接 | 通过 | 本次性能使用新Release二进制，哈希见结果文档 |
| 功能回归 | core 27/27、诊断回归15/15；filler 24次严格diff通过 | 不存在参考DEF的恢复也通过严格diff |
| ON/OFF计时扰动与正式性能统计 | 完成20个正式样本及5对冷写补充 | 已获得分段数据；写入噪声明显，未证明扰动稳定小于5% |

**计时边界限制：** 本轮测adapter调用EDADB完成存取的整体阶段，不是core独占时间；不能据此拆出SQLite、遍历或对象组装各自耗时。绕过上述Session入口的独立调用不会自动启用计时。

历史P1命令时间包含参考DEF扫描，当前命令不包含；新旧总时间不可直接归因为优化收益。本次已重新链接并完成验证，具体口径及结果见[parent-index-results.md](parent-index-results.md)。

## 如何运行

**OFF/ON只控制我们新增的阶段计时器**：OFF（`EDADB_STAGE_TIMING=0`）不取C++阶段时间，仍由Tcl测完整命令；ON（`=1`）额外记录分段。两组使用同一个Release二进制，优化和SQLite配置不随此开关变化。详见[开关及cold/warm定义](transaction-batching.md#off--on是什么意思)。

从仓库根目录，先构建Release并关闭旧profiling与trace。计时关闭/开启应使用同一构建，性能样本串行，不与编译并发：

```bash
EDADB_STAGE_TIMING=1 PERF_WARMUPS=1 PERF_RUNS=5 PERF_SETTLE_SECONDS=5 \
  IEDA_BIN="$PWD/bin-release/iEDA" OUT_DIR=/tmp/iedadb_p1_stage_on \
  bash scripts/edadb/performance/run.sh \
    scripts/design/sky130_gcd/result/iPL_filler_result.def
```

OFF对照将变量设为`EDADB_STAGE_TIMING=0`并改用独立OUT_DIR。本次已比较ON/OFF及冷写交替样本；写入波动超出拟定5%门槛，不能宣称零扰动或稳定达标。

分段记录在每个样本的`write.log`和`read.log`中：

```text
EDADB_STAGE<TAB>write/read<TAB>phase<TAB>ns<TAB>elapsed
```

现有`result.tsv`仍只有Tcl完整命令时间，没有自动变成分段结果。可先使用`grep '^EDADB_STAGE' /tmp/iedadb_p1_stage_on/warm/run-1/write.log`查看。比较原生iEDA时，主要使用含adapter转换/组装及数据提交的write/read，不能拿只计数据库的局部过程替代。

## 最小计时器验证

上述完整验证对应P3/P4合并前的P1计时版本；P3/P4当前验证状态见[事务合并说明](transaction-batching.md)。

[test_stage_timing.cpp](../../../../../scripts/edadb/performance/p1-stage-timing/test_stage_timing.cpp)验证默认关闭、两个命令状态重置、未执行阶段为0及提前返回清理；它不是性能或DEF正确性结果。

```bash
g++-10 -std=c++17 -O3 -Wall -Wextra -Werror -pthread \
  -Isrc/database/edadb/idb \
  scripts/edadb/performance/p1-stage-timing/test_stage_timing.cpp \
  -o /tmp/iedadb_test_stage_timing
/tmp/iedadb_test_stage_timing
```
