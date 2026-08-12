# Handoff: Shadow Scalar/Vector Traversal Refactor

> Canonical EDADB-core implementation handoff:
> `src/database/edadb/core/md/shadow_scalar_vector_traversal_refactor_handoff.md`.
> This integration-side document records the iEDA adapter diagnosis and migration boundary; the
> core document is the source of truth for the refactor steps and test gates.

Paired deliverable checkpoint:

- iEDA adapter: `milestone/iedadb-adapter-deliverable-checkpoint-20260812`
- EDADB core: `milestone/iedadb-core-deliverable-checkpoint-20260812`

## Status

- The generated-via duplication defect is fixed in the adapter by commit `e63ebd001`.
- The current code comments and `idb-adapter/06_idb_via.md` document the confirmed two-phase call chain.
- The EDADB core traversal protocol has not been refactored yet.
- This document records the problem, reasoning, agreed direction, and remaining design decisions before core changes begin.

## Triggering Defect

Native `DefRead::parse_via()` creates a new generated via master, expands `ROWCOL` once, and calls `set_via_shape()` once. EDADB restored the same logical `IdbViaMaster` target twice because `TABLE4SHADOW_WVEC` is traversed in two phases:

1. Scalar phase: a temporary shadow receives parent-row columns and calls `fromShadow(target)`.
2. Vector phase: a second temporary shadow calls `toShadow(target)`, receives child-table rows, and calls `fromShadow(target)` again.

The two temporary shadows are different C++ objects, but both operate on the same target. The parent row is not queried twice; the second SQL work reads normalized child tables.

Relevant call sites:

- Phase split: `src/database/edadb/core/include/edadb/DbObjectTraverser.h:50` and `DbObjectTraverser.h:60`.
- Scalar shadow allocation: `src/database/edadb/core/include/edadb/DbObjectTraverser.h:156`.
- Vector shadow allocation: `src/database/edadb/core/include/edadb/DbObjectTraverser.h:396`.
- First full restore: `src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h:165`.
- Second-phase full extraction: `src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h:196`.
- Child query: `src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h:230`.
- Second full restore: `src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h:201`.

Generated-via restoration used append-only `add_cut_rect()` and `set_via_shape()` behavior. If the source cut count was `N`, two full restores produced `2N` source cuts and then `3N` derived cut shapes. The focused fixture exposed `19` native cuts versus `57` EDADB-restored cuts, producing `27` versus `65` total iRT obstacles.

## Current Adapter Fix

`Shadow<IdbViaMasterGenerate>::fromShadow()` deletes and clears the old owned cut rectangles before rebuilding them. `Shadow<IdbViaMaster>::fromShadow()` clears bottom/cut/top derived shapes before calling `set_via_shape()`.

Both operations are necessary:

- Clearing only source cuts leaves old derived shapes to be appended again.
- Clearing only derived shapes leaves the source cut list duplicated.

This makes the current Via restoration idempotent and fixes every consumer of the shared iDB state, not only iRT. It is nevertheless a class-local mitigation for a broader traversal/API mismatch.

## First-Principles Diagnosis

A normalized object store naturally has two projections:

```text
logical object T
├── scalar/inline projection stored in the parent row
└── vector projection stored in child tables
```

EDADB already traverses those projections separately. The current shadow API instead converts the complete logical object:

```cpp
bool toShadow(T* obj, const uint32_t* idx_ptr = nullptr);
bool fromShadow(T* obj, uint32_t* idx_ptr = nullptr);
```

The mismatch is that EDADB invokes a complete-object conversion while only one projection is available. During SELECT, it uses the target object as temporary cross-phase storage by applying scalar state, extracting the complete object into a second shadow, adding child vectors, and applying the complete object again.

This behavior is safe only when complete conversion is naturally replacement-based. Existing EDADB core shadow tests use whole-object assignment and therefore did not cover append-only owned vectors or derived geometry.

Inline objects do not have this exact problem. Their scalar fields are fetched directly, while child rows are built in a temporary vector and committed by replacement only after the complete child query succeeds.

## Proposed Shadow Contract

For `TABLE4SHADOW_WVEC`, align conversion granularity with traversal granularity:

```cpp
bool toShadowScalars(T* obj, const uint32_t* idx_ptr = nullptr);
bool toShadowVectors(T* obj);
bool fromShadowScalars(T* obj, uint32_t* idx_ptr = nullptr);
bool fromShadowVectors(T* obj);
```

Intended responsibilities:

| Interface | Operation | Responsibility |
| --- | --- | --- |
| `toShadowScalars()` | INSERT scalar phase | Fill parent-row scalar/inline storage only. |
| `toShadowVectors()` | INSERT vector phase | Fill child-table vector storage only. |
| `fromShadowScalars()` | SELECT scalar phase | Restore scalar-owned state only. |
| `fromShadowVectors()` | SELECT vector phase | Replace complete vectors and rebuild state that depends on them. |

The current `DbObjectTraverser` scalar-then-vector structure can remain. Operation-specific hooks call only the matching shadow conversion instead of complete `toShadow()/fromShadow()` twice.

## Proposed Operation Flows

INSERT:

```text
toShadowScalars(target)
→ bind and insert parent row
→ toShadowVectors(target)
→ insert child rows
```

SELECT:

```text
read parent-row columns into scalar shadow
→ fromShadowScalars(target)
→ read every child table into temporary complete vectors
→ validate all rows, indices, and conversions
→ fromShadowVectors(target) once
```

The SELECT vector phase should not call complete `toShadow(target)`. Child foreign keys are bound from the existing table-node `parents` tuple by `DbForeignKeyBinder`, not from a newly copied nested shadow PK.

For INSERT, vector extraction is still required: `toShadowVectors()` must expose the actual child storage view. A PK-only function cannot replace it.

## Confirmed Decisions

- SELECT must support replacing an already non-empty target object.
- Vector restoration is batch/phase based, not row based.
- EDADB must read all child rows into temporary vectors first.
- `fromShadowVectors()` runs once only after every child query and conversion succeeds.
- Empty database vectors must replace old target vectors with empty vectors.
- Vector index/order validation remains in the EDADB child-row reader.
- No new policy or wrapper entity is planned; the capability should be part of the existing shadow/traversal framework.

## Important Granularity Boundary

`fromShadowVectors()` should apply the complete vector projection, not mutate the target once per SQL child row. Per-row target mutation cannot correctly handle an empty result, makes rollback difficult, exposes partially ordered vectors, and cannot safely compute state that depends on multiple vectors.

For the current Via mapping:

- Generated via: scalar restoration rebuilds generated cut and common shapes once; vector restoration is a no-op.
- Fixed via: scalar restoration sets identity/type; vector restoration replaces fixed layer/rect storage, computes the cut bbox, and builds common shapes once.

## Open Design Decisions

### Whole-object failure atomicity

After scalar restoration but before vector commit, a child SQL or conversion failure can leave new scalar state with old vector state. Two contracts remain possible:

1. Phase-level atomicity: vector state is unchanged on failure, but scalar state may already be replaced.
2. Whole-object atomicity: any failure leaves the complete target unchanged.

Whole-object atomicity is safer for active iDB objects but requires preserving staged scalar shadow state until all child work succeeds, or another rollback/staging mechanism. This is the next design decision; do not implement the refactor until it is resolved.

### API compatibility

Decide whether complete `toShadow()/fromShadow()` remain as convenience wrappers or whether `TABLE4SHADOW_WVEC` requires only the four phase-specific methods. Scalar-only shadows should not be forced to implement meaningless vector methods.

### Cross-phase derived state

Define where derived state that depends on both scalar and vector projections is finalized. The recommended default is `fromShadowVectors()`, after scalar state already exists on the target and all vectors are complete.

## Initial Migration Scope

Current iDB mappings declared with `TABLE4SHADOW_WVEC`:

- `IdbDie`
- `IdbTrackGrid`
- `IdbLayerShape`
- `IdbViaMaster`

EDADB core also has `L1_Shad_Vector` test coverage that currently implements complete conversion through whole-object assignment. The migration must add non-idempotent and ownership-sensitive test types instead of relying only on assignment-based shadows.

## Required Tests Before Removing The Local Mitigation

- Count phase-specific shadow calls for INSERT and SELECT.
- Restore generated via with no child rows and verify geometry is built once.
- Restore fixed via with nested layer/rect vectors and verify complete replacement.
- Restore into a non-empty target and verify old owned elements are deleted exactly once.
- Restore an empty database vector over a non-empty target vector.
- Inject duplicate vector indices and child conversion failures; verify no partial vector commit.
- Cover raw-pointer vectors, value vectors, nested shadow vectors, and derived geometry.
- Decide and test the selected whole-object failure-atomicity contract.
- Run EDADB core tests, the complete iDB roundtrip regression, generated-via fixture, and stage-level iRT validation.

## Guardrails

- Do not remove the current Via clear/rebuild mitigation until the new core protocol passes all replacement and failure tests.
- Do not make `toShadow()` mutate the logical target; INSERT conversion remains read-only.
- Do not use vector index as a primary key.
- Do not infer owned/non-owning pointer semantics inside the generic traverser.
- Do not silently apply partial child rows to an active iDB object.
