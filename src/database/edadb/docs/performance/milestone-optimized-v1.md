# 当前优化版本交付

两仓库使用同名annotated tag：`milestone/ieda-edadb-optimized-v1`。标签是固定交付点，分支后续可继续开发；iEDA标签中的core gitlink必须等于EDADB同名标签的提交。

## 版本及范围

- iEDA：交付分支`edadb-idb`；实现起点`bc7e7d6ba`，本次仅补交付文档，最终提交由上述标签确定。
- EDADB core：交付分支`main`，提交`494ce7916bce741f1629a33bd03be0a2043231cd`。
- 包含：子表父键索引、schema事务合并、design写事务合并、逐root流式Shadow、移除参考DEF扫描、阶段计时，以及默认关闭的可选SQL trace打印。
- 不包含历史叶子批量读取；当前仍按Segment查询Point等叶子。SQL trace不是性能优化，正式性能测试保持关闭。
- iEDA `master`、EDADB旧`dev`、既有Demo和基线标签均保持不变。

## 验证与结论

- [功能验收](acceptance.md)、[流式Shadow验证及原始结果](stream-shadow.md)：15/15回归、插入/转换失败回滚及重试、24个样本严格DEF/DB审计通过。
- routed压力用例峰值RSS降低28.20 MiB（9.55%）；write data增加0.63%，完整write增加2.34%。这是内存收益，不是写入加速，亦不推广至所有设计。
- 最后合入SQL trace仅新增两行条件打印：Release `DbManagerLifecycle`在ON/OFF构建下均通过，分别输出55/0条`[EDADB-SQL]`。日志位于`/tmp/edadb-trace-run-ON.log`和`/tmp/edadb-trace-run-OFF.log`，属于临时验证产物，不随提交保存。
- 完整功能/性能证据来自trace合并前的实现；合并后仅执行上述定向验证，没有声称重新完成全量性能实验。本次交付文档不改变执行代码。

## 暂存的历史方案

| 仓库 | 历史分支 | commit | 后续用途 |
| --- | --- | --- | --- |
| iEDA | `edadb-performance-optimization` | `53aa2e52a` | 历史Net按Wire叶子批量读取adapter |
| EDADB | `performance/optimization` | `ad0f820` | 父键前缀批量读取、分组缓存及测试 |

两条历史分支不合并、不移动、不删除。后续另行决定移植按Wire读取，或设计按表/窗口整体恢复；其历史性能不代表当前交付版本。

## 核对配对

在iEDA仓库运行：

```bash
git rev-parse milestone/ieda-edadb-optimized-v1^{commit}
git ls-tree milestone/ieda-edadb-optimized-v1 src/database/edadb/core
git -C src/database/edadb/core rev-parse milestone/ieda-edadb-optimized-v1^{commit}
```

后两条输出的core提交必须一致。构建目录、生成数据库及临时二进制不属于交付源码。
