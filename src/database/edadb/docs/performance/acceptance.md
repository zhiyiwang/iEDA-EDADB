# 当前事务优化版本：功能验收

## 结论

**15/15回归、adapter中途失败回滚、同进程重试、事务契约与阶段计时器测试均通过。** 本轮不测性能，不更新原有毫秒数。

被测生产代码为iEDA `5dae4bb9c`、core `1c4857c`；仅新增测试程序和文档，没有修改adapter/core生产实现，也没有移动`demo/20260924`分支。

## 1. 中途写入失败：验证的是adapter，不只是SQLite

使用原正式Release二进制，通过测试专用`LD_PRELOAD`库，在**第二条Net INSERT**之前安装临时触发器，让真实SQLite执行`RAISE(ABORT)`并返回约束错误。不是伪造成功返回，也不是测试程序代替adapter调用ROLLBACK。

| 检查点 | 实际结果 |
| --- | --- |
| 注入前同一事务已有数据 | 2,604个Instance、1个Net；autocommit=0 |
| 第二条Net INSERT | SQLite返回19（约束错误） |
| adapter错误传播 | Tcl `catch`检测到`edadb_write`失败 |
| adapter执行ROLLBACK后 | 返回0；autocommit=1；Instance和Net均为0 |
| 重试前独立只读连接检查 | 41张业务表全部为空，40个索引保留，integrity/FK检查通过 |
| 同一iEDA进程再次写入 | 成功，注入仅执行一次 |
| 新进程读取重试后的DB | 输出DEF与原生DEF严格相等 |

临时触发器只终止失败语句，不替adapter撤销先前数据；所有表变空以及实际ROLLBACK记录共同证明design事务回滚生效。表和索引保留，证明已经提交的schema不属于design回滚范围。

实现：[注入及事务状态检查](../../test/adapter_fault.cpp#L26)、[Tcl失败与重试](../../test/tcl/adapter_fault.tcl)、[独立检查及严格DEF对照](../../test/run_adapter_fault.py)。[审计记录](../../test/acceptance/adapter-fault.json)。

## 2. 15用例回归：没有放宽原检查

沿用[原回归脚本](../../test/run_idb_roundtrip_regression.sh)，以8路并行运行独立用例；各用例独立目录、数据库和进程。使用同一当前源码的Release诊断程序：只重编读写adapter开启日志，然后复用其余Release对象链接；原性能二进制未被替换。

这样保留原脚本的日志断言，不为适配关闭日志的性能二进制跳过检查。诊断二进制保持`-O3`，但**不能拿它输出的时间作性能结果**。

| 比较方式 | 用例 | 结果 |
| --- | --- | --- |
| 严格DEF文本一致 | default_ipl、design_fields、design_fallback、die_polygon、pin_derived、pin_writer、pin_branches、instance_branches、routed_irt、net_branches | 10/10通过 |
| 原脚本允许的根对象顺序规范化后相等 | aux_optional、group_branches、special_net_branches、grid_branches、via_branches | 5/5通过 |
| 独立数据库审计 | 所有15个DB的integrity_check和foreign_key_check | 全部通过 |

检查包含对象字段、schema、物理顺序扰动后的恢复、Pin/Via/Net等分支及部分用例再解析。不是15个结果都逐字相同，也不代表所有芯片场景已覆盖。[逐用例结果及断言数](../../test/acceptance/regression.json)。

## 3. 复现与产物

从仓库根目录执行；两个Python入口拒绝覆盖现有输出目录。回归脚本本身会清理OUT_DIR，务必指定新的测试目录。

```bash
# 正式二进制：失败、回滚与重试，不修改生产源码。
python3 src/database/edadb/test/run_adapter_fault.py \
  bin-release/iEDA /tmp/iedadb_adapter_fault_repeat

# 基于现有build-release构建日志诊断程序；仅两个翻译单元并行重编。
python3 src/database/edadb/test/build_diagnostic.py /tmp/iedadb_adapter_diag_repeat
IEDA_BIN=/tmp/iedadb_adapter_diag_repeat/iEDA \
  OUT_DIR=/tmp/iedadb_adapter_regression_repeat EDADB_TEST_JOBS=8 \
  bash src/database/edadb/test/run_idb_roundtrip_regression.sh
python3 src/database/edadb/test/audit_regression.py \
  /tmp/iedadb_adapter_regression_repeat /tmp/iedadb_adapter_diag_repeat/iEDA
```

故障注入依赖Linux动态链接SQLite；构建诊断程序要求`build-release`对象与当前源码一致。测试脚本不得用于性能测量。

- 本轮故障日志、DB及DEF：`/tmp/iedadb_adapter_fault_acceptance/`。
- 本轮15用例、日志及DB：`/tmp/iedadb_adapter_full_regression/`。
- 事务契约及计时器可执行文件、日志：`/tmp/iedadb_adapter_contracts/`；两个测试进程均退出0。
- 正式二进制SHA256仍为`be1d2de135a09960288736804aa63fd68181c5e4c3836c3e8afca4341f85f985`；诊断版本哈希记录在审计JSON中。
- 仓库只保存测试源码、小型审计JSON和说明，不纳入生成DB、DEF、二进制及构建产物。

## 4. 验收边界与下一步

本轮补齐了已约定的代表性adapter失败路径与15用例回归。没有覆盖所有root的失败点、COMMIT失败、磁盘满、进程崩溃、断电或C++异常路径，不能宣称这些场景全部安全。

两项事务优化各自的当前版本独立收益仍未补测；已有累计性能和历史逐步证据保持原样。下一步可讨论流式Shadow移植，继续采用独立修改、功能回归、串行性能测量的流程。
