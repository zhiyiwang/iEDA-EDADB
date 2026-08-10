# EDADB iDB Roundtrip Regression

This directory keeps repeatable EDADB adapter tests close to the EDADB code.

Run from the repository root:

```bash
bash src/database/edadb/test/run_idb_roundtrip_regression.sh
```

Cases run in separate processes. The current host has 20 physical cores / 40 logical CPUs
and 125 GiB RAM, so the conservative default concurrency is eight. Override it for a
shared/smaller machine, or run only selected cases:

```bash
EDADB_TEST_JOBS=8 bash src/database/edadb/test/run_idb_roundtrip_regression.sh
EDADB_TEST_JOBS=2 bash src/database/edadb/test/run_idb_roundtrip_regression.sh aux_optional design_fields
```

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
- `special_net_branches`: checks the complete EDADB SpecialNet write view, while output reconstruction intentionally uses the native `SPECIALNETS` parser.
- `routed_irt`: verifies non-empty routed NETS through the original DEF fallback and asserts `iNetSD` is absent.
- `net_branches`: exercises complex regular-wire syntax and `SPECIALNETS USE SIGNAL` dispatch through the original DEF fallback.

For each case the script runs:

1. direct iDB `DEF -> DEF`;
2. `DEF -> EDADB`;
3. `EDADB -> DEF`;
4. byte diff of direct output vs EDADB output.

For `default_ipl`, it also checks:

- design/version/units/bus-bit fields;
- object-family counts for Design, Die, Row, TrackGrid, GCellGrid, Via, Instance, Pin, and the SpecialNet write table;
- die point rows, row fields, track-grid fields and primitive vector layer names;
- via generate fields, instance fields, pin fields and pin port/layer/rect child rows;
- special-net default fields and child rows;
- demo split: Fill and SpecialNet tables exist, Net table is absent, and SpecialNet/Net read logs are absent;
- write/read logs for instance and pin restoration counts.

For `aux_optional`, it also checks SQLite content for key EDADB tables and fields:

- `iBlockageSD`, `iRegion`, `iSlotSD`, `iGroupSD`, `iFillSD`;
- blockage writer fields plus parser-only slots/fills/spacing/width/soft/density, readback state, ordered rectangles, region/slot rectangles, group region and ordered member child rows;
- fill synthetic root identity, layer/via branch isolation, repeated same-name roots, ordered child rows, and root/child physical-order perturbation;
- special-net `ORIGINAL`, `SOURCE`, and `WEIGHT`;
- SpecialNet write-only optional and nested fields.

For `routed_irt` and `net_branches`, the test asserts `iNetSD` is absent and compares native direct-roundtrip DEF with the mixed EDADB/DEF flow. These cases validate fallback compatibility, not Net persistence.

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
| SpecialNet | write-only shadow | optional fields and ordered nested write rows; native DEF read fallback |
| Net | DEF fallback | no EDADB schema/read/write implementation in this demo |

## Demo/20260810 Validation

- Full build: `cmake --build build -j40 --target iEDA` passed.
- Canonical demo passed with byte-identical input/output DEF.
- Full parallel regression passed all 15 cases:

```bash
OUT_DIR=/tmp/iedadb_demo_20260810_full \
EDADB_TEST_JOBS=8 \
bash src/database/edadb/test/run_idb_roundtrip_regression.sh
```

## Planned Order-Stress Tests

Do not implement or run these yet; they are parked here for later test-design review.

- Add a SQLite unordered-read stress mode using `PRAGMA reverse_unordered_selects=ON`; any EDADB read path that needs deterministic vector order must use explicit `ORDER BY`.
- Add real DEF perturbation cases for root-order evidence:
  - swap top-level `PINS` and run iFP `auto_place_pins`;
  - swap top-level `ROWS` and run the iPDN follow-pin stripe path;
  - swap top-level `COMPONENTS` and run iPL with the same seed;
  - check `NETS` ID/list consistency with a targeted adapter/unit harness.
