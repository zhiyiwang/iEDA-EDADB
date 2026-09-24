# P3/P4：合并建表与数据事务

## 本轮范围

在当前`edadb-performance-optimization-dev`分支上移植历史P3（iEDA `6990b7072`）和P4（`8d8550629`），保留已添加的分段计时及不扫描参考DEF的读取路径。不引入P5/P2，不修改core生产源码或gitlink。

## P3：一次提交全部schema

- [edadb_idb_init.cpp:38](../../idb/edadb_idb_init.cpp#L38)：`initTable`、`initAllTables`传递`self_txn`，读路径仍仅注册表定义。
- [134–156行](../../idb/edadb_idb_init.cpp#L134)：在`create`计时作用域内执行`BEGIN → initAllTables(true, false) → COMMIT`。各root建表不自行开事务，子表及P1索引也处于同一schema事务。
- 建表或提交返回失败时尝试ROLLBACK并返回失败。减少重复提交，不改变表结构。

## P4：一次提交全部design数据

- [def_write_edadb.cpp:71](../../../manager/builder/def_builder/def_write_edadb.cpp#L71)：在`write`计时作用域内开启design事务。
- [80–108行](../../../manager/builder/def_builder/def_write_edadb.cpp#L80)：接收每个writer的返回值，不再忽略失败。
- [110–127行](../../../manager/builder/def_builder/def_write_edadb.cpp#L110)：writer失败则回滚；全部成功才提交，提交失败也尝试回滚。
- [245行起](../../../manager/builder/def_builder/def_write_edadb.cpp#L245)：15个root的`insertObject/insertVector`统一传`false`，使用外层事务；保留原有批量Shadow、对象遍历和写入顺序。

```text
init   ：连接、映射初始化
create ：BEGIN → 全部CREATE TABLE/INDEX → COMMIT
write  ：BEGIN → 全部root转换、INSERT、清理 → COMMIT
read   ：原有查询和对象恢复，不变
```

两者是**两个事务，不是一个事务**。数据写入失败后，已提交的schema仍存在，但本次design数据整体回滚。上述失败处理针对返回值错误，未新增C++异常捕获或异常安全事务守卫。

## 计时与验收

### OFF / ON是什么意思？

这是**我们新增的阶段计时开关**，不是SQLite配置，也不是优化开关。两组使用同一个Release二进制，P1/P3/P4优化始终启用。

| 组别 | 环境变量 | 测量内容 | 用途 |
| --- | --- | --- | --- |
| OFF | `EDADB_STAGE_TIMING=0` | 不执行C++阶段取时、不输出EDADB_STAGE；Tcl仍计完整读写命令时间 | 正式整体性能参照 |
| ON | `EDADB_STAGE_TIMING=1` | 保留Tcl完整命令计时，另用C++计时器统计init/create/write/read/other/command | 分析时间分布，检查新增计时的扰动 |

**并非必须每次同时测ON/OFF。** 拆阶段需要在边界读时钟，所以ON是当前主实验；OFF只是辅助检查计时器是否明显扰动运行。无需把两组都列成业务性能结果。详细解释及读写分开的结论见[结果报告](optimization-results.md)。

OFF仍有开关判断，不等于删除了计时代码。ON/OFF差值包含运行波动，不能全部认定为计时器成本。原细粒度`EDADB_ENABLE_PROFILING`及SQL trace在两组中都关闭。

`cold/warm`是另一个独立维度：cold对输入文件请求OS页缓存驱逐（提示，不保证完全冷）；warm预读输入文件。实际有OFF-cold、OFF-warm、ON-cold、ON-warm四组，**每组先预热1次、不计入统计，再正式运行5次**；cold预热后每个样本仍重新执行缓存驱逐。这里的预热指试跑，不代表把cold变成warm。

- `create`和`write`仍分别包含自己的BEGIN/COMMIT；不改变阶段口径，不额外扣除提交时间。
- [上一轮P1结果](parent-index-results.md)保留为修改前证据，不冒充P3/P4结果。
- [事务测试](../../../../../scripts/edadb/performance/p3-p4/test_transactions.cpp)移植自core历史`6d3718f:test/DbFacadeTransactions.cpp`：DDL错误后父子表均回滚；跨root重复主键失败后全部数据回滚；成功提交可完整读取。测试使用当前P1 core API，不切换子仓库。
- 这项测试验证core事务契约，不等于已覆盖adapter所有失败路径。最终阶段验收还应包含adapter故障注入、完整DEF回归及分阶段性能复测。

从仓库根目录执行事务测试：

```bash
mkdir -p /tmp/iedadb_p3p4
g++-10 -std=c++17 -O3 -DEDADB_ENABLE_PROFILING=0 \
  -DEDADB_TEST_OUTPUT_DIR='"/tmp/iedadb_p3p4"' \
  -Isrc/database/edadb/core/include -Isrc/database/edadb/core/test \
  scripts/edadb/performance/p3-p4/test_transactions.cpp -lsqlite3 \
  -o /tmp/iedadb_p3p4/test_transactions
/tmp/iedadb_p3p4/test_transactions
```

## 本轮验证结果

**后续正式实验已完成：** 见[P1+P3+P4结果](optimization-results.md)。OFF/ON各cold/warm正式5次，24个含预热样本严格DEF及逻辑数据库对照通过。下面保留实现阶段的smoke记录，避免混淆两次实验。

- Release完整iEDA重新构建通过（`-j40`）；事务测试退出码0。测试主动制造DDL/重复主键错误，所以日志中的预期SQL错误不表示测试失败。
- filler cold/warm各1次：严格DEF diff、integrity_check、foreign_key_check均通过；两份数据库的完整SQL dump与上一轮P1数据库相同，表、索引及数据未改变。
- 日志及生成物：`/tmp/iedadb_p3p4/`；构建日志：`/tmp/iedadb_p3p4_build.log`。
- 这次是功能smoke，不是正式性能复测；不使用各1次的耗时声明加速比，也不覆盖已保存的P1结果。
- 后续已补齐代表性adapter故障注入和15/15用例回归，见[功能验收](acceptance.md)。累计版本正式性能测试已完成；两项事务优化分别测量及其他故障类型不在本次验收覆盖内。
