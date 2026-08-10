#!/usr/bin/env python3
"""Compare semantic and pointer-order views of iRT's wrapped input database."""

from __future__ import annotations

import argparse
import json
import sys
from collections.abc import Iterable
from pathlib import Path
from typing import Any


def load_snapshot(path: Path) -> dict[str, Any]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(payload, dict) or payload.get("schema_version") != 1:
        raise ValueError(f"unexpected iRT input snapshot: {path}")
    if not isinstance(payload.get("semantic_database"), dict):
        raise ValueError(f"missing semantic_database: {path}")
    if not isinstance(payload.get("pointer_order_views"), dict):
        raise ValueError(f"missing pointer_order_views: {path}")
    return payload


def first_differences(reference: Any, candidate: Any, path: str = "$", limit: int = 12) -> list[str]:
    differences: list[str] = []

    def visit(native: Any, edadb: Any, current_path: str) -> None:
        if len(differences) >= limit:
            return
        if type(native) is not type(edadb):
            differences.append(
                f"{current_path}: type differs native={type(native).__name__} "
                f"edadb={type(edadb).__name__}"
            )
            return
        if native == edadb:
            return
        if isinstance(native, dict):
            native_keys = set(native)
            edadb_keys = set(edadb)
            for key in sorted(native_keys - edadb_keys):
                differences.append(f"{current_path}.{key}: missing from EDADB")
            for key in sorted(edadb_keys - native_keys):
                differences.append(f"{current_path}.{key}: added by EDADB")
            for key in sorted(native_keys & edadb_keys):
                visit(native[key], edadb[key], f"{current_path}.{key}")
            return
        if isinstance(native, list):
            if len(native) != len(edadb):
                differences.append(
                    f"{current_path}: length differs native={len(native)} edadb={len(edadb)}"
                )
            for index, (native_item, edadb_item) in enumerate(zip(native, edadb)):
                visit(native_item, edadb_item, f"{current_path}[{index}]")
            return
        if native != edadb:
            differences.append(f"{current_path}: native={native!r} edadb={edadb!r}")

    visit(reference, candidate, path)
    return differences


def normalized_fixed_rect_groups(groups: Any) -> Any:
    if not isinstance(groups, list):
        return groups
    normalized = []
    for group in groups:
        if not isinstance(group, dict) or not isinstance(group.get("rects"), list):
            normalized.append(group)
            continue
        normalized_group = dict(group)
        normalized_group["rects"] = sorted(
            group["rects"], key=lambda value: json.dumps(value, sort_keys=True)
        )
        normalized.append(normalized_group)
    return normalized


def compare(reference_path: Path, candidate_path: Path) -> tuple[list[str], list[str]]:
    reference = load_snapshot(reference_path)
    candidate = load_snapshot(candidate_path)
    semantic_failures = first_differences(
        reference["semantic_database"],
        candidate["semantic_database"],
        "$.semantic_database",
    )

    native_pointer_views = reference["pointer_order_views"]
    edadb_pointer_views = candidate["pointer_order_views"]
    pointer_differences = first_differences(
        native_pointer_views,
        edadb_pointer_views,
        "$.pointer_order_views",
    )
    if pointer_differences:
        native_fixed_rects = normalized_fixed_rect_groups(native_pointer_views.get("fixed_rects"))
        edadb_fixed_rects = normalized_fixed_rect_groups(edadb_pointer_views.get("fixed_rects"))
        content_differences = first_differences(
            native_fixed_rects,
            edadb_fixed_rects,
            "$.pointer_order_views.fixed_rects.normalized",
        )
        if content_differences:
            semantic_failures.extend(content_differences)
            pointer_differences = []
    return semantic_failures, pointer_differences


def main(argv: Iterable[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    args = parser.parse_args(list(argv))

    semantic_failures, pointer_differences = compare(args.reference, args.candidate)
    if semantic_failures:
        print("FAIL: iRT semantic input database differs", file=sys.stderr)
        for failure in semantic_failures:
            print(f"  - {failure}", file=sys.stderr)
        return 1

    print("PASS: iRT semantic input database is equivalent")
    if pointer_differences:
        print(
            "REVIEW: pointer-ordered fixed-rectangle iteration differs while content matches"
        )
        for difference in pointer_differences[:3]:
            print(f"  - {difference}")
    else:
        print("PASS: iRT pointer-ordered fixed-rectangle iteration also matches")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
