# Milestone阶段计时

本文解释[三者对比与逐P结果](optimization-results.md)使用的计时口径，不介绍后续profiling的root细项或独立COMMIT计时。

## 1. 开关与实现原理

- `EDADB_STAGE_TIMING=1`（ON）启用我们自己的C++阶段计时；未设置或为0（OFF）时关闭。Tcl完整命令计时仍存在。
- **结果主表的原始/未优化时间来自OFF记录，已优化时间来自stream_shadow的ON记录；均为5次中位数。** 总表是已有跨批次概览，不能称为同批严格净收益。P5前后两侧都ON；P1/P3/P4历史增量用OFF，P3/P4内部阶段用ON中位数，具体来源在结果末尾列明。
- Session在命令入口记录开始时间，析构时计算命令耗时；ScopedTimer进入init/create/data作用域时保存 `steady_clock::now()`，退出自动计算终点减起点并累计。
- Session不是数据库事务；统计在计时结束后输出，不逐行计时或打印。ns是输出单位，不代表纳秒精度。
- core细粒度profiling和SQL trace关闭。P2及其读取开关不在milestone中。

## 2. 读取

```text
Tcl开始计时
    C++ Session开始
        init：进入记时 → helper/连接/表映射 → 退出累计
        data：进入记时 → 查询/取列/恢复iDB/清理 → 退出累计
    Session结束计时并输出
Tcl结束计时
```

read data排除init，包含完整对象恢复，不是纯SQL时间。读取不建表（create=0），不再扫描参考DEF。

## 3. 写入

```text
Tcl开始计时
    C++ Session开始
        init：连接及主键映射
        create：schema BEGIN → 全部建表和索引 → COMMIT
        data：design BEGIN → Shadow转换/INSERT/清理 → COMMIT
    Session结束计时并输出
Tcl结束计时
```

每个阶段均在进入/退出作用域取时。write data不含init/create，但包含数据提交。**本批没有单独的BEGIN/COMMIT毫秒数，不列或推算该分项。**

## 4. 加总和排除范围

- 同一样本：`Session command = init + create + data + other`。
- `other`为Session内未被三个阶段覆盖的余量，不是adapter独占耗时。
- `Tcl完整时间 − Session command`是该样本的外围余量，包含计时报表和命令包装，不能当成某个函数耗时。
- 各阶段中位数不能保证相加等于完整命令中位数；不要用表格的中位数相减来推算未测阶段。
- LEF加载、写前输入准备、读后校验均在命令计时外。RSS单独由time工具记录，包含整个进程。
- 本批数据在服务器仓库 `src/database/edadb/test/stream_shadow/results/`。没有逐字段、逐对象或root级耗时，不用后续DETAIL补写。

## 5. 固定标签的代码位置

所有路径相对服务器仓库；行号以 `milestone/ieda-edadb-optimized-v1` 的 `git show` 内容为准：

| 位置 | 源文件及行号 |
| --- | --- |
| Session / ScopedTimer | `src/database/edadb/idb/edadb_stage_timing.h:24` / `:68` |
| 写init / create | `src/database/edadb/idb/edadb_idb_init.cpp:124` / `:134` |
| write data | `src/database/manager/builder/def_builder/def_write_edadb.cpp:71` |
| 读init / data | `src/database/manager/builder/def_builder/def_read_edadb.cpp:61` / `:81` |

Tcl入口为 `scripts/edadb/performance/benchmark.tcl`；同批运行脚本为 `src/database/edadb/test/stream_shadow/run.py`。当前工作区另有新观测代码，不能用其行号或功能替代标签版本。
