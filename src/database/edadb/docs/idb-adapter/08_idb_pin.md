# IdbPin EDADB Adapter Review

## Scope

`IdbPin` 对应 DEF `PINS`，这里只处理 `IdbDesign::get_io_pin_list()` 中的 design IO pins。

- Original write: `DefWrite::write_pin()`，`src/database/manager/builder/def_builder/def_write.cpp:517`
- Original read: `DefRead::parse_pin()`，`src/database/manager/builder/def_builder/def_read.cpp:1573`
- EDADB write: `DefWriteEdadb::writeIdbPin()`，`src/database/manager/builder/def_builder/def_write_edadb.cpp:360`
- EDADB read: `DefReadEdadb::readIdbPin()`，`src/database/manager/builder/def_builder/def_read_edadb.cpp:795`

本文件按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 检查：

- Root container: `IdbPins::_pin_list`。
- Root order: Level B；`_pin_name_sd` 是 identity，`_order_sd` 只保存 append order。
- Nested order: port、layer-shape、rect vectors 都保持父对象内顺序。
- Storage rule: EDADB 只保存 DEF 源字段和分支判别字段；跨层同步值与几何缓存按 `parse_pin()` 重新计算。

## Schema

```cpp
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbLayerShape>, "iLayerShapeSD",
                 (primary_key, _vec_idx, _layer_name_sd, _type_sd),
                 (_rect_list_sd));

TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbPort>, "iPortSD",
                 (primary_key, _vec_idx, _orient_sd,
                  _placement_status_sd, _coordinate_sd),
                 (_layer_shape_list_sd));

TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbTerm>, "iTermSD",
                 (_direction_sd, _type_sd, _has_port_sd, _is_special_net_sd),
                 (_port_list_sd));

TABLE4CLASS(edadb::Shadow<idb::IdbPin>, "iPinSD",
            (_pin_name_sd, _order_sd, _net_name_sd, _io_term_sd,
             _no_port_location_sd, _no_port_orient_sd,
             _no_port_placement_status_sd));
```

代码位置：

- Rect and layer-shape schemas: `src/database/edadb/idb/edadb_idb_schema.h:69`, `src/database/edadb/idb/edadb_idb_schema.h:73`
- Port, Term, Pin schemas: `src/database/edadb/idb/edadb_idb_schema.h:94`, `src/database/edadb/idb/edadb_idb_schema.h:97`, `src/database/edadb/idb/edadb_idb_schema.h:100`
- Pin table registration: `src/database/edadb/idb/edadb_idb_init.cpp:83`
- Pin, Term, Port, LayerShape shadows: `src/database/edadb/idb/shadow/shadow_idb_pin.h:20`, `src/database/edadb/idb/shadow/shadow_idb_term.h:17`, `src/database/edadb/idb/shadow/shadow_idb_port.h:16`, `src/database/edadb/idb/shadow/shadow_idb_layer_shape.h:18`

### Persisted DEF source fields

- Pin root: pin name and net name; root orient belongs to the no-PORT placement branch。
- No-PORT placement: root placement status and location only when `write_pin()` selects its outer `else` branch。
- Term: direction, use/type, `SPECIAL`, and the canonical PORT/no-PORT branch selected by `write_pin()`。
- Port placement: only when `write_pin()` would enter its PORT branch, store orient, placement status and coordinate。
- Geometry: LEF layer name and DEF-relative rects。
- Storage metadata: root `_order_sd`; Port/LayerShape use independent `primary_key` owner identity plus `_vec_idx`; Rect uses `_vec_idx`。

### Not persisted

- `IdbPin::_is_io_pin`: `parse_pin()` always calls `set_as_io()`。
- `IdbTerm::_name`: copied from pin name。
- `IdbTerm::_placement_status`: copied from the first explicit port, or restored from the no-PORT root placement field。
- `IdbTerm::_shape`, `IdbTerm::_is_instance`, `IdbPort::_class`: not read from DEF `PINS` by `parse_pin()`。
- `IdbPin::_average_coordinate`, pin/term/port bounding boxes and `IdbPin::_layer_shape_list`: calculated caches。
- `IdbPin::_net` and `_special_net` pointers: linked later by Net/SpecialNet adapters。
- Root pin special flag and layer count: redundant with Term special and nested geometry。

`Shadow<IdbLayerShape>` is shared with Via and still contains `_type_sd`; Pin restore does not trust it as a DEF source field and explicitly calls `set_type_rect()` because each PINS `LAYER` record is parsed as rect geometry.

## Original DEF Write Mapping

The table follows the brace order of `DefWrite::write_pin()`; `writeIdbPin()` only performs root traversal and batch insertion at `def_write_edadb.cpp:360-400`, while the field mapping is implemented by `toShadow()`.

| Original `DefWrite::write_pin()` brace | DEF output | EDADB `toShadow()` correspondence | Stored source |
| --- | --- | --- | --- |
| function body `{}` at `def_write.cpp:518-586` | obtain `IdbDesign::get_io_pin_list()` and reject null | `writeIdbPin()` performs the same lookup/null check at `def_write_edadb.cpp:361-371` | no column |
| section header at `def_write.cpp:526` | `PINS <count> ;` | root table row count is produced by inserting the complete `IdbPins::_pin_list` | no duplicated count column |
| `for (IdbPin* pin...) {}` at `def_write.cpp:528-579` | one PINS record per root object, in vector order | `writeIdbPin()` passes `pin_idx` to `Shadow<IdbPin>::toShadow()` at `def_write_edadb.cpp:383-394`; Pin shadow stores name/order/net at `shadow_idb_pin.h:32-37` | pin name, net name, root order |
| root record setup at `def_write.cpp:529-534` | `SPECIAL`, `DIRECTION` and `NET` | Term shadow stores direction and Term-side special at `shadow_idb_term.h:29-34`; the Pin-side special relation is restored by the SpecialNet adapter before later DEF output | net, direction, Term special; Pin special pointer remains a cross-table relation |
| `if (use.empty()) {} else {}` at `def_write.cpp:536-540` | optional `+ USE` | Term shadow stores `_type_sd`; `kNone` naturally reproduces the empty branch | use/type enum |
| <code>if (term-&gt;is_port_exist() &#124;&#124; pin-&gt;is_special_net_pin()) {}</code> at `def_write.cpp:542-559` | choose PORT-form output | Pin `toShadow()` computes the identical predicate at `shadow_idb_pin.h:43-48`; Term stores the result as `_has_port_sd` at `shadow_idb_term.h:27-34` and passes it to Port children | canonical writer branch discriminator |
| `for (IdbPort* port...) {}` at `def_write.cpp:543-559` | one `+ PORT` block for each port | Term shadow passes `port_idx` into standard `Port::toShadow()` at `shadow_idb_term.h:37-49` | Port `_vec_idx` and ordered Port children |
| `for (IdbLayerShape*...) {}` at `def_write.cpp:548-558` | one `+ LAYER <name>` line per shape | Port shadow passes `layer_shape_idx` into standard `LayerShape::toShadow()` at `shadow_idb_port.h:75-90` | layer name, owner identity and `_vec_idx` |
| `for (IdbRect*...) {}` at `def_write.cpp:550-552` | all `(ll) (ur)` rectangles | `Shadow<IdbLayerShape>::toShadow()` stores the relative rect vector at `shadow_idb_layer_shape.h:59-79`; EDADB maps each Rect through `Shadow<IdbRect>::_vec_idx` | relative rectangle coordinates and rect order |
| `if (port->is_placed()) {}` at `def_write.cpp:554-556` | optional Port status, coordinate and orient | Port shadow stores these fields only when `_writer_uses_port_branch && port->is_placed()` at `shadow_idb_port.h:63-73` | Port placement source fields; otherwise schema columns remain inactive defaults |
| outer `else {}` at `def_write.cpp:560-576` | legacy pin-level LAYER form | `writer_uses_port_branch == false` is stored as `_has_port_sd == false`; geometry stays in the implicit Port child | no extra flattened geometry |
| nested loops at `def_write.cpp:563-569` | pin-level layer names and rectangles | the same Port → LayerShape → Rect shadow tree is used; Port placement columns remain defaults when writer does not use them | relative geometry only |
| `if (term->is_placed()) {}` at `def_write.cpp:570-572` | pin-level status, location and root orient | Pin shadow stores Term status, Pin location and Pin orient only when `!writer_uses_port_branch && term->is_placed()` at `shadow_idb_pin.h:52-55` | no-PORT placement source fields; otherwise schema columns remain inactive defaults |

### `_has_port_sd` stores the canonical writer branch

- `DefWrite::write_pin()` selects PORT-form output using `term->is_port_exist() || pin->is_special_net_pin()`.
- `toShadow()` stores this result in `_has_port_sd` and propagates it through `setWriterUsesPortBranch()` so Port placement fields use the same branch.
- `fromShadow()` therefore follows the same branch that `DefRead::parse_pin()` would take after reading the original writer output.
- If an in-memory no-PORT pin becomes a special-net pin, the original writer emits PORT form and omits pin-level Term placement. EDADB performs the same canonicalization instead of retaining hidden state that the writer does not serialize.

### Term placement and Port placement are not duplicate values

`IdbTerm` owns `vector<IdbPort*>`, but DEF PINS has two mutually exclusive storage forms:

- Explicit PORT form, `def_write.cpp:542-559`: each Port is the placement source. Every Port can have its own status, coordinate and orient. `DefRead::parse_pin()` restores each Port, then copies only the first placed Port status to Term in the `i == 0` brace at `def_read.cpp:1651-1665`. Term status is therefore a first-Port summary, not a value that must equal every Port.
- Legacy no-PORT form, `def_write.cpp:560-576`: the implicit Port carries only relative layer/rect geometry. Placement comes from Term status plus Pin location/orient. `DefRead::parse_pin()` computes Term average/bbox from the implicit Port geometry, then computes Pin average/bbox from Term geometry plus Pin placement at `def_read.cpp:1681-1734`.

The one-to-many storage follows the same split:

- `Shadow<IdbTerm>::_port_list_sd` is a vector child; EDADB stores one Term view and its ordered Port children. Each Port owns its ordered LayerShape/Rect children.
- Explicit PORT: DB stores each Port's status/coordinate/orient and geometry. Term placement status is not stored independently; `fromShadow()` restores all Ports and copies the first Port status back to Term.
- No-PORT: DB stores Term status in the Pin root placement field, Pin location/orient, and the implicit Port geometry. Port placement columns remain inactive; `fromShadow()` recalculates Term and Pin geometry.

Examples covered by regression:

- `aux_optional/clk`: explicit Port is `PLACED (1000,9990) N`; that Port row is stored, then Term status is rebuilt from the first Port.
- `default_ipl/req_msg[0]`: no explicit PORT; Pin root stores placed status/location while the implicit Port stores the `met5` rect. Term average/bbox and Pin absolute geometry are recomputed.
- `pin_writer/req_msg[0]`: input parser state starts as no-PORT, but the SpecialNet relation makes the original writer emit PORT form. EDADB stores `_has_port_sd == true`, leaves no-PORT placement columns inactive, and reconstructs the same state that parsing the writer output would produce.

## Original DEF Read Mapping

The rows follow the brace structure of `DefRead::parse_pin()`; computed values are rebuilt inside the equivalent EDADB branch rather than loaded as cached columns.

| Original read brace | EDADB read mapping | Source, synchronization, or calculation |
| --- | --- | --- |
| `parse_pin(...) {` at `def_read.cpp:1574` | `readIdbPin()` resets the list, reads ordered roots, calls one `pin_sd.fromShadow(pin)`, then appends on success at `def_read_edadb.cpp:795-836` | Root orchestration |
| root setup brace at `def_read.cpp:1580-1604` | Pin shadow restores name/net/orient and IO flag before creating Term, then copies pin name to Term at `shadow_idb_pin.h:66-79` | `IdbTerm::_name` and IO flag are recomputed/cross-level copied |
| `if (def_pin->hasDirection()) {`, `if (hasUse()) {`, `if (hasSpecial()) {` at `def_read.cpp:1605`, `1609`, `1613` | Term shadow restores only direction/type/special source fields at `shadow_idb_term.h:55-60` | Direct DEF fields |
| `if (def_pin->numPorts() > 0) {` at `def_read.cpp:1617` | canonical writer `_has_port_sd == true`; Term shadow validates and sorts Port shadows by `_vec_idx`, then creates ports at `shadow_idb_term.h:62-77` | Explicit `PORT` branch; independent of SQLite fetch order |
| `for (port...) {` and `for (layer...) {` at `def_read.cpp:1619`, `1624` | Port shadow validates and sorts LayerShape shadows by `_vec_idx`, creates shapes, resolves LEF layers, restores rects and forces rect type at `shadow_idb_port.h:100-117` | Direct port/layer/rect fields; layer pointer is rebuilt by name |
| `if (def_port->hasPlacement()) {` at `def_read.cpp:1634` | Port shadow applies orient/status and calls `set_coordinate()` only when stored status is not `kNone`, after geometry is restored, at `shadow_idb_port.h:119-123` | `set_coordinate()` recomputes port IO average/bbox |
| `if (i == 0) {` at `def_read.cpp:1651` | Term shadow copies the first ordered explicit Port placement status to Term at `shadow_idb_term.h:79-85` | Cross-level synchronization; Term status is not stored independently |
| end of explicit-port brace, `pin->set_port_layer_shape()` at `def_read.cpp:1669` | Pin shadow calls the same method when `term->is_port_exist()` at `shadow_idb_pin.h:81-83` | Rebuild pin absolute geometry |
| `else {` at `def_read.cpp:1671` | `_has_port_sd == false`; the nested Port carries only relative layer/rect geometry; its default orient/status/coordinate are ignored | Legacy no-PORT branch |
| `if (def_pin->hasLayer()) {` and layer loop at `def_read.cpp:1681`, `1687` | Pin shadow traverses restored rects and repeats bbox/min/max/midpoint accumulation at `shadow_idb_pin.h:86-106` | Recalculate Term average and bbox inputs |
| `if (layer_num > 0) {` at `def_read.cpp:1709` | Pin shadow sets Term average position and bbox at `shadow_idb_pin.h:108-112` | Derived Term geometry is not stored |
| `if (def_pin->hasPlacement()) {` at `def_read.cpp:1716` | Stored no-PORT status gates restoration; Pin shadow sets Term status/location, computes `location + term average`, then calls the same Pin geometry method at `shadow_idb_pin.h:116-121` | Direct placement fields plus recomputed average/bbox/absolute shapes |

### Why lines 1731-1734 are not loaded directly

Original `def_read.cpp:1731-1734` performs, in order:

1. `set_location()` from DEF placement coordinates.
2. `set_average_coordinate(location + term average)`; this setter applies orientation transform.
3. `set_bounding_box()`; this rebuilds pin bbox and absolute port shapes.

Therefore EDADB stores only no-PORT placement status/location and relative rect geometry. It must not store and replay the already transformed `_average_coordinate`, otherwise non-R0 pins can be transformed twice.

## Read/Write Paths

- `writeIdbPin()` only obtains the root vector, creates pin shadows, propagates conversion failure, and batch inserts them: `def_write_edadb.cpp:360-408`.
- `readIdbPin()` only owns root query/allocation/append/error handling: `def_read_edadb.cpp:795-841`.
- Nested reconstruction remains in the matching shadow: Pin → Term → Port → LayerShape.
- `createDbByDef()` disables PINS callbacks while this adapter is enabled, so pins are not duplicated by text parsing.

## Primary Key and Order

- `iPinSD`: default PK uses first column `_pin_name_sd`; `_order_sd` is not identity。
- `iPortSD.primary_key`: nested owner identity required to attach layer-shape children; `_vec_idx` separately restores `IdbTerm::_port_list` order。
- `iLayerShapeSD.primary_key`: nested owner identity required to attach Rect children. DEF permits one Port to contain repeated `LAYER <same-name>` records, so `_layer_name_sd` is a lookup reference rather than unique identity; `_vec_idx` separately restores `IdbPort::_layer_shape_list` order。
- `IdbRectSD`: PK disabled in `edadb_idb_init.cpp:27`; `_vec_idx` preserves nested rect order。
- Vector index is never used as PK. Pin roots use `ORDER BY "_order_sd"` at `def_read_edadb.cpp:810-813`; Port/LayerShape shadows sort by `_vec_idx`; Rect order is restored by EDADB's shadow-vector index protocol。

## Tests

`src/database/edadb/test/run_idb_roundtrip_regression.sh` checks:

- default no-PORT pin source fields and root order;
- explicit `PORT` / `SPECIAL` branch, port placement, layer and rect geometry;
- non-R0 no-PORT pin with asymmetric rect geometry, including stored source fields and final DEF roundtrip;
- a no-PORT pin linked by `SPECIALNETS`, verifying that both the original writer and EDADB canonicalize it to stored PORT form and omit no-PORT placement fields;
- three explicit Ports: the first has two same-name `met5` LayerShapes and no placement, proving layer name cannot be the child PK; the second is PLACED and the third is FIXED; no-PORT FIXED/COVER/no-placement branches are also covered;
- synthetic owner keys are negated before EDADB read so SQLite fetch order becomes `Port 2, Port 1, Port 0`; final raw DEF and SQL assertions prove `_vec_idx` restores Port and LayerShape order;
- SQL verifies the two same-name `met5` LayerShapes have two distinct synthetic keys and keep their own Rect children;
- the EDADB-generated DEF is parsed and written again by the original `DefRead/DefWrite` path, proving the generated PINS syntax remains accepted and stable;
- nested child counts and DEF roundtrip;
- removed derived columns are absent from `iPinSD` and child schemas;
- build and complete default/aux/routed regression flows.

## Remaining Coverage

- The non-R0 fixture verifies EDADB source fields and final DEF text; add an in-memory query hook only if direct assertions on rebuilt average coordinate and bbox are required.
- The current PINS `placement_status` grammar accepts FIXED/COVER/PLACED, not UNPLACED; the legal no-placement branch is covered instead. A multi-rect PINS fixture is not applicable to the current `parse_pin()` path because it obtains one `bounds(i)` tuple for each `LAYER` record.
