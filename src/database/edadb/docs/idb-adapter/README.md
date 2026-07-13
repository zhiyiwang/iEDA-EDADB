# EDADB iDB Adapter Demo Scope

本目录记录 `demo/20260713` 中已完成的 DEF object-family adapter。

## Current Scope

EDADB read/write 已启用：

- `01` Design、`02` Die、`03` Row、`04` TrackGrid、`05` GCellGrid、`06` Via
- `07` Instance、`09` Blockage、`10` Region、`11` Slot、`12` Group

原始 DEF parser/writer fallback：Pin、Fill、SpecialNet、Net。它们没有 adapter 文档、schema、shadow、table init 或 `readIdbXXX/writeIdbXXX` 实现。

## Review Rules

每个已启用类都按 `../def-ieda-mapping-and-order.md` 检查：

1. 对齐原始 `DefWrite::write_xxx()` 输出字段。
2. 对齐原始 `DefRead::parse_xxx()` 对象重建过程。
3. 只保存 DEF 语义字段和无法由上下文重建的字段。
4. 非必要不定义 shadow；shadow 只处理多态、匿名 identity、name lookup、精简存储视图或 nested vector owner/order。
5. root identity 与 order 分离；本 demo 继承 `no-sort-abcd` 策略，不保存 root `_order_sd`。
6. nested vector 仍通过 EDADB child index、`_vec_idx` 或 child-local key 保序。
7. schema、init、builder read/write、DEF callback、测试和文档必须同步启用或删除。

## Read/Write Boundary

- `DefWriteEdadb::writeChip2Edadb()` 只写当前 11 个 EDADB families。
- `DefReadEdadb::createDbByEdadb()` 只读当前 11 个 EDADB families。
- `DefReadEdadb::createDbByDef()` 只注册 Pin、Fill、SpecialNet、Net callbacks，并保留 `defrSetAddPathToNet()`。
- normalized diff 只允许当前 EDADB-enabled root records 重排；fallback sections 不归一化。

## Validation

```bash
cd bin
bash /home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/demo/demo.sh 2>&1 | tee run.out
```

```bash
OUT_DIR=/tmp/iedadb_demo_20260713 \
  bash src/database/edadb/test/run_idb_roundtrip_regression.sh
```

回归覆盖 `default_ipl`、`aux_optional`、`routed_irt`，并断言 Pin/Fill/SpecialNet/Net EDADB tables 不存在。
