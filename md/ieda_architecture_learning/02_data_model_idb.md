---
title: iDB 数据模型与文件读写
tags:
  - iEDA
  - iDB
  - database
---

# iDB 数据模型与文件读写

> [!info] Source baseline
> 行号按 `edadb-idb-dev/sort-abc-no-sort-d @ 77fbe5c67` 核对。

[学习地图](00_learning_map.md) · [代码主线](10_complete_tutorial.md) · [点工具](04_eda_tools.md)

## 1. Platform and Builder Roles

| Object | Role | Code anchor |
| --- | --- | --- |
| `DataManager` | 平台数据门面；保存 builder/service/root pointers。 | [`DataManager::readDef()`](../../src/platform/data_manager/idm.cpp#L98) |
| `IdbBuilder` | 文件格式与 iDB 之间的构建器。 | [`IdbBuilder::buildDef()`](../../src/database/manager/builder/builder.cpp#L100) |
| `IdbLefService` | 持有 LEF/layout 侧服务。 | [`IdbBuilder::buildLef()`](../../src/database/manager/builder/builder.cpp#L158) |
| `IdbDefService` | 持有 DEF/design 侧服务，并共享 LEF layout。 | [`IdbBuilder::buildDef()`](../../src/database/manager/builder/builder.cpp#L107) |

## 2. `IdbLayout` versus `IdbDesign`

| Root | Source and lifetime | Representative data |
| --- | --- | --- |
| [`IdbLayout`](../../src/database/data/design/IdbLayout.h#L56) | LEF technology/library plus layout-side DEF/floorplan data | layers, sites, rows, tracks, gcell grids, cell masters, via rules, die/core |
| [`IdbDesign`](../../src/database/data/design/IdbDesign.h#L57) | DEF/Verilog and point-tool implementation updates | instances, IO pins, nets, special nets, design vias, blockages, regions, groups, fills, routed wires |

Useful root accessors:

- rows: [`IdbLayout::get_rows()`](../../src/database/data/design/IdbLayout.h#L69)
- tracks: [`IdbLayout::get_track_grid_list()`](../../src/database/data/design/IdbLayout.h#L71)
- cell masters: [`IdbLayout::get_cell_master_list()`](../../src/database/data/design/IdbLayout.h#L73)
- instances: [`IdbDesign::get_instance_list()`](../../src/database/data/design/IdbDesign.h#L69)
- IO pins: [`IdbDesign::get_io_pin_list()`](../../src/database/data/design/IdbDesign.h#L70)
- signal nets: [`IdbDesign::get_net_list()`](../../src/database/data/design/IdbDesign.h#L71)
- special nets: [`IdbDesign::get_special_net_list()`](../../src/database/data/design/IdbDesign.h#L77)

## 3. LEF Build Path

```text
DataManager::readLef()
  -> DataManager::initLef()
  -> IdbBuilder::buildLef()
  -> LefRead::createDb()
  -> IdbLayout
```

- read tech LEF before cell LEF: [`DataManager::readLef()`](../../src/platform/data_manager/idm.cpp#L63)
- save resulting layout pointer: [`DataManager::initLef()`](../../src/platform/data_manager/idm_init.cpp#L33)
- read every LEF file: [`IdbBuilder::buildLef()`](../../src/database/manager/builder/builder.cpp#L173)

## 4. DEF Build Path

```text
DataManager::readDef()
  -> DataManager::initDef()
  -> IdbBuilder::buildDef()
  -> DefRead callbacks
  -> buildNet() / buildBus()
  -> active IdbDesign
```

- reject DEF read without LEF/layout: [`DataManager::readDef()`](../../src/platform/data_manager/idm.cpp#L98)
- assign active design pointer: [`DataManager::initDef()`](../../src/platform/data_manager/idm_init.cpp#L41)
- create `IdbDefService(layout)`: [`IdbBuilder::buildDef()`](../../src/database/manager/builder/builder.cpp#L107)
- callback registration: [`DefRead::createDb()`](../../src/database/manager/builder/def_builder/def_read.cpp#L85)
- parser execution: [`DefRead::createDb()`](../../src/database/manager/builder/def_builder/def_read.cpp#L134)
- post-parse relationships: [`IdbBuilder::buildDef()`](../../src/database/manager/builder/builder.cpp#L122)

## 5. DEF Save Path

```text
DataManager::saveDef()
  -> IdbBuilder::saveDef()
  -> DefWrite::writeDb()
  -> DefWrite::writeChip()
```

- platform forwarding: [`DataManager::saveDef()`](../../src/platform/data_manager/idm_save.cpp#L60)
- construct `DefWrite`: [`IdbBuilder::saveDef()`](../../src/database/manager/builder/builder.cpp#L263)
- section order: [`DefWrite::writeChip()`](../../src/database/manager/builder/def_builder/def_write.cpp#L205)

## 6. Derived and Cross-Level State

Not every C++ member is an independent source field:

- instance references a LEF cell master;
- pins reference layers/nets/instances;
- parser callbacks and setters calculate bbox, average coordinates and connectivity;
- `buildNet()`/`buildBus()` add relations after raw parsing.

Therefore persistence should store stable source values and names/keys, then rebuild active references and derived state through the same setters/helpers used by native iEDA.

## 7. Tool Consumption

Most point tools do not operate on the full object graph in-place. They select a view:

- iPL wraps rows, cells, instances, nets and regions into placement DB.
- iCTS maps clock nets/pins/instances into CTS objects.
- iRT wraps layers, vias, obstacles and nets into routing DB.
- iSTA converts connectivity into timing netlist/graph.
- iDRC converts geometry into DRC shapes.

Continue with [iEDA 点工具与 iDB 数据桥接](04_eda_tools.md).
