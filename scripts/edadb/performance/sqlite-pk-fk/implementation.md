# 主外键测试实现

源码仅在本目录，SQLite与EDADB链接同一SQLite库；不加载iEDA，不修改core或adapter。
计划见[test_plan.md](test_plan.md)，结果见[experiment_report.md](experiment_report.md)。

## 实现定位

| 内容 | 代码位置 | 实现 |
| --- | --- | --- |
| 类及映射 | [pk_fk_benchmark.cpp:19](pk_fk_benchmark.cpp#L19) | Component八字段+pins；Pin四字段，无shadow |
| schema开关 | [pk_fk_benchmark.cpp:85](pk_fk_benchmark.cpp#L85) | 元数据前设置hasPrimKey；两条路线使用EDADB生成的DDL/SQL |
| 连接/配置 | [pk_fk_benchmark.cpp:127](pk_fk_benchmark.cpp#L127) | memory=A参数，disk默认存储参数；FK显式设置；重连重复设置 |
| 合成输入 | [pk_fk_benchmark.cpp:156](pk_fk_benchmark.cpp#L156) | 计时外生成父子vector；mixed正确性数据含空父和非顺序输入 |
| 消费/校验 | [pk_fk_benchmark.cpp:174](pk_fk_benchmark.cpp#L174) | 性能只累加长度/数值；check另做逐字段、身份、重复对象检查 |
| 写数据 | [pk_fk_benchmark.cpp:210](pk_fk_benchmark.cpp#L210) | SQLite复用两条INSERT；EDADB复用一个root insert op，递归写pins |
| 图读取 | [pk_fk_benchmark.cpp:233](pk_fk_benchmark.cpp#L233) | SQLite复用父/子SELECT；EDADB逐root readNext；均恢复一个父和子vector |
| 全表读取 | [pk_fk_benchmark.cpp:255](pk_fk_benchmark.cpp#L255) | SQLite两条SELECT消费字段，不构建父子关系，单独标记flat |
| DDL/计划核验 | [pk_fk_benchmark.cpp:300](pk_fk_benchmark.cpp#L300) | table_info/index_list/index_info/FK及EXPLAIN；仅check进程 |
| 错误/回滚 | [pk_fk_benchmark.cpp:318](pk_fk_benchmark.cpp#L318) | 孤儿/重复子通过SQL检查约束；重复父通过所选API检查错误传播及整事务回滚 |

## 计时边界

主流程位于[pk_fk_benchmark.cpp:362](pk_fk_benchmark.cpp#L362)，单位ms，steady_clock记录阶段边界。

```text
计时外：生成输入和期望摘要
init：schema元数据/SQL准备 + open/config
create：BEGIN → 建父子表/必要索引 → COMMIT
begin：数据BEGIN
write：prepare/构造op → 逐父及全部子插入 → finalize/销毁op
commit：数据COMMIT
close：文件库关闭
计时外：文件预读至OS cache；memory保留原连接
read init：文件库新连接/config
read：prepare/构造reader → 全量读取/消费/结果对象清理 → finalize/销毁reader
close：关闭连接
```

memory连续测first/repeat，分别重建reader但保留连接。first不宣称冷缓存；disk仅测OS-warm。
write_complete=begin+write+commit，不含init/create/close；-1表示不适用，不是零耗时。
core对子vector有自己的临时恢复实现；SQLite也构建并替换一个父的子vector，但不声称分配次数完全相同。
read_flat只有字段消费，不计入EDADB框架增量对照。read摘要消费和正常错误检查包含在计时中。

## 如何验证

[run_pk_fk.py](run_pk_fk.py)先独立并发check，再串行性能采样；性能阶段不启用SQL trace或逐字段校验。
check通过sqlite3_trace_v2统计实际SELECT执行次数；结果表区分执行次数与prepare次数。
跨路线比较完整DDL、SQL、实际配置和索引列顺序；大规模性能样本核对父/子数量与摘要。
完整数据逐字段验证在check进程进行，性能摘要不能替代它。负测预期SQLite约束错误日志不代表样本失败。

每个输出目录保存源码快照、版本/二进制哈希、编译链接参数、check证据和每个样本日志。
runner从RESULT行输出samples.tsv，再按组生成summary.tsv/report.md，完成后重读日志核验计时并写audit.json。
本轮没有添加逐API内部timer，也不宣称微小计时扰动为零。
