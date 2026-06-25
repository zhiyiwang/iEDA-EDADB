# iFP FloorplanView 研究与工程计划

本文记录如何把 iFP 的 floorplan 初始化、IO 放置、blockage/halo、tapcell/endcap/filler 插入过程接入 EDADB。核心目标是形成可查询、可 replay、可 diff 的 FloorplanView，支撑后续 placement、PDN、routing、DRC 和 flow-level DSE。

## 1. 代码证据

### 1.1 Tcl / Python / API 入口

- Tcl 注册：`src/interface/tcl/tcl_ifp/tcl_register_fp.h`
- Tcl init/track：`src/interface/tcl/tcl_ifp/tcl_init_ifp.cpp`
- Tcl IO：`src/interface/tcl/tcl_ifp/tcl_io.cpp`
- Tcl blockage/halo：`src/interface/tcl/tcl_ifp/tcl_blockage.cpp`
- Tcl tapcell：`src/interface/tcl/tcl_ifp/tcl_tapcell.cpp`
- Python 入口：`src/interface/python/py_ifp/py_ifp.cpp`
- API 层：`src/operation/iFP/api/ifp_api.h`
- API 实现：`src/operation/iFP/api/ifp_api.cpp`

主要命令：

- `init_floorplan`
- `gern_track`
- `auto_place_pins`
- `place_port`
- `auto_place_io`
- `place_io_filler`
- `add_placement_blockage`
- `add_placement_halo`
- `add_routing_blockage`
- `add_routing_halo`
- `tapcell`

### 1.2 关键实现

- `src/operation/iFP/source/module/init_design/init_design.cpp`
- `src/operation/iFP/source/module/io_placer/io_placer.cpp`
- `src/operation/iFP/source/module/tap_cell/tapcell.cpp`

关键行为：

- `InitDesign::initDie()` 重置 die polygon，并写入 die bbox 两个点。
- `InitDesign::initCore()` 设置 core/io/corner site，按 core site 对齐 core bbox，重建 rows，并设置 core bbox。
- `InitDesign::makeTracks()` 为指定 routing layer 创建 X/Y 两个 track grid，track number 由 die size、offset、pitch 计算得到。
- `IoPlacer::autoPlacePins()` 按 side、core/die bbox、manufacture grid 自动放置 IO pin port shape。
- `IoPlacer::placePort()` 以 IO cell bbox + offset 生成 pin port geometry，并设置 pin location/orient/bbox。
- `IoPlacer::autoPlacePad()` 根据 die/corner/pad site 自动放置 IO pad instance。
- `IoPlacer::autoIOFiller()` 找 IO filler master，计算边界空 interval 并插入 filler instance。
- `TapCellPlacer::tapCells()` 根据 row、placement blockage、tapcell/endcap master 和 distance 生成 fixed tapcell/endcap instance。
- `TclFpAddPlacementBlockage/AddRoutingBlockage/AddPlacementHalo/AddRoutingHalo` 直接调用 `dmInst` 写 iDB blockage/halo。

## 2. 为什么适合 EDADB

iFP 是后续物理设计 flow 的 stage 0。它的输出影响后续所有工具：

- die/core/row 决定 placement legal area。
- track grid 决定 routing grid。
- IO pin/port/pad/filler 决定 boundary constraint。
- blockage/halo 决定 placement、routing、tapcell 和 PDN 可用区域。
- tapcell/endcap/filler 是 floorplan stage 生成的 fixed physical instances。

EDADB 的价值不是加速单次 `init_floorplan`，而是：

- 保存 floorplan action log，便于 replay 和 debug。
- 保存 floorplan stage snapshot，便于与 DEF/iDB 做 diff。
- 为 DSE 保存参数与结果，避免只看最终 DEF。
- 为 iPL/iPDN/iRT/iDRC 提供 dirty region 和约束变更来源。

## 3. EDADB FloorplanView schema 建议

### 3.1 Run / command

| 表 | 关键字段 | 含义 |
| --- | --- | --- |
| `FpRun` | `run_id, design_name, stage_id, script_hash, dbu, manufacturing_grid, created_at` | 一次 floorplan stage。 |
| `FpCommand` | `run_id, cmd_id, order_id, command_name, raw_args, status` | Tcl/Python/API 命令调用序列。 |
| `FpCommandInput` | `run_id, cmd_id, key, value` | 结构化命令参数。 |

### 3.2 Die / core / rows / tracks

| 表 | 关键字段 | 含义 |
| --- | --- | --- |
| `FpDie` | `run_id, cmd_id, llx, lly, urx, ury` | `initDie()` 后的 die bbox。 |
| `FpCore` | `run_id, cmd_id, llx, lly, urx, ury, core_site, io_site, corner_site` | `initCore()` 后的 core bbox 和 site 设置。 |
| `FpRow` | `run_id, row_name, site_name, origin_x, origin_y, orient, do_num, by_num, step_x, step_y, llx, lly, urx, ury` | `initCore()` 生成的 rows。 |
| `FpTrackGrid` | `run_id, layer_name, direction, start, pitch, track_number` | `makeTracks()` 生成的 X/Y track grid。 |

### 3.3 IO / port / filler / tapcell

| 表 | 关键字段 | 含义 |
| --- | --- | --- |
| `FpIOPinPlacement` | `run_id, cmd_id, pin_name, side, x, y, layer_name, width, height, status` | `autoPlacePins()` 和 `placePort()` 的 pin placement 结果。 |
| `FpPortShape` | `run_id, cmd_id, pin_name, port_id, layer_name, rel_llx, rel_lly, rel_urx, rel_ury, abs_llx, abs_lly, abs_urx, abs_ury` | IO pin port geometry。 |
| `FpPadPlacement` | `run_id, cmd_id, inst_name, master_name, x, y, orient, status, side` | `autoPlacePad()` 结果。 |
| `FpFillerPlacement` | `run_id, cmd_id, inst_name, master_name, x, y, orient, interval_begin, interval_end, side` | IO filler 插入结果。 |
| `FpTapcellRegion` | `run_id, cmd_id, row_name, row_index, start_x, end_x, y, orient, blocked_by` | tapcell/endcap 可插入 interval。 |
| `FpTapcellPlacement` | `run_id, cmd_id, inst_name, master_name, type, x, y, orient, status, region_id` | tapcell/endcap instance。 |

### 3.4 Blockage / halo / dirty

| 表 | 关键字段 | 含义 |
| --- | --- | --- |
| `FpPlacementBlockage` | `run_id, cmd_id, llx, lly, urx, ury` | placement blockage。 |
| `FpRoutingBlockage` | `run_id, cmd_id, layer_list, llx, lly, urx, ury, except_pg_net` | routing blockage。 |
| `FpPlacementHalo` | `run_id, cmd_id, inst_name, left, bottom, right, top` | placement halo。 |
| `FpRoutingHalo` | `run_id, cmd_id, inst_name, layer_list, left, bottom, right, top, except_pg_net` | routing halo。 |
| `FpDirtyRegion` | `run_id, dirty_id, cmd_id, layer_name, llx, lly, urx, ury, reason` | 给 iPL/iPDN/iRT/iDRC 的增量区域。 |

## 4. 写入策略

### Phase 0：stage snapshot

- iFP 执行完后，从 iDB 抽取 die/core/row/track/io/blockage/instance。
- 目标是建立 full snapshot 和 validator。
- 优点：侵入小；缺点：缺 command provenance。

### Phase 1：command log

- 在 Tcl/Python/API 边界写 `FpCommand` 和 `FpCommandInput`。
- 每个命令执行前分配 `cmd_id`，执行后写 status。
- 可先只记录命令参数，不改变 iFP 内部逻辑。

### Phase 2：generated object provenance

- 在 `InitDesign`、`IoPlacer`、`TapCellPlacer` 和 blockage Tcl handler 中，把生成对象关联到 `cmd_id`。
- 重点记录 `row/track/pin port/filler/tapcell/blockage` 的来源。

### Phase 3：dirty propagation

- die/core/row 变化：影响全局 placement/routing/DRC。
- track 变化：影响 routing/DRC。
- IO/pad/filler/tapcell 变化：影响边界 placement、routing blockage、DRC。
- blockage/halo 变化：影响 placement legalization、PDN generation、routing 和 DRC。

## 5. 验证方法

### 5.1 与 iDB 一致

- `FpDie/FpCore` bbox 与 iDB layout die/core 一致。
- `FpRow` 数量和每行 origin/orient/DO/BY/STEP/bbox 与 iDB rows 一致。
- `FpTrackGrid` 数量、direction、start、pitch、track_number 与 iDB track grid 一致。
- `FpPortShape` 与 iDB IO pin term/port/layer shape 一致。
- `FpPadPlacement/FpFillerPlacement/FpTapcellPlacement` 与 iDB instance list 中对应 fixed/placed instance 一致。
- `FpPlacementBlockage/FpRoutingBlockage/Halo` 与 iDB blockage list 一致。

### 5.2 与 DEF 输出一致

- EDADB snapshot 可重建 DEF 中 `DIEAREA`、`ROW`、`TRACKS`、`PINS`、`COMPONENTS`、`BLOCKAGES` 的 floorplan 相关部分。
- 与原始 `DefWrite` 输出字段对齐，不额外要求保存 DEF 不输出的临时计算变量。

### 5.3 与计算语义一致

- `initCore()` 后 row 数量应为 `core_height / site_height`，每行 orient 按偶/奇行规则交替。
- `makeTracks()` 后 track number 应为 `(die_width - offset) / pitch` 或 `(die_height - offset) / pitch`。
- `autoPlacePins()` 的 pin port shape 应按 manufacturing grid 对齐。
- `autoIOFiller()` 的 filler 应填充 pad interval 空洞，不与已有 IO pad 重叠。
- `tapCells()` 的 tapcell/endcap 应避开 placement blockage，并使用 fixed status。

## 6. 实验设计

| 实验 | 输入 | 验证重点 |
| --- | --- | --- |
| F1 | explicit `die_area/core_area` | die/core/row snapshot。 |
| F2 | `core_util + margin + xy_ratio` | 自动推导 die/core 参数和 row 对齐。 |
| F3 | `gern_track` 多 layer | X/Y track grid 数量和 pitch/start。 |
| F4 | `auto_place_pins` 不指定 sides | 四边 IO pin 自动分布。 |
| F5 | `auto_place_pins -sides left right` | 指定边分布和 side provenance。 |
| F6 | `place_port` | IO cell bbox + offset 生成 port shape。 |
| F7 | `auto_place_io` | pad instance 坐标、orient、status。 |
| F8 | `place_io_filler` | filler interval 覆盖和不重叠。 |
| F9 | placement/routing blockage + halo | blockage/halo table 与 iDB 一致。 |
| F10 | `tapcell` with/without blockage | region 切分、tapcell/endcap 插入和 fixed status。 |

## 7. 可形成的研究问题

### EDA 方向

- 如何把 floorplan stage 从一次性脚本执行变成可 replay、可 diff、可验证的 design state？
- 如何用 FloorplanView 解释后续 placement/routing/DRC 问题的来源？
- 如何在 DSE 中复用 floorplan snapshot，减少重复初始化和失败定位成本？

### Database 方向

- 如何为 stage command log 和 generated object snapshot 建立双向 provenance？
- 如何维护 die/core/row/track/blockage 变更后的跨工具 dirty region？
- 如何在 object table 与 spatial table 之间保持 floorplan geometry 一致？

### AI 交叉方向

- 如何从 FloorplanView 生成 stage-aware ML 数据，用于预测 congestion、timing、IR 和 placement difficulty？
- 如何让 agent 精确查询“哪个 floorplan 命令导致了后续 DRC/placement 问题”？
- 如何用历史 floorplan action + QoR 训练 DSE planner，同时保留 exact iEDA validator？

## 8. 风险

- 单独 iFP 性能收益不强，论文主线应与 iPL/iPDN/iDRC/Flow DSE 结合。
- `autoPlacePins()` 和 `placePort()` 对 pin/term/port 的处理存在多路径，validator 需要覆盖普通 IO pin 和 special-net pin。
- `tapCells()` 依赖 placement blockage 和 row 几何，blockage 边界条件需要重点测。
- `initCore()` 会 reset rows，必须记录 command 顺序，否则 snapshot 无法解释旧 row 消失原因。
- `gern_track` 命令名拼写来自现有 Tcl 注册，文档和实验脚本要按当前代码处理。

## 9. 推荐下一步

1. 先做 Phase 0：从 iDB 抽取 FloorplanView snapshot。
2. 写 validator：row/track/pin/instance/blockage count 和关键字段对齐。
3. 在 Tcl/Python/API 边界加 command log。
4. 将 `FpDirtyRegion` 接到 iPL/iPDN/iDRC 的增量实验。
