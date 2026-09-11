# 主外键读写性能测试计划

状态：首轮main已完成，88组正确性检查、275条计时与55个文件库逐字段审计通过；比例扩展未运行。
独立于[单表基线](../sqlite-baseline/readme.md)，不修改adapter或EDADB core。结果见[实验报告](experiment_report.md)。

## 1. 对象与三种schema

模拟Component拥有多个instance pin，不是DEF顶层PINS，也不包含LEF派生计算。
沿用Component的8个字段，新增pins vector；不使用shadow、不要求vector顺序。

```cpp
struct PinRecord {
    std::string pin_name;
    int32_t direction, x, y;
};
struct ComponentRecord {
    std::string name, master_name;
    int32_t source, status, orient, x, y;
    int64_t record_order;
    std::vector<PinRecord> pins;
};
TABLE4CLASS(PinRecord, "instance_pin", (pin_name, direction, x, y));
TABLE4CLASS_WVEC(ComponentRecord, "component",
    (name, master_name, source, status, orient, x, y, record_order), (pins));
```

父表name始终为PK。EDADB在子表追加父FK，引用父name；子对象身份为(父name, pin_name)，不是vector下标。

| --schema | 子表PK | 子表手工索引 | PinRecord的hasPrimKey trait |
| --- | --- | --- | --- |
| pk | (父FK, pin_name) | 无 | true |
| index | 无 | 非唯一INDEX(父FK, pin_name) | false |
| none | 无 | 无显式索引 | false |

每个样本使用独立数据库，始终只有两张用户表；不是一个库建三个子表。
三组都有FK定义；无显式索引仍有SQLite rowid结构，不使用WITHOUT ROWID。
实际DDL由EDADB元数据生成，两条API使用同一份；父FK和CASCADE定义保持一致。
因core的hasPrimKey也影响首列NOT NULL，测试建表层统一pin_name NOT NULL，避免混入空值约束差异。
建表后核对sqlite_schema及PRAGMA table_info/index_list/index_info/foreign_key_list。

## 2. 配置方式：编译一次，每组独立进程

hasPrimKey是运行时可写的static inline bool，不需要为三种schema分别编译：

```text
解析 --route sqlite|edadb、--schema pk|index|none
设置 Cpp2SqlTypeTrait<ComponentRecord>::hasPrimKey = true
设置 Cpp2SqlTypeTrait<PinRecord>::hasPrimKey = (schema == pk)
之后才创建元数据、生成DDL和创建操作对象
新建数据库 → 建父子表 → 仅index模式创建非唯一复合索引
按route调用SQLite或EDADB进行读写
```

不在同一进程切换schema，避免元数据缓存混用；不修改已有数据库。
index由测试程序执行CREATE INDEX，不增加core自动索引规则。

编译后可运行以下正确性检查（正式调度与结果路径见实验报告）：

```bash
# 同一个二进制，每个组合启动新进程；DB路径必须独立。
for route in sqlite edadb; do
  for schema in pk index none; do
    /tmp/iedadb_pk_fk_build/pk_fk_benchmark \
      --route "$route" --schema "$schema" --storage disk --foreign-keys on \
      --parents 10000 --children-per-parent 10 --mode check \
      --db "/tmp/pk_fk_${route}_${schema}.db"
  done
done
```

check成功后，runner用独立路径执行perf样本；不覆盖check库。memory配置使用:memory:，每进程独立连接。

## 3. 读写与计时

| 阶段 | SQLite C API | EDADB C++ API |
| --- | --- | --- |
| init/create | 连接配置与元数据准备；建两表和索引另计 | 相同DDL和配置；不修改core |
| write | 两条INSERT各prepare一次；逐父及其子bind/step/reset；finalize | makeInsertOp，逐父insert自动递归pins；销毁op |
| read | 父SELECT遍历；复用带父FK参数的子SELECT；恢复父及pins；finalize | makeReadAllOp/readNext恢复父及pins；销毁reader |

- 两条路线都恢复并消费一个父对象及其pins，不保留全部父对象；子vector分配、填充和清理计入read。
- 所有字段都读取，不加ORDER BY；子SELECT只prepare一次，每父重新bind/step/reset，预期1+父数量次SELECT执行。
- 输入生成在计时外；init、create、BEGIN、write、COMMIT、read、close分别记录ms，write_complete=BEGIN+write+COMMIT。
- 两条路线均一个外层数据事务，使用EDADB可复用op避免每父自动包事务。write含索引维护、约束和可能的I/O，不是纯bind时间。
- 额外SQLite fetch-only：两次全表SELECT消费字段，不恢复对象图；仅作吞吐参考，不直接计算EDADB框架开销。

## 4. 数据与运行

父数据沿用单表公式：name=U加7位序号，master_name=bench_cell，source=2/status=3/orient=1，
x=(index%1000)*100，y=(index/1000)*100，record_order=index。
子名=P加4位序号，direction=1，x=父x+10*(子序号+1)，y=父y；不同父可有相同子名。
按父分组、名称递增插入；不推断随机插入性能。

- 首轮：10,000父×10子；SQLite/EDADB × pk/index/none × memory/disk。
- 后续比例：1,000父×100子、100,000父×1子。父记录数也变化，差值不全归因于查询次数。
- FK控制：固定pk和首轮数据，只切换foreign_keys ON/OFF。fetch-only仅首轮数据，单独报告。
- memory/disk复用[A/B-batch参数](../sqlite-baseline/sqlite_config.md)，但主实验统一FK=ON，标记A-fk-on/B-batch-fk-on；重连后也核验。不是完全默认配置。
- Release -O3、同一SQLite库、关闭trace；性能串行，预热1次、正式5次、样本前等待5秒。
- memory测first/repeat，disk新连接OS-warm；首轮不测cold。none先小规模估时，不静默缩减正式数据。

## 5. 正确性与结果

- 独立check逐字段按(父name, pin_name)比较：空库、空父、多子、跨父同名子、非顺序输入；性能只保留消费摘要和正常错误检查。
- FK ON拒绝缺失父；pk拒绝同父重复子名，index/none允许；违规数据仅做正确性，验证失败回滚。
- 诊断在计时外记录DDL、配置、EXPLAIN QUERY PLAN、查询次数。不能只凭有无索引预断SCAN/SEARCH。
- index-none比较普通索引净成本/收益；pk-index比较唯一性及其实现净差异；同schema下FK ON-OFF比较外键检查影响。
- 同组EDADB-SQLite是含递归/对象恢复的整条路径净差值，不声称分离了内部模块时间。
- 完成标准：DDL/配置匹配、check通过、每组5条有效样本且可审计；报告create/write/commit/read分项、均值/中位数/min/max。

## 6. 文件与范围

实现：pk_fk_benchmark.cpp（数据/映射/DDL/读写/check）、run_pk_fk.py（调度统计）、audit_pk_fk.py（结果复核）、CMakeLists.txt（独立Release目标，SQLite库+EDADB头文件）。
方法与结果分别补到implementation.md、experiment_report.md；数据、DB、日志和统计文件放仓库外，不提交Git。
首轮使用runner的--stage main；比例扩展为--stage ratios，单独记录状态，不混称已完成。未经要求不commit/push。

依据：[SQLite FK与索引](https://www.sqlite.org/foreignkeys.html#required_and_suggested_database_indexes)、
[rowid/PK](https://www.sqlite.org/rowidtable.html)；core的Cpp2SqlTypeTrait、SqlStatement4Sqlite::createTableStatement及makeInsertOp/makeReadAllOp。
