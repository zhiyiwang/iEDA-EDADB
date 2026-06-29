# IdbRegion EDADB Adapter Review

## Scope

`IdbRegion` 对应 DEF 的 `REGIONS` section。

- Write: `DefWrite::write_region()`
- Read: `regionCallback()` / `DefRead::parse_region()`
- EDADB Write: `DefWriteEdadb::writeIdbRegion()`
- EDADB Read: `DefReadEdadb::readIdbRegion()`

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
TABLE4SHADOW_WVEC(idb::IdbRegion);
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbRegion>, "iRegion", (_name_sd, _order_sd, _type_sd), (_boundary_list_sd));
```

保存字段覆盖原始 DEF writer/read 需要的 name、type 和 boundary rectangle vector。

## Field Mapping To Original DEF Flow

以下按 EDADB shadow 域列出它对应的原始 DEF read/write 代码位置。

- Region identity / root order: `_name_sd`, `_order_sd`
  - Write source: `DefWrite::write_region()` 按 region list 顺序输出 region name，见 `src/database/manager/builder/def_builder/def_write.cpp:1040-1072`。
  - Read source: `regionCallback()` / `parse_region()` 按 DEF 出现顺序创建 region，见 `src/database/manager/builder/def_builder/def_read.cpp:2075-2115`。

- Region type: `_type_sd`
  - Write source: `write_region()` 输出 region type，见 `src/database/manager/builder/def_builder/def_write.cpp:1040-1072`。
  - Read source: `parse_region()` 读取 region type，见 `src/database/manager/builder/def_builder/def_read.cpp:2093-2115`。

- Boundary rectangles: `_boundary_list_sd`
  - Write source: `write_region()` 输出 region rectangle list，见 `src/database/manager/builder/def_builder/def_write.cpp:1040-1072`。
  - Read source: `parse_region()` 读取 region rects，见 `src/database/manager/builder/def_builder/def_read.cpp:2093-2115`。

## Child Storage View

`IdbRegion` 是 `REGIONS` root，当前持久化子节点是 boundary rectangle vector：

- `_boundary_list_sd`：`vector<IdbRect*>`，使用 direct `IdbRect` child table。
- `IdbRect` 只包含 `_lx/_ly/_hx/_hy` 纯几何标量，不需要 shadow。

不保存 `_instance_list`：它不是 DEF `REGIONS` section 的直接输出字段。`demo` 分支中 `COMPONENTS` / `GROUPS` 仍走原始 DEF fallback，由原始 parser 按 region name 建立引用。

## Why Region Shadow

当前需要 `Shadow<IdbRegion>`：

- `IdbRegion` 的 root identity 是 `_name`，因此 shadow 中用 `_name_sd` 作为 PK。
- `IdbRegionList` 需要恢复 DEF append 顺序，但不能用 vector order index 作为 PK。
- `_order_sd` 单独保存 list order；禁止按 region name 排序代替原始顺序。
- boundary 是 owning vector child，可由 `TABLE4CLASS_WVEC` 直接表达。
- 没有需要通过 name lookup 重建的 non-owning pointer。
- group/instance 到 region 的引用在 `demo` 分支由原始 DEF fallback parser 按 region name 重建。

## EDADB Write Path

当前 `writeIdbRegion()`：

- 从 `design->get_region_list()` 取得 region vector。
- 空列表返回 `kDbSuccess`，避免 EDADB dispatcher 中断整个写流程。
- 非空时按 list 顺序构造 `Shadow<IdbRegion>` vector。
- `toShadow()` 保存 `_name_sd`、`_order_sd`、`_type_sd` 和 `_boundary_list_sd`。
- 使用 `edadb::insertVector<Shadow<IdbRegion>>()` 写入。

这与原始 DEF 输出字段一致；空列表返回值是 adapter 层为 dispatcher 做的语义调整。

## EDADB Read Path

当前 `readIdbRegion()`：

- 通过 `ORDER BY "_order_sd"` 读取 root records，恢复 `IdbRegionList` 原始 append 顺序。
- 循环读取 `Shadow<IdbRegion>`。
- `fromShadow()` 恢复 name/type/boundary rectangles。
- 直接加入 `design->get_region_list()`。
- `createDbByDef()` 不注册 region callback，避免 DEF 文本重复创建 region。

读取顺序早于 `createDbByDef()` 的 fallback parser，因此后续原始 DEF `COMPONENTS` / `GROUPS` callback 可通过 region name 查找并恢复引用。

## Computed Fields

`IdbRegion` 当前没有 read 后计算字段：

- name/type/boundary 全部来自 DEF/EDADB。
- `_instance_list` 不入库；`demo` 分支由原始 DEF fallback parser 按 `_region_name` 反向补回。
- region property 尚未在原始 parser 中实现，不进入 EDADB schema。

## Order / Index

`IdbRegionList` 需要保持原始 append 顺序，且不应该按 name 排序。

依据：

- 原始 `parse_region()` 按 DEF 出现顺序 `add_region()`。
- 原始 `write_region()` 按 `region_list->get_region_list()` 当前顺序输出。
- instance/group 对 region 的语义引用靠 `find_region(name)`，不是靠 list index。
- iPL 等后续流程把 region 作为命名约束集合使用；遍历顺序会影响内部 region id 分配，但 EDA 约束语义不要求按 name 排序。
- 当前 shadow 用 `_name_sd` 作为 root identity，用 `_order_sd` 保存 list order。
- read path 已显式按 `_order_sd` 恢复，不依赖 EDADB/SQLite read-all 物理顺序。

当前状态：已实现。root identity 和 root order 已分离，`_name_sd` 不表达 vector order。

boundary rectangle vector 也应保持原始 DEF 顺序；当前由 `TABLE4CLASS_WVEC` 的 vector child 机制负责。

## Tests

- demo `sky130_gcd` 覆盖空列表路径：`writeIdbRegion insert region_count=0`，`readIdbRegion restored region_count=0`。
- `src/database/edadb/test/run_idb_roundtrip_regression.sh` 的 `aux_optional` case 覆盖非空 region，并检查 `iRegion` count、`_order_sd`、type 和 boundary rectangle。

## Risks / TODO

- `IdbRegion::clear_boundary()` 删除 rect 后没有清空 vector；当前 read path 不调用它，暂不影响 roundtrip。
- 若未来原始 DEF parser 支持 region property，需要同步扩展 schema 和 read/write。
- `_instance_list` 仍由 instance read 阶段反向补回，不随 region root record 入库。
