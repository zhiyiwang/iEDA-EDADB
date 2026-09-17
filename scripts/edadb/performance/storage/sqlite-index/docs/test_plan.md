# SQLite单表主键与索引实验方案

**状态：已按本方案完成并通过审计。** [结果与证据](results.md)。首轮只测试SQLite C API，不测FK、父子遍历、EDADB或adapter，不修改冻结实验。

## 1. 测什么

在相同Component字段、数据和SQL下，比较主键/索引定义对写入、读取和空间的影响。不重跑sqlite-pk-fk的双表、FK ON/OFF或N+1矩阵；仅重新建立必要的同批单表参照，不减历史时间。

## 2. rowid是什么：不是额外再建一个索引

本实验使用普通表，不涉及虚拟表：

- 未写WITHOUT ROWID时，即使没有声明主键，SQLite也有唯一的64位整数rowid。表B-tree本身就按rowid组织整行，不是“数据表之外另建rowid索引”。
- INSERT未指定rowid时，SQLite自动分配；指定时使用给定值。插入仍需维护以rowid为键的表B-tree。更新普通字段修改表记录；改变rowid则改变表的存储键，并需维护相关索引引用。本轮只测插入/读取，不增加UPDATE性能实验。
- TEXT主键不替代rowid：另有唯一索引保存name与rowid；读取非索引列通常先查索引，再用rowid查表。
- 普通表的INTEGER PRIMARY KEY是rowid别名，不再增加主键索引。必须写INTEGER；BIGINT PRIMARY KEY或INT PRIMARY KEY不是该别名。
- WITHOUT ROWID才取消隐式rowid，以声明的PK组织表；不能在无PK的表上仅关闭rowid。

因此none应读作“无额外索引”，不是“无B-tree/无键维护”。本轮不使用AUTOINCREMENT；自动分配rowid不需要该关键字，也不需要其sqlite_sequence维护。
官方：[rowid表](https://www.sqlite.org/rowidtable.html)、[rowid分配](https://www.sqlite.org/autoinc.html)、[WITHOUT ROWID](https://www.sqlite.org/withoutrowid.html)。

## 3. TEXT键组：相同字段，五种schema变体

沿用[单表Record和生成器](../../benchmark/benchmark_support.h)的8字段：

```sql
CREATE TABLE component (
  name TEXT NOT NULL, master_name TEXT,
  source INTEGER, status INTEGER, orient INTEGER,
  x INTEGER, y INTEGER, record_order BIGINT
);
```

每个样本独立新库，仅一张表；运行参数选择DDL，不需分别编译。

| 参数 | 基础DDL的变化 | 要回答的问题 |
| --- | --- | --- |
| none | 不加索引或主键 | 存储相同记录的参照 |
| index | CREATE INDEX component_name_idx ON component(name) | 普通索引的维护代价及查询收益 |
| unique | CREATE UNIQUE INDEX component_name_idx ON component(name) | 相比普通索引，唯一性要求带来的净变化 |
| text-pk | CREATE TABLE内加PRIMARY KEY(name) | TEXT主键和显式唯一索引有无可测差异 |
| text-pk-without-rowid | text-pk末尾加WITHOUT ROWID | 保留相同逻辑主键，改变物理布局的影响 |

所有组name均NOT NULL，正式数据均唯一。none/index没有唯一性保证，只在本次有效数据下比较同样内容，不声称全部约束语义相同。

**是否新建额外索引，要相对于“存整行数据的表B-tree”判断：**

| 定义 | 是否额外新建独立索引 | 谁创建 | 最终物理结构 |
| --- | --- | --- | --- |
| none | 否；隐式rowid不是表外额外索引 | SQLite创建按rowid组织的表 | 1棵表树 |
| index | 是，新增1个普通name索引 | 测试显式执行CREATE INDEX | 1棵表树＋1棵索引树 |
| unique | 是，新增1个唯一name索引 | 测试显式执行CREATE UNIQUE INDEX | 1棵表树＋1棵索引树 |
| text-pk | 是，即使没有写CREATE INDEX，也会新增1个唯一索引 | SQLite根据PRIMARY KEY(name)自动创建，本例名为sqlite_autoindex_component_1 | 1棵表树＋1棵自动索引树 |
| text-pk-without-rowid | 否；主键直接组织表，不另建独立PK索引树 | SQLite创建按name组织的表 | 1棵PK组织的表树 |

因此，显式唯一索引和自动主键索引是不同变体，**不是在同一个表上叠加两种索引**。本轮无其他索引；用sqlite_schema根页核验物理树数，不仅看index_list。WITHOUT ROWID的PK虽出现在index_list中，不能据此认为它额外占一棵树。
官方：[rowid表](https://www.sqlite.org/rowidtable.html)、[WITHOUT ROWID](https://www.sqlite.org/withoutrowid.html)。

## 4. 整数键组：显式比较rowid别名与额外主键索引

仍使用同样8字段和值，把record_order统一声明为INTEGER类型。本组name不设索引；三组均以record_order为逻辑唯一键，值为0至999,999，每次INSERT显式绑定该字段。具体PK/UNIQUE/NOT NULL声明见表，INTEGER PRIMARY KEY的NULL自动分配例外另做正确性检查。

采用下面三种定义，不使用DESC等语法例外：

| 参数 | record_order及表尾定义 | 预期用户B-tree数 |
| --- | --- | ---: |
| integer-unique | INTEGER NOT NULL；额外CREATE UNIQUE INDEX component_order_idx ON component(record_order) | 2：隐式rowid表＋整数唯一索引 |
| integer-rowid | INTEGER PRIMARY KEY | 1：record_order就是rowid |
| integer-without-rowid | INTEGER PRIMARY KEY；表末WITHOUT ROWID | 1：INTEGER主键组织的WITHOUT ROWID表 |

integer-unique与其余两组在本实验中都保证record_order非空、唯一；虽声明不同，但逻辑键和值可比。integer-rowid与integer-without-rowid保持相同逻辑PK，仅改变布局。前者不重复存储独立的整数键值，后者按WITHOUT ROWID记录格式存储，空间差异属于布局影响。

本组查询统一用WHERE record_order=?，返回相同8字段；复用TEXT组选择的100条记录，绑定它们的record_order。全读、写入、事务及计时边界不变。禁止用TEXT点查减整数点查声称“rowid成本”。

integer-unique仍自动生成隐式rowid，integer-rowid则显式绑定其别名；两者差值包含键分配、记录布局和索引维护等净变化，不能精确等同“额外索引维护时间”。本轮不另做自动生成/显式指定rowid的专项计时。

## 5. 数据与配置
### 每种实现实际维护什么

这里“额外索引数”不包含表自身B-tree；“树总数”包含表，不包含sqlite_schema等系统结构。WITHOUT ROWID的逻辑PK可能出现在index_list中，但不表示又有一棵独立PK树。

| 实现 | 声明的主键／唯一键 | 表的存储键 | 额外索引数；树总数 | INSERT维护 | 本轮点查访问 |
| --- | --- | --- | --- | --- | --- |
| none | 无 | 隐式rowid | 0；1 | 分配rowid，插入表树 | name无索引，扫描表并过滤 |
| index | 无PK，普通name索引不保证唯一 | 隐式rowid | 1普通；2 | 插入表树及(name,rowid)索引条目 | 查name索引，再按rowid回表取其余列 |
| unique | 无PK，UNIQUE(name) | 隐式rowid | 1唯一；2 | 唯一性检查，插入表树及索引 | 查唯一索引，再回表 |
| text-pk | PRIMARY KEY(name) | 隐式rowid | 1自动唯一；2 | 检查name唯一性，插入表树及PK索引 | 查PK索引，再回表 |
| text-pk-without-rowid | PRIMARY KEY(name) | name | 0；1 | 在PK组织的表树检查唯一性并插入记录 | 直接查PK组织的表，无rowid回表 |
| integer-unique | 无PK，UNIQUE(record_order) | 隐式rowid | 1唯一；2 | 分配rowid，检查整数键唯一性，维护表与索引 | 查整数唯一索引，再回表 |
| integer-rowid | INTEGER PRIMARY KEY(record_order) | record_order即rowid | 0；1 | 使用绑定的rowid，检查冲突并插入表树 | 直接按整数表键定位 |
| integer-without-rowid | PRIMARY KEY(record_order) | record_order，非rowid | 0；1 | 在PK组织的表树检查唯一性并插入记录 | 直接查PK组织的表 |

全量读取均取8字段：预期扫描表本身，不逐条点查，也不维护索引。最终用实际EQP/EXPLAIN核验；表中的访问路径是待验证预期，不假装已经测得。插入可能还引发页面分裂、缓存及日志维护，不能把树数量直接换算为耗时倍数。
官方依据：[文件格式中的表与索引](https://www.sqlite.org/fileformat.html)、[查询执行路径](https://www.sqlite.org/queryplanner.html)。


- 正式1,000,000条Component，与单表基线规模一致。独立实现相同generate()规则，避免引入iEDA依赖：name为U0000000至U0999999，master_name为bench_cell，source/status/orient为2/3/1，x/y/record_order保持原生成规则。按生成顺序插入，同一批输入与哈希用于全部组；这也是name递增顺序，不外推随机插入。
- 正确性用0、1、1,000条；数据生成、查询键准备、完整校验在正式计时外。
- 使用[已有A与B-batch](../../sqlite-reference/config.md)，读回实际PRAGMA。保留日志与批量事务，不新增autocommit、关闭journal实验；本表无FK。
- TEXT组5变体＋整数组3变体，共8变体×2配置=16组；各预热1次、正式5次，共80个正式配置样本。先A后B，各配置内轮换变体顺序，性能串行运行，Release -O3。
- A使用保留的内存连接；B读取OS-warm文件、新连接。每个读任务独立做不计时预热，不称cold测试。A/B差值不能全算为磁盘代价。

## 6. 写入与读取分别测

**写入各阶段独立计时：**

1. init：打开连接、配置。
2. create：建表BEGIN → CREATE TABLE及本组CREATE INDEX → COMMIT。空库先建索引，不测试导入后建索引。
3. begin：数据BEGIN。
4. write data：prepare一次INSERT → N次绑定8字段/step/reset → finalize；所有组绑定API与错误检查相同。
5. commit：执行COMMIT至返回。
6. close：文件库关闭；内存库保留至读完。

主比较使用write data，另报create、commit、逐样本begin+data+commit。write data仍可能含页写出和事务维护，不是纯CPU时间。

**读取分两个任务，各自计时，不相加混称read；下表展示TEXT组，整数组仅将点查条件改为record_order=?：**

| 任务 | SQL与请求 | 计时范围 |
| --- | --- | --- |
| 全量读取 | SELECT name,master_name,source,status,orient,x,y,record_order FROM component | prepare → step/取8列/复用Record/消费 → DONE → finalize |
| name点查 | 上述SELECT追加WHERE name=? | prepare一次 → 100次bind/step/取8列/读至DONE/reset → finalize |

点查键在计时外生成：j=0..99，取第floor((j+0.5)×N/100)条记录，以固定种子打乱请求顺序并保存清单。每组相同100个命中请求，包括none；无索引最多约100×N候选行扫描是工作量估计，实际用计数器验证，不再重复昂贵的大量N+1测试。

全列读取使普通name索引不能覆盖结果，是否回表用实际EQP/EXPLAIN验证。无ORDER BY；不依赖返回顺序。字符串复制、轻量消费所有组相同；完整逐值校验单独运行，不纳入性能样本。

## 7. 比较和证据

| 同配置、同任务的delta | 可解释的净影响 |
| --- | --- |
| index − none | 增加普通索引 |
| unique − index | 唯一性要求及相关执行路径变化 |
| text-pk − unique | 两种唯一键声明方式 |
| text-pk-without-rowid − text-pk | 相同逻辑主键下的物理布局变化 |
| integer-rowid − integer-unique | 整数键成为rowid别名，而非另有唯一索引的净变化 |
| integer-without-rowid − integer-rowid | 相同INTEGER主键下，两种表组织的净变化 |

- 报告写ms、全读ms、100次点查ms及µs/请求，create/commit单列，另列DB/页面大小。给出中位数、min–max、同轮配对delta；小于波动的差异不下结论。
- 差值不是某内部函数的独占耗时；不预设主键更慢、WITHOUT ROWID更快。[官方查询计划说明](https://www.sqlite.org/queryplanner.html)用于解释实际计划，不替代实测。
- 独立诊断保存DDL、SQL、PRAGMA、版本、构建参数、sqlite_schema、index_xinfo、EQP/EXPLAIN；有dbstat则统计各树空间，无则报告总页数，不换库。
- 如需解释工作量，另跑statement/cache计数，write与commit分开；正式循环不逐行计时、不读计数、不打印SQL。VM步数不是CPU指令数或毫秒。
- 独立正确性验证8字段、行数、命中/未命中、失败回滚；重复name在none/index允许，在unique/两种PK拒绝，NULL name各组均拒绝。
- 上述name唯一性测试仅适用于TEXT组；整数组三组都拒绝重复record_order。INTEGER PRIMARY KEY收到NULL时会自动分配rowid，不能错误断言其一定拒绝NULL；正式数据始终显式提供非NULL整数。独立检查此语义差别，WITHOUT ROWID组和integer-unique应拒绝NULL。
- 验证普通组可SELECT rowid；integer-rowid中rowid=record_order；WITHOUT ROWID组不可SELECT rowid。用根页与EXPLAIN确认表键/索引访问，不能因index_list为空就认定没有键维护。读取rowid仅用于计时外检查。
- 全部正确性通过、各组5次齐全且统计可回溯才算完成。DB、日志、原始样本放仓库外；本目录维护代码、计划和精炼结果。

## 8. 执行边界

按TEXT键5组、整数键3组分别分析，数据1,000,000条、点查100次。不扩展EDADB、FK、UPDATE、cold、随机插入或其他规模。代码、命令和结果入口统一由本目录readme索引。
