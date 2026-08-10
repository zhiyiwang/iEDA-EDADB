# EDADB Documentation Index

This directory is the canonical documentation home for the iEDA + EDADB integration. Each topic has one owner; other documents link to it instead of copying rules or status.

## Read First

1. `EDADB_DEF_READ_WRITE_ONBOARDING.md`: architecture, call chain, and code-reading order for EDADB-backed DEF read/write.
2. `def-ieda-mapping-and-order.md`: canonical DEF-to-iDB mapping, A/B/C/D root-order policy, current order implementation status, and planned order-stress tests.
3. `idb-adapter/README.md`: canonical adapter implementation, review, test, and documentation rules.
4. `../test/README.md`: current executable roundtrip tests, cases, commands, and assertions.
5. `stage-validation/README.md`: first-principles validation of native versus EDADB-restored iDB through iPL, iCTS/iTO, and iRT.

## Per-Class Adapter Notes

Read `idb-adapter/01_idb_design.md` through `idb-adapter/15_idb_net.md` in DEF write order. Each file records:

- original `DefWrite` / `DefRead` semantics;
- EDADB schema and primary-key choices;
- root/nested vector order policy;
- test coverage and class-specific risks.

Root-order status and future order experiments belong only in `def-ieda-mapping-and-order.md`; they are not duplicated in a separate TODO document.

## Document Ownership

- Long-term EDADB adapter documentation lives here.
- `src/database/edadb/idb/` should contain adapter code only.
- Early scratch notes formerly under `src/database/edadb/idb/docs/*.mk` were removed because their useful content is now covered by this directory and `md/ieda_architecture_learning/`.
- Generated logs, diffs, databases, Python caches, and test outputs do not belong under `docs/` or tracked `test/` source directories.
