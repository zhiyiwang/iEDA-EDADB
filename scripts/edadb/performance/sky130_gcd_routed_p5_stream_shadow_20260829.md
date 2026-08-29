# P5 Root Shadow Streaming Result — 2026-08-29

## Goal

Determine whether materializing complete `Shadow<IdbInstance>`, `Shadow<IdbPin>`,
`Shadow<IdbSpecialNet>` and `Shadow<IdbNet>` vectors causes measurable peak-memory amplification,
then remove it without changing stored data, transaction atomicity or DEF semantics.

## Why The Stress Fixture Exists

The repository's Sky130 GCD routed DEF is only about 0.6 MiB, and process startup/LEF loading already
sets a roughly 267 MiB RSS floor. That input cannot expose a short-lived root-Shadow peak.

`make_routed_stress_fixture.py` therefore starts from the real
`scripts/design/sky130_gcd/result/iRT_result.def`, finds its longest parser-proven NETS record, and
copies that record's complete routing body under 1,000 unique route-only net names. The generator
does not synthesize new nested syntax or reorder geometry. The resulting `/tmp/p5_routed_stress_1000.def`
is 6.1 MiB and adds approximately 139,000 segment-level route records. It is a storage/memory stress
fixture, not a physical QoR design.

```bash
python3 scripts/edadb/performance/make_routed_stress_fixture.py \
  scripts/design/sky130_gcd/result/iRT_result.def \
  /tmp/p5_routed_stress_1000.def 1000
```

## Implementation

Before P5, each affected writer converted every root to a heap Shadow, retained the complete vector,
inserted the vector, and only then deleted every Shadow. A routed Net Shadow owns copied nested
wire/segment/point/via vectors, so temporary memory grew with the complete design.

P5 creates one reusable EDADB insert operator, then repeats:

1. construct one stack Shadow;
2. convert one iDB root with the existing standard `toShadow()`;
3. insert the complete root graph;
4. destroy that Shadow before converting the next root.

The reusable operator preserves prepared-statement reuse. The P4 outer design transaction remains
unchanged, so a failure still rolls back every root family. Stored root/nested order fields are also
unchanged.

Code: `src/database/manager/builder/def_builder/def_write_edadb.cpp` in
`writeIdbInstance()`, `writeIdbPin()`, `writeIdbSpecialNet()` and `writeIdbNet()`.

## Peak RSS Evidence

Each row is a five-run median from sequential Release samples. `/usr/bin/time -f %M` reports KiB.

| Implementation | Native max RSS | EDADB-write max RSS | EDADB excess over native |
| --- | ---: | ---: | ---: |
| P4 full Shadow vectors | 273,528 KiB | 302,184 KiB | 28,656 KiB |
| P5 one-root streaming | 273,608 KiB | 273,356 KiB | -252 KiB (measurement noise) |

P5 reduces the EDADB-write median by `28,828 KiB` (`28.15 MiB`, `9.54%` of process peak) and removes
the complete measured EDADB-specific RSS excess.

## Time And Content Checks

| Implementation | Native process elapsed median | EDADB-write process elapsed median |
| --- | ---: | ---: |
| P4 full Shadow vectors | 10.71 s | 13.63 s |
| P5 one-root streaming | 10.90 s | 13.71 s |

The target stress write changes by `+0.59%`, below the 5% regression gate. The generated SQLite
database is byte-identical before and after P5: `42,295,296` bytes with SHA-256
`dd96004d179cf565296fe1a012b68329eac620d270115a19836067c33e5a5c72`.

The standard iPL-filler five-run warm write median was `430.186 ms`; the earlier P4 run was
`378.269 ms`. Profiling attributes the cross-run difference mainly to SQLite Net execution and final
commit variability, while Instance/Pin/SpecialNet root phases are unchanged or lower. P5 therefore
claims a measured memory improvement, not a write-time speedup.

## Correctness

- Release, Debug and profiling-enabled Release builds passed.
- The 1,000-net stress fixture passed strict native-vs-EDADB DEF comparison.
- All 26 EDADB core tests passed.
- The complete adapter regression passed with eight independent fixture jobs, including routed Net,
  optional SpecialNet, Pin and Instance branches.
- P4 whole-design rollback semantics remain covered by `DbFacadeTransactions`.

## Result

P5 is complete. The measured memory amplification came from retaining all converted root Shadows,
not from EDADB's reusable insert operator. Streaming one root at a time removes that amplification
without changing database bytes, DEF output or atomic failure behavior.
