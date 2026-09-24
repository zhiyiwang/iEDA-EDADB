# 流式Root Shadow：实现与验证

## 修改范围

对照为`7f19a4650`对应的事务优化版本；两侧core均为`1c4857c`。继续在`edadb-performance-optimization-dev`修改，不移动`demo/20260924`。

只修改[写入adapter](../../../manager/builder/def_builder/def_write_edadb.cpp)的Instance、Pin、SpecialNet、Net四个writer：

```text
原实现：转换全部root → 保留整组Shadow → insertVector → 统一释放
新实现：创建一个InsertOp → 逐root转换、insert、析构 → 销毁InsertOp
```

当前`insertVector`内部已经复用操作器循环insert，新实现保留这一复用方式，不增加SQL批次或事务。不改schema、绑定API、读路径及计时边界；整个design仍只有一次数据事务。

目标是缩短Shadow存活范围：临时活跃数据从一类root的总量降至最大单个root的规模。原始iDB及SQLite缓存仍存在；局部Shadow中的字符串/vector仍会分配，释放也不保证立即归还OS。

## 功能验证

正确性依据：`insert()`同步完成当前root及其子对象写入后，局部Shadow才析构；操作器在循环外复用，原有序号及外层design事务不变。故障返回仍交给外层回滚。以下通过输出一致性、实际失败路径及SQL次数验证这些约束，而不只检查程序是否退出成功。

- 15/15原回归通过：10个严格DEF比较、5个按原有根对象顺序规范化后比较；所有DB完整性/FK检查通过。没有放宽原断言。
- 正式Release程序的第二条Net INSERT触发真实SQLite错误：前面已有Instance及Net被adapter整体回滚，表/索引保留，同进程重试与严格DEF恢复通过。
- 转换失败使用单独测试二进制：仅在临时源码副本中，将第二个Net传给真实`toShadow()`的指针改为null，触发现有失败返回。失败前已插入2,604个Instance及1个Net；adapter回滚、Tcl错误传播、同进程重试和严格DEF恢复均通过。
- 转换故障代码不进入正式源码或性能二进制；这两类注入不覆盖所有异常、掉电或COMMIT失败。

## 性能方法

- 使用保留的旧Release二进制和新Release二进制；GCC 10、`-O3`，SQL trace/core profiling关闭。
- 两侧都开启现有粗粒度阶段计时：报告write/read data及完整Tcl命令；不是与历史OFF值拼接。
- filler与routed压力输入；后者从iRT DEF复制最长Net路由body，新增1,000个Net，是对象规模压力fixture，不是物理设计有效性用例。
- 热缓存、CPU 2、串行；每版本预热1次，正式5次，轮换版本先后，每个样本前等待5秒。所有编译及并行功能测试先结束。
- init/create/data分别记录；数据write包含转换、插入、提交、清理，read包含查询与iDB恢复。
- `/usr/bin/time`记录KiB单位的**整个写入或读取进程峰值RSS**，包含LEF及输入准备，不是某个writer的独占峰值。
- 每个样本在计时外严格比较DEF、SQLite完整性/FK及完整SQL dump；另起诊断进程用preload计数prepare、INSERT、BEGIN/COMMIT，不将其时间用于性能结果。

## 复现入口

先在改代码之前保存旧二进制，再编译新代码。所有输出目录必须是新目录：

```bash
python3 src/database/edadb/test/stream_shadow/run.py \
  /path/to/baseline-iEDA bin-release/iEDA /tmp/stream-shadow-repeat

python3 src/database/edadb/test/build_diagnostic.py /tmp/stream-shadow-conversion --conversion-fault
python3 src/database/edadb/test/run_adapter_fault.py \
  /tmp/stream-shadow-conversion/iEDA /tmp/stream-shadow-conversion-check --conversion-fault
```

[运行与数据生成](../../test/stream_shadow/run.py)、[独立SQL计数](../../test/stream_shadow/count_sql.cpp)、[功能测试方法](acceptance.md)。本轮输出目录为`/tmp/iedadb_stream_shadow_current`。

## 结果：写入

全部为热缓存5次中位数；时间单位ms。输入分别为filler **726,155 B**、routed压力fixture **6,305,169 B**。

| 数据集 | 阶段 | 整组Shadow基线 | 流式Shadow | 耗时变化 |
| --- | --- | ---: | ---: | ---: |
| filler | init | 0.179 | 0.180 | 约+0.001 ms |
| filler | create | 90.535 | 83.288 | −8.00% |
| filler | **write data** | **317.415** | **317.389** | **−0.01%** |
| filler | 完整write命令 | 408.674 | 400.614 | −1.97% |
| routed压力 | init | 0.185 | 0.190 | 约+0.005 ms |
| routed压力 | create | 88.673 | 90.862 | +2.47% |
| routed压力 | **write data** | **2858.198** | **2876.312** | **+0.63%** |
| routed压力 | 完整write命令 | 2949.917 | 3018.947 | +2.34% |

建表逻辑未改，create变化不作为流式Shadow的收益；各阶段中位数不能保证相加等于完整命令中位数。

| 数据集 | 基线写进程峰值RSS（KiB） | 流式写进程峰值RSS（KiB） | 降低 |
| --- | ---: | ---: | --- |
| filler | 273,564 | 273,512 | 52 KiB，约0.02%，无明显收益 |
| routed压力 | **302,304** | **273,432** | **28.20 MiB，9.55%** |

**写入takeaway：在较大routing对象图上，以write data约0.63%的观测增加换取28.20 MiB峰值内存下降；小设计时间基本不变。** 两个数据集的write data及完整write中位数均未出现超过5%的回退，支持保留本次内存优化，但不宣称它加速了写入或适用于所有设计。

## 结果：读取回归

| 数据集 | 阶段（ms） | 整组Shadow基线 | 流式Shadow | 耗时变化 |
| --- | --- | ---: | ---: | ---: |
| filler | read data | 137.362 | 131.406 | −4.34% |
| filler | 完整read命令 | 141.512 | 135.813 | −4.03% |
| routed压力 | read data | 1678.517 | 1689.464 | +0.65% |
| routed压力 | 完整read命令 | 1697.986 | 1700.133 | +0.13% |

**读取takeaway：未改读路径，且两版数据库SQL dump一致；读取变化仅作为回归观测，不归为读取优化收益。** 本轮当前环境结果不与之前其他批次的约106 ms读取直接相减。

## SQL次数是否增加？

独立诊断中，两版各项计数完全一致：

| 输入 | 可见sqlite3_prepare_v2调用 | INSERT执行 | BEGIN | COMMIT | ROLLBACK |
| --- | ---: | ---: | ---: | ---: | ---: |
| filler，两版各自 | 26 | 34,358 | 2 | 2 | 0 |
| routed压力，两版各自 | 26 | 449,214 | 2 | 2 | 0 |

两个事务分别用于schema与design。计数是测试shim拦截的C API调用，不表示SQLite内部所有解析/VM操作；没有把计数器放入正式计时样本。

这验证了本次多次调用同一操作器的`insert()`没有增加prepare、INSERT或提交次数。改动只是Shadow的存活范围和转换/插入的交错顺序，不是新的SQL批量算法。

首次计数运行发现`LD_PRELOAD`也加载进`time`工具，额外输出全零记录，审计按预期拒绝；修正为只对iEDA子进程加载后，仅重跑诊断计数。24个已完成样本未重测、未挑选；`--finish`会核验manifest、全部保存样本和数据库，再生成统计。

## 审计与文件

- [正式原始计时](../../test/stream_shadow/results/samples.tsv)：280条指标记录，包含两数据集×两版本×5轮×读写；ms或KiB由metric名称标识。
- [均值、中位数及范围](../../test/stream_shadow/results/summary.tsv)、[输入/二进制及writer哈希](../../test/stream_shadow/results/manifest.json)。
- [性能正确性与SQL计数](../../test/stream_shadow/results/audit.json)：包含预热的24个样本均通过严格DEF、完整性/FK及DB dump一致性检查。
- [15用例回归](../../test/stream_shadow/results/regression.json)、[插入失败](../../test/stream_shadow/results/insert-fault.json)、[转换失败](../../test/stream_shadow/results/conversion-fault.json)。
- 全部生产改动在[Instance](../../../manager/builder/def_builder/def_write_edadb.cpp#L391)、[Pin](../../../manager/builder/def_builder/def_write_edadb.cpp#L429)、[SpecialNet](../../../manager/builder/def_builder/def_write_edadb.cpp#L696)、[Net](../../../manager/builder/def_builder/def_write_edadb.cpp#L735)四个writer；core未改，Demo分支未移动。实现与测试结果一同保存于开发分支，不包含生成数据库、构建目录或临时二进制。
