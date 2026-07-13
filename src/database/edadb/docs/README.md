# EDADB Documentation Index

This directory is the canonical documentation home for the iEDA + EDADB integration.

## Read First

- `EDADB_DEF_READ_WRITE_ONBOARDING.md`: code reading path for EDADB-backed DEF read/write.
- `def-ieda-mapping-and-order.md`: DEF section to iEDA class mapping, root-order levels, and planned order-stress tests.
- `idb-adapter/README.md`: per-class adapter review rules and implementation checklist.

## Per-Class Adapter Notes

The `demo/20260713` branch keeps these EDADB-enabled class notes:

- `01` Design, `02` Die, `03` Row, `04` TrackGrid, `05` GCellGrid, `06` Via
- `07` Instance, `09` Blockage, `10` Region, `11` Slot, `12` Group

Pin, Fill, SpecialNet, and Net use the original DEF parser/writer and have no adapter class document on this demo branch. Each retained file records:

- original `DefWrite` / `DefRead` semantics;
- EDADB schema and primary-key choices;
- root/nested vector order policy;
- test coverage and remaining TODOs.

## Document Ownership

- Long-term EDADB adapter documentation lives here.
- `src/database/edadb/idb/` should contain adapter code only.
- Early scratch notes formerly under `src/database/edadb/idb/docs/*.mk` were removed because their useful content is now covered by this directory and `md/ieda_architecture_learning/`.
