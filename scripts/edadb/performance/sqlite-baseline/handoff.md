# 五路线全量性能分析handoff

## 任务边界

本批次运行期间不修改代码，使用prof-test运行包括adapter的完整批次，再讨论性能。
用户随后批准提交、推送基线源码与文档及core的SQL trace打印；不创建milestone，不开始两表PK/FK实验。
本文件是接续入口；测量方法见[implementation.md](implementation.md)，配置见[sqlite_config.md](sqlite_config.md)，最终分析集中在[experiment_report.md](experiment_report.md)。

## 当前批次

- 启动：2026-09-10 19:42，Asia/Shanghai。
- 父仓库：prof-test，7b661baa25041c1c2c69c27dd5ada6dc8d75121c。
- 测量时core：90a5fb249c7b2f49f890bf6573fcc5fb05056b18加SQL trace回调的两行打印；编译明确关闭trace。
- 归档时core：d6656f0，分支codex/sql-trace-baseline；仅提交上述两行，源码与测量时一致，未重写原始manifest版本。
- adapter生产代码未加分段计时；测试入口不再扫描参考DEF。
- 五条路线：text、native、sqlite、edadb、adapter。1,000/10,000/100,000/1,000,000条；B-default仅1,000条。
- Release -O3，编译-j40；正确性并发最多5组，性能串行，1次预热＋5次正式、等待5秒。
- 构建日志：`/tmp/iedadb_stream_full_20260910.build.log`。
- 运行日志：`/tmp/iedadb_stream_full_20260910.log`。
- 输出目录：`/tmp/iedadb_stream_full_20260910`。
- 审计日志：`/tmp/iedadb_stream_full_20260910.audit.log`；流水线退出码：`/tmp/iedadb_stream_full_20260910.exit`。
- 状态：21:00完成，约78分钟，退出码0；99组正确性、540条计时、108个分组各5次及结果审计PASS。运行中约每5分钟汇报过心跳，无失败。没有修改测试或生产代码，没有commit/push。

## 结果如何查验

1. 先看退出码是否0、运行日志是否COMPLETE、audit.json是否PASS。
2. checks.json验证正确性；samples.tsv是逐次计时及log路径，summary.tsv是均值/中位数/min/max。
3. report.md包含统计和完整原始计时；manifest.json记录源码/二进制哈希及硬件，datasets.json记录LEF/DEF输入路径和哈希。
4. 正式比较只用本批次，不合并其他批次样本或部分运行。/tmp不是长期存储，需要保存时整体备份输出目录。

## 必须保留的分析边界

- create与read/write分开。direct的BEGIN/COMMIT单列；事务对照用complete=data+begin+commit，不重复加总。
- adapter的init包含建表，无法独立报create；adapter write包含内部提交，不能伪称已排除COMMIT。
- native的open/close在API内，输出init/close的0是未拆分占位，不代表没有成本。
- text与direct读复用一个Record；native/adapter构建完整iDB。跨路线差值不是纯对象重建或traversal耗时。
- adapter Instance表19列、有name主键并按_order_sd排序；direct为8列无显式PK/FK/index，不排序。不能忽略SQL/schema工作量差异。
- A内存库与B文件库同时改变日志/同步/缓存，时间差不是纯磁盘代价；os-warm不等于设备读取。
- 不通过删除COMMIT、忽略错误、关闭校验或更改生产逻辑制造更快结果。正确性在独立check运行，不计入正式性能样本。
- 不复用本轮已撤销的adapter分段计时方案；当前没有独立测得建表以外的内部SQL/对象重建各自占比。

## 交给下一位agent的问题

先阅读[统一报告](experiment_report.md)，再打开[本批次原始计时与统计](/tmp/iedadb_stream_full_20260910/report.md)。用户不希望拼接多个批次；以后讨论以这个完整批次为依据。

1,000,000条、ms、中位数的关键结论：
- C++文本write/read：748.833 / 622.220；原生iEDA：702.185 / 4843.321。
- SQLite B-batch：1597.901 / 844.419；EDADB B-batch：1685.538 / 1141.248，净增5.48% / 35.15%。write不含外层提交。
- adapter：5813.067 / 4730.975；write含内部提交，init+create另计1415.042，不能冒充纯create。
- 以上只是导航摘要，完整阶段与波动表只在统一报告维护；不要再建一份独立结果表手工维护。

继续讨论顺序：
1. 同条件SQLite与EDADB直接路径的差值能说明什么，不能说明什么？
2. SQLite A/B的数据阶段差异仅几个百分点；提交、建表另列后是否符合用户预期？
3. adapter与native均恢复iDB，但与direct有schema、排序和对象保留差异。能否先明确要比较的工作量，再决定是否测分项？
4. 若用户同意再插桩，需先定义init/create与内部COMMIT的边界，以及仪器扰动控制。当前没有各项独立计时，不能给出伪造占比。

注意1,000,000条adapter读取接近native，不代表所有规模领先；10,000条在本批次明显更慢。数据已验证，原因尚未单独定位。
不要把P系列优化结论或其他分支结果混入本基线，也不要未经授权重启实验、修改schema/排序、删除COMMIT或提交代码。
