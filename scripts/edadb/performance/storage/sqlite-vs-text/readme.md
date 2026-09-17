# SQLite与文本性能对比

**状态：低扰动版本已实现；正式计时、正确性和执行计数在不同进程中运行。** 最新批次状态及结论见[results.md](docs/results.md)。

## 阅读顺序

1. [结果与结论](docs/results.md)：先看写入、读取各慢多少，以及证据的限制。
2. [测试方案](docs/test_plan.md)：固定数据、参数、计时边界及下一轮验证方式。
3. [源码解释](docs/sqlite_source.md)：SQLite的具体调用链和对应版本源码。
4. 查看下方代码入口，区分当前实现与计划修改。

SQLite通用机制不在此重复：[参数](../sqlite-reference/config.md)、[执行计数](../sqlite-reference/execution_analysis.md)、[B-tree存储](../sqlite-reference/btree_storage.md)。

## 目录职责

```text
sqlite-vs-text/
├── readme.md              阅读入口、文件职责和实现状态
├── docs/
│   ├── results.md         已有实测结果、分析及原始证据链接
│   ├── test_plan.md       唯一当前方案；不保存第二份结果
│   └── sqlite_source.md   源码调用链；不重复测试步骤
├── benchmark.cpp          文本与SQLite测试程序
├── CMakeLists.txt         Release构建；普通版和perf版
├── run.py                 正确性、串行样本和结果汇总
├── run_perf.sh            独立perf采样与控制组
└── summarize_perf.py      perf结果和扰动汇总
```

代码暂留根目录，避免纯文档整理改变构建、脚本寻址或历史复现条件；不增加只有一两个文件的代码子目录。

## 测试实现

- [benchmark.cpp](benchmark.cpp#L190)：`text/sqlite/static`三条路线；`check/timing/counters`三种运行目的。正式循环无行内时钟或计数查询；诊断在阶段边界取SQLite已有计数。
- [run.py](run.py)：小规模及全量正确性预热 → A、B各5次串行计时 → 独立计数运行；同一CPU、轮换顺序，统计mean/median/min/max及配对差值。
- [run_perf.sh](run_perf.sh)、[summarize_perf.py](summarize_perf.py)：已有独立阶段采样及无perf控制；CPU占比不等于wall time。
- [CMakeLists.txt](CMakeLists.txt)：Release -O3构建入口，使用现有系统SQLite动态库，不重新编译SQLite。

## 如何运行和查看

从本目录执行；输出目录必须不存在。编译可并行，性能测试不可并发。

```bash
cmake -S . -B /tmp/iedadb_sqlite_text_counters_build \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-10
cmake --build /tmp/iedadb_sqlite_text_counters_build -j40
python3 run.py /tmp/iedadb_sqlite_text_counters_build/benchmark \
  /tmp/iedadb_sqlite_text_counters_new --count 1000000 --runs 5 --settle 1 --cpu 0
```

在普通宿主机shell执行，不与受限沙箱环境混算。`--cpu`选择当前允许的空闲CPU。快速检查可改为`--count 1000 --runs 1 --settle 0`。

- `report.md`：读、写分表的阶段时间；`samples.tsv`：原始时间；`summary.json`：统计值；`deltas.json`：同轮净差值。
- `counters.tsv`：单独运行的语句/Pager计数；不是耗时；`checks.json`、`audit.json`：正确性及计数校验。
- `manifest.json`、`source/`及日志：配置、版本、源代码、编译参数与原始证据。
- 正式实验结束后可运行 `bash run_perf.sh /tmp/iedadb_sqlite_text_counters_build/profile_benchmark /tmp/iedadb_sqlite_text_counters_perf_new`，再运行 `python3 summarize_perf.py /tmp/iedadb_sqlite_text_counters_perf_new`。需perf和sudo权限；这是独立CPU采样，不进入正式时间。

## 边界与产物

- 冻结的`baseline/`和`sqlite-vs-edadb/`不修改；遵循[performance交接](../../handoff.md)。
- 文档内链接使用相对路径。原始数据、DB、二进制和日志不放入此目录；已有服务器产物见[结果来源](docs/results.md#7-结果在哪里)。
- 新实验完成后更新同一份结果文档，标明批次与方法；不要把不同环境的时间相减，也不保留多个相互冲突的计划。
