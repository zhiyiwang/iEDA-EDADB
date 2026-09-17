# SQLite如何执行SQL：从prepare到返回结果

本文说明SQLite官方实现，不包含性能实验方案。源码链接固定到 **3.37.2**；内部指令及布局可能随版本变化。

## 1. 总体流程

SQLite是嵌入式库，应用通过函数调用提交SQL，不需要独立数据库服务器进程。[官方工作原理](https://www.sqlite.org/howitworks.html)

```text
应用：sqlite3_prepare_v2(SQL)
    ↓
分词 → 语法解析 → 名称/类型亲和性等语义处理 → 选择访问计划 → 生成VM指令
    ↓
sqlite3_stmt：保存可执行字节码及执行状态
    ↓
应用：sqlite3_bind_* 为参数赋值
    ↓
应用：sqlite3_step
    ↓
VDBE执行指令 → B-tree访问 → Pager/页缓存 → 必要时经VFS访问文件
    ↓
ROW：一行就绪，应用取列后再次step
DONE：本次语句执行完成
错误码：由应用检查并处理
    ↓
reset：复用语句；或finalize：销毁语句
```

VDBE是SQLite的字节码执行引擎（Virtual Database Engine），不是独立进程，也不是CPU直接执行的机器码。[官方架构](https://www.sqlite.org/arch.html)、[字节码引擎](https://www.sqlite.org/opcode.html)

## 2. 准备：SQL如何变为sqlite3_stmt

| 阶段 | 实际工作 | 3.37.2源码入口 |
| --- | --- | --- |
| prepare入口 | 接收SQL及连接，编译第一条语句；pzTail指向剩余文本 | [prepare.c:882](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/prepare.c#L882) |
| 分词与解析 | tokenize.c识别token，送入由Lemon根据parse.y生成的解析器 | [tokenize.c:568](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/tokenize.c#L568)、[parse.y](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/parse.y) |
| 语义与计划 | 解析表/列等引用；SELECT、INSERT等代码生成器处理对应语义，where相关代码选择访问方式 | [select.c](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/select.c)、[insert.c](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/insert.c)、[where.c](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/where.c) |
| 生成可执行对象 | 构造VDBE指令，完成寄存器等执行资源准备，将statement返回应用 | [build.c:286](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/build.c#L286) |

这些是职责划分，不表示各步骤必然是完全独立、只遍历一次的流水线。
prepare可能需要读取schema；“尚未执行INSERT”不等于“完全不访问数据库”。普通INSERT/SELECT的数据处理在step执行；部分PRAGMA有prepare阶段生效的特殊行为。[schema初始化源码](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/prepare.c#L437)、[PRAGMA说明](https://www.sqlite.org/pragma.html)

prepare成功返回SQLITE_OK与statement；空白/注释可能不生成statement。v2/v3保留原SQL，schema变化或某些影响计划的绑定参数变化时，step可以自动重新prepare。因此“复用statement”不等于任何条件下都绝不重新编译。[官方prepare接口](https://www.sqlite.org/c3ref/prepare.html)

## 3. 绑定：不是把参数拼回SQL字符串

```sql
INSERT INTO component(name,x) VALUES(?,?);
```

prepare先编译带参数的语句；bind将值写入statement的参数槽。执行时**Variable指令**把绑定值送到VM寄存器，而不是每次重新拼接并解析SQL。
字符串绑定的**TRANSIENT**要求SQLite在bind返回前复制；**STATIC**要求调用方保持所引用内容有效至该参数重绑或statement销毁。[官方bind](https://www.sqlite.org/c3ref/bind_blob.html)、[Variable源码](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbe.c#L1347)

### 参数槽与结果槽不是同一组数据

3.37.2的statement中，**`aVar`** 保存绑定参数，`aMem`是执行寄存器，**`pResultSet`** 指向当前结果行。bind按从1开始的参数编号写入`aVar[i-1]`；column接口按从0开始的结果列编号读取当前结果，不把结果写回参数槽。[结构定义](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbeInt.h#L380)

```text
写：应用值 → bind → aVar参数槽 → Variable → aMem寄存器 → MakeRecord → B-tree
读：B-tree记录 → Column → 结果寄存器/pResultSet → sqlite3_column_* → 应用
```

重复出现的同名参数共用编号；未绑定参数按NULL处理。reset不清除绑定，重新bind会替换对应槽的旧值。[官方绑定规则](https://www.sqlite.org/c3ref/bind_blob.html)

### bind：不同类型的所有权不同

| 接口/类型 | SQLite保存什么 | 调用方原数据能否随后释放/改变 |
| --- | --- | --- |
| bind_int / bind_int64 | 数值存入内部Mem；int入口转为int64处理 | 能，不保留调用方整数变量地址 |
| bind_double | double值存入内部Mem | 能，不保留调用方double变量地址 |
| bind_null | 槽标记为NULL | 没有字符串缓冲区生命周期问题 |
| bind_text / bind_blob + TRANSIENT | SQLite在bind返回前取得内容副本 | 返回后可以释放原缓冲区 |
| bind_text / bind_blob + STATIC | 调用方负责缓冲区有效期；不是要求C/C++ static变量 | 必须保持有效至参数重绑或finalize，不能提前修改内容 |
| bind_text / bind_blob + 析构回调 | SQLite用完时调用指定释放函数 | 不应由调用方提前释放；错误路径的回调规则见官方接口 |
| bind_zeroblob | 用长度表示零填充BLOB，初始使用固定量内存 | 不需要调用方准备同等大小的零数组 |

源码：[数值绑定](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbeapi.c#L1443)、[文本/BLOB绑定](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbeapi.c#L1385)、[TRANSIENT复制分支](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbemem.c#L1143)。3.37.2的bind_value按值类型分派，文本/普通BLOB采用TRANSIENT：[源码](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbeapi.c#L1525)。

**STATIC不等于整个写入零复制。** 编码转换可能分配新缓冲区；Variable复制Mem描述及标量，不必深拷贝字符串；之后MakeRecord仍需形成数据库记录。参数绑定类型也不等于最终存储表示，列的类型亲和性可能转换值。[类型与亲和性](https://www.sqlite.org/datatype3.html)、[Variable实现](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbe.c#L1347)

### 取列：数值按值返回，文本/BLOB返回借用指针

| 接口 | 返回形式 | 是否替调用方复制完整内容 |
| --- | --- | --- |
| column_int / int64 / double | C数值，必要时转换类型 | 返回数值本身，不返回原字段地址 |
| column_text / text16 / blob | SQLite管理的当前结果缓冲区指针 | 不自动建立调用方拥有的副本；转换类型/编码时SQLite内部可能复制或分配 |
| column_bytes / bytes16 | 指定表示的字节数 | 不创建应用副本；如果需要转换表示，不能保证无转换成本 |
| column_type | 当前结果初始类型的代码 | 用于区分NULL等；应在引起类型转换之前查询 |

数值NULL取值会得到0/0.0，文本NULL得到空指针，不能据此区分NULL与数值0。零长度BLOB也可能返回空指针，需结合类型和长度判断。
结果指针受下一次step/reset/finalize和类型转换的生命周期限制；例如`std::string.assign(pointer, length)`是应用主动复制，不是SQLite取列API必然执行的复制。[官方取列、转换及有效期](https://www.sqlite.org/c3ref/column_blob.html)

## 4. VM怎样执行内部指令

VDBE是一台寄存器式虚拟机：

| 内部对象 | 职责 | 官方源码 |
| --- | --- | --- |
| 指令数组aOp | 每条含opcode和P1至P5等参数；参数意义由opcode决定 | [vdbe.h:41](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbe.h#L41) |
| 寄存器aMem | 保存整数、浮点、字符串、BLOB、NULL等值及相关状态 | [vdbeInt.h](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbeInt.h) |
| 游标 | 表/索引遍历位置；不是一份完整结果集 | [VDBE结构](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbeInt.h#L380) |
| 程序位置pc | 指示从哪条指令继续；跳转、返回行会改变后续位置 | [执行循环](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbe.c#L775) |

3.37.2的sqlite3VdbeExec通过循环取指，在switch(opcode)中执行对应C代码。普通指令继续下一条，跳转指令改变位置，ResultRow保存恢复位置并返回SQLITE_ROW；下一次step继续执行，不从头扫描所有已返回行。[分派代码:852](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbe.c#L852)、[ResultRow:1511](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbe.c#L1511)

**一次step不等于一条VM指令**：它可以执行许多指令，直到返回一行、执行完成、出错或中断。[官方step](https://www.sqlite.org/c3ref/step.html)

## 5. 写入路径：编码记录，再修改B-tree页面

以下以普通rowid表为例：

```sql
CREATE TABLE component(name TEXT, x INTEGER);
INSERT INTO component VALUES(?,?);
```

3.37.2可用EXPLAIN查看指令。以下按实际控制流概括主要指令，不是通用于所有INSERT的固定程序：

```text
Init → Transaction → Goto
  → OpenWrite：打开目标表游标
  → Variable：绑定参数送入寄存器
  → NewRowid：取得新记录的rowid
  → MakeRecord：将字段编码成SQLite记录
  → Insert：调用B-tree写入
  → Halt：语句收尾
```

对应源码：[MakeRecord:3078](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbe.c#L3078)、[Insert:5242](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbe.c#L5242)。Insert调用sqlite3BtreeInsert，后者处理cell、页空间及必要的树平衡，并通过Pager请求页面可写。[B-tree插入](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/btree.c#L8801)、[PagerWrite](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/pager.c#L6148)

**写记录、写文件和事务提交是不同边界。** 显式BEGIN内INSERT返回DONE不等于COMMIT；隐式事务按语句完成规则提交。Pager按日志模式及同步设置组织写出，缓存压力下也可能在COMMIT之前写出页面。[官方事务](https://www.sqlite.org/lang_transaction.html)、[提交前缓存溢出](https://www.sqlite.org/atomiccommit.html#cache_spill_prior_to_commit)

## 6. 读取路径：游标推进，解码后返回一行

```sql
SELECT name,x FROM component;
```

```text
Init → Transaction → Goto
  → OpenRead：打开表游标
  → Rewind：定位首条记录，空表则结束
  → Column：从当前记录解码字段到寄存器
  → ResultRow：返回SQLITE_ROW
      应用调用sqlite3_column_*取得当前行
      应用再次调用step
  → Next：推进游标，有记录则跳回Column
  → Halt：返回SQLITE_DONE
```

对应源码：[Column:2634](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbe.c#L2634)、[Next:5930](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbe.c#L5930)。带索引条件、JOIN、排序或聚合时，计划会不同；排序可能先构造中间结果，不保证所有SELECT都能立即流式返回首行。[官方查询计划](https://www.sqlite.org/eqp.html)

SQLite C API没有单独的sqlite3_fetch：step得到ROW后用column接口取列。文本/BLOB指针可能因类型转换而失效；再次step、reset或finalize后也不应继续使用旧结果指针。需要跨越这些边界保存内容时，由调用方管理其副本。[官方取列及有效期](https://www.sqlite.org/c3ref/column_blob.html)

## 7. 底层存储：表不是文本行列表

本节只概括执行层关系；rowid、字符串主键、B-tree页面布局及缺页读取详见[btree_storage.md](btree_storage.md)。

| 结构 | 实际含义 |
| --- | --- |
| DB文件/页 | DB文件按固定大小页组织，页号从1开始；页大小为512至65536 B的2次幂，文件头占前100 B |
| rowid表B-tree | 以64位有符号rowid为键，表记录payload保存在叶页cell中 |
| 索引B-tree | 以编码后的键记录组织数据；普通rowid表索引含定位表记录所需的rowid |
| WITHOUT ROWID表 | 使用索引B-tree布局，以声明的主键组织记录，不使用普通rowid表布局 |
| cell与记录 | cell是B-tree页内的条目；记录payload包括描述各字段类型/长度的header和字段body，不是SQL文本 |
| overflow页 | payload超出页内可保存部分时，剩余内容使用溢出页链 |
| schema与空闲页 | sqlite_schema记录数据库对象定义及rootpage等信息；freelist管理可复用空闲页 |

官方：[数据库文件格式](https://www.sqlite.org/fileformat2.html)。普通表即使没有显式索引也有自身的B-tree；记录并非按C/C++对象的内存布局原样保存。

### B-tree、Pager、页缓存、VFS的分工

```text
B-tree：记录、键、游标、cell、树结构
    ↓ 请求页/修改页
Pager：页访问、事务状态、日志及提交/回滚协调
    ↔ pcache：缓存内存页
    ↓ 必要时请求文件操作
VFS：xRead / xWrite / xSync 等
    ↓
平台实现与操作系统
```

rollback journal保存恢复所需的旧页内容；WAL采用追加修改后页的方式，之后checkpoint写DB文件。它们是不同日志模式，不是每次写入都同时走两套日志。xWrite与xSync也不是一回事：前者写数据，后者请求同步。详细模式与时序以官方[原子提交](https://www.sqlite.org/atomiccommit.html)、[WAL](https://www.sqlite.org/wal.html)、[VFS](https://www.sqlite.org/vfs.html)为准。

## 8. 返回、复用与销毁

- ROW：当前结果行可读取；DONE：语句成功执行完；错误码须检查，不能一概当作结束。
- reset：恢复statement到可再次执行状态，**不清除绑定值**；clear_bindings才清除参数绑定。
- finalize：释放statement，可返回前次执行的错误；不是显式事务中的COMMIT替代。
- 连接关闭与statement销毁是不同资源操作，应用应正确释放statement及其他连接资源。

官方：[reset](https://www.sqlite.org/c3ref/reset.html)、[clear_bindings](https://www.sqlite.org/c3ref/clear_bindings.html)、[finalize](https://www.sqlite.org/c3ref/finalize.html)、[close](https://www.sqlite.org/c3ref/close.html)。
