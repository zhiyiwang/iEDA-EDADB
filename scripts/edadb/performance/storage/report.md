# 文本、SQLite与EDADB存取性能：实验汇报

本报告汇总四组已完成并冻结的实验，供会议讨论。各组详细结果仍是权威记录；本文不引入新测量、不替换原结论。计时默认单位为**毫秒（ms）**，1,000 ms = 1秒；各表使用5次正式样本的中位数。

项目位置：`/home/zhiyiwang/cs/arch/eda/iEDA-EDADB`。
实验根目录：`scripts/edadb/performance/storage/`；本文为该目录的`report.md`。
冻结内容归档于`prof-test`，最新提交`7ebde2b94`；不同实验的实际测量版本以各自manifest/源码快照为准，不表示所有实验都由该提交同时运行。

## 1. 四组实验的关系

```text
baseline：不同存取路线总体需要多少时间？
  ├─ sqlite-vs-edadb：同样8字段，框架比直接SQLite多做什么？
  ├─ sqlite-pk-fk：有父子关系时，索引、FK检查和逐父查询影响多大？
  └─ sqlite-index：去掉父子关系，单独比较主键、额外索引、rowid布局。
```

| 实验 | 数据与变量 | 已完成验收 |
| --- | --- | --- |
| [五路线baseline](baseline/results.md) | C++文本、原生iEDA、SQLite、EDADB、adapter；1,000/10,000/100,000/1,000,000条Component | 99组正确性，540条计时 |
| [SQLite与EDADB对齐](sqlite-vs-edadb/results.md) | 1,000,000条Component；分别增删NULL检查、清绑定及SQL/API匹配 | 40组正确性，100条计时 |
| [两表PK/FK](sqlite-pk-fk/docs/results.md) | 10,000个Component＋100,000个Pin；子表PK/index/none及FK检查开关 | 88组正确性，275条计时，55个文件库审计 |
| [单表索引与rowid](sqlite-index/docs/results.md) | 1,000,000条Component；8种主键/索引布局 | 48组小规模检查，80个正式样本，16组全量诊断 |

**这些数量的单位不同，不能相加称为统一“测试次数”。** 前三组按读写记录统计，最后一组的每个样本包含多个计时阶段。
sqlite-vs-text仍为“未完成、待继续”，不纳入本报告的已完成结论。

**互相补充的关系**：实验一发现路线间的总体差距；实验二专门解释实验一中EDADB直接API相对SQLite的差距。实验三引入实验一没有的父子关系，检查关联查询与FK约束。实验四去掉实验三的关联恢复和FK变量，单独验证额外索引及表布局的取舍。实验三、四帮助理解真实adapter可能承担的数据库工作，但不能据此精确分摊实验一的adapter时间。

## 2. 共同方法：测什么时间，如何保证可比

### 2.1 数据不是任意字节，而是有类型的Component记录

单表基础对象为8字段：

```cpp
struct ComponentRecord {
    std::string name, master_name;
    int32_t source, status, orient, x, y;
    int64_t record_order;
};
```

- name为U加7位序号，master_name为bench_cell；source/status/orient为2/3/1。
- x=(序号%1000)×100，y=(序号/1000)×100，record_order为序号；按生成顺序插入。
- baseline与API对齐的表没有显式主外键、二级索引或NOT NULL。sqlite-index按变体添加约束；不能把它们说成完全相同DDL。
- 数据生成、输入对象准备、LEF预加载、完整正确性比较不计入数据读写阶段。正式读取仍真正取出字段，复制字符串并消费结果，不能用空循环代替读取。

### 2.2 两类SQLite运行配置

| 项目 | A / memory | B-batch / disk |
| --- | --- | --- |
| 数据库本体 | `:memory:`，内存中 | 独立文件库 |
| journal_mode | MEMORY，回滚日志在内存 | 本机实测默认DELETE |
| synchronous | OFF(0) | 本机实测默认FULL(2) |
| cache_size | -8192，约8192 KiB预算 | -2000，约2000 KiB预算 |
| 数据写事务 | 显式BEGIN，整批写入，COMMIT | 同样显式批量事务 |
| 文件读取 | 不适用 | OS缓存预热，不是cold磁盘读 |

实际PRAGMA逐连接读回并保存。A与B同时改变存储、日志、同步、缓存，**两者差值不能全部称为磁盘耗时**；A不是可替代文件持久化的配置。

名词区别：
- **事务/COMMIT**决定哪些修改作为一组提交；显式事务之外仍可能存在SQLite隐式事务。
- **journal**保存回滚所需内容；MEMORY只说明日志位置，不单独说明DB本体位置。
- **synchronous**控制向VFS请求持久化同步的策略；OFF不等于没有事务，COMMIT也不等于固定一次sync。

官方依据：[事务](https://www.sqlite.org/lang_transaction.html)、[journal_mode](https://www.sqlite.org/pragma.html#pragma_journal_mode)、[synchronous](https://www.sqlite.org/pragma.html#pragma_synchronous)。完整参数见[配置说明](sqlite-reference/config.md)；本报告不展开未测的WAL等其他模式。

### 2.3 计时边界：建表、写入、提交必须分开

采用C++ `steady_clock`墙钟时间，每个阶段各取开始/结束时间：

```text
输入生成、对象准备                          [计时外]
打开连接、配置                              [init]
建表BEGIN → CREATE TABLE/INDEX → 建表COMMIT   [create]
数据BEGIN                                   [begin]
W0
  prepare INSERT / 构造EDADB writer
  循环：绑定字段 → step → reset
  finalize / 销毁writer
W1                                          write data = W1 - W0
C0 → 执行数据COMMIT并等待返回 → C1           commit = C1 - C0
关闭连接                                    [close；内存库延后]

准备读缓存/预热                             [计时外]
打开读取连接（文件库）                       [read init]
R0
  prepare SELECT / 构造EDADB reader
  循环：step → 取列 → 恢复/消费结果
  finalize / 销毁reader
R1                                          read data = R1 - R0
关闭                                        [read close]
```

每个样本先算`write complete = begin + write data + commit`，再统计中位数；**不把各阶段中位数相加冒称完整中位数**。create不在write complete中。
以上边界适用于直接SQLite/EDADB；原生iEDA、文本及adapter的例外见第3节。

COMMIT单列不代表write data排除了所有日志、页面写出或事务维护。读取、写入分别比较，不能拿read与write相减解释性能。

### 2.4 执行与统计

1. 检查schema、合法及非法数据、恢复内容，正确性检查不进入正式性能样本。
2. Release `-O3`构建；同一实验固定库版本、数据与配置。性能样本串行运行，避免多个测试争抢CPU/内存/I/O。
3. 预热后每组正式测5次；保存每次原始时间、均值、中位数、min/max。PK/FK及index内存主结果取repeat，baseline和API对齐取first，不能混淆。
4. 不在每条记录内部反复读时钟。SQL计划、trace、完整字段验证、计数器等诊断按各实验规则放在独立运行，不把诊断时间当正式结果。
5. `EXPLAIN QUERY PLAN`证明SCAN/SEARCH访问方式；VM、页缓存计数证明工作量，**不是每个内部函数的毫秒数**。没有测到的成本不编造分摊。

delta有两种口径：API对齐和PK/FK表中的一般差值是同批中位数相减；sqlite-index专门的delta表先按同轮相减，再取中位数。它们不必数值相同，不能混算。

## 3. 实验一：五路线整体基线

阅读入口：[实验README：数据、配置与运行](baseline/readme.md)。

### 方法与数据

用相同的Component生成规则，分别构造文本记录、最小LEF/DEF或数据库输入。简单SQLite/EDADB表：

```sql
CREATE TABLE component(
  name TEXT, master_name TEXT, source INTEGER, status INTEGER,
  orient INTEGER, x INTEGER, y INTEGER, record_order BIGINT
);
```

| 路线 | 写入实现 | 读取实现 | 工作量边界 |
| --- | --- | --- | --- |
| C++文本 | ofstream格式化8字段 | ifstream解析到Record | 简单文本，不是完整DEF解析器 |
| 原生iEDA | 原生DefWrite写DEF | 原生DefRead解析并恢复iDB | LEF加载在计时外，DEF API内部工作计入 |
| SQLite直接 | 手写prepare/bind/step/reset/finalize | 取8列到复用Record | 无显式PK/FK/二级索引 |
| EDADB直接 | makeInsertOp逐记录insert | makeReadAllOp/readNext | 与SQLite直接相同字段/约束，含框架工作 |
| iEDA adapter | 应用转换＋EDADB写入 | EDADB＋adapter恢复实际iDB | 实际应用schema、引用及对象重建，不是简单Record |

adapter使用41张表，Instance表19列及name主键；它与8字段直接API路线工作量不同。合成输入对应同一类对象，**不代表各路线执行相同数量的操作**。

### 写入结果：1,000,000条

| 路线 | write data ms |
| --- | ---: |
| C++文本 | 748.833 |
| 原生iEDA | 702.185 |
| SQLite A | 1510.430 |
| SQLite B-batch | 1597.901 |
| EDADB A | 1603.358 |
| EDADB B-batch | 1685.538 |
| adapter | 5813.067 |

**口径例外**：文本打开/关闭单列，write包含其格式化输出及flush；原生DEF的打开/关闭在API计时内。adapter的write包含内部事务提交，init混合连接/注册/建表；它的read恢复完整iDB，不包含额外参考DEF扫描。因此不能把adapter−SQLite直接当“adapter包装成本”。

建表与提交另列，避免把一次性成本混入write：

| 路线 | create ms | 数据COMMIT ms | write complete ms |
| --- | ---: | ---: | ---: |
| SQLite A | 0.160 | 1.408 | 1511.874 |
| SQLite B-batch | 78.141 | 715.792 | 2316.542 |
| EDADB A | 0.188 | 13.598 | 1616.961 |
| EDADB B-batch | 74.585 | 714.206 | 2391.578 |
| adapter | 未独立拆分；init合计1415.042 | 已包含于write data | 5813.067 |

### 读取结果：1,000,000条

| 路线 | read data ms |
| --- | ---: |
| C++文本 | 622.220 |
| 原生iEDA | 4843.321 |
| SQLite A | 825.183 |
| SQLite B-batch | 844.419 |
| EDADB A | 1095.802 |
| EDADB B-batch | 1141.248 |
| adapter | 4730.975 |

直接API只恢复8字段Record；原生iEDA与adapter恢复实际iDB。它们不是等量对象重建，不能把时间比直接当作替换方案的加速比。

### 能说明什么

- 本例没有证明SQLite/EDADB在完整顺序写入上比原生DEF更快；即使不计建表和外层提交，写data仍更长。
- 简单Record读取比原生DEF解析/恢复iDB短，但不能当作完整iDB恢复已经获得相同比例加速。实际adapter读取与原生iEDA处在相近量级。
- EDADB直接比SQLite直接有额外成本，需要下一组受控实验解释，而不是把差值统称“遍历”。
- 文件库COMMIT是不可忽略的独立阶段；并不等于该阶段时间全是sync。

完整规模趋势、A-no-journal及小规模autocommit补充见[baseline结果](baseline/results.md)；本节聚焦1,000,000条A/B-batch，不把未显示的配置当未运行。

**Takeaway**：本例数据库顺序写入未优于原生DEF；简单Record读得快不代表完整iDB恢复也更快。实验二继续解释其中可控、同schema的SQLite/EDADB差距。

## 4. 实验二：为什么EDADB比直接SQLite多花时间

阅读入口：[实验README：对照组与证据索引](sqlite-vs-edadb/readme.md)。

固定1,000,000条8字段、无PK/FK/二级索引。下面的“匹配”都是手写SQLite对照，不调用EDADB框架；真实EDADB API另列。手写SQL预先生成，EDADB首次SQL生成仍计入data。

### 读取：多做哪些操作，增加多少时间

基本流程：`prepare SELECT → 循环step/取8列/消费Record → finalize`。

两类改变分别控制：
1. **NULL检查**：每列取值前调用 **`sqlite3_column_type`** 并判断是否为NULL，每条记录8次。
2. **SELECT文本、字符串取列顺序、查询结束清理**：三项一起启用，不加NULL检查：
   - SELECT从不带引号的列名改为EDADB生成的带双引号、空格和末尾分号的文本；表、列及列顺序相同，均无WHERE或ORDER BY，只prepare一次。
   - 两个字符串显式按 **`column_bytes → column_text → string.assign`** 执行；原代码把取长度、取指针作为assign实参，求值顺序未规定。不是新增字符串复制，也不能断言机器码顺序一定改变。
   - 整个SELECT返回DONE后，额外调用一次`clear_bindings → reset`，然后finalize；**不是每行执行**。

“NULL＋三项匹配”同时启用上述两类改变；不能将三项组合的差值拆成各自独占耗时。确切SQL及源码调用位置见[读取实现](sqlite-vs-edadb/alignment.md#2-读取null检查sql取列收尾操作)。

| 路线 | A read ms | B-batch read ms |
| --- | ---: | ---: |
| SQLite基本读取 | 817.614 | 835.683 |
| 仅加NULL检查 | 1063.591 | 1087.837 |
| 仅匹配SQL/取列/收尾 | 825.662 | 843.838 |
| NULL＋三项匹配 | 1079.313 | 1094.703 |
| EDADB API | 1073.911 | 1115.088 |

**读侧分析**：加NULL检查净增约246–252 ms；三项已匹配时，加NULL仍净增约251–254 ms。这是本例读取差距的主要已验证因素。全部匹配后，EDADB剩余差值A为−5.401 ms、B为20.386 ms，样本区间重叠，不把小幅负值称为稳定加速。

约252 ms/1,000,000条等于均摊252 ns/条；每条检查8列，不能称为每次NULL检查252 ns，也不是函数独占计时。

### 写入：多做哪些操作，增加多少时间

基本流程：`prepare INSERT → 循环bind 8字段/step/reset → finalize`；BEGIN、COMMIT单列。

两类改变分别控制：
1. **每行清绑定**：step之后、reset之前增加一次 **`sqlite3_clear_bindings`**。每条一次，reset本来就有。
2. **INSERT文本及绑定实现替换**：采用EDADB相同INSERT文本；两个字符串由`bind_text`改为`bind_text64`，均使用**TRANSIENT**；5个整数由`bind_int64`改为`bind_int`，record_order仍用`bind_int64`。同时由整数数组循环改成逐字段调用。这是替换操作，不是再多绑定8次。

“clear＋文本/绑定匹配”同时启用上述两类改变。具体SQL生成与绑定位置见[写入实现](sqlite-vs-edadb/alignment.md#3-写入clear绑定sql及绑定api)。

| 路线 | A write ms | B-batch write ms |
| --- | ---: | ---: |
| SQLite基本写入 | 1533.247 | 1608.843 |
| 仅加clear_bindings | 1590.237 | 1661.594 |
| 仅匹配SQL/绑定API | 1511.963 | 1593.447 |
| clear＋文本/绑定匹配 | 1583.346 | 1664.665 |
| EDADB API | 1630.426 | 1688.907 |

**写侧分析**：加clear净增约53–57 ms；文本/绑定实现已匹配时净增约71 ms，即均摊约53–71 ns/条。clear释放/重置绑定值，也影响后续资源复用；差值不是函数本体独占时间。全部匹配后，EDADB剩余47.080/24.242 ms，约占其写data的2.89%/1.44%，尚不能精确归因于遍历、锁或内存分配。

上述均为同批中位数之差；两种上下文的NULL增量不能相加，两种clear增量同样不能相加。

结论只适用于本例数据和接口；没有证明NULL/clear在通用系统中应删除。完整操作及源码见[对齐方法](sqlite-vs-edadb/alignment.md)，数字见[对齐结果](sqlite-vs-edadb/results.md)。

**Takeaway**：对实验一的框架差距，读侧NULL检查是主要已验证因素，写侧clear解释一部分；API对齐后的剩余量不能笼统称为遍历开销。本实验不解释父子查询或adapter恢复完整iDB的成本。

## 5. 实验三：父子关联、索引与FK检查

阅读入口：[实验README：两表定义、运行及结果](sqlite-pk-fk/readme.md)。

### schema、数据及执行

父表沿用8字段，声明name主键；每父10个Pin，共10,000父＋100,000子，合计110,000条。Pin是instance的子对象，不是DEF顶层PINS。

```sql
-- 父表：Component八字段，name TEXT NOT NULL，PRIMARY KEY(name)
CREATE TABLE component_pins_instance_pin (
  pin_name TEXT NOT NULL, direction INTEGER, x INTEGER, y INTEGER,
  component_name TEXT NOT NULL,
  FOREIGN KEY(component_name) REFERENCES component(name)
    ON DELETE CASCADE ON UPDATE CASCADE
);
-- pk组：子表增加PRIMARY KEY(component_name,pin_name)
-- index组：不加子PK，增加普通INDEX(component_name,pin_name)
-- none组：不加子PK或额外索引
```

每样本独立库，只建两张用户表。父PK、字段和主矩阵FK ON保持不变；SQLite与EDADB使用相同实际DDL。子表none仍有rowid表树，父表仍有PK，**不是整个数据库没有索引/约束**。

### 写入：父子插入与索引维护

写入复用statement：写一个父，再写其10个子；整批事务。以下选文件库结果，单位ms，create与数据COMMIT不在write中：

| 子表定义 | SQLite write ms | EDADB write ms |
| --- | ---: | ---: |
| 复合主键pk | 419.516 | 414.834 |
| 普通复合索引index | 419.262 | 410.731 |
| 无额外子索引none | 317.914 | 306.821 |

普通子索引比none增加写入101.348 ms（约31.88%），是额外索引维护及相关页操作的净增量，不是索引函数独占时间。

创建与提交另列：SQLite文件库pk的create=75.142 ms、数据COMMIT=162.164 ms；index为69.593/142.705 ms；none为70.318/125.279 ms。三组DB大小分别为6,164,480、6,164,480、3,551,232 B。

### 读取：逐父查询并恢复子对象

先查父表，对每个父执行：

```sql
SELECT pin_name,direction,x,y,component_name
FROM component_pins_instance_pin WHERE component_name=?;
```

子statement复用，但仍是每父一次查询；两条API都恢复父与pins。这与单表全读不同，不能直接比较绝对时间。

| 子表定义 | SQLite关联read ms | EDADB关联read ms |
| --- | ---: | ---: |
| 复合主键pk | 150.274 | 162.000 |
| 普通复合索引index | 149.124 | 161.644 |
| 无额外子索引none | 129745.899 | 127462.238 |

- 普通子索引使SQLite关联读取从约130秒降至约149 ms。
- 计划证据：pk/index使用父FK前缀SEARCH；none每父SCAN子表。100父检查实测101次SELECT，正式10,000父沿同一流程预期10,001次SELECT。
- none约10,000×100,000=1,000,000,000次候选行检查是按循环和计划的估算，不是假称实测计数。约130秒是完整关联恢复，不是一次查询。
- 内存库none也约120–122秒，说明这一慢现象不以磁盘I/O为必要条件。**加索引减少每次查询成本，并没有消除N+1。**

### 写入补充：FK ON/OFF独立对照

**复合主键pk** ： 固定子表pk及合法数据，仅切换连接的foreign_keys，比较**写入阶段**：

| 文件库API | FK OFF write ms | FK ON write ms |
| --- | ---: | ---: |
| SQLite | 346.530 | 419.516 |
| EDADB | 343.153 | 414.834 |

ON时插入子记录需检查父键存在；OFF不删除PK、索引或FK字段。普通SELECT不是逐行重做FK校验，本表不能解释为读侧检查成本。

API小差值也需谨慎：本实验SQLite写入包装器成功路径含错误字符串构造，两条路线其他细节也不同，EDADB较快不能解释为“负的框架开销”。另测的两次全表flat读取只消费字段、不恢复父子对象，不能拿flat−graph精确分离N+1成本。

完整内存/文件结果、min/max、实际SQL、FK配置及限制见[PK/FK结果](sqlite-pk-fk/docs/results.md)。

**Takeaway**：写侧维护子索引增加成本；读侧完整父FK前缀索引避免逐父全表扫描，但不消除N+1。它补充实验一、二缺少的父子访问场景；实验四进一步隔离其中的索引与存储布局因素。

## 6. 实验四：去掉关联，只看单表主键与索引布局

阅读入口：[实验README：8种布局与诊断方法](sqlite-index/readme.md)。

### 为什么需要这一组

上一组混合了父子恢复、FK检查和查询次数。这里回到1,000,000条Component，只用SQLite直接API，改变单表定义，分别测全量INSERT、全读、100次固定命中点查及空间；不引入FK、EDADB或adapter。

“额外索引”不包含表自身的B-tree；普通表没有显式PK时，仍按隐式rowid组织整行。[SQLite官方说明](https://www.sqlite.org/rowidtable.html)

| 变体 | 声明的主键/唯一性 | 额外索引数；用户树总数 | 写入/点查的主要路径 |
| --- | --- | --- | --- |
| none | 无PK | 0；1 | 维护rowid表；按name只能扫描 |
| index | 普通name索引，无唯一约束 | 1；2 | 维护表与索引；查name索引后回表 |
| unique | UNIQUE(name)，无PK | 1；2 | 额外检查唯一性；索引后回表 |
| text-pk | PRIMARY KEY(name) | 1自动唯一；2 | SQLite自动建PK索引；索引后回表 |
| text-pk-without-rowid | PRIMARY KEY(name) | 0；1 | name直接组织表，无rowid回表 |
| integer-unique | UNIQUE(record_order)，无PK | 1；2 | 隐式rowid表＋整数唯一索引 |
| integer-rowid | INTEGER PRIMARY KEY | 0；1 | record_order成为rowid别名，直接查表键 |
| integer-without-rowid | INTEGER PRIMARY KEY＋WITHOUT ROWID | 0；1 | 整数PK组织的WITHOUT ROWID表 |

TEXT组按name点查；整数组按record_order点查。返回同样8字段，但**不跨键类型相减声称rowid开销**。正式数据都唯一、非NULL；none/index不保证唯一性，不能声称所有变体约束完全等价。

### 写入结果：文件库B-batch

write不含create/BEGIN/COMMIT；空间单位B：

| 变体 | write ms | DB大小 B |
| --- | ---: | ---: |
| none | 1624.210 | 43,732,992 |
| index | 2697.484 | 63,225,856 |
| unique | 2736.137 | 63,225,856 |
| text-pk | 2743.657 | 63,225,856 |
| text-pk-without-rowid | 2310.086 | 46,137,344 |
| integer-unique | 2584.098 | 57,421,824 |
| integer-rowid | 1793.381 | 40,706,048 |
| integer-without-rowid | 2154.715 | 46,137,344 |

**写侧证据与限制**：

- **额外索引有维护成本**：同轮index−none写入delta中位数为+1,079.387 ms；额外name索引实占4,759页，约19.49 MB。
- **TEXT主键确实有额外索引**：根页/dbstat证实两棵树；WITHOUT ROWID只有一棵，减少本例写入与空间。不能据此说所有schema都应改WITHOUT ROWID。
- **INTEGER PRIMARY KEY是重要特例**：同整数键下，rowid别名比额外唯一索引更省空间；WITHOUT ROWID反而比rowid别名写得慢。树数量一样也不代表成本一样。
- **VM步数不是时间**：TEXT主键写24,000,000个VM步骤，WITHOUT ROWID为29,000,000步但仍更快；每条VM指令代价不同，不能按步数分配耗时。

### 读取结果：全表扫描与点查分别测试

返回相同8字段；point为100次固定命中查询合计，不是单次查询时间：

| 变体 | 全读ms | 100次point ms |
| --- | ---: | ---: |
| none | 803.720 | 13268.658 |
| index | 803.637 | 2.239 |
| unique | 801.993 | 2.190 |
| text-pk | 806.901 | 2.206 |
| text-pk-without-rowid | 796.620 | 1.913 |
| integer-unique | 805.457 | 2.064 |
| integer-rowid | 793.835 | 1.528 |
| integer-without-rowid | 790.752 | 1.816 |

**读侧证据与限制**：none的100次点查实测99,999,900次FULLSCAN_STEP，有索引为0；计划从SCAN变为SEARCH。全读仍要取所有行，没有获得点查同等级的收益。索引组100次点查总计仅约2 ms，需结合min/max理解小差距，不泛化到所有查询负载。

### 与最初baseline如何对应

只有none是相近的无额外索引场景，但本轮新增name NOT NULL、调整整数绑定API，并换成GCC 11.4独立程序；baseline为g++-10/LTO，CPU与等待设置也不同。

写data边界一致：A为baseline 1510.430 → 本轮1563.863 ms（+3.54%）；B-batch为1597.901 → 1624.210 ms（+1.65%）。**这不是严格同实现复测，不能把跨批差值分摊给索引或NOT NULL**。本轮索引delta只用本轮none参照。

create/commit、A配置、同轮delta范围及源代码差异表统一见[单表索引结果](sqlite-index/docs/results.md)，此处不重复全部阶段表。

**Takeaway**：普通rowid表的TEXT主键需要额外唯一索引；INTEGER PRIMARY KEY别名不需要。额外索引增加写入和空间，却大幅改善点查；全量读取不一定受益。本实验帮助解释实验三的索引取舍，不代表WITHOUT ROWID对所有负载最优。

## 7. 结论：目前可以做出的判断

1. **数据库不天然快于文本。** 本例顺序写入没有体现SQLite/EDADB相对原生DEF的优势；简化Record读取快，不等于完整iDB恢复同样快。
2. **框架差距有可验证原因。** 单表读取的NULL检查、写入的clear绑定是已量化因素；剩余差值尚不能精确分给遍历等模块。
3. **查询方式和索引配套会决定性能量级。** 无索引全读不必慢；无索引反复按父查询会造成大量重复扫描。有索引不等于消除N+1。
4. **索引用维护和空间换查询收益。** 主键还承担唯一性；rowid、TEXT主键及WITHOUT ROWID实际结构不同，必须按键类型和访问负载选择。
5. **四组实验不是同一条可加的成本分解。** 不同schema、对象恢复语义、数据规模、编译和批次之间，只能作注明边界的场景比较。

尚未由这些结果证明：冷盘性能、随机插入/UPDATE、真实大设计的普遍收益、adapter各内部阶段独占耗时、等价对象恢复下的批量读优势。以上不是已完成优化，不在汇报中填入推测数字。

## 8. 补充内容

文档内链接使用相对路径，便于整体复制目录。下面各实验readme包含运行命令；源码链接用于核查实现，不需要重跑才阅读结果。

| 问题 | 方法/运行 | 源码 | 结果 |
| --- | --- | --- | --- |
| 五条路线如何计时 | [baseline/readme](baseline/readme.md)、[实现说明](benchmark/implementation.md) | [stream_benchmark.cpp](benchmark/stream_benchmark.cpp) | [baseline/results](baseline/results.md) |
| NULL/clear如何做对照 | [alignment](sqlite-vs-edadb/alignment.md) | [alignment.cpp](sqlite-vs-edadb/alignment.cpp) | [API对齐results](sqlite-vs-edadb/results.md) |
| 两张表如何写入/读取 | [pk-fk/readme](sqlite-pk-fk/readme.md)、[实现说明](sqlite-pk-fk/docs/implementation.md) | [pk_fk_benchmark.cpp](sqlite-pk-fk/pk_fk_benchmark.cpp) | [pk-fk/results](sqlite-pk-fk/docs/results.md) |
| rowid/索引定义及计划 | [index/readme](sqlite-index/readme.md)、[test_plan](sqlite-index/docs/test_plan.md) | [benchmark.cpp](sqlite-index/benchmark.cpp) | [index/results](sqlite-index/docs/results.md) |

每个结果页底部都链接原始samples、统计、正确性/审计、manifest和配置。原始产物位于服务器/tmp，不随Git提交，也不会随单个Markdown文件拷贝；冻结文档保留实测摘要与复现方法。本文只整合四个冻结实验，不修改它们的代码或结论。

## 9. 历史起点：Sky130 GCD filler为何读取约慢250倍

这里回顾的是**最初未优化的实际iEDA＋adapter路径**，不是前述1,000,000条合成Component实验。输入为`scripts/design/sky130_gcd/result/iPL_filler_result.def`：726,155 B、17,151行，包含2,604个Instance、677个Net、2个SpecialNet。

当时基线为`prof-test @ 7b661baa2`、core `90a5fb249`，iEDA与EDADB均为Release，SQL trace关闭。每种缓存模式预热1次、正式串行5次，以中位数比较。cold使用文件缓存驱逐建议，不代表已证明物理磁盘完全冷；warm预读输入文件。

计时在Tcl命令外层：LEF加载、进程启动、输出DEF严格diff均在计时外。原生测`def_init`/`def_save`；EDADB测完整`edadb_read`/`edadb_write`，**写包含初始化建表和内部提交，读包含当时生产路径的参考DEF扫描**。不要与前面已排除create/外层COMMIT的data时间混用。

### 9.1 读取：原生约48 ms，EDADB约12秒

| 缓存模式 | 原生DEF read ms | EDADB read ms | EDADB/原生耗时比 |
| --- | ---: | ---: | ---: |
| cold | 48.421 | 11933.243 | 246.45倍 |
| warm | 47.576 | 11872.868 | 249.56倍 |

**这批完整读取命令耗时约为原生的250倍**，不是EDADB通用接口在所有场景都慢250倍。

**主要问题：Net的两个叶子表缺少支持父键查询的索引。** 实际物理表名如下，均有FOREIGN KEY定义，但没有以完整父FK为左前缀的PK/索引：

| 子对象 | 实际表名 | 后续诊断库行数 |
| --- | --- | ---: |
| Point | `iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__point_list_sd_iCoordSD` | 14,256 |
| ViaRef | `iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__via_ref_list_sd_iRegViaRef` | 3,716 |

恢复过程是`Net → Wire → Segment → Point/ViaRef`。每次恢复一个Segment的子对象，都按完整的“Net名＋Wire键＋Segment键”筛选子表。上述两表的查询计划为SCAN，需要重复扫描，而不是借助父键索引SEARCH。隐式rowid表树不支持按这三个父键直接查找；仅声明FK也没有建立这个查询索引。