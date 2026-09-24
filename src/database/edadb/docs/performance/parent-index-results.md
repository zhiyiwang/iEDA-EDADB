# 仅父键索引分段计时：验证与结果

## 版本、输入与方法

- iEDA：`edadb-performance-optimization-dev @4ba761281`，加当前工作区的粗粒度计时及删除参考DEF扫描修改；core同名分支`1c4857c`，未改源码。不是原始仅父键索引逐字不变版本。
- Release `-O3`；旧细粒度profiling和SQL trace关闭。编译`-j40`；正确性用例可并行，性能样本严格串行且不与编译重叠。
- 输入：Sky130 GCD `iPL_filler_result.def`，726,155 B；SHA256：`b2a8ade6f7e74f620491f56914a34d8fb89dd3efe957e4dc3ec4a42d50104c5e`。
- 正式二进制SHA256：`3ce7ad02e6a9b9f2af4fc5f8128455ed6e1e75014ca1c9e3aa6deab2bdcc1108`。
- 同一二进制先OFF后ON；每种开关均cold/warm各预热1次、正式5次，每个样本前等待5秒。LEF解析和正确性检查均在计时外。
- Tcl测完整命令；C++ Session测adapter入口，各阶段用作用域构造/析构累计。写数据含转换及原有数据事务，读数据含对象恢复；init/create独立统计。详见[计时实现与源码位置](stage-timing.md)。

## 正确性与验证

| 检查 | 结果 |
| --- | --- |
| 完整Release iEDA构建、独立计时器测试 | 通过；存在第三方已有编译警告 |
| EDADB core全套测试 | 27/27通过，串行耗时487.95 s |
| 正式Release的15个DEF用例 | DEF等价检查通过；原脚本的诊断日志断言因日志关闭而失败 |
| 独立诊断二进制完整回归 | 15/15通过，未跳过日志断言；仅重编读写adapter开启EDADB_OUTPUT_DEBUG，仍为-O3，不用于性能结果 |
| OFF/ON预热及正式样本 | 24/24严格DEF diff通过；SQLite integrity_check和foreign_key_check通过 |
| 不存在参考DEF的读取 | 通过，输出与native严格相同 |
| ON分项与OFF输出审计 | ON非负、阶段加other等于command；OFF不输出阶段记录 |

15用例回归沿用原脚本的规范化规则，部分用例人为扰动根对象顺序，**不能称为15个都严格文本相同**；filler性能样本的24次比较不使用规范化。原始日志保留在`/tmp/iedadb_p1_stage_validation/`。

## 性能结果

单位ms；每组5次，中位数；预热不参与统计。

## 完整命令：OFF性能与ON扰动

| 缓存 | 操作 | native OFF | EDADB OFF | EDADB/native耗时比 | EDADB ON | ON/OFF变化 |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| cold | read | 48.374 | 146.164 | 3.022 | 142.897 | -2.24% |
| cold | write | 6.858 | 1796.981 | 262.027 | 2131.196 | +18.60% |
| warm | read | 47.449 | 106.307 | 2.240 | 109.619 | +3.12% |
| warm | write | 6.818 | 1864.653 | 273.490 | 1779.858 | -4.55% |

## ON阶段分布

占比为各样本stage/command的平均值；不是中位数相加。

| 缓存 | 操作 | 阶段 | 均值ms | 中位数ms | 最小ms | 最大ms | 平均占比 |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| cold | write | init | 0.182 | 0.170 | 0.169 | 0.229 | 0.01% |
| cold | write | create | 1256.059 | 1249.095 | 1024.474 | 1398.460 | 57.96% |
| cold | write | write | 905.952 | 906.816 | 830.789 | 972.382 | 42.03% |
| cold | write | other | 0.001 | 0.001 | 0.001 | 0.001 | 0.00% |
| cold | write | command | 2162.193 | 2131.113 | 1996.877 | 2371.072 | 100.00% |
| cold | read | init | 3.526 | 0.870 | 0.721 | 14.414 | 1.56% |
| cold | read | create | 0.000 | 0.000 | 0.000 | 0.000 | 0.00% |
| cold | read | read | 166.565 | 139.316 | 135.216 | 241.766 | 98.44% |
| cold | read | other | 0.000 | 0.000 | 0.000 | 0.001 | 0.00% |
| cold | read | command | 170.091 | 140.211 | 135.937 | 256.180 | 100.00% |
| warm | write | init | 0.173 | 0.173 | 0.172 | 0.176 | 0.01% |
| warm | write | create | 1033.353 | 982.339 | 955.454 | 1248.484 | 56.51% |
| warm | write | write | 790.854 | 797.298 | 738.907 | 822.797 | 43.48% |
| warm | write | other | 0.001 | 0.001 | 0.000 | 0.001 | 0.00% |
| warm | write | command | 1824.381 | 1779.775 | 1721.418 | 2045.959 | 100.00% |
| warm | read | init | 0.418 | 0.421 | 0.404 | 0.425 | 0.40% |
| warm | read | create | 0.000 | 0.000 | 0.000 | 0.000 | 0.00% |
| warm | read | read | 105.188 | 106.392 | 98.469 | 109.319 | 99.60% |
| warm | read | other | 0.000 | 0.000 | 0.000 | 0.000 | 0.00% |
| warm | read | command | 105.606 | 106.814 | 98.874 | 109.739 | 100.00% |

审计：20个正式样本严格DEF相等，ON阶段非负且加总一致，OFF无阶段日志。
原始数据：[samples.tsv](../../../../../scripts/edadb/performance/p1-stage-timing/samples.tsv)；统计：[summary.tsv](../../../../../scripts/edadb/performance/p1-stage-timing/summary.tsv)。
write含内部事务及转换；read含对象组装；均不含init/create。
cold使用文件级缓存驱逐提示，不代表设备缓存完全冷却。ON/OFF差值包含批次噪声，不是计时器独占成本。

## 分析：读写分开

- **写入**：热组建表平均占command的56.51%，数据写入占43.48%。排除建表后的write中位数为797.298 ms，仍包含各root事务、转换和清理，不能称为纯SQLite插入时间。现有计时不能把其中的COMMIT、bind或Shadow成本进一步分开。
- **读取**：热组init仅0.418 ms（均值），read占command的99.60%；瓶颈位于读取/恢复组合阶段，不在连接初始化。尚未测出SQLite、对象遍历和adapter组装各自独占的时间。
- **与原生iEDA比较**：使用OFF完整命令同批对照，热读约为native的2.24倍时间，热写约273.49倍。数据阶段和native完整命令边界不同，只能辅助分析，不替代端到端比较。
- **Session外的工作**：热读Tcl中位数109.619 ms，C++ command中位数106.814 ms。入口外还包括builder的buildNet/buildBus/log等，以及报告输出；这两个中位数的差不是某个函数的独占计时。
- **与旧仅父键索引结果的区别**：本版移除了参考DEF扫描，不把新旧总时间差全部归因于索引或计时器；历史数据不混入本批统计。

## 计时扰动复核

首批冷写ON/OFF中位数变化+18.60%，超过拟定5%门槛；热写为−4.55%，读为−2.24%/+3.12%。为检验批次波动，补做5对冷写，奇数对OFF→ON、偶数对ON→OFF，仍串行，其他配置相同。

- 每对ON相对OFF：+18.62%、−19.95%、+1.80%、−5.19%、−1.00%；配对变化中位数−1.00%。
- 10个补充数据库均通过integrity_check，完整SQL dump摘要一致；检查在写入计时外执行。
- **结论：写入波动明显，当前数据不足以证明计时扰动稳定小于5%。** 不把负差解释为计时器加速，也不从阶段结果扣除猜测的计时成本。若要判断几个百分点的优化收益，需要更稳定环境及更多交替样本。
- 原始补充数据：[paired-write.tsv](../../../../../scripts/edadb/performance/p1-stage-timing/paired-write.tsv)；[审计结果](../../../../../scripts/edadb/performance/p1-stage-timing/audit.json)。

## 复现与文件

从仓库根目录运行；两个开关按顺序执行，不并行：

```bash
cmake --build build-release -j40 --target iEDA
for timing in 0 1; do
  EDADB_STAGE_TIMING=$timing PERF_WARMUPS=1 PERF_RUNS=5 PERF_SETTLE_SECONDS=5 \
    IEDA_BIN="$PWD/bin-release/iEDA" OUT_DIR="/tmp/iedadb_stage_$timing" \
    bash scripts/edadb/performance/run.sh \
      scripts/design/sky130_gcd/result/iPL_filler_result.def
done
```

- [runner](../../../../../scripts/edadb/performance/run.sh)、[Tcl计时](../../../../../scripts/edadb/performance/benchmark.tcl)、[统计脚本](../../../../../scripts/edadb/performance/p1-stage-timing/summarize.py)。
- 统计脚本接受包含`off/`和`on/`的结果根目录。本次命令：`python3 scripts/edadb/performance/p1-stage-timing/summarize.py /tmp/iedadb_p1_stage_validation`。
- 完整日志、DEF、DB、二进制哈希及工作区diff：`/tmp/iedadb_p1_stage_validation/`。这里是可再生成的本机证据，不提交大型生成物。
- 仓库仅保留本报告、小型原始计时、统计表及审计记录；未commit/push。
