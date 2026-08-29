#!/usr/bin/env python3

import csv
import statistics
import sys
from collections import defaultdict
from pathlib import Path


COMMAND_METRICS = (
    "native_def_read_us",
    "native_def_write_us",
    "edadb_write_us",
    "edadb_read_us",
)


def tsv_writer(stream):
    return csv.writer(stream, delimiter="\t", lineterminator="\n")


def read_command_rows(path):
    rows = {}
    with open(path, newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream, delimiter="\t"):
            rows[(row["cache_mode"], row["sample"])] = {
                metric: int(row[metric]) for metric in COMMAND_METRICS
            }
    return rows


def read_command_results(path):
    values = defaultdict(list)
    for (cache_mode, _sample), row in read_command_rows(path).items():
        for metric in COMMAND_METRICS:
            values[(cache_mode, metric)].append(row[metric])
    return values


def write_overhead(baseline_path, profiled_path, output_path):
    baseline = read_command_results(baseline_path)
    profiled = read_command_results(profiled_path)
    with open(output_path, "w", newline="", encoding="utf-8") as stream:
        writer = tsv_writer(stream)
        writer.writerow((
            "cache_mode", "metric", "baseline_median_us", "profiled_median_us",
            "overhead_percent"))
        for key in sorted(baseline):
            baseline_median = statistics.median(baseline[key])
            profiled_median = statistics.median(profiled[key])
            overhead = (profiled_median / baseline_median - 1.0) * 100.0
            writer.writerow((
                key[0], key[1], f"{baseline_median:.1f}", f"{profiled_median:.1f}",
                f"{overhead:.2f}"))


def collect_profile_rows(profiled_dir):
    rows = []
    for cache_mode in ("cold", "warm"):
        cache_dir = Path(profiled_dir) / cache_mode
        if not cache_dir.is_dir():
            continue
        for sample_dir in sorted(cache_dir.glob("run-*")):
            for process in ("write", "read"):
                log_path = sample_dir / f"{process}.log"
                if not log_path.is_file():
                    continue
                with open(log_path, encoding="utf-8", errors="replace") as stream:
                    for line in stream:
                        if not line.startswith("EDADB_PROFILE\t"):
                            continue
                        fields = line.rstrip("\n").split("\t")
                        if len(fields) != 6:
                            raise RuntimeError(f"invalid profile record in {log_path}: {line.rstrip()}")
                        rows.append({
                            "cache_mode": cache_mode,
                            "sample": sample_dir.name,
                            "process": process,
                            "operation": fields[1],
                            "kind": fields[2],
                            "name": fields[3],
                            "calls": int(fields[4]),
                            "total_us": int(fields[5]),
                        })
    if not rows:
        raise RuntimeError("no EDADB_PROFILE records found; verify EDADB_ENABLE_PROFILING=ON")
    return rows


def write_raw_profile(rows, output_path):
    columns = ("cache_mode", "sample", "process", "operation", "kind", "name", "calls", "total_us")
    with open(output_path, "w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(
            stream, fieldnames=columns, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def write_metric_summary(rows, output_path):
    grouped = defaultdict(lambda: {"calls": [], "total_us": []})
    for row in rows:
        key = (row["cache_mode"], row["process"], row["operation"], row["kind"], row["name"])
        grouped[key]["calls"].append(row["calls"])
        grouped[key]["total_us"].append(row["total_us"])

    with open(output_path, "w", newline="", encoding="utf-8") as stream:
        writer = tsv_writer(stream)
        writer.writerow((
            "cache_mode", "process", "operation", "kind", "name",
            "median_calls", "median_total_us"))
        for key in sorted(grouped):
            values = grouped[key]
            writer.writerow((*key,
                f"{statistics.median(values['calls']):.1f}",
                f"{statistics.median(values['total_us']):.1f}"))


def write_phase_summary(rows, output_path):
    samples = defaultdict(lambda: defaultdict(int))
    for row in rows:
        key = (row["cache_mode"], row["sample"], row["process"], row["operation"])
        if row["kind"] == "phase" and row["name"] == "phase_total":
            samples[key]["phase_total_us"] += row["total_us"]
        elif row["kind"] == "metric":
            if row["name"].startswith("sqlite_"):
                samples[key]["sqlite_total_us"] += row["total_us"]
            elif row["name"].startswith("shadow_"):
                samples[key]["shadow_total_us"] += row["total_us"]
            elif row["name"] in ("core_read", "core_write"):
                samples[key]["core_total_us"] += row["total_us"]

    grouped = defaultdict(lambda: defaultdict(list))
    for key, values in samples.items():
        group_key = (key[0], key[2], key[3])
        phase_total = values["phase_total_us"]
        sqlite_total = values["sqlite_total_us"]
        shadow_total = values["shadow_total_us"]
        grouped[group_key]["phase_total_us"].append(phase_total)
        grouped[group_key]["core_total_us"].append(values["core_total_us"])
        grouped[group_key]["sqlite_total_us"].append(sqlite_total)
        grouped[group_key]["shadow_total_us"].append(shadow_total)
        grouped[group_key]["non_sqlite_nonshadow_us"].append(
            phase_total - sqlite_total - shadow_total)

    columns = (
        "phase_total_us", "core_total_us", "sqlite_total_us", "shadow_total_us",
        "non_sqlite_nonshadow_us")
    with open(output_path, "w", newline="", encoding="utf-8") as stream:
        writer = tsv_writer(stream)
        writer.writerow(("cache_mode", "process", "operation", *columns))
        for key in sorted(grouped):
            writer.writerow((*key, *(
                f"{statistics.median(grouped[key][column]):.1f}" for column in columns)))


def write_process_summary(rows, profiled_result_path, output_path):
    commands = read_command_rows(profiled_result_path)
    samples = defaultdict(lambda: defaultdict(int))
    for row in rows:
        key = (row["cache_mode"], row["sample"], row["process"])
        if row["kind"] == "phase" and row["name"] == "phase_total":
            samples[key]["phase_total_us"] += row["total_us"]
        elif row["kind"] == "metric":
            if row["name"].startswith("sqlite_"):
                samples[key]["sqlite_total_us"] += row["total_us"]
            elif row["name"].startswith("shadow_"):
                samples[key]["shadow_total_us"] += row["total_us"]
            elif row["name"] in ("core_read", "core_write"):
                samples[key]["core_total_us"] += row["total_us"]

    command_metric = {"write": "edadb_write_us", "read": "edadb_read_us"}
    grouped = defaultdict(lambda: defaultdict(list))
    for key, values in samples.items():
        cache_mode, sample, process = key
        command_total = commands[(cache_mode, sample)][command_metric[process]]
        phase_total = values["phase_total_us"]
        sqlite_total = values["sqlite_total_us"]
        shadow_total = values["shadow_total_us"]
        group = grouped[(cache_mode, process)]
        group["command_total_us"].append(command_total)
        group["phase_total_us"].append(phase_total)
        group["core_total_us"].append(values["core_total_us"])
        group["sqlite_total_us"].append(sqlite_total)
        group["shadow_total_us"].append(shadow_total)
        group["profiled_phase_residual_us"].append(
            phase_total - sqlite_total - shadow_total)
        group["non_sqlite_nonshadow_us"].append(
            command_total - sqlite_total - shadow_total)
        group["outside_profiled_phases_us"].append(command_total - phase_total)

    value_columns = (
        "command_total_us", "phase_total_us", "core_total_us", "sqlite_total_us",
        "shadow_total_us", "profiled_phase_residual_us", "non_sqlite_nonshadow_us",
        "outside_profiled_phases_us")
    with open(output_path, "w", newline="", encoding="utf-8") as stream:
        writer = tsv_writer(stream)
        writer.writerow((
            "cache_mode", "process", *value_columns,
            "sqlite_percent", "shadow_percent", "non_sqlite_nonshadow_percent"))
        for key in sorted(grouped):
            medians = {
                column: statistics.median(grouped[key][column]) for column in value_columns
            }
            command_total = medians["command_total_us"]
            writer.writerow((
                *key,
                *(f"{medians[column]:.1f}" for column in value_columns),
                f"{medians['sqlite_total_us'] / command_total * 100.0:.2f}",
                f"{medians['shadow_total_us'] / command_total * 100.0:.4f}",
                f"{medians['non_sqlite_nonshadow_us'] / command_total * 100.0:.2f}"))


def write_api_summary(rows, profiled_result_path, output_path):
    commands = read_command_rows(profiled_result_path)
    samples = defaultdict(lambda: defaultdict(int))
    for row in rows:
        if row["kind"] != "metric":
            continue
        if not (row["name"].startswith("sqlite_") or row["name"].startswith("shadow_")):
            continue
        key = (row["cache_mode"], row["sample"], row["process"], row["name"])
        samples[key]["calls"] += row["calls"]
        samples[key]["total_us"] += row["total_us"]

    command_metric = {"write": "edadb_write_us", "read": "edadb_read_us"}
    grouped = defaultdict(lambda: defaultdict(list))
    command_values = defaultdict(list)
    for (cache_mode, sample), values in commands.items():
        for process, metric in command_metric.items():
            command_values[(cache_mode, process)].append(values[metric])

    for key, values in samples.items():
        cache_mode, _sample, process, name = key
        group = grouped[(cache_mode, process, name)]
        group["calls"].append(values["calls"])
        group["total_us"].append(values["total_us"])

    with open(output_path, "w", newline="", encoding="utf-8") as stream:
        writer = tsv_writer(stream)
        writer.writerow((
            "cache_mode", "process", "api", "median_calls", "median_total_us",
            "command_percent"))
        for key in sorted(grouped):
            cache_mode, process, _name = key
            median_calls = statistics.median(grouped[key]["calls"])
            median_total = statistics.median(grouped[key]["total_us"])
            command_total = statistics.median(command_values[(cache_mode, process)])
            writer.writerow((
                *key,
                f"{median_calls:.1f}",
                f"{median_total:.1f}",
                f"{median_total / command_total * 100.0:.4f}"))


def main():
    if len(sys.argv) != 5:
        raise SystemExit(
            "Usage: summarize_profile.py BASELINE_RESULT PROFILED_RESULT PROFILED_DIR OUTPUT_DIR")
    baseline_result, profiled_result, profiled_dir, output_dir = sys.argv[1:]
    output = Path(output_dir)
    output.mkdir(parents=True, exist_ok=True)
    write_overhead(baseline_result, profiled_result, output / "overhead.tsv")
    rows = collect_profile_rows(profiled_dir)
    write_raw_profile(rows, output / "profile.tsv")
    write_metric_summary(rows, output / "metric_summary.tsv")
    write_phase_summary(rows, output / "phase_summary.tsv")
    write_process_summary(rows, profiled_result, output / "process_summary.tsv")
    write_api_summary(rows, profiled_result, output / "api_summary.tsv")


if __name__ == "__main__":
    main()
