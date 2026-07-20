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
- `aux_optional`: generated from `iPL_result.def`, adding non-empty `BLOCKAGES`, `REGIONS`, `SLOTS`, a two-member `GROUPS` entry, and fallback `FILLS` / `SPECIALNETS` / `NETS` fields.
- `routed_irt`: sky130_gcd `iRT_result.def`, verifying routed `NETS` survive the original DEF fallback path while GCellGrid remains EDADB-backed.
- `net_branches`: generated from `iRT_result.def`, verifying legal regular-wire states survive the original DEF fallback path.

For each case the script runs:

1. direct iDB `DEF -> DEF`;
2. `DEF -> EDADB`;
3. `EDADB -> DEF`;
4. byte diff of direct output vs EDADB output.

For `default_ipl`, it also checks:

- design/version/units/bus-bit fields;
- object-family counts for Design, Die, Row, TrackGrid, GCellGrid, Via, Instance, and Pin;
- die point rows, row fields, track-grid fields and primitive vector layer names;
- via generate fields, instance fields, pin fields and pin port/layer/rect child rows;
- write/read logs for instance and pin restoration counts.
- absence of `iFillSD` / `iSpecNetSD` / `iNetSD` tables and adapter logs.

For `aux_optional`, it also checks SQLite content for key EDADB tables and fields:

- `iBlockageSD`, `iRegion`, `iSlotSD`, `iGroupSD`;
- blockage fields, region/slot rectangles, group region and ordered member child rows;
- fallback `FILLS` / `SPECIALNETS` / `NETS` fields through direct-vs-EDADB DEF equivalence.

For `routed_irt`, it checks GCellGrid EDADB content, confirms Net/SpecialNet tables are absent, and uses DEF equivalence to validate routed-net fallback.

For `net_branches`, direct-vs-EDADB DEF equivalence validates `FIXED`, `COVER`, `NOSHIELD`, and `VIRTUAL` syntax through the fallback parser/writer.

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
| Blockage / Region / Slot / Group | direct or shadow | routing vs placement, region type, ordered group members |
| Fill / SpecialNet / Net | DEF fallback | no EDADB tables; original callbacks restore all three sections |

## Planned Order-Stress Tests

Do not implement or run these yet; they are parked here for later test-design review.

- Add a SQLite unordered-read stress mode using `PRAGMA reverse_unordered_selects=ON`; any EDADB read path that needs deterministic vector order must use explicit `ORDER BY`.
- Add real DEF perturbation cases for root-order evidence:
  - swap top-level `PINS` and run iFP `auto_place_pins`;
  - swap top-level `ROWS` and run the iPDN follow-pin stripe path;
  - swap top-level `COMPONENTS` and run iPL with the same seed;
  - check `NETS` ID/list consistency with a targeted adapter/unit harness.
