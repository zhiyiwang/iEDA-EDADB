#!/usr/bin/env python3
"""Check generated-via geometry in native and EDADB iRT snapshots."""

from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from pathlib import Path
from typing import Any, Iterable


CUT_COUNTS = {"via": 10, "via2": 4, "via3": 4, "via4": 1}
ROUTING_COUNTS = {"met1": 1, "met2": 2, "met3": 2, "met4": 2, "met5": 1}


def load_snapshot(
    path: Path,
) -> tuple[Any, dict[str, Any], list[tuple[object, ...]]]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    try:
        die = payload[0]["die"]
        environment = payload[1]["env_shape"]
        shapes = environment["obs"]["shape"]
    except (IndexError, KeyError, TypeError) as error:
        raise ValueError(f"invalid iRT environment snapshot: {path}") from error
    if not isinstance(environment, dict) or not isinstance(shapes, list):
        raise ValueError(f"invalid env_shape content: {path}")
    return die, environment, [tuple(shape) for shape in shapes]


def layer_counts(shapes: Iterable[tuple[object, ...]]) -> Counter[str]:
    return Counter(str(shape[-1]) for shape in shapes)


def select_layers(
    shapes: Iterable[tuple[object, ...]], layers: set[str]
) -> Counter[tuple[object, ...]]:
    return Counter(shape for shape in shapes if str(shape[-1]) in layers)


def require_counts(label: str, actual: Counter[str], expected: dict[str, int]) -> None:
    if actual != Counter(expected):
        raise AssertionError(f"{label} layer counts differ: actual={dict(actual)} expected={expected}")


def check_known_defect(
    native_die: Any,
    native_environment: dict[str, Any],
    native_shapes: list[tuple[object, ...]],
    edadb_die: Any,
    edadb_environment: dict[str, Any],
    edadb_shapes: list[tuple[object, ...]],
) -> None:
    if native_die != edadb_die:
        raise AssertionError(f"die differs: native={native_die} EDADB={edadb_die}")
    if set(native_environment) != set(edadb_environment):
        raise AssertionError(
            "environment keys differ: "
            f"native={sorted(native_environment)} EDADB={sorted(edadb_environment)}"
        )
    for key in native_environment:
        if key != "obs" and native_environment[key] != edadb_environment[key]:
            raise AssertionError(f"unexpected non-obstacle environment difference: {key}")

    native_layers = layer_counts(native_shapes)
    edadb_layers = layer_counts(edadb_shapes)
    require_counts("native", native_layers, ROUTING_COUNTS | CUT_COUNTS)
    require_counts(
        "EDADB",
        edadb_layers,
        ROUTING_COUNTS | {layer: count * 3 for layer, count in CUT_COUNTS.items()},
    )

    routing_layers = set(ROUTING_COUNTS)
    cut_layers = set(CUT_COUNTS)
    native_routing = select_layers(native_shapes, routing_layers)
    edadb_routing = select_layers(edadb_shapes, routing_layers)
    if native_routing != edadb_routing:
        raise AssertionError("routing-layer enclosure geometry differs")

    native_cuts = select_layers(native_shapes, cut_layers)
    edadb_cuts = select_layers(edadb_shapes, cut_layers)
    expected_edadb_cuts = Counter(
        {shape: multiplicity * 3 for shape, multiplicity in native_cuts.items()}
    )
    if edadb_cuts != expected_edadb_cuts:
        raise AssertionError("EDADB cut geometry is not the exact native multiset tripled")

    print("PASS: reproduced the known generated-via restoration defect")
    print("  native: 19 cut shapes + 8 enclosure shapes = 27 obstacles")
    print("  EDADB:  57 cut shapes + 8 enclosure shapes = 65 obstacles")
    print("  delta:   38 duplicate cut shapes; routing-layer geometry unchanged")


def main(argv: Iterable[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("native", type=Path)
    parser.add_argument("edadb", type=Path)
    parser.add_argument(
        "--expect-known-defect",
        action="store_true",
        help="pass only when the exact current 27-to-65 defect is reproduced",
    )
    args = parser.parse_args(list(argv))

    try:
        native_die, native_environment, native_shapes = load_snapshot(args.native)
        edadb_die, edadb_environment, edadb_shapes = load_snapshot(args.edadb)
        if args.expect_known_defect:
            check_known_defect(
                native_die,
                native_environment,
                native_shapes,
                edadb_die,
                edadb_environment,
                edadb_shapes,
            )
            return 0

        if native_die != edadb_die or set(native_environment) != set(edadb_environment):
            print("FAIL: generated-via iRT environment structure differs", file=sys.stderr)
            return 1
        non_obstacle_keys_match = all(
            native_environment[key] == edadb_environment[key]
            for key in native_environment
            if key != "obs"
        )
        if not non_obstacle_keys_match or Counter(native_shapes) != Counter(edadb_shapes):
            native_count = Counter(native_shapes)
            edadb_count = Counter(edadb_shapes)
            added = sum((edadb_count - native_count).values())
            removed = sum((native_count - edadb_count).values())
            print("FAIL: generated-via iRT obstacle geometry differs", file=sys.stderr)
            print(
                f"  native={len(native_shapes)} edadb={len(edadb_shapes)} "
                f"added={added} removed={removed}",
                file=sys.stderr,
            )
            return 1
    except (AssertionError, OSError, ValueError, json.JSONDecodeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1

    print("PASS: generated-via iRT obstacle geometry is equivalent")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
