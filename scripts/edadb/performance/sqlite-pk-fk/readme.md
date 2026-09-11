# SQLite主外键性能实验

1. [test_plan.md](test_plan.md)：三种schema、两条API、数据与对照范围。
2. [implementation.md](implementation.md)：源码位置、读写伪代码、计时边界。
3. [experiment_report.md](experiment_report.md)：运行命令、结果路径及分析。

源码入口[pk_fk_benchmark.cpp](pk_fk_benchmark.cpp)，调度[run_pk_fk.py](run_pk_fk.py)，复核[audit_pk_fk.py](audit_pk_fk.py)。
本实验不修改单表基线或生产代码；生成产物存于仓库外。
