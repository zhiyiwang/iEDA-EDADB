#!/usr/bin/env python3
"""Summarize native and EDADB iRT variability without inventing a pass tolerance."""

from __future__ import annotations

import argparse
import hashlib
import itertools
import json
import math
import re
import statistics
import sys
from pathlib import Path
from typing import Iterable


FINAL_VIOLATION_PATTERN = re.compile(
    r"\|\s+Total\s+\|\s+(\d+)\s+\|\s+(\d+)\s+\|\s+(\d+)\s+\|\s+(\d+)\s+\|\s+(\d+)\s+\|"
)
RUN_RT_ELAPSED_PATTERN = re.compile(
    r"RTInterface\.cpp:162 .* runRT\] Completed \(elapsed = (\d+):(\d+):(\d+),"
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def final_violation_total(log_path: Path) -> int:
    matches = FINAL_VIOLATION_PATTERN.findall(log_path.read_text(encoding="utf-8", errors="replace"))
    if not matches:
        raise ValueError(f"cannot find final iRT violation summary in {log_path}")
    return int(matches[-1][-1])


def run_rt_elapsed_seconds(log_path: Path) -> int:
    matches = RUN_RT_ELAPSED_PATTERN.findall(log_path.read_text(encoding="utf-8", errors="replace"))
    if not matches:
        raise ValueError(f"cannot find final iRT runtime in {log_path}")
    hours, minutes, seconds = map(int, matches[-1])
    return hours * 3600 + minutes * 60 + seconds


def load_run(run_dir: Path) -> dict[str, object]:
    summary = json.loads((run_dir / "feature/summary.json").read_text(encoding="utf-8"))
    design = summary["Design Statis"]
    nets = summary["Nets"]
    metrics: dict[str, int | float] = {
        "wire_len": nets["wire_len"],
        "num_wire": nets["num_wire"],
        "num_segment": nets["num_segment"],
        "num_via": nets["num_via"],
        "num_patch": nets["num_patch"],
        "final_drc_violation_total": final_violation_total(run_dir / "run.log"),
    }
    for layer in summary["Layers"]["routing_layers"]:
        name = layer["layer_name"]
        metrics[f"routing_layer.{name}.wire_len"] = layer["wire_len"]
        metrics[f"routing_layer.{name}.wire_num"] = layer["wire_num"]
        metrics[f"routing_layer.{name}.patch_num"] = layer["patch_num"]
    for layer in summary["Layers"]["cut_layers"]:
        metrics[f"cut_layer.{layer['layer_name']}.via_num"] = layer["via_num"]
    return {
        "name": run_dir.name,
        "post_tool_def_sha256": sha256(run_dir / "post_tool.def"),
        "structure": {
            "num_instances": design["num_instances"],
            "num_iopins": design["num_iopins"],
            "num_nets": design["num_nets"],
            "num_pdn": design["num_pdn"],
            "num_layers": design["num_layers"],
            "num_layers_routing": design["num_layers_routing"],
            "num_layers_cut": design["num_layers_cut"],
        },
        "routing_metrics": metrics,
        "runtime_metrics": {
            "run_rt_elapsed_seconds": run_rt_elapsed_seconds(run_dir / "run.log"),
        },
    }


def exact_two_sided_permutation_p_value(native_values: list[float], edadb_values: list[float]) -> float:
    pooled_values = native_values + edadb_values
    native_size = len(native_values)
    observed_delta = abs(statistics.mean(native_values) - statistics.mean(edadb_values))
    tolerance = max(1.0, observed_delta) * 1e-12
    extreme_count = 0
    partition_count = 0
    for native_indices in itertools.combinations(range(len(pooled_values)), native_size):
        native_index_set = set(native_indices)
        permuted_native = [pooled_values[index] for index in native_indices]
        permuted_edadb = [
            value for index, value in enumerate(pooled_values) if index not in native_index_set
        ]
        permuted_delta = abs(statistics.mean(permuted_native) - statistics.mean(permuted_edadb))
        extreme_count += permuted_delta + tolerance >= observed_delta
        partition_count += 1
    return extreme_count / partition_count


def group_statistics(native_values: list[int | float], edadb_values: list[int | float]) -> dict[str, object]:
    native_float = list(map(float, native_values))
    edadb_float = list(map(float, edadb_values))
    native_mean = statistics.mean(native_float)
    edadb_mean = statistics.mean(edadb_float)
    mean_delta = edadb_mean - native_mean
    native_variance = statistics.variance(native_float) if len(native_float) > 1 else 0.0
    edadb_variance = statistics.variance(edadb_float) if len(edadb_float) > 1 else 0.0
    pooled_standard_deviation = math.sqrt((native_variance + edadb_variance) / 2.0)
    return {
        "native_mean": native_mean,
        "edadb_mean": edadb_mean,
        "mean_delta": mean_delta,
        "mean_delta_percent": 100.0 * mean_delta / native_mean if native_mean else None,
        "native_sample_standard_deviation": math.sqrt(native_variance),
        "edadb_sample_standard_deviation": math.sqrt(edadb_variance),
        "standardized_mean_difference": (
            mean_delta / pooled_standard_deviation if pooled_standard_deviation else 0.0
        ),
        "exact_two_sided_permutation_p_value": exact_two_sided_permutation_p_value(
            native_float, edadb_float
        ),
    }


def statistical_design(native_count: int, edadb_count: int) -> dict[str, int | float | None]:
    permutation_partition_count = math.comb(native_count + edadb_count, native_count)
    return {
        "permutation_partition_count": permutation_partition_count,
        "minimum_attainable_two_sided_p_value_for_equal_groups": (
            2.0 / permutation_partition_count if native_count == edadb_count else None
        ),
    }


def observed_ranges(
    native_runs: list[dict[str, object]], edadb_runs: list[dict[str, object]], metric_group: str
) -> dict[str, object]:
    metric_names = sorted(native_runs[0][metric_group])
    result: dict[str, object] = {}
    for name in metric_names:
        native_values = [run[metric_group][name] for run in native_runs]
        edadb_values = [run[metric_group][name] for run in edadb_runs]
        native_min = min(native_values)
        native_max = max(native_values)
        result[name] = {
            "native": native_values,
            "edadb": edadb_values,
            "native_observed_min": native_min,
            "native_observed_max": native_max,
            "edadb_outside_native_observed_range": [
                value for value in edadb_values if value < native_min or value > native_max
            ],
            **group_statistics(native_values, edadb_values),
        }
    return result


def main(argv: Iterable[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("stage_root", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(list(argv))

    native_dirs = sorted(args.stage_root.glob("native-[0-9]*"))
    edadb_dirs = sorted(args.stage_root.glob("edadb-[0-9]*"))
    if not native_dirs or not edadb_dirs:
        parser.error("stage_root must contain native-N and edadb-N run directories")

    native_runs = [load_run(path) for path in native_dirs]
    edadb_runs = [load_run(path) for path in edadb_dirs]
    structures = [run["structure"] for run in (*native_runs, *edadb_runs)]
    native_count = len(native_runs)
    edadb_count = len(edadb_runs)
    design = statistical_design(native_count, edadb_count)
    report = {
        "stage_root": str(args.stage_root.resolve()),
        "sample_count": {"native": native_count, "edadb": edadb_count},
        "statistical_design": design,
        "all_fixed_structure_equal": all(value == structures[0] for value in structures[1:]),
        "native_post_tool_def_stable": len({run["post_tool_def_sha256"] for run in native_runs}) == 1,
        "edadb_post_tool_def_stable": len({run["post_tool_def_sha256"] for run in edadb_runs}) == 1,
        "runs": {"native": native_runs, "edadb": edadb_runs},
        "routing_metric_observed_ranges": observed_ranges(
            native_runs, edadb_runs, "routing_metrics"
        ),
        "runtime_metric_observed_ranges": observed_ranges(
            native_runs, edadb_runs, "runtime_metrics"
        ),
        "interpretation": (
            "Observed ranges and exploratory group statistics are descriptive, not acceptance tolerances. "
            f"With {native_count} native and {edadb_count} EDADB runs, the exact two-sided permutation "
            f"test enumerates {design['permutation_partition_count']} partitions. These samples cannot prove "
            "distributional equivalence; strict "
            "native/EDADB wrapped-input equality remains the adapter gate."
        ),
    }
    output = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output, encoding="utf-8")
    else:
        sys.stdout.write(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
