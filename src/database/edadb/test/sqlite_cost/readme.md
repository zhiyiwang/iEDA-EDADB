# 无P2版本的SQLite profiling

先读[计划](../../docs/performance/sqlite-cost-plan.md)和[结果](../../docs/performance/sqlite-cost-results.md)。生产代码为P1/P3/P4/P5加观测，core不变；P2已经从源码移除。

## 实现

- `run.py`：生成固定fixture，复用Tcl计时，OFF/ON交替各5次；计时外核对DEF、DB完整性、FK和逻辑dump。启动时拒绝脏core头文件、批读API和错误优化/trace配置。
- `probe.cpp`：仅独立诊断加载。计数器在statement清理及阶段边界读取；MARKERS_ONLY仅提供阶段标记。
- `analyze.py`：汇总计数、查询计划、perf样本和I/O。嵌套阶段不相加。
- `review.py RESULTS`：指定批次，重算统计、阶段加总和SQL次数；报告当前源码/二进制是否还匹配，不修改数据。
- `collect.py OUTPUT --destination NEW_DIRECTORY`：核验源码、core、脚本、性能和功能证据后收集；目的目录存在时立即拒绝。没有默认覆盖目录。
- `test_probe.py`：验证2,001条INSERT及SELECT精确执行/step次数；`test_workflow.py`：验证环境隔离、防覆盖及必填批次。
- `edadb_stage_timing.h`：RAII计时；Session在命令结束输出，DetailTimer覆盖root family与事务。data包含数据事务，COMMIT只能作为子项解读。

## 构建和验证

从仓库根目录执行，`out`必须是新的目录。不要复用含P2对象文件的旧二进制。

```bash
out=/tmp/edadb-profile-only
testdir=src/database/edadb/test
profile=$testdir/sqlite_cost
cmake -S . -B "$out/build" -DCMAKE_C_COMPILER=gcc-10 -DCMAKE_CXX_COMPILER=g++-10 \
  -DCMD_BUILD=ON -DCOMPATIBILITY_MODE=OFF -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$out/bin" \
  -DEDADB_ENABLE_PROFILING=OFF -DEDADB_DEBUG_TRACE_SQL_STMT=OFF \
  -DBUILD_GUI=OFF -DBUILD_PYTHON=OFF
cmake --build "$out/build" --target iEDA -j32
python3 "$profile/test_workflow.py"
python3 "$profile/test_probe.py" "$out"
g++-10 -std=c++17 -O3 -pthread -Isrc/database/edadb/idb \
  scripts/edadb/performance/p1-stage-timing/test_stage_timing.cpp -o "$out/test-timing"
"$out/test-timing" > "$out/timer-test.log"
python3 "$testdir/build_diagnostic.py" "$out/regression-bin" --build "$out/build"
python3 "$testdir/build_diagnostic.py" "$out/conversion-bin" --build "$out/build" --conversion-fault
EDADB_STAGE_TIMING=1 IEDA_BIN="$out/regression-bin/iEDA" OUT_DIR="$out/regression" \
  EDADB_TEST_JOBS=8 bash "$testdir/run_idb_roundtrip_regression.sh"
python3 "$testdir/audit_regression.py" "$out/regression" "$out/regression-bin/iEDA"
EDADB_STAGE_TIMING=1 python3 "$testdir/run_adapter_fault.py" "$out/bin/iEDA" "$out/insert-fault"
EDADB_STAGE_TIMING=1 python3 "$testdir/run_adapter_fault.py" \
  "$out/conversion-bin/iEDA" "$out/conversion-fault" --conversion-fault
```

功能检查全部结束后，再进行正式性能测量。

## 运行与收集

先准备本机 `environment.json`，记录库版本/Build-ID、硬件、编译条件。不能照抄旧二进制或库哈希。perf需要授权，strace需要ptrace权限；不更改系统全局权限。

离线符号必须匹配运行库。在 `$out/symbols` 中准备对应Build-ID缓存；可用 `eu-unstrip` 合并实际SQLite共享库与匹配debug文件，再用 `perf --buildid-dir "$out/symbols" buildid-cache -a SYMBOLIZED_ELF` 注册。不替换运行库。源码/符号位置见本批environment记录。

```bash
for phase in smoke timing summarize counters markers perf io; do
  python3 "$profile/run.py" "$phase" --output "$out" --binary "$out/bin/iEDA" --build "$out/build"
  case "$phase" in
    counters|markers|perf|io) python3 "$profile/analyze.py" "$phase" "$out" ;;
  esac
done
python3 "$profile/review.py" "$out"
python3 "$profile/collect.py" "$out" --destination "$profile/results/profile-only"
```

每个运行phase只执行一次；失败保留日志，选择新目录复测。正式运行不继承LD_PRELOAD或旧P2环境变量；只有独立诊断加载探针。perf按普通用户运行iEDA，采样cycles:u/999Hz/16KiB DWARF栈，解析关闭内联展开。

## 如何查看

`results/profile-only/`保存原始时间、统计、SQL/API计数、CPU/I/O诊断、manifest、production.patch和审计。大DB/DEF/二进制/perf原文件保存在输出目录，不进Git。

- `samples.tsv / summary.tsv`：正式墙钟时间；mean与median分开，init/create不计入data。
- `sql-statements.tsv / query-plans.json`：工作量与执行计划，不是函数耗时。
- `cpu-categories.tsv`：用户态CPU样本占比，不包含全部I/O等待；内联限制对象恢复归因。
- `io-stages.tsv / diagnostic-cpu-time.tsv`：独立诊断，不能与正式时间相减。
- `manifest.json / production.patch / audit.json`：版本和测量身份。commit后HEAD变化不代表代码必然变化，按源码哈希及补丁核对。

旧的含P2但关闭开关批次及未提交P2源码已在服务器 `/tmp/edadb-p2-before-rollback/` 归档，不作为本次提交结果。
