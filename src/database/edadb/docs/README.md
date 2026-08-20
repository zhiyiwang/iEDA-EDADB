# iEDA + EDADB Adapter Documentation

本目录是 adapter 文档的唯一入口。每个主题只有一个权威正文；README 只概括并索引，不复制规则、命令或结果。

## System Summary

Adapter 在不修改原始 iEDA 点工具数据模型的前提下，把 DEF parser 构建的 iDB storage view 写入 EDADB，并在新进程中结合 LEF/design lookup 重建 parser-equivalent iDB：

```text
DEF + LEF -> native DefRead -> iDB -> DefWriteEdadb -> EDADB
LEF + EDADB -> DefReadEdadb + selective DEF fallback -> rebuilt iDB -> DefWrite / point tools
```

- `def_write_edadb.*` / `def_read_edadb.*`：root object 编排、fallback callback 和失败传播。
- `idb/edadb_idb_schema.h` / `edadb_idb_init.cpp`：storage view、PK 和表初始化。
- `idb/shadow/*`：DEF source fields、variant flattening、reference lookup、nested owner/order 和 parser-equivalent rebuild。
- `test/`：对象级 roundtrip、SQL/order assertions、最小 fixture 和点工具阶段验证。

Adapter 只持久化 DEF source、必要 branch/reference、identity 和 order；cross-level synchronization 与 derived/cache state 按原始 parser setter 顺序重新计算。详细约束只在开发规则和逐类文档中维护。

## Start Here

1. [架构与代码阅读顺序](EDADB_DEF_READ_WRITE_ONBOARDING.md)：理解 DEF、builder、adapter、schema/shadow 与 EDADB core 的调用关系。
2. [开发与审查规则](adapter-development-rules.md)：实现、shadow、PK/order、测试和文档的统一收敛规则。
3. [DEF/iDB 映射与顺序策略](def-ieda-mapping-and-order.md)：DEF section、iDB root container 和 A/B/C/D 顺序结论。
4. [逐类审计索引](idb-adapter/README.md)：按 DEF 顺序阅读 01–14 类的字段、schema、write/read 和风险。
5. [测试与验证](adapter-testing.md)：唯一测试正文，包含数据集、命令、断言、结果、缺陷分类和产物。

## Specialized Records

- [原生 iEDA 已知缺陷](stage-validation/known-native-defects.md)：只记录测试中确认但不在 adapter 分支修改的原生问题。
- [Shadow scalar/vector 重构 handoff](handoff/shadow-scalar-vector-traversal-refactor.md)：adapter 侧问题、设计选择和迁移边界。
- `../core/md/shadow_scalar_vector_traversal_refactor_handoff.md`：EDADB core 仓库中的对应重构契约。

## Current Demo

`demo/20260814` 以 `5fcb67bc7` 为基线，启用 `IdbDesign` 至 `IdbSpecialNet` 共 14 类的 EDADB read/write；普通 `IdbNet` 使用原始 DEF fallback。当前构建、canonical demo 和 15-case 并行对象回归结果统一记录在 [测试与验证](adapter-testing.md)。

## Ownership

- 长期 adapter 文档只放在 `src/database/edadb/docs/`。
- `src/database/edadb/idb/` 只放 adapter 代码、schema、helper 和 shadow。
- `src/database/edadb/test/` 只放可执行测试、配置和 fixture；README 只索引权威测试文档。
- EDADB core 文档属于 `src/database/edadb/core/` 子模块，不在本次 adapter 文档整理范围内。
- 运行日志、diff、SQLite DB、缓存和临时结果写入仓库外目录，例如 `/tmp/`。
