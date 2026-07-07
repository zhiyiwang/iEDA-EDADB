# iEDA 代码架构与物理流程完整教程

这份教程把现有 `README`、`docs/user_guide`、`md/ieda_architecture_learning`、`edadb_readme.md` 和关键源码入口整理成一条可读主线。目标不是替代所有细节文档，而是让你能从 `main()` 一路读到 DEF/EDADB、点工具和实际物理设计流程。

## 0. 如何使用这份教程

建议先把 iEDA 看成三层：

```text
用户脚本层：Tcl/Python flow，描述“我要做 floorplan、place、CTS、route”
平台调度层：Flow / Tcl command / ToolManager / DataManager，负责把命令分发出去
实现数据层：iDB + operation 点工具，真正读写设计对象并修改物理实现
```

阅读时不要从 `src/operation` 随机钻进去。先顺着“入口 -> 命令 -> 调度 -> 数据 -> 点工具”的链路读，再回头看具体算法。

## 1. 项目一句话

iEDA 是一个开源数字 ASIC 后端工具链，从 netlist/LEF/DEF/SDC/Lib 等输入开始，完成 floorplan、placement、CTS、routing、timing/power/DRC 分析和 GDS/DEF/Verilog 输出。本仓库的 `edadb-idb` 分支额外加入 EDADB，用数据库方式验证 iDB/DEF 对象的持久化读写。

## 2. 顶层模块

| 模块 | 位置 | 主要职责 |
| --- | --- | --- |
| App 入口 | `src/apps` | 生成 `iEDA` 可执行文件，解析 `-script/-log/-v` 后进入 flow。 |
| Interface | `src/interface` | Tcl/Python 命令注册与命令类实现，是脚本进入 C++ 的入口。 |
| Platform | `src/platform` | `Flow`、`ToolManager`、`DataManager` 等平台门面，连接接口、数据库和点工具。 |
| Database | `src/database` | iDB 数据结构、LEF/DEF/Verilog/GDS/JSON/EDADB 读写、builder/service。 |
| Operation | `src/operation` | iFP、iPL、iCTS、iRT、iSTA、iTO、iDRC、iPNP 等后端点工具。 |
| Evaluation/Feature | `src/evaluation`、`src/feature` | 指标评估、特征输出、报告辅助。 |
| Solver/Utility | `src/solver`、`src/utility` | 通用求解器、日志、字符串、时间、数据结构等基础能力。 |
| Scripts | `scripts/design` | 示例设计 flow，尤其 `sky130_gcd` 是当前最重要的端到端样例。 |
| Docs | `README*`、`docs`、`md` | 官方使用说明、用户手册、当前分支整理文档。 |

## 3. 全局执行链路

运行一个脚本时，主链路是：

```text
bin/iEDA -script xxx.tcl
  -> src/apps/ieda_main.cpp::main()
  -> iplf::Flow::runTcl()
  -> tcl::tcl_start()
  -> tcl::registerCommands()
  -> 具体 Tcl command class::exec()
  -> ToolManager 或 DataManager
  -> iDB builder / operation 点工具
  -> 修改 iDB，输出 DEF/Verilog/GDS/report/EDADB
```

关键文件：

- `src/apps/ieda_main.cpp`：二进制入口。
- `src/platform/flow/flow.cpp`：平台 flow 入口，当前主要进入 Tcl shell。
- `src/interface/tcl/tcl_main.h`：启动 `UserShell`。
- `src/interface/tcl/tcl_register.h`：集中注册 DB、FP、PL、CTS、RT、STA、TO、DRC、Power、PNP 等命令。
- `src/platform/tool_manager/tool_manager.cpp`：点工具调度门面。
- `src/platform/data_manager/idm*.cpp`：数据库和文件读写门面。

## 4. iDB 数据模型

iDB 是 iEDA 的核心内存数据库。理解 iDB 时先分清两侧：

| iDB 侧 | 数据来源 | 典型对象 | 物理含义 |
| --- | --- | --- | --- |
| `IdbLayout` | LEF/工艺 | layer、site、row、track、gcell、cell master、via rule | 工艺和版图约束，偏“技术库”。 |
| `IdbDesign` | DEF/Verilog/工具结果 | design、units、instance、pin、net、via、blockage、region、group、fill | 当前芯片设计状态，偏“设计实例”。 |

常见入口：

- `src/database/data/design/IdbLayout.h`：layout 根对象。
- `src/database/data/design/IdbDesign.h`：design 根对象。
- `src/database/manager/builder/builder.h/.cpp`：读写器总入口。
- `src/database/manager/builder/def_builder/def_read.cpp`：DEF 文本读入 iDB。
- `src/database/manager/builder/def_builder/def_write.cpp`：iDB 写回 DEF 文本。

## 5. 文件读写流程

### 5.1 LEF / DEF / Verilog 读入

脚本命令通常从 Tcl 进入：

```text
tech_lef_init / lef_init / def_init / verilog_init
  -> src/interface/tcl/tcl_idb/tcl_db_file.cpp
  -> DataManager::readLef/readDef/readVerilog
  -> DataManager::initLef/initDef/initVerilog
  -> IdbBuilder::buildLef/buildDef/rustBuildVerilog
  -> LEF/DEF/Verilog parser
  -> IdbLayout / IdbDesign
```

读 LEF 主要建立工艺和库对象；读 DEF 主要建立当前设计的 floorplan、component、pin、net 等对象；读 Verilog 主要补充逻辑连接关系。

### 5.2 DEF 写出

DEF writer 的核心顺序在 `DefWrite::writeChip()`：

```text
version -> divider/busbit -> design -> units -> die -> row
-> track/gcell -> via -> component -> pin -> blockage
-> region -> slot -> group -> fill -> special_net -> net -> end
```

后续做 EDADB 适配时，必须以这个顺序和字段语义为基准：原始 DEF writer 写什么，EDADB writer 就优先持久化什么；原始 DEF reader 如何重建对象，EDADB reader 就按同样语义重建对象。

### 5.3 EDADB 读写

当前分支新增命令：

```text
edadb_write
  -> DataManager::saveDefToEdadb()
  -> IdbBuilder::saveDefToEdadb()
  -> DefWriteEdadb::writeDb2Edadb()
  -> DefWriteEdadb::writeChip2Edadb()

edadb_read
  -> DataManager::readDefFromEdadb()
  -> IdbBuilder::buildDefFromEdadb()
  -> DefReadEdadb::createDbFromEdadb()
  -> createDbByDef() + createDbByEdadb()
```

`createDbByDef()` 仍需要 DEF path，是为了复用 DEF parser 的上下文初始化和未切换对象；已由 EDADB 接管的对象要关闭对应 DEF callback，防止同一类对象被 DEF 和 EDADB 重复创建。

## 6. Tcl 命令与工具调度

看一个功能时，先找命令注册，再找 `exec()`：

```bash
rg "registerTclCmd|registerCmd" src/interface/tcl
rg "class Cmd.*|::exec" src/interface/tcl
```

典型链路：

- DB 命令：`src/interface/tcl/tcl_idb/tcl_db_file.cpp` -> `DataManager`。
- Floorplan/PDN：Tcl command -> `ToolManager` -> `ifp_io` / `ipdn_io` -> iFP/iPNP。
- Placement：Tcl command -> `ToolManager::autoRunPlacer()` -> `iPL`。
- CTS：Tcl command -> `ToolManager::autoRunCTS()` -> `iCTS`。
- Routing：Tcl command -> `ToolManager::autoRunRouter()` -> `iRT`。
- STA/TO：Tcl command -> STA/TO API，读 Liberty/SDC/SPEF/DEF 后做 timing 分析和优化。

`ToolManager` 是“调度门面”，不应该把它看成算法实现；算法通常在 `src/operation/<tool>` 内。

## 7. 数字后端物理流程与代码对应

以 `scripts/design/sky130_gcd/run_iEDA.sh` 为主线：

| 阶段 | 脚本 | 物理含义 | 主要代码入口 | 主要修改对象 |
| --- | --- | --- | --- | --- |
| Floorplan | `iFP_script/run_iFP.tcl` | 定义 die/core、row、track、IO pin、tapcell、PDN 初始结构 | iFP / PDN command | `IdbDie`、`IdbRow`、pin、special net、blockage |
| Fanout 修复 | `iNO_script/run_iNO_fix_fanout.tcl` | 对高 fanout net 插 buffer 或调整连接 | iNO | instance、net、pin |
| Placement | `iPL_script/run_iPL.tcl` | 将 standard cell 放到 row/site 上 | iPL | instance 坐标、状态、合法化结果 |
| CTS | `iCTS_script/run_iCTS.tcl` | 构建 clock tree，插 clock buffer | iCTS | clock net、instance、routing guide/结果 |
| Timing Opt | `iTO_script/run_iTO_*.tcl` | 修 DRV、hold、setup 等 timing 问题 | iTO + iSTA | instance、net、buffer、尺寸/位置 |
| Incremental LG | `iPL_script/run_iPL_legalization.tcl` | 优化后重新合法化 placement | iPL | instance placement |
| Routing | `iRT_script/run_iRT.tcl` | 生成信号线详细布线 | iRT | net wire、segment、via、shape |
| DRC | `iRT_script/run_iRT_DRC.tcl` | 检查 spacing/enclosure/min-area 等规则 | iDRC | 报告和 violation 数据 |
| Filler | `iPL_script/run_iPL_filler.tcl` | 填充空白 row site，保证 well/implant 连续 | iPL/filler | fill/component |
| GDS | `DB_script/run_def_to_gds_text.tcl` | 从最终 DEF/LEF 输出版图 | DB writer | GDS/text 输出 |

这条 flow 的关键点：每一步都不是“独立生成新设计”，而是在同一个 iDB 设计状态上读入上一阶段 DEF，修改一部分对象，再写出下一阶段 DEF。

## 8. EDA 物理概念到 iDB 对象

| 物理概念 | iDB 对象 | 阅读重点 |
| --- | --- | --- |
| Die/Core | `IdbDie`、core bbox | 芯片边界和可放置区域，影响 row、placement、routing 范围。 |
| Site/Row | `IdbSite`、`IdbRow` | standard cell 对齐网格；placement 合法化依赖 row/site。 |
| Layer/Track/GCell | `IdbLayer`、`IdbTrackGrid`、`IdbGCellGrid` | routing pitch/方向/全局网格；router 依赖这些约束。 |
| Cell master/Instance | `IdbCellMaster`、`IdbInstance` | LEF macro/cell 模板与 DEF component 实例。 |
| Pin/Port | `IdbPin`、port/shape | IO pin 和 instance pin，连接 net，同时包含几何信息。 |
| Net/Wire/Via | `IdbNet`、wire、segment、`IdbVia` | 信号连接和实际金属几何，routing 阶段重点修改。 |
| SpecialNet/PDN | `IdbSpecialNet` | power/ground/clock 等特殊网络，PDN 和 DEF SPECIALNETS 相关。 |
| Blockage/Region/Group/Slot/Fill | 对应 `Idb*` 类 | 约束、分组和填充结构，影响 placement/routing 或最终版图。 |
| Timing 数据 | Liberty/SDC/SPEF + STA graph | 不完全属于 iDB；iSTA 建模时会从 iDB 和时序文件共同构造分析图。 |

## 9. 如何追一个具体功能

### 9.1 追 `def_init`

```text
Tcl: def_init
  -> CmdInitDef::exec()
  -> DataManager::readDef()
  -> DataManager::initDef()
  -> IdbBuilder::buildDef()
  -> DefRead::createDb()
  -> DEF callbacks
  -> IdbDesign / IdbLayout partial update
```

先看 `src/interface/tcl/tcl_idb/tcl_db_file.cpp`，再看 `src/platform/data_manager/idm_init.cpp`，最后看 `src/database/manager/builder/def_builder/def_read.cpp`。

### 9.2 追 `def_save`

```text
Tcl: def_save
  -> CmdSaveDef::exec()
  -> DataManager::saveDef()
  -> IdbBuilder::saveDef()
  -> DefWrite::writeChip()
  -> DEF sections
```

重点看 `DefWrite::writeChip()` 的 section 顺序，再逐个看 `write_row()`、`write_component()`、`write_net()` 这类函数。

### 9.3 追 `run_placer`

```text
Tcl: run_placer
  -> placer command::exec()
  -> ToolManager::autoRunPlacer()
  -> iPL API / wrapper
  -> placement db/operator/solver
  -> update IdbInstance location/status
```

先从命令和 `ToolManager` 看入口，再去 `src/operation/iPL` 读数据准备、求解器和回写。

### 9.4 追 `run_rt`

```text
Tcl: init_rt/run_rt
  -> RT command::exec()
  -> ToolManager::autoRunRouter()
  -> iRT API
  -> routing database / engine
  -> update net wire / via / segment
```

router 需要特别关注 layer、track、gcell、net、pin、via 的数据来源。

### 9.5 追 `edadb_write/read`

```text
edadb_write：iDB -> DefWriteEdadb -> EDADB tables
edadb_read：DEF 初始化上下文 + EDADB tables -> DefReadEdadb -> iDB
```

阅读顺序：

1. `src/interface/tcl/tcl_idb/tcl_db_file.cpp`
2. `src/platform/data_manager/idm_edadb.cpp`
3. `src/database/manager/builder/builder.cpp`
4. `src/database/manager/builder/def_builder/def_write_edadb.cpp`
5. `src/database/manager/builder/def_builder/def_read_edadb.cpp`
6. `src/database/edadb/idb/edadb_idb_schema.h`
7. `src/database/edadb/idb/edadb_idb_init.cpp`
8. `src/database/edadb/idb/shadow/*`

## 10. EDADB 适配原则

当前 C 分支的 EDADB 适配不是重新设计 DEF 语义，而是把原始 DEF read/write 的对象流迁移到数据库：

1. 先读 `DefWrite`，确认原始 DEF 会输出哪个类、哪个字段、什么顺序。
2. 再读 `DefRead`，确认原始 parser 如何重建对象，哪些字段来自文件，哪些字段由 LEF/iDB 计算得到。
3. 在 `edadb_idb_schema.h` 定义必要的表字段；字段应对应 DEF 语义，不应盲目持久化所有 C++ 成员。
4. 只有当原类无法表达数据库主键、归属关系或需要更稳定的存储视图时才定义 shadow。
5. 新版 EDADB 能隐式处理的嵌套成员，不额外定义 shadow。
6. `DefWriteEdadb::writeIdbXXX()` 要模仿 `DefWrite::write_xxx()`。
7. `DefReadEdadb::readIdbXXX()` 要模仿 `DefRead::parse_xxx()`。
8. 关闭对应 DEF callback，证明该类对象确实来自 EDADB。
9. 每切一个类，都跑 demo 和回归脚本，确认输入/输出 DEF 和数据库内容。

这也是后续 review `IdbInstance`、`IdbPin`、`IdbBlockage`、`IdbRegion`、`IdbSlot`、`IdbGroup`、`IdbFill`、`SpecialNet`、`Net` 的统一方法。

## 11. 推荐阅读路线

### 初次理解 iEDA

1. `README-CN.md`
2. `docs/user_guide/iEDA_user_guide-cn.md`
3. 本文档第 1-8 节
4. `md/ieda_architecture_learning/01_entry_runtime.md`
5. `md/ieda_architecture_learning/02_data_model_idb.md`
6. `md/ieda_architecture_learning/04_eda_tools.md`

### 代码 review 路线

1. `src/apps/ieda_main.cpp`
2. `src/platform/flow/flow.cpp`
3. `src/interface/tcl/tcl_register.h`
4. `src/interface/tcl/tcl_idb/tcl_db_file.cpp`
5. `src/platform/data_manager/idm*.cpp`
6. `src/database/manager/builder/builder.cpp`
7. `src/database/manager/builder/def_builder/def_read.cpp`
8. `src/database/manager/builder/def_builder/def_write.cpp`
9. 再进入对应 `src/operation/<tool>`。

### EDADB 路线

1. `edadb_readme.md`
2. `md/ieda_architecture_learning/09_edadb_idb_vs_master.md`
3. 本文档第 5.3 节和第 10 节
4. `src/database/manager/builder/def_builder/def_write_edadb.cpp`
5. `src/database/manager/builder/def_builder/def_read_edadb.cpp`
6. `src/database/edadb/idb/edadb_idb_schema.h`
7. `src/database/edadb/test/README.md`

## 12. 常用定位命令

```bash
rg "registerCmd|registerTclCmd" src/interface/tcl
rg "class Cmd.*|::exec" src/interface/tcl
rg "autoRun|idbStart|saveDef|readDef" src/platform
rg "writeChip|write_[a-z_]+\\(" src/database/manager/builder/def_builder
rg "parse_|defrSet.*Cbk" src/database/manager/builder/def_builder
rg "writeIdb|readIdb|writeSpecialNet|readSpecialNet" src/database/manager/builder/def_builder
rg "TABLE4|EDADB_INIT_TABLE|SHADOW" src/database/edadb/idb
```

## 13. 当前分支注意事项

- `edadb-idb` 不是纯 master；它基于 A/C 布局继续开发，EDADB adapter 放在 `src/database/edadb/idb`。
- `src/database/edadb/core` 跟踪 EDADB 新 API；iEDA 侧应通过 `idb::edadb_adapter` 适配，避免 builder 层直接扩散 EDADB 初始化细节。
- 当前 EDADB 回归重点是 DEF roundtrip 和数据库内容检查；直接 native `DEF -> DEF` 与 `origin/master` 的字节级结果可能已有分支内有意差异。
- 做新类迁移时，先写最小闭环：schema/init -> write -> read -> 关闭 DEF callback -> demo -> 回归 -> 文档 -> commit。
