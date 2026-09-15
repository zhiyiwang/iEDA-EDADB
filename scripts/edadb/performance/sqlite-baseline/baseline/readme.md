# 五路线基线：计划与运行

## 目标与数据

测量各路线的读写阶段耗时和同条件净增量，不将路线差值解释为某个内部模块的独立耗时。
实现与计时位置见[benchmark/implementation.md](/home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/performance/sqlite-baseline/benchmark/implementation.md)，执行证据见[baseline/results.md](/home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/performance/sqlite-baseline/baseline/results.md)。

固定单表component，两个TEXT、五个INTEGER和一个BIGINT，无显式PK/FK/二级索引：

```cpp
struct ComponentRecord {
    std::string name, master_name;
    int32_t source, status, orient, x, y;
    int64_t record_order;
};
```

- 规模：1,000、10,000、100,000、1,000,000条；正确性另测0、1、8条。
- 第index条：name=U加7位补零序号，master_name=bench_cell；source=DIST(2)、status=PLACED(3)、orient=N(1)。
- x=(index%1000)*100，y=(index/1000)*100，record_order=index。DEF以COMPONENT记录顺序表达record_order。
- 输入vector、最小LEF、canonical DEF均在计时外准备；文件路径与哈希记录到datasets.json。
- 选择标量COMPONENT以比较基本存取；不代表完整DEF设计或复杂嵌套schema。

## 实验矩阵

| 路线 | 实现与结果对象 | 配置 | write数据阶段 | read数据阶段 |
| --- | --- | --- | --- | --- |
| text | C++文本↔复用Record | 文件 | 输出8字段至flush完成 | 解析8字段并消费 |
| native | DefWrite/DefRead↔完整iDB | 文件 | native_save封装全程 | native_load封装全程 |
| sqlite | SQLite C API↔复用Record | A、A-no-journal、B-default、B-batch | prepare→bind/step/reset循环→finalize | prepare→step/column/消费→finalize |
| edadb | EDADB C++ API↔复用Record | 同sqlite | 构造op→insert循环→销毁op | 构造reader→readNext/消费→销毁reader |
| adapter | iEDA adapter↔完整iDB | 保留应用配置与事务 | writeChip2Edadb，含内部提交 | 仅createDbByEdadb，不扫描参考DEF |

参数仅在[sqlite_params/config.md](/home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/performance/sqlite-baseline/sqlite_params/config.md)维护：
- A：内存库低文件I/O参考；A-no-journal只改变日志设置，不预设其最快。
- B-default：文件库实际默认参数，逐条隐式提交；B-batch只加外层数据BEGIN/COMMIT。
- B-default首轮限制1,000条；其余配置覆盖全部规模。
- adapter不参与A/B调参；单COMPONENT family写事务以B-batch作为粒度参考。

## 计时与正确性

- 使用steady_clock墙钟差，单位ms；每阶段仅记录边界，不做逐行计时。
- 数据库init、create、begin、data、commit、close分别记录。create包含建表事务，不进入读写比率。
- batch的write_complete=begin+data+commit；autocommit的write_complete=data，隐式提交已在其中。
- B-default与B-batch必须比较write_complete，不能只比data；不重复累加complete及其子阶段。
- native文件open/close在API内；adapter init包含建表，不能称为纯create。字段与N/A规则见实现说明。
- direct/text读取复用一个Record，不保存结果vector；两个字符串仍复制，不能声称零分配。
- 性能读取只消费整数和字符串长度摘要；独立正确性进程逐字段检查，同长度字符串篡改必须被拒绝。
- native/adapter计时后提取字段并验证；正确性输出DEF严格比较。完整校验不进入正式性能样本。
- 正常返回值检查和读取消费属于被测实现；不声称计时、摘要或系统调度没有扰动。

## 运行与比较

- Release -O3/-DNDEBUG，关闭SQL trace；记录二进制、源码、依赖及实际PRAGMA。
- 正确性可并发，性能串行；1次预热、5次正式采样，样本前和写读之间各等待5秒。
- memory：同连接first-read和repeat；disk：预读文件后的os-warm、新连接。不是设备冷缓存测试。
- disk os-cold/repeat与计时开关扰动对照尚未纳入本批次；不得作为已完成结果。
- 输出每次原始日志、samples.tsv、summary.tsv及report.md；统计平均值、中位数、min/max。
- 同配置EDADB减SQLite为整条框架路径净增量；text/native/adapter对象构建不同，差值不能直接拆成traversal或重建时间。
- write即使不含外层COMMIT，仍可能包含事务管理、日志和文件写入，不是纯格式转换时间。
- 每条成本=ms×1000/N（µs/条）；文件bytes÷耗时只是表观吞吐，不等于磁盘带宽。小于样本波动的差异不作确定性归因。

## 编译与运行

先按[共用构建说明](/home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/performance/sqlite-baseline/benchmark/implementation.md)生成Release可执行文件。以下从仓库根目录执行，输出目录必须不存在：

```bash
cd /home/zhiyiwang/cs/arch/eda/iEDA-EDADB
python3 scripts/edadb/performance/sqlite-baseline/baseline/run.py \
  --binary /tmp/iedadb_benchmark_build/stream_benchmark \
  --out /tmp/iedadb_baseline_full \
  --counts 1000 10000 100000 1000000 --runs 5 --settle 5
python3 scripts/edadb/performance/sqlite-baseline/baseline/audit.py /tmp/iedadb_baseline_full
```

快速验证：将输出目录换成新的目录，使用`--counts 100 --runs 1 --settle 0`；仍覆盖五路线及0/1/8条正确性检查，不用于正式性能结论。使用`--routes`可只选择部分路线；输入的原生DEF准备仍在计时外。

输出：`samples.tsv`保存逐次阶段时间，`summary.tsv`和`report.md`保存统计，`checks.json`/`audit.json`保存正确性与审计。`datasets.json`记录输入大小/哈希，`manifest.json`和`source/`记录版本、硬件、二进制哈希与源码快照。先查正确性，再查原始样本与统计，最后读[结果分析](/home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/performance/sqlite-baseline/baseline/results.md)。

## 接续事项

- adapter的独立建表、内部COMMIT、SQL读取和对象重建尚未分别计时；不得将总差值称为单一模块成本。
- 原生与adapter恢复完整iDB，direct只复用Record；adapter schema、主键、排序及FK设置也不同，需先对齐工作量再解释差距。
- NULL／参数清理对照已完成，见[SQLite与EDADB对照结果](/home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/performance/sqlite-baseline/sqlite_vs_edadb/results.md)；不把其他分支优化结果混入本基线。
- 未测disk OS-cold、等价文本fsync、硬件指令计数；如需开展，先定义边界和扰动控制，不自动修改生产代码。
