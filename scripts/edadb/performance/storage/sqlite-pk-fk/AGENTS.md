# Frozen PK/FK experiment

This directory is a user-approved, frozen experiment. Do not edit, rename,
delete, regenerate, or replace its code, scripts, build configuration,
documentation, or recorded conclusions without explicit user approval.

The frozen experiment compares SQLite and EDADB APIs using 10,000 Component
parents and 100,000 Pin children, with child composite PK, non-unique composite
index, and no child index. The full rerun passed 88 correctness cases,
275 timing records in 55 groups, and 55 file-database audits.

Keep the readme as the entry point, docs/test_plan.md as the experiment
definition, docs/implementation.md as the implementation and timing reference,
and docs/results.md as the authoritative results and evidence index.
Do not silently replace results or mix samples from different batches.

Put follow-up experiments in sibling directories. In particular, rowid versus
WITHOUT ROWID belongs to ../sqlite-index/, not this frozen experiment.
Changes to shared dependencies that affect reproduction require user approval.

Generated datasets, databases, executables, logs, and raw timing files stay
outside Git. Keep reproducible commands, configuration, source references,
and measured conclusions in this directory.
