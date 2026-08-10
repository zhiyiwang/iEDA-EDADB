#!/usr/bin/env python3
"""Summarize native and EDADB iRT variability without inventing a pass tolerance."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path
from typing import Iterable


FINAL_VIOLATION_PATTERN = re.compile(
    r"\|\s+Total\s+\|\s+(\d+)\s+\|\s+(\d+)\s+\|\s+(\d+)\s+\|\s+(\d+)\s+\|\s+(\d+)\s+\|"
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
    }


def observed_ranges(native_runs: list[dict[str, object]], edadb_runs: list[dict[str, object]]) -> dict[str, object]:
    metric_names = sorted(native_runs[0]["routing_metrics"])
    result: dict[str, object] = {}
    for name in metric_names:
        native_values = [run["routing_metrics"][name] for run in native_runs]
        edadb_values = [run["routing_metrics"][name] for run in edadb_runs]
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
    report = {
        "stage_root": str(args.stage_root.resolve()),
        "sample_count": {"native": len(native_runs), "edadb": len(edadb_runs)},
        "all_fixed_structure_equal": all(value == structures[0] for value in structures[1:]),
        "native_post_tool_def_stable": len({run["post_tool_def_sha256"] for run in native_runs}) == 1,
        "edadb_post_tool_def_stable": len({run["post_tool_def_sha256"] for run in edadb_runs}) == 1,
        "runs": {"native": native_runs, "edadb": edadb_runs},
        "routing_metric_observed_ranges": observed_ranges(native_runs, edadb_runs),
        "interpretation": (
            "Observed ranges are descriptive, not acceptance tolerances. Three samples cannot prove "
            "distributional equivalence; strict native/EDADB wrapped-input equality remains the adapter gate."
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
