# Sky130 GCD EDADB Demo

GCD means **Greatest Common Divisor**. It is the small digital benchmark design implemented by this
example flow. `sky130_gcd` means that the GCD design is synthesized and physically implemented with
the SkyWater 130 nm PDK; GCD is the design, while Sky130 supplies its manufacturing and cell data.

This demo validates an existing Sky130 GCD stage DEF through three independent iEDA processes:

```text
native:  Sky130 LEF + input DEF -> iDB -> direct.def
write:   Sky130 LEF + input DEF -> iDB -> edadb.db
read:    Sky130 LEF + edadb.db  -> iDB -> edadb.def
oracle:  direct.def == edadb.def by exact textual comparison
```

The stage aliases below all use the same Sky130 technology and cell LEFs. Therefore they can
replace the default `iPL_filler_result.def` without changing the PDK profile.

## Generate The Sky130 Result DEFs

The files under `scripts/design/sky130_gcd/result/*.def` are generated physical-design results.
They are intentionally ignored by Git: their contents depend on the exact iEDA, EDADB, PDK,
configuration and point-tool execution used to produce them. A fresh checkout must rebuild iEDA
and rerun the checked-in Sky130 GCD flow instead of treating generated DEF files as source data.

From first principles, the flow starts with the synthesized gate-level netlist
`scripts/design/sky130_gcd/result/verilog/gcd.v`. The Sky130 LEF/LIB/SDC/SPEF data and the checked-in
iEDA JSON/Tcl configuration then progressively add floorplan, placement, clock, optimization,
routing and filler state. Each stage reads the previous stage's DEF and saves a richer DEF.

First align the EDADB submodule and build iEDA from the repository root:

```bash
cd /home/zhiyiwang/cs/arch/eda/iEDA-EDADB
git submodule update --init --recursive

# Optional full clean before rebuilding.
bash build.sh -d -n -y
bash build.sh -j40
test -x bin/iEDA
```

Then run the complete Sky130 GCD physical-design flow from `bin/`:

```bash
cd /home/zhiyiwang/cs/arch/eda/iEDA-EDADB/bin
bash ../scripts/design/sky130_gcd/run_iEDA.sh \
  2>&1 | tee /tmp/sky130_gcd_flow.out
```

`run_iEDA.sh` establishes the design/PDK environment and executes this dependency chain:

| Order | Point-tool Tcl | Input | Generated DEF |
| --- | --- | --- | --- |
| 1 | `iFP_script/run_iFP.tcl` | GCD netlist + Sky130 PDK | `iFP_result.def` |
| 2 | `iNO_script/run_iNO_fix_fanout.tcl` | `iFP_result.def` | `iTO_fix_fanout_result.def` |
| 3 | `iPL_script/run_iPL.tcl` | `iTO_fix_fanout_result.def` | `iPL_result.def` |
| 4 | `iCTS_script/run_iCTS.tcl` | `iPL_result.def` | `iCTS_result.def` |
| 5 | `iTO_script/run_iTO_drv.tcl` | `iCTS_result.def` | `iTO_drv_result.def` |
| 6 | `iTO_script/run_iTO_hold.tcl` | `iTO_drv_result.def` | `iTO_hold_result.def` |
| 7 | `iPL_script/run_iPL_legalization.tcl` | `iTO_hold_result.def` | `iPL_lg_result.def` |
| 8 | `iRT_script/run_iRT.tcl` | `iPL_lg_result.def` | `iRT_result.def` |
| 9 | `iPL_script/run_iPL_filler.tcl` | `iRT_result.def` | `iPL_filler_result.def` |

The flow also runs STA/DRC checks between selected stages and finally generates GDS; those checks do
not replace the result DEFs listed above. Confirm the default demo input was produced before running
the EDADB test:

```bash
test -s ../scripts/design/sky130_gcd/result/iPL_filler_result.def
```

## Run

Run the default `filler` case, which provides the widest existing Sky130 GCD DEF coverage:

```bash
cd bin
bash /home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/demo/demo.sh \
  2>&1 | tee run.out
```

List available stages:

```bash
bash scripts/edadb/demo/demo.sh --list
```

Run a named stage:

```bash
bash scripts/edadb/demo/demo.sh irt
bash scripts/edadb/demo/demo.sh filler
```

With no argument, artifacts remain in `scripts/edadb/demo/result/` for compatibility. A named
stage writes to `scripts/edadb/demo/result/<stage>/` unless `RUN_DIR` is supplied:

```bash
RUN_DIR=/tmp/iedadb_irt bash scripts/edadb/demo/demo.sh irt
```

## Root Families

In this project, a **root family** is one top-level adapter read/write unit. It starts from a DEF
top-level field, statement group or section, is owned by an iDB singleton/container, and is stored
as one EDADB root table with any nested objects in child tables.

```text
DEF tag/section
  -> iDB singleton or root list
  -> writeIdbT()/readIdbT()
  -> EDADB root table
  -> nested child tables for vectors and owned subobjects
```

For example:

| DEF data | iDB root owner and element | EDADB storage relationship |
| --- | --- | --- |
| `ROW` statements | `IdbRows::_row_list` / `IdbRow` | Row root records; Site is rebuilt by name from LEF |
| `COMPONENTS` | `IdbInstanceList::_instance_list` / `IdbInstance` | Instance root records plus placement/Halo fields |
| `NETS` | `IdbNetList::_net_list` / `IdbNet` | Net roots with Wire, Segment, Point and Via-reference child tables |

Root family does not mean a C++ inheritance root. It is a persistence and test boundary. Design and
Die are singleton roots; Row, Instance, Pin, Net and similar families are root lists; Port, Rect,
Wire and Segment are nested children of another root family.

## Stage DEF Sequence

The result directory is:

```text
/home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/design/sky130_gcd/result/
```

Its DEF files represent successive physical-design states consumed or produced by point tools. The
"increment" column describes what changes relative to the previous row; unchanged root families
remain present in the cumulative iDB state.

| Alias | Point-tool stage | Increment relative to previous stage | Non-empty iEDA/iDB root families after this stage | Result DEF path |
| --- | --- | --- | --- | --- |
| `ifp` | Floorplan/PDN, iFP/iPDN | Establish Die, Rows, Tracks, design Vias, Components, IO Pins, Nets and PDN SpecialNets | Design, Die, Row, TrackGrid, Via, Instance, Pin, SpecialNet, Net | `scripts/design/sky130_gcd/result/iFP_result.def` |
| `fix-fanout` | Fanout repair before placement | Add/reconnect buffer Instances, Pins and Nets | Same root families; Instance/Pin/Net contents change | `scripts/design/sky130_gcd/result/iTO_fix_fanout_result.def` |
| `ipl` | Global placement, iPL | Set standard-cell coordinates, orientation and placement status | Same root families; Instance placement state changes | `scripts/design/sky130_gcd/result/iPL_result.def` |
| `icts` | Clock-tree synthesis, iCTS | Add clock buffers, Pins and clock Nets | Same root families; Instance/Pin/Net contents increase | `scripts/design/sky130_gcd/result/iCTS_result.def` |
| `ito-drv` | DRV optimization, iTO | Insert/resize buffers and reconnect Nets | Same root families; topology changes | `scripts/design/sky130_gcd/result/iTO_drv_result.def` |
| `ito-hold` | Hold optimization, iTO | Add hold-fix buffers and reconnect Nets | Same root families; topology changes again | `scripts/design/sky130_gcd/result/iTO_hold_result.def` |
| `ipl-lg` | Incremental legalization, iPL | Legalize optimized Instance coordinates | Same root families; final pre-route placement state | `scripts/design/sky130_gcd/result/iPL_lg_result.def` |
| `irt` | Detailed routing, iRT | Add six GCellGrid roots and regular Net Wire/Segment/Point/Via geometry | Adds GCellGrid; routed Net nested objects become non-empty | `scripts/design/sky130_gcd/result/iRT_result.def` |
| `filler` | Filler-cell insertion, iPL | Add physical-only filler-cell Instances | Same root families as `irt`; Instance count increases | `scripts/design/sky130_gcd/result/iPL_filler_result.def` |

The resulting root-family status is:

| Status in the listed Sky130 result DEFs | iEDA/iDB root families |
| --- | --- |
| Non-empty from `ifp` onward | Design, Die, Row, TrackGrid, Via, Instance, Pin, SpecialNet, Net |
| First becomes non-empty at `irt` | GCellGrid |
| Defined by the adapter but empty in all listed results | Blockage, Region, Slot, Group, Fill |

The `filler` stage changes `IdbInstanceList`; its filler cells are Components/Instances and are not
`IdbFill` objects from the DEF `FILLS` section.

`iPL_result_edadb.def` is an earlier EDADB-generated comparison output, not an independent point-
tool stage, so it is intentionally not exposed as a demo alias.

## Recommended Single Demo

If only one case can be shown, use `filler`:

```bash
RUN_DIR=/tmp/iedadb_filler bash scripts/edadb/demo/demo.sh filler
```

`iPL_filler_result.def` is generated from the routed `iRT_result.def` and then adds physical-only
filler-cell Instances. It is therefore the largest existing DEF state in this Sky130 GCD flow.

Tags/records present and used by the current adapter are:

| DEF tag/record | Adapter root family | Non-empty content in `iPL_filler_result.def` |
| --- | --- | --- |
| `VERSION`, `BUSBITCHARS`, `DESIGN`, `UNITS` | Design | Version 5.8, design `gcd`, DBU 1000 |
| `DIEAREA` | Die | One rectangular die represented by two points |
| `ROW` | Row | 39 rows |
| `TRACKS` | TrackGrid | 12 track-grid records |
| `GCELLGRID` | GCellGrid | 6 gcell-grid records |
| `VIAS` | Via | 4 design Via definitions |
| `COMPONENTS` | Instance | 2604 instances, including filler cells |
| `PINS` | Pin | 56 IO pins |
| `SPECIALNETS` | SpecialNet | 2 special nets with 639 segments |
| `NETS` | Net | 677 routed nets with 8997 segments and 3716 Via references |

Adapter root tags not present as non-empty data in this result are:

| Missing DEF section | Adapter root family |
| --- | --- |
| `BLOCKAGES` | Blockage |
| `REGIONS` | Region |
| `SLOTS` | Slot |
| `GROUPS` | Group |
| `FILLS` | Fill |

`DIVIDERCHAR` and `END DESIGN` also appear in the text, but they are syntax/control records rather
than separate migrated root families in the current adapter.

This case has been validated with the complete adapter: its native canonical DEF and EDADB-restored
DEF matched exactly.
