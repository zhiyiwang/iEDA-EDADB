# SQLite存储结构：rowid、主键、B-tree与Pager

本页依据SQLite官方文档与3.37.2源码；[execution_flow.md](execution_flow.md)负责SQL执行时序，本页负责数据如何组织和访问。

## 1. 所有表都会自动加入rowid主键吗？

**不是所有表都有rowid；普通rowid表的底层键也不等于额外声明了一列SQL PRIMARY KEY。**

| 表定义 | 实际组织 |
| --- | --- |
| 普通表，没有声明主键 | 仍有64位有符号rowid，以它作为表B-tree的键 |
| 普通表，INTEGER PRIMARY KEY | 满足规则时该列是rowid别名，共用同一个键，不再为它另建唯一索引 |
| 普通表，TEXT PRIMARY KEY或一般复合主键 | 表仍以rowid组织，声明的主键通常通过 **额外唯一索引** 实施 |
| WITHOUT ROWID表 | 必须声明主键，直接按主键组织索引B-tree，没有隐藏rowid |
| 虚表 | 存储由模块实现，不能套用普通表B-tree规则 |

`INTEGER PRIMARY KEY`要求特定的INTEGER类型声明；`INT PRIMARY KEY`不是rowid别名。列级`INTEGER PRIMARY KEY DESC`有历史兼容例外。普通表的rowid可用rowid、_rowid_、oid访问，但同名用户列会遮蔽对应名称。未由INTEGER PRIMARY KEY别名固定的rowid可能被VACUUM改变。
官方：[rowid表](https://www.sqlite.org/rowidtable.html)、[INTEGER PRIMARY KEY规则](https://www.sqlite.org/lang_createtable.html#rowids_and_the_integer_primary_key)、[WITHOUT ROWID](https://www.sqlite.org/withoutrowid.html)。

## 2. 字符串主键如何存储和查找

### 普通rowid表：表与唯一索引两棵树

```sql
CREATE TABLE component(name TEXT PRIMARY KEY NOT NULL, x INTEGER);
```

```text
① TEXT主键的唯一索引B-tree
   (name='U0', rowid=7)
                 │ 取得rowid=7
                 ↓
② 表B-tree：本身按rowid组织
   rowid=7 → 整行(name='U0', x=100)
```

**两棵树：一棵存表数据，一棵支持TEXT主键；不是“表数据之外，再额外建立两个索引”。**
例如`SELECT x FROM component WHERE name='U0'`，典型执行是先查①，再按rowid查②取得x。

- 声明的TEXT主键不是表B-tree的整数键；它由额外唯一索引支持。
- 准确说是“一棵存整行的表B-tree＋一棵TEXT主键的唯一索引B-tree”，不是表之外再加两份索引。表B-tree本身按rowid查找，无须第三份rowid索引。
- 按name查询x时，典型路径是先查索引取得rowid，再查表记录。
- 如果查询所需内容已由索引覆盖，则可以直接从索引返回，不必回表。
- 插入一行需要维护表和对应索引；索引键按字段值及collation规则比较，不是把字符串直接变成rowid。

官方以字符串主键给出相同的两棵树解释：[WITHOUT ROWID原理](https://www.sqlite.org/withoutrowid.html#benefits_of_without_rowid_tables)、[覆盖索引](https://www.sqlite.org/queryplanner.html#covering_indexes)、[索引记录比较](https://www.sqlite.org/fileformat2.html#record_sort_order)。

### WITHOUT ROWID：按主键组织

```sql
CREATE TABLE component(name TEXT PRIMARY KEY, x INTEGER) WITHOUT ROWID;
```

```text
一棵B-tree：直接按主键name组织整个表
   编码条目(name='U0', x=100)
                 │ 按name找到条目
                 ↓
             直接取得x=100
```

这里“索引B-tree”说的是SQLite底层的页面格式，不是给表另加一个普通二级索引。这棵树本身就是WITHOUT ROWID表的存储：编码条目包含name与x，以主键name确定记录的查找顺序，找到条目即可取x。不会再保存一份`rowid → 整行`的表树。

在这个没有其他索引的例子中，表从两棵树变成一棵树，没有“先取rowid再查表”的必要。其他二级索引使用主键列定位原记录，而不是隐藏rowid；宽主键可能增大这些索引。WITHOUT ROWID不是对所有表都更快，官方建议按记录大小和访问模式选择。[官方说明](https://www.sqlite.org/withoutrowid.html)

补充：普通非STRICT rowid表的非INTEGER主键存在历史允许NULL的行为，不能概括为所有PRIMARY KEY都自动NOT NULL；示例显式写NOT NULL。WITHOUT ROWID主键列强制NOT NULL。[主键约束](https://www.sqlite.org/lang_createtable.html#the_primary_key)

## 3. B-tree由哪些页面组成

```text
root page
  ├── interior page：键与子页编号
  │      ├── leaf page：cell、record payload
  │      └── leaf page
  └── interior/leaf page
```

小树可只有一个root leaf。每棵树有rootpage；数据库可同时容纳多棵表/索引树。

| 页/条目 | 布局与作用 |
| --- | --- |
| 页大小 | 固定页大小，512至65536 B的2次幂；第1页前100 B为数据库文件头 |
| B-tree页头 | 叶页8 B，内部页12 B；记录cell数量、空闲空间等，内部页另有最右子页编号 |
| cell指针数组 | 每项2 B，是页内偏移；按键顺序排列，cell字节本身不必物理连续有序 |
| table interior cell | 左子页编号与rowid分隔键；不存整条用户记录 |
| table leaf cell | payload长度、rowid、记录payload及必要的overflow指针 |
| index cell | 编码键记录；内部页还含子页编号，内部页也可包含键payload，不应把所有索引内容都理解成仅存在叶页 |
| record payload | header含字段serial type等，body含字段字节；不保存C/C++对象指针 |
| overflow page | 保存超出本地payload容量的部分，通过页号串联；与页缓存溢出CACHE_SPILL不是同一概念 |

官方：[B-tree页格式](https://www.sqlite.org/fileformat2.html#b_tree_pages)、[记录格式](https://www.sqlite.org/fileformat2.html#record_format)。页空闲区、freeblock及数据库freelist负责不同层次的可复用空间，不等同于业务记录。

### 一页是否就是一行？

不是。页是固定大小的存储/缓存单位，记录是可变长度的逻辑数据：一张表的叶页通常放多个cell，可容纳多条较小的完整记录；较大记录的payload会部分留在cell、其余存到overflow页。内部table页主要存分隔rowid和子页号，不存完整用户行；普通二级索引页存索引条目，不必包含整行所有列。[官方页面及payload布局](https://www.sqlite.org/fileformat2.html#b_tree_pages)

```text
表叶页：页头 | cell指针数组 | 空闲区 | cell(行A) | cell(行B) | ...
大行：  cell中的payload片段 → overflow页 → overflow页
```

**从“哪些页保存了某行内容”看，可以概括为多对多，但这是物理存储对应，不是SQL表间的多对多关系。**具体说：

- 一页可包含多行的cell；一行的payload也可跨本地页和多张overflow页。
- 每条记录仍从一个cell及其overflow链定位，并非任意分散在多个页中。
- overflow页链属于对应cell的payload，不是多行共享的一片通用缓冲区；内部table页则不存完整行。

例如P10包含行A的本地部分与完整行B；A的剩余内容在P11、P12。因此P10对应A/B，而A对应P10/P11/P12。[官方overflow布局](https://www.sqlite.org/fileformat2.html#cell_payload_overflow_pages)

## 4. 查询和写入怎样使用这棵树

- **查找**：从根页比较键，选择子页并继续；普通表按整数rowid比较，索引按编码记录及collation比较。
- **遍历**：游标保留路径和当前位置，Next推进，不必每次从根页重新查找已返回的行。
- **插入**：定位目标页、构造cell、分配页内空间；容量不足时按实现平衡/分配页面，通过Pager登记页面修改。
- **溢出记录**：需要内容时读取overflow页；不能将一行假定为固定大小或只占一个页。

3.37.2源码：[btree.c](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/btree.c)、[sqlite3BtreeInsert:8801](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/btree.c#L8801)、[insertCell:6863](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/btree.c#L6863)。

## 5. Pager缓存缺页后，是否自动读磁盘？

**对于需要已有内容的文件页，通常由Pager自动取得；但缓存未命中不等于一定访问物理磁盘。**

```text
B-tree请求页号
  → Pager/pcache有可用内容：直接返回缓存页
  → 没有可用内容：
      新页/明确不需要旧内容：初始化内容，不读旧页
      需要已有内容：
        WAL中有当前快照适用的页版本 → 读WAL frame
        否则 → 从DB文件对应偏移读取
  → 内容准备好后返回给B-tree；失败则返回错误
```

3.37.2的[getPageNormal:5470](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/pager.c#L5470)处理缓存和内容初始化；[readDbPage:2963](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/pager.c#L2963)先检查WAL，否则以`(页号-1)*page_size`从DB文件读取。

- 应用通常不需要自己另发一次“读页请求”；获取页的调用链完成后VM才能使用其内容。
- VFS读取可能由操作系统文件缓存满足，不代表磁盘设备真正读了一次。SQLite缓存缺页也不同于操作系统虚拟内存缺页。
- 启用mmap时可能使用映射路径，见[getPageMMap](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/pager.c#L5574)。内存数据库没有从DB文件补页这条路径。
- 读页是获取内容，不是xSync；sync属于文件同步保障。WAL读取必须遵守读事务快照，不能直接拿任意最新frame。

官方：[Pager与页缓存架构](https://www.sqlite.org/arch.html)、[WAL读取机制](https://www.sqlite.org/wal.html#how_wal_works)、[mmap](https://www.sqlite.org/mmap.html)、[内存数据库](https://www.sqlite.org/inmemorydb.html)。

## 6. Pager官方资料阅读顺序

**职责概括：B-tree知道键和记录；Pager按页管理内容、锁与事务；pcache缓存页；VFS执行文件操作。**
读时先获取缓存页，必要时取得DB/WAL中的页版本；写时管理脏页，并按模式协调日志、写出及提交/回滚。缓存未命中不等于物理磁盘读取，写出不等于同步持久化。[官方架构](https://www.sqlite.org/arch.html)

1. [Architecture：Page Cache](https://www.sqlite.org/arch.html)：Pager负责页读写、事务及锁；pcache负责内存缓存，VFS对接文件操作。
2. [File Locking：Overview](https://www.sqlite.org/lockingv3.html)：描述Pager怎样把文件视为编号页并提供事务/锁保障。本文主要讨论rollback模式；不能用其全部锁流程解释WAL。
3. [Atomic Commit](https://www.sqlite.org/atomiccommit.html)：rollback journal、写DB文件、同步与提交的先后关系，尤其第6.3节提交前缓存溢出。
4. [WAL：How WAL Works](https://www.sqlite.org/wal.html#how_wal_works)：补充WAL读快照、追加页及checkpoint，与rollback模式分开理解。
5. [3.37.2 pager.c](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/pager.c)：文件头的INVARIANTS、Pager状态说明，以及getPageNormal/readDbPage、pagerStress、CommitPhaseOne等实现。

Pager得到的是页号及页面内容请求，不负责解释SQL的字段或主键；“某页里哪一个cell属于目标行”由B-tree层处理。
