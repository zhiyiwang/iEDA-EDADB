# EDADB Adapter Test Index

测试方法、数据集、命令、断言和结果统一记录在 [adapter-testing.md](../docs/adapter-testing.md)。本目录只保存可执行测试源码与 fixture。

## Entrypoints

- `run_idb_roundtrip_regression.sh`：对象、schema、字段、顺序与 DEF roundtrip 回归。
- `normalize_def_for_diff.py`：只规范化允许重排的 DEF root records。
- `test_normalize_def_for_diff.sh`：normalizer 单元测试。
- `stage_validation/run_stage_validation.sh`：native 与 EDADB-restored iDB 的点工具阶段验证。
- `stage_validation/run_generated_via_fixture.sh`：generated-via 因果 fixture。
- `stage_validation/run_verilog_alias_fixture.sh`：原生 Verilog alias 缺陷 fixture。

运行测试、选择并发度或解释结果前，先阅读唯一测试文档，不在本 README 维护第二份命令或状态。
