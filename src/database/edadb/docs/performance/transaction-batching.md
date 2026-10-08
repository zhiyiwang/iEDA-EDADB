# Milestone实现：索引、事务与流式Shadow

本文解释P1/P3/P4/P5，不重复时间表；结果见[optimization-results.md](optimization-results.md)。

## 1. 父键索引（P1）

parent FK有效且表未声明PK时，收集完整祖先FK列，在建表后建立普通复合索引。避免按父对象查子表时反复全表扫描，不改变查询次数或取消FK约束；写入仍需维护索引。

这是当前实现的判定规则，不是动态检查所有已有索引的通用覆盖分析。源码：core `include/edadb/backend/sqlite/SqlStatement4Sqlite.h:114`，`createParentForeignKeyIndexStatement()`。

## 2. 合并建表事务（P3）

```text
BEGIN
    全部root的CREATE TABLE及子表索引
    各createTable使用self_txn=false
COMMIT
```

减少每棵schema tree分别提交；失败返回错误并尝试回滚。源码：`src/database/edadb/idb/edadb_idb_init.cpp:120`，`initWriteDb()`。

## 3. 合并数据事务（P4）

```text
BEGIN
    依次写入全部root family
    检查每个writer返回值
全部成功 → COMMIT
任一失败 → ROLLBACK并返回失败
```

各root使用外层事务，减少重复提交。schema与design是两个事务：数据失败时schema可保留，design数据整体回滚。仍是逐行INSERT，不是多行VALUES。源码：`src/database/manager/builder/def_builder/def_write_edadb.cpp:51`，`writeDb2Edadb()`。

## 4. 流式Shadow（P5）

```text
原来：转换全部root → 保存整组Shadow → insertVector → 统一释放
现在：创建可复用InsertOp → 逐root转换、insert、释放 → 销毁操作器
```

仅修改Instance、Pin、SpecialNet、Net四类writer。每个root及其子对象同步写完后才释放Shadow；沿用P4事务和原有顺序。目标是减少同时存活的临时对象，不减少记录数或承诺写入加速。

对应同一写入源文件的InsertOp位置：Instance :413、Pin :451、SpecialNet :719、Net :758。

## 5. 如何核对代码与时间

上述行号属于固定标签 `milestone/ieda-edadb-optimized-v1`，不是含新实验的工作区。仓库根目录用以下命令读取，不必切换当前分支：

```bash
git show milestone/ieda-edadb-optimized-v1:src/database/manager/builder/def_builder/def_write_edadb.cpp
git -C src/database/edadb/core show milestone/ieda-edadb-optimized-v1:include/edadb/backend/sqlite/SqlStatement4Sqlite.h
```

create单独计时，含schema事务；write data含design事务，不含init/create。本批未独立计时BEGIN/COMMIT，不能用新profiling结果补入，详见[stage-timing.md](stage-timing.md)。功能测试覆盖代表性失败回滚及重试，不声称覆盖全部异常、掉电或提交故障。
