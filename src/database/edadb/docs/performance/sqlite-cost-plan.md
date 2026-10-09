# P1/P3/P4/P5实现的profiling计划

## 范围

在 `edadb-performance-optimization-dev` 上，仅保留P1父键索引、P3/P4事务合并、P5流式Shadow及观测代码。core固定为 `494ce79`，P2已从core和adapter源码移除，不是仅关闭开关。milestone不移动。

分别回答：读的重复查询、索引定位、取值与对象恢复花费在哪里；写的插入、索引维护与提交等待花费在哪里。不能把Net时间全部叫N+1，不能把step样本全部叫FK检查。

## 测量

- 数据：Sky130 GCD filler 726,155 B；同源routed压力fixture 6,305,169 B，后者复制1,000个Net路由，不代表新的物理有效设计。
- 隔离目录完整Release构建，O3/LTO；trace和core高频计时OFF。编译、功能验证可并行；正式测量串行、固定CPU2，热OS缓存、新进程/连接。
- 正式时间：计时器OFF/ON各预热1次、正式5次，交替顺序。OFF/ON不是SQLite配置。init/create/data单列，数据COMMIT包含在write data中。
- 独立计数：SQL/API次数、VM步骤、查询计划、页缓存；不将计数探针耗时作为性能结果。
- 独立采样：perf三轮，匹配SQLite Build-ID和符号；I/O及CPU/墙钟另跑，按阶段过滤。CPU比例不能乘正式墙钟得到函数独占时间。
- 不增加逐行时钟，不修改SQL、schema、SQLite同步或FK设置。创建/写入/读取与正确性校验边界保持原定义。

## 验收与记录

1. core工作树干净，源码不存在批读API，编译配置和生产源码/测试脚本哈希保存；未提交生产差异保存为 `production.patch`。
2. 计时器、探针精确计数和脚本隔离/防覆盖测试通过；15用例回归及插入/转换失败回滚、同进程重试通过。
3. 每次读写后严格DEF、DB完整性和FK检查；逻辑dump与冻结流式Shadow结果相同。
4. 重新计算全部统计，验证每组5次、阶段加总、子项不重复；perf无丢样。新结果目录必须不存在，收集程序拒绝覆盖旧批次。
5. 只提交观测代码、脚本、文档、小型原始TSV/JSON证据；不提交P2、生成DB/DEF、构建目录或大perf文件。

## 尚不承诺

本轮验证无P2的profiling-only版本；尚未做“完全无新观测代码”的同批构建对照，也未覆盖更多真实大设计、cold或并发。计时OFF/ON差异不是所有观测成本的严格上界。下一步是否做P2，以[结果](sqlite-cost-results.md)判断，不预设结论。
