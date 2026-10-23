# EDADB DuckDB＋Parquet后端：分析交接

## 1. 给接手agent的任务

先审阅现有EDADB后端和独立DuckDB/Parquet证据，提出可验证的接入设计；用户审阅后再实施。不是立即替换SQLite、切分支或删除旧实现。目标为完整iDB保存/恢复更快，同时保留未来局部修改的需求；不能只优化全量快照而默认放弃更新。

需要回答：DuckDB原生数据库承担什么？Parquet快照承担什么？现有ORM哪些接口可复用、哪些语义不兼容？给出修改文件、伪代码、实际SQL、验收及失败回滚方案。不以平面扫描速度承诺端到端更快。

## 2. 实查状态：哪些已有结论

当前iEDA工作区`/home/zhiyiwang/cs/arch/eda/iEDA-EDADB`在`prof-test@d1e3a2125`，core gitlink `d6656f0`且无工作区修改。本次文档提交保存SQLite写读实验、总结/交接、导航及.gitignore；基线HEAD d1e3a2125是提交前起点，不是新提交SHA。另有未跟踪构建目录。不要checkout/reset/clean或git add .。

历史已优化交付代码包含父键索引、建表事务合并、design写事务合并、流式Shadow。已有跨批filler报告：完整读11931.378→135.813ms，完整写2277.177→400.614ms；原始iEDA DEF为47.484/6.855ms。仍未快于原始DEF，且不是统一同批最终版本复测。见[整体总结](../experiment-summary.md)。

本聊天的新SQLite A/B结果：100万条百行INSERT比单行写降低约36%～38%，仍慢于文本；三表切换调整没有稳定的大幅收益。见[结果](sqlite-write/results.md)。这里的旧data时间排除提交，不与下述独立目录完整持久化时间相减。

**独立三库实验已由其他工作继续开展。** [三库计划](db-text-plan.md)已标为历史设计；下列目录的结果不在本仓库内，也不自动随本次提交归档。最新入口：

```text
/home/zhiyiwang/cs/db/sqlite/bench/README.md
/home/zhiyiwang/cs/db/sqlite/bench/results-brief.md
/home/zhiyiwang/cs/db/sqlite/bench/final-report.md
/home/zhiyiwang/cs/db/duckdb/bench/parquet-nested-handoff.md
```

独立报告使用SQLite 3.54.1、DuckDB 1.5.6、RocksDB 11.8.1；当前iEDA旧基线的系统SQLite为3.37.2。不要把不同版本/计时的数字混成一次实验。

### 平面Parquet结果支持到哪里

100万条、8字段，无PK/FK/NULL；固定组name8字节、master_name10字节，变长组master_name10～106字节且内容重复度改变。实际文本42.60／88.37MiB。串行固定CPU0、5次中位数、完整持久化写和warm全字段恢复：

| 数据 | 方法 | write ms | read ms |
| --- | --- | ---: | ---: |
| fixed | 常规流文本 | 1379.54 | 742.57 |
| fixed | Parquet无压缩 | 908.33 | 323.27 |
| fixed | Parquet Zstd | 770.30 | 361.39 |
| varied | 常规流文本 | 2292.73 | 891.25 |
| varied | Parquet无压缩 | 2209.08 | 537.57 |
| varied | Parquet Zstd | 2160.66 | 639.79 |
| varied | 优化文本 | 1720.43 | 496.13 |

fixed两种Parquet读写五轮配对均更快；varied读更快，写差值有正有负。varied优化文本中位数快于两种Parquet，不能说“比所有文本快”。以上从现有报告读取，本次未重跑或重新独立审计。

实际实现：DuckDB C API，threads=1、memory_limit=512MB，Chunk最多2048行；Record→内存暂存表→COPY Parquet→文件/目录同步；读取read_parquet→查询结果→Record。Parquet V1，row group122880，UNCOMPRESSED/ZSTD。不能漏计暂存表填充、COPY、结果物化或对象转换。

源证据在`/home/zhiyiwang/cs/db/sqlite/results/direct-fixed-20261022/`及`direct-varied-20261022/`：summary-text-stream.json、samples.jsonl、manifest.json、audit-text-stream.json。先核对这些证据及最新报告，再引用。

## 3. 应在什么文件夹做什么

| 位置 | 工作 |
| --- | --- |
| `/home/zhiyiwang/cs/db/duckdb/bench/` | 独立验证DuckDB原生表、Parquet、LIST/STRUCT、共享引用、更新代价；先不依赖iEDA |
| `/home/zhiyiwang/cs/db/sqlite/bench/common/` | 复用现有生成器、文本基线和校验，不复制另一份慢文本 |
| `/home/zhiyiwang/cs/db/duckdb/results/` | 新run-id，保存样本、配置、版本、审计，不覆盖已有数据 |
| iEDA仓库`src/database/edadb/core/` | 真正的EDADB后端实现；这是独立Git子模块，需独立提交与记录gitlink |
| core的`include/edadb/backend/duckdb/`与`src/core/backend/duckdb/` | 建议新增的后端目录，当前未实现；不在SQLite目录直接替换函数 |
| iEDA的`src/database/edadb/idb/` | schema/Shadow映射与初始化；保留adapter语义，只在确需改动时扩展 |
| iEDA的`src/database/manager/builder/def_builder/` | DEF/EDADB写入及恢复入口、对象关联、顺序与事务边界 |
| iEDA的`src/database/edadb/docs/`、`test/` | 设计、正确性与端到端性能；先检查目标分支的AGENTS和现有目录 |

建议独立EDADB目录`/home/zhiyiwang/cs/arch/eda/edadb`从`milestone/ieda-edadb-profiled-v2`解析出的core `494ce7916bce741f1629a33bd03be0a2043231cd`创建`codex/duckdb-backend`；其iEDA配对提交为`1ec76562a4aeb740e30ba2b41b93398327e6be7d`，之后可用于`codex/duckdb-integration`。独立edadb当前仍在旧main `5044977`，需先fetch并核对标签；**本次不创建、不切换**。该milestone包含P1/P3/P4/P5及profiling验收，不含P2；P3/P4/P5主要在iEDA adapter而非core。优化v1和profiled-v2的core相同。

当前prof-test用于测试，不覆盖已优化产品代码。开发分支另保存root-stream WIP（iEDA ad6800b72、core codex/net-table-read的21d5b62）；不默认引入。核心开发在独立edadb提交，再由iEDA子模块检出准确SHA验证，先push core后push引用它的iEDA；不复制源码同步。接手时重新检查分支和脏文件。

## 4. 代码阅读与修改地图

以下均相对iEDA仓库；实查基于当前prof-test，目标开发提交还需复核。

1. `core/include/edadb.h`：公开API与事务、CRUD、reader/writer生命周期。
2. `core/include/edadb/Config.h`：backend_type目前编译期固定SQLITE。
3. `core/include/edadb/DbBackendType.h`：已有DUCKDB枚举，**不代表后端已经实现**。
4. `core/include/edadb/{DbManager,DbStatement,DbTableOperator,SqlStatement}.h`：模板/别名分派；DbManager还直接include sqlite3与SQLite statement，需审计解耦而非只切枚举。
5. `core/include/edadb/backend/sqlite/`：连接、prepare/bind/取列、DDL及CRUD；逐项列出sqlite特定行为。`src/core/backend/`当前仅sqlite。
6. `core/include/edadb/{Table4Class,DbObjectTraverser,DbTableDefBuilder,PrimitiveVectorRow}.h`：字段元数据、对象遍历、子表/顺序映射；优先复用，不另造动态通用对象图。
7. `idb/edadb_idb_schema.h`、Shadow头、`idb/edadb_idb_init.cpp`，再读`def_write_edadb.cpp`与`def_read_edadb.cpp`：最终iDB构造、共享引用及顺序副作用。
8. core和backend的CMakeLists：选择依赖、构建测试；切DuckDB时不能仍无意依赖SQLite专有头/类型。

重点审计：SQLite PRAGMA、rowid生成及last_insert_rowid、SQL方言、参数与类型、NULL、返回码、reset语义、约束、事务/savepoint和失败处理。DuckDB不能靠替换sqlite3函数名实现兼容。候选写用Appender/Chunk，读用Chunk；不得为了模拟step而复制整批数据或对每行重新发SQL。

## 5. 先决定的架构：数据库后端不等于Parquet文件

DuckDB是查询/事务引擎；Parquet是快照文件格式。COPY导出的多个文件不是自动具备原数据库事务、PK/FK和更新语义。

建议首先评估两条分开的路径：
- **DuckDB原生后端**：保持现有关系schema/CRUD语义，支持局部更新；先验证平面和父子图，再接adapter。用于隔离“换引擎”效果。
- **Parquet快照API**：显式export/import或readSnapshot/writeSnapshot，不静默接管updateObject。独占子对象可进一步采用LIST/STRUCT；共享对象保留稳定ID和引用。

现有updateObject历史语义为删除旧root对象图再插入，需在拟用提交核实并保留；不要顺便改为字段UPDATE。局部修改发生在iDB内存，后续需要写回，不得因快照更快就删除这一需求。

多文件快照应使用独立临时generation、schema/version与文件清单、必要同步、验证后统一发布manifest；失败保持旧generation可用。重命名与同步协议须明确文件系统前提，不能宣称COPY自身提供跨文件原子发布。旧SQLite库保留读取/迁移入口，不直接就地改格式。

## 6. 最小验证与验收步骤

1. **契约表**：逐项列出旧API语义、DuckDB支持方式、Parquet限制及暂不支持项。包含空对象、NULL、vector位置、稳定身份、共享Pin/master引用、环、更新及回滚；不要假设对象图全是独占树。
2. **独立模型**：先两/三层小型对象图，包含空vector、重复业务值、乱序身份、共享引用、失败。对比关系表、DuckDB嵌套表、Parquet嵌套三组，先不引入iEDA复杂度。现有嵌套handoff尚是计划，不冒称已验证。
3. **最小后端**：经用户批准添加连接/事务、schema、批量插入、完整读取、测试；SQLite默认路径不变。仅支持部分API时明确报不支持，不能静默降级或跳过检查。
4. **adapter验证**：选择代表root，再完整filler及routed压力；实际SQL/查询次数单独诊断，恢复iDB需验证字段、vector顺序、共享对象身份及反向引用，不能只对行数。
5. **性能验收**：同批原始DEF、现行优化SQLite、DuckDB原生、DuckDB＋Parquet；写入全流程和读完整对象恢复都报告，init/create/转换/执行/commit/export/close另列。记录RSS、临时空间、文件大小、线程、压缩、冷热状态。未做部分阶段独占计时的，不伪造归因。
6. **判断**：平面读快但完整iDB不快，需分析转换和关系恢复；写慢则检查暂存/复制/导出。不要仅以查询次数降低验收。最终结论分读、写、内存、更新语义，明确哪项尚未满足。

第一轮交付应为实现计划而非直接大改：文件清单、接口草案、伪代码/SQL、兼容矩阵、测试矩阵、风险与待确认项。编译/功能测试可并行，性能串行；长任务输出心跳。没有用户授权不提交/推送、不移动milestone。

## 7. 官方依据与交接阅读顺序

先读独立`results-brief.md`→`parquet-nested-handoff.md`→本页第4节代码→提出方案。官方：[DuckDB Appender](https://duckdb.org/docs/current/data/appender)、[Parquet读写](https://duckdb.org/docs/current/data/parquet/overview)、[导出数据库](https://duckdb.org/docs/current/sql/statements/export)。实现时使用实际固定版本1.5.6对应文档/源码，不照搬preview接口。

本次仅检查文件和撰写交接，没有迁移后端、运行新性能实验或操作分支。跨仓库目录在当前文档中使用完整路径便于新agent定位；仓库内文档链接保持相对路径。
