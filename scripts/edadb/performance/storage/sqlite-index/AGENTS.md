# Frozen SQLite single-table index experiment

This directory is frozen by explicit user approval. Do not edit, rename,
delete, regenerate, or replace its source, scripts, build configuration,
documentation, or recorded conclusions without further explicit user approval.

The experiment uses 1,000,000 eight-field Component records and SQLite C API.
It compares five TEXT-key variants and three integer-key variants, using
memory A and file B-batch configurations. It passed 48 small correctness cases,
80 formal samples in 16 groups (five samples each), and 16 independent
full-data diagnostic checks. It does not test FK, EDADB, or iEDA adapter.

Keep readme.md as the entry point, docs/test_plan.md as the method and schema
reference, and docs/results.md as the authoritative measured results and
analysis. Preserve the distinction between table B-trees and additional
indexes, and between measured time, counters, and inferred causes.

The historical baseline is a related scenario, not an identical implementation.
Do not silently combine its samples with this experiment or replace the
documented comparison boundaries. Do not replace results from one batch with
another without approval.

Put future extensions in sibling directories. Generated records, database
files, binaries, logs, raw samples, and Python caches remain outside Git.
Preserve reproducible commands, tested source references, and conclusions.
Changes to shared dependencies affecting reproduction require user approval.
