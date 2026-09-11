# 主外键性能实验记录

## 同代码复测

按review要求，不修改测试C++、runner、adapter或core，复用同一个Release二进制。
数据仍为10,000个Component、100,000个Pin；预热1次、正式5次，性能串行，整批约105分钟。
88组正确性检查、275条计时（55组×5次）、55个文件库全字段及完整性审计全部PASS。
本批与下文首批独立统计，不混合样本；仅完成main，未运行ratios扩展。

复测命令与下文相同，使用另一个尚不存在的输出目录。各类结果文件见“原始证据”；
本报告保留实测汇总，生成产物保留本地，不随Git提交。

以下为FK ON、graph路径的中位数，单位均为ms。read采用memory repeat或disk warm；
write只含数据写入，不含init、create、外层COMMIT。各列中位数不能相加当成总时间中位数。

| 存储 | API | 子表 | create | write | COMMIT | read |
| --- | --- | --- | ---: | ---: | ---: | ---: |
| memory | SQLite | pk | 0.265 | 364.083 | 0.056 | 107.698 |
| memory | SQLite | index | 0.310 | 358.842 | 0.056 | 107.575 |
| memory | SQLite | none | 0.239 | 252.662 | 0.033 | 119299.620 |
| memory | EDADB | pk | 0.270 | 352.556 | 0.055 | 118.680 |
| memory | EDADB | index | 0.308 | 356.460 | 0.058 | 119.387 |
| memory | EDADB | none | 0.247 | 240.036 | 0.034 | 122402.840 |
| disk | SQLite | pk | 75.681 | 425.800 | 189.661 | 148.732 |
| disk | SQLite | index | 76.196 | 433.435 | 192.027 | 149.743 |
| disk | SQLite | none | 72.517 | 312.715 | 151.738 | 126557.256 |
| disk | EDADB | pk | 71.435 | 414.468 | 187.531 | 158.931 |
| disk | EDADB | index | 75.593 | 416.027 | 192.747 | 158.882 |
| disk | EDADB | none | 72.366 | 299.447 | 155.414 | 126813.471 |

复测结论：
- 区别在于每写一个Pin，index还维护一条复合索引记录，pk还要求组合唯一；none没有这项child索引维护，但仍有父PK和FK检查。SQLite disk相对none：index写入净增120.720ms（38.60%），pk净增113.085ms（36.16%）。这是整条路径的净差值，不是索引函数独立耗时。
- 本批disk的pk写入反而略快于index，与首批的排序相反；不能把两者差值解释成精确的唯一性检查时间。稳定结论是两种索引均增加写入成本，而有索引的关联读取显著快于none。
- 有索引时EDADB read相对SQLite：memory多约10.20%–10.98%，disk多约6.10%–6.86%。这是API路径净增量，包含对象恢复、分配和包装等，不能全部归因于traversal。
- 无索引read仍需约119–127秒，而有索引约108–159毫秒；memory也慢，不能把问题只归因于磁盘。schema证据和查询方式见下文。
- 保留了原SQLite成功路径构造错误字符串的代码，故小幅写入差异仍有包装器成本与波动限制。未为获得更好结果修改实现。

## 批次与运行

首轮整批运行约104分钟。
88组独立正确性检查、275条正式计时（55组×5次）、时间统计审计全部通过；
性能结束后另对55个正式文件库逐字段校验，并执行integrity_check/foreign_key_check，全部通过。
SQLite/EDADB的DDL、SQL、参数与索引列定义一致。无需修改adapter或EDADB core。
首轮包含10,000个父对象、100,000个子对象；两条API、三种schema、两种存储及FK-OFF/flat控制。
本轮完成--stage main；比例扩展--stage ratios尚未运行。无索引首轮一次关联读取约120秒，
100,000父的扩展可能显著增加运行时间，留待review后执行，不据此伪称所有比例已经验证。

```bash
cd /home/zhiyiwang/cs/arch/eda/iEDA-EDADB
cmake -S scripts/edadb/performance/sqlite-pk-fk \
  -B /tmp/iedadb_pk_fk_build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-10
cmake --build /tmp/iedadb_pk_fk_build -j40
# 必须使用尚不存在的输出目录；check并发，性能样本串行。
python3 scripts/edadb/performance/sqlite-pk-fk/run_pk_fk.py \
  --binary /tmp/iedadb_pk_fk_build/pk_fk_benchmark \
  --out /tmp/iedadb_pk_fk_main_rerun --stage main --runs 5 --settle 5 --check-jobs 4
# 全部性能测量结束后，独立复核时间统计和正式文件库的每个字段。
python3 scripts/edadb/performance/sqlite-pk-fk/audit_pk_fk.py /tmp/iedadb_pk_fk_main_rerun
```

运行时可将终端输出重定向到自行指定的日志文件；结果目录由--out指定。
编译Release -O3/-DNDEBUG，关闭core SQL trace；40逻辑CPU、125 GiB RAM，启动约122 GiB可用。
父仓库prof-test@4f49ce07b加本目录源码快照，core@d6656f0；样本启动时load1为0.037–1.277。
同一系统SQLite 3.37.2；编译日志有core模板的unused-parameter告警，不修改生产代码消除。

## 原始证据

以下文件位于每次运行的--out目录内：

- `samples.tsv`：每次分阶段计时，单位ms。
- `summary.tsv`：均值、中位数、min/max。
- `report.md`：统计表与原始计时汇总。
- `checks.json`：正确性、实际DDL、索引、SQL、EXPLAIN与SELECT执行次数。
- `manifest.json`：源码/二进制哈希、版本、硬件；source目录为启动时快照。
- `audit.json`：275条计时/55组统计复核。
- `database_audit.json`：55个文件库逐字段核验、字节数和哈希。

生成的DB、输入、日志、统计不提交Git。代码、方法和本报告结论保留在仓库。

## 写入：建表、数据、提交分别看

以下均为5次正式样本中位数，单位ms；FK=ON，graph主对照。create包含两表及必要索引，
不包含在write中；完整数据写入为逐样本BEGIN+write+COMMIT。各项中位数不能直接相加冒充完整中位数。

| API | 存储 | 子表 | create | write | COMMIT |
| --- | --- | --- | ---: | ---: | ---: |
| SQLite | memory | pk | 0.263 | 365.881 | 0.055 |
| SQLite | memory | index | 0.317 | 360.806 | 0.055 |
| SQLite | memory | none | 0.244 | 254.000 | 0.033 |
| EDADB | memory | pk | 0.266 | 350.367 | 0.057 |
| EDADB | memory | index | 0.306 | 353.119 | 0.059 |
| EDADB | memory | none | 0.246 | 239.949 | 0.034 |
| SQLite | disk | pk | 78.961 | 434.190 | 191.244 |
| SQLite | disk | index | 61.068 | 414.553 | 176.988 |
| SQLite | disk | none | 77.337 | 316.150 | 158.525 |
| EDADB | disk | pk | 78.251 | 423.805 | 190.160 |
| EDADB | disk | index | 84.347 | 426.047 | 194.526 |
| EDADB | disk | none | 76.497 | 301.476 | 147.774 |

SQLite memory的index比none写数据多42.05%，pk多44.05%；这是索引维护等净成本，不是单独计时的B-tree函数。
pk与index的差异明显小于二者与none的差异，不据此声称主键总是更快或更慢。
disk写入有较大波动，例如SQLite index为391.890–440.096ms，EDADB index为393.522–450.881ms。
同DDL的建表中位数也会波动；create均由相同生成DDL交给SQLite执行，不是不同建表算法。

## 读取：同样恢复一个父对象及其pins

单位ms；memory同时列first与repeat，disk为OS-warm的新连接。此表没有建表或COMMIT时间。

| API | 子表 | memory first | memory repeat | disk warm |
| --- | --- | ---: | ---: | ---: |
| SQLite | pk | 107.515 | 107.720 | 146.579 |
| SQLite | index | 108.500 | 108.485 | 149.989 |
| SQLite | none | 120605.277 | 119203.567 | 127407.542 |
| EDADB | pk | 119.362 | 119.717 | 160.716 |
| EDADB | index | 119.291 | 119.469 | 161.864 |
| EDADB | none | 121867.368 | 121748.762 | 128257.876 |

**本输入下，child索引是关联读取的决定性因素。**SQLite disk中none/index约849倍，
none/pk约869倍；EDADB分别约792倍、798倍。不是数据库所有工作负载都具有这个加速比。
none已经使用内存库仍约120秒，不能把慢的原因仅归为磁盘读取。

实际子查询（两条API相同，无ORDER BY）：

```sql
SELECT "pin_name", "direction", "x", "y", "component_name"
FROM "component_pins_instance_pin" WHERE "component_name" = ?;
```

check中EXPLAIN结果：

```text
pk:    SEARCH component_pins_instance_pin USING INDEX sqlite_autoindex_component_pins_instance_pin_1 (component_name=?)
index: SEARCH component_pins_instance_pin USING INDEX component_pins_instance_pin__owner_name_idx (component_name=?)
none:  SCAN component_pins_instance_pin
```

每个父对象执行一次子查询。100父check实际trace为101次SELECT；主数据按同一循环执行10,001次SELECT，
正式计时不启用trace。none需反复扫描100,000条子记录，约10,000×100,000次行检查；
pk/index按父键定位该父的10条子记录。索引不消除N+1查询次数，只改变每次访问代价。
正式文件库示例：pk/index均6,164,480 bytes，none为3,551,232 bytes；索引有空间成本。

## 两条API、FK开关及全表读取对照

同schema的EDADB减SQLite，读取使用memory repeat/disk warm：

| 子表 | memory read增量 | disk read增量 |
| --- | ---: | ---: |
| pk | +11.996ms（+11.14%） | +14.136ms（+9.64%） |
| index | +10.983ms（+10.12%） | +11.875ms（+7.92%） |
| none | +2545.195ms（+2.14%） | +850.334ms（+0.67%） |

有索引时EDADB的整条读取路径比本SQLite实现多约8%–11%；不是独立测得的traversal时间。
none的增量处于较大扫描波动内：disk SQLite为126764.722–133051.910ms，EDADB为125741.281–136750.209ms，
不把0.67%解读成准确的框架成本。

写入没有一致的EDADB慢趋势，例如disk pk差-10.386ms，index差+11.494ms，波动区间重叠。
还需注意SQLite测试包装器的Statement::insert在成功路径也构造错误说明字符串，该成本计入write；
因此这些净差值不能证明EDADB固有写入速度优于最精简SQLite实现，更不能用负差值推导负的框架开销。
本轮没有修改该路径重测，后续若重点讨论小幅写入差异，应先精简包装器并统一复测。

固定pk，仅切换FK检查，write数据阶段：

| API | 存储 | FK OFF | FK ON | ON相对OFF |
| --- | --- | ---: | ---: | ---: |
| SQLite | memory | 292.750 | 365.881 | +24.98% |
| EDADB | memory | 280.006 | 350.367 | +25.13% |
| SQLite | disk | 377.977 | 434.190 | +14.87% |
| EDADB | disk | 349.420 | 423.805 | +21.29% |

该对照保留同一DDL/索引/事务，只改变FK开启状态；反映成功插入时约束检查的净影响。
普通SELECT不是逐行FK校验，FK开关的读取差异不作约束校验成本解释。

SQLite fetch-only中位数（不恢复对象图）：

| 子表 | memory repeat | disk warm |
| --- | ---: | ---: |
| pk | 80.614 | 121.506 |
| index | 80.655 | 113.552 |
| none | 80.467 | 119.263 |

两次全表SELECT没有反复按父查找，none也很快；但它不构建父子对象关系，不能直接替代graph读取。
下一步若研究批量读取，应加上分组挂接和对象恢复，再与当前graph路径比较。

## 结论与后续范围

1. 已验证三种schema及SQLite/EDADB首轮读写正确性，最显著差异来自child查询有无索引。
2. 普通复合索引与复合PK读性能接近，只有PK提供本实验的父子组合唯一性约束；不能用性能代替语义选择。
3. 索引与FK检查增加写入成本；create、COMMIT已单列。memory/disk同时改变存储/缓存/日志/同步，不把差值全算磁盘I/O。
4. 尚未运行父子比例扩展、cold、真正批量对象恢复或生产工具；不将本例推广为所有iEDA设计结论。
5. 全部结果保留真实波动和包装器限制；不编造内部模块耗时，不修改生产代码，不自动commit/push。
