# iEDA + EDADB Adapter Documentation Entry

该文件保留为历史兼容入口，不再维护第二份 adapter 正文。所有长期内容统一位于 [src/database/edadb/docs/README.md](src/database/edadb/docs/README.md)。

## Recommended Reading Order

1. [架构与代码阅读顺序](src/database/edadb/docs/EDADB_DEF_READ_WRITE_ONBOARDING.md)
2. [开发与审查规则](src/database/edadb/docs/adapter-development-rules.md)
3. [DEF/iDB 映射与顺序策略](src/database/edadb/docs/def-ieda-mapping-and-order.md)
4. [逐类 adapter 审计](src/database/edadb/docs/idb-adapter/README.md)
5. [测试方法、命令与结果](src/database/edadb/docs/adapter-testing.md)

当前 `demo/20260814` 启用 Design 至 SpecialNet 共 14 类的 EDADB read/write；普通 Net 使用原始 DEF fallback。具体 scope 和验证结果以以上权威文档为准。
