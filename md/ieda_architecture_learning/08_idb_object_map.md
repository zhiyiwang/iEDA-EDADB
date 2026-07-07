# iDB 对象速查表

这个文件用于把 EDA 概念直接对应到 iDB 类。阅读点工具源码时，看到这些类名就能判断它在操作哪个后端概念。

## 相关类及执行过程

### Layout 侧对象

Layout 侧主要来自 LEF/tech LEF，描述工艺、物理规则和可用资源。

| iDB 类 | 位置 | EDA 含义 |
| --- | --- | --- |
| `IdbLayout` | `src/database/data/design/IdbLayout.h` | layout 总对象，聚合 layer/site/row/track/cell master/via 等。 |
| `IdbLayer` | `db_layout/IdbLayer.h` | 通用 layer 基类。 |
| `IdbLayerRouting` | `db_layout/IdbLayer.h` | routing metal layer。 |
| `IdbLayerCut` | `db_layout/IdbLayer.h` | cut/via layer。 |
| `IdbLayers` | `db_layout/IdbLayer.h` | layer 集合。 |
| `IdbSite`、`IdbSites` | `db_layout/IdbSite.h` | 标准单元 site。 |
| `IdbRow`、`IdbRows` | `db_layout/IdbRow.h` | placement row。 |
| `IdbCellMaster`、`IdbCellMasterList` | `db_layout/IdbCellMaster.h` | 标准单元或 macro master。 |
| `IdbViaMaster`、`IdbViaMasterList` | `db_layout/IdbViaMaster.h` | LEF via master。 |
| `IdbViaRuleGenerate`、`IdbViaRuleList` | `db_layout/IdbViaRule.h` | via generation rule。 |
| `IdbTrackGridList` | `db_design/IdbTrackGrid.h` | routing track grid。 |

执行过程：

```text
tech_lef_init / lef_init
  -> DataManager::readLef
  -> IdbBuilder::buildLef
  -> LefRead::createDb
  -> IdbLefService
  -> IdbLayout
```

### Design 侧对象

Design 侧主要来自 DEF/Verilog，描述当前设计实例、连接、pin 和布线结果。

| iDB 类 | 位置 | EDA 含义 |
| --- | --- | --- |
| `IdbDesign` | `src/database/data/design/IdbDesign.h` | design 总对象，聚合 instance/pin/net/blockage/region/fill 等。 |
| `IdbInstance`、`IdbInstanceList` | `db_design/IdbInstance.h` | cell/macro/filler/buffer instance。 |
| `IdbPin`、`IdbPins` | `db_design/IdbPins.h` | IO pin 或 instance pin。 |
| `IdbNet`、`IdbNetList` | `db_design/IdbNet.h` | signal net。 |
| `IdbSpecialNet`、`IdbSpecialNetList` | `db_design/IdbSpecialNet.h` | power/ground/clock 等 special net。 |
| `IdbVia`、`IdbVias` | `db_design/IdbVias.h` | design 中实例化的 via。 |
| `IdbBlockage`、`IdbBlockageList` | `db_design/IdbBlockages.h` | placement/routing blockage。 |
| `IdbRegion`、`IdbRegionList` | `db_design/IdbRegion.h` | placement region/group constraint。 |
| `IdbFill`、`IdbFillList` | `db_design/IdbFill.h` | filler/metal fill。 |
| `IdbBusList` | `db_design/IdbBus.h` | bus net/bit 信息。 |

执行过程：

```text
def_init
  -> DataManager::readDef
  -> IdbBuilder::buildDef
  -> DefRead::createDb
  -> buildNet / buildBus
  -> IdbDefService
  -> IdbDesign
```

```text
verilog_init
  -> DataManager::readVerilog
  -> IdbBuilder::rustBuildVerilog
  -> RustVerilogRead
  -> IdbDesign
```

### Service / Builder 对象

| 类 | 作用 |
| --- | --- |
| `IdbLefService` | 管理 `IdbLayout`，服务 LEF 侧数据。 |
| `IdbDefService` | 管理 `IdbDesign`，服务 DEF/design 侧数据。 |
| `IdbBuilder` | 创建 service，调用 parser/writer，执行 `buildNet/buildBus`。 |
| `DataManager` | 平台层统一入口，保存 builder/service/design/layout 指针。 |

## EDA 抽象与 iEDA 类的对应关系

| EDA 概念 | iDB 类 | 常见修改者 |
| --- | --- | --- |
| 标准单元库 | `IdbCellMasterList` | LEF reader |
| 工艺层 | `IdbLayers`、`IdbLayerRouting`、`IdbLayerCut` | LEF reader、routing/DRC |
| 设计实例 | `IdbInstance` | Verilog/DEF reader、iPL、iCTS、iTO、iNO |
| 逻辑网络 | `IdbNet` | Verilog/DEF reader、iTO、iNO |
| IO pin | `IdbPin`、`IdbPins` | DEF reader、iFP |
| Placement 坐标 | `IdbInstance` location/status | iFP、iPL、iTO |
| Clock tree | `IdbNet`、`IdbInstance`、clock tree report | iCTS |
| Routing wire | `IdbRegularWire`、`IdbSpecialWire`、`IdbVia` | iRT、iPDN、iPNP |
| Blockage | `IdbBlockage` | iFP、用户 Tcl 命令、routing/placement 约束 |
| Timing constraint | iSTA SDC 对象 | iSTA |
| Liberty timing arc | iSTA liberty model | iSTA、iTO |
| DRC violation | iDRC violation/result object | iDRC、iRT DRC engine |
| EDADB record | schema/shadow/table object | `DefReadEdadb`、`DefWriteEdadb` |

## 阅读建议

读点工具源码时，可以先判断它在改哪类对象：

```text
改 IdbInstance 坐标/status -> placement/floorplan/optimization
改 IdbNet 连接 -> netlist optimization/timing optimization
改 wire/via -> routing/PDN/PNP
只读 timing/lib/sdc -> STA/TO 分析阶段
只输出 violation/report -> DRC/report/eval
```

这个判断比从文件名猜功能可靠很多。
