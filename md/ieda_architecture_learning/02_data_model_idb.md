# iDB 数据模型与文件读写

这一层回答：EDA 文件被读进来以后，在 iEDA 内部变成什么对象，后续点工具共享和修改的“设计状态”在哪里。

## 相关类及执行过程

### `idm::DataManager`

位置：`src/platform/data_manager/idm.h`、`src/platform/data_manager/idm.cpp`、`src/platform/data_manager/idm_save.cpp`

`DataManager` 是平台层的数据门面，宏 `dmInst` 指向它的单例。

核心成员：

| 成员 | 类型 | 作用 |
| --- | --- | --- |
| `_config` | `DataConfig` | 保存 LEF/DEF/output path 等数据配置。 |
| `_idb_builder` | `IdbBuilder*` | 调用 parser/writer 构建或保存 iDB。 |
| `_idb_def_service` | `IdbDefService*` | 维护 DEF/design 侧服务。 |
| `_idb_lef_service` | `IdbLefService*` | 维护 LEF/layout 侧服务。 |
| `_design` | `IdbDesign*` | 当前设计对象。 |
| `_layout` | `IdbLayout*` | 当前工艺/版图对象。 |

关键方法：

| 方法 | 执行过程 |
| --- | --- |
| `init(config_path)` | 创建 `IdbBuilder`，读配置，依次 `initLef`、`initDef`。 |
| `readLef(config_path)` | 读配置，先读 tech LEF，再读普通 LEF。 |
| `readLef(vector<string>, bool)` | 直接读一组 LEF 文件。 |
| `readDef(path)` | 要求 LEF/layout 已经存在，然后 `initDef(path)`。 |
| `readVerilog(path, top)` | 要求 LEF/layout 已经存在，然后由 builder 构建 design。 |
| `saveDef(path)` | 调用 `_idb_builder->saveDef(path)`。 |
| `saveGDSII(path)` | 调用 `_idb_builder->saveGDSII(path)`。 |
| `saveJSON(path, options)` | 调用 `_idb_builder->saveJSON(path, options)`。 |

典型执行链路：

```text
lef_init / def_init / verilog_init
  -> CmdXXX::exec()
  -> dmInst->readLef/readDef/readVerilog
  -> DataManager::initLef/initDef/initVerilog
  -> IdbBuilder::buildLef/buildDef/rustBuildVerilog
  -> LefRead / DefRead / RustVerilogRead
  -> IdbLayout / IdbDesign
```

### `idb::IdbBuilder`

位置：`src/database/manager/builder/builder.h`、`src/database/manager/builder/builder.cpp`

`IdbBuilder` 是文件格式与 iDB 对象之间的构建器。

关键方法：

| 方法 | 作用 |
| --- | --- |
| `buildLef(files, b_techfile)` | 创建或重建 `IdbLefService`，用 `LefRead` 读入工艺/宏单元/层信息。 |
| `buildDef(file)` | 基于已有 `IdbLayout` 创建 `IdbDefService`，用 `DefRead` 读入设计。 |
| `rustBuildVerilog(file, top)` | 用 Rust Verilog reader 从网表生成 design。 |
| `saveDef(file)` | 用 `DefWrite` 输出 DEF。 |
| `saveVerilog(file)` | 用 `VerilogWriter` 输出网表。 |
| `saveGDSII(file)` | 用 `Def2GdsWrite` 输出 GDS。 |
| `saveJSON(file, options)` | 用 `Gds2JsonWrite` 输出 JSON。 |

`buildDef` 读完 DEF 后会调用：

```text
buildNet()
buildBus()
log()
```

这表示 parser 只是把文件内容转换成原始对象，builder 还会补充 net/pin/bus 等内部关系，并打印 layout/design 统计信息。

### `idb::IdbLayout`

位置：`src/database/data/design/IdbLayout.h`

`IdbLayout` 描述工艺和布局规则，主要来自 LEF/tech LEF。

核心内容：

| iDB 类 | EDA 含义 |
| --- | --- |
| `IdbLayers` | routing/cut/implant 等层信息。 |
| `IdbSites` | 标准单元摆放 site。 |
| `IdbRows` | DEF 中的 row，placement 可用轨道。 |
| `IdbTrackGridList` | routing track。 |
| `IdbGCellGridList` | global routing grid。 |
| `IdbCellMasterList` | standard cell/macro master。 |
| `IdbVias`、`IdbViaRuleList` | via/via rule。 |
| `IdbDie`、`IdbCore` | die/core 区域。 |

### `idb::IdbDesign`

位置：`src/database/data/design/IdbDesign.h`

`IdbDesign` 描述当前芯片设计实例，主要来自 DEF/Verilog。

核心内容：

| iDB 类 | EDA 含义 |
| --- | --- |
| `IdbInstanceList` | instance/cell/macro 实例。 |
| `IdbPins` | IO pin。 |
| `IdbNetList` | signal net。 |
| `IdbSpecialNetList` | power/ground/clock 等 special net。 |
| `IdbVias` | design 中实际使用的 via。 |
| `IdbBlockageList` | placement/routing blockage。 |
| `IdbRegionList` | placement region。 |
| `IdbFillList` | filler/metal fill 等填充对象。 |
| `IdbBusList` | bus 结构。 |

## EDA 抽象与 iEDA 类的对应关系

| EDA 抽象 | 文件来源 | iEDA 类/模块 |
| --- | --- | --- |
| Tech LEF | `.tlef` | `IdbLayout`、`IdbLayers`、`IdbViaRuleList` |
| Standard cell / macro LEF | `.lef` | `IdbCellMasterList`、`IdbTerm`、`IdbSite` |
| DEF design | `.def` | `IdbDesign`、`IdbInstanceList`、`IdbNetList` |
| Verilog netlist | `.v` | `RustVerilogRead`、`IdbDesign`、`IdbInstance`、`IdbNet` |
| Die/core | DEF DIEAREA / ROW + floorplan 信息 | `IdbDie`、`IdbCore`、`IdbRows` |
| Pin | DEF PINS / LEF pins | `IdbPins`、`IdbTerm` |
| Net | Verilog/DEF NETS | `IdbNetList`、`IdbNet` |
| Power grid | DEF SPECIALNETS | `IdbSpecialNetList` |
| Routing result | DEF wires/vias | `IdbRegularWire`、`IdbSpecialWire`、`IdbVias` |
| Output artifact | DEF/GDS/Verilog/JSON | `DefWrite`、`Def2GdsWrite`、`VerilogWriter`、`Gds2JsonWrite` |

## 阅读建议

先把 `Layout` 和 `Design` 分清：

```text
Layout = 工艺/规则/可用资源
Design = 这颗芯片当前有哪些实例、网络、pin、阻塞和布线
```

大多数点工具的本质都是读取 `IdbLayout + IdbDesign`，修改 `IdbDesign`，最后由 `DataManager` 保存为 DEF、Verilog、GDS 或 EDADB。
