# Sky130 Generated-Via Fixture

## Purpose

This fixture isolates one semantic requirement: a generated DEF via must produce the same derived enclosure and cut geometry after native DEF parsing and after EDADB restoration. The comparison is made at the iRT input boundary because iRT materializes those shapes as routing obstacles before routing begins.

The records are reduced from `scripts/design/sky130_gcd/result/iPL_lg_result.def`. This is not a synthetic via formula detached from the real flow; it uses the same four sky130 generated-via definitions that exposed the full-design failure.

## Minimal Physical State

- `DIEAREA` gives iRT a finite physical domain.
- One `ROW` establishes a non-empty iDB core. An entirely row-free DEF parses correctly but violates an iRT initialization precondition and is therefore not a valid stage fixture.
- `TRACKS` supplies routing axes for `met1` through `met5`.
- Four `VIAS` cover cut arrays of `2x5`, `1x4`, `1x4`, and `1x1`.
- One `SPECIALNETS` record uses each via exactly once. iRT then exposes each via's derived geometry in `env_shape.obs`.
- Components, pins, and regular nets are empty so they cannot contribute obstacle geometry.

## First-Principles Oracle

Each generated via contributes two routing-layer enclosure rectangles. The four vias therefore contribute eight enclosure shapes. Their cut arrays contribute `10 + 4 + 4 + 1 = 19` cut shapes. A correct native or EDADB snapshot must contain exactly 27 obstacles with the same rectangle multiset.

The pre-fix adapter defect produced:

- native: 19 cuts + 8 enclosures = 27 obstacles;
- EDADB: 57 cuts + 8 enclosures = 65 obstacles;
- difference: every cut rectangle has multiplicity three instead of one, adding 38 shapes; enclosure geometry is unchanged.

This exact `27 -> 65` signature proved that the two-phase EDADB `fromShadow()` lifecycle appended generated cuts more than once. It was not a textual DEF ordering difference. The fixed implementation clears owned source and derived rectangles before reconstruction, so both phases now produce the same `27`-shape state.

## Run

Strict regression mode expects semantic equivalence and is the default acceptance test:

```bash
bash src/database/edadb/test/stage_validation/run_generated_via_fixture.sh
```

Historical diagnostic mode succeeds only when the exact pre-fix defect is reproduced:

```bash
EXPECT_KNOWN_DEFECT=1 \
bash src/database/edadb/test/stage_validation/run_generated_via_fixture.sh
```

The native snapshot and EDADB write are independent. They run concurrently only when at least four logical CPUs and 48 GiB of `MemAvailable` are present; otherwise they run serially. `FIXTURE_RUN_JOBS=1` forces serial execution and `FIXTURE_RUN_JOBS=2` forces concurrency. EDADB read always starts after the database writer succeeds. A process trap terminates child iEDA processes if the harness is interrupted.

Outputs are stored under a timestamped `/tmp/iedadb_generated_via_fixture/` directory by default and can be redirected with `OUT_ROOT`. Native, writer, and EDADB-read directories each contain a manifest with the source revision, dirty state, input/config hashes, command, exit status, host resources, and selected concurrency.
