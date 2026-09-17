# SQLite源码与实测热点对应

## 1. 查看哪个版本

以下源码用于解释测试路径；测试程序单独编译，未重新编译SQLite或替换系统动态库。

- 运行库：`libsqlite3-0 3.37.2-2ubuntu0.7`；测试记录的source ID以`872ba256…alt1`结尾，不能用最新SQLite源码代替。
- 对应Ubuntu源码（已应用发行版补丁）：`/home/zhiyiwang/cs/db/sqlite/ubuntu-3.37.2-2ubuntu0.7`。
- 官方上游源码：`/home/zhiyiwang/cs/db/sqlite/upstream-3.37.2`，tag `version-3.37.2`，Git提交`eecb3e6b161b37c0484f0049ff3e6813f8177d41`。
- Ubuntu源码包从[Ubuntu镜像目录](https://mirrors.tuna.tsinghua.edu.cn/ubuntu/pool/main/s/sqlite3/)取得；`.dsc`及三个归档保存在上述sqlite目录。三个SHA256与`.dsc`一致；本机缺少签名公钥，未完成PGP签名验证。源码包版本匹配不等于已进行二进制可重现构建验证。

以下`src/...:行号`以Ubuntu源码根目录为准。官方链接固定到3.37.2；本次关键调用位置已与本地源码核对，不引用不断变化的master行号。

## 2. 写入：step具体多做什么

测试在[benchmark.cpp](../benchmark.cpp#L190)复用一个INSERT：每行bind八字段、step、reset。prepare/finalize计入data，外层BEGIN/COMMIT单列。

| 执行层 | 源码位置 | 相比顺序输出文本，多做的工作 |
| --- | --- | --- |
| API入口 | `src/vdbeapi.c:761`，mutex进入`:771`，执行VM`:716` | 连接互斥、statement状态管理，进入已编译SQL程序；不是每条重新解析SQL |
| VM操作 | `src/vdbe.c:3078` OP_MakeRecord；`:5242` OP_Insert | 字段编码为SQLite记录，然后执行表插入；本次EXPLAIN中还有Variable/NewRowid/OpenWrite |
| 表与页 | `src/vdbe.c:5306` → `src/btree.c:8801` sqlite3BtreeInsert；`:9038` insertCell | 维护rowid B-tree、组织cell/页面，必要时分配空间或平衡；无显式索引仍然有表B-tree |
| 页修改 | `src/btree.c:6863` insertCell → `src/pager.c:6148` sqlite3PagerWrite | 标记可写/脏页，处理需要的日志与页面状态；调用PagerWrite不等于立即写文件或sync |
| 语句结束 | `src/vdbe.c:1152` → `src/vdbeaux.c:3019` sqlite3VdbeHalt | 收尾游标、状态和事务检查；不是每次Halt都提交数据事务 |

官方源码：[step入口](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbeapi.c#L761)、[VM](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbe.c#L3078)、[B-tree](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/btree.c#L8801)。

**实测对应**：见[结果](results.md)中的写入CPU热点。step和内部VdbeExec、VdbeHalt、BtreeInsert是嵌套Children占比，不能相加，也不能据此给出每个函数精确毫秒数。

### bind成本不应算给step

`src/vdbeapi.c:1385`的bindText进入`src/vdbemem.c:1102`的sqlite3VdbeMemSetStr；TRANSIENT分支`:1143`准备存储并在`:1157`执行memcpy。STATIC分支保存指针，不要求这次复制，但仍有绑定状态管理。

本实验只改变两个字符串绑定所有权，净差值统一见[结果](results.md)，不是memcpy独占时间。输入必须一直有效到重绑/finalize，不能推广为任意场景均可STATIC。
官方：[绑定生命周期](https://www.sqlite.org/c3ref/bind_blob.html)、[具体实现](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbemem.c#L1143)。

## 3. sync：谁触发，是否在本次step里

**先区分两个阶段：上层Pager决定是否同步；下层VFS/操作系统执行同步。**

```text
路径一：显式COMMIT（本实验单列，不属于write data）
  sqlite3VdbeHalt → vdbeCommit
  → sqlite3BtreeCommitPhaseOne → sqlite3PagerCommitPhaseOne
  → 必要时syncJournal → 写DB文件脏页 → sqlite3PagerSync

路径二：插入过程中缓存压力（可能属于write data）
  需要缓存页 → pagerStress
  → 满足NEED_SYNC/状态条件时syncJournal → 写出该DB页面

实际同步后端：
  sqlite3OsSync → VFS xSync → unixSync → full_fsync
  → 按平台/构建分支调用fdatasync或fsync等
```

| 判据 | 对应源码 | 本实验能够得出的结论 |
| --- | --- | --- |
| 是否在语句结束提交 | `src/vdbeaux.c:3107`检查`db->autoCommit`；`:3127`才调用vdbeCommit | 外层BEGIN内的正常INSERT不逐行提交；Halt热点不能解释为每行fsync |
| 提交中的同步顺序 | `src/pager.c:6526` syncJournal；`:6557`写页；`:6580` sqlite3PagerSync | B的COMMIT可包含日志、写页、同步及收尾；已测744.687ms不能全命名为sync时间 |
| 提交前写出 | `src/pager.c:4530` pagerStress；`:4582`条件；`:4585` syncJournal；`:4591`写页 | B可能在INSERT的step里同步/写文件；源码支持可能性，但目前没有该批次的系统调用次数和等待时间 |
| A内存库 | `src/pager.c:4953`内存库不注册pagerStress；`:3560`/`:3565`设置noSync；`:6319`检查noSync | A的内存DB、MEMORY日志、OFF同步配置，不以磁盘sync解释其step CPU成本 |
| 日志同步条件 | `src/pager.c:4219`检查noSync，`:4221`排除MEMORY日志 | 进入syncJournal函数不代表一定调用文件同步 |
| 后端执行 | `src/os.c:99`调用xSync；`src/os_unix.c:3735` unixSync；`:3754` full_fsync | 持久化请求针对文件，而非SQL行；具体系统调用及次数需运行跟踪，不能仅凭函数名判断 |

官方：[提交前缓存溢出](https://www.sqlite.org/atomiccommit.html#cache_spill_prior_to_commit)、[同步配置](https://www.sqlite.org/pragma.html#pragma_synchronous)、[pagerStress实现](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/pager.c#L4530)、[提交路径](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/pager.c#L6379)、[VFS转发](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/os.c#L99)、[Unix同步](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/os_unix.c#L3735)。

这里的sync请求的是文件持久化及恢复协议所需顺序，不是线程mutex。perf中的pthread_mutex热点不能当成磁盘sync；当前采样事件是`cycles:u`，也不能量出阻塞等待磁盘的wall time。

## 4. 读取：step之外还有取列

- `src/vdbe.c:2634` OP_Column从记录解码字段；`:1511` OP_ResultRow返回一行；`:5930` OP_Next推进游标。
- 应用随后执行[fetch_record](../benchmark.cpp#L178)：两个字符串各取text及bytes，六个整数取值，共10次column API，再赋值和消费结果。
- `src/vdbeapi.c:1067` columnMem负责访问结果字段、列范围及连接互斥；后续columnMallocFailure处理错误并释放互斥。该路径不是仅返回一个C++成员。
- 当前读取CPU占比统一见[结果](results.md)；不能把全部读差距归于step，也没有证据表明正常全表SELECT在逐行sync。

官方：[OP_Column](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbe.c#L2634)、[取列实现](https://github.com/sqlite/sqlite/blob/version-3.37.2/src/vdbeapi.c#L1067)。

## 5. 结论与下一项可验证问题

已建立“测试阶段 → 实际采样 → 对应版本源码”的证据链：A写入首先是VM/记录与页处理，加上step外的绑定；读取必须同时看step和取列。源码没有证明实现存在bug，也没有证明sync是当前data差距的主要原因。

若继续定位B：在独立诊断运行中按write data与COMMIT边界记录`fsync/fdatasync/pwrite64`次数与持续时间，或用VFS包装器记录xSync/xWrite。跟踪有扰动，不能混入正式计时，也不能把所有文件写入称为sync。先验证实际调用，再决定是否增加单因素实验；当前未运行此项跟踪。
