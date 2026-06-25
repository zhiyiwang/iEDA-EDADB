# EDADB + iEDA 工程与研究思路台账

本文记录“为什么要做、参考了什么、现有代码能支撑什么、哪些问题可能有 CCF A 级别研究价值”。所有结论都分成外部资料、代码证据、痛点、创新问题和验证计划，避免凭空编故事。

## 0. 本次调研方法

### 0.1 外部资料查找

检索主题：

- EDA database / design database / physical design common database。
- OpenROAD / OpenDB / common in-memory database / binary save。
- ML for EDA datasets / stage-aware physical design schema。
- database incremental view maintenance / provenance sketch / graph query / indexing。
- DAC / ICCAD / SIGMOD / VLDB / ICDE 等严肃会议方向。

本次只把以下会议作为主要严肃目标讨论：

- EDA 方向：`DAC`、`ICCAD`。
- 数据库方向：`SIGMOD`、`VLDB`、`ICDE`。
- 说明：`ASP-DAC`、`DATE`、`ISPD` 是 EDA 社区重要会议，但本文不把它们当作“CCF A 主目标”来包装。最终投稿等级需要按你所在单位使用的最新版 CCF 目录再次核对。

### 0.2 本地代码阅读范围

已阅读或核对的本地文件：

- iEDA 总入口和调度：
  - `src/apps/ieda_main.cpp`
  - `src/interface/tcl/tcl_register.h`
  - `src/platform/tool_manager/tool_manager.cpp`
  - `src/platform/data_manager/idm*.cpp`
- DEF / EDADB adapter：
  - `src/database/manager/builder/def_builder/def_read.cpp`
  - `src/database/manager/builder/def_builder/def_write.cpp`
  - `src/database/manager/builder/def_builder/def_read_edadb.cpp`
  - `src/database/manager/builder/def_builder/def_write_edadb.cpp`
  - `src/database/edadb/idb/edadb_idb_schema.h`
  - `src/database/edadb/idb/edadb_idb_init.cpp`
  - `src/database/edadb/idb/shadow/*`
- EDADB core：
  - `src/database/edadb/core/AGENTS.md`
  - `src/database/edadb/core/include/edadb.h`
  - `src/database/edadb/core/include/edadb/DbTableDefTraverser.h`
  - `src/database/edadb/core/include/edadb/DbObjectTraverser.h`
  - `src/database/edadb/core/include/edadb/DbTableOperator.h`
  - `src/database/edadb/core/md/edadb_orm_test_matrix.md`
  - 目标覆盖审计记录在 `docs/paper/goal_coverage_audit.md`
  - 外部资料和会议目标记录在 `docs/paper/literature_and_venue_notes.md`
  - 执行计划和实验矩阵记录在 `docs/paper/research_execution_plan.md`
  - 代码审计和系统路线记录在 `docs/paper/edadb_core_research_notes.md`
  - 工具侧机会矩阵记录在 `docs/paper/ieda_tool_edadb_opportunity_matrix.md`
- 点工具入口和 wrapper：
  - `src/operation/iPL/README.md`
  - `src/operation/iPL/source/module/wrapper/IDBWrapper.hh`
  - `src/operation/iPL/source/module/wrapper/IDBWrapper.cc`
  - `src/operation/iRT/README.md`
  - `src/operation/iRT/interface/RTInterface.cpp`
  - `src/operation/iDRC/README.md`
  - `src/operation/iDRC/interface/DRCInterface.cpp`
  - `src/operation/iCTS/README.md`
  - `src/operation/iTO/README.md`
  - `src/operation/iPNP/README.md`

## 1. 外部资料和可引用结论

### 1.1 核验状态说明

下表按“能支撑什么结论”来记录资料，不把资料包装成已经证明我们的方案可行。

- `已核验`：已打开原始或相对权威页面，资料内容可直接支撑表中结论。
- `待人工核验`：我能找到 arXiv/网页条目，但最终发表 venue、CCF 分类或版本仍需你人工确认。
- `只作背景`：只能说明领域背景，不能作为论文核心 novelty 依据。

| 资料 | 链接 | 可引用结论 | 对本项目的启发 |
| --- | --- | --- | --- |
| OpenDB 文档 | `https://openroad.readthedocs.io/en/latest/main/src/odb/README.html` | 已核验：OpenDB 是 OpenROAD 的 physical design database，基于 LEF/DEF 5.6，支持 binary save/load，文档还说明类要足够快，避免应用复制到专用结构。 | EDADB 不能只做 DEF roundtrip；要向“可查询、可增量、可分析”的 EDA database 发展。 |
| OpenROAD 文档 | `https://openroad.readthedocs.io/en/latest/main/README.html` | 已核验：OpenROAD 文档列出完整后端阶段，包括 floorplan、placement、CTS、global/detailed routing、parasitics、PDN/IR 等。 | iEDA + EDADB 的研究应覆盖跨阶段数据，而不是孤立 DEF 文件读写。 |
| DAC 官网 | `https://www.dac.com/` | 已核验：DAC 2026 页面明确有 Research Track、Engineering Track，并覆盖 EDA、AI & Design 等方向。 | 如果做工程系统，可以考虑 Research Track 或 Engineering Track，但论文贡献仍需足够强。 |
| ICCAD 官网 | `https://www.iccad.com/` | 待人工核验：ICCAD 是 EDA 领域主会议；本次未完整抓取 CCF 分类页面。 | 本文暂把 ICCAD 作为 EDA 顶会目标，但最终 CCF A 分类需人工核验最新版目录。 |
| ML for EDA Survey | `https://arxiv.org/abs/2102.03357` | 已核验 arXiv 条目：综述 ML for EDA，强调 VLSI 复杂度上升和 ML 方法进入 EDA 多层级。 | 支撑“EDA 数据和跨阶段特征管理有研究需求”这一背景，不直接证明 EDADB novelty。 |
| CircuitNet | `https://arxiv.org/abs/2208.01040` | 已核验 arXiv 条目：提出开源 ML-for-EDA 数据集，动机是大规模公开数据不足。 | EDADB 可以做数据采集底座，但必须避免只做“又一个数据集”。 |
| EDA-Schema-V2 | `https://arxiv.org/abs/2605.06952` | 待人工核验：arXiv 2026 条目，声称提出 physical design 多模态 schema、跨 synthesis/floorplan/placement/CTS/routing，多 PDK、多参数 sweep。 | 说明“stage-aware schema + benchmark”已有人做；我们的创新必须转向 database-native、incremental、provenance。 |
| R2G | `https://arxiv.org/abs/2604.08810` | 待人工核验：arXiv 2026 条目，强调 RTL-to-GDS 多视图 graph benchmark 和表示选择影响模型效果。 | 支撑“多视图导出”方向，但不能作为已发表定论；后续需核验代码和数据。 |
| iEDA paper | `https://arxiv.org/abs/2308.01857` | 已核验 arXiv 条目：iEDA 覆盖 physical design flow 和部分分析工具，包括 placement、CTS、routing、timing optimization、STA、power analysis。 | 说明本项目有真实开源 EDA flow，可做跨工具数据库实验，而不是 toy benchmark。 |
| PowerNet | `https://arxiv.org/abs/2011.13494` | 已核验 arXiv 条目：面向 dynamic IR drop estimation，目标是加速昂贵的 IR 分析。 | 支撑 PowerIRView / IR prediction 的应用价值，但我们的重点应放在数据库化数据来源、provenance 和闭环验证。 |
| PDNNet | `https://arxiv.org/abs/2403.18569` | 已核验 arXiv 条目：使用 PDNGraph 表示 PDN structure 和 cell-PDN relation 做 dynamic IR drop prediction。 | 说明 IR/PDN 任务需要 graph/spatial/power 多视图，适合 EDADB 提供统一底座。 |
| DALI-PD | `https://arxiv.org/abs/2507.10606` | 待人工核验：arXiv 条目声称用 diffusion 生成 layout heatmap，覆盖 power、IR drop、congestion、macro placement、cell density。 | 说明 synthetic physical-design data 是热点；EDADB 可提供真实 flow 的可校验数据与 synthetic 数据互补。 |
| DBSP | `https://arxiv.org/abs/2203.16684` | 已核验 arXiv 条目：提出 rich query language 的 automatic incremental view maintenance。 | 可借鉴其思想，把 EDA metrics/query 变成可增量维护的 views。 |
| Provenance-based Data Skipping | `https://arxiv.org/abs/2104.12815` | 已核验 arXiv 条目：用 provenance sketches 记录 query 相关数据，后续 query 跳过无关数据。 | 可转化为 EDA ECO 下的 tile/net/layer skipping。 |
| In-memory Incremental Maintenance of Provenance Sketches | `https://arxiv.org/abs/2505.20683` | 已核验 arXiv 条目：provenance sketch 会随数据更新变 stale，需要增量维护。 | 支撑“EDADB 要有 update log + sketch maintenance”的数据库研究问题。 |

### 1.2 由资料推导研究问题的过程

1. OpenDB 已经证明“shared physical design database + binary save/load”是成熟开源 EDA 基础设施方向，所以 EDADB 若只复刻 OpenDB 或只保存 DEF，创新性不足。
2. ML-for-EDA、CircuitNet、EDA-Schema-V2、R2G 都指向“数据、schema、多阶段、多视图”的需求，所以 EDADB 可以做数据底座，但不能只发数据集。
3. DBSP 和 provenance skipping 指向数据库系统侧的成熟问题：incremental view maintenance、data skipping、sketch maintenance。EDA 的 ECO、局部移动、局部 reroute、局部 DRC 天然适合这些技术。
4. 因此最有希望的研究交叉点不是“EDA 使用数据库”，而是：
   - EDA-specific object/spatial/graph workload；
   - incremental/provenance-aware database techniques；
   - 在真实开源后端 flow 中证明工具 runtime 或迭代效率提升。

### 1.3 不能夸大的地方

- 不能声称 EDADB 当前已经是高性能 EDA database；当前它更接近 SQLite ORM 原型。
- 不能声称上述 arXiv 2026 工作都已顶会发表；它们目前只能作为待核验前沿线索。
- 不能把 CCF A 作为已经确认结论；本文只把 DAC/ICCAD/SIGMOD/VLDB/ICDE 作为严肃目标，最终分类要人工核对最新版 CCF 目录。
- 不能把 OpenDB 文档当作数据库系统论文引用；它可作为工程 baseline/related system，不是理论依据。

## 2. 当前代码给出的事实基础

### 2.1 iEDA 当前是“内存 iDB + 文本/脚本驱动”的 flow

代码证据：

- `src/apps/ieda_main.cpp` 进入 Tcl。
- `src/interface/tcl/tcl_register.h` 注册 DB、FP、PL、CTS、NO、TO、RT、DRC、STA、Power、PNP 等命令。
- `src/platform/tool_manager/tool_manager.cpp` 把命令转发到各点工具或 `DataManager`。
- `src/platform/data_manager/idm*.cpp` 负责 LEF/DEF/Verilog/GDS/EDADB 读写。

结论：

- 当前 EDADB 已经插入 iEDA 的 DB read/write 链路，但主要还在 DEF 对象持久化层。
- 如果要提高 EDA 性能，下一步不能只优化 `def_read/write`；要进入各点工具的“数据准备、查询、增量更新、结果回写”路径。

### 2.2 EDADB 当前能力是对象图 ORM，不是完整 EDA DBMS

代码证据：

- `src/database/edadb/core/include/edadb.h` 提供 `createTable<T>()`、`insertObject()`、`readAll()`、`readVectorByPredicate()`、`updateObject()`、`upsertObject()` 等 facade。
- `src/database/edadb/core/include/edadb/DbTableDefTraverser.h` 编译期遍历类型，展开 inline object 和 vector child table。
- `src/database/edadb/core/include/edadb/DbObjectTraverser.h` 运行期两阶段遍历对象：先 scalar，再 vector child。
- `src/database/edadb/core/AGENTS.md` 明确当前后端是 SQLite，非线程安全，UPDATE/UPSERT 是整图替换。
- EDADB core 代码审计和系统研究路线见 `docs/paper/edadb_core_research_notes.md`。

结论：

- EDADB 目前适合验证复杂 C++ 对象图映射。
- 还缺少 EDA 高性能数据库所需的多版本、空间索引、图索引、列式/批量 scan、增量视图、并发和查询优化。

### 2.3 iEDA 点工具已经存在大量可数据库化的数据访问模式

代码证据：

- iPL 的 `IDBWrapper` 从 iDB 包装 placement 专用 DB，支持 `updateFromSourceDataBase()`、指定 instance list 的增量更新、`writeBackSourceDatabase()`。
- iRT `RTInterface::runRT()` 内部有 PinAccessor、SupplyAnalyzer、TopologyGenerator、LayerAssigner、SpaceRouter、TrackAssigner、DetailedRouter、ViolationReporter 等阶段。
- iDRC 从 iDB 包装 die、rules、layers、shape，然后做 violation 检查。
- iSTA/iCTS/iTO 通过 TimingEngine / TimingIDBAdapter 从 iDB 构建时序图并反复 update timing。

结论：

- EDADB 可以切入“工具内部缓存和查询”，而不是只存 DEF。
- 最自然的切入点是 iPL、iRT、iDRC、iSTA/iTO，因为这些工具反复查询 instance/net/pin/geometry/timing/congestion。

### 2.4 逐工具证据台账

| 工具/模块 | 本地证据 | 说明 | EDADB 可切入问题 |
| --- | --- | --- | --- |
| EDADB read/write | `src/platform/data_manager/idm_edadb.cpp:13`、`src/platform/data_manager/idm_edadb.cpp:33` | `DataManager` 已有 `readDefFromEdadb()` / `saveDefToEdadb()`。 | 当前只证明 DEF 对象持久化入口，不代表工具内部增量能力。 |
| EDADB table init | `src/database/edadb/idb/edadb_idb_init.cpp:68` | `initAllTables()` 注册 Design/Die/Row/Track/Via/Instance/Pin/Net 等对象族。 | 需要从 DEF snapshot 扩展到 stage/version/view tables。 |
| EDADB write path | `src/database/manager/builder/def_builder/def_write_edadb.cpp:67` | `writeChip2Edadb()` 按对象族写入 EDADB。 | 可复用对象族作为基础事实表。 |
| EDADB read path | `src/database/manager/builder/def_builder/def_read_edadb.cpp:267` | `createDbByEdadb()` 按对象族从 EDADB 恢复 iDB。 | 后续应增加 view/read API，而不是每次只恢复完整 DEF。 |
| iPL placement wrapper | `src/operation/iPL/source/module/wrapper/IDBWrapper.cc:48`、`:77`、`:121` | iPL 能全量或按 instance list 从 iDB 更新 placement DB。 | 适合做 changed-inst delta、HPWL/bin-density incremental view。 |
| iPL Tcl/API | `src/platform/tool_manager/tool_api/ipl_io/ipl_io.cpp:52`、`:78`、`:101` | iPL 已有 placement、incremental legalization、changed inst list 接口。 | 可把 EDADB dirty set 直接喂给 incremental legalization。 |
| iRT router pipeline | `src/operation/iRT/interface/RTInterface.cpp:95`、`:117` | iRT 分 EGR 和完整 RT，完整流程含 pin access、topology、layer/track/detail route、violation report。 | 适合保存 access point、routing guide、tile occupancy、wire/via delta。 |
| iRT DEF cleanup | `src/operation/iRT/interface/RTInterface.cpp:190` | `clearDef()` 会清理 net wire/virtual 等 route 结果。 | EDADB 可记录 route result version，避免全量清理/重建。 |
| iDRC check | `src/operation/iDRC/interface/DRCInterface.cpp:83`、`:186` | iDRC 包装 database 后构造 shape/rule，再跑 `checkDef()`。 | 最适合做 spatial index、candidate skipping、violation provenance。 |
| iCTS | `src/operation/iCTS/api/CTSAPI.cc:74` | CTS 有独立 `runCTS()`，依赖 timing engine 和 clock topology。 | 可缓存 sink clustering、clock tree topology、timing model query。 |
| iTO | `src/operation/iTO/api/ToApi.cpp:61` | TO 运行 timing optimization，涉及 buffer/resize/timing update。 | 可记录 ECO delta、affected cone、critical path snapshot。 |
| iSTA | `src/platform/tool_manager/tool_api/ista_io/ista_io.cpp:271` | `reportTiming()` 会 buildGraph/updateTiming/report。 | 可做 timing graph mapping 和增量 timing affected-set 数据底座。 |
| iFP | `src/operation/iFP/api/ifp_api.cpp` | `initDie/initCore/makeTracks/placePort/tapCells` 修改 floorplan、track、IO、tapcell/endcap。 | 可做 FloorplanView 和 stage action provenance，支撑 flow diff。 |
| iPDN | `src/interface/tcl/tcl_ipdn/tcl_register_pdn.h`、`src/operation/iPDN/source/module/pdn_plan/pdn_plan.cpp` | Tcl 命令创建 grid/stripe/via/special wire，并写入 `IdbSpecialNet`。 | 可做 PDNView：command 参数、special wire/via、region/template provenance。 |
| iNO | `src/operation/iNO/source/module/fix_fanout/FixFanout.cpp` | 扫 STA fanout，插 buffer、建新 net、重连 load pins。 | 可做 NetlistECOView，直接产出 dirty inst/net/pin 给 PL/RT/STA。 |
| Flow/DSE | `src/interface/tcl/tcl_register.h`、`src/platform/tool_manager/tool_manager.cpp` | Tcl 注册并调度各点工具，tool_io 记录 runtime/memory。 | 可做 FlowMemory：run/stage/config/QoR/failure/provenance。 |

更多工具侧机会矩阵见 `docs/paper/ieda_tool_edadb_opportunity_matrix.md`。

## 3. 面向 CCF A 目标的研究方向候选

### 方向 A：EDA-native Incremental Physical Design Database

目标会议：

- 主目标：`DAC` / `ICCAD`。
- 若强调数据库系统贡献：`SIGMOD` / `VLDB` / `ICDE`，但需要更强 DB novelty。

痛点：

- 物理设计 flow 阶段多，每阶段读写 DEF/报告，文本 I/O 和重复建模成本高。
- iPL、iRT、iDRC、iSTA 都把 iDB 再包装成自己的内部 DB，跨工具共享差。
- ECO 场景中设计只变一小部分，但很多工具会重建大块数据。

创新问题：

- 能否把 iDB object graph 转换为一种 EDA-native relational/graph hybrid database，使 placement/routing/DRC/timing 的常用视图可增量维护？
- 能否用 DBSP 类似思想，为 HPWL、bin density、RUDY congestion、DRC candidate pairs、timing affected cone 建 incremental views？
- 能否把 ECO 修改记录成 delta log，只重新计算受影响的 nets/instances/tiles/layers？

可落地工程：

- 在 EDADB 增加 change log：instance move、net rewire、pin shape change、wire segment add/delete。
- 为 iPL 建 `PlacementView`：inst 坐标、cell size、net-pin relation、bin density、HPWL。
- 为 iRT/iDRC 建 `GeometryView`：layer rect/via/segment spatial partitions。
- 每个 view 提供 full build 与 delta update 两套 API。

实验验证：

- baseline：当前 iEDA full rebuild wrapper / direct iDB scan。
- EDADB：persistent + incremental view update。
- 指标：runtime、memory、dirty object 数、view rebuild ratio、PPA/DRC/timing 结果一致性。
- 数据：sky130_gcd 起步，再扩到 OpenROAD/OpenLane 公开 benchmark 或自建多参数 sweep。

风险：

- 如果只做“SQLite 存对象 + SELECT 查询”，顶会创新性不够。
- 必须证明增量维护对真实 EDA workload 有显著收益。

### 方向 B：Provenance-guided EDA Query and Data Skipping

目标会议：

- 主目标：`SIGMOD` / `VLDB` / `ICDE`。
- EDA 应用主目标：`DAC` / `ICCAD`。

痛点：

- iDRC、iRT、iPL congestion/timing evaluation 会反复扫大量 geometry/net/pin 数据。
- ECO 后只有少量区域或 nets 变化，但传统工具难准确跳过无关数据。

创新问题：

- 能否为每次 DRC/congestion/timing query 记录 provenance：哪些 rows、nets、instances、tiles 真正影响结果？
- 能否把 provenance sketch 与 EDA 空间/图结构结合，做 tile-level / net-level / layer-level skipping？
- 能否在设计更新后增量维护 sketches，避免 stale sketch 导致错误跳过？

可落地工程：

- EDADB 增加 `QueryProvenance` 表：query_id、object_family、object_pk、tile_id、layer、version。
- 增加 tile/layer/net 分区表，记录 object 到 partition 的映射。
- DRC/congestion/timing API 查询时先用 sketch 缩小候选集合。

实验验证：

- workloads：instance move、buffer insertion、routing segment update、PDN change。
- queries：DRC spacing/short candidate、RUDY congestion、HPWL、net bounding box、affected timing cone。
- 指标：跳过比例、false positive rate、结果正确性、sketch 更新成本。

风险：

- 数据库顶会需要通用性：不能只写 iEDA 特例。需要抽象成“domain-aware provenance skipping for spatial-graph workloads”。

### 方向 C：Stage-aware Physical Design Data Lake for ML and LLM Agents

目标会议：

- EDA/AI for EDA：`DAC` / `ICCAD`。
- 如果只是数据集，顶会风险较高；必须有 schema 和 task novelty。

痛点：

- ML for EDA 需要大量可复现、多阶段数据。
- 现有数据集多集中在图、图像或固定特征，跨阶段 provenance、参数、工具内部中间状态不足。
- iEDA 有完整 flow，EDADB 可以保存每阶段 object graph 和 tool metrics。

创新问题：

- 能否定义 iEDA/EDADB 的 stage-aware schema，统一保存 floorplan、placement、CTS、routing、DRC、timing、power、工具参数和中间 view？
- 能否支持从同一数据库导出多视图：netlist graph、placement grid image、routing graph、timing graph、query tables？
- 能否设计任务：跨阶段 QoR 预测、增量 ECO 影响预测、congestion/DRC hotspot prediction、工具参数推荐？

可落地工程：

- EDADB 加 `run_id`、`stage_id`、`tool_config_hash`、`design_version`。
- 每个工具保存 summary 和中间特征：
  - iPL：HPWL、density、congestion、timing estimate。
  - iCTS：clock tree topology、skew、buffer count。
  - iRT：guide、wire segment、via、violation。
  - iTO/iSTA：WNS/TNS、critical paths、changed instances。
- 导出 PyTorch Geometric / DGL / parquet / SQL views。

实验验证：

- 先做 sky130_gcd 多参数 sweep。
- 再接入公开 OpenROAD/OpenLane benchmark 做跨设计泛化。
- baseline：CircuitNet / EDA-Schema-V2 / R2G 中同类任务。

风险：

- “又一个数据集”不够。必须强调数据库原生采集、多视图一致性、跨阶段 delta/provenance。

### 方向 D：EDA Object-Relational Mapping with Automatic Schema Synthesis

目标会议：

- 工程系统味更重，可尝试 `DAC` engineering / `ICCAD`，数据库顶会难度较高。

痛点：

- iDB C++ 对象复杂：裸指针、vector、嵌套对象、工具内部派生字段、对象所有权不清。
- 手写 `TABLE4CLASS` 和 shadow 容易错，字段和 DEF read/write 语义容易不一致。

创新问题：

- 能否根据 C++ class + DEF read/write 行为自动推荐持久化字段？
- 能否自动判断 direct mapping、shadow mapping、derived field、lookup field？
- 能否生成 schema、adapter、roundtrip test、SQLite validation SQL？

可落地工程：

- 读取 `DefWrite::write_xxx()` 和 `DefRead::parse_xxx()` 的字段访问，生成候选 schema。
- 对比 EDADB schema，发现多余字段、缺失字段和 derived field。
- 自动生成 class-level regression：DEF -> EDADB -> DEF + table count + edge cases。

实验验证：

- 对当前已迁移的 Design/Die/Row/.../Net 做回放，比较自动推荐与人工实现差异。
- 指标：人工代码减少量、schema bug 检出率、roundtrip 覆盖率。

风险：

- 如果没有强算法，只是工程脚本，顶会贡献不足。
- 可以作为支撑方向 A/B/C 的工程基础。

### 方向 E：Hybrid EDA Storage Engine: Row/Column/Spatial/Graph Views

目标会议：

- 数据库系统：`SIGMOD` / `VLDB` / `ICDE`。
- EDA 应用：`DAC` / `ICCAD`。

痛点：

- EDA 同时有 object traversal、大规模 scan、空间范围查询、netlist graph traversal、增量更新。
- 单一 SQLite row-store 很难同时满足这些 workload。

创新问题：

- 能否在 EDADB 中把同一 EDA 对象同时维护为：
  - row-oriented object tables；
  - columnar metric/features tables；
  - spatial index for geometry；
  - graph index for netlist/timing/routing connectivity？
- 能否根据工具 query pattern 自动选择 physical layout？

可落地工程：

- 保留 SQLite 作为 correctness backend。
- 增加内存列式 cache 或 DuckDB/Arrow/Parquet 导出。
- 增加 R-tree 或 tile index 管理 rectangles/segments。
- 增加 adjacency table for net-pin/inst-pin/timing arc。

实验验证：

- 查询集合：bbox overlap、net fanout、pin access candidates、HPWL、bin density、routing layer occupancy、critical cone。
- baseline：直接 iDB 指针遍历、SQLite naive query。
- EDADB hybrid：indexed query / column scan / graph traversal。

风险：

- 数据库顶会需要系统设计完整性和大量 benchmark。
- EDA 顶会需要证明对实际工具 runtime/PPA 有帮助。

### 方向优先级矩阵

| 方向 | 顶会潜力 | 当前代码支撑 | 工程成本 | 最大风险 | 推荐程度 |
| --- | --- | --- | --- | --- | --- |
| A. EDA-native incremental physical design DB | 高：EDA + DB 交叉，能投 DAC/ICCAD，做强系统也可冲数据库会 | 高：iEDA flow、EDADB schema、iPL/iRT/iDRC/iSTA 均有入口 | 高 | 只做成工程缓存，缺少可泛化方法 | 首选 |
| B. Provenance-guided skipping | 高：数据库 novelty 更清晰 | 中：需新增 provenance 和 query API | 中高 | correctness 证明和 false positive 控制 | 首选 |
| C. Stage-aware ML data lake | 中高：热点方向，但竞争强 | 高：iEDA 可跑完整 flow，EDADB 可采集 | 中 | 容易变成普通数据集 | 次选，配合 A/B |
| D. Automatic schema synthesis | 中：更偏工程工具 | 高：已有 DEF/EDADB adapter 可对照 | 中 | 算法贡献不够 | 支撑项 |
| E. Hybrid storage engine | 高：若做完整可冲数据库会 | 中：EDADB 目前后端较弱 | 很高 | 系统工程量大，benchmark 要强 | 长线 |

我当前判断：

1. 最适合作为第一篇主线的是 `A + B`：用 provenance / incremental view 技术解决 EDA physical design 中的局部更新和重复查询问题。
2. `C` 适合做 artifact 或第二条线：当 A/B 的 database 能记录 stage/version/provenance 后，自然导出 ML 数据。
3. `D` 是降低适配成本的工程论文/工具章节，不建议单独作为 CCF A 主贡献。
4. `E` 是长期系统目标；现在直接做会太大，适合拆成空间索引和图索引两个可验证模块。

## 4. 除 DEF read/write 外，EDADB 可提高 iEDA 性能的位置

| 工具 | 当前数据模式 | EDADB 可切入点 | 预期收益 |
| --- | --- | --- | --- |
| iPL | `IDBWrapper` 从 iDB 包装 placement DB，支持全量和部分 instance 更新。 | 保存 placement view、net-pin adjacency、bin density、HPWL delta、changed inst list。 | 增量 legalization / congestion eval 更快，减少重复 wrap。 |
| iRT | 多阶段 router：pin access、topology、layer assign、space/track/detail route、violation report。 | 保存 routing guide、access point、wire segment、via、tile occupancy、violation provenance。 | ECO routing、局部 reroute、DRC candidate 缩小。 |
| iDRC | 从 iDB 包装 die/rule/layer/shape 后做 rule validation。 | 建 geometry spatial index、violation table、query provenance sketch。 | 增量 DRC，不必每次扫全设计。 |
| iSTA/iTO | TimingEngine 从 iDB 转 timing netlist，TO 反复 buffer/resize/updateTiming。 | 保存 timing graph mapping、affected cone、critical path snapshot、cell/net delta。 | ECO timing 更新和参数调优加速。 |
| iCTS | 依赖 timing、clock net、buffer insertion 和 skew report。 | 保存 clock tree topology、sink clustering、wirelength/timing model cache；细化计划见 `docs/paper/edadb_icts_clock_tree_plan.md`。 | 多 clock / 大 fanout clock tree 的重复计算减少。 |
| iPNP/iPA/iIR | 需要 PDN、power、IR、congestion 数据。 | 保存 PDN geometry、instance current、IR map、congestion map。 | 多模板/退火迭代时共享评估结果。 |
| Flow/DSE | 当前脚本产生大量中间 DEF/report。 | 保存 run/stage/config/result provenance。 | 自动调参、失败诊断、结果复现和 ML 数据生成。 |

## 5. EDADB 自身需要补强的能力

### 5.1 查询和索引能力

当前状态：

- 有 `readVectorByPredicate()` 和 `QUERY_GENERIC`，但主要是 root table 查询。
- 缺少统一的 domain index。

需要补强：

- root/child table predicate API。
- bbox / tile / layer spatial index。
- netlist graph adjacency index。
- common query templates：by net、by inst、by layer、by bbox、by tile、by changed object。

### 5.2 增量更新能力

当前状态：

- UPDATE/UPSERT 是 delete + insert 整图替换。

需要补强：

- object-level diff。
- child vector partial update。
- design version / stage version。
- transaction-level change log。
- view invalidation and refresh API。

### 5.3 性能后端能力

当前状态：

- SQLite 是唯一真实后端。
- EDADB 非线程安全。

需要补强：

- prepared op 批量插入和批量读取。
- PRAGMA/transaction/write-ahead-log 策略可配置。
- 多 backend 抽象：SQLite correctness、DuckDB/Arrow analytics、memory backend for tool runtime。
- 并发读、单写或 snapshot read。

### 5.4 Schema 和 shadow 规则自动化

当前状态：

- `edadb_idb_schema.h` 人工维护大量 `TABLE4CLASS` / `TABLE4SHADOW`。
- shadow 当前用于 synthetic PK、简化视图、layer/via/name lookup、ordered vectors 等。

需要补强：

- schema lint：第一列 PK 是否合理、shadow 是否必要、字段是否参与 DEF 输出。
- schema diff：对比 `DefWrite`/`DefRead` 的字段访问。
- 自动生成 SQLite validation SQL。

### 5.5 数据集和实验管理

当前状态：

- `src/database/edadb/test/run_idb_roundtrip_regression.sh` 已经能做 DEF/EDADB roundtrip 和 SQLite 内容断言。

需要补强：

- 多 design、多 PDK、多 stage 的自动采集。
- 保存 config、git commit、tool version、run logs、QoR metrics。
- 导出 ML 格式：graph、grid、table、sequence。

### 5.6 系统补强细化计划

EDADB 自身从 ORM 原型演进到 EDA DBMS 的路线见 `docs/paper/edadb_system_improvement_plan.md`。该文档把 query/index/delta/view/storage/schema 六条能力拆成实施阶段和实验矩阵。

## 6. 建议优先推进的三条路线

主线选择和三个月切片见 `docs/paper/paper_mainline_selection.md`。当前建议第一篇论文优先推进 `iNO -> iPL -> iDRC` 的 persistent incremental view 闭环。

### P0：工程底座，先让 EDADB 成为 iEDA stage database

目标：

- 不先冲论文，先做可复现实验底座。

任务：

1. 给 EDADB 表加 `run_id/stage_id/design_version`。
2. 对 iPL/iRT/iDRC/iSTA 建最小 view schema。
3. 每个 view 支持 full build + query + validation。
4. 扩展 regression：不仅比较 DEF，也比较 view query 结果。

验收：

- `run_iEDA.sh` 每阶段都能记录 DB snapshot。
- 能用 SQL 查询每阶段 instance/net/geometry/timing/congestion summary。

### P1：增量 DRC / routing / placement view

目标：

- 做出最容易证明性能收益的研究原型。

建议起点：

- iDRC geometry candidate skipping。
- iPL HPWL/bin density delta update。
- iRT routing tile occupancy delta update。

原因：

- 这些任务的局部性最强，结果正确性也最容易验证。
- iDRC 的细化计划见 `docs/paper/edadb_idrc_incremental_drc_plan.md`。
- iPL 的细化计划见 `docs/paper/edadb_ipl_incremental_placement_plan.md`。
- iRT 的细化计划见 `docs/paper/edadb_irt_routing_eco_plan.md`。
- iSTA/iTO 的细化计划见 `docs/paper/edadb_timing_eco_plan.md`。
- iCTS 的细化计划见 `docs/paper/edadb_icts_clock_tree_plan.md`。
- EDADB 系统能力补强计划见 `docs/paper/edadb_system_improvement_plan.md`。

### P2：stage-aware ML dataset 和 agent memory

目标：

- 把 EDADB 从“工具内部 DB”扩展为“可复现实验数据底座”。

任务：

- 多参数 sweep。
- 保存每阶段 object graph、tool metrics、config、provenance。
- 导出多视图数据。

论文角度：

- 不能只说 dataset；要强调 database-backed, provenance-preserving, stage-consistent, multi-view benchmark。

## 7. 当前不建议包装成论文主贡献的内容

- 单纯把 DEF 写入 SQLite。
- 单纯 ORM 宏封装。
- 单纯比较 EDADB roundtrip 比 DEF 快或慢。
- 没有真实工具接入的 schema 设计。
- 没有增量 workload 的 partial update。
- 没有公开 benchmark 或可复现实验的 ML 数据集。

这些可以作为工程章节或 artifact，但不足以支撑 CCF A 主贡献。

## 8. 下一步可执行计划

1. 选一个工具做 P1 原型：建议先选 iDRC 或 iPL。
2. 记录该工具当前 full build 的数据访问路径和耗时。
3. 定义 EDADB view schema 和 query workload。
4. 做 full build correctness。
5. 做 delta update correctness。
6. 对比 full rebuild vs incremental view。
7. 扩展到多个 design 和多个变更类型。

如果目标是最快产生论文雏形，我建议先做：

```text
Provenance-guided Incremental Spatial-Graph Database for Open-source Physical Design
```

最小闭环：

- iDRC：geometry spatial query + violation provenance。
- iPL：bin density / HPWL incremental view。
- iRT：routing segment/tile occupancy incremental view。

这样能同时连接 EDA 痛点和数据库技术，不会停留在 DEF 持久化层。

## 9. 研究点扩展清单

本节按三个方向列出候选研究点。每个点都要经过后续文献核验、代码可行性验证和实验设计收敛，不能直接当作已经成立的论文贡献。

### 9.1 EDA 领域研究点

#### EDA-1：增量 DRC / 局部 Signoff Verification

痛点：

- ECO、局部 reroute、buffer insertion 后，实际变化往往局限在少量区域和少量 nets，但 DRC 容易重新扫描大量 geometry。
- iDRC 当前从 iDB 包装 die/rule/layer/shape 后做 check，适合插入 geometry view 和 dirty region。

可能创新：

- 构建 layer/tile/net-aware spatial database，记录 geometry 到 tile/layer/net 的映射。
- 对每次修改维护 dirty tile 和 affected neighbor tile。
- 只在 affected region 重新生成 candidate pairs，并通过 provenance 确认哪些历史 violation 需要 invalidation。

本项目落点：

- `src/operation/iDRC/interface/DRCInterface.cpp:83`
- `src/operation/iDRC/interface/DRCInterface.cpp:186`
- EDADB 增加 `GeometryShape`、`TileIndex`、`ViolationProvenance`、`DirtyRegion` 表。

验证方式：

- 构造 instance move、wire segment add/delete、via change、blockage change。
- 比较 full DRC 和 incremental DRC 的 violation set 是否一致。
- 指标：candidate pair 数量、runtime、dirty tile 比例、false positive/false negative。

#### EDA-2：增量 Placement Evaluation and Legalization

痛点：

- iPL 已有 `runIncrementalLegalization()` 和 changed instance list，但 HPWL、bin density、congestion estimate 等指标仍可能重复构建或重复扫描。
- 大量 placement 优化循环中，只有少量 instances 改变位置。

可能创新：

- 用 EDADB 维护 placement view：inst bbox、net-pin adjacency、net bbox、bin occupancy、region/blockage constraint。
- 对 instance move 做 delta update：只更新相关 nets 的 HPWL、相关 bins 的 density、相关 rows 的 legality。
- 结合 provenance 记录一个 placement metric 依赖哪些 instances/nets/bins。

本项目落点：

- `src/operation/iPL/source/module/wrapper/IDBWrapper.cc:48`
- `src/operation/iPL/source/module/wrapper/IDBWrapper.cc:77`
- `src/platform/tool_manager/tool_api/ipl_io/ipl_io.cpp:78`
- `src/platform/tool_manager/tool_api/ipl_io/ipl_io.cpp:101`
- 细化实验计划见 `docs/paper/edadb_ipl_incremental_placement_plan.md`。

验证方式：

- 从 iPL result 随机移动一批 cells，规模从 0.1% 到 10%。
- 比较 full recompute 与 EDADB delta view 的 HPWL、density、合法性结果。
- 指标：runtime、memory、更新对象数量、QoR 一致性。

#### EDA-3：Routing ECO Database and Local Reroute Support

痛点：

- iRT 流程长，完整 `runRT()` 包含 pin access、topology、layer assign、space route、track assign、detail route、violation report。
- ECO 可能只影响少数 nets，但重跑完整 route 成本高。

可能创新：

- 用 EDADB 保存 routing guide、access point、wire segment、via、tile occupancy、DRC violation provenance。
- 修改 net 后只 invalid affected route regions，保留未受影响的 route cache。
- 将 routing result 版本化，支持 route diff 和 rollback。

本项目落点：

- `src/operation/iRT/interface/RTInterface.cpp:95`
- `src/operation/iRT/interface/RTInterface.cpp:117`
- `src/operation/iRT/interface/RTInterface.cpp:190`
- `src/operation/iRT/source/data_manager/DataManager.hpp:31`
- `src/operation/iRT/source/data_manager/advance/GCell.hpp:23`
- 当前 EDADB 已能保存 `iNetSD`、wire、segment、point、via 相关表，可作为 routed geometry 基础事实表。
- 细化实验计划见 `docs/paper/edadb_irt_routing_eco_plan.md`。

验证方式：

- 对 routed DEF 做局部 net 修改，比较 full reroute 与 local reroute。
- 指标：rerouted nets、changed segments、DRC violations、wirelength、runtime。

#### EDA-4：Cross-stage Consistency Debugging Database

痛点：

- iEDA flow 中每阶段输出 DEF/report，跨阶段问题定位困难：例如 placement 引入 congestion，routing 后触发 DRC，timing opt 改动又影响 placement。

可能创新：

- EDADB 保存每个 stage 的 design snapshot、tool config、QoR summary 和 object delta。
- 支持查询“哪个阶段第一次引入某个 violation / critical path / congestion hotspot”。
- 构建 stage-level provenance graph。

本项目落点：

- `scripts/design/sky130_gcd/run_iEDA.sh`
- `src/platform/data_manager/idm_edadb.cpp`
- `src/database/edadb/test/run_idb_roundtrip_regression.sh`
- iSTA/iTO timing ECO 细化计划见 `docs/paper/edadb_timing_eco_plan.md`。
- iCTS clock tree 细化计划见 `docs/paper/edadb_icts_clock_tree_plan.md`。

验证方式：

- 跑完整 sky130 flow，保存每阶段 DB。
- 对比 DEF/report 与 EDADB stage query。
- 指标：问题定位时间、可追溯对象数量、stage delta 大小。

#### EDA-5：PowerIRView for PDN Optimization and IR Hotspot Debugging

痛点：

- iPNP 的 PDN 模板优化需要反复评估 IR drop 和 congestion；iPA/iIR 的 power/IR 结果也容易在 ECO 后重复计算。
- IR hotspot、current source、PDN shape、region template 之间缺少可查询的 provenance。

可能创新：

- 建立 `PowerView + IRView + PDNView`：保存 instance power、IR node voltage、hotspot、PDN template、region score。
- 对局部 template change、via change、high-power instance move 做 affected region 增量更新。
- 用 full iPA/iIR/iPNP 结果做 oracle，验证 PowerIRView 的一致性。

本项目落点：

- `src/operation/iPNP/source/PNP.cpp`
- `src/operation/iPA/api/Power.cc`
- `src/operation/iIR/api/iIR.cc`
- 细化实验计划见 `docs/paper/edadb_power_ir_pdn_plan.md`。

验证方式：

- 对比 iPA power report、iIR IR drop report、iPNP final report 与 EDADB query。
- 指标：max/min/avg IR drop、hotspot top-k recall、overflow、runtime、dirty region ratio。

#### EDA-6：CTS / Timing / Routing 联动的 Clock ECO Database

痛点：

- CTS 后 buffer tree、clock wire、RC/timing estimate 和后续 routing/timing ECO 强相关。
- 只看最终 DEF 难以解释 skew/latency 变化来自哪个 clock subtree 或 routing change。

可能创新：

- 用 EDADB 记录 clock tree、RC estimate、timing metric、routing update 和 ECO action。
- 支持 subtree-level dirty propagation：clock buffer move/resize 后只重查 affected sinks 和相关 route。
- 把 CTS、route、timing 三个阶段的 object delta 串成可追踪 graph。

本项目落点：

- `docs/paper/edadb_icts_clock_tree_plan.md`
- `docs/paper/edadb_timing_eco_plan.md`
- `docs/paper/edadb_irt_routing_eco_plan.md`

验证方式：

- 构造 clock buffer insertion/move/resize，比较 full CTS/STA 与 subtree incremental result。
- 指标：skew/latency/slack 一致性、affected subtree size、runtime saving。

#### EDA-7：Flow-level Design Space Exploration Memory

痛点：

- iEDA flow 中 clock、utilization、aspect ratio、routing/placement 参数 sweep 成本高，历史结果复用弱。
- DSE 失败原因常散落在 log、DEF、report 里，难以形成可学习经验。

可能创新：

- EDADB 保存 flow config、stage snapshot、QoR summary、failure reason、object delta。
- 对相近 config 复用 stage view 或预测高风险参数组合。
- 支持 “why this config failed” 的 stage-level provenance query。

本项目落点：

- `scripts/design/sky130_gcd/run_iEDA.sh`
- `src/platform/tool_manager/tool_manager.cpp`
- EDADB run/stage/version schema。
- 细化计划：`docs/paper/edadb_flow_dse_memory_plan.md`

验证方式：

- 多参数 sweep，比较无记忆 DSE 与 EDADB-backed DSE。
- 指标：有效配置发现速度、失败定位时间、可复用 stage 比例。

### 9.2 Database 领域研究点

#### DB-1：EDA-specific Incremental View Maintenance

痛点：

- 通用 IVM 研究通常面向 relational/streaming workload；EDA workload 同时有 object graph、spatial geometry、netlist graph 和 iterative optimization。

可能创新：

- 定义 EDA view algebra：object table + vector child table + spatial tile + graph adjacency。
- 为 HPWL、density、geometry overlap、affected timing cone 等 EDA views 设计增量维护规则。
- 将 EDADB 的 object traversal 和 DBSP 类 IVM 思想结合。

本项目落点：

- EDADB 当前 `DbObjectTraverser` 已能遍历 object graph。
- 需要新增 view definition、delta propagation、view invalidation API。

验证方式：

- workload：instance move、net connection change、wire segment change。
- views：HPWL、bin density、bbox overlap、net fanout、tile occupancy。
- baseline：full recompute / SQLite naive query。

#### DB-2：Provenance-based Data Skipping for Spatial-Graph Workloads

痛点：

- EDA query 中很多数据只有局部相关性，但提前判断相关数据很难。
- Provenance sketch 可以记录历史 query 依赖，但 EDA 更新频繁，需要 sketch maintenance。

可能创新：

- 定义 EDA provenance unit：object pk、net id、layer id、tile id、stage id。
- 查询执行时生成 over-approx provenance sketch。
- 更新时增量维护 sketch，并用 sketch 跳过无关 tiles/nets。

本项目落点：

- EDADB 增加 `QueryProvenance`、`SketchUnit`、`DirtyObject` 表。
- iDRC/iPL/iRT query 都能生成自然的 provenance。

验证方式：

- 测试 top-k congestion、DRC candidate、affected net bbox、critical cone query。
- 指标：skip ratio、sketch size、maintenance cost、false positive rate。

#### DB-3：Hybrid Row/Column/Spatial/Graph Physical Layout for EDA

痛点：

- EDADB 当前 SQLite row-store 适合 correctness，但 EDA 同时需要：
  - object roundtrip；
  - 大规模 metric scan；
  - bbox overlap；
  - netlist/timing graph traversal。

可能创新：

- 同一逻辑对象维护多种物理视图：row tables、columnar feature tables、spatial index、adjacency index。
- 根据 query pattern 自动选择 row scan、column scan、spatial query 或 graph traversal。
- 定义 consistency protocol，保证 object update 后多视图一致。

本项目落点：

- EDADB 保留 SQLite ORM 表作为 source of truth。
- 增加 memory column cache、R-tree/tile index、adjacency table。

验证方式：

- 查询集合：by bbox、by net、by layer、by fanout、by changed inst、by critical cone。
- 指标：query latency、update latency、storage overhead、consistency check cost。

#### DB-4：Schema Synthesis and Static Verification for C++ Object ORM

痛点：

- EDADB schema 依赖 `TABLE4CLASS` 和 shadow；字段顺序、PK、vector、pointer、derived field 很容易写错。
- iEDA 类定义和 DEF read/write 语义不是一一持久化全部成员。

可能创新：

- 从 C++ AST + `DefWrite`/`DefRead` 字段访问中推导应持久化字段。
- 自动判断 direct mapping、shadow mapping、derived/recomputed field。
- 对 schema 做 static lint：PK 合法性、FK path、shadow 必要性、DEF roundtrip coverage。

本项目落点：

- `src/database/edadb/idb/edadb_idb_schema.h`
- `src/database/manager/builder/def_builder/def_write.cpp`
- `src/database/manager/builder/def_builder/def_read.cpp`

验证方式：

- 对已迁移对象族回放，比较人工 schema 和自动建议。
- 指标：bug 检出数、误报、漏报、人工代码减少量。

#### DB-5：Versioned EDA Object Store and Time-travel Query

痛点：

- EDA flow 是多阶段、多轮 ECO、多工具反复修改同一批 objects；只保存最新状态无法解释 QoR 变化来源。

可能创新：

- 为 EDADB 增加 stage/version/run_id 语义，支持 object-level delta、snapshot、time-travel query。
- 支持查询某个 net/inst/violation/critical path 在各阶段的演化。
- 将 DEF roundtrip 从单版本持久化升级为多版本 flow database。

验证方式：

- 跑 placement/CTS/routing/timing optimization 多阶段 flow。
- 指标：snapshot storage overhead、delta query latency、stage provenance correctness。

#### DB-6：Workload-aware EDA Index Advisor

痛点：

- EDA query 模式混合了 name lookup、bbox overlap、net adjacency、critical cone、tile scan；固定索引很难全局最优。

可能创新：

- 从 EDADB query log 中学习/推断该设计当前需要哪些 index：B-tree、tile index、R-tree、adjacency index、column cache。
- 根据 dirty ratio 和 query frequency 自动开启/关闭 view/index。
- 把 EDA 工具调用变成可观测 workload，而不是手工猜索引。

验证方式：

- workload：iPL/iDRC/iRT/iSTA/iPNP query trace。
- 指标：query latency、update overhead、index build cost、advisor regret。

#### DB-7：Incremental Matrix/Graph View for IR and Timing Analysis

痛点：

- IR 分析和 STA 都包含 graph/matrix 结构；局部 ECO 后全量重建 RC tree、conductance matrix 或 timing graph 成本高。

可能创新：

- 在 EDADB 中保存 RC graph、current vector、timing/power graph 的版本化 view。
- 对局部 power/RC/netlist 变化做 affected subgraph 或 matrix block 更新。
- 将 solver 输入输出纳入数据库 correctness validation。

验证方式：

- workload：instance power change、wire resistance change、buffer insertion、routing segment change。
- 指标：matrix/view update cost、solver result error、full rebuild speedup。

### 9.3 AI + EDA + Database 交叉研究点

#### AI-1：Database-backed Stage-aware ML Dataset for Physical Design

痛点：

- ML for EDA 缺少多阶段、可复现、带 provenance 的开放数据。
- 现有 dataset/schema 方向已经有人做，必须避免只做数据搬运。

可能创新：

- 用 EDADB 自动记录每个 stage 的 object graph、tool config、QoR、delta 和 provenance。
- 从同一数据库导出 grid、graph、table、sequence 多视图，保证跨视图一致。
- 支持跨阶段任务：placement -> routing congestion、pre-route -> post-route timing、ECO impact prediction。

本项目落点：

- `run_iEDA.sh` 完整 flow。
- EDADB stage/version schema。
- iPL/iRT/iDRC/iSTA summary 和 feature 输出。

验证方式：

- 多参数 sweep：clock、core utilization、aspect ratio、tool config。
- baseline：CircuitNet、EDA-Schema-V2、R2G 类任务。
- 指标：数据完整性、跨视图一致性、模型效果、可复现性。

#### AI-2：LLM/Agent-assisted EDA Debugging with Persistent Design Memory

痛点：

- EDA debug 需要跨脚本、日志、DEF、report、设计对象关联；LLM 如果只读文本日志，很难定位对象级原因。

可能创新：

- EDADB 作为 agent memory，保存 stage snapshots、queryable design objects、tool logs、violations、critical paths。
- LLM agent 不直接读大 DEF，而是调用数据库 query：查 changed nets、violation provenance、critical path object chain。
- 构建 EDA debug benchmark：给定失败 flow，要求定位引入问题的 stage/object/config。

本项目落点：

- EDADB stage database。
- `docs/paper` 中已有代码/flow 教程可作为 agent context。
- 后续可给 iEDA 增加 query API 或 Tcl command。

验证方式：

- 人工构造 DRC/timing/congestion 失败案例。
- 比较纯日志 LLM、纯 SQL query、EDADB+agent 的定位准确率和步骤数。

#### AI-3：Learning-guided Incremental EDA Query Planning

痛点：

- 对同一个 EDA query，是走 full scan、tile index、net adjacency，还是 provenance skipping，取决于 dirty set、设计规模、query type。
- 手工规则难以覆盖所有场景。

可能创新：

- 学习一个 query planner，根据 design stats、dirty set、query predicate、历史运行时间选择执行策略。
- 结合 EDADB query provenance 形成在线反馈。
- 目标不是替代 EDA 算法，而是选择最快的数据库访问路径。

本项目落点：

- EDADB query layer。
- iDRC/iPL/iRT 的候选 query workload。
- stage database 可收集训练数据。

验证方式：

- 多设计、多 dirty ratio、多 query 类型。
- baseline：fixed strategy、rule-based planner。
- 指标：latency、planner overhead、regret、robustness。

#### AI-4：EDA ECO Impact Prediction from EDADB Delta Graph

痛点：

- ECO 后是否需要重跑 placement、routing、DRC、STA，影响哪些 nets/instances/tiles，目前多靠经验或保守全量重跑。

可能创新：

- 用 EDADB delta graph 表示 object changes：changed inst、pin、net、wire、region、timing arc。
- 学习预测受影响区域和 QoR 风险：DRC hotspot、timing degradation、routing congestion。
- 与增量 view 结合：预测 dirty region，再由数据库保证 correctness。

本项目落点：

- EDADB change log。
- iPL/iRT/iDRC/iSTA view。
- stage snapshots 生成 supervised labels。

验证方式：

- 构造 ECO 操作集：move、insert buffer、resize、rewire、reroute。
- label 来自 full tool rerun。
- 指标：affected-set recall、precision、runtime saving、QoR consistency。

#### AI-5：Power/IR/PDN Prediction with Verifiable Database Provenance

痛点：

- IR/PDN 预测模型需要 power、PDN geometry、placement density、RC graph 等多源数据；如果数据来源不可追踪，模型结果难以被 EDA flow 信任。

可能创新：

- EDADB 统一提供 PowerView、IRView、PDNView、congestion view，并记录每个 feature 的来源。
- 训练 IR hotspot / PDN template score predictor，同时保留 full iIR/iPNP 验证闭环。
- 研究 “prediction + exact validation” 的混合流程：模型只负责缩小候选区域，数据库和工具保证 correctness。

验证方式：

- baseline：PowerNet、PDNNet、CircuitNet 类任务。
- 指标：hotspot recall、template ranking quality、full verification runtime saving。

#### AI-6：Synthetic + Real Physical-design Data Co-training

痛点：

- 真实 EDA flow 数据生成慢，synthetic heatmap 生成快但真实性和可验证性弱。

可能创新：

- 用 EDADB 保存真实 iEDA flow 的 stage-aligned data，作为 synthetic data 校准和验证目标。
- 研究 real/synthetic 混合训练：真实数据保证物理一致性，synthetic 数据扩大覆盖。
- 对每个 synthetic sample 生成可检查的 feature distribution 和 QoR proxy。

验证方式：

- 与只用真实数据、只用 synthetic 数据比较。
- 指标：跨设计泛化、跨 PDK 泛化、feature distribution gap、下游任务误差。

#### AI-7：Agentic EDA Optimization with Queryable Memory

痛点：

- LLM/agent 可以写脚本、调参数，但缺少可靠 design memory 时容易只读 log 猜原因。

可能创新：

- 把 EDADB 暴露为 agent query memory：stage snapshot、QoR、violation、critical path、dirty object、provenance。
- agent 每次改参数或脚本后，必须用数据库 query 解释结果变化。
- 构建可复现的 EDA agent benchmark：给定目标 QoR，要求最少迭代找到可行 flow。

验证方式：

- baseline：纯日志 agent、人工规则 DSE、随机/网格搜索。
- 指标：达到目标 QoR 的迭代次数、失败定位准确率、不可解释操作比例。
