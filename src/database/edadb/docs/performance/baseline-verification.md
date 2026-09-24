# 原生DEF基线差异核查

## 复测结论

完整命令仍使用相同Tcl计时；同环境、交替执行、warm正式5次，中位数及范围如下，单位ms：

| 路径 | 未优化二进制：中位数（最小–最大） | 三项修改后二进制：中位数（最小–最大） |
| --- | ---: | ---: |
| 原生DEF read | 48.272（48.078–48.767） | 47.295（47.189–47.681） |
| 原生DEF write | 6.920（6.897–6.971） | 7.034（6.951–7.120） |
| EDADB完整read | 11931.378（11921.305–12382.369） | 108.873（102.469–111.069） |
| EDADB完整write | 2277.177（2153.786–2494.176） | 419.340（375.621–438.057） |

**原生读取只差约2.02%，写入差约1.65%；未复现历史约71与47 ms的大差距。** 此次没有改用C++分段计时。已确认的问题是此前主表混用了历史批次，缺少同环境原生控制；历史运行差距的具体物理原因仍无充分证据，不能猜成某个确定因素。

EDADB相对同环境未优化版：完整读取时间减少99.09%（109.59倍），完整写入减少81.59%（5.43倍）。这是全部代码变化后的完整操作收益，包含初始化、建表及提交等对应工作，并包含移除参考DEF扫描的影响，不是某一个内部阶段的独占收益。

全部24个含预热样本通过严格DEF、integrity/FK及各表行数检查。正式统计20个样本，预热4个样本不计入。[原始计时](../../../../../scripts/edadb/performance/baseline-verification/samples.tsv)、[统计](../../../../../scripts/edadb/performance/baseline-verification/summary.tsv)、[审计](../../../../../scripts/edadb/performance/baseline-verification/audit.json)、[哈希](../../../../../scripts/edadb/performance/baseline-verification/manifest.sha256)。

## 为什么重新测量

前次总表把两个历史批次放在一起：原生DEF读取71.285与47.499 ms，写入8.663与7.001 ms。它们并非同一次交替对照实验。因此，不能把这些差值当作EDADB优化使原生DEF变快，也不能据此确定CPU、缓存或编译中的某一因素。

本轮保留[结果总表](optimization-results.md)，重新测量未优化iEDA+EDADB和三项修改后的版本。完整命令结果使用新批次；旧分段结果保留来源，不冒称本轮重新计时。

## 已核实的证据

- 未优化源代码：iEDA `62504bd9d`、core `be6bbdd`，现存工作区无修改；使用服务器保留的Release构建，没有下载或重建其他版本。
- 优化版：父仓库起点`4ba761281`、core `1c4857c`，另含当前adapter事务合并、计时及移除参考DEF扫描的工作区修改。
- 原生DEF读写、iDB数据代码和DEF解析器源码没有上述版本差异。测试runner、Tcl计时、Sky130 LEF初始化脚本也没有差异。
- 特别核验：历史提交与当前`benchmark.tcl`的SHA256均为`3c980c473568778644cb2b0b1c47aa8d1fd2e466b1d933a579157075b2039b7f`。原生读取71.285、47.499 ms及本轮复测均为同一个Tcl `time_command`包围`def_init`所得，不是Tcl与C++计时的差异。
- 两构建均GCC 10、Release、`-O3`、LTO；def_builder及DEF解析库的编译选项和宏相同。EDADB细粒度profiling和SQL trace均关闭。
- 历史与前轮的canonical DEF输出SHA256相同，均为`b2a8ade6f7e74f620491f56914a34d8fb89dd3efe957e4dc3ec4a42d50104c5e`，也与本轮输入一致。这证明规范化输出相同，不替代未保存的历史原始输入哈希。
- 初步数据库核对：两版均41张表、34,358条记录，各表行数一致；未优化版27个索引，优化版40个索引。

以上排除了明显的输入内容、计时脚本和原生源码差异；**没有证明历史差距的唯一物理原因**。不能仅凭耗时声称是CPU频率、缓存、调度或代码布局造成。

## 同环境复测方法

### 不要混淆两种计时

| 指标 | 时钟与边界 | 用途 |
| --- | --- | --- |
| 原生DEF/EDADB完整命令 | Tcl `clock microseconds`，命令调用前至返回后 | 本轮两版本正式对照，沿用历史方法 |
| EDADB init/create/data | C++ `steady_clock`，对应adapter作用域入口至出口 | 前轮内部阶段分析；本轮关闭，不用于解释原生71→47 ms差异 |

Tcl计时实现见[benchmark.tcl:18](../../../../../scripts/edadb/performance/benchmark.tcl#L18)，原生调用见[第49行](../../../../../scripts/edadb/performance/benchmark.tcl#L49)。本轮新增的CPU绑定、版本交替属于运行条件控制，不改变这段计时边界。

1. 两版使用同一DEF、LEF、Tcl脚本、SQLite动态库和输出文件系统。
2. 固定CPU 2；所有样本串行，测试期间不编译。每个样本前等待5秒。
3. 两版各先运行一次cold及warm作为预热；随后5轮正式测试，每轮都测两个版本，轮换先后顺序。
4. 每个版本/缓存条件保留5次正式值；预热不计入统计。冷缓存为文件级驱逐请求，热缓存为预读文件，不声称设备缓存完全清空。
5. 原生计时：`def_init`和`def_save`调用前后Tcl计时；LEF加载在外。写入EDADB前的DEF加载及读取后的DEF输出也在EDADB计时外。
6. `EDADB_STAGE_TIMING=0`，比较完整命令；不从完整时间推算未优化版内部阶段。
7. 每个样本进行严格DEF比较；结束后检查所有DB的integrity/FK结果、跨版本canonical DEF一致性。

脚本仍为[run.sh](../../../../../scripts/edadb/performance/run.sh)与[benchmark.tcl](../../../../../scripts/edadb/performance/benchmark.tcl)。驱动脚本在`/tmp/iedadb_verify_same_environment.sh`；它只改变二进制路径与输出目录，不修改生产代码或SQL。

本轮产物目录：`/tmp/iedadb_same_environment_20260924/`：

- `baseline/round-0`、`optimized/round-0`：预热，不纳入正式统计。
- 两版`round-1`至`round-5`：原始日志、逐次TSV、DEF、DB。
- `manifest.sha256`、`input_hashes.json`、`environment.txt`：二进制/输入/脚本哈希及机器信息。
- `samples.tsv`、`summary.tsv`、`audit.json`：完成后汇总的原始时间、统计及正确性检查。

二进制SHA256：

- 未优化：`68ca7bfeb96574dbffcd1e67fc841e189d042c3aae1355e1e3efcf9936230be0`。
- 三项修改后：`be1d2de135a09960288736804aa63fd68181c5e4c3836c3e8afca4341f85f985`。
