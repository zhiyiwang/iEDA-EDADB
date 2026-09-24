# iEDA+EDADB性能文档

会议入口：[Demo版本说明](demo.md)，对应`demo/20260924`分支。沿用现有Demo分支约定，不设置milestone，不代表完整验收通过。

## 阅读顺序

1. [优化实验结果](optimization-results.md)：接入前原生iEDA `007435241`与当前EDADB已同环境复测；读写分开，init/create单列。
   原生DEF历史时间差异与同环境复测方法见[基线核查](baseline-verification.md)；完整命令始终使用原Tcl计时，不能与C++内部阶段混算。
2. [分段计时方法](stage-timing.md)：计时开关、起止位置、包含与排除的工作、代码位置及运行命令。
   [功能验收报告](acceptance.md)：15/15用例、adapter中途失败回滚、同进程重试及审计。
3. [事务合并实现](transaction-batching.md)与[代码变化](optimization-code-changes.md)：核对具体改动及验证范围。
4. [仅父键索引版本结果](parent-index-results.md)与[历史实验记录](optimization-history.md)：用于追溯，不将中间优化版本当作未优化基线。

## 当前结果与限制

- 当前版本包含父键索引、建表事务合并、design写事务合并，并移除参考DEF扫描；不包含后续流式Shadow和叶子批量读取。
- 原始iEDA已重新Release构建并完成热缓存复测：完整读取47.484 ms、写入6.855 ms；当前EDADB为106.132 ms、371.532 ms，均为5次中位数。
- 当前EDADB完整读、写分别耗时为原生DEF的2.24倍、54.20倍。此次不使用历史profiling版本代替原始iEDA。
- 同批独立阶段观测组：读data均值102.967 ms；写init 0.181 ms、create 89.754 ms、data 279.471 ms。data包含数据事务，不含建表。分段不得与OFF中位数相减。
- ON/OFF仅表示新增阶段计时器开启/关闭，优化逻辑相同；OFF作为计时扰动对照，不是未优化基线。
- 本轮6次跨版本原生DEF严格比较、12次EDADB roundtrip、12个数据库完整性/FK检查通过；原始时间和审计见结果文档。更早core 27/27、诊断回归15/15不冒充本轮回归。
- 总表补入已有未优化EDADB完整读11931.378 ms、写2277.177 ms，并区分同批收益与跨批比较；来源为`62504bd9d/be6bbdd`，不冒充配对milestone的实测值。无阶段数据则不列。
- 两仓库开发分支为`edadb-performance-optimization-dev`；父仓库以`4ba761281`为起点，Demo提交保存累计修改；core固定`1c4857c`，无本次新增修改。

## 入口与维护

- [性能运行脚本](../../../../../scripts/edadb/performance/run.sh)、[Tcl计时](../../../../../scripts/edadb/performance/benchmark.tcl)、[计时器测试](../../../../../scripts/edadb/performance/p1-stage-timing/test_stage_timing.cpp)。
- [历史原始样本](../../../../../scripts/edadb/performance/p_optimization_samples.tsv)、[集成文档总入口](../README.md)。
- 正文集中在本目录；使用相对链接，不复制源码、数据库或大日志。
- 结果使用描述性名称；历史commit、原始TSV标识和数据路径保留原样以便审计。内部编号仅留在历史代码追溯资料中。
