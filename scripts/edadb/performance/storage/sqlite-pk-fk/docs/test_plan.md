# 主外键实验方案

## 1. 固定对象与schema

模拟Component拥有instance Pin，不是DEF顶层PINS，不包含LEF派生计算或Shadow。父对象沿用单表8字段，新增`vector<PinRecord> pins`；Pin包含pin_name、direction、x、y。子对象身份为(父名称, pin_name)，不要求vector顺序。

实际公共DDL如下；两条API都使用EDADB生成的相同定义：

```sql
CREATE TABLE component (
  name TEXT NOT NULL, master_name TEXT,
  source INTEGER, status INTEGER, orient INTEGER, x INTEGER, y INTEGER,
  record_order BIGINT, PRIMARY KEY(name)
);
CREATE TABLE component_pins_instance_pin (
  pin_name TEXT NOT NULL, direction INTEGER, x INTEGER, y INTEGER,
  component_name TEXT NOT NULL,
  FOREIGN KEY(component_name) REFERENCES component(name)
    ON DELETE CASCADE ON UPDATE CASCADE
);
```

| --schema | 对公共子表定义的改变 | 语义 |
| --- | --- | --- |
| pk | 加入PRIMARY KEY(component_name, pin_name) | 父内Pin名称唯一 |
| index | 建非唯一INDEX(component_name, pin_name)，不加PK | 加速查找，不限制重复组合 |
| none | 不增加PK或显式索引 | 仍是普通rowid表，仍有FK定义 |

现有主矩阵每组新建独立数据库，只有两张用户表，不在同库建三张子表；均为普通rowid表。父PK始终相同，pin_name NOT NULL在三组一致。WITHOUT ROWID另设独立实验，不替换这三组结果。

编译一次，通过参数选择：创建元数据前设置Pin的hasPrimKey，仅pk为true；index由测试程序创建索引。每个样本独立进程，避免元数据缓存跨schema混用，不修改core。

## 2. 数据与实验矩阵

主数据为10,000父×10子，共110,000条记录，按父分组递增写入。只代表这种访问模式，不等同于单表10,000条数据量。

- 父：name=U加7位序号，master_name=bench_cell；source/status/orient=2/3/1；x=(序号%1000)*100，y=(序号/1000)*100，record_order=序号。
- 子：pin_name=P加4位序号，direction=1，x=父x+10*(子序号+1)，y=父y；不同父可有同名子。
- 输入与期望数据在计时外生成。字段值与生成公式由[实现](implementation.md)中的代码负责。

| 对照 | 固定条件 | 改变的因素 |
| --- | --- | --- |
| 主矩阵 | 数据、父PK、FK ON、对象图恢复 | SQLite/EDADB × pk/index/none × memory/disk |
| FK检查 | pk、相同DDL/事务/数据 | foreign_keys ON/OFF |
| flat参考 | SQLite、相同两表及数据 | 两次全表取字段，不恢复父子对象图 |

memory/disk沿用[A/B-batch](../../sqlite-reference/config.md)，主矩阵均额外指定FK ON，重连后重新设置并核验；不称完全默认配置。memory测同连接first/repeat，disk测新连接OS-warm；不测试cold。

## 3. 执行与验收

- Release -O3、同一SQLite库、关闭trace；预热1次、正式5次串行，样本前等待5秒。
- SQLite与EDADB都逐父插入及递归读取。子查询复用statement，无ORDER BY；预计1+父数次SELECT执行。
- 数据写入使用一个外层事务；init/create/BEGIN/write/COMMIT/read/close单列，具体边界只在[实现](implementation.md)维护。
- 独立check逐值验证空库、空父、多子、跨父同名子、非顺序输入；孤儿、重复键及失败回滚单独验证。性能只消费摘要和正常错误检查。
- check核对DDL、索引列序、FK、实际配置、SQL、EQP和SELECT次数；正式计时不启用trace。
- 每组5个有效样本，原始日志可重算统计；文件库逐值及integrity_check/foreign_key_check通过。

index−none是普通索引的净影响，pk−index是唯一性及实现的净差异；EDADB−SQLite包含包装、对象分配与恢复，不能全称遍历成本。flat少做对象关联，不用于计算框架开销。

## 4. 独立实验边界

本实验比较child的pk/index/none，不扩展WITHOUT ROWID。后续独立的[sqlite-index](../../sqlite-index/docs/test_plan.md)已收敛为单表主键、索引与布局对照，不包含FK；两套结果分别保存。

## 5. 其他未做范围

父子比例扩展1,000×100及100,000×1、cold、相同对象语义的批量读取尚未运行。比例变化也改变父记录量，不能把差值全归于查询次数。先审阅当前结果，再决定补测。
