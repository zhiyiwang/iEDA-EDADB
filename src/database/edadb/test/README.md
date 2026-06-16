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

The script currently runs three cases:

- `default_ipl`: normal sky130_gcd `iPL_result.def`, using direct iDB `DEF -> DEF` as the baseline.
- `aux_optional`: generated from `iPL_result.def`, adding non-empty `BLOCKAGES`, `REGIONS`, `SLOTS`, `GROUPS`, `FILLS`, special-net optional fields, and regular-net optional fields.
- `routed_irt`: sky130_gcd `iRT_result.def`, covering non-empty regular NETS routed wires and segments.

For each case the script runs:

1. direct iDB `DEF -> DEF`;
2. `DEF -> EDADB`;
3. `EDADB -> DEF`;
4. byte diff of direct output vs EDADB output.

For `aux_optional`, it also checks SQLite content for key EDADB tables and fields:

- `iBlockageSD`, `iRegion`, `iSlotSD`, `iGroupSD`, `iFillSD`;
- group region/member child rows;
- fill layer/via child rows;
- special-net `ORIGINAL`, `SOURCE`, and `WEIGHT`;
- regular-net `ORIGINAL`, `SOURCE`, `WEIGHT`, `XTALK`, `FIXEDBUMP`, and `FREQUENCY`.

For `routed_irt`, it also checks SQLite content for routed regular-net tables:

- `iNetSD = 677`;
- `iNetSD__wire_list_sd_iRegWireSD = 677`;
- `iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD = 8997`;
- `iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__point_list_sd_iCoordSD = 14256`.
