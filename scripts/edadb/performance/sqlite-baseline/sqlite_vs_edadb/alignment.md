# SQL/API对齐实验：方法与实现

本页只定义实验、实现和运行方法；时间表、delta及原始证据统一见[results.md](results.md)。

## 1. 实验条件与阅读方法

固定1,000,000条8字段Component，无PK/FK/index；[数据定义](../baseline/readme.md)不变。
**下述每一组都分别运行A和B-batch。中文名称说明测试内容，括号内英文仅用于查找脚本和原始结果。**

- A：内存DB、MEMORY日志、同步OFF、批量事务；同连接first-read。
- B-batch：文件DB、本批实际DELETE/FULL、批量事务；OS-warm新连接读取。[配置说明](../sqlite_params/config.md)
- Release -O3；完整规模check/预热1次，正式串行5次，样本前及读写间等待1秒。
- 正确性覆盖0/1/8条及完整规模、逐字段校验、同长度损坏检测和实际SQL/次数；性能样本不开trace。
- 所有表中时间为**data中位数，单位ms**。init、create、BEGIN、COMMIT、close另列，不混入data。
- 只在同一配置、同一读写方向、同一批次中相减；首轮对照独立归档，不参与本批delta。

## 2. 读取：NULL检查、SQL/取列/收尾操作

### 过程与组定义

```text
开始read data
  prepare SELECT / 构造EDADB reader
  循环：step → 取8列 → 赋值并消费Record
  到达DONE → 结束清理 → finalize / 销毁reader
结束read data
```
已修正读取块缩进，保持逻辑、行号及计时边界不变。Release -O3重新构建后，目标文件和完整可执行文件
均与整理前逐字节相同；可执行文件SHA256仍为
`f9c06a09313a98d980adc301163e4bdbec739ceb9c696f2c519c8196eb96fd54`，与正式批次manifest一致。

先定义两类改变，避免用“其他操作”笼统代指：

- **逐列NULL检查**：每列取值前调用`sqlite3_column_type`判断是否为NULL；每条记录8次。
- **SQL/取列/收尾匹配**：①使用与EDADB完全相同的SELECT文本；②字符串显式按`column_bytes → column_text → assign`顺序读取；③整个SELECT返回DONE后，增加一次`clear_bindings → reset`。三项一起改变，本实验没有单独测它们各自的成本。

**“全部匹配”（脚本名aligned）= 上述两类改变同时启用。它仍是手写SQLite，不调用EDADB的成员遍历或操作器状态管理。**不是把EDADB全部实现复制进来，也不是对数据库记录排序。

#### SELECT究竟哪里不同？

```sql
-- 基本读取、仅加NULL检查：raw / checks
SELECT name,master_name,source,status,orient,x,y,record_order FROM component
-- 匹配读取及EDADB：other / aligned / edadb
SELECT "name", "master_name", "source", "status", "orient", "x", "y", "record_order" FROM "component";
```

**只有标识符引号、空格和末尾分号不同；读取的表、8列及列次序相同，都没有WHERE、JOIN或ORDER BY。**
这不是两种不同的关联查询，也没有额外排序。每次读取仅prepare一次，不能把SQL生成/prepare成本乘以记录数。
实际执行文本由正确性运行中的trace核验，见[结果页的原始证据](results.md#原始数据与复核)。

| 匹配项 | EDADB具体实现 | test bench如何对应 |
| --- | --- | --- |
| SELECT文本 | [readAllStatement:195](../../../../../src/database/edadb/core/include/edadb/backend/sqlite/SqlStatement4Sqlite.h#L195)调用[projectAllStatement:169](../../../../../src/database/edadb/core/include/edadb/backend/sqlite/SqlStatement4Sqlite.h#L169)拼列名及表名 | [alignment.cpp:112](alignment.cpp#L112)选择上述两条SQL；全部仍调用prepare_v2一次 |
| NULL检查 | [Select:76](../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h#L76)→[fetchNull:347](../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbStatement4Sqlite.h#L347)→column_type | [alignment.cpp:152](alignment.cpp#L152)对8列分别增加同样的检查 |
| 字符串取列 | [fetchColumn:395](../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbStatement4Sqlite.h#L395)：bytes→text→assign | [alignment.cpp:85](alignment.cpp#L85)显式采用同一顺序；raw把两个API作为assign实参，C++不规定这两个实参谁先求值，**不能断言实际机器码顺序不同** |
| DONE后收尾 | [Select:411](../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h#L411)→[resetForReuse:131](../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbStatement4Sqlite.h#L131)：clear→reset | [alignment.cpp:174](alignment.cpp#L174)在整个SELECT结束后执行一次；所有组最后都finalize |

本次两个字符串均为UTF-8 TEXT。这里匹配的是现有EDADB代码，不是建议通用取列顺序；
SQLite官方更稳妥的通用建议是先`column_text`再`column_bytes`，避免类型转换影响指针/长度。[官方取列说明](https://www.sqlite.org/c3ref/column_blob.html)

| 读取组（脚本名） | 本组启用的操作 |
| --- | --- |
| SQLite基本读取（raw） | 直接取8列 |
| SQLite仅加NULL检查（checks） | raw＋逐列NULL检查 |
| SQLite仅匹配SQL/取列/收尾（other） | 匹配上述三项，不加NULL检查 |
| SQLite全部匹配读取（aligned） | NULL检查＋上述三项 |
| EDADB原生API读取（edadb） | 完整EDADB reader |

手写匹配SQL在计时外生成；EDADB仍在首次prepare内生成SQL，计入data。DONE清理整次查询只执行一次。

源码：[取列与NULL控制](alignment.cpp#L96)、[字符串顺序](alignment.cpp#L85)、
[read计时](alignment.cpp#L280)；
EDADB依据：[逐列NULL](../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h#L76)、
[取字符串](../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbStatement4Sqlite.h#L395)、
[DONE清理](../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpSelect4Sqlite.h#L411)。

## 3. 写入：clear绑定、SQL及绑定API

### 过程与组定义

```text
init、create、数据BEGIN（分别计时，不属于write data）
开始write data
  prepare INSERT / 构造EDADB writer
  循环：绑定8字段 → step → [部分组增加clear_bindings] → reset
  finalize / 销毁writer
结束write data
数据COMMIT、关闭（分别计时，不属于write data）
```

**reset所有组都有；新增的是clear_bindings。**不能把原有reset耗时全部算成EDADB额外成本。

写入也分两类改变：
- **每行清除绑定**：每次INSERT的step之后、reset之前，增加一次`clear_bindings`。
- **SQL/绑定API匹配**：使用与EDADB相同的INSERT文本；2个字符串由`bind_text`改为`bind_text64`；5个整数由`bind_int64`改为`bind_int`，record_order仍用`bind_int64`。

**全部匹配写入 = 每行清除绑定＋SQL/绑定API匹配。**仍由手写SQLite完成，不包含EDADB成员遍历。

| 写入组（脚本名） | 本组启用的操作 |
| --- | --- |
| SQLite基本写入（raw） | 手写INSERT；2次bind_text、6次bind_int64；step→reset |
| SQLite仅加清除绑定（checks） | raw＋每行clear_bindings |
| SQLite仅匹配SQL/绑定API（other） | 匹配SQL及绑定函数，不加clear |
| SQLite全部匹配写入（aligned） | clear＋SQL/绑定API匹配 |
| EDADB原生API写入（edadb） | 完整EDADB writer |

字符串绑定均使用TRANSIENT。手写匹配SQL在计时外生成，EDADB生成SQL的成本仍计入data。

源码：[绑定API及clear控制](alignment.cpp#L96)、[write计时](alignment.cpp#L264)；
EDADB依据：[写后clear/reset](../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbTableOpInsert4Sqlite.h#L230)、
[SQL生成](../../../../../src/database/edadb/core/include/edadb/backend/sqlite/SqlStatement4Sqlite.h#L123)。

具体API对应：EDADB的[clearBindings:108](../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbStatement4Sqlite.h#L108)、
[bind_int:184](../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbStatement4Sqlite.h#L184)、
[bind_int64:207](../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbStatement4Sqlite.h#L207)、
[bind_text64:242](../../../../../src/database/edadb/core/include/edadb/backend/sqlite/DbStatement4Sqlite.h#L242)，
分别对照bench的[每行clear:139](alignment.cpp#L139)和[绑定函数选择:121](alignment.cpp#L121)。
绑定匹配同时把raw的整数数组循环换成逐字段调用，因此它是**绑定实现整体对照**，不是某一个SQLite API的单因素测试。

## 4. 构建、运行与审计

- [alignment.cpp](alignment.cpp)：独立实验入口，从现有计时入口隔离出direct两路线；额外操作在模板中选择，无逐记录实验开关。
- [共用支持头](../benchmark/benchmark_support.h)：只读复用Record、生成器及辅助函数。
- [run_alignment.py](run_alignment.py)：边界/全量正确性、顺序采样、统计、原始证据。
- [audit_alignment.py](audit_alignment.py)：重算统计及delta，核对日志、实际SQL次数、配置和源码哈希，生成report.md；不重跑性能样本。
- [CMakeLists.txt](CMakeLists.txt)：Release -O3、SQL trace默认关闭；不改生产依赖或原测试文件。

```bash
cd "$(git rev-parse --show-toplevel)"
cmake -S scripts/edadb/performance/sqlite-baseline/sqlite_vs_edadb \
  -B /tmp/iedadb_alignment_build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-10
cmake --build /tmp/iedadb_alignment_build -j40 --target alignment

# 小规模检查，输出目录须不存在
python3 scripts/edadb/performance/sqlite-baseline/sqlite_vs_edadb/run_alignment.py \
  /tmp/iedadb_alignment_build/alignment /tmp/iedadb_alignment_smoke \
  --count 1000 --runs 1 --settle 0

# 正式实验；不要与其他性能实验并发
python3 scripts/edadb/performance/sqlite-baseline/sqlite_vs_edadb/run_alignment.py \
  /tmp/iedadb_alignment_build/alignment /tmp/iedadb_alignment_full \
  --count 1000000 --runs 5 --settle 1
python3 scripts/edadb/performance/sqlite-baseline/sqlite_vs_edadb/audit_alignment.py \
  /tmp/iedadb_alignment_full
```

输出目录保存samples.tsv、summary.json、deltas.json、checks.json、audit.json、
manifest.json、configs.json、SQL文本、编译参数、源代码快照及日志。
生成DB在检查结束后删除；正式时刻和完整数据不写入Git。

原始证据与完成状态统一见[结果页](results.md#原始数据与复核)。
