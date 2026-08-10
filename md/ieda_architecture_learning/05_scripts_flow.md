---
title: sky130 iEDA 多进程物理设计流程
aliases:
  - iEDA Script Flow
tags:
  - iEDA
  - flow
  - sky130
---

# sky130 iEDA 多进程物理设计流程

> [!info] Source baseline
> 脚本和源码链接按 `edadb-idb-dev/sort-abc-no-sort-d @ 77fbe5c67` 核对。

[学习地图](00_learning_map.md) · [代码主线](10_complete_tutorial.md) · [点工具](04_eda_tools.md)

## 1. The Most Important Runtime Fact

The driver starts a fresh `iEDA` executable for every stage:

- first floorplan process: [`run_iEDA.sh`](../../scripts/design/sky130_gcd/run_iEDA.sh#L25)
- ordered stage list: [`run_iEDA.sh`](../../scripts/design/sky130_gcd/run_iEDA.sh#L28)
- process loop: [`run_iEDA.sh`](../../scripts/design/sky130_gcd/run_iEDA.sh#L42)

```text
process N: read LEF + previous DEF -> run one tool -> save DEF/Verilog -> exit
process N+1: read LEF + previous result -> run next tool -> save -> exit
```

So the stage output is not only a report; it is the persistent input of the next process.

## 2. Stage Pipeline

| Order | Script | Main action | Persistent output |
| --- | --- | --- | --- |
| 1 | `iFP_script/run_iFP.tcl` | Build floorplan from LEF + Verilog/config. | `iFP_result.def` |
| 2 | `iNO_script/run_iNO_fix_fanout.tcl` | Insert fanout buffers and split nets. | fanout-fixed DEF/Verilog |
| 3 | `iPL_script/run_iPL.tcl` | Global/legal/detailed placement. | `iPL_result.def` |
| 4 | `iCTS_script/run_iCTS.tcl` | Build clock tree. | `iCTS_result.def` |
| 5 | `iCTS_script/run_iCTS_STA.tcl` | Analyze CTS timing. | reports |
| 6 | `iTO_script/run_iTO_drv.tcl` | Fix DRV. | `iTO_drv_result.def` |
| 7 | `iTO_script/run_iTO_hold.tcl` | Fix hold. | `iTO_hold_result.def` |
| 8 | `iPL_script/run_iPL_legalization.tcl` | Legalize inserted/changed cells. | `iPL_lg_result.def` |
| 9 | `iRT_script/run_iRT.tcl` | Route signal nets. | `iRT_result.def` |
| 10 | `iRT_script/run_iRT_DRC.tcl` | Check routed design. | DRC report |
| 11 | `iPL_script/run_iPL_filler.tcl` | Insert filler-cell instances. | filler DEF/Verilog |
| 12 | `DB_script/run_def_to_gds_text.tcl` | Export final layout. | GDS/text |

The authoritative order is the shell script, not the document table: [`run_iEDA.sh`](../../scripts/design/sky130_gcd/run_iEDA.sh#L28).

## 3. Common Stage Template

Most stage Tcl files follow this shape:

```text
flow/db config
  -> source LEF paths and read LEF
  -> def_init previous_stage.def
  -> run point-tool command
  -> def_save current_stage.def
  -> netlist_save current_stage.v
  -> reports/features
  -> flow_exit
```

DB command implementations:

- tech LEF: [`CmdInitTechLef::exec()`](../../src/interface/tcl/tcl_idb/tcl_db_file.cpp#L72)
- LEF: [`CmdInitLef::exec()`](../../src/interface/tcl/tcl_idb/tcl_db_file.cpp#L110)
- DEF: [`CmdInitDef::exec()`](../../src/interface/tcl/tcl_idb/tcl_db_file.cpp#L148)
- DEF save: [`CmdSaveDef::exec()`](../../src/interface/tcl/tcl_idb/tcl_db_file.cpp#L228)

## 4. Concrete Examples

### Placement

- load input DEF: [`run_iPL.tcl`](../../scripts/design/sky130_gcd/script/iPL_script/run_iPL.tcl#L35)
- run placer: [`run_iPL.tcl`](../../scripts/design/sky130_gcd/script/iPL_script/run_iPL.tcl#L40)
- save result DEF: [`run_iPL.tcl`](../../scripts/design/sky130_gcd/script/iPL_script/run_iPL.tcl#L46)

### CTS

- load placed DEF: [`run_iCTS.tcl`](../../scripts/design/sky130_gcd/script/iCTS_script/run_iCTS.tcl#L35)
- run CTS: [`run_iCTS.tcl`](../../scripts/design/sky130_gcd/script/iCTS_script/run_iCTS.tcl#L40)
- save clock-tree DEF: [`run_iCTS.tcl`](../../scripts/design/sky130_gcd/script/iCTS_script/run_iCTS.tcl#L48)

### Timing Optimization

- load previous result: [`run_iTO_drv.tcl`](../../scripts/design/sky130_gcd/script/iTO_script/run_iTO_drv.tcl#L35)
- optimize DRV: [`run_iTO_drv.tcl`](../../scripts/design/sky130_gcd/script/iTO_script/run_iTO_drv.tcl#L40)
- save updated design: [`run_iTO_drv.tcl`](../../scripts/design/sky130_gcd/script/iTO_script/run_iTO_drv.tcl#L47)

### Routing

- load legal placement: [`run_iRT.tcl`](../../scripts/design/sky130_gcd/script/iRT_script/run_iRT.tcl#L35)
- initialize and run router: [`run_iRT.tcl`](../../scripts/design/sky130_gcd/script/iRT_script/run_iRT.tcl#L40)
- destroy router and trigger output: [`run_iRT.tcl`](../../scripts/design/sky130_gcd/script/iRT_script/run_iRT.tcl#L51)
- save routed DEF: [`run_iRT.tcl`](../../scripts/design/sky130_gcd/script/iRT_script/run_iRT.tcl#L57)

## 5. What Persists Between Stages

| Data | Main persistence path | Notes |
| --- | --- | --- |
| Physical design | DEF | die/rows/components/pins/nets/routes/constraints. |
| Logical connectivity | DEF and Verilog | Netlist output is important after buffer insertion or net splitting. |
| Technology/library | LEF re-read | Normally not copied into each DEF. |
| Timing constraints/models | SDC/Lib re-read | iSTA graph is rebuilt in a new process. |
| Parasitics | SPEF or tool-computed state | Not generally encoded by ordinary DEF object persistence. |
| Tool-private state | Usually not persisted | Placement/routing/timing internal IDs and caches are rebuilt. |

This is why a DEF-equivalent EDADB restore is necessary but not automatically sufficient for resuming every internal algorithm state.

## 6. EDADB Research Boundary

The current adapter can replace or supplement the physical-design checkpoint:

```text
stage N active iDB -> EDADB snapshot
stage N+1 EDADB restore -> active iDB -> unchanged point-tool import
```

Potential extensions must distinguish:

1. persistent iDB/DEF source data;
2. derived iDB fields rebuilt by setters/helpers;
3. point-tool private state not currently represented in iDB;
4. analysis files such as Liberty/SDC/SPEF.

## 7. Efficient Reading Order

For one stage, read only these five anchors:

1. stage `run_*.tcl` input;
2. core Tcl command;
3. command `exec()` and platform/API forwarding;
4. tool import/writeback bridge in [EDA 点工具](04_eda_tools.md);
5. `def_save` output.

This produces a complete data-lifecycle view before studying the algorithm itself.
