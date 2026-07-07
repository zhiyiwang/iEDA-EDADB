# EDADB 集成

这一层回答：本分支的 EDADB 与原有 iDB/DEF 流程是什么关系，以及从 Tcl 到 SQLite ORM 的链路在哪里。

## 相关类及执行过程

### Tcl 入口

位置：`src/interface/tcl/tcl_idb/tcl_register_idb.h`

新增或保留的 EDADB 命令：

| Tcl 命令 | C++ command class | 作用 |
| --- | --- | --- |
| `edadb_read` | `CmdEdadbRead` | 从 EDADB 数据库读 design/DEF 数据。 |
| `edadb_write` | `CmdEdadbWrite` | 将当前 design/DEF 数据写入 EDADB 数据库。 |

实现位置：`src/interface/tcl/tcl_idb/tcl_db_file.cpp`

`CmdEdadbRead::exec()`：

```text
读取 -path 和 -edadb_db_path
  -> dmInst->readDefFromEdadb(edadb_path, path)
```

`CmdEdadbWrite::exec()`：

```text
读取 -name 和 -edadb_db_path
  -> 如果 name 有效且 idbSave 成功，保存普通 DEF
  -> 否则 dmInst->saveDefToEdadb(edadb_path)
```

注意：当前实现中 `CmdEdadbWrite::exec()` 会先尝试 `iplf::tmInst->idbSave(name)`；如果这个分支成功，就提前返回。要验证 EDADB 写入路径时，需要关注传参和执行分支是否真的走到 `dmInst->saveDefToEdadb`。

### DataManager 桥接

位置：`src/platform/data_manager/idm.h`、`src/platform/data_manager/idm_edadb.cpp`

新增接口：

| 方法 | 作用 |
| --- | --- |
| `readDefFromEdadb(const char* edadb_path, const char* path)` | 类似 `readDef`，但通过 EDADB reader 构建 `IdbDefService`。 |
| `saveDefToEdadb(const char* edadb_path)` | 类似 `saveDef`，但调用 EDADB writer。 |

执行过程：

```text
edadb_read
  -> DataManager::readDefFromEdadb
  -> IdbBuilder::buildDefFromEdadb
  -> DefReadEdadb::createDbFromEdadb
  -> IdbDefService / IdbDesign
```

```text
edadb_write
  -> DataManager::saveDefToEdadb
  -> IdbBuilder::saveDefToEdadb
  -> DefWriteEdadb::writeDb2Edadb
  -> SQLite database
```

### IdbBuilder 桥接

位置：`src/database/manager/builder/builder.h`、`src/database/manager/builder/builder.cpp`

新增接口：

| 方法 | 作用 |
| --- | --- |
| `buildDefFromEdadb(edadb_path, path)` | 创建 `IdbDefService`，用 `DefReadEdadb` 从 EDADB 构建 design。 |
| `saveDefToEdadb(edadb_path, type)` | 用 `DefWriteEdadb` 把当前 `_def_service` 写入 EDADB。 |

`buildDefFromEdadb` 读完后和普通 DEF 一样会调用：

```text
buildNet()
buildBus()
log()
```

说明 EDADB 目标不是绕过 iDB，而是替代或补充 DEF parser/writer 的持久化通道，最终仍然回到 iDB 的 `IdbDesign`/`IdbLayout` 对象图。

### EDADB core 与 schema

位置：

- `src/database/edadb/core`
- `src/database/edadb/idb`
- `src/database/manager/builder/def_builder/def_read_edadb.cpp`
- `src/database/manager/builder/def_builder/def_write_edadb.cpp`

角色：

| 模块 | 作用 |
| --- | --- |
| `edadb/core` | SQLite backend、table definition、CRUD/select operator 等 ORM 基础设施。 |
| `edadb/idb/edadb_idb_schema.h` | iDB 对象到 EDADB 表结构的 schema 定义。 |
| `edadb/idb/edadb_idb_shadow.h` | shadow/helper 层，用于在数据库对象与 iDB 对象之间建立映射。 |
| `DefReadEdadb` | 从 EDADB 读表并重建 iDB design。 |
| `DefWriteEdadb` | 遍历 iDB design 并写入 EDADB 表。 |

## EDA 抽象与 iEDA/EDADB 类的对应关系

| EDA 抽象 | iDB 内存对象 | EDADB 方向 |
| --- | --- | --- |
| Design header | `IdbDesign` | design table/schema |
| Die/core/row | `IdbDie`、`IdbCore`、`IdbRow` | layout/floorplan 相关表 |
| Instance | `IdbInstance`、`IdbInstanceList` | instance table |
| Pin / IO | `IdbPin`、`IdbPins` | pin table |
| Net | `IdbNet`、`IdbNetList` | net table + net-pin 关系 |
| Via | `IdbVia`、`IdbVias` | via table |
| Blockage/Region/Slot/Group/Fill | 对应 `Idb*List` | DEF object tables |
| Routing wire | `IdbRegularWire`、`IdbSpecialWire` | wire/segment tables |

## 当前边界和阅读重点

EDADB 集成目前更像一个实验性持久化分支，而不是替代整个 iDB 的新数据库层。读源码时建议特别关注：

1. `DataManager` 仍然是上层统一入口。
2. 点工具仍然消费 iDB 对象，不直接消费 SQLite。
3. EDADB reader/writer 需要把 SQLite 表重新映射回 `IdbDefService`/`IdbDesign`。
4. 如果某些 schema/table 初始化处有 TODO 或调试宏，要把它视为当前实现边界。

推荐追踪链路：

```text
tcl_db_file.cpp
  -> idm_edadb.cpp
  -> builder.cpp
  -> def_read_edadb.cpp / def_write_edadb.cpp
  -> edadb/idb/schema/shadow/helper
  -> edadb/core SQLite backend
```
