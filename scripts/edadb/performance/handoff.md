# Performance实验交接

## 范围与阅读入口

本文件是整个performance目录的交接入口，不是某个实验的重复结果报告。

| 内容 | 入口 | 状态与边界 |
| --- | --- | --- |
| 五路线、单表8字段基线 | [baseline](storage/baseline/results.md) | 已冻结；保留原始口径和数字 |
| SQLite与EDADB API对齐 | [sqlite-vs-edadb](storage/sqlite-vs-edadb/readme.md) | 已冻结；NULL、clear及其他API因素分开 |
| 主外键实验 | [sqlite-pk-fk](storage/sqlite-pk-fk/readme.md) | 独立实验，不与单表数据量或查询方式混比 |
| SQLite与文本定位 | [sqlite-vs-text](storage/sqlite-vs-text/readme.md) | 本轮归因、SELECT补充及perf审计完成；已冻结 |
| SQLite参数与机制 | [sqlite-reference](storage/sqlite-reference/) | 共用说明；改共享配置需先确认影响 |

## 不得突破的边界

- 起点为prof-test@513f11a39，EDADB core@d6656f08c9ae3123ce5272a344ea3d5515159fe7；不混入历史优化分支。
- 遵守[baseline冻结规则](storage/baseline/AGENTS.md)及[API对齐冻结规则](storage/sqlite-vs-edadb/AGENTS.md)。未经用户同意，不改冻结代码、文档、数字及共享依赖。
- 当前新增测试只写storage/sqlite-vs-text；不修改iEDA、adapter或EDADB生产逻辑。不得自行commit/push。
- 小写文件名、相对文档链接；readme负责运行入口，results负责结论，源码分析负责调用链。不要复制多份结果。
- 数据、DB、二进制、完整日志存仓库外独立目录；Git保留必要代码、说明、配置与结论。失败批次保留，不覆盖原证据。

## 性能实验统一口径

- Release -O3、固定版本/SQLite动态库/数据/配置；记录源码与二进制哈希。编译可-j40，性能样本必须串行。
- init、create、BEGIN、data、COMMIT、close分别统计；prepare/finalize属于data。write与read分别比较。
- 正确性检查、输入生成、缓存准备及日志输出不进入data计时；检查全部字段、数量与顺序，不能只靠摘要。
- 明确memory/file、OS缓存与SQLite连接缓存。当前A同连接first-read，B文件OS-warm新连接，不是假定cold。
- 计时开/关与perf开/关必须各有同二进制、同环境控制；目标总时间扰动不超过5%。采样占比不是精确wall time，嵌套调用占比不能累加。
- 源码证明“可能经过什么路径”，采样/对照证明“本次实际花在哪里”；不得直接把总差值称为遍历或sync成本。
- 新实验必须列出与baseline的schema、dataset生成规则、配置、计时边界及历史时间对应；记录未对齐的构建/环境/缓存条件。跨批次时间不作因果delta，冻结结果不自行替换。
- 读、写分别组织计时流程、结果表、delta和原因分析；共同schema/数据/环境只说明一次，不在一张表中混合读写成本。
- 长任务每5–8分钟汇报带时间的心跳；功能失败先定位，不推进正式性能结论。

## 当前实验交接

- 本次收尾前基点为prof-test@7ebde2b94，core为d6656f08c9ae3123ce5272a344ea3d5515159fe7；交付提交以[配对milestone](milestone.md)解析为准。测量版本以各批manifest和source快照核对，不能仅用收尾提交标识历史测量代码。
- [唯一结果记录](storage/sqlite-vs-text/docs/results.md)：主实验60组正确性、120条正式计时、24个统计组及10次独立计数；比较文本、SQLite、STATIC、10/100行INSERT与step-only读取消融，审计PASS。
- SELECT补充：16组正确性、20条正式计时、4次计数；比较星号与显式全部列的完整读取及独立prepare/finalize循环，字节码一致，审计PASS。
- perf：24组采样＋24组同二进制控制，审计PASS、无丢失样本；只采样data窗口，不能把CPU占比当作独占wall time。
- 当前正式证据分别为`/tmp/iedadb_sqlite_text_causal_host`、`/tmp/iedadb_select_host`、`/tmp/iedadb_sqlite_text_causal_perf`。旧host_full/perf_final批次不再是当前主结果，不混算。
- 主结果使用普通宿主机环境；前置沙箱批次不作为性能依据。正式运行归档二进制，避免构建替换；详见结果中的环境与批次说明。

## 本轮收尾与提交边界

- 本轮实验已获用户授权收尾、冻结并建立配对milestone；不扩大实验矩阵或替换原始性能样本。交付范围与已知限制见milestone说明。
- 本分支拟提交测试代码、方法、结果、审计程序、整体报告和状态文档；不提交生成DB、二进制、构建目录及完整日志。
- 后续生产优化建议保留在本地记录中，不纳入本轮基线提交；导航不依赖该被忽略文件。基线的测量限制属于当前实验，应随实验文档保留。
- 本轮未量化SQLite各内部函数的精确独占时间，也未覆盖所有cold条件或验证掉电持久性；这些不应写成已通过的验收项。生产adapter细分及EDADB优化实作是独立后续工作，不阻塞本轮已测范围的归档。
