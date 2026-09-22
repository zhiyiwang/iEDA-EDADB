# 配对性能基线milestone

两个仓库使用同名annotated tag：`milestone/storage-performance-baseline-v1`。

| 仓库 | 标签指向 |
| --- | --- |
| iEDA-EDADB | 包含本文的基线收尾提交；以标签解析出的commit为准 |
| EDADB | `d6656f08c9ae3123ce5272a344ea3d5515159fe7` |

iEDA提交中的`src/database/edadb/core` gitlink必须与EDADB同名标签一致。两份标签说明记录双方完整SHA；不要移动标签，后续修订另建版本。

## 交付范围

- [整体汇报](storage/report.md)及[实验导航](storage/readme.md)。
- 已冻结的五路线baseline、SQLite/EDADB对齐、PK/FK及单表索引实验。
- 本次冻结SQLite与文本归因、SELECT补充、源码解释、运行脚本和审计程序。数字只在[结果文档](storage/sqlite-vs-text/docs/results.md)维护。
- adapter生产逻辑不包含历史P系列优化；core为Release配置修正及条件SQL trace基线。不是优化后的产品milestone。
- 不包含生成数据、DB、二进制、构建目录、完整日志；本地后续优化建议不纳入提交。

## 验收证据与限制

- 归因实验：60组正确性、120条计时、24个统计组、10次独立计数，审计PASS。
- SELECT补充：16组正确性、20条计时、4次计数，审计PASS；两种SELECT字节码一致。
- 独立perf：24组采样＋24组控制，审计PASS、无丢失样本；不是内部函数独占时间测量。
- 收尾复核：重新Release -O3构建三个测试目标；1,000条功能smoke通过（主实验60组检查、SELECT补充16组检查），主批causal审计重新PASS。smoke仅检查可运行性，不作为新性能结论；证据在`/tmp/iedadb_milestone_smoke`及`/tmp/iedadb_milestone_select_smoke`。
- 当前benchmark源码与SELECT补充归档一致；相对归因主批，增加可选SELECT参数及供补充程序复用的main编译保护，CMake增加补充目标。主批数字仍对应其归档版本，不冒称全部来自收尾提交的一次重跑。
- 原始证据及哈希入口见结果文档。`/tmp`不是永久存储。已有轻量交付包`/tmp/performance-test-package-20260921_084945.tar.gz`早于本次文档收尾，不替代本标签源码。
- 未全面验证cold条件、掉电持久性或完整adapter成本归因；step-only不是完整读取加速。不会用新样本替换历史结果。

## 获取配对代码

在已clone、工作区干净的iEDA仓库执行：

```bash
git fetch origin --tags
git switch --detach milestone/storage-performance-baseline-v1
git submodule update --init --recursive
git -C src/database/edadb/core fetch origin --tags
git ls-tree HEAD src/database/edadb/core
git -C src/database/edadb/core rev-parse 'milestone/storage-performance-baseline-v1^{commit}'
```

最后两项必须给出相同core SHA。继续开发另建分支，不覆盖已有milestone。
