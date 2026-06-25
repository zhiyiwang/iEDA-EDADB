# EDADB + iNO NetlistECOView / Fanout ECO 实验计划

本文是 `EDADB + iEDA` 研究路线中 M1 的细化计划。目标是把 iNO fanout optimization 中的 buffer insertion、net split、pin reconnect 记录成可查询的 EDADB NetlistECOView，作为后续 PlacementView / GeometryView / TimingView 的真实 dirty-set 来源。

## 1. 研究问题

核心问题：

```text
Can EDADB turn iNO fanout fixing into a persistent ECO action log and dirty-set generator for downstream placement, routing, DRC, and timing views?
```

为什么先做 iNO：

- iNO 会真实修改 iDB netlist，不需要人工伪造 ECO。
- iNO 修改对象类型集中：instance、net、pin connection。
- iNO 直接依赖 STA fanout 信息，天然连接 TimingView。
- iNO 输出 dirty inst/net/pin 后，可驱动 iPL incremental legalization 和 iDRC dirty region。

## 2. 代码证据

### 2.1 Tcl / ToolManager 入口

- `src/interface/tcl/tcl_ino/tcl_register_no.h`
  - 注册 `run_no_fixfanout`。
- `src/platform/tool_manager/tool_api/ino_io/ino_io.cpp`
  - `NoIO::runNOFixFanout()` 设置 stage `iNO - FixFanout`。
  - 调用 `NoApiInst.initNO(config)`。
  - 调用 `NoApiInst.iNODataInit(dmInst->get_idb_builder(), nullptr)`。
  - 调用 `NoApiInst.fixFanout()`。

### 2.2 NoApi 初始化路径

- `src/operation/iNO/api/NoApi.cpp`
  - `initNO()` 读取 iNO config。
  - `iNODataInit()` 初始化 iDB 和 TimingEngine。
  - `initISTA()` 创建 `TimingIDBAdapter`，把 iDB 转换成 timing netlist。
  - `fixFanout()` 调用 `_ino->fixFanout()`。

### 2.3 iNO DB interface

- `src/operation/iNO/source/io/DbInterface.cpp`
  - `get_db_interface()` 保存 config、iDB、TimingEngine。
  - `set_eval_data()` 从 TimingEngine 读取 clock WNS/TNS/freq。
- `src/operation/iNO/source/io/DbInterface.h`
  - 暴露 `get_insert_buffer()`、`get_max_fanout()`、`get_timing_engine()`、`get_idb()`。

### 2.4 Fanout 修复逻辑

- `src/operation/iNO/source/module/fix_fanout/FixFanout.cpp`
  - `fixFanout()` 遍历 STA netlist，跳过 clock net。
  - 对 `sta_net->getFanouts() > _max_fanout` 的 net，通过 `TimingIDBAdapter::staToDb()` 找到 `IdbNet`。
  - `fixFanout(IdbNet*)` 在循环中：
    - `makeNet()` 创建新 `IdbNet`。
    - `makeInstance()` 创建 buffer `IdbInstance`。
    - 找到 buffer input/output pin。
    - `connect(insert_buf, buf_input_pin, in_net)`。
    - `connect(insert_buf, buf_output_pin, out_net)`。
    - 对前 `_max_fanout` 个 load pin 做 `disconnectPin()` 和 `connect(..., out_net)`。
    - 如果 net 连接 IO port，会交换 in/out net name。

关键观察：

- iNO 已经有明确 ECO action：insert buffer、split net、move load pins。
- 但这些 action 只体现在 iDB 结果中，没有持久化 action provenance。
- EDADB 最适合在 `fixFanout(IdbNet*)` 内部记录 action，而不是事后 diff 全设计。

## 3. NetlistECOView schema

### 3.1 Run / summary

| 表 | 主键 | 字段 | 说明 |
| --- | --- | --- | --- |
| `EcoRun` | `(run_id)` | stage_id, config_hash, max_fanout, insert_buffer, before_violations, inserted_buffers | 一次 iNO fanout run。 |
| `EcoTimingSummary` | `(run_id, clock_name, phase)` | setup_wns, setup_tns, hold_wns, hold_tns, freq | 对齐 `DbInterface::set_eval_data()`。 |

### 3.2 Violation / action

| 表 | 主键 | 字段 | 说明 |
| --- | --- | --- | --- |
| `FanoutViolation` | `(run_id, vio_id)` | sta_net_name, idb_net_name, fanout, max_fanout, is_clock, fixed | 原始 fanout violation。 |
| `EcoAction` | `(run_id, action_id)` | action_type, src_net, dst_net, buffer_inst, buffer_master, renamed_net, order_id | 一次 net split / buffer insertion。 |
| `EcoInsertedInst` | `(run_id, inst_name)` | action_id, master_name, x, y, orient, source | 新插入 buffer。 |
| `EcoCreatedNet` | `(run_id, net_name)` | action_id, connect_type, original_net, is_renamed | 新建或重命名 net。 |
| `EcoMovedPin` | `(run_id, action_id, pin_name)` | inst_name, old_net, new_net, is_io_pin, order_id | 从 old net 转移到 new net 的 load pin。 |

### 3.3 Dirty set

| 表 | 主键 | 字段 | 说明 |
| --- | --- | --- | --- |
| `EcoDirtyInst` | `(run_id, inst_name)` | reason, action_id | 给 iPL/iSTA 使用。 |
| `EcoDirtyNet` | `(run_id, net_name)` | reason, action_id | 给 iRT/iSTA 使用。 |
| `EcoDirtyPin` | `(run_id, inst_name, pin_name)` | old_net, new_net, action_id | 给 iPL/iRT/iSTA 使用。 |
| `EcoDirtyBBox` | `(run_id, dirty_id)` | llx, lly, urx, ury, reason, action_id | 后续接 GeometryView / DRC。第一版可为空或由实例 bbox 估计。 |

## 4. 插桩位置

### 4.1 最小侵入位置

建议第一版只在 `FixFanout::fixFanout(IdbNet*)` 内记录：

- 进入 while 前记录 violation。
- `makeNet()` 后记录 created net。
- `makeInstance()` 后记录 inserted buffer。
- 每次 `disconnectPin()` / `connect(..., out_net)` 后记录 moved pin。
- 处理 IO port rename 时记录 `renamed_net=true`。

优点：

- 能拿到 action 上下文。
- 不需要事后复杂 diff。
- dirty set 与 iNO 实际动作一致。

### 4.2 不建议第一版做的事

- 不要先改 iNO 算法。
- 不要先做完整 iDB diff。
- 不要先接 iPL/iDRC 实时增量；先把 action log 和 dirty set 写对。
- 不要在 EDADB 中重建完整 timing graph；TimingView 后续单独做。

## 5. Correctness validator

### 5.1 Action log correctness

检查：

- `EcoInsertedInst` 中的每个 inst 在 iDB instance list 中存在。
- `EcoCreatedNet` 中的每个 net 在 iDB net list 中存在。
- `EcoMovedPin.new_net` 与 iDB pin 当前 net 一致。
- `EcoMovedPin.old_net` 不再包含该 pin。
- `EcoAction.buffer_inst` 同时连接 src_net 和 dst_net。

### 5.2 Dirty set correctness

检查：

- `EcoDirtyInst` 至少包含所有 inserted buffer。
- `EcoDirtyNet` 至少包含 old net 和 new net。
- `EcoDirtyPin` 包含所有 moved load pins 和 buffer pins。
- dirty set 能覆盖 iDB diff 中的 netlist changes。

### 5.3 Timing correctness

第一版只记录，不保证修复 QoR：

- fanout violation count before/after。
- WNS/TNS/freq before/after。
- 后续 TimingView 再做 affected cone 和 full STA 一致性。

## 6. 实验设计

| 实验 | 输入 | 检查 | 指标 |
| --- | --- | --- | --- |
| E1 | 无 fanout violation design | no action | action_count=0, dirty_count=0。 |
| E2 | 单个 high fanout net | one or more split actions | inserted buffer、created net、moved pins 正确。 |
| E3 | high fanout net with IO pin | net rename case | renamed_net 记录正确，pin current net 正确。 |
| E4 | 多个 high fanout nets | batch actions | action order、dirty set、summary count 正确。 |
| E5 | iNO -> iPL | dirty inst list | iPL incremental legalization 能接受 inserted buffers。 |
| E6 | iNO -> iDRC | dirty bbox/tile | dirty bbox 覆盖 inserted/moved objects。 |

## 7. 与后续 view 的接口

### 7.1 给 PlacementView

输出：

- inserted buffer instance list。
- moved/created net list。
- affected pin list。

用途：

- 更新 PlInst / PlNet / PlNetPin。
- 更新 affected HPWL。
- 输入 iPL incremental legalization。

### 7.2 给 GeometryView

输出：

- inserted buffer bbox。
- old/new net affected pin bbox。
- optional dirty bbox。

用途：

- 更新 affected shape/tile。
- 触发 iDRC dirty region check。

### 7.3 给 TimingView

输出：

- inserted buffer。
- split net。
- moved load pins。

用途：

- 更新 timing netlist graph。
- 计算 affected cone。

## 8. 实施阶段

Phase 0：离线 diff validator

- 不改 iNO，先跑一次 iNO 前后 DEF/iDB diff。
- 明确哪些对象会被修改。

Phase 1：action log

- 在 iNO 中写 `EcoRun/FanoutViolation/EcoAction`。
- 不接下游工具。

Phase 2：dirty set

- 写 `EcoDirtyInst/EcoDirtyNet/EcoDirtyPin`。
- validator 对齐 iDB 当前状态。

Phase 3：PlacementView handoff

- 从 EDADB query dirty inst。
- 调用 iPL incremental legalization。

Phase 4：跨阶段实验

- iNO -> iPL -> iDRC。
- full run vs incremental view correctness。

## 9. 风险

- iNO 当前 buffer placement 可能没有最终合法位置，dirty inst 给 iPL 后需要 legalization。
- IO port rename 分支可能让 old/new net 名称语义复杂，必须单独测试。
- TimingEngine 内部 STA netlist 是否同步 iDB 修改需要 review；第一版只把 iDB 作为 truth。
- 如果没有 fanout violation 的公开设计，需要构造测试 net 或调低 max_fanout。

