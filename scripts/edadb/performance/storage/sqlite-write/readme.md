# SQLite与文本读写：批量与交错

仅测试原生SQLite和C++文本；不构建或修改EDADB/adapter。

状态：本提交保存已完成A/B及独立写profiling，未建立新milestone、未push。提交前四个Release目标构建、105组空集/小规模/尾批检查、4组全量profiling控制通过；未重跑正式性能或perf采样。

证据复核：早期写审计、B独立审计、历史12次perf报告审计通过；A的80条时间全部阶段中位数及12组配对对照复算通过，测量源码/二进制哈希一致，唯CMake在测量后增加交织目标，保留原manifest不改写。A完整旧审计含该CMake哈希要求，不能声称原脚本无差异重跑PASS。初次验证误给profile目标传入普通bench参数，修正调用为`route control`后四组通过，未改变测试源码。

阅读顺序：[结果](results.md) → [计划](test_plan.md) → 对应实现/运行器。不要用早期run.py代替最新A/B：

| 批次 | 实现／运行器 | 证据 | 结论 |
| --- | --- | --- | --- |
| A：单表写粒度＋完整读 | 冻结benchmark.cpp直接编译；run_read_write.py | results/rw-*，80条时间 | 百行写改善约36%～38%；完整读仍慢于文本 |
| B：三表读写交织 | interleaved.cpp；run_interleaved.py | results/interleave-*，200条时间 | 写切换无稳定大收益；内存按表读改善约1.8% |
| 早期写专项 | benchmark.cpp；run.py | results内无前缀文件，70条时间 | 写入批量收益及三表写对照，历史独立批次 |
| 独立写perf | profile.cpp；run_profile.py | results/profile-*，12采样＋12控制 | 定位写CPU路径，不解释最新读取独占耗时 |

共用冻结[SQLite测试函数](../sqlite-vs-text/benchmark.cpp)：A直接构建原入口，其余通过SQLITE_TEXT_LIBRARY包含；不修改原文件，也不链接EDADB。

**完整读写补测**：单表复用冻结源码构建sqlite_read_write；三表交织新增sqlite_interleaved，与三文件文本匹配对照。先看[结果](results.md)的读、写分表，不将不同批次相减。

```bash
cmake -S scripts/edadb/performance/storage/sqlite-write -B /tmp/sqlite-read-write-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/sqlite-read-write-build --target sqlite_read_write -j 40
python3 scripts/edadb/performance/storage/sqlite-write/run_read_write.py --binary /tmp/sqlite-read-write-build/sqlite_read_write --output /tmp/sqlite-read-write-new-run
python3 scripts/edadb/performance/storage/sqlite-write/audit_read_write.py /tmp/sqlite-read-write-new-run
```

补测预热逐字段验证，正式读循环只保留两条路线相同的摘要消费，不把逐字段验证算成读取时间。读取是完整解码、保存、消费，不是COUNT或step-only。详细边界见[计划补充](test_plan.md#完整读写补测)。以下命令仍用于保留的写入与交错实验。

```bash
cmake -S scripts/edadb/performance/storage/sqlite-write -B /tmp/sqlite-write-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/sqlite-write-build -j 40
python3 scripts/edadb/performance/storage/sqlite-write/run.py --binary /tmp/sqlite-write-build/sqlite_write --output /tmp/sqlite-write-new-run
```

输出目录必须不存在；生成DB/文本在逐字段校验后删除。编译与功能测试可以并行；正式性能组串行、固定一个可用CPU，不与重负载争用。每个正式样本输出进度。保存samples、summary、counters、manifest及audit；日志/二进制留在外部运行目录，不提交。

本轮数据不覆盖旧批次，也不将编译器或环境不同批次的毫秒数直接相减。sqlite-write的memory/disk是存储配置，t0/t1/t2是三张表，不使用同名A/B造成混淆。

三表／三文件交织补测（完整读写）：

```bash
cmake -S scripts/edadb/performance/storage/sqlite-write -B /tmp/sqlite-interleaved-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/sqlite-interleaved-build --target sqlite_interleaved -j 40
python3 scripts/edadb/performance/storage/sqlite-write/run_interleaved.py --binary /tmp/sqlite-interleaved-build/sqlite_interleaved --output /tmp/sqlite-interleaved-new-run
python3 scripts/edadb/performance/storage/sqlite-write/audit_interleaved.py /tmp/sqlite-interleaved-new-run
```

阅读顺序：[结果](results.md) → [交织计时与控制](test_plan.md#三表三文件交织补测) → [调度与完整取值实现](interleaved.cpp) → [运行矩阵](run_interleaved.py)。不修改EDADB或冻结测试。

正式时间结束后单独复核与采样：

```bash
python3 scripts/edadb/performance/storage/sqlite-write/audit.py /tmp/sqlite-write-new-run
python3 scripts/edadb/performance/storage/sqlite-write/run_profile.py --binary /tmp/sqlite-write-build/sqlite_write_profile --output /tmp/sqlite-write-new-profile
python3 scripts/edadb/performance/storage/sqlite-write/audit_profile.py /tmp/sqlite-write-new-profile
```

采样需要现有sudo权限（脚本使用sudo -n），不修改系统安全设置。仅内存写入，四路线各3次采样及3次同profile二进制无perf控制；FIFO开关在计时边界外，仅采data阶段。正式时间仍取sqlite_write二进制，不与提权宿主机profile的毫秒数相减。perf原始数据与日志留在外部目录，保存精简热点及审计。
