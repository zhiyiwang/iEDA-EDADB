# edadb-idb 相比 master 的修改与 DEF 读写对应关系

本文说明 `edadb-idb` 分支相对 `master` 做了哪些修改，以及这些修改如何对应 iEDA 原始 DEF read/write 流程。

## 结论

`master` 分支只有传统文本文件路径：

```text
DEF text
  -> DefRead callbacks
  -> IdbDefService / IdbDesign
  -> EDA tools modify iDB
  -> DefWrite
  -> DEF text
```

`edadb-idb` 分支新增了一条数据库持久化路径：

```text
iDB in memory
  -> DefWriteEdadb
  -> EDADB SQLite tables
  -> DefReadEdadb
  -> iDB in memory
```

也就是说，EDADB 并没有替代 iDB。它替代或补充的是 DEF 文本读写的一部分持久化通道：上层仍然是 `DataManager`/`IdbBuilder`，点工具仍然消费和修改 iDB 对象。

## 相比 master 的主要修改

### 1. 新增 EDADB 子模块和构建入口

新增或修改：

| 文件/目录 | 作用 |
| --- | --- |
| `.gitmodules` | 增加 `src/database/edadb/core` 子模块，指向 `zhiyiwang/edadb`。 |
| `src/database/edadb/core` | EDADB core 子模块，提供 SQLite backend、table definition、DbTableOp API。 |
| `src/database/edadb/CMakeLists.txt` | 把 EDADB 目录接入 iEDA 构建。 |
| `src/database/edadb/idb/CMakeLists.txt` | 构建 iDB 到 EDADB 的 adapter。 |
| `CMakeLists.txt`、`src/apps/CMakeLists.txt` | 把 `edadb` 相关 target/link 进主程序。 |

### 2. 新增 iDB <-> EDADB schema/shadow 层

新增目录：`src/database/edadb/idb`

核心文件：

| 文件 | 作用 |
| --- | --- |
| `edadb_idb_schema.h` | 定义 iDB/Shadow 类型到 EDADB 表的映射。 |
| `edadb_idb_shadow.h` | 声明 shadow/helper 结构。 |
| `edadb_idb_init.cpp` | 初始化数据库、主键规则和所有表。 |
| `shadow/shadow_idb_*.h` | 将复杂 iDB 对象压成可 ORM 的 shadow 对象。 |

当前已纳入 `initAllTables()` 的对象族包括：

```text
Design
Die
Row
TrackGrid
GCellGrid
Via
Instance
Pin
Blockage
Region
Slot
Group
Fill
SpecialNet
Net
RegularWire / SpecialWire / segment / pin ref
```

为什么需要 shadow 层：

1. 原始 iDB 类里有大量指针、私有成员、对象所有权关系。
2. SQLite 表更适合存值类型、字符串、数字、vector、外键式引用。
3. shadow 对象把 `IdbInstance`、`IdbPin`、`IdbNet` 这类对象拆成可持久化字段，例如名字、类型、坐标、layer name、via name、pin ref。
4. 读回时再根据 name/ref 去 `IdbLayout` 或 `IdbDesign` 中找回真实对象指针。

### 3. 在 DataManager/IdbBuilder 增加 EDADB 读写入口

新增或修改：

| 文件 | 修改 |
| --- | --- |
| `src/platform/data_manager/idm.h` | 增加 `readDefFromEdadb()`、`saveDefToEdadb()`。 |
| `src/platform/data_manager/idm_edadb.cpp` | 实现 DataManager 到 IdbBuilder 的桥接。 |
| `src/database/manager/builder/builder.h` | 增加 `buildDefFromEdadb()`、`saveDefToEdadb()`。 |
| `src/database/manager/builder/builder.cpp` | 实现 EDADB 版 build/save。 |

EDADB 读路径：

```text
DataManager::readDefFromEdadb(edadb_path, def_path)
  -> IdbBuilder::buildDefFromEdadb(edadb_path, def_path)
  -> DefReadEdadb::createDbFromEdadb(edadb_path, def_path)
  -> DefReadEdadb::createDbByEdadb(edadb_path)
  -> readIdbDesign/readIdbDie/.../readIdbNet
  -> createDbByDef(def_path)
  -> buildNet/buildBus/log
```

EDADB 写路径：

```text
DataManager::saveDefToEdadb(edadb_path)
  -> IdbBuilder::saveDefToEdadb(edadb_path)
  -> DefWriteEdadb::writeDb2Edadb(edadb_path)
  -> initWriteDb()
  -> writeChip2Edadb()
  -> writeIdbDesign/writeIdbDie/.../writeIdbNet
```

### 4. 新增 EDADB 版 DEF reader/writer

新增：

| 文件 | 作用 |
| --- | --- |
| `def_read_edadb.h/.cpp` | 从 EDADB 表恢复 iDB 对象。 |
| `def_write_edadb.h/.cpp` | 从 iDB 对象写入 EDADB 表。 |

继承关系：

```text
DefReadEdadb  : public DefRead
DefWriteEdadb : public DefWrite
```

这个设计很重要：EDADB 版读写不是完全另起炉灶，而是沿用原始 DEF reader/writer 的上下文、服务对象和部分工具函数。

### 5. 新增 Tcl 命令

修改：

| 文件 | 修改 |
| --- | --- |
| `src/interface/tcl/tcl_idb/tcl_register_idb.h` | 注册 `edadb_read`、`edadb_write`。 |
| `src/interface/tcl/tcl_idb/tcl_db_file.h/.cpp` | 增加 `CmdEdadbRead`、`CmdEdadbWrite`。 |

命令链路：

```text
edadb_read
  -> CmdEdadbRead::exec()
  -> dmInst->readDefFromEdadb(edadb_path, path)
```

```text
edadb_write
  -> CmdEdadbWrite::exec()
  -> dmInst->saveDefToEdadb(edadb_path)
```

### 6. 新增回归脚本和说明

新增：

| 文件 | 作用 |
| --- | --- |
| `src/database/edadb/test/README.md` | EDADB/iDB roundtrip 测试说明。 |
| `src/database/edadb/test/run_idb_roundtrip_regression.sh` | 自动回归入口。 |
| `src/database/edadb/test/tcl/def2edadb_generic.tcl` | DEF -> EDADB。 |
| `src/database/edadb/test/tcl/edadb2def_generic.tcl` | EDADB -> DEF。 |
| `src/database/edadb/test/tcl/direct_def_roundtrip.tcl` | 直接 DEF roundtrip 对照。 |

## 原始 DEF read 流程

在 `master` 中，DEF 读入主要经过：

```text
def_init
  -> CmdInitDef::exec()
  -> dmInst->readDef(def_path)
  -> DataManager::readDef()
  -> DataManager::initDef()
  -> IdbBuilder::buildDef(file)
  -> DefRead::createDb(file)
  -> defrRead callbacks
  -> IdbDefService / IdbDesign
  -> buildNet()
  -> buildBus()
  -> log()
```

`IdbBuilder::buildDef()` 的核心逻辑是：

```text
delete old _def_service
layout = _lef_service->get_layout()
_def_service = new IdbDefService(layout)
_def_service->DefFileInit(file)
DefRead def_read(_def_service)
def_read->createDb(file)
buildNet()
buildBus()
log()
```

`DefRead::createDb()` 的本质是调用 DEF parser，并注册一组 callback：

| DEF 内容 | callback 方向 | iDB 对象 |
| --- | --- | --- |
| VERSION / DESIGN / UNITS | version/design/units callback | `IdbDesign` 基本字段、`IdbUnits` |
| DIEAREA | die area callback | `IdbDie` |
| ROW | row callback | `IdbRow` |
| TRACKS / GCELLGRID | track/gcell callback | `IdbTrackGrid`、`IdbGCellGrid` |
| VIAS | via callbacks | `IdbVia` |
| COMPONENTS | component callbacks | `IdbInstance` |
| PINS | pin callbacks | `IdbPin` |
| BLOCKAGES | blockage callback | `IdbBlockage` |
| REGIONS / SLOTS / GROUPS / FILLS | corresponding callbacks | `IdbRegion`、`IdbSlot`、`IdbGroup`、`IdbFill` |
| SPECIALNETS | special net callbacks | `IdbSpecialNet`、special wires |
| NETS | net callbacks | `IdbNet`、regular wires |

原始读流程的特点是：**以 DEF 文本为源头，通过 callback 边读边构建 iDB。**

## 原始 DEF write 流程

在 `master` 中，DEF 写出主要经过：

```text
def_save
  -> CmdSaveDef::exec()
  -> dmInst->saveDef(def_path)
  -> DataManager::saveDef()
  -> IdbBuilder::saveDef(file)
  -> DefWrite::writeDb(file)
  -> writeChip() / writeDbSynthesis()
  -> DEF text
```

`IdbBuilder::saveDef()` 的核心逻辑是：

```text
_def_service->DefFileWriteInit(file)
DefWrite def_write(_def_service, type)
def_write->writeDb(file)
```

`DefWrite::writeDb()` 根据 `DefWriteType` 选择写出模式。完整 chip 通常对应：

```text
write_version()
write_divider_char()
write_busbit_char()
write_design()
write_units()
write_die()
write_row()
write_tracks()
write_gcell_grid()
write_via()
write_component()
write_pin()
write_blockage()
write_region()
write_slot()
write_group()
write_fill()
write_special_net()
write_net()
write_end_design()
```

原始写流程的特点是：**以 iDB 为源头，按 DEF section 顺序序列化成文本。**

## EDADB write 如何对应原始 DefWrite

`edadb-idb` 中新增的 `DefWriteEdadb::writeDb2Edadb()` 对应原始 `DefWrite::writeDb()`。

两者对照：

| 原始文本 DEF 写出 | EDADB 写出 |
| --- | --- |
| `DefWrite::writeDb(file)` | `DefWriteEdadb::writeDb2Edadb(edadb_path)` |
| `initFile(file)` | `edadb_adapter::initWriteDb(edadb_path)` |
| `writeChip()` | `writeChip2Edadb()` |
| `writeDbSynthesis()` | `writeDbSynthesis2Edadb()` |
| `write_version/write_design/write_units` | `writeIdbDesign()` |
| `write_die()` | `writeIdbDie()` |
| `write_row()` | `writeIdbRow()` |
| `write_tracks()` | `writeIdbTrackGrid()` |
| `write_gcell_grid()` | `writeIdbGCellGrid()` |
| `write_via()` | `writeIdbVia()` |
| `write_component()` | `writeIdbInstance()` |
| `write_pin()` | `writeIdbPin()` |
| `write_blockage()` | `writeIdbBlockage()` |
| `write_region()` | `writeIdbRegion()` |
| `write_slot()` | `writeIdbSlot()` |
| `write_group()` | `writeIdbGroup()` |
| `write_fill()` | `writeIdbFill()` |
| `write_special_net()` | `writeSpecialNet()` |
| `write_net()` | `writeIdbNet()` |

完整 EDADB 写出顺序：

```text
writeIdbDesign()
writeIdbDie()
writeIdbRow()
writeIdbTrackGrid()
writeIdbGCellGrid()
writeIdbVia()
writeIdbInstance()
writeIdbPin()
writeIdbBlockage()
writeIdbRegion()
writeIdbSlot()
writeIdbGroup()
writeIdbFill()
writeSpecialNet()
writeIdbNet()
```

这个顺序与 DEF section 的依赖关系基本一致：

1. 先写 design/layout 基础对象。
2. 再写 via、instance、pin、blockage 等 design 对象。
3. 最后写 special net 和 signal net，因为 net 依赖 instance/pin/via/layer 等引用。

## EDADB read 如何对应原始 DefRead

`DefReadEdadb::createDbFromEdadb()` 对应原始 `DefRead::createDb()`，但数据源变成 EDADB 表。

EDADB 读入分两段：

```text
createDbFromEdadb()
  -> initReadDb(edadb_path)
  -> createDbByEdadb(edadb_path)
  -> createDbByDef(def_path)
```

第一段 `createDbByEdadb()` 从数据库恢复主要对象：

```text
readIdbDesign()
readIdbDie()
readIdbRow()
readIdbTrackGrid()
readIdbGCellGrid()
readIdbVia()
readIdbRegion()
readIdbInstance()
readIdbPin()
readIdbBlockage()
readIdbSlot()
readIdbGroup()
readIdbFill()
readSpecialNet()
readIdbNet()
```

第二段 `createDbByDef(def_path)` 仍然调用 DEF parser，但大量 callback 被禁用：

```cpp
#if 0
defrSetVersionStrCbk(...)
defrSetDieAreaCbk(...)
defrSetRowCbk(...)
defrSetViaCbk(...)
defrSetComponentCbk(...)
defrSetPinCbk(...)
defrSetBlockageCbk(...)
defrSetNetCbk(...)
defrSetSNetCbk(...)
#endif
```

它的含义是：这些对象已经从 EDADB 恢复，不再从 DEF 文本重建。保留 DEF parser 的意义主要是兼容原有初始化/session 流程，以及作为当前迁移阶段的辅助通道。

两者对照：

| 原始文本 DEF 读入 | EDADB 读入 |
| --- | --- |
| `DefRead::createDb(file)` | `DefReadEdadb::createDbFromEdadb(edadb_path, def_path)` |
| `defrRead()` + callbacks | `edadb::readNext/readAll` + shadow restore |
| `designCallback/unitsCallback` | `readIdbDesign()` |
| `dieAreaCallback` | `readIdbDie()` |
| `rowCallback` | `readIdbRow()` |
| `trackGridCallback` | `readIdbTrackGrid()` |
| `gcellGridCallback` | `readIdbGCellGrid()` |
| `viaCallback` | `readIdbVia()` |
| `componentsCallback` | `readIdbInstance()` |
| `pinCallback` | `readIdbPin()` |
| `blockageCallback` | `readIdbBlockage()` |
| `regionCallback` | `readIdbRegion()` |
| `slotsCallback` | `readIdbSlot()` |
| `groupCallback` | `readIdbGroup()` |
| `fillCallback` | `readIdbFill()` |
| `specialNetCallback` | `readSpecialNet()` |
| `netCallback` | `readIdbNet()` |

## 为什么 EDADB read 仍然需要一个 DEF path

`CmdEdadbRead` 和 `DataManager::readDefFromEdadb` 接收两个路径：

```text
edadb_path: SQLite database path
path:       text DEF path
```

原因是当前 `IdbBuilder::buildDefFromEdadb()` 仍然沿用原始 DEF service 初始化方式：

```text
_def_service = new IdbDefService(layout)
_def_service->DefFileInit(path)
DefReadEdadb::createDbFromEdadb(edadb_path, path)
```

因此 `path` 仍被用于：

1. 初始化 `IdbDefService` 的 DEF 文件上下文。
2. 让 `createDbByDef(path)` 走一遍 DEF parser/session。
3. 在迁移阶段保留原始 DEF 文本作为辅助输入或对照。

目标形态可以理解为：未来如果 EDADB 表覆盖所有必须对象和初始化语义，`DEF text` 依赖可以继续减少。

## 数据结构层面的对应关系

| DEF section / iDB 对象 | 原始 master 路径 | edadb-idb 路径 |
| --- | --- | --- |
| DESIGN / VERSION / UNITS | DEF callback -> `IdbDesign` | `iDesign` table -> `readIdbDesign()` |
| DIEAREA | DEF callback -> `IdbDie` | `iDieSD` shadow table -> `readIdbDie()` |
| ROW | DEF callback -> `IdbRow` | `iRow` table -> `readIdbRow()` |
| TRACKS | DEF callback -> `IdbTrackGrid` | `iTrackGridSD` shadow table -> `readIdbTrackGrid()` |
| GCELLGRID | DEF callback -> `IdbGCellGrid` | `iGCellGrid` table -> `readIdbGCellGrid()` |
| VIAS | DEF callback -> `IdbVia` | `iVia` and via master shadow tables -> `readIdbVia()` |
| COMPONENTS | DEF callback -> `IdbInstance` | `iInstSD` shadow table -> `readIdbInstance()` |
| PINS | DEF callback -> `IdbPin`/`IdbTerm`/`IdbPort` | `iPinSD`/`iTermSD`/`iPortSD` -> `readIdbPin()` |
| BLOCKAGES | DEF callback -> `IdbBlockage` | `iBlockageSD` -> `readIdbBlockage()` |
| REGIONS | DEF callback -> `IdbRegion` | `iRegion` -> `readIdbRegion()` |
| SLOTS | DEF callback -> `IdbSlot` | `iSlotSD` -> `readIdbSlot()` |
| GROUPS | DEF callback -> `IdbGroup` | `iGroupSD` -> `readIdbGroup()` |
| FILLS | DEF callback -> `IdbFill` | `iFillSD`/`iFillLayerSD`/`iFillViaSD` -> `readIdbFill()` |
| SPECIALNETS | DEF callback -> `IdbSpecialNet` | `iSpecNetSD`/wire/segment/ref tables -> `readSpecialNet()` |
| NETS | DEF callback -> `IdbNet` | `iNetSD`/regular wire/ref tables -> `readIdbNet()` |

## iEDA 类级一一对应关系

下面这张表按 iEDA/iDB 类来对齐，而不是按 DEF section 粗略对齐。每一行说明同一个对象族在两个版本中分别如何读入和写出。

| iEDA/iDB 类或对象族 | master 读入：`DefRead` | master 写出：`DefWrite` | edadb-idb 读入：`DefReadEdadb` | edadb-idb 写出：`DefWriteEdadb` | EDADB 表/Shadow |
| --- | --- | --- | --- | --- | --- |
| `IdbDesign` | `designCallback()` -> `parse_design()`；`versionCallback()` -> `parse_version()`；`unitsCallback()` -> `parse_units()`；`busBitCharsCallBack()` -> `parse_bus_bit_chars()` | `write_version()`、`write_design()`、`write_units()`、`write_busbit_char()` | `readIdbDesign()` 读 `IdbDesign`，恢复 design name/version/units/bus bit chars | `writeIdbDesign()` 插入 `IdbDesign` | `iDesign`，并关联 `iUnits`、`iBusBitChars` |
| `IdbUnits` | `unitsCallback()` -> `parse_units()`，通常挂在 `IdbDesign` 下 | `write_units()` | 随 `readIdbDesign()` 从表中恢复 | 随 `writeIdbDesign()` 写入 | `iUnits` |
| `IdbBusBitChars` | `busBitCharsCallBack()` -> `parse_bus_bit_chars()` | `write_busbit_char()` | 随 `readIdbDesign()` 恢复 | 随 `writeIdbDesign()` 写入 | `iBusBitChars` |
| `IdbDie` | `dieAreaCallback()` -> `parse_die()` | `write_die()` | `readIdbDie()` 读 shadow，恢复 die polygon/box | `writeIdbDie()` 写 die shadow | `iDieSD`，内部使用 `iCoordSD` |
| `IdbRow` | `rowCallback()` -> `parse_row()`；根据 DEF row 中的 site name 绑定已有 `IdbSite` | `write_row()` | `readIdbRow()` 读 row 表，并用 site name 回连 `IdbSite` | `writeIdbRow()` 写 row vector | `iRow` |
| `IdbTrackGrid` / `IdbTrack` | `trackGridCallback()` -> `parse_track_grid()`；track layer name 需要在 `IdbLayout::get_layers()` 中解析 | `write_track_grid()` | `readIdbTrackGrid()` 读 `IdbTrackGrid` shadow，并用 layer name 回连 `IdbLayer` | `writeIdbTrackGrid()` 写 track grid shadow | `iTrackGridSD`、`iTrack` |
| `IdbGCellGrid` | `gcellGridCallback()` -> `parse_gcell_grid()` | `write_gcell_grid()` | `readIdbGCellGrid()` 读 gcell grid vector | `writeIdbGCellGrid()` 写 gcell grid vector | `iGCellGrid` |
| `IdbVia` | `viaBeginCallback()`/`viaCallback()` -> `parse_via_num()`/`parse_via()`；fixed/generate via 依赖 `IdbViaMaster`、layer shape | `write_via()` | `readIdbVia()` 读 via 表，恢复 design via list，必要时从 layout/via master 查引用 | `writeIdbVia()` 写 design via vector | `iVia`，并使用 `iViaMasterSD`、`iViaMasterGenerateSD`、`iLayerShapeSD` |
| `IdbInstance` | `componentNumberCallback()`、`componentsCallback()`、`componentEndCallback()` -> `parse_component_number()`/`parse_component()`；cell master name 解析到 `IdbCellMaster` | `write_component()` | `readIdbInstance()` 读 instance shadow，用 `_cell_master_name_sd` 找 `IdbCellMaster`，恢复坐标、状态、orient、halo/region 引用 | `writeIdbInstance()` 写 instance shadow vector | `iInstSD`，辅助 `iHalo`、`iRouteHaloSD` |
| `IdbPin` / `IdbTerm` / `IdbPort` | `pinsBeginCallback()`、`pinCallback()`、`pinsEndCallback()` -> `parse_pin_number()`/`parse_pin()`；pin shape/layer/port 从 DEF pin 信息构造 | `write_pin()` | `readIdbPin()` 读 pin shadow、term shadow、port shadow，恢复 IO pin、port、layer shape、net name 等 | `writeIdbPin()` 写 pin/term/port/layer shape 相关 shadow | `iPinSD`、`iTermSD`、`iPortSD`、`iLayerShapeSD` |
| `IdbBlockage` | `blockageCallback()` -> `parse_blockage()`；可产生 placement/routing blockage，并可能引用 instance/layer | `write_blockage()` | `readIdbBlockage()` 读 blockage shadow，用 instance name/layer name 恢复引用 | `writeIdbBlockage()` 写 blockage shadow vector | `iBlockageSD`，辅助 `IdbRect` |
| `IdbRegion` | `regionCallback()` -> `parse_region()` | `write_region()` | `readIdbRegion()` 读 region 表，恢复 region list | `writeIdbRegion()` 写 region vector | `iRegion` |
| `IdbSlot` | `slotsCallback()` -> `parse_slot()`；slot layer name 解析到 layer | `write_slot()` | `readIdbSlot()` 读 slot shadow，恢复 layer + rect list | `writeIdbSlot()` 写 slot shadow vector | `iSlotSD` |
| `IdbGroup` | `groupNameCallback()`、`groupMemberCallback()`、`groupCallback()` -> `parse_group_name()`/`parse_group_member()`/`parse_group()` | `write_group()` | `readIdbGroup()` 读 group shadow，恢复 group name、region name、member instance names | `writeIdbGroup()` 写 group shadow vector | `iGroupSD` |
| `IdbFill` / `IdbFillLayer` / `IdbFillVia` | `fillsCallback()`、`fillCallback()` -> `parse_fill_number()`/`parse_fill()`；按 layer fill 或 via fill 恢复 | `write_fill()` | `readIdbFill()` 读 fill shadow，按 layer name/via name 找回引用，恢复 rect/coord 列表 | `writeIdbFill()` 写 fill/layer/via shadow | `iFillSD`、`iFillLayerSD`、`iFillViaSD` |
| `IdbSpecialNet` | `specialNetBeginCallback()`、`specialNetCallback()`、`specialNetEndCallback()` -> `parse_special_net()`；PDN 相关还会走 `parse_pdn()`/`parse_pdn_wire()`/`parse_pdn_rects()` | `write_special_net()`，内部继续写 `IdbSpecialWire` 与 `IdbSpecialWireSegment` | `readSpecialNet()` 读 special net shadow，恢复 net 属性、pin refs、wire list、segment point/via/rect | `writeSpecialNet()` 写 special net shadow vector | `iSpecNetSD`、`iSpecWireSD`、`iSpecWireSegSD`、`iSpecPinRef` |
| `IdbSpecialWire` / `IdbSpecialWireSegment` | 作为 `parse_special_net()` 的内部对象创建 | `write_specialnet_wire()`、`write_specialnet_wire_segment()`、`write_specialnet_wire_segment_points()`、`write_specialnet_wire_segment_via()`、`write_specialnet_wire_segment_rect()` | `readSpecialNet()` 内部恢复 wire/segment；layer/via 通过名字回连 | `writeSpecialNet()` 内部写 wire/segment shadow | `iSpecWireSD`、`iSpecWireSegSD` |
| `IdbNet` | `netBeginCallback()`、`netCallback()`、`netEndCallback()` -> `parse_net_number()`/`parse_net()` | `write_net()` | `readIdbNet()` 读 net shadow，恢复普通 signal net、pin refs、regular wire list | `writeIdbNet()` 写 net shadow vector | `iNetSD`、`iNetPinRef` |
| `IdbRegularWire` / `IdbRegularWireSegment` | 作为 `parse_net()` 的内部对象创建 | `write_net_wire()`、`write_net_wire_segment()`、`write_net_wire_segment_points()`、`write_net_wire_segment_via()`、`write_net_wire_segment_rect()` | `readIdbNet()` 内部恢复 regular wire/segment；layer/via 通过名字回连 | `writeIdbNet()` 内部写 wire/segment shadow | `iRegWireSD`、`iRegWireSegSD` |
| `IdbCoordinate<int32_t>` | 不是单独 DEF section；在 die、pin、wire、fill 等 callback 中作为坐标字段创建 | 不单独写；随 die/pin/wire/fill 等对象输出 | 不单独作为顶层对象恢复；由各 shadow 中的坐标 vector 恢复 | 不单独作为顶层对象写；随各 shadow 写入 | `iCoordSD` |
| `IdbRect` | 不是单独 DEF section；在 blockage、fill、pin shape、wire rect 等解析中创建 | 不单独写；随 blockage/fill/pin/wire rect 输出 | 由 blockage/fill/layer shape/wire segment 等 shadow 恢复 | 随相关 shadow 写入 | `IdbRect` |
| `IdbLayerShape` | 不是单独 DEF section；主要作为 pin/port/via/fill/blockage 的几何形状 | 不单独写；随 pin/via/fill/blockage 输出 | 由 `readIdbPin()`、`readIdbVia()` 等间接恢复 | 随 pin/via/fill/blockage 等 shadow 写入 | `iLayerShapeSD` |

还有几类对象在 DEF read/write 中是“引用对象”，不是当前 DEF 持久化的主对象：

| 引用类 | 在 master 中的作用 | 在 edadb-idb 中的作用 |
| --- | --- | --- |
| `IdbLayer` / `IdbLayers` | 先由 LEF 读入；DEF 中的 track、wire、blockage、fill、pin shape 通过 layer name 查找它。 | EDADB shadow 通常保存 layer name，读回时仍然从已有 `IdbLayout` 的 `IdbLayers` 查找真实 layer。 |
| `IdbSite` | 先由 LEF 读入；DEF row 通过 site name 引用。 | `readIdbRow()` 根据 row 中保存的 site name 回连已有 `IdbSite`。 |
| `IdbCellMaster` | 先由 LEF 读入；DEF component 通过 master name 引用。 | `readIdbInstance()` 根据 `_cell_master_name_sd` 回连已有 `IdbCellMaster`。 |
| `IdbViaMaster` | 先由 LEF 或 DEF via 信息建立；routing segment/via 通过 via name/master 信息引用。 | EDADB 为 via master 建 shadow，但读回 wire/fill/via 时仍需要通过名字恢复真实引用。 |

这说明 `edadb-idb` 的读写仍然假设 LEF/layout 侧已经初始化。EDADB 当前主要保存 DEF/design 侧对象以及一部分为恢复 DEF 对象所需的 shadow 几何和引用信息。

## 按读写方向展开的对象流

### master 读入对象流

```text
DefRead::createDb(def)
  -> defrSetVersionStrCbk        -> parse_version          -> IdbDesign::_version
  -> defrSetBusBitCbk            -> parse_bus_bit_chars    -> IdbBusBitChars
  -> defrSetUnitsCbk             -> parse_units            -> IdbUnits
  -> defrSetDesignCbk            -> parse_design           -> IdbDesign::_design_name
  -> defrSetDieAreaCbk           -> parse_die              -> IdbDie
  -> defrSetRowCbk               -> parse_row              -> IdbRow
  -> defrSetTrackCbk             -> parse_track_grid       -> IdbTrackGrid
  -> defrSetGcellGridCbk         -> parse_gcell_grid       -> IdbGCellGrid
  -> defrSetViaCbk               -> parse_via              -> IdbVia
  -> defrSetComponentCbk         -> parse_component        -> IdbInstance
  -> defrSetPinCbk               -> parse_pin              -> IdbPin / IdbTerm / IdbPort
  -> defrSetBlockageCbk          -> parse_blockage         -> IdbBlockage
  -> defrSetRegionCbk            -> parse_region           -> IdbRegion
  -> defrSetSlotCbk              -> parse_slot             -> IdbSlot
  -> defrSetGroup*Cbk            -> parse_group*           -> IdbGroup
  -> defrSetFillCbk              -> parse_fill             -> IdbFill
  -> defrSetSNetCbk              -> parse_special_net      -> IdbSpecialNet
  -> defrSetNetCbk               -> parse_net              -> IdbNet
```

### master 写出对象流

```text
DefWrite::writeChip()
  -> write_version / write_design / write_units / write_busbit_char
  -> write_die
  -> write_row
  -> write_track_grid
  -> write_gcell_grid
  -> write_via
  -> write_component
  -> write_pin
  -> write_blockage
  -> write_region
  -> write_slot
  -> write_group
  -> write_fill
  -> write_special_net
  -> write_net
  -> write_end
```

### edadb-idb 读入对象流

```text
DefReadEdadb::createDbFromEdadb(edadb, def)
  -> initReadDb
  -> createDbByEdadb
     -> readIdbDesign       -> IdbDesign / IdbUnits / IdbBusBitChars
     -> readIdbDie          -> IdbDie
     -> readIdbRow          -> IdbRow
     -> readIdbTrackGrid    -> IdbTrackGrid
     -> readIdbGCellGrid    -> IdbGCellGrid
     -> readIdbVia          -> IdbVia
     -> readIdbRegion       -> IdbRegion
     -> readIdbInstance     -> IdbInstance
     -> readIdbPin          -> IdbPin / IdbTerm / IdbPort
     -> readIdbBlockage     -> IdbBlockage
     -> readIdbSlot         -> IdbSlot
     -> readIdbGroup        -> IdbGroup
     -> readIdbFill         -> IdbFill
     -> readSpecialNet      -> IdbSpecialNet / IdbSpecialWire
     -> readIdbNet          -> IdbNet / IdbRegularWire
  -> createDbByDef
     -> DEF parser session with most object callbacks disabled
```

### edadb-idb 写出对象流

```text
DefWriteEdadb::writeDb2Edadb(edadb)
  -> initWriteDb
  -> writeChip2Edadb
     -> writeIdbDesign       -> iDesign / iUnits / iBusBitChars
     -> writeIdbDie          -> iDieSD
     -> writeIdbRow          -> iRow
     -> writeIdbTrackGrid    -> iTrackGridSD
     -> writeIdbGCellGrid    -> iGCellGrid
     -> writeIdbVia          -> iVia
     -> writeIdbInstance     -> iInstSD
     -> writeIdbPin          -> iPinSD / iTermSD / iPortSD
     -> writeIdbBlockage     -> iBlockageSD
     -> writeIdbRegion       -> iRegion
     -> writeIdbSlot         -> iSlotSD
     -> writeIdbGroup        -> iGroupSD
     -> writeIdbFill         -> iFillSD / iFillLayerSD / iFillViaSD
     -> writeSpecialNet      -> iSpecNetSD / iSpecWireSD / iSpecWireSegSD
     -> writeIdbNet          -> iNetSD / iRegWireSD / iRegWireSegSD
```

## 对原有流程的侵入程度

`edadb-idb` 的设计是加旁路，而不是改掉主路。

原有文本 DEF 读写仍然存在：

```text
def_init -> readDef -> buildDef -> DefRead::createDb
def_save -> saveDef -> DefWrite::writeDb
```

新增 EDADB 路径是并列入口：

```text
edadb_read  -> readDefFromEdadb -> buildDefFromEdadb -> DefReadEdadb
edadb_write -> saveDefToEdadb   -> saveDefToEdadb    -> DefWriteEdadb
```

好处：

1. 原有 flow 脚本和点工具可以继续使用 iDB，不需要知道 EDADB。
2. EDADB roundtrip 可以作为独立能力逐步完善。
3. 每个 iDB 对象族可以逐个启用和测试。

代价：

1. `DefReadEdadb` 和 `DefWriteEdadb` 需要维护一套与 `DefRead/DefWrite` 对应的对象顺序。
2. shadow 对象和真实 iDB 对象之间要处理引用恢复，比如 layer name、via name、instance name、pin name。
3. 当前仍保留 `def_path`，说明读路径还没有完全脱离 DEF 文本上下文。

## 当前分支的实际状态

从提交历史看，`edadb-idb` 在 `edadb` 基础上按对象族逐步启用 roundtrip：

```text
design
die
row
track/gcell
via
instance
pin
blockage
region
slot
group
fill
special net
net
```

后续修复提交还包括：

```text
preserve optional net fields
harden net roundtrip
support nonempty fill roundtrip
preserve def group members
```

因此这个分支的重点不是简单“能链接 EDADB”，而是让 iDB 中越来越多的 DEF 对象可以：

```text
DEF -> iDB -> EDADB -> iDB -> DEF
```

并尽量保持语义等价。

## 建议后续阅读顺序

1. 先看 `src/platform/data_manager/idm_edadb.cpp`，理解新增入口。
2. 再看 `src/database/manager/builder/builder.cpp` 中 `buildDefFromEdadb()` 和 `saveDefToEdadb()`。
3. 然后看 `DefWriteEdadb::writeChip2Edadb()`，因为写路径更直观。
4. 再看 `DefReadEdadb::createDbByEdadb()`，理解对象恢复顺序。
5. 最后按对象族读 `shadow/shadow_idb_*.h` 和对应 `readIdbXXX/writeIdbXXX`。
