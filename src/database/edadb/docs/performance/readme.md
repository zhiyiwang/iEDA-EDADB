# iEDA+EDADB性能文档

## 已交付milestone：P1/P3/P4/P5

1. [demo.md](demo.md)：交付范围与四份文档阅读入口。
2. [optimization-results.md](optimization-results.md)：原始iEDA、未优化EDADB、优化后EDADB对比；历史各P收益单列。
3. [transaction-batching.md](transaction-batching.md)：索引、事务及流式Shadow实现。
4. [stage-timing.md](stage-timing.md)：交付版的计时边界。

配对标签 `milestone/ieda-edadb-optimized-v1` 指向iEDA `27d1ceefd` / core `494ce79`。下面profiling提交不移动该标签。

## 新的profiling：源码不含P2

1. [结论](sqlite-cost-takeaway.md)。
2. [读写结果及证据](sqlite-cost-results.md)。
3. [计划与限制](sqlite-cost-plan.md)。
4. [测试实现、运行和原始结果](../../test/sqlite_cost/readme.md)。

本轮只增加观测，不改变存储格式、SQL和对象恢复；P2实验已移出工作树。完整时间使用OFF中位数，内部阶段使用ON均值，COMMIT是data子项。历史批次不混算，未启用批量读取。

## 配对版本与历史保留

- 新配对标签：`milestone/ieda-edadb-profiled-v2`，iEDA指向本轮profiling提交，core仍为`494ce79`；不是新增性能优化。
- iEDA的`edadb-performance-optimization`与`edadb-performance-optimization-dev`同步到新提交；core的`performance/optimization`与`edadb-performance-optimization-dev`同步到`494ce79`。
- 原含P2的历史实现用两仓库同名标签`archive/ieda-edadb-opt-with-p2-before-profiled-v2`保留：iEDA `53aa2e52a` / core `ad0f820`。这是分支指向替换，不是将历史P2合并到现行代码。
- 旧`milestone/ieda-edadb-optimized-v1`不移动；新v2的性能证据只使用本轮无P2源码复测。
