# SQLite主键存储布局实验

状态：独立方案，待实现、待测试。不会修改已有sqlite-pk-fk实验或覆盖其结果。

数据、字段及父子关系沿用[主外键方案](../../sqlite-pk-fk/docs/test_plan.md)；本实验父表始终为TEXT主键，child始终为复合主键，所有布局保持同样PK/FK约束。

### 要区分什么

普通rowid表的TEXT或复合主键由额外唯一索引支持；典型INTEGER PRIMARY KEY则可直接作为rowid别名。WITHOUT ROWID用声明主键组织表，不再维护那份独立rowid表。不是所有主键都会增加第二棵树，也不是给无主键表追加一个主键声明。

本例父表name是TEXT主键，pk模式的child是复合主键，因此当前两张表各有两棵B-tree。index模式的child同样有表树和索引树，但索引不保证唯一。现有pk−none同时改变索引与唯一性约束，不能分离物理布局的影响。

### 保持主外键语义，只改变布局

本实验拟议参数为`--layout rr|rw|wr|ww`，不提供index/none模式；r表示普通rowid表，w表示WITHOUT ROWID。父子字段、显式NOT NULL、主键列序、FK/CASCADE和SQL全部保留，不增加其他索引。

| layout | 父Component | 子Pin | 两表物理B-tree预期总数 |
| --- | --- | --- | ---: |
| rr | rowid表＋TEXT PK唯一索引 | rowid表＋复合PK唯一索引 | 4 |
| rw | 同rr | 直接按复合PK组织 | 3 |
| wr | 直接按TEXT PK组织 | 同rr | 3 |
| ww | 直接按TEXT PK组织 | 直接按复合PK组织 | 2 |

不计sqlite_schema等系统结构。实施时在测试建表层为所选表的CREATE TABLE末尾、分号前增加`WITHOUT ROWID`；不删除PK，不用INTEGER替换TEXT，不改C++对象或生产core。每个样本仍新建数据库，不能复用旧库改表。

先做SQLite四组，再验证EDADB能否用相同DDL与现有API读写；检查是否依赖rowid/last_insert_rowid。兼容性未确认前不宣称EDADB支持，也不静默修改core。数据仍为10,000父×10子，A/B-batch各自配对、FK ON，沿用原阶段边界及5次串行测量。rr须同批重测，不能拿历史rr减去新rw。

### 分别测写入与读取

- **写入**：字段、父子插入顺序和事务相同；比较write，create及COMMIT单列。同时记录提交后DB大小和每棵树的页数。树减少是结构事实，是否更快或更小必须实测。
- **读取**：沿用逐父查询、完整恢复pins的graph路径，SQL不变、不加ORDER BY。child查询还取direction/x/y，普通PK索引不覆盖这些字段，通常需要回表；WITHOUT ROWID可从主键组织的表记录取得它们。用EQP及EXPLAIN核实，而不是假定所有主键查询都访问两棵树。
- **父表限制**：当前父读取是全表扫描，不是按name点查；FK检查只确认父键存在，也可能直接用唯一索引完成。因此wr不保证读得更快。若以后测父键点查，应另列`SELECT x FROM component WHERE name=?`工作负载，不混入graph总时间。
- 校验按父名称与(父名称, Pin名称)比较，不依赖隐含rowid或返回顺序；保留重复键、孤儿、NULL主键和回滚负测。当前Pin显式NOT NULL，使两种布局的空值约束一致。

### 独立核验结构与工作量，不干扰正式计时

1. 从sqlite_schema取两张用户表及所属索引的name/type/rootpage/sql，核对实际DDL与不同根页。不能用PRAGMA index_list的条目数直接推断树数：WITHOUT ROWID也会显示逻辑PK信息。
2. 构建支持dbstat时，在独立诊断中按name汇总页数、pgsize及payload，分别看父/子表及PK索引；不支持时明确缺少分树统计，不为此偷偷更换SQLite库。DB文件大小只统计disk；memory可报页数，不冒称文件大小。
3. 独立取得EQP/EXPLAIN、RUN/VM_STEP及CACHE_HIT/MISS/WRITE/SPILL；write与COMMIT分开。计数不是耗时，正式样本不开trace、不逐行计时。
4. 在本目录新增独立测试入口与审计，分组键包含layout，按实际布局核验主键/根页；不能沿用“一个PK条目等于一份独立索引树”的假设。复用现有字段及SQL定义的原则，但不修改原实验runner或改变原结果分组。具体代码复用方式在实现前确认。

### 差值如何解释

- rr−rw：父表固定时，改变child布局的净影响。
- rr−wr：child固定时，改变父表布局的净影响。
- rr−ww：父子同时改变的整体效果，不要求等于前两项之和。

按同配置、API、轮次分别计算写/读delta，再报告中位数与范围。它们是布局变化的净差，包含编码、缓存、页密度及查找路径变化，不能叫“额外索引函数的独占时间”。WITHOUT ROWID并非对所有行宽和访问方式都更快。

依据：[SQLite主键与rowid](https://www.sqlite.org/rowidtable.html)、[WITHOUT ROWID结构、限制及适用条件](https://www.sqlite.org/withoutrowid.html)、[dbstat页统计](https://www.sqlite.org/dbstat.html)。通用机制见[存储说明](../../sqlite-reference/btree_storage.md)，本节只维护实验设计。
