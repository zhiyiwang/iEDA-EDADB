# 五路线全量性能报告

## 完成情况与证据

2026-09-10 19:42–21:00（Asia/Shanghai），约78分钟。**退出码0；99组正确性检查、540条正式计时、108个分组各5次及最终审计全部通过。**
本文所有数值只使用本次完整批次，不拼接其他运行。

- [原始计时samples.tsv](/tmp/iedadb_stream_full_20260910/samples.tsv)：每次read/write分段时间、log路径、时间戳和load。
- [统计summary.tsv](/tmp/iedadb_stream_full_20260910/summary.tsv)：平均值、中位数、min/max。
- [统计及完整原始计时report.md](/tmp/iedadb_stream_full_20260910/report.md)。
- [正确性checks.json](/tmp/iedadb_stream_full_20260910/checks.json) / [审计audit.json](/tmp/iedadb_stream_full_20260910/audit.json)。
- [输入datasets.json](/tmp/iedadb_stream_full_20260910/datasets.json) / [版本、硬件与哈希manifest.json](/tmp/iedadb_stream_full_20260910/manifest.json)。
- [编译参数](/tmp/iedadb_stream_full_20260910/compile_flags.txt) / [链接命令](/tmp/iedadb_stream_full_20260910/link_command.txt) / [源码快照](/tmp/iedadb_stream_full_20260910/source/)。
- [构建日志](/tmp/iedadb_stream_full_20260910.build.log) / [运行日志](/tmp/iedadb_stream_full_20260910.log) / [退出码](/tmp/iedadb_stream_full_20260910.exit)。

测量实现统一见[implementation.md](implementation.md)，实验规则见[test_plan.md](test_plan.md)，参数见[sqlite_config.md](sqlite_config.md)，接续工作见[handoff.md](handoff.md)。

## 数据与运行

- 合成8字段COMPONENT：1,000、10,000、100,000、1,000,000条；0/1/8条另做正确性检查。B-default仅1,000条。
- 文件位于输出目录的n数量/下：bench.lef、canonical.def、每次运行.data和.log。内存库没有DB文件。
- 1,000,000条：canonical.def为75,776,213 bytes；C++文本44,664,890 bytes；SQLite B-batch数据库43,732,992 bytes；adapter数据库75,517,952 bytes。
- Release g++-10，-O3/-DNDEBUG，SQL trace关闭；测量时父仓库prof-test@7b661baa2，core@90a5fb249加两行trace打印修改，但未启用。归档时两行提交为core@d6656f0；代码内容未变，原始manifest仍记录测量时版本。
- 40逻辑CPU、125 GiB RAM，启动时122 GiB可用；正式样本前load1为0.004–0.908。编译-j40，正确性最多5组并发，性能串行。
- 每组1次预热＋5次正式，样本前和write/read之间各等待5秒。memory为first/repeat；文件为OS-warm、新连接。
- 正确性独立进程逐字段检查；native/adapter严格DEF比较通过。性能不做逐字段比较，direct/text读取消费摘要仍在计时内。
- 链接存在LEF/DEF的ODR类型警告，见构建日志；未修改依赖，不宣称已解决该警告。

复现（新输出目录不能已存在）：

```bash
cd /home/zhiyiwang/cs/arch/eda/iEDA-EDADB
cmake -S scripts/edadb/performance/sqlite-baseline \
  -B /tmp/iedadb_stream_clean_build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++-10
cmake --build /tmp/iedadb_stream_clean_build -j40 --target stream_benchmark
set -o pipefail
python3 scripts/edadb/performance/sqlite-baseline/run_stream.py \
  --binary /tmp/iedadb_stream_clean_build/stream_benchmark \
  --out /tmp/iedadb_stream_full_rerun \
  --counts 1000 10000 100000 1000000 --runs 5 --settle 5 \
  2>&1 | tee /tmp/iedadb_stream_full_rerun.log
python3 scripts/edadb/performance/sqlite-baseline/audit_stream.py \
  /tmp/iedadb_stream_full_rerun
```

只复核本批次无需重跑：`python3 scripts/edadb/performance/sqlite-baseline/audit_stream.py /tmp/iedadb_stream_full_20260910`。

## 阶段时间：1,000,000条

单位ms，以下为5次样本中位数。**create单列，不计入write/read。**
分项中位数不能保证相加等于complete中位数；阶段占比应先逐样本计算再汇总。

### 写入

| 实现 | init | create | BEGIN | write data | COMMIT | close | write complete |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| C++文本 | 0.109 | — | — | 748.833 | — | 0.021 | 748.833 |
| 原生iEDA | 包含于data | — | — | 702.185 | — | 包含于data | 702.185 |
| SQLite A | 0.124 | 0.160 | 0.002 | 1510.430 | 1.408 | — | 1511.874 |
| SQLite A-no-journal | 0.128 | 0.108 | 0.002 | 1515.544 | 1.406 | — | 1516.931 |
| SQLite B-batch | 0.118 | 78.141 | 0.010 | 1597.901 | 715.792 | 0.430 | 2316.542 |
| EDADB A | 0.129 | 0.188 | 0.002 | 1603.358 | 13.598 | — | 1616.961 |
| EDADB A-no-journal | 0.137 | 0.128 | 0.002 | 1603.744 | 13.485 | — | 1617.187 |
| EDADB B-batch | 0.134 | 74.585 | 0.011 | 1685.538 | 714.206 | 0.383 | 2391.578 |
| adapter | 1415.042 | 包含于init | 包含于data | 5813.067 | 包含于data | 0.288 | 5813.067 |

- complete=data+适用的BEGIN/COMMIT，不含init/create/close。B-default提交已在data中，不能填0冒充没有提交。
- adapter init包含打开、注册和建表，**没有独立测得create**；adapter data包含内部family事务。不能将它与direct排除提交的data作纯转换成本比较。
- 内存库写后保持同一连接，write close为—，最终关闭计在read close。native文件打开关闭位于API内，原始字段的0不是免费操作。
- B-batch完整数据写入中，COMMIT占比中位数为SQLite **31.01%**、EDADB **29.86%**；先按每个样本计算，再取中位数。
- 若以已测write阶段总和init+create+begin+data+commit+close为分母，SQLite B-batch：data约66.71%、COMMIT29.99%、create3.27%；adapter：混合init约19.80%、data约80.20%。这不是完整进程时间分布，计时外LEF/输入准备没有包含。

### 读取

| 实现 | read init | read data | read close | read data min–max |
| --- | ---: | ---: | ---: | ---: |
| C++文本 | 0.026 | 622.220 | 0.009 | 620.219–628.212 |
| 原生iEDA | 包含于data | 4843.321 | 包含于data | 4811.415–4849.885 |
| SQLite A | — | 825.183 | 5.757 | 819.951–831.917 |
| SQLite A-no-journal | — | 835.204 | 5.738 | 828.342–838.996 |
| SQLite B-batch | 0.138 | 844.419 | 0.256 | 842.235–858.069 |
| EDADB A | — | 1095.802 | 4.271 | 1095.417–1135.463 |
| EDADB A-no-journal | — | 1097.338 | 4.419 | 1095.735–1098.549 |
| EDADB B-batch | 0.176 | 1141.248 | 0.253 | 1127.029–1143.620 |
| adapter | 0.163 | 4730.975 | 0.185 | 4712.844–4781.578 |

内存配置取first-read；repeat数据在完整统计文件。读取表不重复计算write阶段的建表。
adapter read仅createDbByEdadb，不扫描参考DEF；恢复完整iDB。direct读取只覆盖同一个Record，不能把两者差值全算成封装成本。

## 规模趋势

每格为write data / read data中位数，单位ms。direct取B-batch；adapter write含内部提交，其余direct write不含外层提交。

| 条数 | C++文本 | 原生iEDA | SQLite | EDADB直接 | adapter |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1,000 | 1.062 / 0.810 | 0.976 / 12.492 | 3.255 / 1.957 | 3.481 / 2.651 | 176.385 / 13.234 |
| 10,000 | 9.116 / 7.411 | 6.611 / 49.898 | 31.174 / 18.026 | 30.704 / 24.319 | 260.318 / 76.639 |
| 100,000 | 73.496 / 60.565 | 66.812 / 461.563 | 230.626 / 117.859 | 225.686 / 143.631 | 850.670 / 472.102 |
| 1,000,000 | 748.833 / 622.220 | 702.185 / 4843.321 | 1597.901 / 844.419 | 1685.538 / 1141.248 | 5813.067 / 4730.975 |

## 比较结论与依据

1. **本例未证明SQLite全量顺序存取比简单文本快。**1,000,000条SQLite B-batch数据阶段，write为C++文本2.134倍、read为1.357倍。文本未执行等价持久化同步，不将这组数值当作相同durability比较。
2. **EDADB直接路径的额外成本以读更明显。**同schema、同B-batch配置，write净增87.637 ms（5.48%），read净增296.829 ms（35.15%）。这是整个路径净差，未独立计量traversal/字符串/分配各占比。
3. **A比B-batch的数据阶段差距不大。**SQLite A的write低5.47%、read低2.28%；较大差异在create和COMMIT。配置事实是A内存库/MEMORY日志/OFF同步/8192 KiB缓存预算，B文件库/DELETE/FULL/2000 KiB；多个因素一起改变，因此“不能全归因磁盘”是实验控制条件的限制，不是时间值证明了某个内部原因。
4. **adapter读取与原生iEDA在1,000,000条接近。**4730.975对4843.321 ms，比例0.977；不能推广为所有规模都更快，10,000条本批次adapter为76.639、native为49.898 ms。前者构建完整iDB，不能直接与844.419 ms的SQLite逐行Record消费比较。
5. **adapter写入仍明显慢，但原因未被分项量化。**5813.067 ms为native的8.279倍。源码明确包含Shadow vector构造/释放、实际schema写入及内部提交；当前没有这些部分的独立计时，不把差值武断分配给其中一项。
6. **自动提交成本必须单独看事务组。**1,000条write complete：SQLite B-default 55434.749、B-batch 83.859 ms；EDADB B-default 54271.159、B-batch 83.888 ms。两组实际PRAGMA一致，仅数据事务边界不同；差值是事务机制整体效应，不能称为纯fsync时间。大规模B-default未测。

### SQL与对象工作量并不相同

- direct：component单表8列，无显式PK/FK/index；全量SELECT不加ORDER BY，读复用Record。
- adapter：数据库41张表，iInstSD有19列、name主键；Instance读取按_order_sd排序，每行创建Instance、恢复cell master/坐标等并加入列表。
- 对本批次DB的等价查询`EXPLAIN QUERY PLAN SELECT * FROM iInstSD ORDER BY _order_sd`返回`SCAN iInstSD`和`USE TEMP B-TREE FOR ORDER BY`。这是只读计划检查，不是逐语句执行时间；全表读取出现SCAN本身并不等于索引缺失bug。
- 实现位置：[Instance写入](../../../../src/database/manager/builder/def_builder/def_write_edadb.cpp#L330)、[Instance读取](../../../../src/database/manager/builder/def_builder/def_read_edadb.cpp#L680)、[schema](../../../../src/database/edadb/idb/edadb_idb_schema.h#L91)。
- adapter实际foreign_keys=1，B-direct为0；其他已记录文件配置相同。不同schema/排序/对象语义和约束设置意味着并非单变量adapter实验。

## 尚未测到与讨论顺序

- 先审阅同批次阶段表与原始样本，再决定是否继续研究direct的约35%读取净增量。
- adapter的独立create、内部提交、SQL读取与对象恢复分项仍未测。若要分解，另行确认最小计时方案；本次不重新加入已撤销的插桩。
- 未运行disk os-cold/repeat、文本等价fsync、计时开关扰动对照；不承诺测量零扰动，不凭格式化小数位声称精度。
- 没有测磁盘峰值带宽或CPU硬件计数器，文件大小/耗时只能称表观吞吐。
- 仅覆盖8字段合成COMPONENT，不代表复杂Net/Via、全部DEF tag或点工具端到端性能。不改生产代码，不自动推进任何优化。
