# Performance实验交接

## 范围与阅读入口

本文件是整个performance目录的交接入口，不是某个实验的重复结果报告。

| 内容 | 入口 | 状态与边界 |
| --- | --- | --- |
| 五路线、单表8字段基线 | [baseline](storage/baseline/results.md) | 已冻结；保留原始口径和数字 |
| SQLite与EDADB API对齐 | [sqlite-vs-edadb](storage/sqlite-vs-edadb/readme.md) | 已冻结；NULL、clear及其他API因素分开 |
| 主外键实验 | [sqlite-pk-fk](storage/sqlite-pk-fk/readme.md) | 独立实验，不与单表数据量或查询方式混比 |
| SQLite与文本定位 | [sqlite-vs-text](storage/sqlite-vs-text/readme.md) | 当前工作；先A写、再A读、最后B-batch复核 |
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

- [结果](storage/sqlite-vs-text/docs/results.md)：普通用户宿主机批次32组正确性检查、80条正式计时；perf 24组采样及24组同二进制控制。
- 主批证据：`/tmp/iedadb_sqlite_text_host_full`；perf：`/tmp/iedadb_sqlite_text_perf_final`。后者含environment对照。
- 同一保存二进制在沙箱/宿主机出现显著时间差，原因尚未细分。主表只使用普通用户宿主机批次；`/tmp/iedadb_sqlite_text_full`仅为早期环境诊断，不混合统计。
- perf已安装，需sudo权限；不调整全局sysctl。通过FIFO及ACK只采样data窗口，COMMIT不在窗口内。ACK尾部NUL及A内存无DB文件的脚本问题已修复，失败样本未纳入结果。
- [源码分析](storage/sqlite-vs-text/docs/sqlite_source.md)关联实际发行版源码、官方源码与现有采样。下一步若需量化B的sync，应独立追踪系统调用及阶段边界，不改冻结基线，也不将跟踪运行时间替换正式样本。
