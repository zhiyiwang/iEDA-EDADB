# EDADB iDB Roundtrip Regression

This directory keeps repeatable EDADB adapter tests close to the EDADB code.

Run from the repository root:

```bash
bash src/database/edadb/test/run_idb_roundtrip_regression.sh
```

Default output is written to:

```text
/tmp/iedadb_regression
```

Override paths when needed:

```bash
IEDA_BIN=/path/to/iEDA OUT_DIR=/tmp/my_edadb_run bash src/database/edadb/test/run_idb_roundtrip_regression.sh
```

The script currently runs four cases with detailed DEF-diff, SQLite, and selected log assertions:

- `default_ipl`: normal sky130_gcd `iPL_result.def`, using direct iDB `DEF -> DEF` as the baseline.
- `aux_optional`: generated from `iPL_result.def`, adding non-empty `BLOCKAGES`, `REGIONS`, `SLOTS`, a two-member `GROUPS` entry, `FILLS`, special-net optional fields, and regular-net optional fields.
- `routed_irt`: sky130_gcd `iRT_result.def`, covering non-empty regular NETS routed wires, segments, point rows, and ordered pin refs.
- `net_branches`: generated from `iRT_result.def`, covering legal regular-wire states `FIXED`, `COVER`, and `NOSHIELD`, plus a `VIRTUAL` second point while retaining the full routed-net fixture.

For each case the script runs:

1. direct iDB `DEF -> DEF`;
2. `DEF -> EDADB`;
3. `EDADB -> DEF`;
4. byte diff of direct output vs EDADB output.

For `default_ipl`, it also checks:

- design/version/units/bus-bit fields;
- object-family counts for Design, Die, Row, TrackGrid, GCellGrid, Via, Instance, Pin, SpecialNet, and Net;
- die point rows, row fields, track-grid fields and primitive vector layer names;
- via generate fields, instance fields, pin fields and pin port/layer/rect child rows;
- special-net default fields and child rows, regular-net default fields;
- write/read logs for instance and pin restoration counts.

For `aux_optional`, it also checks SQLite content for key EDADB tables and fields:

- `iBlockageSD`, `iRegion`, `iSlotSD`, `iGroupSD`, `iFillSD`;
- blockage fields, region/slot rectangles, group region and ordered member child rows;
- fill layer/via typed rows and child rows;
- special-net `ORIGINAL`, `SOURCE`, and `WEIGHT`;
- regular-net `ORIGINAL`, `SOURCE`, `WEIGHT`, `XTALK`, `FIXEDBUMP`, and `FREQUENCY`.

For `routed_irt`, it also checks SQLite content for routed regular-net tables:

- `iNetSD = 677`;
- `iNetSD__wire_list_sd_iRegWireSD = 677`;
- `iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD = 8997`;
- `iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__point_list_sd_iCoordSD = 14256`.
- routed segment type counters cover via segments, rect segments, virtual second points, and via-name rows;
- `clk_0` ordered instance-pin refs preserve `_order_sd = 0..18`;
- largest routed segment nets remain `clk_0`, `clk_1`, and `dpath/a_mux/_066_`;
- write/read logs report `net_count=677`.

For `net_branches`, it repeats all `routed_irt` checks and additionally verifies:

- `FIXED`, `COVER`, and `NOSHIELD` enum values in `iRegWireSD`;
- one `_is_second_point_virtual_sd` segment;
- raw direct-DEF vs EDADB-DEF equality for all four cases.

Regular `+ SHIELD <name>` is not generated: the current native writer has a `kShield` branch, but the native DEF parser rejects that regular-NETS syntax. This remains an original writer/parser limitation rather than a supported adapter roundtrip case.

## Verification Rule

Each migrated class must be checked against the original `DefWrite`/`DefRead` behavior:

- persist only fields that can affect DEF output or later object rebuild;
- recompute derived fields in the same way as the original parser/writer;
- disable the matching DEF callback when `readIdbXXX()` restores that object from EDADB;
- prefer direct EDADB mapping; use shadow only for synthetic primary keys, reduced DEF views, layer/via/name lookup, ordered child vectors, or variant flattening.

The executable baseline is the current branch's direct iDB `DEF -> DEF` path. It may include small intentional fixes beyond `origin/master`, so master-diff reports must call out that drift separately.

Current class coverage:

| Family | Storage | Main edge covered |
| --- | --- | --- |
| Design / Die / Row / TrackGrid / GCellGrid / Via | direct or minimal shadow | scalar fields, point/vector rows, empty and non-empty GCell |
| Instance / Pin | shadow | master/placement/halo fields, port/layer/rect child rows |
| Blockage / Region / Slot / Group / Fill | direct or shadow | routing vs placement, region type, ordered group members, layer-fill vs via-fill |
| SpecialNet / Net | shadow | ordered pin refs, optional fields, routed wire/segment/point rows |

## Planned Order-Stress Tests

Do not implement or run these yet; they are parked here for later test-design review.

- Add a SQLite unordered-read stress mode using `PRAGMA reverse_unordered_selects=ON`; any EDADB read path that needs deterministic vector order must use explicit `ORDER BY`.
- Add real DEF perturbation cases for root-order evidence:
  - swap top-level `PINS` and run iFP `auto_place_pins`;
  - swap top-level `ROWS` and run the iPDN follow-pin stripe path;
  - swap top-level `COMPONENTS` and run iPL with the same seed;
  - check `NETS` ID/list consistency with a targeted adapter/unit harness.
