# EDADB Documentation Index

This directory is the canonical documentation home for the iEDA + EDADB integration.

## Read First

- `EDADB_DEF_READ_WRITE_ONBOARDING.md`: code reading path for EDADB-backed DEF read/write.
- `def-ieda-mapping-and-order.md`: DEF section to iEDA class mapping, root-order levels, and planned order-stress tests.
- `idb-adapter/README.md`: per-class adapter review rules and implementation checklist.

## Per-Class Adapter Notes

Read `idb-adapter/01_idb_design.md` through `idb-adapter/12_idb_group.md` in DEF write order. In `demo/20260720`, `FILLS`, `SPECIALNETS`, and `NETS` remain on the original DEF parser path.

- original `DefWrite` / `DefRead` semantics;
- EDADB schema and primary-key choices;
- root/nested vector order policy;
- test coverage and remaining TODOs.

## Document Ownership

- Long-term EDADB adapter documentation lives here.
- `src/database/edadb/idb/` should contain adapter code only.
- Early scratch notes formerly under `src/database/edadb/idb/docs/*.mk` were removed because their useful content is now covered by this directory and `md/ieda_architecture_learning/`.
