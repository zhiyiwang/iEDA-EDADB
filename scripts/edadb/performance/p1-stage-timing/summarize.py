#!/usr/bin/env python3
"""Audit sequential ON/OFF samples and summarize command/stage times in ms."""

import csv
import statistics
import sys
from pathlib import Path


def summarize(root):
    samples = []
    for setting in ("off", "on"):
        directory = root / setting
        with (directory / "result.tsv").open() as source:
            rows = list(csv.DictReader(source, delimiter="\t"))
        assert len(rows) == 10, (setting, "expected 10 measured samples")
        assert {(row["cache_mode"], row["sample"]) for row in rows} == {
            (cache, f"run-{index}") for cache in ("cold", "warm") for index in range(1, 6)
        }
        for row in rows:
            cache, sample = row["cache_mode"], row["sample"]
            sample_dir = directory / cache / sample
            assert (sample_dir / "native.def").read_bytes() == (sample_dir / "edadb.def").read_bytes()
            for field in ("native_def_read_us", "native_def_write_us", "edadb_read_us", "edadb_write_us"):
                samples.append((setting, cache, sample, field[:-3], float(row[field]) / 1000))
            for operation in ("write", "read"):
                stages = {}
                for line in (sample_dir / f"{operation}.log").read_text().splitlines():
                    if not line.startswith("EDADB_STAGE\t"):
                        continue
                    _, logged_operation, phase, unit, value = line.split("\t")
                    assert setting == "on" and logged_operation == operation and unit == "ns"
                    assert phase not in stages
                    stages[phase] = int(value)
                if setting == "on":
                    assert set(stages) == {"init", "create", operation, "other", "command"}, stages
                    assert all(value >= 0 for value in stages.values())
                    assert stages["command"] == sum(value for phase, value in stages.items() if phase != "command")
                    for phase, value in stages.items():
                        samples.append((setting, cache, sample, f"stage_{operation}_{phase}", value / 1_000_000))
    with (root / "samples.tsv").open("w") as output:
        writer = csv.writer(output, delimiter="\t")
        writer.writerow(("timing", "cache", "sample", "metric", "time_ms"))
        writer.writerows(samples)
    groups = {}
    for setting, cache, sample, metric, value in samples:
        groups.setdefault((setting, cache, metric), []).append(value)
    summary = []
    for key, values in sorted(groups.items()):
        assert len(values) == 5
        summary.append((*key, len(values), statistics.mean(values), statistics.median(values), min(values), max(values)))
    with (root / "summary.tsv").open("w") as output:
        writer = csv.writer(output, delimiter="\t")
        writer.writerow(("timing", "cache", "metric", "count", "mean_ms", "median_ms", "min_ms", "max_ms"))
        writer.writerows(summary)
    medians = {row[:3]: row[5] for row in summary}
    report = ["# EDADB分段计时结果", "", "单位ms；每组5次中位数；预热不参与统计。", "",
              "## 数据读写（ON同批）", "",
              "EDADB仅使用Data阶段，排除init/create。native仍为def_init/def_save命令时间，内部初始化尚未单拆；此列为参照，不是严格同边界结果。",
              "", "| 缓存 | 操作 | native现有DEF命令参照 | EDADB数据阶段 |",
              "| --- | --- | ---: | ---: |"]
    for cache in ("cold", "warm"):
        for operation in ("read", "write"):
            native = medians["on", cache, f"native_def_{operation}"]
            data = medians["on", cache, f"stage_{operation}_{operation}"]
            report.append(f"| {cache} | {operation} | {native:.3f} | {data:.3f} |")
    report += ["", "## 初始化与建表（不加入数据读写）", "",
               "native内部init未单测，数据库create不适用；未测不等于0。", "",
               "| 缓存 | EDADB操作 | init ms | create ms |",
               "| --- | --- | ---: | ---: |"]
    for cache in ("cold", "warm"):
        for operation in ("read", "write"):
            init = medians["on", cache, f"stage_{operation}_init"]
            create = medians["on", cache, f"stage_{operation}_create"]
            report.append(f"| {cache} | {operation} | {init:.3f} | {create:.3f} |")
    report += ["", "## 完整命令仅用于ON/OFF扰动检查", "",
               "| 缓存 | 操作 | EDADB OFF ms | EDADB ON ms | ON/OFF变化 |",
               "| --- | --- | ---: | ---: | ---: |"]
    for cache in ("cold", "warm"):
        for operation in ("read", "write"):
            off = medians["off", cache, f"edadb_{operation}"]
            on = medians["on", cache, f"edadb_{operation}"]
            report.append(f"| {cache} | {operation} | {off:.3f} | {on:.3f} | {(on/off-1)*100:+.2f}% |")
    report += ["", "## ON阶段分布", "", "占比为各样本stage/command的平均值；不是中位数相加。", "",
               "| 缓存 | 操作 | 阶段 | 均值ms | 中位数ms | 最小ms | 最大ms | 平均占比 |",
               "| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |"]
    for cache in ("cold", "warm"):
        for operation in ("write", "read"):
            totals = groups["on", cache, f"stage_{operation}_command"]
            for phase in ("init", "create", operation, "other", "command"):
                values = groups["on", cache, f"stage_{operation}_{phase}"]
                share = statistics.mean(value / total * 100 for value, total in zip(values, totals))
                report.append(f"| {cache} | {operation} | {phase} | {statistics.mean(values):.3f} | {statistics.median(values):.3f} | {min(values):.3f} | {max(values):.3f} | {share:.2f}% |")
    report += ["", "审计：20个正式样本严格DEF相等，ON阶段非负且加总一致，OFF无阶段日志。",
               "原始数据：[samples.tsv](samples.tsv)；统计：[summary.tsv](summary.tsv)。",
               "write含内部事务及转换；read含对象组装；均不含init/create。",
               "cold使用文件级缓存驱逐提示，不代表设备缓存完全冷却。ON/OFF差值包含批次噪声，不是计时器独占成本。"]
    (root / "report.md").write_text("\n".join(report) + "\n")


if __name__ == "__main__":
    summarize(Path(sys.argv[1]))
