# iPDN PDNView 研究与工程计划

本文记录如何把 iPDN 的 PDN 构建过程接入 EDADB。重点不是替代 iPDN 算法，而是把命令、生成的 special-net 几何、via、blockage cutting 和后续 IR/DRC 影响持久化，形成可验证的 PDNView。

## 1. 代码证据

### 1.1 入口

- Tcl 入口：`src/interface/tcl/tcl_ipdn/tcl_ipdn.cpp`
- Python 入口：`src/interface/python/py_ipdn/py_ipdn.cpp`
- API 层：`src/operation/iPDN/api/ipdn_api.h`
- API 实现：`src/operation/iPDN/api/ipdn_api.cpp`

主要命令：

- `addIOPin()`
- `globalConnect()`
- `placePdnPort()`
- `createGrid()`
- `createStripe()`
- `connectLayerList()`
- `connectMacroToPdnGrid()`
- `connectIOPinToPowerStripe()`
- `connectPowerStripe()`
- `addSegmentStripeList()`
- `addSegmentVia()`

### 1.2 生成对象

核心实现：

- `src/operation/iPDN/source/module/pdn_plan/pdn_plan.cpp`
- `src/operation/iPDN/source/module/pdn_plan/pdn_plan.h`
- `src/operation/iPDN/source/module/pdn_plan/pdn_cut_stripe.cpp`
- `src/operation/iPDN/source/module/pdn_plan/pdn_cut_stripe.h`
- `src/operation/iPDN/source/module/pdn_plan/pdn_plan_macro.cpp`
- `src/operation/iPDN/source/module/pdn_via/pdn_via.cpp`
- `src/operation/iPDN/source/module/pdn_via/pdn_via.h`

主要修改的 iDB 对象：

- `IdbSpecialNet`
- `IdbSpecialWire`
- `IdbSpecialWireSegment`
- `IdbVia`
- `IdbPin`
- `IdbLayerShape`

关键行为：

- `PdnPlan::addIOPin()` 创建/查找 special net 和 IO pin。
- `PdnPlan::globalConnect()` 创建/查找 special net，并记录 pin pattern。
- `PdnPlan::placePdnPort()` 为 IO pin 创建 port geometry。
- `PdnPlan::createGrid()` 根据 row/core/blockage 创建 follow-pin special wire segment。
- `PdnPlan::createStripe()` 创建 power/ground stripe。
- `PdnPlan::createSpecialWireSegment()` 设置 layer、width、shape type、points、bbox。
- `PdnPlan::createSpecialWireSegmentWithInBlockage()` 按 routing blockage 切分 segment。
- `PdnPlan::connectTwoLayerForWire()` 在上下层 segment 交点插入 via segment。
- `CutStripe` 支持 stripe 连接、切割和 segment array 更新。
- `PdnVia::findVia()` 查找或创建 via master。
- `PdnVia::createSpecialWireVia()` 创建 via 类型 special wire segment。
- `PdnVia::addSegmentVia()` 将 via segment 加到指定 special net。

## 2. 与 iPNP/iIR 的区别

- iPDN 是命令式 PDN 构造器：输入 Tcl/Python/API 命令，输出 iDB special-net geometry。
- iPNP 更像 PDN 规划/优化器：关注 template、region、score、优化轨迹。
- iIR 是电源网分析器：关注 conductance matrix、current vector、IR result。
- EDADB 中建议分三层 view：
  - `PDNView`：记录 iPDN 命令和生成几何。
  - `PowerIRView`：记录 iPA/iIR power、current、IR drop、hotspot。
  - `PDNOptimizationView`：记录 iPNP search action、score 和候选方案。

## 3. EDADB PDNView schema 建议

### 3.1 Run / command

| 表 | 关键字段 | 含义 |
| --- | --- | --- |
| `PdnRun` | `run_id, design_name, stage_id, script_hash, dbu, created_at` | 一次 PDN 构建或增量修改。 |
| `PdnCommand` | `run_id, cmd_id, order_id, command_name, raw_args, status` | Tcl/Python/API 命令调用序列。 |
| `PdnCommandInput` | `run_id, cmd_id, key, value` | 结构化命令参数，避免只存 raw string。 |

### 3.2 Special net / connect

| 表 | 关键字段 | 含义 |
| --- | --- | --- |
| `PdnSpecialNet` | `run_id, net_name, net_type, connect_type, source_cmd_id` | power/ground special net。 |
| `PdnGlobalConnect` | `run_id, cmd_id, net_name, pin_pattern, is_power` | `globalConnect()` 语义。 |
| `PdnIOPin` | `run_id, cmd_id, pin_name, net_name, direction, is_power` | `addIOPin()` 语义。 |
| `PdnPortShape` | `run_id, cmd_id, pin_name, io_cell_name, layer_name, llx, lly, urx, ury` | `placePdnPort()` 生成的 port geometry。 |

### 3.3 Geometry / via

| 表 | 关键字段 | 含义 |
| --- | --- | --- |
| `PdnRouteInfo` | `run_id, cmd_id, layer_name, width, pitch, offset, source_type` | `createGrid/createStripe` 的 route 参数。 |
| `PdnShape` | `run_id, shape_id, cmd_id, net_name, wire_id, segment_id, layer_name, shape_type, width, x1, y1, x2, y2, llx, lly, urx, ury` | special wire segment 的可查询形态。 |
| `PdnViaShape` | `run_id, shape_id, cmd_id, net_name, via_name, cut_layer, top_layer, bottom_layer, x, y, width, height, generated_via` | via segment 和 via master。 |
| `PdnBlockageCut` | `run_id, cmd_id, original_shape_id, result_shape_id, blockage_id, layer_name` | 一个 segment 被 blockage 切成多个 segment 的 provenance。 |
| `PdnDirtyRegion` | `run_id, dirty_id, cmd_id, layer_name, llx, lly, urx, ury, reason` | 给 iDRC/iIR/routing 的增量区域。 |

## 4. 写入时机

### Phase 0：离线抽取

- 在 iPDN 命令执行完后，从 iDB 的 `IdbSpecialNetList` 抽取最终 special-net geometry。
- 优点：侵入最小，先验证 schema 和 query。
- 缺点：只能知道最终形态，不知道每个 shape 来自哪个命令。

### Phase 1：命令边界记录

- 在 `PdnApi::*` 边界记录 `PdnCommand` 和输入参数。
- 每个命令执行前分配 `cmd_id`。
- 命令失败时记录 status，方便 flow debug。

### Phase 2：生成点记录 source cmd

- 在 `PdnPlan::createSpecialWireSegment()`、`PdnVia::createSpecialWireVia()`、`CutStripe` 切割/连接点补充 source cmd。
- 目标是每个 `IdbSpecialWireSegment` 都能追溯到 `cmd_id`。

### Phase 3：跨工具 dirty view

- 将新增/修改的 segment bbox 写入 `PdnDirtyRegion`。
- 提供给 iDRC dirty-region check、iIR 局部更新、routing blockage/PDN overlap 检查。

## 5. 验证方法

### 5.1 iDB 一致性

- `PdnSpecialNet` 数量等于 iDB special net list 中相关 power/ground net 数量。
- `PdnShape + PdnViaShape` 数量等于 special wire segment 数量。
- 每个 `PdnShape` 的 layer、width、shape type、points、bbox 与 iDB segment 一致。
- 每个 `PdnViaShape` 的 via name、cut/top/bottom layer、坐标与 iDB segment 一致。

### 5.2 DEF 一致性

- EDADB 抽取的 special-net geometry 能重建 DEF `SPECIALNETS` 中对应字段。
- `createGrid/createStripe/addSegmentStripe/addSegmentVia` 生成的 DEF 与原 iEDA writer 输出一致。

### 5.3 Provenance 一致性

- 每个 command 产生的 shape 都能回溯到 `cmd_id`。
- 一个 command 产生多个 segment 时，所有 segment 的 `cmd_id` 相同。
- blockage cutting 后，`PdnBlockageCut` 能连接 original segment 和 result segments。

### 5.4 几何正确性

- blockage cutting 后，同层 segment 不应与 routing blockage 重叠。
- `connectTwoLayerForWire()` 生成的 via 坐标应位于上下层 segment 交点。
- `connectIOPinToPowerStripe()` 和 `connectPowerStripe()` 后，相关 stripe segment 的拓扑变化能在 EDADB 中复现。

## 6. 实验设计

| 实验 | 输入 | 验证重点 |
| --- | --- | --- |
| P1 | `addIOPin + globalConnect` | special net、pin、pin pattern。 |
| P2 | `placePdnPort` | IO pin port geometry 与 bbox。 |
| P3 | `createGrid` | follow-pin segment 数量、row/core 对齐、power/ground 分配。 |
| P4 | `createStripe` | stripe pitch、offset、方向、bbox。 |
| P5 | `addSegmentStripeList` | polyline 是否按相邻点拆成 segment。 |
| P6 | `addSegmentVia(cut_layer)` | 单 cut layer via shape。 |
| P7 | `addSegmentVia(top,bottom)` | 跨多层时是否为每个 cut layer 插入 via。 |
| P8 | `connectLayerList` | 上下层 stripe 交点 via。 |
| P9 | `connectMacroToPdnGrid` | macro pin 到 PDN grid 的 via/segment。 |
| P10 | 带 routing blockage 的 grid/stripe | segment cutting 和 dirty region。 |

## 7. 可形成的研究问题

### EDA 方向

- 如何把命令式 PDN 构造过程变成可查询、可验证、可 replay 的 persistent PDNView？
- 如何用 PDNView 支持局部 PDN ECO 后的 DRC/IR/routing 增量验证？
- 如何把 PDN generation provenance 与 IR hotspot debug 连接起来？

### Database 方向

- 如何为 PDN special-net geometry 设计 object + spatial + provenance 混合索引？
- 如何维护 command-level provenance 与 shape-level delta，避免每次重建全部 special nets？
- 如何为 PDN dirty region 自动选择 tile/layer/net 粒度？

### AI 交叉方向

- 如何用 PDNView 生成带 provenance 的 IR/PDN 训练数据？
- 如何让 agent 精确查询“哪个 PDN 命令导致了 hotspot 附近的几何变化”？
- 如何基于历史 PDN action 预测 IR/DRC 风险，并用 iIR/iDRC exact result 验证？

## 8. 风险

- iPDN 当前大量对象直接写入 iDB，若要记录 source cmd，需要少量侵入式 instrumentation。
- 一个命令可能被 blockage cutting 扩展成很多 segment，不能只做 command-level table。
- `PdnVia::findVia()` 可能创建 generated via，EDADB 必须记录 via 是否由 iPDN 自动生成。
- `createGrid()` 会影响 core/row 相关几何语义，PDNView 不能只存 special net 结果。
- 与 `PowerIRView` 集成前，先不要声称 EDADB 已经能加速 IR，只能说提供 PDN 数据底座。

## 9. 推荐下一步

1. 先做 Phase 0：从 final iDB special nets 抽取 `PdnShape/PdnViaShape`。
2. 写 validator：EDADB row count 和 iDB special wire segment count 必须一致。
3. 再在 `PdnApi::*` 加 command logger。
4. 最后把 dirty bbox 接到 iDRC/iIR 的局部验证实验。
