---
title: iEDA-EDADB Knowledge Base
aliases:
  - iEDA-EDADB 文档入口
tags:
  - iEDA
  - EDADB
  - index
---

# iEDA-EDADB Knowledge Base

本目录保存个人架构学习、EDADB 研究和环境记录；iEDA 官方文档仍保留在源码原有目录中。

## Start Here

1. [iEDA 架构学习地图](ieda_architecture_learning/00_learning_map.md)
2. [iEDA 代码与数据流主线](ieda_architecture_learning/10_complete_tutorial.md)
3. [点工具如何读取和回写 iDB](ieda_architecture_learning/04_eda_tools.md)
4. [sky130 多进程物理设计流程](ieda_architecture_learning/05_scripts_flow.md)
5. [EDADB 集成边界](ieda_architecture_learning/06_edadb_integration.md)

## Topic Index

| 主题 | 文档 | 用途 |
| --- | --- | --- |
| Runtime | [入口与运行时](ieda_architecture_learning/01_entry_runtime.md) | 从 `main()` 追到 Tcl shell。 |
| iDB | [iDB 数据模型](ieda_architecture_learning/02_data_model_idb.md) | 区分 `IdbLayout`、`IdbDesign` 和 builder/service。 |
| Dispatch | [Tcl/Python 调度](ieda_architecture_learning/03_interface_and_dispatch.md) | 从命令名追到 C++ API。 |
| Tools | [EDA 点工具](ieda_architecture_learning/04_eda_tools.md) | 判断工具是直接使用 iDB、复制后回写，还是只读分析。 |
| Flow | [示例脚本 Flow](ieda_architecture_learning/05_scripts_flow.md) | 理解阶段间 DEF checkpoint。 |
| EDADB | [EDADB 集成](ieda_architecture_learning/06_edadb_integration.md) | 理解 EDADB 如何恢复 iDB，而不是替代点工具。 |
| Adapter | [EDADB Adapter Docs](../src/database/edadb/docs/README.md) | 与代码同步的 DEF/iDB adapter 文档。 |
| Research | [Research Index](paper/README.md) | EDA、database 和交叉研究路线。 |

## Documentation Rules

- 架构说明以当前源码为准，不根据模块名推断行为。
- 源码引用使用相对链接和 GitHub 行号，例如 `[main()](../src/apps/ieda_main.cpp#L35)`。
- 行号对应 `edadb-idb-dev/sort-abc-no-sort-d @ 77fbe5c67`；代码变化后需要重新核对。
- 区分两种生命周期：单个 iEDA 进程内共享 active iDB；完整 sky130 flow 的不同阶段通过 DEF/Verilog 文件传递状态。
- 每份子文档只回答一个问题；总览文档只保留调用主线，不重复算法细节。

## GitHub Sparse Checkout

只下载个人文档和 EDADB 文档：

```bash
git clone --filter=blob:none --no-checkout git@github.com:<user>/<repo>.git
cd <repo>
git sparse-checkout init --cone
git sparse-checkout set md src/database/edadb/docs
git checkout edadb-idb-dev/sort-abc-no-sort-d
```

后续使用 `git pull` 同步。
