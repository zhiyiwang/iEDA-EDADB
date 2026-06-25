# iEDA FlowMemory / DSE Memory 研究与工程计划

本文记录如何把 iEDA 的 Tcl flow、ToolManager 调度、tool_io runtime/memory、feature/report 输出和 EDADB stage view 连接起来，形成 FlowMemory。目标不是再做一个脚本日志系统，而是把每次 flow 的配置、stage 输入输出、QoR、failure、object delta 和 provenance 统一入库，用于 DSE、debug、ML dataset 和 agent memory。

## 1. 代码证据

### 1.1 命令注册和脚本驱动

- Tcl 总注册：`src/interface/tcl/tcl_register.h`
- Flow 命令注册：`src/interface/tcl/tcl_flow/tcl_register_flow.h`
- Flow config 命令：`src/interface/tcl/tcl_flow/tcl_flowconfig.cpp`
- 默认 flow config：`src/interface/default_config/flow_config.json`
- 设计脚本：`scripts/design/*/script/*/run_*.tcl`

当前 Tcl 注册会统一注册 DB、iFP、iPDN、iPL、iCTS、iNO、iTO、iRT、iDRC、iSTA、Power、iPNP、report、feature、ECO、vectorization 等命令。实际 flow 主要由 `run_*.tcl` 脚本按阶段串联：

```text
flow_init / flow_config
db_init / lef_init / def_init
run_iFP / run_iPL / run_iCTS / run_iTO / run_iRT / run_drc / run_sta / ...
def_save / netlist_save / report_db / feature_summary / feature_tool
flow_exit
```

### 1.2 ToolManager 调度

- 总入口：`src/platform/tool_manager/tool_manager.h`
- 实现：`src/platform/tool_manager/tool_manager.cpp`
- tool_io：
  - `src/platform/tool_manager/tool_api/ipl_io/ipl_io.cpp`
  - `src/platform/tool_manager/tool_api/icts_io/icts_io.cpp`
  - `src/platform/tool_manager/tool_api/ino_io/ino_io.cpp`
  - `src/platform/tool_manager/tool_api/ito_io/ito_io.cpp`
  - `src/platform/tool_manager/tool_api/idrc_io/idrc_io.cpp`
  - `src/platform/tool_manager/tool_api/ista_io/ista_io.cpp`
  - `src/platform/tool_manager/tool_api/ipw_io/ipw_io.cpp`
  - `src/platform/tool_manager/tool_api/ipnp_io/ipnp_io.cpp`

`ToolManager` 分发到各点工具：

- `autoRunPlacer()` / `runPlacerFiller()` / `runPlacerIncrementalFlow()`
- `RunNOFixFanout()`
- `autoRunTO()` / `RunTODrv()` / `RunTOHold()` / `RunTOSetup()`
- `autoRunCTS()`
- `autoRunRouter()`
- `autoRunDRC()`
- `autoRunSTA()` / `initSTA()` / `runSTA()`
- `autoRunPower()`
- `autoRunPNP()`

### 1.3 现有状态和 QoR 输出

- Flow config：`src/platform/flow/config/flow_config.h`
- Flow config 解析：`src/platform/flow/config/flow_config.cpp`
- Feature builder：`src/feature/builder/feature_builder.cpp`
- Feature Tcl：`src/interface/tcl/tcl_feature/tcl_feature.cpp`
- DB report Tcl：`src/interface/tcl/tcl_report/tcl_report_db/tcl_report_db.cpp`

当前已有基础状态：

- `PLFConfig::get_*_path()` 保存各工具 config path。
- `PLFConfig::set_status_stage()` 保存当前 stage 字符串。
- `PLFConfig::add_status_runtime()` 累加 runtime。
- `PLFConfig::set_status_memmory()` 保存 memory delta。
- `FeatureBuilder::buildSummaryInfo()` 已把 eda version、design name、stage、runtime、memory 写入 feature summary。
- `feature_summary`、`feature_tool`、`report_db` 已在脚本中保存 JSON/report。

这说明 FlowMemory 第一版不需要从零发明指标体系，可以先统一收集现有 status、feature JSON、report 和各工具 view。

## 2. 为什么适合 EDADB

当前 iEDA flow 的数据分散在：

- Tcl 脚本和环境变量。
- flow_config/tool config JSON。
- 中间 DEF/Verilog/SPEF/SDC/report。
- 各点工具内部数据结构。
- feature/report JSON。
- terminal log。

EDADB 可以把这些变成可查询的 run/stage/version/provenance：

- 每次 run 有唯一 `run_id`。
- 每个 stage 有 `stage_id`、tool、config、input/output artifact、runtime、memory、status。
- 每个 stage 可以关联已有 tool view，例如 `FloorplanView`、`PlacementView`、`ClockTreeView`、`RoutingView`、`GeometryView`、`TimingView`、`PowerIRView`。
- DSE 可以按 config/QoR/failure 查询历史，而不是扫目录和解析日志。
- agent 可以用 SQL/API 查询 design state，而不是只读文本 report。

## 3. EDADB FlowMemory schema 建议

### 3.1 Run / stage

| 表 | 关键字段 | 含义 |
| --- | --- | --- |
| `FlowRun` | `run_id, design_name, pdk, benchmark, git_commit, edadb_commit, start_time, end_time, status, workspace` | 一次完整或局部 iEDA run。 |
| `FlowStage` | `run_id, stage_id, order_id, stage_name, tool_name, command_name, status, start_time, end_time, runtime_s, memory_mb` | 一个工具阶段或关键命令。 |
| `FlowStageConfig` | `run_id, stage_id, config_path, config_hash, config_json, env_hash` | stage 使用的配置和环境。 |
| `FlowStageArtifact` | `run_id, stage_id, artifact_kind, path, file_hash, producer_stage, consumer_stage` | DEF/Verilog/report/feature/log 等输入输出文件。 |

### 3.2 QoR / feature / report

| 表 | 关键字段 | 含义 |
| --- | --- | --- |
| `FlowMetric` | `run_id, stage_id, metric_name, value, unit, source` | runtime、memory、HPWL、WNS、TNS、DRC count、wire length、IR drop 等统一指标。 |
| `FlowFeatureSummary` | `run_id, stage_id, json_path, json_hash, design_dbu, die_area, core_area, inst_count, net_count, pin_count` | `feature_summary` 的可查询摘要。 |
| `FlowReport` | `run_id, stage_id, report_kind, path, hash, parser_status` | `report_db`、CTS/DRC/STA/Power 等报告文件索引。 |
| `FlowFailure` | `run_id, stage_id, error_type, message, log_path, object_kind, object_key` | 失败原因、日志位置和相关对象。 |

### 3.3 Version / delta / provenance

| 表 | 关键字段 | 含义 |
| --- | --- | --- |
| `DesignVersion` | `run_id, version_id, parent_version_id, stage_id, snapshot_kind, snapshot_hash` | stage 后的 design version。 |
| `StageDelta` | `run_id, stage_id, object_kind, object_key, change_type, old_version, new_version, bbox, reason` | object-level delta。 |
| `StageViewLink` | `run_id, stage_id, view_name, view_run_id, table_prefix, status` | 关联各工具独立 view。 |
| `StageProvenance` | `run_id, stage_id, output_kind, output_key, dependency_kind, dependency_key` | QoR/report/object 与输入对象、配置、artifact 的依赖。 |

### 3.4 DSE / agent

| 表 | 关键字段 | 含义 |
| --- | --- | --- |
| `DseExperiment` | `exp_id, objective, search_space_hash, baseline_run_id, created_at` | 一组参数搜索实验。 |
| `DseTrial` | `exp_id, trial_id, run_id, params_json, status, score, rank` | 一次 trial 和对应 flow run。 |
| `DseParam` | `exp_id, trial_id, key, value, source_stage` | 结构化参数。 |
| `AgentQueryLog` | `run_id, query_id, stage_id, question, query_api, result_hash, action_taken` | agent/debug 查询日志。 |

## 4. 写入策略

### Phase 0：外部采集

- 不改 iEDA C++ 逻辑，先从脚本、目录、feature/report JSON、DEF/Verilog 文件 hash 建 FlowRun/FlowStage/Artifact/Metric。
- 优点：侵入最小，能快速支撑 DSE 汇总。
- 缺点：stage delta 和 object provenance 弱。

### Phase 1：ToolManager / tool_io 插桩

- 在 `ToolManager` 或各 `tool_io` 的 run 函数入口/出口写 `FlowStage`。
- 使用 `Stats` 采集 runtime/memory 的位置同步写 `FlowMetric`。
- 失败时记录 `FlowFailure` 和 stage status。

### Phase 2：接入 tool view

- iFP 写 `FloorplanView` 后，在 `StageViewLink` 关联。
- iPL 写 `PlacementView`。
- iNO 写 `NetlistECOView`。
- iDRC 写 `GeometryView`。
- iRT/iCTS/iSTA/iPA/iIR/iPNP 分别关联已有计划中的 view。

### Phase 3：object delta / time travel

- 每个 stage 前后抽取关键 object fingerprint。
- 生成 `StageDelta`，支持 stage diff 和 blame query。
- 对 ECO/增量工具优先记录 dirty object，而不是全量 diff。

### Phase 4：DSE / agent memory

- DSE trial 直接从 `FlowMetric` 和 `FlowFailure` 查询历史。
- agent query 通过 EDADB API 查询 stage/object/provenance，不直接解析散落日志。

## 5. 验证方法

### 5.1 run/stage 完整性

- 每个脚本执行的关键 command 都能在 `FlowStage` 中找到。
- stage order 与 Tcl 脚本顺序一致。
- 每个 stage 的 input/output artifact path 存在，hash 可复算。

### 5.2 runtime/memory 一致性

- `FlowStage.runtime_s` 与 tool_io 中 `Stats::elapsedRunTime()` 一致。
- `FlowStage.memory_mb` 与 `Stats::memoryDelta()` / `flowConfigInst->get_status_memmory()` 一致。
- `FlowMetric` 与 feature/report 中同名指标一致。

### 5.3 QoR 一致性

- `FlowFeatureSummary` 与 `feature_summary` JSON 一致。
- DRC violation count 与 iDRC report/detail JSON 一致。
- STA WNS/TNS 与 STA report 或 TimingView 一致。
- placement HPWL/density 与 PlacementView 或 placement report 一致。
- route wire length/violation/congestion 与 RoutingView/report 一致。

### 5.4 delta / provenance 一致性

- `StageDelta` 中新增/删除/修改对象能从 stage 前后 snapshot 复算。
- `StageViewLink` 的 view row count 与对应工具 view 一致。
- failure stage 能查询到相关 artifact、log 和最近 object delta。

## 6. 实验设计

| 实验 | 输入 | 验证重点 |
| --- | --- | --- |
| D1 | 单 stage iPL 脚本 | FlowRun/FlowStage/Artifact/Metric 基础闭环。 |
| D2 | iNO -> iPL -> iDRC | ECO dirty set、placement metric、DRC result 跨 stage provenance。 |
| D3 | iFP -> iPL -> iCTS -> iRT -> iDRC | stage order、artifact chain、summary QoR。 |
| D4 | 修改 iPL config 参数 | DSE trial 与 QoR drift。 |
| D5 | 人工制造 DRC failure | FlowFailure 和 blame query。 |
| D6 | 多 PDK/多 design sweep | run metadata、config hash、feature schema 稳定性。 |
| D7 | agent debug benchmark | log-only vs EDADB-query agent 的定位成功率和工具调用次数。 |

## 7. 可形成的研究问题

### EDA 方向

- 如何把开源 physical design flow 变成可 replay、可 diff、可 blame 的 stage database？
- 如何用 stage memory 降低 DSE 失败定位成本和重复实验成本？
- 如何用跨 stage provenance 解释 QoR drift，例如哪个 floorplan/placement/routing 改动导致 DRC/timing 退化？

### Database 方向

- 如何设计 run/stage/version/delta schema，支持 EDA object graph、spatial geometry、timing graph 和 report metric 的统一查询？
- 如何在多 stage snapshot 和 object delta 之间权衡存储成本、查询延迟和可解释性？
- 如何为 DSE workload 设计 provenance-aware materialized views 和 data skipping？

### AI 交叉方向

- 如何把 FlowMemory 作为 agent memory，使 agent 能精确查询 design state、失败原因和候选修复动作？
- 如何从 FlowMemory 生成 stage-aware ML dataset，避免 feature/report 与真实 object state 脱节？
- 如何用历史 run/trial 训练 DSE planner，并用 exact iEDA flow 验证建议参数？

## 8. 与其他 view 的关系

FlowMemory 是 glue layer，不替代各工具 view：

- `FloorplanView`：提供 iFP stage 的 die/core/row/track/io/blockage/tapcell。
- `NetlistECOView`：提供 iNO/iTO stage 的 dirty inst/net/pin。
- `PlacementView`：提供 iPL HPWL/bin density/legalization。
- `ClockTreeView`：提供 iCTS tree/topology/metric。
- `RoutingView`：提供 iRT route/gcell/violation。
- `GeometryView`：提供 iDRC shape/violation/dirty region。
- `TimingView`：提供 iSTA/iTO affected cone/path/slack。
- `PowerIRView/PDNView`：提供 power/IR/PDN geometry 和 hotspot。

FlowMemory 负责记录这些 view 的版本、来源、依赖和跨 stage 关系。

## 9. 风险

- 只保存 log/report 不够，必须关联 object delta 和 tool view，否则数据库贡献弱。
- 每个 stage 全量 snapshot 可能存储成本高，第一版应结合 summary + selected object fingerprint。
- ToolManager 层看不到所有 Tcl command 细节，Tcl 层插桩和 tool_io 层插桩需要配合。
- 当前 `ToolManager::autoRunRouter()` 中 routing 调用被禁用，FlowMemory 文档和实验要按实际可运行状态处理。
- DSE 方向不能只做随机 sweep；需要提出 reuse、failure diagnosis、provenance query 或 learned planner 的实质贡献。

## 10. 推荐下一步

1. Phase 0：写外部采集脚本，把现有 `run_*.tcl` 输出目录、feature/report JSON 和 file hash 入库。
2. Phase 1：在 tool_io run 入口/出口插桩，记录 `FlowStage` 和 runtime/memory。
3. Phase 2：把 iNO/iPL/iDRC 最小闭环的 view 都挂到 `StageViewLink`。
4. Phase 3：做一个 DSE 小实验：改变 iPL config 或 floorplan utilization，比较 FlowMemory 查询 vs 手工扫目录。
