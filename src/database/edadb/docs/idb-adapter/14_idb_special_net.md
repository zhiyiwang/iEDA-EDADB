# IdbSpecialNet Demo Adapter

## Scope And Demo Policy

This demo keeps the EDADB schema and write path for POWER/GROUND `SPECIALNETS`, but deliberately reads the entire `SPECIALNETS` section with the original DEF parser.

- EDADB write is enabled by `DefWriteEdadb::writeChip2Edadb()`: `src/database/manager/builder/def_builder/def_write_edadb.cpp:73` and `src/database/manager/builder/def_builder/def_write_edadb.cpp:115`.
- `writeIdbSpecialNet()` converts and inserts the special-net list: `src/database/manager/builder/def_builder/def_write_edadb.cpp:656`.
- EDADB read is disabled: `createDbByEdadb()` stops after `readIdbFill()`, `src/database/manager/builder/def_builder/def_read_edadb.cpp:208` and `src/database/manager/builder/def_builder/def_read_edadb.cpp:224`.
- Original `SPECIALNETS` and `NETS` callbacks are registered together in `setDefFallbackCallbacks()`, `src/database/manager/builder/def_builder/def_read_edadb.cpp:862`.

Therefore `iSpecNetSD` validates EDADB serialization only. Final DEF equivalence validates the original DEF read fallback, not `fromShadow()` execution.

## Constraint Check

按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 检查：

- Root container is `IdbSpecialNetList::_net_list`, Level D; root `_order_sd` is not stored.
- `_net_name_sd` is the natural root identity.
- Connections, wires, segments, and points are nested vectors and retain owner-local order in the EDADB write view.
- `SPECIALNETS` SIGNAL/CLOCK records are dispatched by the native parser into `IdbNetList`; this demo keeps that behavior because both SNet and Net callbacks are enabled.

## Schema And Initialization

Schema definitions are at `src/database/edadb/idb/edadb_idb_schema.h:122`:

```cpp
TABLE4CLASS(idb::edadb_adapter::SpecialNetPinRef, "iSpecPinRef", ...);
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSpecialWireSegment>, "iSpecWireSegSD", ...);
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSpecialWire>, "iSpecWireSD", ...);
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbSpecialNet>, "iSpecNetSD", ...);
```

- `SpecialNetPinRef` has no primary key because `_order_sd` is child order, not identity: `src/database/edadb/idb/edadb_idb_init.cpp:31`.
- Only the root shadow is explicitly initialized; EDADB recursively creates nested tables: `src/database/edadb/idb/edadb_idb_init.cpp:90`.
- Segment, Wire, and Net shadows are defined in `src/database/edadb/idb/shadow/shadow_idb_special_net.h:29`, `src/database/edadb/idb/shadow/shadow_idb_special_net.h:205`, and `src/database/edadb/idb/shadow/shadow_idb_special_net.h:294`.

## Original DEF Write Mapping

| Original `DefWrite` order | DEF/iDB source | EDADB write view |
| --- | --- | --- |
| Root name and connection branch | net name; `(* pin)` or explicit IO/instance pins | `_net_name_sd`; pin-string, IO-name, and ordered instance-pin vectors |
| `USE`, optional `SOURCE`, `ORIGINAL`, `WEIGHT` | `IdbSpecialNet` scalar fields | corresponding root shadow fields |
| Wire state and segment loop | ordered `IdbSpecialWireList` | `_wire_list_sd` with `_vec_idx` |
| Segment dispatch via → rect → points | layer/via/width/shape/style/rect/points | `Shadow<IdbSpecialWireSegment>::toShadow()`, `shadow_idb_special_net.h:40` |

`writeIdbSpecialNet()` uses the standard `toShadow()` interface and one batch insert: `def_write_edadb.cpp:678-704`.

## Original DEF Read Fallback

| Original `DefRead` order | Runtime behavior in this demo |
| --- | --- |
| `specialNetCallback()` → `parse_special_net()` | Executed directly by `defrRead()` because SNet callbacks are registered. |
| `USE POWER/GROUND` → `parse_pdn()` | Rebuilds `IdbSpecialNet`, connection references, wires, segments, and computed geometry from DEF text. |
| `USE SIGNAL/CLOCK` → `parse_net()` | Rebuilds the corresponding `IdbNet` through the same native dispatch as master iEDA. |
| `NETS` callbacks → `parse_net()` | Rebuilds regular nets entirely from DEF text; no `iNetSD` table exists. |

The retained `fromShadow()` methods are the standard EDADB shadow interfaces, but this demo does not call them. They remain with the SpecialNet definitions so the write schema is self-contained and can later regain EDADB read without redesigning the storage view.

## Test Coverage

- `default_ipl` and `aux_optional` verify `iSpecNetSD` root and nested write tables.
- `special_net_branches` covers pin-string/explicit-pin branches and via/rect/point segment write views.
- Tests assert `writeIdbSpecialNet` is present, `readIdbSpecialNet` is absent, and `iNetSD` is absent.
- Direct DEF roundtrip output is compared with EDADB-flow output, proving the native SpecialNet/Net fallback remains compatible with all EDADB-restored prerequisite families.

```bash
OUT_DIR=/tmp/iedadb_demo_specialnet \
EDADB_TEST_JOBS=1 \
bash src/database/edadb/test/run_idb_roundtrip_regression.sh special_net_branches
```
