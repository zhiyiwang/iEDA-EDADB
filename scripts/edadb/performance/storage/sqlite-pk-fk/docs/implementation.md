# 主外键实验实现

本目录只链接SQLite与EDADB，不加载iEDA或adapter；两条API共享实际DDL和SQL。schema与数据定义见[方案](test_plan.md)。

**核验结论：graph两条路径的数据语义、父子读写顺序及事务边界一致，但不是逐个SQLite API调用完全一致。** EDADB路线直接运行core API，不是手写模拟；SQLite路线是对照实现。差异与验证证据见第5节；flat不属于对象恢复等价对照。

## 1. 源码入口

| 内容 | 位置 |
| --- | --- |
| C++对象与映射 | [pk_fk_benchmark.cpp:20](../pk_fk_benchmark.cpp#L20) |
| schema选择与SQL生成 | [Schema:95](../pk_fk_benchmark.cpp#L95) |
| 连接与PRAGMA | [Connection:169](../pk_fk_benchmark.cpp#L169) |
| 输入生成、消费与校验 | [generate:200](../pk_fk_benchmark.cpp#L200)、[Consumer:221](../pk_fk_benchmark.cpp#L221) |
| 写入、图读取、flat读取 | [write_data:261](../pk_fk_benchmark.cpp#L261)、[read_data:290](../pk_fk_benchmark.cpp#L290)、[read_flat:314](../pk_fk_benchmark.cpp#L314) |
| schema/EQP、约束及回滚 | [inspect:362](../pk_fk_benchmark.cpp#L362)、[constraints:382](../pk_fk_benchmark.cpp#L382) |
| 阶段计时 | [main:433](../pk_fk_benchmark.cpp#L433) |

## 2. 写入

```text
SQLite：prepare父INSERT、子INSERT（各一次）
        对每个父：bind/step/reset父 → 对其每个子bind/step/reset子
        finalize两条语句
EDADB：构造root insert op
       对每个父调用insert，core递归写pins
       销毁op
```

父子statement确实交织使用；本实验没有“交织 vs 分阶段”或“重复prepare vs 复用”的专项对照，不能把差值归因于交织。

```text
计时外：生成输入
init：元数据/SQL准备、连接及配置
create：BEGIN → 两张表及必要索引 → COMMIT
begin：数据BEGIN
write：prepare/构造op → 全部父子插入 → finalize/销毁op
commit：数据COMMIT
close：文件连接关闭；memory留给读取
```

write_complete=begin+write+commit，按每个样本计算；不含init/create/close。write可能包含脏页写出和约束维护，不是纯bind时间。

## 3. 读取

```text
SQLite：prepare父SELECT和带父FK参数的子SELECT
        父step → 取字段 → 子bind/step/取字段/reset → 恢复pins → 消费父对象
        重复直至父DONE → finalize
EDADB：构造reader → readNext恢复一个父及其pins → 消费 → 重复 → 销毁reader
```

两边都恢复一个父及子vector，不保留全部父对象；分配、填充、替换和清理计入read，但不宣称分配次数相同。flat只执行两次全表SELECT消费字段，不构建对象关系。

```text
计时外：文件预读；memory保留连接
read init：文件库新连接与配置
read：prepare/构造reader → 全量取字段/恢复/消费/清理 → finalize/销毁
close：关闭连接
```

memory的first/repeat均重建reader但复用连接，first不等于冷缓存。读取不混入建表、写入和COMMIT。

## 4. 正确性、统计与已知限制

[run_pk_fk.py](../run_pk_fk.py)并发独立check后串行测时，保存原始日志、源码、编译参数和版本；[audit_pk_fk.py](../audit_pk_fk.py#L14)重算时间统计并核验文件库。SELECT执行次数由check中的trace核对，不在正式计时中采集。

SQLite [Statement::insert](../pk_fk_benchmark.cpp#L77)成功路径也构造错误信息字符串。这项既有成本未修改，不能用小幅负差值证明EDADB比最精简SQLite实现更快。若以后专门分析API净开销，应先统一包装器，再做独立实验。

## 5. 与EDADB实际执行过程的核验

### 写入：父先于子，数据事务由测试入口统一控制

| 环节 | 手写SQLite | EDADB实际调用 |
| --- | --- | --- |
| schema | Schema生成DDL，两条路线共用create | 同样执行测试的create，不测试core独立建表入口 |
| 每个父对象 | bind_parent → 父INSERT → 逐个bind_pin/子INSERT | insert → insertWithParents → 成员遍历；afterObjectScalars先绑定FK并执行当前行，再递归子vector |
| statement | 循环前prepare父子语句，循环内复用 | prepare有prepared保护；子operator缓存复用，不是每父重新prepare |
| 事务 | main中BEGIN → write_data → COMMIT | 使用同一外层事务；此处调用insert op，不经过adapter的root-family事务 |

测试入口：[write_data](../pk_fk_benchmark.cpp#L261)。core依据：[insertWithParents](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpInsert4Sqlite.h#L100)、[当前行先执行](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpInsert4Sqlite.h#L210)、[子operator复用及递归](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpInsert4Sqlite.h#L285)。

### 读取：同样是N+1，恢复一个父及其子vector

| 环节 | 手写SQLite | EDADB实际调用 |
| --- | --- | --- |
| 读取父 | 父SELECT逐行step/fetch_parent | readNext → fetchStep → 遍历父标量 |
| 读取子 | 绑定当前父name，执行带WHERE的子SELECT | travObjectVectorChild → readByForeignKey绑定父键、执行同类子SELECT |
| 恢复关系 | 临时pins收集全部子记录，成功后move到record.pins | fetched_vec暂存子记录，成功后commitFetchedVector替换目标vector |
| 查询次数 | 1次父SELECT + 每父1次子SELECT；无ORDER BY | 相同；有索引只改变访问计划，不消除N+1 |

测试入口：[read_data](../pk_fk_benchmark.cpp#L290)。core依据：[readNext](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h#L390)、[子查询入口](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h#L242)、[按FK读取](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h#L546)、[替换vector](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h#L321)。按父名/子名校验结果，不要求无ORDER BY查询的返回顺序一致。

### 不一致之处：不能把时间差全部称为遍历成本

- **绑定API**：手写整数统一bind_int64、字符串bind_text；core按字段宽度使用bind_int/bind_int64，字符串使用bind_text64。两边字符串均使用SQLITE_TRANSIENT。[core绑定](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbStatement4Sqlite.h#L175)
- **复用清理**：手写执行后仅reset；core的resetForReuse先clearBindings再reset，读到结束也有清理。[core清理](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbStatement4Sqlite.h#L131)
- **读取字段**：core逐映射字段检查NULL；手写仅要求字符串非NULL，整数直接取列。输入不含NULL，因此本测试结果等价，不覆盖任意NULL数据。字符串取值顺序也不同：手写text→bytes，core bytes→text。[NULL检查](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h#L76)、[字符串读取](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbStatement4Sqlite.h#L395)
- **构造与准备**：手写fetch_pin构造临时值再push_back，core先emplace_back再填充；core另有元数据派发、状态检查和子operator管理，prepare时机也非逐调用相同。SQL文本生成、prepare及析构等需按实际计时边界理解，不能据源码步骤数推算毫秒。[core子对象恢复](../../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h#L725)
- **测试包装器**：手写INSERT成功路径仍构造诊断字符串；两边都在read计时内消费结果。当前实验适合比较schema变化，不是已严格对齐所有API的框架净开销实验。

### 本轮验证及范围

- 当前源码重新以Release/-O3构建，core为`d6656f08c9ae3123ce5272a344ea3d5515159fe7`；未修改测试逻辑或core。
- **88组check通过**：覆盖三种schema、两种API/存储、FK开关补充和SQLite flat；输入包含空集、单父单子、混合空/单/多子及100父×10子。
- **32组跨API配对通过**：生成SQL、实际DDL/列/索引/FK、EQP、SELECT次数及约束/回滚检查一致；其中16组文件库额外按键排序逐值比较父子表。
- 正式主矩阵每次graph读取10,000父对应10,001次SELECT；本轮100父小样本实际检查101次，flat检查2次。已有大规模性能数据不因本轮check而更新。
- 日志中的SQL行是生成文本，不能冒充逐条执行trace；SELECT计数来自check回调，调用顺序同时由上述源码核验。尚未采集逐个prepare/bind/step/reset调用序列，因此不声称逐API完全一致。
- [检查明细](../../../../../../../../../../../../tmp/iedadb_pk_fk_flow_verify_qacos8l1/checks.json)、[配对核验](../../../../../../../../../../../../tmp/iedadb_pk_fk_flow_verify_qacos8l1/audit.json)。本轮check包含额外校验和trace，其时间不用于性能结论。
