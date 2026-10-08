# Milestone汇报：P1/P3/P4/P5

## 版本与范围

两仓库使用同名标签 `milestone/ieda-edadb-optimized-v1`：

| 仓库 | 本次更新分支 | 交付代码 |
| --- | --- | --- |
| iEDA | `edadb-performance-optimization-dev` | 基于 `0da489453`，本次仅更新四份文档 |
| EDADB core | `main` | `494ce79` |

iEDA的milestone标签随本次文档提交更新，源码和core gitlink不变；core同名标签仍指向 `494ce79`。旧iEDA标签位置保留为 `archive/ieda-edadb-optimized-v1-before-doc-update`。`edadb-idb`分支本次不移动；用 `git rev-parse milestone/ieda-edadb-optimized-v1^{commit}` 查看各仓库的标签提交。

**包含P1/P3/P4/P5，不含P2批量读取。** 另包含移除参考DEF扫描、粗粒度计时和默认关闭的SQL trace。后续profiling及按Wire批读尚属开发工作区，不进入这四份文档的性能表。

## 做了什么

- **P1：子表父键索引。** 对parent FK有效且无PK的子表建立完整父键复合索引，避免反复全表扫描；不减少逐父查询次数。
- **P3：一次schema事务。** 全部建表和建索引共用一次提交，减少create阶段重复提交。
- **P4：一次design事务。** 全部root数据写入共用一次提交，失败整体回滚；不是多行INSERT。
- **P5：流式Shadow。** Instance、Pin、SpecialNet、Net逐root转换、写入、释放，复用InsertOp，减少临时内存。

P3与P4是两个事务。P5主要改善内存，不能仅凭它推断读写更快。

## 阅读顺序

1. 本文件：确认交付范围。
2. **[optimization-results.md](optimization-results.md)**：第1节看数据集与来源，第2、3节看三者时间及“各版本耗时÷原始iEDA”，第4–6节看各P收益，第7节看最终汇总。优化前后加速比另用“优化前÷优化后”，不与耗时倍数混淆。
3. [transaction-batching.md](transaction-batching.md)：解释实现及对应源码。
4. [stage-timing.md](stage-timing.md)：解释每段计时边界。

## 验收边界

已优化实现的时间采用交付前流式Shadow验收；原始iEDA、未优化EDADB及各P收益采用各自已有原始记录。总表是跨批次概览，不把各P收益相加。流式Shadow测量两侧core均为 `1c4857c`；交付core `494ce79`之后合入默认关闭的SQL trace，只补开关验证，**没有在最终标签上重新跑全量性能矩阵**。

15/15功能回归、代表性插入/转换失败回滚与重试、24次严格DEF/DB往返通过；范围详见结果文档。不引入后续批读实验的28/28或58次验收数字。

## 文件与代码

服务器仓库：`/home/zhiyiwang/cs/arch/eda/iEDA-EDADB`。
文档目录相对仓库为 `src/database/edadb/docs/performance/`。四份文档可独立下载阅读，互相使用相对链接。

文件名 `demo.md` 沿用你下载的名称；历史分支 `demo/20260924` 是更早快照，未移动。当前开发工作区有新实验，核对交付源码应使用上述标签，而不是直接认定工作区等于milestone。本次只提交四份文档并更新iEDA标签，不修改生产代码、core版本或原始计时，也未重新运行性能实验。
