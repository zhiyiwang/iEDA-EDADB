# iEDA-EDADB 架构学习地图

本文档组用于把 Understand Anything 生成的全量知识图谱压缩成可阅读的学习路线。Dashboard 适合查局部关系，但第一次熟悉项目时，建议先按这里的文档读主链路，再回到 dashboard 或 `/understand-chat` 查细节。

## 项目一句话

iEDA-EDADB 是一个从 Netlist 到 GDS 的开源数字 ASIC 后端 EDA 工具链。它以 Tcl/Python 脚本作为用户入口，以 iDB 作为内存设计数据库，通过平台层调度 floorplan、placement、CTS、routing、STA、timing optimization、DRC、power/IR、PNP 等点工具；本分支额外集成 EDADB，用 SQLite ORM 方式探索 iDB/DEF 数据持久化。

## 推荐阅读顺序

0. `10_complete_tutorial.md`：先读这份总教程，建立入口、架构、物理流程和 EDADB 适配的完整主线。
1. `01_entry_runtime.md`：理解二进制入口、Flow、Tcl shell 如何启动。
2. `02_data_model_idb.md`：理解 iDB 内存模型，尤其是 Layout 与 Design 的区别。
3. `03_interface_and_dispatch.md`：理解 Tcl/Python 命令如何注册并进入 ToolManager/DataManager。
4. `04_eda_tools.md`：把 EDA 后端阶段映射到 `src/operation` 下的点工具。
5. `05_scripts_flow.md`：从 `scripts/design/sky130_gcd` 的 Tcl flow 反推执行过程。
6. `06_edadb_integration.md`：理解 EDADB 与 iDB 的桥接方式和当前实现边界。
7. `09_edadb_idb_vs_master.md`：需要总结分支差异或做 code review 时再读。

## 全局执行主链路

```text
iEDA binary
  -> src/apps/ieda_main.cpp
  -> iplf::Flow::runTcl()
  -> tcl::tcl_start()
  -> UserShell::userMain()
  -> tcl::registerCommands()
  -> Tcl command classes
  -> ToolManager / DataManager
  -> iDB / EDA point tools
  -> DEF / Verilog / GDS / report / EDADB
```

## 核心类和执行过程

| 角色 | 代码位置 | 作用 |
| --- | --- | --- |
| `main` | `src/apps/ieda_main.cpp` | 解析 `-script`、`-log`、`-v`，然后进入 `plfInst->runTcl(argc, argv)`。 |
| `iplf::Flow` | `src/platform/flow/flow.h/.cpp` | 平台级 flow 入口，当前核心职责是把执行交给 Tcl shell。 |
| `tcl::tcl_start` | `src/interface/tcl/tcl_main.h` | 获取 `UserShell`，设置命令注册函数，启动 Tcl 主循环。 |
| `tcl::registerCommands` | `src/interface/tcl/tcl_register.h` | 注册 DB、FP、PL、CTS、RT、STA、TO、DRC、Power、PNP 等 Tcl 命令。 |
| `iplf::ToolManager` | `src/platform/tool_manager/tool_manager.h/.cpp` | 平台层工具调度门面，负责把命令转发给各点工具或 DataManager。 |
| `idm::DataManager` | `src/platform/data_manager/idm.h/.cpp` | 平台级数据门面，负责 iDB 初始化、LEF/DEF/Verilog/GDS/JSON/EDADB 读写。 |
| `idb::IdbBuilder` | `src/database/manager/builder/builder.h/.cpp` | iDB 构建器，真正调用 LEF/DEF/Verilog parser 与 writer。 |

## EDA 抽象到 iEDA 的对应关系

| EDA 抽象 | 在数字后端中的含义 | iEDA 中的主要承载 |
| --- | --- | --- |
| Technology / Process | 工艺层、金属层、via、site、cell master 等 | `IdbLayout`、`IdbLayers`、`IdbSites`、`IdbCellMasterList`、LEF parser |
| Design / Netlist | 当前设计的 instance、pin、net、blockage、special net 等 | `IdbDesign`、`IdbInstanceList`、`IdbPins`、`IdbNetList`、DEF/Verilog parser |
| Runtime Flow | 用户从脚本启动后端步骤 | `Flow`、`UserShell`、Tcl command classes |
| Tool Dispatch | 把命令转发到点工具 | `ToolManager`、各 `*IO`/API 单例 |
| Database Access | 统一读写设计数据 | `DataManager`、`IdbBuilder`、`IdbDefService`、`IdbLefService` |
| Physical Implementation | floorplan、placement、CTS、routing 等 | `src/operation/iFP`、`iPL`、`iCTS`、`iRT` |
| Signoff / Analysis | timing、DRC、power、IR、report | `iSTA`、`iDRC`、`iPA/iPW`、`iIR`、report/feature/eval |
| Persistence | 文件或数据库形式保存设计 | DEF/GDS/Verilog/JSON writer，EDADB SQLite ORM |

## 使用 dashboard 的轻量方法

Dashboard 不建议从全图开始看。更有效的使用方式是：

1. 先读本文档组，拿到主链路。
2. 在 dashboard 里搜索具体节点名，例如 `DataManager`、`IdbBuilder`、`run_placer`、`CmdEdadbRead`。
3. 每次只看一个节点的一跳关系，即“谁调用它、它调用谁”。
4. 对具体文件再使用 `/understand-explain`，对概念链路使用 `/understand-chat`。
