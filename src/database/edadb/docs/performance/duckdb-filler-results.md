# Sky130 GCD filler：iEDA 存取性能

报告日期：**2026-11-20**。这是报告日期；实测批次和原始记录保持不变。

## 1. 实验配置

- **输入：** 同一份 Sky130 GCD `iPL_filler_result.def`（726,155 字节；2,604 个 Component、677 个 Net），使用 Sky130 HS LEF。
- **环境：** `cherry13`，固定逻辑 CPU 2，串行、热 OS 缓存；GCC 10、Release、`-O3`/LTO。SQLite 与 DuckDB 不是同批测试，Core 和内核版本不同。
- **当前源码：** iEDA 分支 `codex/duckdb-integration`，HEAD 为 `1ec76562a`；该提交仍锁定 Core `494ce79`，工作区实际检出的是 EDADB `8b8566a`。两批 DuckDB 都用 `8b8566a`；iEDA 的子模块指针与[建库修复](../../../manager/builder/builder.cpp#L382)未提交，单线程批次额外使用[连接线程设置](../../idb/edadb_idb_init.cpp#L24)。只检出 HEAD 不能复现本地实验。

| 配置 | 优化 SQLite 参照 | DuckDB 复测 |
| --- | --- | --- |
| 引擎 / EDADB Core | SQLite 3.37.2 / `494ce79` | DuckDB 1.5.6 / `8b8566a` |
| 并行与约束 | 固定 CPU 2，FK 开启 | 固定 CPU 2，读写连接均核验 `threads=1`，保留 PK/FK |
| 持久化与缓存 | `DELETE/FULL`；`cache_size=-2000`、`page_size=4096`、`mmap_size=0` | 原生 WAL/检查点、默认内存上限；不与 SQLite 页缓存参数强行等同 |

**默认配置批次：** DuckDB 设置为 `threads=40`，但 iEDA 进程同样固定 CPU 2，**并非使用 40 个 CPU 的跑分**。默认配置诊断记录的 `memory_limit` 为 100.5 GiB。

## 2. 测试过程

预热 1 轮，正式 5 轮取中位数；正式计时关闭 SQL trace、core profiling 和阶段计时。用同一 [Tcl 入口](../../../../../scripts/edadb/performance/benchmark.tcl)计完整命令：

- **读取：** 从新进程打开数据库、恢复 iDB。LEF 加载、读后导出 DEF 和校验不计时。
- **写入：** 从已加载的 iDB 写入新数据库；计连接、建表、数据写入与提交。不计写前的 DEF 解析。

DuckDB 每轮严格核对恢复 DEF，6 轮和另一次阶段诊断均通过。建表和数据各用一个事务，仍逐行执行 INSERT。

## 3. 读取结果

| 实现 | 完整读取中位数（ms） | 相对原始 iEDA 耗时比 |
| --- | ---: | ---: |
| 原始 iEDA（DEF 文本） | 47.484 | 1.00× |
| iEDA＋EDADB SQLite，未优化 | 11,931.378 | 251.27× |
| iEDA＋EDADB SQLite，已优化 | 103.668 | 2.18× |
| iEDA＋EDADB DuckDB，默认 40 线程 | 23,431.180 | 493.45× |
| **iEDA＋EDADB DuckDB，单线程** | **20,656.599** | **435.02×** |

耗时比 = 各组读取中位数 ÷ 原始 iEDA 读取中位数（47.484 ms），越小越好。

**结论：** DuckDB 读耗时是未优化／已优化 SQLite 的 **1.73×／199.26×**。独立诊断中，Net 恢复约 **20.04 秒**。当前仍逐父查子表，Point、ViaRef 的代表性计划为 `SEQ_SCAN`；这解释了为何会反复扫描，但未量化扫描独占耗时。

## 4. 写入结果

| 实现 | 完整写入中位数（ms） | 相对原始 iEDA 耗时比 |
| --- | ---: | ---: |
| 原始 iEDA（DEF 文本） | 6.855 | 1.00× |
| iEDA＋EDADB SQLite，未优化 | 2,277.177 | 332.19× |
| iEDA＋EDADB SQLite，已优化 | 391.170 | 57.06× |
| iEDA＋EDADB DuckDB，默认 40 线程 | 29,233.615 | 4,264.57× |
| **iEDA＋EDADB DuckDB，单线程** | **24,859.674** | **3,626.50×** |

**结论：** DuckDB 写耗时是未优化／已优化 SQLite 的 **10.92×／63.55×**。独立诊断中，Net 写入约 **20.17 秒**，最后一次 `COMMIT` 约 **0.47 秒**。优先验证逐行 `Execute()`，目前未拆出 PK/FK、ART 维护与 Shadow 转换各自的耗时。

单线程批次比默认 40 线程批次读少 **11.84%**、写少 **14.96%**；两批并非同一二进制交替运行，不能把降幅全归给线程数。同批原生 DEF 控制：默认组读 **71.236 ms**、写 **8.601 ms**；单线程组读 **71.082 ms**、写 **8.596 ms**。它们不替代接入 EDADB 前的原始 iEDA。SQLite 为跨批参照，耗时差也不是引擎的独立净效应。

## 5. 慢在哪里

**读：逐父查询重复扫描。** 实际库有 27 条 PK、26 条 FK。Point 没有 PK，`_vec_idx` 只是位置；它用 Net 名、Wire 键、Segment 键引用 Segment 的复合 PK。查询形如 `WHERE net = ? AND wire = ? AND segment = ?`（简写），[代表性计划](../../../../../scripts/edadb/performance/duckdb-filler/results/20261117/query-plans.txt#L248)为 `PROJECTION → SEQ_SCAN`，扫描时按三列过滤。

[DuckDB 官方《ART Index Scans》](https://duckdb.org/docs/current/guides/performance/indexing#art-index-scans)明确规定：只有**无表达式的单列索引**有资格做 ART 索引扫描，并将 `CREATE INDEX idx ON t(a,b)` 列为反例。因此多列 ART 可以用于[约束检查](https://duckdb.org/docs/current/sql/constraints)，但即使 `SELECT` 给齐全部键列，也不能用**这个多列 ART**做点查。实测补建 13 个 SQLite P1 同款索引后，6 个非空表的代表性访问方式都没变；单轮完整读取仅少 0.38%，不足以证明收益。

**写：时间主要在数据路径，不在建表或最终提交。** 单线程诊断中，连接与建表合计 **116.113 ms**。DuckDB [每行一次 `Execute()`](../../core/include/edadb/backend/duckdb/DbStatement4Duckdb.h#L207)，随后重置；复用预编译和合并事务都不减少执行次数。PK/FK 检查与 ART 维护仍要做，但未独立量化。[DuckDB 官方导入指南](https://duckdb.org/docs/lts/guides/performance/import)建议批量导入；下一步应固定线程、约束和事务，对比逐行写入与批量写入。

## 6. 编译与运行

在保留上述集成改动的 iEDA 仓库根目录执行；需要 GCC 10 和配套的 DuckDB 1.5.6 头文件及共享库。

```bash
bench_dir=/tmp/edadb-filler-repro
cmake -S . -B "$bench_dir/build" \
  -DCMAKE_C_COMPILER=gcc-10 -DCMAKE_CXX_COMPILER=g++-10 \
  -DCMAKE_BUILD_TYPE=Release -DCMD_BUILD=ON -DCOMPATIBILITY_MODE=OFF \
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$bench_dir/bin" \
  -DEDADB_BACKEND=DUCKDB \
  -DEDADB_DUCKDB_ROOT=/home/zhiyiwang/cs/db/duckdb/sources/v1.5.6 \
  -DEDADB_ENABLE_PROFILING=OFF -DEDADB_DEBUG_TRACE_SQL_STMT=OFF
cmake --build "$bench_dir/build" --target iEDA -j32

runner=scripts/edadb/performance/duckdb-filler/run.py
python3 "$runner" "$bench_dir/bin/iEDA" "$bench_dir/run-default" --build "$bench_dir/build"
python3 "$runner" "$bench_dir/bin/iEDA" "$bench_dir/run-threads1" --build "$bench_dir/build" --duckdb-threads 1
```

两组顺序运行，输出路径必须不存在。不传线程参数时采用引擎默认值，只有在本机原配置下才对应 40 线程。脚本依次输出样本、汇总和 DEF 审计。

**原始数据：** [单线程复测与诊断](../../../../../scripts/edadb/performance/duckdb-filler/results/20261118/)、[默认配置归档](../../../../../scripts/edadb/performance/duckdb-filler/results/20261117/)、[iEDA 历史基线](optimization-results.md)、[SQLite 配置与结果](sqlite-cost-results.md)。两批的 `manifest.json` 与 `production-diff.txt` 记录实际提交和源码差异。
