#!/usr/bin/env python3
"""Compare iRT's wrapped input environment for native and EDADB paths."""

from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from pathlib import Path
from typing import Any, Iterable


def load_environment(path: Path) -> tuple[Any, dict[str, Any]]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(payload, list) or len(payload) != 2:
        raise ValueError(f"unexpected iRT environment payload: {path}")
    die = payload[0].get("die")
    environment = payload[1].get("env_shape")
    if not isinstance(environment, dict):
        raise ValueError(f"missing env_shape object: {path}")
    return die, environment


def shape_list(environment: dict[str, Any], key: str) -> list[list[Any]]:
    entry = environment.get(key, {})
    shapes = entry.get("shape", []) if isinstance(entry, dict) else []
    if not isinstance(shapes, list):
        raise ValueError(f"invalid shape list for {key}")
    return shapes


def compare(reference_path: Path, candidate_path: Path) -> list[str]:
    reference_die, reference = load_environment(reference_path)
    candidate_die, candidate = load_environment(candidate_path)
    failures: list[str] = []
    if reference_die != candidate_die:
        failures.append(f"die differs: native={reference_die} edadb={candidate_die}")

    differing_keys: list[str] = []
    for key in sorted(set(reference) | set(candidate)):
        native_shapes = shape_list(reference, key)
        edadb_shapes = shape_list(candidate, key)
        if native_shapes == edadb_shapes:
            continue

        differing_keys.append(key)
        native_counts = Counter(map(tuple, native_shapes))
        edadb_counts = Counter(map(tuple, edadb_shapes))
        added = sum((edadb_counts - native_counts).values())
        removed = sum((native_counts - edadb_counts).values())
        if added == 0 and removed == 0:
            failures.append(
                f"{key}: order differs with identical shape multiset "
                f"(count={len(native_shapes)})"
            )
        else:
            failures.append(
                f"{key}: native={len(native_shapes)} edadb={len(edadb_shapes)} "
                f"added={added} removed={removed}"
            )

    if differing_keys:
        failures.insert(0, "differing env_shape keys: " + ", ".join(differing_keys))
    return failures


def main(argv: Iterable[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    args = parser.parse_args(list(argv))

    failures = compare(args.reference, args.candidate)
    if failures:
        print("FAIL: iRT wrapped input differs", file=sys.stderr)
        for failure in failures:
            print(f"  - {failure}", file=sys.stderr)
        return 1
    print("PASS: iRT wrapped input environment is equivalent")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
