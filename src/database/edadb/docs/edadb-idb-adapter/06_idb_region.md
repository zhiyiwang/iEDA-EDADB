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
TABLE4CLASS_WVEC(idb::IdbRegion, "iRegion", (_name, _type), (_boudary_list));
```

保存字段覆盖原始 DEF writer/read 需要的 name、type 和 boundary rectangle vector。

## Why No Region Shadow

当前不需要 `Shadow<IdbRegion>`：

- `IdbRegion` 的 root identity 是 `_name`。
- boundary 是 owning vector child，可由 `TABLE4CLASS_WVEC` 直接表达。
- 没有需要通过 name lookup 重建的 non-owning pointer。
- group/instance 到 region 的引用由它们各自的 adapter 保存 region name 后重建。

## EDADB Write Path

当前 `writeIdbRegion()`：

- 从 `design->get_region_list()` 取得 region vector。
- 空列表返回 `kDbSuccess`，避免 EDADB dispatcher 中断整个写流程。
- 非空时使用 `edadb::insertVector<IdbRegion>()` 写入。

这与原始 DEF 输出字段一致；空列表返回值是 adapter 层为 dispatcher 做的语义调整。

## EDADB Read Path

当前 `readIdbRegion()`：

- 循环读取 `IdbRegion`。
- 直接加入 `design->get_region_list()`。
- `createDbByDef()` 不注册 region callback，避免 DEF 文本重复创建 region。

读取顺序在 `readIdbInstance()` / `readIdbGroup()` 之前，因此 instance/group 可通过 region name 查找并恢复引用。

## Computed Fields

`IdbRegion` 当前没有 read 后计算字段：

- name/type/boundary 全部来自 DEF/EDADB。
- `_instance_list` 不入库，由 instance read 阶段按 `_region_name` 反向补回。
- region property 尚未在原始 parser 中实现，不进入 EDADB schema。

## Order / Index

`IdbRegionList` 需要保持原始 append 顺序，且不应该按 name 排序。

依据：

- 原始 `parse_region()` 按 DEF 出现顺序 `add_region()`。
- 原始 `write_region()` 按 `region_list->get_region_list()` 当前顺序输出。
- instance/group 对 region 的语义引用靠 `find_region(name)`，不是靠 list index。
- iPL 等后续流程把 region 作为命名约束集合使用；遍历顺序会影响内部 region id 分配，但 EDA 约束语义不要求按 name 排序。

当前状态：未显式实现 root order；direct mapping 依赖 EDADB `insertVector()` / `readAll` 的读回顺序稳定。如果 EDADB `readAll` 不能保证写入顺序，应增加显式 `_order` 字段，而不是对 region name 排序。

boundary rectangle vector 也应保持原始 DEF 顺序；当前由 `TABLE4CLASS_WVEC` 的 vector child 机制负责。

## Tests

- demo `sky130_gcd` 覆盖空列表路径：`writeIdbRegion insert region_count=0`，`readIdbRegion restored region_count=0`。
- `src/database/edadb/test/run_idb_roundtrip_regression.sh` 的 `aux_optional` case 覆盖非空 region，并检查 `iRegion` count、type、boundary rectangle 和 group-region 引用。

## Risks / TODO

- `IdbRegion::clear_boundary()` 删除 rect 后没有清空 vector；当前 read path 不调用它，暂不影响 roundtrip。
- 若未来原始 DEF parser 支持 region property，需要同步扩展 schema 和 read/write。
- 当前 region root list 没有显式 `_order`；如果后续 DB backend 不保证 insertion order，需补 order 字段。
