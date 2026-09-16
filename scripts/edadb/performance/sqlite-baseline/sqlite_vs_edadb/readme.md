# SQLite与EDADB：直接API开销对照

**已冻结**：测试实现与结论经用户确认；后续修改须先获得明确同意，规则见[AGENTS.md](AGENTS.md)。新实验放在独立目录，不覆盖本批结果。

只比较固定8字段Component的直接SQLite与EDADB API，不涉及adapter。**当前主实验是SQL/API对齐实验**，首轮结果只供追溯。

## 阅读顺序

1. [alignment.md](alignment.md)：先看读、写各组增加什么操作，SQL、源码对应及计时边界。
2. [results.md](results.md)：当前时间、delta、每条成本、结论和未证实的部分；开会可直接打开这一页。
3. [alignment.cpp](alignment.cpp#L96)：核对控制变量及API调用；[run_alignment.py](run_alignment.py)负责运行，[audit_alignment.py](audit_alignment.py)负责审计。
4. 要追溯首轮实验才看[archive/initial_checks.md](archive/initial_checks.md)，不与当前批次混算。

## 目录职责

```text
sqlite_vs_edadb/
├── readme.md                 导航，不重复时间表
├── alignment.md              当前实验方法、源码对应、唯一运行说明
├── results.md                当前结果、分析、原始证据
├── alignment.cpp             当前独立C++测试入口
├── CMakeLists.txt            构建alignment
├── run_alignment.py          正确性、串行采样、统计
├── audit_alignment.py        核验原始日志、统计及SQL
├── archive/
│   └── initial_checks.md     首轮两个独立批次的结果与复现说明
├── run_read.py               首轮读NULL实验runner
└── run_write.py              首轮写绑定/clear实验runner
```

保留脚本和C++路径，避免改变已有命令、源代码行号和结果快照；只调整文档职责。
首轮runner调用共用stream_benchmark，当前runner调用独立alignment，不能互换。

- schema/生成器：[共用支持头](../benchmark/benchmark_support.h#L111)。
- SQLite配置：[config.md](../sqlite_params/config.md)；机制及官方依据：[runtime.md](../sqlite_params/runtime.md)。
- 五路线总体性能：[冻结基线](../baseline/results.md)，本次不修改。
- 运行命令只维护在[方法页](alignment.md#4-构建运行与审计)；结果目录只维护在[结果页](results.md#原始数据与复核)。生成数据和二进制在仓库外，不提交。
