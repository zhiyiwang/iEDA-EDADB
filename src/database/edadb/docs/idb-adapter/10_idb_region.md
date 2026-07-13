# IdbRegion EDADB Adapter Review

## Scope

`IdbRegion` 对应 DEF 的 `REGIONS` section。

- Write: `DefWrite::write_region()`
- Read: `regionCallback()` / `DefRead::parse_region()`
- EDADB Write: `DefWriteEdadb::writeIdbRegion()`
- EDADB Read: `DefReadEdadb::readIdbRegion()`

本文件按 `src/database/edadb/docs/def-ieda-mapping-and-order.md` 的约束检查：

- DEF section 映射：`REGIONS` section。
- iEDA root container：`IdbRegionList::_region_list`。
- root-vector order 等级：Level D，当前没有发现点工具依赖 `IdbRegionList::_region_list` 的 root index/order。
- nested vector 约束：boundary rectangle vector 是 region 内部几何列表，必须随 root record 保持原始顺序，不参与 D-level root sort。

## Original Write Semantics

原始 `DefWrite::write_region()` 输出：

- region name: `region->_name`
- boundary rectangles: `region->_boudary_list`
- type: `region->_type`，通过 `IdbRegionProperty` 转成 `FENCE` / `GUIDE`

空 region list 时，原始 writer 返回 `kDbFail`，但 top-level DEF writer 不把它当作整体失败。

## Original Read Semantics

原始 `DefRead::parse_region()`：

- 通过 `region_list->add_region(def_region->name())` 创建或复用 region。
- `hasType()` 时设置 region type。
- 逐个 DEF rectangle 调用 `region->add_boundary(...)`。
- property 当前仍是 TBD，不进入 iDB 状态。

## EDADB Schema

当前 schema：

```cpp
TABLE4CLASS_WVEC(idb::IdbRegion, "iRegion", (_name, _type), (_boudary_list));
```

Schema / init 代码位置：

- `iRegion` direct table macro: `src/database/edadb/idb/edadb_idb_schema.h:89`
- Primary-key setup: `src/database/edadb/idb/edadb_idb_init.cpp:21`
- Table registration: `src/database/edadb/idb/edadb_idb_init.cpp:88`

保存字段覆盖原始 DEF writer/read 需要的 name、type 和 boundary rectangle vector。

Schema 与 order/index 约束的关系：

- 依据 `src/database/edadb/docs/def-ieda-mapping-and-order.md`，`REGIONS` 映射到 `IdbRegionList::_region_list`，等级为 Level D。
- Level D 的含义是当前未发现点工具依赖 root vector index/order；normalized diff 可以按 stable key 排序 `REGIONS` root records。
- 当前 adapter 不保存 `_order_sd`；如果 DB 读回顺序不同，测试应通过 Level-D normalized diff 判断语义一致性。
- `_name` 是 direct table 的 primary key，表达 region identity；它不表达 vector order。

Primary-key audit:

- `initPrimKeys()` 没有关闭 `IdbRegion` 的 primary-key 行为；`iRegion` 使用 table macro 第一列 `_name` 作为 root identity。
- `_boudary_list` 是 owned child vector，使用 `Shadow<IdbRect>::_vec_idx` 保存其 nested order。
- `initReadDb()` / `initWriteDb()` 都先调用 `initPrimKeys()`，再调用 `initAllTables()`，因此 read/write 的 table metadata 一致。

## Original DEF Write/Read Roundtrip Mapping

### Original DEF Write Flow

| 原始 `DefWrite` 执行顺序 | EDADB write 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. `write_region()` 检查 list；空 list 返回失败；输出 section count，见 `def_write.cpp:1042-1053` | `writeIdbRegion()` 检查 list 后直接 `insertVector<IdbRegion>()`；空 vector 返回成功，见 `def_write_edadb.cpp:391-412` | `REGIONS <N>` / `IdbRegionList::_region_list` / `iRegion` row count |
| 2. 按 root vector 遍历并输出 region name，见 `def_write.cpp:1055-1056` | direct mapping 保存 `IdbRegion::_name`；不保存 root order | `- <region_name>` / `IdbRegion::_name` / `iRegion._name` |
| 3. 按 boundary vector 顺序输出所有 rectangle，见 `def_write.cpp:1058-1060` | `TABLE4CLASS_WVEC` 保存 `_boudary_list`；nested `IdbRect` 顺序由 child vector index 保留 | region rectangles / `IdbRegion::_boudary_list` / `iRegion._boudary_list` child rows |
| 4. 将 region enum 转为名称并无条件输出 `TYPE`，见 `def_write.cpp:1062-1063` | direct mapping 保存 `_type` enum | `+ TYPE` / `IdbRegion::_type` / `iRegion._type` |
| 5. 输出 record/section terminator，见 `def_write.cpp:1065-1068` | 由 table row 和 vector 边界重建，不存文本终止符 | `;`, `END REGIONS` / 无 iDB 成员 / 无 EDADB 字段 |

### Original DEF Read Flow

| 原始 `DefRead` 执行顺序 | EDADB read 对应 | DEF 域 / iDB 变量 / EDADB 域 |
| --- | --- | --- |
| 1. `regionCallback()` 校验参数后调用 `parse_region()`，后者按 name `add_region()`，见 `def_read.cpp:2075-2101` | `readIdbRegion()` 创建 `IdbRegion`，由 EDADB direct mapping 恢复 `_name` 后 append 到 list，见 `def_read_edadb.cpp:506-535` | region root/name / `IdbRegionList::_region_list`, `IdbRegion::_name` / `iRegion._name` |
| 2. DEF record 有 `TYPE` 时恢复 type，见 `def_read.cpp:2103-2105` | direct mapping 从 `iRegion._type` 恢复 enum | `+ TYPE` / `IdbRegion::_type` / `iRegion._type` |
| 3. 按 DEF rectangle 顺序调用 `add_boundary()`，见 `def_read.cpp:2107-2109` | EDADB 按 child vector index 恢复 `_boudary_list` | region rectangles / `IdbRegion::_boudary_list` / `iRegion._boudary_list` child rows |
| 4. property 分支仍为 TODO，见 `def_read.cpp:2111-2112` | schema 不保存 region property，与原始 parser 最终 iDB 状态一致 | property / 无已实现 iDB 成员 / 无 EDADB 字段 |

## Child Storage View

`IdbRegion` 是 `REGIONS` root，当前持久化子节点是 boundary rectangle vector：

- `_boudary_list`：`vector<IdbRect*>`，使用 `Shadow<IdbRect>` child table；字段名沿用 iEDA 原始类中的拼写。
- `Shadow<IdbRect>` 保存 `_vec_idx/_lx_sd/_ly_sd/_hx_sd/_hy_sd`，用 `_vec_idx` 恢复 boundary rectangle vector 的原始顺序。

不保存 `_instance_list`：它不是 DEF `REGIONS` section 的直接输出字段，而是由 `COMPONENTS` 中的 region name 在 `readIdbInstance()` 阶段反向补回。

## Why Direct Mapping

当前使用 direct `IdbRegion` mapping：

- `IdbRegion` 的 DEF-visible root identity 是 `_name`，可直接作为 PK，不需要 shadow 另造 `_name_sd`。
- `IdbRegionList` 是 Level D；root order 没有点工具语义依赖，因此不额外保存 `_order_sd`。
- boundary 是 owning vector child，可由 `TABLE4CLASS_WVEC` 直接表达。
- `_instance_list` 不入库；instance/group 到 region 的引用由它们各自的 adapter 保存 region name 后重建。
- 当前没有需要通过 shadow 重建的 non-owning pointer。
- 旧的 dormant `shadow_idb_region.h` 已删除，避免保留未启用 shadow 造成误导。

当前实现与 order 约束没有出入：schema 是 direct no-shadow/no-order，write/read 也都没有 `ORDER BY` 或 `_order_sd`。

## EDADB Write Path

当前 `writeIdbRegion()`：

- Code: `src/database/manager/builder/def_builder/def_write_edadb.cpp:391`
- Region vector access: `src/database/manager/builder/def_builder/def_write_edadb.cpp:404`
- Empty-list return: `src/database/manager/builder/def_builder/def_write_edadb.cpp:408`
- EDADB direct insert: `src/database/manager/builder/def_builder/def_write_edadb.cpp:412`

- 从 `design->get_region_list()` 取得 region vector。
- 空列表返回 `kDbSuccess`，避免 EDADB dispatcher 中断整个写流程。
- 非空时直接 `edadb::insertVector<IdbRegion>(region_vec)` 写入。
- direct table 保存 name、type 和 boundary rectangles。

这与原始 DEF 输出字段一致；空列表返回值是 adapter 层为 dispatcher 做的语义调整。

## EDADB Read Path

当前 `readIdbRegion()`：

- Code: `src/database/manager/builder/def_builder/def_read_edadb.cpp:506`
- EDADB read op: `src/database/manager/builder/def_builder/def_read_edadb.cpp:519`
- EDADB read loop: `src/database/manager/builder/def_builder/def_read_edadb.cpp:522`
- Add to active list: `src/database/manager/builder/def_builder/def_read_edadb.cpp:535`

- 使用 `makeReadAllOp<IdbRegion>()` 循环读取 direct rows。
- 读出的 `IdbRegion` 已包含 name/type/boundary rectangles。
- 直接加入 `design->get_region_list()`。
- `createDbByDef()` 不注册 region callback，避免 DEF 文本重复创建 region。

读取顺序在 `readIdbInstance()` / `readIdbGroup()` 之前，因此 instance/group 可通过 region name 查找并恢复引用。

## Computed Fields

`IdbRegion` 当前没有 read 后计算字段：

- name/type/boundary 全部来自 DEF/EDADB。
- `_instance_list` 不入库，由 instance read 阶段按 `_region_name` 反向补回。
- region property 尚未在原始 parser 中实现，不进入 EDADB schema。

## Order / Index

`IdbRegionList` 在 iEDA 点工具语义上是 Level D；当前 adapter 不保存 root order。

依据：

- 原始 `parse_region()` 按 DEF 出现顺序 `add_region()`。
- 原始 `write_region()` 按 `region_list->get_region_list()` 当前顺序输出，因此 raw text roundtrip 可能受 DB 读回顺序影响。
- instance/group 对 region 的语义引用靠 `find_region(name)`，不是靠 list index。
- `def-ieda-mapping-and-order.md` 中记录：iPL wrapping 会遍历 region，但后续语义主要通过 region name lookup；未发现 root index/front/order-derived ID 依赖。
- `IDBWrapper::wrapRegions()` 按 `IdbRegionList` 遍历并创建 iPL `Region`，见 `src/operation/iPL/source/module/wrapper/IDBWrapper.cc:700-733`。
- iPL `Design::add_region()` 会按插入顺序分配 `_region_id`，见 `src/operation/iPL/source/data/Design.hh:156-161`；但当前代码中只发现 `get_region_id()` 定义，未发现算法消费该 ID。
- legalizer/detail placer 后续通过 region name map 查找实例所属 region，见 `src/operation/iPL/source/module/detail_placer/DetailPlacer.cc:205-210`、`src/operation/iPL/source/module/detail_placer/database/DPLayout.cc:58-75`。
- `PlacerDB` / `MapFiller` / `NesterovPlace` 对 region list 的遍历用于插入 boundary geometry 或累积 blockage area，见 `src/operation/iPL/source/PlacerDB.cc:477-484`、`src/operation/iPL/source/module/filler/src/MapFiller.cpp:35-53`、`src/operation/iPL/source/module/global_placer/electrostatic_placer/NesterovPlace.cc:558-564`；这些使用没有 root index/front/order-derived ID。
- 因为 root order 没有点工具语义依赖，当前不引入 `_order_sd`；如果 raw diff 只因 Level-D root order 变化失败，应使用 normalized diff。

当前状态：已实现 direct no-shadow/no-order mapping。

对 normalized diff 的影响：

- `REGIONS` 是 Level D root list；如果 raw diff 只因为不同 `REGIONS` root record 顺序失败，normalized diff 可以按 region name 排序后通过。
- 排序单位必须是完整 region record；record 内部 boundary rectangle vector 不排序。
- 如果 name/type/boundary rectangle 内容不同，normalized diff 必须失败。

boundary rectangle vector 也应保持原始 DEF 顺序；当前由 `Shadow<IdbRect>::_vec_idx` 负责，不依赖 SQLite child-row 返回顺序。

## Tests

- demo `sky130_gcd` 覆盖空列表路径：`writeIdbRegion insert region_count=0`，`readIdbRegion restored region_count=0`。
- `src/database/edadb/test/run_idb_roundtrip_regression.sh` 的 `aux_optional` case 覆盖非空 region，并检查 `iRegion` count、name/type、boundary rectangle 和 group-region 引用。

## Risks / TODO

- `IdbRegion::clear_boundary()` 删除 rect 后没有清空 vector；当前 read path 不调用它，暂不影响 roundtrip。
- 原始 writer 对空 region list 返回失败，EDADB writer 对空 vector 返回成功。
- `readIdbRegion()` 不清空现有 list，依赖“新 design、单次恢复”的调用前提。
- 若未来原始 DEF parser 支持 region property，需要同步扩展 schema 和 read/write。
- `_instance_list` 仍由 instance read 阶段反向补回，不随 region root record 入库。
- direct no-order mapping 可能导致 raw DEF 中多个 region 的输出顺序变化；这是 Level D 场景，应由 normalized diff 处理。
