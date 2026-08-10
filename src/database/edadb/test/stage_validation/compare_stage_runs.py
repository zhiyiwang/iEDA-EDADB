#!/usr/bin/env python3
"""Compare native and EDADB stage-validation artifacts."""

from __future__ import annotations

import argparse
import importlib.util
import json
import re
import sys
from pathlib import Path
from typing import Any, Iterable


VOLATILE_JSON_KEYS = {"flow_memory", "flow_runtime"}
UNDEFINED_ITO_TOOL_FIELDS = {"HPWL", "STWL"}
VOLATILE_REPORT_PATTERNS = (
    re.compile(r"^Time\s*:.*$", re.MULTILINE),
    re.compile(r"^\| Runtime\s*\|.*$", re.MULTILINE),
    re.compile(r"^\| Memmory\s*\|.*$", re.MULTILINE),
)


def load_normalizer(script_path: Path):
    spec = importlib.util.spec_from_file_location("edadb_normalize_def", script_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load DEF normalizer: {script_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def canonical_json(value: Any, path: tuple[str, ...] = ()) -> Any:
    if isinstance(value, dict):
        return {
            key: canonical_json(child, (*path, key))
            for key, child in sorted(value.items())
            if key not in VOLATILE_JSON_KEYS
            and not (path and path[0] in {"optDrv", "optHold", "optSetup"} and key in UNDEFINED_ITO_TOOL_FIELDS)
        }
    if isinstance(value, list):
        return [canonical_json(child, path) for child in value]
    return value


def normalized_report(path: Path) -> str:
    text = path.read_text(encoding="utf-8")
    for pattern in VOLATILE_REPORT_PATTERNS:
        text = pattern.sub("", text)
    return "\n".join(line.rstrip() for line in text.splitlines() if line.strip())


def normalized_verilog(path: Path) -> str:
    lines = path.read_text(encoding="utf-8").splitlines()
    if lines and lines[0].startswith("//Generate the verilog at "):
        lines[0] = "//Generate the verilog at <timestamp> by iSTA."
    return "\n".join(line.rstrip() for line in lines) + "\n"


def compare_value(label: str, expected: Any, actual: Any, failures: list[str]) -> None:
    if expected != actual:
        failures.append(label)


def compare_runs(reference: Path, candidate: Path, normalizer_path: Path, pre_only: bool) -> list[str]:
    normalizer = load_normalizer(normalizer_path)
    failures: list[str] = []

    compare_value(
        "pre-tool DEF",
        normalizer.normalize_file(reference / "pre_tool.def"),
        normalizer.normalize_file(candidate / "pre_tool.def"),
        failures,
    )
    compare_value(
        "pre-tool DB report",
        normalized_report(reference / "pre_tool_db.rpt"),
        normalized_report(candidate / "pre_tool_db.rpt"),
        failures,
    )
    if pre_only:
        return failures

    compare_value(
        "post-tool DEF",
        normalizer.normalize_file(reference / "post_tool.def"),
        normalizer.normalize_file(candidate / "post_tool.def"),
        failures,
    )
    compare_value(
        "post-tool Verilog",
        normalized_verilog(reference / "post_tool.v"),
        normalized_verilog(candidate / "post_tool.v"),
        failures,
    )
    compare_value(
        "post-tool DB report",
        normalized_report(reference / "report/post_tool_db.rpt"),
        normalized_report(candidate / "report/post_tool_db.rpt"),
        failures,
    )
    for relative_path, label in (
        (Path("feature/tool.json"), "point-tool feature JSON"),
        (Path("feature/summary.json"), "feature summary JSON"),
    ):
        expected = canonical_json(json.loads((reference / relative_path).read_text(encoding="utf-8")))
        actual = canonical_json(json.loads((candidate / relative_path).read_text(encoding="utf-8")))
        compare_value(label, expected, actual, failures)
    return failures


def main(argv: Iterable[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--normalizer", type=Path, required=True)
    parser.add_argument("--pre-only", action="store_true")
    args = parser.parse_args(list(argv))

    failures = compare_runs(args.reference, args.candidate, args.normalizer, args.pre_only)
    if failures:
        print("FAIL: artifact differences: " + ", ".join(failures), file=sys.stderr)
        return 1
    print("PASS: stage artifacts are equivalent")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
