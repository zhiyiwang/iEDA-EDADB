# EDADB Demo Subset Regression

Run from the repository root:

```bash
OUT_DIR=/tmp/iedadb_demo_20260713 \
  bash src/database/edadb/test/run_idb_roundtrip_regression.sh
```

The script compares direct iDB `DEF -> DEF` output with `DEF -> EDADB -> DEF` output.
Raw diff is attempted first. Normalized diff may reorder only EDADB-enabled root records; Pin, Fill, SpecialNet and Net remain order-sensitive DEF fallback sections.

Cases:

- `default_ipl`: Design/Die/Row/TrackGrid/Via/Instance fields and counts.
- `aux_optional`: non-empty Blockage/Region/Slot/Group plus Instance weight/region; it also exercises fallback Pin/Fill/SpecialNet/Net syntax.
- `routed_irt`: non-empty GCellGrid and routed NETS fallback.

Every case asserts that disabled EDADB root/child tables are absent and that disabled read/write adapter logs are absent.

Normalizer unit test:

```bash
bash src/database/edadb/test/test_normalize_def_for_diff.sh
```
