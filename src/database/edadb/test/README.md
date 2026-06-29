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

The script currently runs three cases with detailed DEF-diff, SQLite, and selected log assertions:

- `default_ipl`: normal sky130_gcd `iPL_result.def`, using direct iDB `DEF -> DEF` as the baseline.
- `aux_optional`: generated from `iPL_result.def`, adding non-empty `BLOCKAGES`, `REGIONS`, `SLOTS`, a two-member `GROUPS` entry, `FILLS`, special-net optional fields, and regular-net optional fields.
- `routed_irt`: sky130_gcd `iRT_result.def`, covering non-empty regular NETS routed wires, segments, point rows, and ordered pin refs.

For each case the script runs:

1. direct iDB `DEF -> DEF`;
2. `DEF -> EDADB`;
3. `EDADB -> DEF`;
4. byte diff of direct output vs EDADB output.

For `default_ipl`, the `demo` branch checks:

- design/version/units/bus-bit fields;
- object-family counts for Design, Die, Row, TrackGrid, GCellGrid, Region, and Slot;
- die point rows, row fields, track-grid fields and primitive vector layer names;
- write/read logs proving only the demo EDADB groups are enabled.

For `aux_optional`, the `demo` branch checks SQLite content for:

- `iRegion`;
- `iSlotSD`;
- region and slot rectangle fields.

For `routed_irt`, the `demo` branch checks SQLite content for non-empty GCellGrid data:

- `iGCellGrid` count and fields;
- demo EDADB write/read log scope.

All other DEF object families use original DEF fallback in `demo`; their correctness is covered by the direct DEF output vs EDADB output byte diff.

## Verification Rule

Each migrated class must be checked against the original `DefWrite`/`DefRead` behavior:

- persist only fields that can affect DEF output or later object rebuild;
- recompute derived fields in the same way as the original parser/writer;
- disable the matching DEF callback when `readIdbXXX()` restores that object from EDADB;
- prefer direct EDADB mapping; use shadow only for synthetic primary keys, reduced DEF views, layer/via/name lookup, ordered child vectors, or variant flattening.

The executable baseline is the current branch's direct iDB `DEF -> DEF` path. It may include small intentional fixes beyond `origin/master`, so master-diff reports must call out that drift separately.

Current `demo` class coverage:

| Family | Storage | Main edge covered |
| --- | --- | --- |
| Design / Die / Row / TrackGrid / GCellGrid | direct or minimal shadow | scalar fields, point/vector rows, empty and non-empty GCell |
| Region / Slot | shadow | region type/boundaries, slot layer/rect rows, explicit root order |
| Via / Instance / Pin / Blockage / Group / Fill / SpecialNet / Net | DEF fallback | preserved by original DEF parser callbacks and final DEF diff |
