# SQLite与文本性能对比

**状态：本轮归因对照完成，已冻结。** 多行INSERT、step-only、独立计数及perf复核均完成：60组正确性、120条正式计时、24组采样及24组控制。核心原因和限制统一见[results.md](docs/results.md)，交付范围见[配对milestone](../../milestone.md)。

## 本轮完成了什么，还差什么

| 对照 | 已完成与结论边界 |
| --- | --- |
| 写入 | 单行与10/100行INSERT、TRANSIENT与STATIC独立对照，验证语句粒度及绑定策略的净影响；未测两者组合，不代表EDADB已实现优化 |
| 读取 | 完整8字段与step-only消融，定位取列/保存/消费路径的净影响；step-only不是等价完整读取 |
| SELECT补充 | 16组正确性、20条计时、4次计数；两种写法字节码相同，未证明完整读取有稳定速度差异 |
| perf | 24组采样及24组控制，无丢失样本；支持热点判断，不提供内部函数精确独占毫秒数 |

测试与审计已经完成，并按用户授权冻结本轮范围。cold、掉电持久性、完整adapter归因不在本轮已验证范围。详细时间只在results维护，后续生产优化另行讨论，不自动修改冻结实验。

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
├── audit_causal.py        新增对照的统计、SQL及计数审计
├── run_perf.sh            独立perf采样与控制组
├── summarize_perf.py      perf结果和扰动汇总
├── select_projection.cpp  星号与显式全部列读取补充
└── run_select.py          SELECT补充运行与审计
```

代码暂留根目录，避免纯文档整理改变构建、脚本寻址或历史复现条件；不增加只有一两个文件的代码子目录。

## 测试实现

- [benchmark.cpp](benchmark.cpp#L196)：`text/sqlite/static/batch10/batch100/step-only`六条路线；`check/timing/counters`三种运行目的。正式循环无行内时钟或计数查询；诊断在阶段边界取SQLite已有计数。step-only仅作读取消融，不与文本竞争完整读取速度。
- [run.py](run.py)：小规模及全量正确性预热 → A、B各5次串行计时 → 独立计数运行；同一CPU、轮换顺序，统计mean/median/min/max及配对差值。
- [run_perf.sh](run_perf.sh)、[summarize_perf.py](summarize_perf.py)：已有独立阶段采样及无perf控制；CPU占比不等于wall time。
- [CMakeLists.txt](CMakeLists.txt)：Release -O3构建入口，使用现有系统SQLite动态库，不重新编译SQLite。
- [audit_causal.py](audit_causal.py)：复算样本与delta、核验RUN和SELECT计数、以相同SQLite source ID生成批量EXPLAIN。独立运行，不进入正式计时。

## 如何运行和查看

从本目录执行；输出目录必须不存在。编译可并行，性能测试不可并发。

```bash
cmake -S . -B /tmp/iedadb_sqlite_text_counters_build \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-10
cmake --build /tmp/iedadb_sqlite_text_counters_build -j40
python3 run.py /tmp/iedadb_sqlite_text_counters_build/benchmark \
  /tmp/iedadb_sqlite_text_causal_new --count 1000000 --runs 5 --settle 1 --cpu 0
python3 audit_causal.py /tmp/iedadb_sqlite_text_causal_new
```

在普通宿主机shell执行，不与受限沙箱环境混算。`--cpu`选择当前允许的空闲CPU。快速检查可改为`--count 1000 --runs 1 --settle 0`。

- `report.md`：读、写分表的阶段时间；`samples.tsv`：原始时间；`summary.json`：统计值；`deltas.json`：同轮净差值。
- `counters.tsv`：单独运行的语句/Pager计数；不是耗时；`checks.json`、`audit.json`：正确性及计数校验。
- `manifest.json`、`source/`及日志：配置、版本、源代码、编译参数与原始证据。
- `causal_audit.json`：新增对照审计；`batch_explain.json`：批量SQL及字节码（独立解释，不是正式执行trace）。运行器执行输出目录中的二进制副本，避免构建目录重新编译替换样本使用的程序。
- 正式实验结束后可运行 `bash run_perf.sh /tmp/iedadb_sqlite_text_counters_build/profile_benchmark /tmp/iedadb_sqlite_text_counters_perf_new`，再运行 `python3 summarize_perf.py /tmp/iedadb_sqlite_text_counters_perf_new`。需perf和sudo权限；这是独立CPU采样，不进入正式时间。

## 补充：SELECT *与显式全部列

[select_projection.cpp](select_projection.cpp)复用既有schema、数据及读取循环，仅切换SQL；[run_select.py](run_select.py)核验全字段、计数和EXPLAIN，再分别统计完整read与批量prepare/finalize。具体边界见[方案第8节](docs/test_plan.md#8-select-与显式全部列读取补充)。

```bash
cmake -S . -B /tmp/iedadb_select_build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-10
cmake --build /tmp/iedadb_select_build -j40 --target select_projection
python3 run_select.py /tmp/iedadb_select_build/select_projection /tmp/iedadb_select_new \
  --count 1000000 --runs 5 --prepare-repeats 10000 --settle 1
```

在普通宿主机shell运行；输出目录必须不存在。查看samples.tsv、summary.json、deltas.json、audit.json；日志保留实际SQL、PRAGMA、字节码和计数。写入仅准备数据库，不作为本补充实验的性能结果。

## 边界与产物

- 冻结的`baseline/`和`sqlite-vs-edadb/`不修改；遵循[performance交接](../../handoff.md)。
- 文档内链接使用相对路径。原始数据、DB、二进制和日志不放入此目录；已有服务器产物见[结果记录](docs/results.md)。
- 新实验完成后更新同一份结果文档，标明批次与方法；不要把不同环境的时间相减，也不保留多个相互冲突的计划。
