# P1开始的逐版本代码修改范围

## 阅读方式与边界

**展示规则：** 小改动直接贴真实diff；重复模式贴一段真实代码；文件链接全部使用相对于本文的仓库内路径，指向`/home/zhiyiwang/cs/arch/eda/iEDA-EDADB`现有文件。当前开发分支基于P1；行号标注来自历史commit；本地后续编辑可能使锚点偏移，请以函数名和git diff复核。标记“历史行号”的链接仅打开当前文件，不代表当前文件已包含该阶段修改；历史完整代码用本节`git diff`命令查看。历史新增但当前不存在的文件只列路径，不制造失效链接。无需下载或导出源码副本。

- 先读总览，再看各阶段“生产代码”说明；文件清单完整列出生产、测试、脚本、文档和仓库配置，不省略core子仓库。
- 实际历史顺序：**B0 → P1 → P3 → P4 → P5 → P2**。每节比较相邻累计版本，不是每个P都直接与B0比较。
- 本文依据Git实际diff。新增/删除行数包含注释与格式调整，不代表运行时成本或有效代码量。
- 当前开发分支为`edadb-performance-optimization-dev`，基于P1配对提交，adapter已有未提交的粗粒度计时，以及新移植的P3/P4事务合并。当前修改见[transaction-batching.md](transaction-batching.md)。本文的各P修改以历史commit为准，不包含本次新增计时。
- 父仓库路径以iEDA仓库根为基准；core表格路径以`src/database/edadb/core/`为基准。gitlink的一增一删只是提交指针变化，不能当成core只有两行修改。
- 性能与计时说明单独见[实验汇总](optimization-history.md)，本文仅审阅修改范围。

## 总览

| 新增阶段 | iEDA前→后 | core前→后 | 核心修改范围 |
| --- | --- | --- | --- |
| P1 | `62504bd9d → 4ba761281` | `be6bbdd → 1c4857c` | 2个core生产头文件；adapter逻辑不变 |
| P3 | `4ba761281 → 6990b7072` | `1c4857c → c2c92da` | 1个adapter源文件；core仅测试 |
| P4 | `6990b7072 → 8d8550629` | `c2c92da → 6d3718f` | 1个adapter源文件；core仅测试 |
| P5 | `8d8550629 → 29808d61a` | `6d3718f → 7066b01` | 1个adapter源文件改逻辑；另有注释与文档 |
| P2 | `29808d61a → 9c376467b` | `7066b01 → ad0f820` | 1个adapter源文件＋6个core头文件（含profiling） |

## P1：子表parent-FK索引

### 修改前后：查询没变，创建的数据库结构变了

原来`createOneTable()`只执行CREATE TABLE，成功便返回。Point/ViaRef等无自有主键的child表，没有为完整父键链建立查询索引。P1在这个返回之前增加CREATE INDEX：

```text
修改前：createOneTable → CREATE TABLE → 返回
修改后：createOneTable → CREATE TABLE → 判断是否需要父键索引
                                    → CREATE INDEX → 返回
```

以Point为例，以下用简化列名说明SQL变化，不是实际完整表名：

```sql
-- 新增的数据库结构
CREATE INDEX point_parent_fk ON point(net_id, wire_id, segment_id);
-- 读取SQL未改；现在该条件可以使用新增索引
SELECT * FROM point WHERE net_id=? AND wire_id=? AND segment_id=?;
```

具体新增两步：
1. `SqlStatement4Sqlite.h`生成SQL：无parent FK或者已有生成主键时返回空串，否则收集全部祖先FK列，组成非唯一索引。
2. `DbTableOpCreate4Sqlite.h`执行SQL：空串不执行；执行失败返回`-1`，不继续假装建表成功。

**改变的是查找路径，不是恢复算法：** 仍然每个父对象发起一次child查询；插入会多维护一棵索引，DB也会更大。这里没有改变FK约束、行数据和对象所有权。

**生产代码：core新增索引生成与执行，adapter存取逻辑不变。**
- `SqlStatement4Sqlite.h::createParentForeignKeyIndexStatement()`：有parent FK且`hasPrimKey == false`时，收集完整父FK列链，生成非UNIQUE的`CREATE INDEX IF NOT EXISTS`。不把vector序号额外加入索引，也不动态分析任意已有索引。
- `DbTableOpCreate4Sqlite.h::createOneTable()`：CREATE TABLE后执行上述索引SQL；错误向上传播。沿用当前遍历与事务，不另遍历对象图。
- 新增`DbChildForeignKeyIndex.cpp`测试。
- 父仓库`.gitmodules`的分支提示从`main`改成`performance/optimization`；实际版本仍由gitlink锁定，不能只依据分支名判断。

**不包含：** 查询批量化、事务合并、Shadow重构。

### 直接查看代码：两处小改动，完整diff

[include/edadb/backend/sqlite/SqlStatement4Sqlite.h:114](../../core/include/edadb/backend/sqlite/SqlStatement4Sqlite.h#L114)
```diff
diff --git a/include/edadb/backend/sqlite/SqlStatement4Sqlite.h b/include/edadb/backend/sqlite/SqlStatement4Sqlite.h
index 5327886..bb660c6 100644
--- a/include/edadb/backend/sqlite/SqlStatement4Sqlite.h
+++ b/include/edadb/backend/sqlite/SqlStatement4Sqlite.h
@@ -111,6 +111,27 @@ public:
         return sql;
     } // createTableStatement

+    static std::string createParentForeignKeyIndexStatement(const DbTableDef<T>& tableDef) {
+        if (!tableDef.parentFkc()->valid() || Cpp2SqlTypeTrait<T>::hasPrimKey) {
+            return {};
+        }
+
+        std::vector<std::string> fk_col_names;
+        collectCurrentTableForeignKeyColumns(tableDef, fk_col_names);
+        assert(!fk_col_names.empty());
+
+        const std::string& table_name = tableDef.getTableName();
+        const std::string index_name = table_name + "__edadb_parent_fk_idx";
+
+        std::string sql = "CREATE INDEX IF NOT EXISTS \"" + index_name
+                        + "\" ON \"" + table_name + "\" (";
+        for (std::size_t index = 0; index < fk_col_names.size(); ++index) {
+            sql += (index > 0 ? ", \"" : "\"") + fk_col_names[index] + "\"";
+        }
+        sql += ");";
+        return sql;
+    } // createParentForeignKeyIndexStatement
+
     static std::string dropTableStatement(const DbTableDef<T> &tableDef) {
         return "DROP TABLE IF EXISTS \"" + tableDef.getTableName() + "\";";
     } // dropTableStatement
```

[include/edadb/backend/sqlite/DbTableOpCreate4Sqlite.h:120](../../core/include/edadb/backend/sqlite/DbTableOpCreate4Sqlite.h#L120)
```diff
diff --git a/include/edadb/backend/sqlite/DbTableOpCreate4Sqlite.h b/include/edadb/backend/sqlite/DbTableOpCreate4Sqlite.h
index a53058f..cc2c66c 100644
--- a/include/edadb/backend/sqlite/DbTableOpCreate4Sqlite.h
+++ b/include/edadb/backend/sqlite/DbTableOpCreate4Sqlite.h
@@ -117,6 +117,14 @@ private:
             return -1;
         }

+        const std::string index_sql =
+            SqlStatement<DbTableDefT>::createParentForeignKeyIndexStatement(*typed_table_def);
+        if (!index_sql.empty() && !DbManager::i().exec(index_sql)) {
+            std::cerr << "DbTableOperatorImpl::createOneTable: create parent-FK index failed for table "
+                      << tableDef->getTableName() << std::endl;
+            return -1;
+        }
+
         return 0;
     } // createOneTable
```

### iEDA父仓库完整修改清单

| 文件 | 新增行 | 删除行 |
| --- | ---: | ---: |
| [.gitignore:41](../../../../../.gitignore#L41) | 1 | 0 |
| [.gitmodules:15](../../../../../.gitmodules#L15) | 1 | 1 |
| [agent.md:917](../../../../../agent.md#L917) | 10 | 0 |
| [scripts/edadb/performance/README.md:183](../../../../../scripts/edadb/performance/README.md#L183) | 16 | 2 |
| [scripts/edadb/performance/TODO.md:1](../../../../../scripts/edadb/performance/TODO.md#L1) | 360 | 0 |
| [scripts/edadb/performance/sky130_gcd_ipl_filler_p1_child_fk_index_20260829.md:1](../../../../../scripts/edadb/performance/sky130_gcd_ipl_filler_p1_child_fk_index_20260829.md#L1) | 81 | 0 |
| `src/database/edadb/core` | 1 | 1 |

### EDADB core完整修改清单

| 文件 | 新增行 | 删除行 |
| --- | ---: | ---: |
| [include/edadb/backend/sqlite/DbTableOpCreate4Sqlite.h:120](../../core/include/edadb/backend/sqlite/DbTableOpCreate4Sqlite.h#L120) | 8 | 0 |
| [include/edadb/backend/sqlite/SqlStatement4Sqlite.h:114](../../core/include/edadb/backend/sqlite/SqlStatement4Sqlite.h#L114) | 21 | 0 |
| [test/DbChildForeignKeyIndex.cpp:1](../../core/test/DbChildForeignKeyIndex.cpp#L1) | 389 | 0 |

### 查看这一阶段的完整代码diff（无需切分支）

```bash
git diff 62504bd9d 4ba761281 -- src/database
git -C src/database/edadb/core diff be6bbdd 1c4857c
```

## P3：合并schema事务

### 修改前后：把每棵表树的事务移到初始化外层

```text
修改前 initWriteDb：
  initAllTables(true)
    createTable<Design>()  → BEGIN → 创建该root的表树 → COMMIT
    createTable<Die>()     → BEGIN → 创建该root的表树 → COMMIT
    ……其他root各自执行事务

修改后 initWriteDb：
  BEGIN
  initAllTables(true, false)
    createTable<Design>(false) → 只创建表树，不自己提交
    createTable<Die>(false)    → 只创建表树，不自己提交
    ……
  全部成功：COMMIT；任一步失败：ROLLBACK并返回错误
```

为实现这一点，参数必须传到底，不能只在外面加BEGIN：

```diff
-int initTable(bool crt_tab)
+int initTable(bool crt_tab, bool self_txn = true)
-edadb::createTable<T>()
+edadb::createTable<T>(self_txn)
-int initAllTables(bool crt_tab)
+int initAllTables(bool crt_tab, bool self_txn = true)
```

上面是签名/调用的差异摘录；`EDADB_INIT_TABLE`宏和15处root注册调用也同步透传`self_txn`。默认值保留`true`，仅该初始化路径显式传`false`。

**为什么不是P4：** 此时只合并CREATE TABLE/INDEX，真正INSERT数据仍由每个root writer单独提交。P3没有合并数据事务。

**生产代码：只改adapter建表事务边界；core只有测试增加。**
- `edadb_idb_init.cpp::initTable()`、`initAllTables()`及宏`EDADB_INIT_TABLE`增加/传递`self_txn`，调用`createTable<T>(self_txn)`。
- `initWriteDb()`：原本每棵root schema tree独立事务，改为`BEGIN → initAllTables(true, false) → COMMIT`；创建或提交失败时ROLLBACK并返回错误。
- core `DbFacadeTransactions.cpp`增加外部schema事务的提交、失败回滚验证。

**不包含：** 新的core生产API、表字段变化、design数据事务合并。

### 直接查看代码：事务关键段

以下为修改后真实代码片段，不是伪代码；省略函数前面的连接初始化。

[src/database/edadb/idb/edadb_idb_init.cpp:129（历史行号；链接为当前文件）](../../idb/edadb_idb_init.cpp)
```cpp
    if (!edadb::beginTransaction()) {
        std::cerr << "Error: failed to begin schema transaction" << std::endl;
        return -1;
    }

    if (initAllTables(true, false) < 0) {
        if (!edadb::rollbackTransaction()) {
            std::cerr << "Error: failed to rollback schema transaction" << std::endl;
        }
        std::cerr << "Error: failed to initAllTables in edadb database" << std::endl;
        return -1;
    }

    if (!edadb::commitTransaction()) {
        std::cerr << "Error: failed to commit schema transaction" << std::endl;
        if (!edadb::rollbackTransaction()) {
            std::cerr << "Error: failed to rollback schema transaction" << std::endl;
        }
        return -1;
    }
```

参数传递位置：[src/database/edadb/idb/edadb_idb_init.cpp:38（历史行号；链接为当前文件）](../../idb/edadb_idb_init.cpp)（initTable）、[src/database/edadb/idb/edadb_idb_init.cpp:72（历史行号；链接为当前文件）](../../idb/edadb_idb_init.cpp)（initAllTables）。完整函数从[src/database/edadb/idb/edadb_idb_init.cpp:119（历史行号；链接为当前文件）](../../idb/edadb_idb_init.cpp)查看。

### iEDA父仓库完整修改清单

| 文件 | 新增行 | 删除行 |
| --- | ---: | ---: |
| [.gitignore:42（历史行号；链接为当前文件）](../../../../../.gitignore) | 1 | 0 |
| [agent.md:927（历史行号；链接为当前文件）](../../../../../agent.md) | 6 | 0 |
| [scripts/edadb/performance/README.md:187（历史行号；链接为当前文件）](../../../../../scripts/edadb/performance/README.md) | 10 | 4 |
| [scripts/edadb/performance/TODO.md:34（历史行号；链接为当前文件）](../../../../../scripts/edadb/performance/TODO.md) | 17 | 12 |
| `scripts/edadb/performance/sky130_gcd_ipl_filler_p3_schema_transaction_20260829.md:1`（历史版本文件，当前P1工作区无此文件） | 70 | 0 |
| `src/database/edadb/core` | 1 | 1 |
| [src/database/edadb/idb/edadb_idb_init.cpp:38（历史行号；链接为当前文件）](../../idb/edadb_idb_init.cpp) | 38 | 22 |

### EDADB core完整修改清单

| 文件 | 新增行 | 删除行 |
| --- | ---: | ---: |
| [test/DbFacadeTransactions.cpp:161（历史行号；链接为当前文件）](../../core/test/DbFacadeTransactions.cpp) | 33 | 0 |

### 查看这一阶段的完整代码diff（无需切分支）

```bash
git diff 4ba761281 6990b7072 -- src/database
git -C src/database/edadb/core diff 1c4857c c2c92da
```

## P4：合并design事务

### 修改前后：数据写完再统一提交，并检查writer是否失败

```text
修改前：schema初始化完成
        Design → BEGIN/写入/COMMIT
        Die    → BEGIN/写入/COMMIT
        ……
        Net失败时，先前root可能已经提交

修改后：schema初始化完成（P3的schema事务已结束）
        BEGIN design事务
        Design、Die、……、Net都只写数据，不自己开关事务
        全部成功 → COMMIT
        任一root失败 → ROLLBACK → 返回false
```

代码分两处配合，缺一不可：
1. 外层`writeDb2Edadb()`管理事务，并把原来忽略的writer结果保存为`write_status`。
2. 所有root里的`insertObject/insertVector`都传`false`，避免继续使用各自的事务边界。

实际调用变化示例：

```diff
-writeChip2Edadb();
+write_status = writeChip2Edadb();
```

新增的`write_status`检查确保不能在子writer已失败时仍返回成功；BEGIN失败直接退出，COMMIT失败则尝试ROLLBACK并返回失败。

**改变的不只是性能：** 回滚范围由单个root扩大到全部design数据。schema仍单独提交，因此design写失败后可能留下空表结构，并不是整个数据库文件也被删除。

**生产代码：只改adapter写入事务与错误传播；core只有测试变化。**
- `def_write_edadb.cpp::writeDb2Edadb()`：schema初始化之后开启design事务；保存各分支writer返回值，成功统一COMMIT，失败ROLLBACK，提交失败也处理回滚。
- 各root writer调用`insertObject(..., false)`或`insertVector(..., false)`，不再由root各自提交。
- 新增adapter transaction begin/commit/rollback profiling阶段；OFF配置不启用内部profiling。
- core `DbFacadeTransactions.cpp`验证两张root表共同提交、后一个root重复键失败时前面写入一起回滚。

**语义变化：** 从可能部分root已提交，变为整个design原子成功或失败。schema事务仍与design事务分开；不改读取。

### 大改动：按执行顺序查看

- [src/database/manager/builder/def_builder/def_write_edadb.cpp:50（历史行号；链接为当前文件）](../../../manager/builder/def_builder/def_write_edadb.cpp)：writeDb2Edadb，包含BEGIN、writer结果检查、COMMIT及ROLLBACK。
- [src/database/manager/builder/def_builder/def_write_edadb.cpp:240（历史行号；链接为当前文件）](../../../manager/builder/def_builder/def_write_edadb.cpp)：Design关闭自有事务。
- [src/database/manager/builder/def_builder/def_write_edadb.cpp:295（历史行号；链接为当前文件）](../../../manager/builder/def_builder/def_write_edadb.cpp)：Row vector关闭自有事务。其余root使用相同的false参数，完整diff见本节末命令。

两类调用的实际修改示例：
```diff
-    if (!edadb::insertObject<idb::IdbDesign>(design)) {
+    if (!edadb::insertObject<idb::IdbDesign>(design, false)) {
-    if (!edadb::insertVector<edadb::Shadow<idb::IdbRow>>(row_sd_vec)) {
+    if (!edadb::insertVector<edadb::Shadow<idb::IdbRow>>(row_sd_vec, false)) {
```

### iEDA父仓库完整修改清单

| 文件 | 新增行 | 删除行 |
| --- | ---: | ---: |
| [agent.md:932（历史行号；链接为当前文件）](../../../../../agent.md) | 8 | 1 |
| [scripts/edadb/performance/README.md:553（历史行号；链接为当前文件）](../../../../../scripts/edadb/performance/README.md) | 8 | 1 |
| [scripts/edadb/performance/TODO.md:34（历史行号；链接为当前文件）](../../../../../scripts/edadb/performance/TODO.md) | 3 | 3 |
| `scripts/edadb/performance/sky130_gcd_ipl_filler_p4_design_transaction_20260829.md:1`（历史版本文件，当前P1工作区无此文件） | 76 | 0 |
| `src/database/edadb/core` | 1 | 1 |
| [src/database/manager/builder/def_builder/def_write_edadb.cpp:67（历史行号；链接为当前文件）](../../../manager/builder/def_builder/def_write_edadb.cpp) | 49 | 20 |

### EDADB core完整修改清单

| 文件 | 新增行 | 删除行 |
| --- | ---: | ---: |
| [test/DbFacadeTransactions.cpp:25（历史行号；链接为当前文件）](../../core/test/DbFacadeTransactions.cpp) | 55 | 1 |

### 查看这一阶段的完整代码diff（无需切分支）

```bash
git diff 6990b7072 8d8550629 -- src/database
git -C src/database/edadb/core diff c2c92da 6d3718f
```

## P5：流式root Shadow

### 修改前后：不再同时保留全部root的Shadow

下面是执行逻辑示意，真实代码摘录在下一小节：

```text
修改前：
  对所有root：new Shadow → toShadow → 放进临时vector
  insertVector(全部Shadow, false)
  对所有Shadow：delete

修改后：
  makeInsertOp()，放在循环外复用
  对每个root：
    栈上创建Shadow → toShadow → insert(&shadow)
    本次循环结束，销毁Shadow及其临时成员
```

- 删除：四个writer中的`vector<Shadow*>`、预留容量、逐个new、集中插入和集中delete，包括手工清理先前Shadow的错误分支。
- 增加：循环外一个insert operator；循环内一个栈对象及立即插入的返回值检查。
- 保留：`toShadow()`转换、Instance/Pin/Net顺序参数、嵌套vector的原有插入方式、P4数据事务。

**内存边界是“一个root”，不是一条SQL行。** 一个大型Net仍可能构造很大的Shadow graph；此改动仅避免把全部Net的临时graph同时保留。prepared statement没有变成每root重新构造，也没有变成多行INSERT。

**生产逻辑：只改四个root writer的临时Shadow生命周期。**
- `def_write_edadb.cpp`中的`writeIdbInstance()`、`writeIdbPin()`、`writeIdbSpecialNet()`、`writeIdbNet()`：
  原来：构造全部heap Shadow → 保存到vector → insertVector → 统一delete。
  现在：先创建可复用`makeInsertOp()` → 逐root构造stack Shadow → toShadow → insert → 立即销毁。
- 保留root/vector顺序、嵌套对象转换、prepared statement复用及P4外层事务；转换/插入失败返回错误。
- `edadb_idb_init.cpp`新增2行设计注释；core `SqlStatement4Sqlite.h`新增3行P1索引说明，**没有新的core执行逻辑**。
- 新增`make_routed_stress_fixture.py`生成路由压力数据，更新adapter文档与RSS实验报告。

**不包含：** 批量多行INSERT、STATIC绑定、读取优化。

### 直接查看代码：Instance代表实现

以下是修改后的真实循环，其余三类使用相同的生命周期策略。

[src/database/manager/builder/def_builder/def_write_edadb.cpp:411（历史行号；链接为当前文件）](../../../manager/builder/def_builder/def_write_edadb.cpp)
```cpp
    auto insert_op = edadb::makeInsertOp<edadb::Shadow<idb::IdbInstance>>();
    for (uint32_t inst_idx = 0; inst_idx < inst_vec.size(); ++inst_idx) {
        edadb::Shadow<idb::IdbInstance> inst_sd;
        if (!inst_sd.toShadow(inst_vec[inst_idx], &inst_idx)) {
            std::cerr << "DefWriteEdadb::writeIdbInstance failed to convert instance shadow" << std::endl;
            return kDbFail;
        }
        if (insert_op.insert(&inst_sd) < 0) {
            std::cerr << "DefWriteEdadb::writeIdbInstance failed to insert shadow" << std::endl;
            return kDbFail;
        }
    }
```

四个完整函数：
- [src/database/manager/builder/def_builder/def_write_edadb.cpp:388（历史行号；链接为当前文件）](../../../manager/builder/def_builder/def_write_edadb.cpp)：writeIdbInstance。
- [src/database/manager/builder/def_builder/def_write_edadb.cpp:427（历史行号；链接为当前文件）](../../../manager/builder/def_builder/def_write_edadb.cpp)：writeIdbPin。
- [src/database/manager/builder/def_builder/def_write_edadb.cpp:694（历史行号；链接为当前文件）](../../../manager/builder/def_builder/def_write_edadb.cpp)：writeIdbSpecialNet。
- [src/database/manager/builder/def_builder/def_write_edadb.cpp:733（历史行号；链接为当前文件）](../../../manager/builder/def_builder/def_write_edadb.cpp)：writeIdbNet。

### iEDA父仓库完整修改清单

| 文件 | 新增行 | 删除行 |
| --- | ---: | ---: |
| [agent.md:940（历史行号；链接为当前文件）](../../../../../agent.md) | 10 | 0 |
| [scripts/edadb/performance/README.md:578（历史行号；链接为当前文件）](../../../../../scripts/edadb/performance/README.md) | 12 | 9 |
| [scripts/edadb/performance/TODO.md:34（历史行号；链接为当前文件）](../../../../../scripts/edadb/performance/TODO.md) | 16 | 8 |
| `scripts/edadb/performance/make_routed_stress_fixture.py:1`（历史版本文件，当前P1工作区无此文件） | 72 | 0 |
| `scripts/edadb/performance/sky130_gcd_routed_p5_stream_shadow_20260829.md:1`（历史版本文件，当前P1工作区无此文件） | 88 | 0 |
| `src/database/edadb/core` | 1 | 1 |
| [src/database/edadb/docs/idb-adapter/07_idb_instance.md:9（历史行号；链接为当前文件）](../idb-adapter/07_idb_instance.md) | 4 | 4 |
| [src/database/edadb/docs/idb-adapter/08_idb_pin.md:9（历史行号；链接为当前文件）](../idb-adapter/08_idb_pin.md) | 5 | 5 |
| [src/database/edadb/docs/idb-adapter/09_idb_blockage.md:9（历史行号；链接为当前文件）](../idb-adapter/09_idb_blockage.md) | 2 | 2 |
| [src/database/edadb/docs/idb-adapter/10_idb_region.md:9（历史行号；链接为当前文件）](../idb-adapter/10_idb_region.md) | 5 | 5 |
| [src/database/edadb/docs/idb-adapter/11_idb_slot.md:9（历史行号；链接为当前文件）](../idb-adapter/11_idb_slot.md) | 3 | 3 |
| [src/database/edadb/docs/idb-adapter/12_idb_group.md:9（历史行号；链接为当前文件）](../idb-adapter/12_idb_group.md) | 3 | 3 |
| [src/database/edadb/docs/idb-adapter/13_idb_fill.md:9（历史行号；链接为当前文件）](../idb-adapter/13_idb_fill.md) | 3 | 3 |
| [src/database/edadb/docs/idb-adapter/14_idb_special_net.md:14（历史行号；链接为当前文件）](../idb-adapter/14_idb_special_net.md) | 3 | 3 |
| [src/database/edadb/docs/idb-adapter/15_idb_net.md:11（历史行号；链接为当前文件）](../idb-adapter/15_idb_net.md) | 4 | 4 |
| [src/database/edadb/idb/edadb_idb_init.cpp:129（历史行号；链接为当前文件）](../../idb/edadb_idb_init.cpp) | 2 | 0 |
| [src/database/manager/builder/def_builder/def_write_edadb.cpp:67（历史行号；链接为当前文件）](../../../manager/builder/def_builder/def_write_edadb.cpp) | 36 | 81 |

### EDADB core完整修改清单

| 文件 | 新增行 | 删除行 |
| --- | ---: | ---: |
| [include/edadb/backend/sqlite/SqlStatement4Sqlite.h:115（历史行号；链接为当前文件）](../../core/include/edadb/backend/sqlite/SqlStatement4Sqlite.h) | 3 | 0 |

### 查看这一阶段的完整代码diff（无需切分支）

```bash
git diff 8d8550629 29808d61a -- src/database
git -C src/database/edadb/core diff 6d3718f 7066b01
```

## P2：Wire范围叶子批量读取

### 修改前后：SQL覆盖多个Segment，C++再分组恢复

假设一个Wire有3个Segment，各自有Point vector。以下使用简化表名/列名演示访问范围，不是实际完整SQL文本：

```sql
-- 修改前：为3个Segment分别执行，绑定3次不同segment_id
SELECT point_idx, x, y FROM point
WHERE net_id=? AND wire_id=? AND segment_id=?;

-- 修改后：这个Wire范围执行一次，额外返回segment_id用于分组
SELECT segment_id, point_idx, x, y FROM point
WHERE net_id=? AND wire_id=?
ORDER BY segment_id, point_idx;
```

减少的是查询执行次数，不是少读取Point数据，也不是不再调用step取每一行。

| 新增步骤 | 具体做什么 | 为什么需要 |
| --- | --- | --- |
| adapter开关 | `readIdbNet()`调用`enableLeafBatchRead()` | 仅Net选择新路径，其他调用默认不变 |
| 资格检查 | 祖先深度至少2，且当前表没有child | 本实现只批读叶子，不处理叶子下的完整子树 |
| SQL/绑定改变 | WHERE只绑定前d−1个父FK；最后一个FK作为结果列返回 | 扩大到共同祖先范围，同时保留直接父对象身份 |
| 首次范围加载 | 遍历结果，取首列父键，按父键暂存vector | 同一次查询取得多个Segment的Point |
| 列偏移改变 | 数据字段从原起点加1开始读取 | 第0列新增了分组父键，不能当作原字段读取 |
| 交付某父vector | 读取父键，找到暂存组，move出并调用`commitFetchedVector()` | 保持原有逐父恢复接口；没有该组则交付空vector |
| 范围切换 | 比较共同祖先对象地址；切Wire清理并重新加载 | 不能把前一Wire的结果交给后一Wire |
| root切换 | `beginRootReadRow()`清除后代缓存 | root对象可能复用同一个地址，仅比较地址不足以防止跨root旧数据命中 |
| 失败清理 | 加载失败清空暂存，释放仍持有的指针元素 | 避免泄漏及把未完整加载的结果交给目标vector |

对应流程：

```text
第一次恢复Segment A.Point：查询当前Wire全部Point → 暂存A/B/C组 → 取出A组
随后恢复Segment B.Point：直接取出B组，不再查询
随后恢复Segment C.Point：直接取出C组，不再查询
切换Wire或root：清缓存，按新范围重新加载
```

**新增代价与限制：** 多返回一列父键，增加分组容器、范围缓存及可能的排序成本；是否划算取决于少发查询的收益。它仍按原对象遍历推进，只在叶子层预取，不是把整个恢复框架改成BFS。失败保障针对当前暂存批次/目标vector，不等于整个芯片已经恢复的对象都自动回滚。

**生产代码：core叶子批读实现＋adapter仅Net显式启用；本组范围最大。**
- adapter `def_read_edadb.cpp::readIdbNet()`：在query operator上调用`enableLeafBatchRead()`，原root排序保留。
- core `DbTableOperator.h`：新增策略开关、向child传播、`beginRootReadRow()`缓存生命周期入口。
- core `DbForeignKeyBinder.h`：新增`bindFirst()`，只绑定祖先FK前缀。
- core `SqlStatement4Sqlite.h`：新增`queryForeignKeyPrefixStatement()`；d个祖先FK中前d−1个用于WHERE，最后一个FK随SELECT返回作直接父对象分组键；按该键和首个本地映射列排序。
- core `DbTableOpSelect4Sqlite.h`：新增叶子batch cache与前缀加载/分组恢复；缓存限定当前root内的范围，root切换时清理；支持暂存结果、失败清理及指针所有权处理，恢复vector顺序。
- core `DbTableOpQueryGeneric4Sqlite.h`接入root行开始通知；`DbProfiler.h`增加批加载/命中计数与计数读取接口。
- `DbTableOpSelect.cpp`、`DbProfiler.cpp`扩展正确性及计数测试；`AGENTS.md`更新维护说明。

**实际作用域：** Net→Wire→Segment→Point/ViaRef/VirtualPoint；每个Wire范围批读叶子，再归属各Segment。不是全表BFS或通用root-window；写入路径不变。
**区间说明：** P5到P2之间还有`89e51ff87`讨论/复核文档提交，下表包含它的文档差异，没有把这些文档算作生产优化。

### 直接查看代码：adapter小改动

[src/database/manager/builder/def_builder/def_read_edadb.cpp:994（历史行号；链接为当前文件）](../../../manager/builder/def_builder/def_read_edadb.cpp)
```diff
diff --git a/src/database/manager/builder/def_builder/def_read_edadb.cpp b/src/database/manager/builder/def_builder/def_read_edadb.cpp
index f510ad218..3d5f5911b 100644
--- a/src/database/manager/builder/def_builder/def_read_edadb.cpp
+++ b/src/database/manager/builder/def_builder/def_read_edadb.cpp
@@ -991,7 +991,8 @@ bool DefReadEdadb::readIdbNet(void) {

     net_list->reset();

-    auto net_reader = edadb::makeGenericQueryOp<edadb::Shadow<idb::IdbNet>>();
+  auto net_reader = edadb::makeGenericQueryOp<edadb::Shadow<idb::IdbNet>>();
+  net_reader.enableLeafBatchRead();
     if (net_reader.preparePredicate("ORDER BY \"_order_sd\"") < 0) {
         std::cerr << "DefReadEdadb::readIdbNet failed to prepare ordered query!" << std::endl;
         return false;
```

### core大改动：建议按下列顺序阅读

- [include/edadb/DbTableOperator.h:70（历史行号；链接为当前文件）](../../core/include/edadb/DbTableOperator.h)：策略开关与传播。
- [include/edadb/DbTableOperator.h:98（历史行号；链接为当前文件）](../../core/include/edadb/DbTableOperator.h)：root行边界通知。
- [include/edadb/backend/sqlite/SqlStatement4Sqlite.h:265（历史行号；链接为当前文件）](../../core/include/edadb/backend/sqlite/SqlStatement4Sqlite.h)：前缀WHERE、分组键投影、ORDER BY生成。
- [include/edadb/DbForeignKeyBinder.h:44（历史行号；链接为当前文件）](../../core/include/edadb/DbForeignKeyBinder.h)：bindFirst入口。
- [include/edadb/DbForeignKeyBinder.h:108（历史行号；链接为当前文件）](../../core/include/edadb/DbForeignKeyBinder.h)：递归绑定实现。
- [include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h:549（历史行号；链接为当前文件）](../../core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h)：缓存类型与清理。
- [include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h:594（历史行号；链接为当前文件）](../../core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h)：root行切换失效。
- [include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h:676（历史行号；链接为当前文件）](../../core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h)：批读适用条件。
- [include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h:729（历史行号；链接为当前文件）](../../core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h)：按父键查找暂存组并交付vector。
- [include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h:778（历史行号；链接为当前文件）](../../core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h)：执行范围查询、取列、分组、错误清理。

### iEDA父仓库完整修改清单

| 文件 | 新增行 | 删除行 |
| --- | ---: | ---: |
| [.gitignore:38（历史行号；链接为当前文件）](../../../../../.gitignore) | 2 | 0 |
| `scripts/edadb/performance/PROGRESS.md:1`（历史版本文件，当前P1工作区无此文件） | 92 | 0 |
| [scripts/edadb/performance/README.md:506（历史行号；链接为当前文件）](../../../../../scripts/edadb/performance/README.md) | 36 | 20 |
| [scripts/edadb/performance/TODO.md:34（历史行号；链接为当前文件）](../../../../../scripts/edadb/performance/TODO.md) | 49 | 12 |
| `scripts/edadb/performance/sky130_gcd_p2_reassessment_20260829.md:1`（历史版本文件，当前P1工作区无此文件） | 145 | 0 |
| `scripts/edadb/performance/sky130_gcd_routed_p2_leaf_batch_20260829.md:1`（历史版本文件，当前P1工作区无此文件） | 122 | 0 |
| `src/database/edadb/core` | 1 | 1 |
| [src/database/manager/builder/def_builder/def_read_edadb.cpp:994（历史行号；链接为当前文件）](../../../manager/builder/def_builder/def_read_edadb.cpp) | 2 | 1 |

### EDADB core完整修改清单

| 文件 | 新增行 | 删除行 |
| --- | ---: | ---: |
| [AGENTS.md:304（历史行号；链接为当前文件）](../../core/AGENTS.md) | 5 | 0 |
| [include/edadb/DbForeignKeyBinder.h:43（历史行号；链接为当前文件）](../../core/include/edadb/DbForeignKeyBinder.h) | 52 | 0 |
| [include/edadb/DbProfiler.h:51（历史行号；链接为当前文件）](../../core/include/edadb/DbProfiler.h) | 15 | 1 |
| [include/edadb/DbTableOperator.h:40（历史行号；链接为当前文件）](../../core/include/edadb/DbTableOperator.h) | 24 | 1 |
| [include/edadb/backend/sqlite/DbTableOpQueryGeneric4Sqlite.h:178（历史行号；链接为当前文件）](../../core/include/edadb/backend/sqlite/DbTableOpQueryGeneric4Sqlite.h) | 1 | 0 |
| [include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h:12（历史行号；链接为当前文件）](../../core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h) | 225 | 9 |
| [include/edadb/backend/sqlite/SqlStatement4Sqlite.h:265（历史行号；链接为当前文件）](../../core/include/edadb/backend/sqlite/SqlStatement4Sqlite.h) | 26 | 0 |
| [test/DbProfiler.cpp:23（历史行号；链接为当前文件）](../../core/test/DbProfiler.cpp) | 2 | 0 |
| [test/DbTableOpSelect.cpp:72（历史行号；链接为当前文件）](../../core/test/DbTableOpSelect.cpp) | 213 | 3 |

### 查看这一阶段的完整代码diff（无需切分支）

```bash
git diff 29808d61a 9c376467b -- src/database
git -C src/database/edadb/core diff 7066b01 ad0f820
```

## 建议审阅顺序

1. **P1**：索引生成条件、父FK列链是否正确、索引失败处理。
2. **P3 → P4**：事务边界、子操作是否关闭自有事务、错误返回与回滚。二者分别处理schema和数据。
3. **P5**：Shadow活多久，insert返回前是否已用完其字段，以及root顺序与失败传播是否保留。
4. **P2**：最后审阅，重点看FK前缀、排序列、缓存失效、对象所有权与失败原子性；其复杂度高于前几项。

以上范围不包含后来讨论但尚未在这些提交实现的多行INSERT、STATIC绑定、通用BFS/root-window或新的独立粗粒度计时。
