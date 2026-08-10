# EDADB iDB Roundtrip Regression

This directory keeps repeatable EDADB adapter tests close to the EDADB code.

- `run_idb_roundtrip_regression.sh`: object/field/schema/DEF roundtrip regression.
- `stage_validation/`: native versus EDADB-restored point-tool stage validation.

Run from the repository root:

```bash
bash src/database/edadb/test/run_idb_roundtrip_regression.sh
```

Cases run in separate processes. Automatic scheduling is bounded by both logical CPU count
and current `MemAvailable`. On the current 20-physical-core / 40-logical-CPU / 125-GiB host,
the resulting cap is eight. Override it for a shared/smaller machine, or run only selected
cases:

```bash
EDADB_TEST_JOBS=8 bash src/database/edadb/test/run_idb_roundtrip_regression.sh
EDADB_TEST_JOBS=2 bash src/database/edadb/test/run_idb_roundtrip_regression.sh aux_optional design_fields
```

The automatic memory budget reserves 16 GiB for the host and budgets 8 GiB per active case.
Change `EDADB_TEST_PROCESS_MEMORY_GIB` or `EDADB_TEST_MEMORY_RESERVE_GIB` only after measuring
the selected dataset.

Each case owns a separate output directory, SQLite database, iEDA processes, and log. Fixtures are generated once before parallel execution. Per-case scheduler logs are under `OUT_DIR/case-logs/`.

Default output is written to:

```text
/tmp/iedadb_regression
```

Override paths when needed:

```bash
IEDA_BIN=/path/to/iEDA OUT_DIR=/tmp/my_edadb_run bash src/database/edadb/test/run_idb_roundtrip_regression.sh
```

The script runs independent cases with detailed DEF-diff, SQLite, and selected log assertions. Core cases include:

- `default_ipl`: normal sky130_gcd `iPL_result.def`, using direct iDB `DEF -> DEF` as the baseline.
- `aux_optional`: generated from `iPL_result.def`, adding seven routing/placement `BLOCKAGES` records, `REGIONS`, `SLOTS`, a two-member `GROUPS` entry, repeated same-name layer/via `FILLS`, special-net optional fields, and regular-net optional fields.
- `group_branches`: replaces the Group member list with a regex plus a duplicate exact member, then checks parser expansion/deduplication, primitive-vector order recovery, and EDADB output reparsing.
- `special_net_branches`: adds explicit IO/instance connections, STYLE, SHIELD, two-point Via, and three-point route branches; checks complete DB/read-state restoration and native-writer canonical output.
- `routed_irt`: sky130_gcd `iRT_result.def`, covering non-empty regular NETS routed wires, segments, point rows, and ordered pin refs.
- `net_branches`: generated from `iRT_result.def`, covering legal regular-wire states `FIXED`, `COVER`, and `NOSHIELD`, `SPECIALNETS USE SIGNAL` dispatch into `IdbNet`, multiple Via tokens, and multiple `VIRTUAL` points while retaining the full routed-net fixture.

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
- blockage writer fields plus parser-only slots/fills/spacing/width/soft/density, readback state, ordered rectangles, region/slot rectangles, group region and ordered member child rows;
- fill synthetic root identity, layer/via branch isolation, repeated same-name roots, ordered child rows, and root/child physical-order perturbation;
- special-net `ORIGINAL`, `SOURCE`, and `WEIGHT`;
- regular-net `ORIGINAL`, `SOURCE`, `WEIGHT`, `XTALK`, `FIXEDBUMP`, and `FREQUENCY`.

For `routed_irt`, it also checks SQLite content for routed regular-net tables:

- `iNetSD = 677`;
- `iNetSD__wire_list_sd_iRegWireSD = 677`;
- `iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD = 8997`;
- `iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__point_list_sd_iCoordSD = 14256`.
- routed segment type counters cover via segments, rect segments, virtual-point indices, and ordered Via-reference rows;
- `clk_0` ordered instance-pin refs preserve `_order_sd = 0..18`;
- largest routed segment nets remain `clk_0`, `clk_1`, and `dpath/a_mux/_066_`;
- write/read logs report `net_count=677`.

For `net_branches`, it repeats all `routed_irt` checks and additionally verifies:

- `FIXED`, `COVER`, and `NOSHIELD` enum values in `iRegWireSD`;
- complete parser state for multiple Via tokens and multiple `VIRTUAL` points in one segment;
- physical DB order perturbation for Net roots, pin references, wires, segments, points, Via references, and virtual-point indices;
- native-writer canonicalization that emits only the first Via and its supported point subset;
- raw direct-DEF vs EDADB-DEF equality for the routed and branch fixtures.

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
| Blockage / Region / Slot / Group / Fill | direct or shadow | routing/placement source fields, parser-only blockage state, region type, ordered group members, layer-fill vs via-fill |
| SpecialNet / Net | shadow | ordered pin refs, optional fields, routed wire/segment/point rows |

## Future Test Plans

Future order-stress experiments are maintained only in `../docs/def-ieda-mapping-and-order.md`. Add them here after they become executable cases; this README documents current behavior rather than duplicating planned work.
