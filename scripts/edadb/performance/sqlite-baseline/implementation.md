# 五条baseline的实现与计时

本文行号对应当前源码；实测数据对应结果目录manifest中的二进制及source快照。
数据规则见[test_plan.md](test_plan.md)，配置见[sqlite_config.md](sqlite_config.md)，运行命令与结果见[experiment_report.md](experiment_report.md)。

## 公共数据与时钟

- [benchmark_support.h:111](benchmark_support.h#L111)：ComponentRecord及TABLE4CLASS，单表8字段。
- [benchmark_support.h:133](benchmark_support.h#L133)：generate构造写入数据；[fixtures.py:15](fixtures.py#L15)生成LEF/DEF。
- [benchmark_support.h:33](benchmark_support.h#L33)、[44–48](benchmark_support.h#L44)：steady_clock及毫秒转换。耗时=end−start，包含调度等待，不是CPU时间。
- [stream_benchmark.cpp:9–24](stream_benchmark.cpp#L9)：Consumer消费整数和字符串长度；check模式才逐字段验证。
- [stream_benchmark.cpp:234–236](stream_benchmark.cpp#L234)：停止计时后核验条数/摘要并输出STREAM记录。

## 1. text：C++文本

代码：[stream_benchmark.cpp:181–198](stream_benchmark.cpp#L181)。

```text
计时外：准备输入Record vector
init：打开输出文件
write data：循环ofstream << 8字段 → flush → 检查输出状态
close：关闭输出文件
计时外：等待、预读并核验文件缓存
init：打开输入文件
read data：复用Record → ifstream解析8字段 → 消费摘要 → 检查EOF
close：关闭输入文件
```

write边界183–186行，read边界191–197行。read不保存结果vector；字符串赋值仍有成本。

## 2. native：原生iEDA DEF

调用及计时：[stream_benchmark.cpp:199–220](stream_benchmark.cpp#L199)。
封装：[benchmark_support.h:98–109](benchmark_support.h#L98)。
生产入口：[def_write.cpp:143](../../../../src/database/manager/builder/def_builder/def_write.cpp#L143)、[def_read.cpp:70](../../../../src/database/manager/builder/def_builder/def_read.cpp#L70)。

```text
计时外：加载LEF → 创建write service → 加载canonical.def得到完整iDB
write data：native_save → 构造DefWrite → writeDb → 返回值/输出文件检查
计时外：独立加载read侧LEF → 创建read service → 准备文件缓存
read data：native_load → 构造DefRead → createDb → 检查返回值
计时外：从iDB提取8字段；check模式输出DEF并做严格比较
```

write在205行、read在220行围住整个封装函数，不靠日志估算。
API内部文件打开/关闭、DEF解析及iDB对象恢复都计入data；native_save还包含文件存在检查。
LEF与输入iDB准备不计入。原始输出init/close=0是未单独测量的占位值，不代表这些工作免费。

## 3. sqlite：直接SQLite C API

实现：[stream_benchmark.cpp:64–94](stream_benchmark.cpp#L64)；外层计时见下方公共DB阶段。

```text
write data：prepare INSERT一次
            对每条Record：bind两字符串和六整数 → step(DONE) → reset
            finalize
read data：prepare SELECT一次 → 创建一个复用Record
           循环step(ROW)：column取8列 → 复制到Record → 消费摘要
           step(DONE) → finalize
```

两字符串绑定使用SQLITE_TRANSIENT；读取使用assign复制字符串，整数按类型读取。
没有独立fetch API调用；这里fetch指step返回ROW后column取值。
SELECT无参数，不需要bind；逐行继续step，不逐行reset。prepare/finalize均计入data，不细分其内部时间。

## 4. edadb：直接EDADB C++ API

实现：[stream_benchmark.cpp:51–63](stream_benchmark.cpp#L51)；映射设置：[130行](stream_benchmark.cpp#L130)。

```text
write data：makeInsertOp（不自带数据事务）
            循环writer.insert(record) → 销毁writer
read data：makeReadAllOp → 创建一个复用Record
           循环edadb::readNext(reader, &record) → 消费摘要 → 销毁reader
```

不经过iEDA adapter；使用与sqlite相同的8字段单表，hasPrimKey=false。
op构造及作用域结束时的析构包含在data_operation内，均被计时；EDADB内部临时对象和字符串成本也包含在内。
外层统一控制事务，不是每条记录重新构造insertObject操作。

### 两条direct的公共阶段

| 输出字段 | 范围 | stream_benchmark.cpp位置 |
| --- | --- | --- |
| init | 打开连接、配置及必要映射设置 | [124–140](stream_benchmark.cpp#L124)，read重开在163行 |
| create | CREATE TABLE及其建表事务 | [147–150](stream_benchmark.cpp#L147) |
| begin | batch的BEGIN | [151–153](stream_benchmark.cpp#L151) |
| write data | data_operation完整写调用 | [154–155](stream_benchmark.cpp#L154) |
| commit | batch的COMMIT | [156–158](stream_benchmark.cpp#L156) |
| read data | data_operation完整读调用 | [168–169](stream_benchmark.cpp#L168) |
| close | 关闭连接 | [161](stream_benchmark.cpp#L161)、[180](stream_benchmark.cpp#L180) |

内存库写后不关闭，同连接读；对应write.close/read.init为−1（N/A）。
文件库写后关闭，准备缓存，再打开读连接；repeat的预读在计时外（165–167行）。
配置打印、缓存核验及等待不计入data；不存在一个覆盖全部进程工作的total计时器。

## 5. adapter：应用层EDADB读写

实现：[stream_benchmark.cpp:207–232](stream_benchmark.cpp#L207)；访问封装：[benchmark_support.h:121–131](benchmark_support.h#L121)。
生产入口：[def_write_edadb.cpp:73](../../../../src/database/manager/builder/def_builder/def_write_edadb.cpp#L73)、[def_read_edadb.cpp:208](../../../../src/database/manager/builder/def_builder/def_read_edadb.cpp#L208)。

```text
计时外：LEF + canonical.def → write侧完整iDB
write init：initWriteDb（含打开数据库及建表）
计时外：构造AdapterWriter
write data：writeChip2Edadb，保留内部事务提交
close：closeDatabase
计时外：独立read侧LEF/service、缓存、AdapterReader和全局helper
read init：initReadDb
read data：createDbByEdadb（不扫描参考DEF）
close：closeDatabase
计时外：提取字段、校验；check模式输出DEF
```

以下均为stream_benchmark.cpp的当前行号，计时起点和终点分别列出：

| 阶段 | 起点 | 执行调用 | 终点 |
| --- | --- | --- | --- |
| write init | [204](stream_benchmark.cpp#L204) | initWriteDb：[207](stream_benchmark.cpp#L207) | [208](stream_benchmark.cpp#L208) |
| write data | [209](stream_benchmark.cpp#L209) | writeChip2Edadb：[210](stream_benchmark.cpp#L210) | [210](stream_benchmark.cpp#L210) |
| read init | [224](stream_benchmark.cpp#L224) | initReadDb：[224](stream_benchmark.cpp#L224) | [225](stream_benchmark.cpp#L225) |
| read data | [225](stream_benchmark.cpp#L225) | createDbByEdadb：[226](stream_benchmark.cpp#L226) | [227](stream_benchmark.cpp#L227) |
| read close | [227](stream_benchmark.cpp#L227) | closeDatabase：[228](stream_benchmark.cpp#L228) | [228](stream_benchmark.cpp#L228) |

字段提取在[231行](stream_benchmark.cpp#L231)，check模式DEF输出在[232行](stream_benchmark.cpp#L232)，均在读计时结束后。
write init合并初始化和建表，create为N/A；内部commit不能从data扣除。
此路线恢复完整iDB，而direct只覆盖复用Record，二者差值不能命名为纯adapter重建成本。

## 执行与结果字段

- [run_stream.py:103](run_stream.py#L103)：生成输入、核验Release选项、先并发正确性，再串行预热和正式采样。
- `--routes adapter`只选择adapter检查和采样；原生DEF规范化仍用于准备输入，不进入性能统计。不指定时运行全部路线。
- [run_stream.py:24](run_stream.py#L24)：启动每组子进程，将输出保存为.log，解析STREAM计时行；Python进程等待不作为C++读写耗时。
- [run_stream.py:61](run_stream.py#L61)：汇总samples.tsv、summary.tsv和report.md。原始日志路径随样本保存，可逐条追溯。
- [stream_benchmark.cpp:26–32](stream_benchmark.cpp#L26)：complete=data+适用的begin+commit。batch写比较用complete，autocommit的提交已包含在data；read complete等于data。

所有时间单位为ms；complete不含init/create/close，也不能与其data重复相加。
性能模式的direct/text摘要消费在read计时内；native/adapter的字段提取与摘要在计时外。
逐字段正确性比较使用独立check进程，不进入正式性能统计；这不等于整个测量没有时钟、消费或调度扰动。
