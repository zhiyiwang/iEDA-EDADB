# 会议Demo：事务合并与原始DEF性能对照

这是阶段性会议演示版本，不是milestone。沿用仓库已有的`demo/YYYYMMDD`分支约定，本次会议入口为`demo/20260924`。

## 阅读顺序

1. [结果报告](optimization-results.md)：原始iEDA、未优化EDADB、当前EDADB完整读写比较；随后分别解释读取和写入阶段。
2. [事务合并](transaction-batching.md)：建表与design数据各使用一个事务。
3. [计时实现](stage-timing.md)与[源码改动](optimization-code-changes.md)：计时范围、实现及代码位置。

## 版本与范围

- 父仓库分支：`edadb-performance-optimization-dev`；本次提交标题以`demo:`开头，提交本身记录完整源码和结果。
- `demo/20260924`指向包含逐项优化收益分析的新提交，替代`7ae1f46be`作为本次会议入口。旧提交保留在历史中，不改写已推送历史，也不创建milestone标签。
- EDADB core固定为`1c4857c485a172d7ffb8c8fc615f3046fbb68a0f`，沿用已有父键索引实现；本次不修改core，不人为增加空提交。
- 新增建表事务合并、design写事务合并、默认关闭的阶段计时；移除参考DEF扫描。
- 保存必要测试源码、脚本、文档、小型原始时间及审计文件；不保存构建目录、二进制、生成数据库或大日志。

## 验证与限制

- Release构建、事务契约测试及GCD filler严格DEF对比通过。
- 原始iEDA复测批次：6次跨版本DEF比较、12次EDADB roundtrip、12个数据库完整性/FK检查通过。
- 原始iEDA完整读/写中位数为47.484/6.855 ms；当前EDADB为106.132/371.532 ms，热缓存正式5次。阶段时间及未优化版本的批次区别见结果报告。
- 后续功能验收已完成：adapter注入失败后回滚、同进程重试及15/15用例回归通过，详见[验收报告](acceptance.md)。该补充尚未更新会议分支。
- 两项事务优化各自的当前版本独立收益仍未补测；未验证的故障类型见验收报告，不宣称所有异常场景安全。
