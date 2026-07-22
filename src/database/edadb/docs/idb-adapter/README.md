# EDADB iDB Adapter Review Process

本目录记录把 iEDA DEF read/write 迁移到 EDADB read/write 时的逐类 review 方法。

## Goal

EDADB adapter 文档的核心目标是：每个 root class 都必须按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 的约束检查，并把检查结论写进对应 `0X_idb_*.md`。

在这个核心目标下，EDADB adapter 不是 dump 完整 C++ 对象，而是贴近 iEDA 原始 DEF 语义：

- 先确认 DEF section 到 iEDA class/root list 的映射，以及 root order 等级 A/B/C/D。
- `writeIdbT()` 对齐 `DefWrite::write_xxx()` 实际输出的 DEF 字段。
- `readIdbT()` 对齐 `DefRead::parse_xxx()` 实际重建的 iDB 状态。
- DB schema 只保存 DEF 语义需要的字段，以及 read 时无法从上下文重新计算的字段。

## Adapter Rules

- 当前分支的 `DefWrite::write_xxx()`、`DefRead::parse_xxx()`、相关 iDB class/setter 和 LEF/DEF parser grammar 是实现依据；旧 adapter、旧文档和字段名只能作为线索，不能作为语义依据。禁止根据“看起来应该如此”补字段或分支。
- Writer 和 parser 必须分别逐 brace 检查，不能假设二者天然对称：writer 决定当前 iDB 状态如何输出，parser 决定哪些值来自 DEF、哪些值被跨层复制、哪些值在读后计算。
- EDADB read 替代原始 parser 时，当前 writer 未输出但 parser 会读取、且会影响 active iDB/点工具语义的 source field 仍需保存；必须标记为 `parser-only`，说明保留理由，并用 DB SQL 和 read-state fixture 验证，不能只靠最终 DEF diff。`IdbInstance` 的 `WEIGHT/REGION` 是当前例子。
- EDADB 表达的是 DEF 语义视图，不一定等于完整 C++ object dump。
- 优先 direct mapping；只有 direct mapping 无法表达 polymorphism、anonymous root identity、non-owning pointer/name-reference rebuild、nested vector owner/order，或 reduced DEF storage view 时才引入 `Shadow<T>`。
- 如果某个成员类型 `T` 已注册 `TABLE4SHADOW(T)` 或 `TABLE4SHADOW_WVEC(T)`，则包含它的 root class 可以继续 direct mapping；EDADB 遍历成员时会自动把 `T` / `T*` 的 store type 改写为 `edadb::Shadow<T>`。
- Shadow 自动转换流程：write 阶段对原始成员指针/对象调用 `toShadow()` 后写 shadow fields；read 阶段先读 shadow fields，再调用 `fromShadow()` 重建原始成员，最后写回 root object。
- 每个 `Shadow<T>` specialization 必须保持 EDADB 模板接口：`bool toShadow(T*, const uint32_t* idx_ptr = nullptr)` 和 `bool fromShadow(T*, uint32_t* idx_ptr = nullptr)`。禁止增加 context 参数、额外 overload 或自定义替代接口。
- Shadow 转换所需的额外上下文用普通 getter/setter、adapter helper 或已初始化的全局 lookup context 提供；transient 控制变量不得进入 schema。`toShadow/fromShadow` 返回 false 时必须向上传播失败。
- 对 `vector<Shadow<T>*>` 这类 shadow-owned child vector，EDADB 不会再次替用户调用 logical `T` 的转换接口；owner shadow 必须把 index 传给 child `toShadow()`，将 `_vec_idx` 映射入表，并在 `fromShadow()` 前按 `_vec_idx` 恢复顺序。
- Nested child 的 synthetic `primary_key` 只负责 owner identity/关联下一层 child；`_vec_idx` 只负责顺序。两者不能合并，尤其不能用 vector index 充当 PK。
- 对 root list，identity 和 order 必须分开：不要用 vector order index 当 PK。
- 如无必要不增加 synthetic `primary_key`：只有在候选 natural key 在当前 table/parent scope 内唯一、稳定且能安全关联 child rows 时，才可直接使用 name/ID。没有天然 identity，或 name 允许重复的 root/nested owner，才增加 synthetic PK。
- 不能仅因为对象含有 `name` 就把它作为 PK。例如一个 Port 或 fixed Via 可以包含多个同名 layer records，而每个 `IdbLayerShape` 又独立 owns Rect children；`_layer_name_sd` 是引用字段，不是 identity，因此 `Shadow<IdbLayerShape>::primary_key` 必须保留。
- Primary key 只用于 root identity 或 nested vector-owner storage view；纯 inline/nested scalar value view 必须关闭 PK。例如 `Shadow<IdbViaMasterGenerate>` 只是 `Shadow<IdbViaMaster>::_master_generate_sd`，不是独立 root/vector owner，因此在 `initPrimKeys()` 中关闭 PK；`Shadow<IdbViaMaster>` owns `fixed_layer_shape_list_sd`，保留 EDADB 默认 PK。
- 只有 iEDA 语义需要保序或明确要求 raw roundtrip 保序的 root list 才增加 `_order_sd`；Level D root list 默认不保存 root order，优先依赖 normalized diff。
- `SLOTS` 是当前已 review 类中的明确例外：它是 Level D，但 root record 没有 name，且 raw roundtrip 需要稳定 anonymous record 输出，因此保留 `primary_key + _order_sd`。
- DB 保存原始 writer 实际输出、原始 parser 能重新读取的 canonical DEF view，以及必要 identity/order/reference；computed/cache fields 不入库，read path 按原始 parser 语义重新计算。
- 字段必须归入四类之一：DEF source、branch discriminator/reference、cross-level copy/synchronization、derived/cache。前两类按需持久化；后两类默认不持久化，必须在 `fromShadow()` 中按 parser 原顺序恢复。
- 原始 parser 中跨 Pin/Term/Port/Layer 等层次的复制、同步和计算必须在 `fromShadow()` 中按相同顺序重做；文档要标明源对象、目标对象和触发分支。
- Storage branch discriminator 必须对应原始 writer 实际输出的 DEF 分支，使 `fromShadow()` 重建结果等价于“原始 writer 输出后再由原始 parser 读入”。原始 iDB 中未被 writer 输出的 hidden parser state 默认规范化掉；只有点工具语义明确需要时才额外持久化，并必须单独说明。
- `toShadow()` 按原始 writer 的条件选择存储视图；`fromShadow()` 按原始 parser 的分支顺序执行 allocation、name lookup、字段设置、跨层同步和派生计算。不得为了减少代码改变 setter 调用顺序或把计算结果改成数据库列。
- 当原始 parser 将连续或重复的 DEF records 按 name/type 聚合为 nested object/vector 时，EDADB 保存 parser 构建后的 iDB storage view。文档必须说明 record-to-group-to-child-vector 映射、保留的顺序和 parser 已丢失的顺序；`fromShadow()` 重建 parser-equivalent object structure，不尝试恢复原始 parser 本身未保留的文本交错顺序。
- 对会触发派生状态更新的 setter，`fromShadow()` 必须保持 parser 的依赖顺序：先恢复 identity/master/reference 和基础状态，最后调用 coordinate/geometry 等触发 bbox、pin、halo、obs 重算的 setter；禁止提前调用后再用数据库列覆盖派生结果。
- Non-owning pointer 不直接持久化：保存稳定 name/ID，read 时从 active LEF/design lookup，并恢复必要 backlink。`IdbInstance` 的 cell master、region 和 route-halo layers 是当前例子。
- Optional inline scalar child 没有独立 identity 时关闭 PK；只有 owns nested rows 的 child storage view 才保留 owner PK。`IdbHalo`、`Shadow<IdbRouteHalo>` 属于前者。
- `writeIdbT()` / `readIdbT()` 尽量只负责 root lookup、query/insert、allocation、append 和错误处理；nested object 的字段转换与重建放在对应 `toShadow()` / `fromShadow()`。
- Root vector 默认使用一次 batch transaction/prepared operation；只有测量证明全量 shadow 临时内存成为瓶颈时才改 streaming batch，不能退化为逐对象 transaction。
- 每个 root 文档必须说明 child storage view：哪些子节点 direct mapping，哪些子节点 shadow，哪些运行时 pointer/cache 不入库以及如何重建。
- 每启用一个 `readIdbXXX/writeIdbXXX`，必须同步 schema/init、DEF callback、测试 SQL 和文档。
- 未被任何 enabled adapter 读写、注册或验证的 schema macro 必须休眠并标 `EDADB_TODO`，不能因为原始类存在就默认建表。

## Test Convergence Rules

- Regression cases are process-isolated and run concurrently. Select jobs from physical
  cores, available RAM, and measured per-case load; this 20-core/125-GiB host defaults to
  `EDADB_TEST_JOBS=8`, with an environment override for shared/smaller machines.
- Use selected cases for local iteration, then run the complete suite before handoff.
  Each case must own its output directory, SQLite DB, and logs; concurrency must not
  remove SQL, raw/normalized DEF diff, or order-perturbation checks.
- Fixture 必须符合当前 LEF/DEF grammar；不可用 parser 无法接受的输入假装覆盖分支。源码中存在但 grammar 不可达的分支要记录为不可达，不得伪造测试结论。
- 每个原始 `if/else`、optional field 和 nested loop 至少要有一个合法输入覆盖；测试数据要能区分分支，不能只依赖默认 sky130 样例。
- 同时验证三层结果：direct `DefRead/DefWrite` baseline、EDADB 表中的 source/identity/order 字段、EDADB 重建后的 DEF。Derived/cache 列应通过 schema absence assertion 证明未被误存。
- EDADB 生成的 DEF 要再经过原始 `DefRead/DefWrite` 解析和输出；这用于证明 adapter 输出仍符合原始 parser/writer 语义，而不只是两个文本偶然相同。
- 对需要保序的 root/nested vector，要主动扰乱无 `ORDER BY` 的数据库读取顺序，再验证 `_order_sd` / `_vec_idx` 恢复结果；禁止把 SQLite 当前返回顺序当成保证。
- Null child、lookup 失败和 `toShadow/fromShadow` 失败必须向上传播；测试或代码 review 至少覆盖失败路径，禁止留下部分构造对象继续 append/insert。
- 对 parser-only source field 和原始 writer 缺失分支，增加合法 optional fixture，并检查 SQLite/read-state；原始 writer 无法输出的字段不能宣称已由最终 DEF diff 覆盖。

## Documentation Convergence Rules

- 每次更新类文档都要重新阅读当前源码，不依据旧文档或记忆复制字段和行号。
- `Original DEF Write Mapping` 与 `Original DEF Read Mapping` 必须分开，并以原始 `DefWrite` / `DefRead` 的实际 `{}`、`if/else`、loop 顺序组织；一行对应一个明确 brace/branch，不使用笼统的“阶段”范围。
- Write 表推荐四列：original writer brace、DEF output、EDADB `write/toShadow` correspondence、stored source。Read 表推荐三列：original parser brace、EDADB `read/fromShadow` correspondence、source/synchronization/calculation。
- 每个 mapping row 同时回答：DEF tag/field 是什么、来自哪个 iDB member、写入哪个 EDADB field，或 read 时如何 lookup/copy/recompute。
- 必须显式记录 writer-only、parser-only、fallback、cross-adapter relation 和 branch discriminator。例如 Pin 的 special pointer 由 SpecialNet adapter 恢复，不在 Pin root 中重复保存。
- 原始 writer/parser 不对称或存在原生输出限制时，单列 `Known Native Writer Differences`；明确哪些状态只能由 SQL/read-state 验证，避免把 iEDA 原生差异误判为 adapter 问题。
- shared shadow 若包含当前 root 不需要的列，必须说明当前 root 是否写入、是否忽略、read 时如何 canonicalize；不能默认把 shared schema 的所有列都当成该 DEF section 的源字段。
- 源码定位必须使用当前分支的文件名和行号；代码修改后重新核对。行范围应对应完整函数或 brace body，而不是人为划分的宽泛阶段。
- Markdown 表格中的源码若含 `|` 或 `||`，必须使用 `&#124;` / `&#124;&#124;`，避免被解析成列分隔符；提交前检查每行列数和 `git diff --check`。
- 文档保持精简：Schema、Persisted/Not Persisted、Write Mapping、Read Mapping、PK/Order、Tests/Risks 各自只表达一次，删除重复的语义概述。
- 测试记录至少包含 direct iDB DEF baseline 与 EDADB DEF、关键 SQLite 字段/child rows，以及新增分支或 derived-field fixture；只做文本 diff 不能证明 DB 字段和重建逻辑正确。

## Per-Class Checklist

对每个 iEDA class `T`，按以下顺序检查：

1. 找到 `def_write.cpp` 中对应的 `write_xxx()`，一个 `T` 可能对应多个 writer。
2. 按 writer 的 brace/branch/loop 顺序列出实际输出字段、字段来源和分支条件，包括嵌套 class/vector。
3. 找到 `def_read.cpp` 中对应的 callback / `parse_xxx()`，一个 `T` 可能对应多个 parser。
4. 按 parser 的 brace/branch/loop 顺序列出 allocation、DEF source、lookup、setter、跨层同步和派生计算。
5. 将每个字段归类为 DEF source、branch/reference、cross-level synchronization 或 derived/cache；只让前两类按需进入 schema。
6. 检查 `def-ieda-mapping-and-order.md` 中对应 DEF section 的 root order 等级，并在类文档中记录该约束。
7. 检查 `edadb_idb_schema.h` 中 `TABLE4CLASS` / `TABLE4SHADOW` 是否覆盖上述字段，并记录宏定义代码位置。
8. 判断是否需要 shadow：优先直接映射；只有 direct mapping 无法表达 PK、vector ownership、引用查找、重建视图时才定义 shadow。
9. 检查每个 specialization 是否严格使用 EDADB 标准 `toShadow/fromShadow` 签名；额外状态只能通过 getter/setter/helper 传入。
10. 审计 natural key 在实际 table/parent scope 内是否唯一；能用稳定 name/ID 时不加 synthetic PK，name 可重复或需要独立挂接 child rows 时才加。再检查 `edadb_idb_init.cpp` 的 PK 设置和表初始化是否一致。
11. 任何 helper/child class table macro 如果没有 enabled adapter 使用，必须休眠；文档要说明为什么不需要建表。
12. 检查 `DefReadEdadb::createDbByDef()` 是否只禁用了已由 EDADB 完整恢复的 DEF callbacks。
13. 逐 brace 核对 write/toShadow 与 read/fromShadow，确认 stored branch discriminator 对应 writer 输出，并能驱动 parser 等价的 setter/计算顺序。
14. 用合法 targeted fixtures 覆盖分支；验证 direct baseline、DB source/absence/order、EDADB roundtrip，并将输出 DEF 再交给原始 reader/writer。
15. 对有序 vector 扰乱数据库无序读取，验证显式 order 恢复；对 conversion/lookup 失败确认错误向上传播。
16. 重新核对文档行号、Markdown table 列数和 `git diff --check`。

## Order / Index Policy

对 iDB root list，例如 `IdbDesign::_region_list` 或 `IdbLayout::_rows`，默认目标是保持原始 `t1, t2, t3` append 顺序。iEDA 原始 parser 通常按 DEF 出现顺序 append，writer 再按 vector 当前顺序输出。

判断原则：

- 不要为了稳定输出对 iDB list 做 name sort；这会改变原始 DEF statement order。
- 先区分使用方式：对象是通过 name lookup 找到，还是通过 vector traversal / index 使用。
- 如果代码使用 `front()`、`operator[]`、row id、按 vector 遍历生成内部 id，则 root list 顺序更重要。
- 如果对象主要通过 name lookup 引用，EDA 语义通常不依赖顺序；Level D 的 raw text order 差异优先交给 normalized diff，除非该类文档明确列为 raw-roundtrip exception。
- 如果 EDADB API/DB backend 不能明确保证 `insertVector()` / `readAll` 顺序稳定，A/B/C root list 必须补显式 `_order`；Level D 不强制。
- 成员 vector child 的顺序由对应 shadow/EDADB vector 机制处理；本表只判断 root list 顺序。

已 review 类的顺序需求：

| Class / Root List | Preserve Order? | Usage Basis | Current State |
| --- | --- | --- | --- |
| `IdbDesign` | No | singleton object | No root list order. |
| `IdbDie` | No for root; yes for points | singleton root; point order is geometry/DEF semantics | Root no order; point order already handled by nested shadow `_vec_idx`. |
| `IdbRowList` | Yes | vector traversal plus `front()` / index-derived row logic | Implemented with `_order_sd` and ordered read. |
| `IdbTrackGridList` | No | Level D; no point-tool root index/order dependency found | `primary_key` identity; no `_order_sd`; nested layer-name vector preserves order. |
| `IdbGCellGridList` | No | Level D; no point-tool root index/order dependency found | Direct no-shadow/no-order mapping; normalized diff handles root order-only differences. |
| `IdbRegionList` | No | Level D; references are name-based and no point-tool root index/order dependency found | Direct no-shadow/no-order mapping; normalized diff handles root order-only differences. |
| `IdbSlotList` | Yes | Level D exception: anonymous `SLOTS` records preserve DEF append order for raw roundtrip | `primary_key` identity plus `_order_sd` ordered read; rect vector uses `Shadow<IdbRect>::_vec_idx`. |
| `IdbBlockageList` | No | Level D; no point-tool root index/order dependency found | Synthetic `primary_key` identity; no `_order_sd`; rect vector uses `Shadow<IdbRect>::_vec_idx`. |
| `IdbGroupList` | No | Level D; references are name-based and no point-tool root index/order dependency found | `_group_name_sd` identity; no `_order_sd`; member vector preserves order. |
| `IdbFillList` | No | Level D; no point-tool root index/order dependency found | `primary_key` identity; no `_order_sd`; rect/coordinate vectors preserve order. |
| `IdbSpecialNetList` | No | Level D; PDN tools resolve nets by name, no root index/order dependency found | `_net_name_sd` identity; no root `_order_sd`; pin refs and wire/segment/point vectors preserve order. |

## Current Progress

| Area | Status | Notes |
| --- | --- | --- |
| Design / Units / BusBitChars | Done | Direct `IdbDesign`; only DEF-visible fields are persisted. |
| Die | Done | Shadow root plus ordered point vector. |
| Row | Done | `Shadow<IdbRow>`; `_name_sd` identity, `_order_sd` root order, site cloned from LEF. |
| TrackGrid | Done | Level D root order; `primary_key` identity, no `_order_sd`, layer names rebuild LEF/routing references. |
| GCellGrid | Done | Direct `IdbGCellGrid`; no shadow, no `_order_sd`, DEF four-field view. |
| Via | Done | Direct root object; member-level via master/layer-shape shadows handle rebuild. |
| Instance | Done | `_name_sd` identity, `_order_sd` root order, master/region/layer references rebuilt by name. |
| Pin | Done | `_pin_name_sd` identity, `_order_sd` root order, port/layer shape relative geometry preserved. |
| Blockage | Done | Level D root order; `primary_key` identity, no `_order_sd`, layer/instance references rebuilt by name. |
| Region | Done | Direct `IdbRegion`; `_name` identity, no `_order_sd`, boundary vector preserved. |
| Slot | Done | `primary_key` identity for anonymous root records, `_order_sd` root order, rectangle vector uses `Shadow<IdbRect>::_vec_idx`. |
| Group | Done | Level D root order; `_group_name_sd` identity, no `_order_sd`, member vector order preserved. |
| Fill | Done | Level D root order; `primary_key` identity, no `_order_sd`, layer/via references rebuilt by name. |
| SpecialNet | Done | Level D root order; `_net_name_sd` identity, no root `_order_sd`, pin-string/explicit pin refs plus via/rect/point segment branches covered. |
| Net | Done | `_net_name_sd` identity, `_order_sd` root order, pin/wire/segment vectors preserved. |

## Output Template

每个类的 review 文档保持这个结构：

- Scope: 当前类覆盖哪些 DEF section / callback / writer。
- Constraint Check: root class/list、A/B/C/D order、identity 和 nested-order 结论。
- EDADB Schema: 当前 DB 保存哪些 class/member，并区分 persisted DEF source 与 not-persisted derived/cache fields。
- Schema / Init: 记录 `TABLE4CLASS` / `TABLE4SHADOW` 宏、`initPrimKeys()`、`EDADB_INIT_TABLE()` 的代码位置；同时说明 PK 是否启用。
- Original DEF Write Mapping: 按原始 writer 的 brace 顺序，写清 DEF output、`toShadow()` 和 stored source。
- Original DEF Read Mapping: 按原始 parser 的 brace 顺序，写清 `fromShadow()`、name lookup、跨层同步和 computed-field rebuild。
- Child Storage View: root 下有哪些子节点、direct/shadow 选择、为什么不用原始类。
- EDADB Read/Write Paths: builder 与 shadow 的责任边界。
- Order / Index: root list 是否需要保持顺序、依据是什么、当前是否已显式实现。
- Tests / Risks: 已覆盖分支、SQLite assertions、derived-field checks 和剩余风险。

## Class Review Index

- `01_idb_design.md`: `IdbDesign`, `IdbUnits`, `IdbBusBitChars` for `VERSION`, `BUSBITCHARS`, `DESIGN`, `UNITS`.
- `02_idb_die.md`: `IdbDie` for `DIEAREA`, including point vector persistence and bounding-box rebuild.
- `03_idb_row.md`: `IdbRow` for `ROW`, including site clone/rebuild, origin, DO/BY, STEP, and bbox recomputation.
- `04_idb_track_grid.md`: `IdbTrackGrid` for `TRACKS`, including track fields, layer-name references, and routing-layer backlink rebuild.
- `05_idb_gcell_grid.md`: `IdbGCellGrid` for `GCELLGRID`, including direct four-field mapping and empty-list adapter semantics.
- `06_idb_via.md`: `IdbVia` for `VIAS`, including direct root storage and via-master/layer-shape shadow rebuild.
- `07_idb_instance.md`: `IdbInstance` for `COMPONENTS`, including component fields, name references, and explicit root order.
- `08_idb_pin.md`: `IdbPin` for `PINS`, including IO term, port/layer shape storage, computed absolute geometry, and explicit root order.
- `09_idb_blockage.md`: `IdbBlockage` for `BLOCKAGES`, including routing/placement polymorphism, rect vector, name references, and Level D root-order policy.
- `10_idb_region.md`: `IdbRegion` for `REGIONS`, including name/type and boundary rectangle vector persistence.
- `11_idb_slot.md`: `IdbSlot` for `SLOTS`, including layer name, rectangle vector, and anonymous root identity.
- `12_idb_group.md`: `IdbGroup` for `GROUPS`, including region/member name references, Level D root-order policy, and member order.
- `13_idb_fill.md`: `IdbFill` for `FILLS`, including layer/via typed storage, geometry vectors, and Level D root-order policy.
- `14_idb_special_net.md`: `IdbSpecialNet` for `SPECIALNETS`, including pin refs, special wires, segments, geometry, and Level D root-order policy.
- `15_idb_net.md`: `IdbNet` for `NETS`, including pin refs, regular wires, segments, geometry, and explicit root order.
- `todo.md`: root list order guarantees that still need implementation or verification.

## Suggested Next Steps

1. Keep each new root adapter aligned with original `DefWrite` / `DefRead` semantics.
2. For each root list, decide whether order needs explicit `_order_sd`; A/B/C preserve order, Level D defaults to normalized diff unless documented as an exception.
3. After each class: update schema/read/write if needed, extend SQL assertions, run demo and regression, then commit.
