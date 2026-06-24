# iEDA + EDADB Code Reading Guide

这个 README 的目标是告诉你如何按顺序阅读当前 `edadb-idb` 分支里和 EDADB
相关的代码，而不是一次性解释所有实现细节。

当前分支把 EDADB 作为 iEDA 的一个 DEF 数据持久化后端接入进来。主流程是：

```text
DEF -> iDB -> EDADB SQLite database -> iDB -> DEF
```

建议阅读时始终带着两个问题：

1. 这个文件是原始 iEDA 代码、iEDA+EDADB wrapper，还是 EDADB core？
2. 这个代码处在写入 EDADB、读回 EDADB，还是测试/验证链路上？

## 0. 先确认当前版本

当前关键版本：

- iEDA branch: `edadb-idb`
- iEDA current commit: `HEAD` (`fix: adapt idb adapter to latest edadb vector api`)
- official iEDA base commit: `0074352412f6a4a8c88c13739946cdf5004f25c0`
- EDADB core submodule: `src/database/edadb/core @ 3077132`

可用下面命令确认：

```bash
git branch --show-current
git log --oneline -5
git submodule status --recursive
```

如果你想看当前分支相对官方 iEDA 改了哪些文件：

```bash
git diff --name-status 007435241..HEAD
```

如果你想看 EDADB core 相对当前 C baseline 的变化：

```bash
git -C src/database/edadb/core diff --name-status 8a4e3bf..HEAD
```

## 1. 先看目录分层

先把代码分成三类，这样后面读起来不会乱。

### 原始 iEDA 代码被修改的部分

这些文件原本属于 iEDA，为了接入 EDADB 或修复 DEF 字段保真被修改：

- Build/CMake:
  - `CMakeLists.txt`
  - `src/apps/CMakeLists.txt`
  - `src/database/CMakeLists.txt`
  - `src/database/manager/builder/CMakeLists.txt`
  - `src/database/manager/builder/def_builder/CMakeLists.txt`
  - `src/platform/data_manager/CMakeLists.txt`
- Tcl/DataManager/Builder:
  - `src/interface/tcl/tcl_idb/tcl_db_file.*`
  - `src/interface/tcl/tcl_idb/tcl_register_idb.h`
  - `src/interface/tcl/tcl_definition.h`
  - `src/platform/data_manager/idm.*`
  - `src/database/manager/builder/builder.*`
- DEF normal reader/writer:
  - `src/database/manager/builder/def_builder/def_read.*`
  - `src/database/manager/builder/def_builder/def_write.*`
- iDB data structures:
  - `src/database/data/design/**`
  - `src/database/basic/geometry/**`

### iEDA + EDADB wrapper/adapter

这些是本分支新增或主要为 EDADB 接入服务的代码：

- `src/database/edadb/idb/*`
- `src/database/edadb/idb/shadow/*`
- `src/database/manager/builder/def_builder/def_read_edadb.*`
- `src/database/manager/builder/def_builder/def_write_edadb.*`
- `src/platform/data_manager/idm_edadb.cpp`
- `scripts/edadb/demo/*`
- `src/database/edadb/test/*`

### EDADB core

EDADB 自己的 ORM / SQLite / table-op 实现在子模块：

- `src/database/edadb/core/include/edadb/*`
- `src/database/edadb/core/src/*`
- `src/database/edadb/core/test/*`
- `src/database/edadb/core/demo/*`

阅读 iEDA 接入逻辑时，先不要深入 core。先读 wrapper，等你看到
`edadb::insertObject<T>()`、`edadb::readAll<T>()`、`TABLE4CLASS*` 这些 API 时，再回到
core 里查它们的机制。

## 2. 从可运行入口开始读

先运行或阅读这个 demo：

```bash
cd bin
bash /home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/demo/demo.sh
```

入口脚本：

- `scripts/edadb/demo/demo.sh`

它做三件事：

1. 设置 sky130_gcd 的环境变量。
2. 调用 `def2edadb.tcl` 做 `DEF -> iDB -> EDADB`。
3. 调用 `edadb2def.tcl` 做 `EDADB -> iDB -> DEF`。

然后比较输入 DEF 和输出 DEF。

接着读两个 Tcl：

- `scripts/edadb/demo/tcl/def2edadb.tcl`
- `scripts/edadb/demo/tcl/edadb2def.tcl`

重点看这两个命令：

```tcl
edadb_write -edadb_db_path $db_path
edadb_read -edadb_db_path $db_path -path $::env(INPUT_DEF)
```

这两个 Tcl 命令是进入 C++ 代码的门。

## 3. 读 Tcl 命令注册和执行

从注册开始：

1. `src/interface/tcl/tcl_idb/tcl_register_idb.h`
   - 看 `registerTclCmd(CmdEdadbRead, "edadb_read")`
   - 看 `registerTclCmd(CmdEdadbWrite, "edadb_write")`

2. `src/interface/tcl/tcl_idb/tcl_db_file.h`
   - 看 `CmdEdadbRead`
   - 看 `CmdEdadbWrite`

3. `src/interface/tcl/tcl_idb/tcl_db_file.cpp`
   - 看 `CmdEdadbRead::exec()`
   - 看 `CmdEdadbWrite::exec()`

这里要确认两件事：

- Tcl 参数 `-edadb_db_path` 是否正确传到 C++。
- `edadb_read` 是否同时拿到 EDADB database path 和原始 DEF path。

当前关键调用是：

```cpp
dmInst->saveDefToEdadb(edadb_path);
dmInst->readDefFromEdadb(edadb_path, path);
```

## 4. 读 DataManager 和 Builder 接口

接下来进入平台层：

1. `src/platform/data_manager/idm.h`
   - 看 `readDefFromEdadb()`
   - 看 `saveDefToEdadb()`

2. `src/platform/data_manager/idm_edadb.cpp`
   - 看 `DataManager::readDefFromEdadb()`
   - 看 `DataManager::saveDefToEdadb()`

3. `src/database/manager/builder/builder.h`
   - 看 `buildDefFromEdadb()`
   - 看 `saveDefToEdadb()`

4. `src/database/manager/builder/builder.cpp`
   - 看 `IdbBuilder::buildDefFromEdadb()`
   - 看 `IdbBuilder::saveDefToEdadb()`

这一层不要陷入具体 object 字段，只确认调用方向：

```text
Tcl command
  -> DataManager
  -> IdbBuilder
  -> DefWriteEdadb / DefReadEdadb
```

## 5. 读 EDADB 写入链路

写入入口：

- `src/database/manager/builder/def_builder/def_write_edadb.h`
- `src/database/manager/builder/def_builder/def_write_edadb.cpp`

先读：

```cpp
DefWriteEdadb::writeDb2Edadb()
DefWriteEdadb::writeChip2Edadb()
```

然后按顺序读这些函数：

```cpp
writeIdbDesign()
writeIdbDie()
writeIdbRow()
writeIdbTrackGrid()
writeIdbGCellGrid()
writeIdbVia()
writeIdbInstance()
writeIdbPin()
writeIdbBlockage()
writeIdbRegion()
writeIdbSlot()
writeIdbGroup()
writeIdbFill()
writeSpecialNet()
writeIdbNet()
```

每个函数的阅读方法一样：

1. 看它从 iDB 哪个 list 取数据。
2. 看它用原始对象直接写，还是先转成 `edadb::Shadow<T>`。
3. 看它最后调用哪个 EDADB API，例如：

```cpp
edadb::insertObject<T>(...)
```

写入链路需要特别关注：

- `writeIdbNet()` 和 `writeSpecialNet()` 是否保存 pin 顺序、source、original、weight 等字段。
- `writeIdbFill()` 是否区分 layer fill 和 via fill。
- `writeIdbGroup()` 是否保存 group name、region name、instance member。

## 6. 读 EDADB 读回链路

读回入口：

- `src/database/manager/builder/def_builder/def_read_edadb.h`
- `src/database/manager/builder/def_builder/def_read_edadb.cpp`

先读：

```cpp
DefReadEdadb::createDbFromEdadb()
DefReadEdadb::createDbByEdadb()
```

然后按对象族读：

```cpp
readIdbDesign()
readIdbDie()
readIdbRow()
readIdbTrackGrid()
readIdbGCellGrid()
readIdbVia()
readIdbRegion()
readIdbInstance()
readIdbPin()
readIdbBlockage()
readIdbSlot()
readIdbGroup()
readIdbFill()
readSpecialNet()
readIdbNet()
```

这里重点看三件事：

1. EDADB 读出的 shadow 是否完整还原到 iDB 对象。
2. readback 是否依赖 helper 查找 layer、via、instance、region。
3. DEF parser callbacks 是否被关闭，避免同一个对象既从 EDADB 建一次，又从 DEF text 建一次。

对应 helper 在：

- `src/database/edadb/idb/edadb_idb_helper.h`
- `src/database/edadb/idb/edadb_idb_helper.cpp`

## 7. 读 schema 和 shadow

当你理解 read/write 调用后，再读 adapter 的核心映射。

先读聚合入口：

- `src/database/edadb/idb/edadb_idb.h`
- `src/database/edadb/idb/edadb_idb_init.h`
- `src/database/edadb/idb/edadb_idb_init.cpp`

重点看：

```cpp
initWriteDb()
initReadDb()
initPrimKeys()
initAllTables()
```

然后读表定义：

- `src/database/edadb/idb/edadb_idb_schema.h`

这里会看到类似：

```cpp
TABLE4CLASS(...)
TABLE4CLASS_WVEC(...)
```

这些宏定义了：

- 哪个 C++ 类型对应哪个 SQL table。
- 哪些字段是普通 column。
- 哪些 vector 字段会展开成 child table。
- primary key / foreign key 如何绑定。

最后读 shadow：

- `src/database/edadb/idb/shadow/shadow_idb_net.h`
- `src/database/edadb/idb/shadow/shadow_idb_special_net.h`
- `src/database/edadb/idb/shadow/shadow_idb_fill.h`
- `src/database/edadb/idb/shadow/shadow_idb_group.h`
- 其他 `shadow_idb_*.h`

shadow 的作用是：

```text
iDB object <-> serializable EDADB object
```

阅读每个 shadow 时重点看：

- constructor / `fromIdb` 一类逻辑如何从 iDB 抽字段。
- `toIdb` / restore 一类逻辑如何把字段放回 iDB。
- vector child 是否有顺序字段，例如 net pin ref 的 `_order_sd`。

## 8. 再读原始 DEF reader/writer 的改动

这一层是容易混淆的地方：有些修复不是 EDADB wrapper，而是原始 iEDA DEF
读写器本身需要补字段。

重点文件：

- `src/database/manager/builder/def_builder/def_read.cpp`
- `src/database/manager/builder/def_builder/def_read.h`
- `src/database/manager/builder/def_builder/def_write.cpp`
- `src/database/manager/builder/def_builder/def_write.h`

重点检查：

- `DefRead::read_net()` / related parser 是否读入 `FIXEDBUMP`。
- GROUP parser 是否保存 group name 和 member。
- `DefWrite::write_net()` 是否输出 `SOURCE`、`ORIGINAL`、`WEIGHT`、`XTALK`、
  `FIXEDBUMP`、`FREQUENCY`。
- `DefWrite::write_special_net()` 是否输出 `SOURCE`、`ORIGINAL`、`WEIGHT`。
- `write_fill()` 和 `write_region()` 是否输出完整 section end。

这些修改会同时影响 direct iDB baseline 和 EDADB roundtrip，所以测试时不要只比较原始
input DEF，要比较：

```text
input DEF -> normal iDB -> output DEF
input DEF -> EDADB -> iDB -> output DEF
```

## 9. 读 EDADB core

只有当你需要理解 EDADB API 本身时，再进入 core。

建议顺序：

1. `src/database/edadb/core/include/edadb.h`
   - 先看 public API。

2. `src/database/edadb/core/include/edadb/Table4Class.h`
   - 看 table/class 映射宏。

3. `src/database/edadb/core/include/edadb/Shadow.h`
   - 看 shadow specialization 的机制。

4. `src/database/edadb/core/include/edadb/DbTableOperator.h`
   - 看 insert/select/update/delete 的抽象。

5. `src/database/edadb/core/include/edadb/backend/sqlite/*`
   - 看 SQLite backend 如何生成和执行 SQL。

6. `src/database/edadb/core/test/*`
   - 看 EDADB core 自己的 ORM/table-op 测试。

当前 core 相对 C baseline `8a4e3bf` 的本地变化只有：

- `include/edadb/backend/sqlite/DbStatement4Sqlite.h`
- `test/CMakeLists.txt`

这两个文件主要服务于 SQLite debug trace 测试。

## 10. 最后读测试

可重复测试放在：

- `src/database/edadb/test/README.md`
- `src/database/edadb/test/run_idb_roundtrip_regression.sh`
- `src/database/edadb/test/tcl/*.tcl`

运行：

```bash
bash src/database/edadb/test/run_idb_roundtrip_regression.sh
```

默认输出：

```text
/tmp/iedadb_regression
```

当前测试包含两个 case：

- `default_ipl`
  - 使用 sky130_gcd `iPL_result.def`。
  - 比较 direct iDB DEF output 和 EDADB DEF output。

- `aux_optional`
  - 从 `iPL_result.def` 生成复杂 fixture。
  - 添加非空 `BLOCKAGES`、`REGIONS`、`SLOTS`、`GROUPS`、`FILLS`。
  - 添加 regular net 和 special net optional fields。
  - 检查 SQLite 表内容和 DEF roundtrip diff。

测试读法：

1. 先看 `run_idb_roundtrip_regression.sh` 如何生成 fixture。
2. 再看 `run_case()` 如何跑 direct baseline 和 EDADB roundtrip。
3. 最后看 SQLite assertions，它们对应 EDADB 内部表是否真的写对。

## 11. 推荐完整阅读顺序

按下面顺序读，最不容易迷路：

1. `scripts/edadb/demo/demo.sh`
2. `scripts/edadb/demo/tcl/def2edadb.tcl`
3. `scripts/edadb/demo/tcl/edadb2def.tcl`
4. `src/interface/tcl/tcl_idb/tcl_register_idb.h`
5. `src/interface/tcl/tcl_idb/tcl_db_file.*`
6. `src/platform/data_manager/idm.h`
7. `src/platform/data_manager/idm_edadb.cpp`
8. `src/database/manager/builder/builder.*`
9. `src/database/manager/builder/def_builder/def_write_edadb.*`
10. `src/database/manager/builder/def_builder/def_read_edadb.*`
11. `src/database/edadb/idb/edadb_idb_init.*`
12. `src/database/edadb/idb/edadb_idb_schema.h`
13. `src/database/edadb/idb/shadow/*`
14. `src/database/manager/builder/def_builder/def_read.*`
15. `src/database/manager/builder/def_builder/def_write.*`
16. `src/database/edadb/core/include/edadb.h`
17. `src/database/edadb/core/include/edadb/Table4Class.h`
18. `src/database/edadb/core/test/*`
19. `src/database/edadb/test/run_idb_roundtrip_regression.sh`

## 12. 阅读时的检查清单

读每个对象族时，用同一套问题检查：

- iDB 对象在哪个 list 里？
- 写入 EDADB 时使用 direct object 还是 shadow？
- schema 表里有哪些 scalar column？
- vector 字段展开到了哪些 child table？
- child table 是否需要稳定顺序？
- EDADB 读回时是否能找到 layer/via/instance/region 等引用？
- normal DEF writer 是否会把该字段写回 DEF？
- 测试里是否既有 DEF diff，又有 SQLite 内部字段断言？

如果某个字段只写进 EDADB，但 normal DEF writer 不输出它，那么 DEF byte diff 看不出来，
需要 adapter-level object assertion 或 SQLite assertion。
