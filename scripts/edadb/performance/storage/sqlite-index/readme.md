# SQLite索引与存储布局实验

**状态：完整测试及审计通过，已由用户确认冻结。** 48组小规模正确性、80个正式样本、16组独立全量诊断通过。先读[结果与分析](docs/results.md)，再读[测试方案](docs/test_plan.md)。未经用户明确批准，不修改本目录代码、配置、计划或结论；后续扩展放独立目录，见[冻结规则](AGENTS.md)。

- 本实验：单张Component表、相同8字段，比较无索引、普通索引、唯一索引、TEXT主键与WITHOUT ROWID。
- rowid独立对照：同一整数键，比较隐式rowid＋唯一索引、INTEGER PRIMARY KEY别名、WITHOUT ROWID；不混算TEXT/整数键差值。
- 分开比较写数据、全量读取、name点查；建表、提交及空间单列。首轮仅SQLite C API。
- 不重跑双表PK/FK、N+1实验，不加入EDADB、adapter或文本对照。
- 代码和报告只在本目录新增，不修改既有实验或生产core。生成产物仍放仓库外。

## 阅读顺序与实现

1. [test_plan.md](docs/test_plan.md)：8种实现的PK、额外索引/树数量、INSERT维护和点查路径；实验边界。
2. [benchmark.cpp](benchmark.cpp)：实际C API调用及粗粒度计时。数据生成第45行，DDL第68行，绑定/取列第83/93行，写第130行，全读第141行，点查第157行，计时第220行，检查第249行，schema/EQP/VM证据第284行，阶段编排第308行。
3. [run.py](run.py)：第117行起并行做48组小规模正确性检查；第131行起串行运行预热和正式样本；第152行起独立诊断；第191行起统计。
4. [summarize.py](summarize.py)：读取已审计原始样本生成report.md，不运行实验。

计时逻辑：write包含prepare/逐条bind-step-reset/finalize；全读及点查包含prepare/取全部8字段/消费/finalize。BEGIN、COMMIT、create单列。没有逐行计时；诊断模式独立收集SQLite原生计数器及逐字段验证，不用于性能统计。读复用一个Record，不累计结果vector。

## 构建与运行

从仓库根目录执行，输出目录必须尚不存在：

```bash
cmake -S scripts/edadb/performance/storage/sqlite-index \
  -B /tmp/iedadb_sqlite_index_build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build /tmp/iedadb_sqlite_index_build -j40

# 小规模完整流程验收，不代替正式性能结果。
python3 scripts/edadb/performance/storage/sqlite-index/run.py \
  --binary /tmp/iedadb_sqlite_index_build/index_benchmark \
  --out /tmp/iedadb_sqlite_index_smoke --count 1000 --runs 1 --settle 0

# 正式：每种变体预热一次，计时五次；CPU固定，性能样本不并行。
python3 scripts/edadb/performance/storage/sqlite-index/run.py \
  --binary /tmp/iedadb_sqlite_index_build/index_benchmark \
  --out /tmp/iedadb_sqlite_index_full --count 1000000 --runs 5 --settle 2

# 测试成功后，生成可独立阅读的统计报告。
python3 scripts/edadb/performance/storage/sqlite-index/summarize.py \
  /tmp/iedadb_sqlite_index_full
```

程序使用系统SQLite库，不替换或修改SQLite、EDADB。Release -O3由构建和runner核验；检查并发4进程，正式运行固定单CPU（默认当前允许CPU中的最小编号，可用--cpu指定）。阶段间不加后台压测。

## 输出怎么看

输出目录中的文件：
- audit.json：完成数量及审计结果，先看passed。
- samples.tsv：每次每阶段原始ms；summary.tsv：分组均值、中位数、最小/最大值。
- samples.json：样本与配置、输入哈希、记录消费结果、空间和原始log路径。
- diagnostics.json、diagnostics/*.log：独立完整字段验证、SQL、树根页、索引列、EQP、VM、计数器及可用时的dbstat。
- manifest.json：版本、源文件/二进制哈希、编译参数、硬件；checks.json：48组小规模检查。
- progress.json：运行进度与预计剩余计时阶段时长；实际整批时长见audit.json的elapsed_s。
- report.md：summarize.py生成的完整统计表；仓库内docs/results.md保留本批数字、证据链接与人工分析。

正式进程每个样本输出进度；可把stdout/stderr重定向到仓库外的launcher.log查看。阶段时间为-1代表不适用或诊断模式禁用，不能解释为负耗时。完整数据、DB、二进制和log不提交。

当前结果目录为/tmp/iedadb_sqlite_index_full，约2.8 GiB；这里是本机临时产物，不是永久存档。重跑时换一个新的--out目录，不覆盖本批证据。
