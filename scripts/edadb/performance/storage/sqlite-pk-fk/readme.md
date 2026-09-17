# 主外键性能实验

本实验属于SQLite基线实验组，但独立于单表测试；不把不同数据量、对象恢复方式的时间直接相减。

## 阅读顺序

1. [结果](docs/results.md)：先看复测结论，写入和读取分别展示。
2. [方案](docs/test_plan.md)：两张表、三种schema、数据和对照变量。
3. [实现](docs/implementation.md)：伪代码、计时边界及源码行号。

```text
sqlite-pk-fk/
├── readme.md                 导航及运行命令
├── docs/
│   ├── test_plan.md          唯一实验定义
│   ├── implementation.md     代码与测量方法
│   └── results.md            唯一结果汇总及证据入口
├── pk_fk_benchmark.cpp       数据、映射、读写和检查
├── run_pk_fk.py              正确性调度、串行测时、统计
├── audit_pk_fk.py            原始计时及文件库复核
└── CMakeLists.txt            独立Release构建
```

## 状态与边界

- 主矩阵已按当前源码完成整批复测：88组正确性检查、275条正式计时（55组×5次）、55个文件库完整审计通过；比例扩展未运行。
- 主键存储布局对照已独立为[sqlite-rowid](../sqlite-rowid/readme.md)：固定PK/FK，只改变rowid/WITHOUT ROWID；尚未实现或运行，不并入本实验。
- 本轮使用迁移并补充注释后的Release/-O3程序，执行逻辑未改；先完成独立正确性检查，再串行运行1轮预热和5轮正式样本。结果及证据统一见[results](docs/results.md)。
- SQLite通用参数统一见[配置](../sqlite-reference/config.md)，不在本目录重复说明事务、日志和同步机制。
- 生成数据、DB、日志和二进制放仓库外；源码与结果解释保留在这里。结果中的本机证据链接不是随仓库分发的附件。

## 如何运行

从本目录执行。以下命令复现同一主矩阵；输出目录需使用新路径：

```bash
cmake -S . -B /tmp/iedadb_pk_fk_build_new \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-10
cmake --build /tmp/iedadb_pk_fk_build_new -j40

# 输出目录必须不存在；正确性可并行，性能样本串行。
python3 run_pk_fk.py --binary /tmp/iedadb_pk_fk_build_new/pk_fk_benchmark \
  --out /tmp/iedadb_pk_fk_main_new --stage main --runs 5 --settle 5 --check-jobs 4
python3 audit_pk_fk.py /tmp/iedadb_pk_fk_main_new
```

本轮完整采样流程约104分钟（含预热和等待），无索引组是主要耗时；文件库完整审计在性能采样结束后执行，避免干扰计时。扩大父子比例使用`--stage ratios`，需另行决定。
