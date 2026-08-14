# DEF iEDA Mapping And Order

This is the canonical bridge note between DEF sections and iEDA/iDB implementation behavior. Keep one copy in `src/database/edadb/docs/`; other iEDA and EDADB documents should link here instead of duplicating the tables.

Related DEF standard material is external to this adapter note. This file records only iEDA-specific mapping, point-tool order evidence, adapter policy, and planned dynamic validation.

## Scope

This note records iEDA-specific conclusions extracted from the native DEF read/write path and point-tool usage. It supplements, but does not replace, the DEF 5.7 grammar. When the DEF spec permits named records to be treated as independent objects, iEDA may still require root-vector order for physical behavior, index consistency, or reproducibility.

The levels below classify iEDA point-tool dependence on direct root-vector order. They do not override DEF grammar dependencies, duplicate/incremental update semantics, or nested route/geometry ordering. For example, `SPECIALNETS` is Level D for iEDA root-vector order, but repeated special-net records and route point streams still keep DEF read order.

## DEF Section To iEDA Mapping

| DEF Section | iEDA Class/List Mapping |
|---|---|
| `VERSION` | `IdbDesign::_version` |
| `DIVIDERCHAR` | No active iDB object in current native DEF writer. |
| `BUSBITCHARS` | `IdbDesign::_bus_bit_chars`, `IdbBusBitChars` |
| `DESIGN` | `IdbDesign::_design_name` |
| `TECHNOLOGY` | No active iDB object in current native DEF flow. |
| `UNITS` | `IdbDesign::_units`, `IdbUnits` |
| `HISTORY` | Not modeled in current iDB DEF flow. |
| `PROPERTYDEFINITIONS` | Not modeled in current iDB DEF flow. |
| `DIEAREA` | `IdbLayout::_die`, `IdbDie`, `IdbCoordinate<int32_t>` |
| `ROWS` | `IdbLayout::_rows`, `IdbRows::_row_list`, `IdbRow` |
| `TRACKS` | `IdbLayout::_track_grid_list`, `IdbTrackGridList::_track_grid_list`, `IdbTrackGrid`, `IdbTrack` |
| `GCELLGRID` | `IdbLayout::_gcell_grid_list`, `IdbGCellGridList::_gcelll_grid_list`, `IdbGCellGrid` |
| `VIAS` | `IdbDesign::_via_list`, `IdbVias::_via_list`, `IdbVia`, `IdbViaMaster` |
| `STYLES` | No active iDB section object. |
| `NONDEFAULTRULES` | Not modeled in current iDB DEF flow. |
| `REGIONS` | `IdbDesign::_region_list`, `IdbRegionList::_region_list`, `IdbRegion` |
| `COMPONENTS` | `IdbDesign::_instance_list`, `IdbInstanceList::_instance_list`, `IdbInstanceList::_instance_map`, `IdbInstance` |
| `PINS` | `IdbDesign::_io_pin_list`, `IdbPins::_pin_list`, `IdbPin`, `IdbTerm`, `IdbPort` |
| `PINPROPERTIES` | Not modeled in current iDB DEF flow. |
| `BLOCKAGES` | `IdbDesign::_blockage_list`, `IdbBlockageList::_blockage_list`, `IdbBlockage` |
| `SLOTS` | `IdbDesign::_slot_list`, `IdbSlotList::_slot_list`, `IdbSlot` |
| `FILLS` | `IdbDesign::_fill_list`, `IdbFillList::_fill_list`, `IdbFill` |
| `SPECIALNETS` | `IdbDesign::_special_net_list`, `IdbSpecialNetList::_net_list`, `IdbSpecialNet`, `IdbSpecialWire` |
| `NETS` | `IdbDesign::_net_list`, `IdbNetList::_net_list`, `IdbNetList::_net_map`, `IdbNet`, `IdbRegularWire` |
| `SCANCHAINS` | Not modeled in current iDB DEF flow. |
| `GROUPS` | `IdbDesign::_group_list`, `IdbGroupList::_group_list`, `IdbGroup` |
| `BEGINEXT` | Not modeled in current iDB DEF flow. |

## Root-Vector Order Levels

Scope: only DEF/design direct child containers are judged here, such as `IdbRows::_row_list`, `IdbInstanceList::_instance_list`, and `IdbNetList::_net_list`. Deeper nested vectors are not expanded in this table; EDADB adapters should conservatively treat nested vectors as order/index-sensitive unless separately proven otherwise.

Criterion: root vector order/index is important only when iEDA point tools consume vector order, derive tool-internal IDs from it, use direct index/front access, or make algorithm decisions from traversal order. Native DEF read/write traversal alone is not proof that the index is semantically important.

Point-tool coverage checked: `iCTS`, `iDRC`, `iECO`, `iFP`, `iNO`, `iPA`, `iPDN`, `iPL`, `iPNP`, `iRT`, `iSTA`, `iTO`. `iIR` hits were found only in deeper pin/port structures, not in the root containers judged below.

| Level | Meaning | Required handling |
|---|---|---|
| A: index/ID consistency | `vector[index]` must match an object ID/index stored or propagated by tools; changing only order is a correctness bug. | Preserve order, or rebuild ID/index fields and every dependent mapping consistently. |
| B: physical semantic order | Order directly changes physical design meaning or generated physical objects. | Preserve original order. |
| C: tool reproducibility order | Name/key identity remains valid, but traversal order changes tool-internal IDs, random sequence consumption, or algorithm state. | Preserve order for reproducible tool behavior. |
| D: deterministic-output order | No point-tool root index/order dependency is found; order affects text/debug output only. | Semantic/normalized diff may sort by stable key; raw text diff may differ. |

Each class takes the highest applicable level: `A > B > C > D`.

## Root Container Evidence

This table uses the same `DEF Section` keys as the mapping table above. `DEF Section(s)` is the grammar-facing anchor; `iEDA Root Container` is the concrete direct iDB container whose order/index behavior is judged.

| DEF Section(s) | iEDA Root Container | Level | Point-Tool Usage / Evidence |
|---|---|---:|---|
| `VERSION`, `BUSBITCHARS`, `DESIGN`, `UNITS` | `IdbDesign` singleton fields | D | No root vector; design name/version/units/bus-bit state is singleton data. |
| `DIEAREA` | `IdbLayout::_die`, `IdbDie` | D | No design-level `vector<IdbDie>`; `DIEAREA` is singleton geometry. |
| `ROWS` | `IdbRows::_row_list` | B | iTO uses first row for site size: `src/operation/iTO/source/data_manager/data_manager.cpp:82`.<br>iCTS uses first row orient: `src/operation/iCTS/source/data_manager/io/CtsDBWrapper.cc:341`.<br>iPDN assigns VDD/VSS follow-pin stripes by row traversal parity: `src/operation/iPDN/source/module/pdn_plan/pdn_plan.cpp:198`. |
| `TRACKS` | `IdbTrackGridList::_track_grid_list` | D | iFP creates grids: `src/operation/iFP/source/module/init_design/init_design.cpp:125`.<br>iRT resets/rebuilds grids: `src/operation/iRT/interface/RTInterface.cpp:1274`.<br>No point-tool root index/front/order-derived ID found. |
| `GCELLGRID` | `IdbGCellGridList::_gcelll_grid_list` | D | iRT resets/rebuilds grids from routing DB: `src/operation/iRT/interface/RTInterface.cpp:1318`.<br>No point-tool root index/front/order-derived ID found. |
| `VIAS` | `IdbVias::_via_list` | D | iPDN resolves by name: `src/operation/iPDN/source/module/pdn_via/pdn_via.cpp:46`.<br>iPNP resolves by name: `src/operation/iPNP/source/module/synthesis/PowerVia.cpp:126`.<br>iRT resolves by name: `src/operation/iRT/interface/RTInterface.cpp:1580`. |
| `COMPONENTS` | `IdbInstanceList::_instance_list`, `_instance_map` | C | iPL wraps instances by traversal: `src/operation/iPL/source/module/wrapper/IDBWrapper.cc:567`.<br>iPL assigns `inst_id` by insertion order: `src/operation/iPL/source/data/Design.hh:132`.<br>Fixed-seed random placement consumes instance order: `src/operation/iPL/source/module/initial_placer/random_placer/RandomPlace.cc:33`. |
| `PINS` | `IdbPins::_pin_list` for IO pins | B | iFP IO placer gets IO pin list: `src/operation/iFP/source/module/io_placer/io_placer.cpp:101`.<br>It assigns physical locations by `pin_list[pin_index++]`: `src/operation/iFP/source/module/io_placer/io_placer.cpp:123`, `src/operation/iFP/source/module/io_placer/io_placer.cpp:198`. |
| `BLOCKAGES` | `IdbBlockageList::_blockage_list` | D | iPDN filters blockages by traversal: `src/operation/iPDN/source/module/pdn_plan/pdn_plan.cpp:289`.<br>iPDN then sorts overlap edges: `src/operation/iPDN/source/module/pdn_plan/pdn_plan.cpp:332`.<br>No point-tool root index/front/order-derived ID found. |
| `REGIONS` | `IdbRegionList::_region_list` | D | iPL wraps regions by traversal: `src/operation/iPL/source/module/wrapper/IDBWrapper.cc:706`.<br>Later usage is name-based through iPL region lookup; no point-tool root index/front/order-derived ID found. |
| `SLOTS` | `IdbSlotList::_slot_list` | D | No `src/operation` root slot-list consumer found. |
| `GROUPS` | `IdbGroupList::_group_list` | D | No direct `IdbGroupList::_group_list` point-tool consumer found.<br>`src/operation/iPL` has its own topology `Group` list, not iDB root groups. |
| `FILLS` | `IdbFillList::_fill_list` | D | No `src/operation` root fill-list consumer found. |
| `SPECIALNETS` | `IdbSpecialNetList::_net_list` | D | iPDN resolves named PDN nets: `src/operation/iPDN/source/module/pdn_plan/pdn_plan.cpp:49`.<br>iPNP resolves VDD/VSS by name: `src/operation/iPNP/source/data_manager/PNPIdbWrapper.cpp:46`.<br>No point-tool root special-net index/front/order-derived ID found. |
| `NETS` | `IdbNetList::_net_list`, `_net_map` | A | `IdbNetList::add_net()` assigns `IdbNet::_id` by insertion order: `src/database/data/design/db_design/IdbNet.cpp:291`.<br>iDRC stores shape `net_idx` from `idb_net->get_id()`: `src/operation/iDRC/interface/DRCInterface.cpp:917`.<br>iDRC reports `idb_net_list[net_idx]`: `src/operation/iDRC/interface/DRCInterface.cpp:1021`. |

## Adapter Policy

Root-level storage model for iEDA-compatible EDADB adapters:

- Use singleton fields for `VERSION`, `DIVIDERCHAR`, `BUSBITCHARS`, `DESIGN`, `TECHNOLOGY`, `UNITS`, and `DIEAREA`.
- Preserve root order for `ROWS`, `COMPONENTS`, `PINS`, and `NETS` when targeting iEDA-compatible behavior; keep name/key maps only as lookup indexes.
- Use keyed or deterministic storage for root vectors without point-tool order evidence, such as `TRACKS`, `GCELLGRID`, `VIAS`, `REGIONS`, `BLOCKAGES`, `SLOTS`, `FILLS`, `GROUPS`, and `SPECIALNETS`, but only after preserving any DEF dependency, duplicate/update, or round-trip requirements.
- Use ordered parse streams for `HISTORY`, `BEGINEXT`, repeated incremental updates, and nested route/geometry streams.
- Keep a dependency-aware emitter even when the in-memory model stores some root children unordered.

Condensed iEDA root-vector conclusions:

- Level A: `IdbNetList::_net_list`. Preserve order or rebuild `IdbNet::_id` and all dependent mappings consistently; reordering only the vector is a correctness bug.
- Level B: `IdbRows::_row_list`, `IdbPins::_pin_list` for IO pins. Preserve order because it changes physical row/pin behavior.
- Level C: `IdbInstanceList::_instance_list`. Preserve order for reproducible placer behavior; name remains identity.
- Level D: `IdbTrackGridList::_track_grid_list`, `IdbGCellGridList::_gcelll_grid_list`, `IdbVias::_via_list`, `IdbBlockageList::_blockage_list`, `IdbRegionList::_region_list`, `IdbSlotList::_slot_list`, `IdbGroupList::_group_list`, `IdbFillList::_fill_list`, `IdbSpecialNetList::_net_list`, plus singleton `IdbDesign`/`IdbDie`. Root order is not proven point-tool semantic; deterministic output or normalized comparison is enough after DEF dependency and duplicate/update rules are respected.

Reverse impact of the preserved root vectors:

- `IdbRows::_row_list`: reordering can make iTO/iCTS pick a different first row for site width/height/orient; iPDN changes VDD/VSS follow-pin stripe parity; iFP tapcell region indices change; iPL row IDs change.
- `IdbInstanceList::_instance_list`: reordering before iPL wrapping changes assigned `inst_id`s and fixed-seed random initial locations. Name-based lookup still works, so this is reproducibility/algorithm-state sensitive rather than object-identity corruption.
- `IdbPins::_pin_list`: reordering before iFP IO placement changes which pins go to left/right/bottom/top slots and coordinates. This directly changes pin placement.
- `IdbNetList::_net_list`: reordering only the vector after IDs are assigned can make DRC report wrong net names because `net_idx` comes from `IdbNet::_id` while the report indexes `idb_net_list[net_idx]`. If the list is rebuilt in a new order, IDs and all dependent mappings must be rebuilt consistently.

iEDA-aware normalized-diff rule:

- Raw DEF text diff remains the strongest round-trip test.
- If raw diff fails only because Level-D root-section element order changed, run normalized semantic diff.
- Normalized diff may sort Level-D root records by stable key: track axis/start/layer set, gcell axis/start/step, via name, region name, blockage signature, slot/fill geometry signature, group name, or special-net name. Do this only for records that are not duplicate/update streams.
- Normalized diff must not reorder Level A/B/C root lists and must not reorder deeper nested route/geometry vectors.

## Current Adapter Order Status

This table is the single root-order implementation status for the current `sort-abc-no-sort-d` adapter. Identity and order remain separate; vector index is never used as a root primary key.

| Root object/container | Level | Current handling |
|---|---:|---|
| `IdbDesign`, `IdbDie` | D | Singleton objects; no root-vector order. Die polygon point order is nested order and uses `_vec_idx`. |
| `IdbRows::_row_list` | B | `Shadow<IdbRow>::_order_sd` plus ordered read preserves append order. |
| `IdbInstanceList::_instance_list` | C | `Shadow<IdbInstance>::_order_sd` plus ordered read preserves append order. |
| `IdbPins::_pin_list` | B | `Shadow<IdbPin>::_order_sd` plus ordered read preserves append order. |
| `IdbNetList::_net_list` | A | `Shadow<IdbNet>::_order_sd` plus ordered read preserves list/ID consistency. |
| `IdbSlotList::_slot_list` | D exception | Anonymous roots retain synthetic identity and `_order_sd` for stable raw DEF emission. Nested rectangles use `_vec_idx`. |
| `TRACKS`, `GCELLGRID`, `VIAS`, `BLOCKAGES`, `REGIONS`, `GROUPS`, `FILLS`, `SPECIALNETS` roots | D | No semantic root order is stored. Raw root-order-only differences may use normalized diff. Natural or synthetic identity remains independent of order. |

All deeper nested vectors remain order-sensitive unless a class document proves otherwise. Their owner association and order are implemented by EDADB child-vector metadata or explicit `_vec_idx`/`_order_sd` fields described in the enabled adapter documents. On `demo/20260814`, that range is `idb-adapter/01_idb_design.md` through `14_idb_special_net.md`; Net uses the original DEF path.

## Planned Dynamic Order Tests

These tests are planned evidence for the static point-tool analysis above. This section is their canonical planning location; executable tests and commands belong in `../test/README.md` only after implementation.

- SQLite unordered-read stress: run EDADB read paths with `PRAGMA reverse_unordered_selects=ON` to expose any adapter query that depends on implicit SQLite row order. SQLite documents that `SELECT` without `ORDER BY` has undefined row order, so every order-sensitive EDADB read must use explicit `ORDER BY`.
- `PINS` / iFP physical-order test: swap two top-level IO pin records in a real sky130 DEF, run `auto_place_pins`, and compare pin coordinates. Expected result: order changes physical pin placement because iFP consumes `pin_list[pin_index++]`.
- `ROWS` / iPDN physical-order test: swap two top-level row records, run the PDN path that assigns follow-pin stripes by row traversal parity, and compare VDD/VSS stripe assignment.
- `COMPONENTS` / iPL reproducibility test: swap two component records before placement and compare placer IDs, logs, and final coordinates under the same fixed seed.
- `NETS` / iDRC ID-consistency test: use a targeted adapter/unit harness to verify `IdbNet::_id` remains consistent with `IdbNetList::_net_list`; reordering the vector without rebuilding IDs should be caught because DRC reports use `idb_net_list[net_idx]`.
