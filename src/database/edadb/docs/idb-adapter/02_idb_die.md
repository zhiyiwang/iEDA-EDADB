# IdbDie EDADB Adapter Review

## Scope

`IdbDie` 对应 DEF 的 `DIEAREA` section。

- Write: `DefWrite::write_die()`
- Read: `dieAreaCallback()` / `DefRead::parse_die()`
- EDADB Write: `DefWriteEdadb::writeIdbDie()`
- EDADB Read: `DefReadEdadb::readIdbDie()`

## Original Write Semantics

原始 `DefWrite::write_die()`：

- 从 `IdbLayout` 取得 `IdbDie* die`。
- 输出 `DIEAREA`。
- 遍历 `die->get_points()`，按顺序输出每个 `IdbCoordinate<int32_t>` 的 `x/y`。
- 不直接输出 bounding box、area、polygon cache 等派生状态。

因此 EDADB 需要保存：

- `IdbDie` 的点列表顺序。
- 每个点的 `x/y`。

不需要保存：

- `IdbObject` bounding box。
- `_area`。
- `_polygon`。

这些都可以由点列表重建。

## Original Read Semantics

原始 `DefRead::parse_die()`：

- 从 DEF `defiBox` 中读取 `points`。
- 对每个点调用 `die->add_point(x, y)`。
- 调用 `die->set_bounding_box()`。

`IdbDie::add_point()` 会把点加入 `_points`，同时 append 到 `_polygon`；当点数达到矩形点数时，也会触发一次 `set_bounding_box()`。

## EDADB Schema

当前 schema：

```cpp
TABLE4SHADOW_WVEC(idb::IdbDie);
TABLE4CLASS_WVEC(edadb::Shadow<idb::IdbDie>, "iDieSD", (primary_key), (points_sd));
```

`points_sd` 中的元素使用 `Shadow<IdbCoordinate<int32_t>>` / `iCoordSD` 存储：

- `_vec_idx`
- `_x_sd`
- `_y_sd`

## Field Mapping To Original DEF Flow

以下按 EDADB shadow 域列出它对应的原始 DEF read/write 代码位置。

- Die points: `_points_sd`
  - Write source: `DefWrite::write_die()` 输出 `DIEAREA` 点序列，见 `src/database/manager/builder/def_builder/def_write.cpp:340-360`。
  - Read source: `dieAreaCallback()` / `parse_die()` 按 DEF point 顺序创建 die points，见 `src/database/manager/builder/def_builder/def_read.cpp:691-729`。

- Die bounding box: computed from points
  - Write source: DEF writer 不直接输出 bounding box，只输出 die points，见 `src/database/manager/builder/def_builder/def_write.cpp:340-360`。
  - Read source: 原始 `parse_die()` 在 points 恢复后调用 `die->set_bounding_box()`，见 `src/database/manager/builder/def_builder/def_read.cpp:709-729`。

- Root identity: `primary_key`
  - Write source: 原始 DEF 中 `DIEAREA` 是 singleton section，没有 name identity，见 `src/database/manager/builder/def_builder/def_write.cpp:340-360`。
  - Read source: 原始 `parse_die()` 写入当前 layout singleton `IdbDie`，见 `src/database/manager/builder/def_builder/def_read.cpp:709-729`。

## Child Storage View

`IdbDie` 是 `DIEAREA` root，唯一持久化子节点是 point vector：

- `points_sd`：`vector<IdbCoordinate<int32_t>*>`，使用 `Shadow<IdbCoordinate<int32_t>>`。
- `Shadow<IdbCoordinate<int32_t>>` 保存 `_vec_idx/_x_sd/_y_sd`，其中 `_vec_idx` 是 nested vector order，不是 root identity。

这里不直接使用原始 `IdbCoordinate`，因为原始 coordinate 只有 x/y，没有 child vector index；DEF polygon 点序必须可恢复。

## Why Shadow Is Used

当前不直接存 `IdbDie`，而是使用 `Shadow<IdbDie>`，原因是：

- `DIEAREA` 本质是一个有序点 vector。
- EDADB 需要一个稳定 root primary key，把多个 point child rows 归属到同一个 die。
- `IdbDie` 自身没有天然 DEF name/ID 作为 root PK。
- shadow 给 `IdbDie` 补了 `primary_key`，并定义了只包含 DEF 语义字段的存储视图。

如果后续 EDADB 可以对 root object 的 vector child 自动生成 hidden owner key，并保证 vector 顺序，那么可以考虑不用 `Shadow<IdbDie>`；但当前实现保留 shadow 是合理的。

## EDADB Write Path

当前 `writeIdbDie()`：

- 从 `layout->get_die()` 取 active die。
- 调用 `Shadow<IdbDie>::toShadow(die)`。
- `toShadow()` 只把 `die->get_points()` 赋给 `points_sd`。
- 调用 `edadb::insertObject<Shadow<IdbDie>>()`。

这与原始 `write_die()` 一致：只持久化 DEF 输出所需的点列表。

## EDADB Read Path

当前 `readIdbDie()`：

- 从 `layout->get_die()` 取 active die。
- 先调用 `die->reset()` 清空旧点。
- 从 EDADB 读取一个 `Shadow<IdbDie>`。
- 调用 `fromShadow(die)`。
- `fromShadow()` 按 `points_sd` 顺序把点加入 active die，并调用 `set_bounding_box()`。

这与原始 `parse_die()` 一致：点来自持久化数据，bounding box 由点重建。

## Computed Fields

这些字段不入库：

- bounding box：读回后由 `set_bounding_box()` 根据点列表计算。
- `_area`：`get_area()` lazy 计算。
- `_polygon`：`add_point()` 时同步构建。

## Order / Index

`IdbDie` 是 singleton root object，不存在 `vector<IdbDie>` root list 顺序问题。

`IdbDie` 的 point vector 是 nested member，必须保存顺序：

- DEF `DIEAREA` 输出按 `die->get_points()` 顺序写点。
- polygon/bounding box 重建也依赖点序。
- 当前 `Shadow<IdbCoordinate<int32_t>>` 使用 `_vec_idx` 保存 vector index，这是必要字段。

当前状态：root order 不需要；nested point order 已由 shadow `_vec_idx` 实现。

## Risks / TODO

当前实现总体贴近原始 DEF read/write 语义，但需要注意：

- `Shadow<IdbDie>::toShadow()` 直接引用 active die 的 point pointers，不深拷贝；这是写入期间的 non-owning view，要求 `insertObject()` 在 die 生命周期内同步完成。
- `Shadow<IdbDie>::fromShadow()` 会把 EDADB 读出的 point pointers 转移给 active die，并清空 `points_sd`；这依赖 EDADB read 为 vector child 分配新对象。
- `die->reset()` 会删除旧点，因此必须在 `fromShadow()` 前执行。
- 如果未来 EDADB 支持 root vector child 的隐式 owner key，`Shadow<IdbDie>` 可以再评估是否删除；当前阶段不建议改。
