#!/usr/bin/env python3
"""Summarize an audited run outside the repository; does not rerun experiments.

  python3 summarize.py /tmp/iedadb_sqlite_index_full
"""
import csv
import json
from pathlib import Path
import statistics
import sys

LAYOUTS = ["none", "index", "unique", "text-pk", "text-pk-without-rowid",
           "integer-unique", "integer-rowid", "integer-without-rowid"]
PAIRS = [("index", "none"), ("unique", "index"), ("text-pk", "unique"),
         ("text-pk-without-rowid", "text-pk"), ("integer-rowid", "integer-unique"),
         ("integer-without-rowid", "integer-rowid")]


def main():
    output = Path(sys.argv[1]).resolve()
    audit = json.loads((output / "audit.json").read_text())
    if not audit["passed"]:
        raise RuntimeError("run has not passed audit")
    samples = json.loads((output / "samples.json").read_text())
    diagnostics = json.loads((output / "diagnostics.json").read_text())
    summary = list(csv.DictReader((output / "summary.tsv").open(), delimiter="\t"))
    count = int(audit["input"][0])

    def median(config, layout, phase):
        return float(next(row["median"] for row in summary if
                          (row["config"], row["layout"], row["phase"]) == (config, layout, phase)))

    lines = [
        "# SQLite单表主键与索引：实测结果", "",
        f"数据：{count:,}条Component、8字段；每组{audit['runs_per_group']}次，单位ms，表格使用中位数。",
        f"检查：{audit['correctness_cases']}组小规模正确性、{audit['formal_samples']}个正式样本、"
        f"{audit['diagnostic_full_field_checks']}组独立全量逐字段/约束/结构诊断通过。",
        f"整批运行时长：{audit['elapsed_s']/60:.1f}分钟。正式样本串行，Release -O3，系统SQLite；无逐行计时。", "",
        "## 1. 数据读写", "",
        "write不含init/create/BEGIN/COMMIT/close；scan取全8列；point为100次命中点查总时间。",
        "TEXT组按name点查，整数组按record_order点查；两组之间不可把键类型差值称为rowid独占开销。",
        "A为内存连接预热读取；B-batch为OS-warm文件、新连接读取，不是cold。", "",
        "| 配置 | 实现 | write data | 全读scan | 100次point |",
        "| --- | --- | ---: | ---: | ---: |",
    ]
    for config in ("A", "B-batch"):
        for layout in LAYOUTS:
            lines.append(f"| {config} | {layout} | {median(config, layout, 'write'):.3f} | "
                         f"{median(config, layout, 'scan'):.3f} | {median(config, layout, 'point'):.3f} |")
    lines += ["", "## 2. 建表、提交与空间", "",
              "create包含建表事务及空表索引创建；commit仅为数据事务COMMIT调用。它不是全部I/O或纯sync耗时。",
              "空间为page_count×page_size；文件库对应DB文件页空间，内存库不是进程RSS。", "",
              "| 配置 | 实现 | create ms | commit ms | DB页空间 B |",
              "| --- | --- | ---: | ---: | ---: |"]
    for config in ("A", "B-batch"):
        for layout in LAYOUTS:
            space = [item["space"][2] for item in samples if item["config_name"] == config and item["layout"] == layout]
            lines.append(f"| {config} | {layout} | {median(config, layout, 'create'):.3f} | "
                         f"{median(config, layout, 'commit'):.3f} | {int(statistics.median(space)):,} |")
    lines += ["", "## 3. 同轮写入差值", "",
              "先逐轮相减，再取中位数；不是两个中位数相减。正值表示前者更慢。差值包含布局、约束、缓存等净变化，不是某函数独占耗时。", "",
              "| 配置 | 前者 − 参照 | Δ中位数 ms | Δ最小–最大 ms | Δ ns/记录 |",
              "| --- | --- | ---: | ---: | ---: |"]
    for config in ("A", "B-batch"):
        for changed, reference in PAIRS:
            def values(layout):
                return {item["repeat"]: item["times"]["write"] for item in samples
                        if item["config_name"] == config and item["layout"] == layout}
            modified, baseline = values(changed), values(reference)
            delta = [modified[repeat] - baseline[repeat] for repeat in sorted(baseline)]
            center = statistics.median(delta)
            lines.append(f"| {config} | {changed} − {reference} | {center:.3f} | "
                         f"{min(delta):.3f}–{max(delta):.3f} | {center*1e6/count:.1f} |")
    lines += ["", "## 4. 实际访问计划与计数", "",
              "下表为独立A配置诊断；完整B配置、VM及树信息见diagnostics。计数不是毫秒或CPU指令数。", "",
              "| 实现 | 点查EQP | INSERT VM步数 | 全读VM步数 | 100次点查FULLSCAN_STEP |",
              "| --- | --- | ---: | ---: | ---: |"]
    for layout in LAYOUTS:
        item = next(item for item in diagnostics if item["config_name"] == "A" and item["layout"] == layout)
        counters = {(phase, name): int(value) for phase, name, value in item["counters"]}
        plan = "; ".join(row[-1] for row in item["eqp"] if row[0] == "EQP point")
        lines.append(f"| {layout} | {plan} | {counters['write', 'vm_step']:,} | "
                     f"{counters['scan', 'vm_step']:,} | {counters['point', 'fullscan_step']:,} |")
    lines += ["", "## 5. 原始证据", "",
              "- [原始阶段计时](samples.tsv)、[统计含min/max](summary.tsv)、[样本配置及空间](samples.json)。",
              "- [正确性](checks.json)、[完整审计](audit.json)、[独立诊断](diagnostics.json)、[构建及硬件](manifest.json)。",
              "- 正式timing和独立diagnostics的log分别保存；诊断TIME不用于统计。", ""]
    (output / "report.md").write_text("\n".join(lines))
    print(output / "report.md")


if __name__ == "__main__":
    main()
