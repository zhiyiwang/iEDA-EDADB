#!/usr/bin/env python3

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from compare_irt_input_snapshots import compare


class CompareIrtInputSnapshotsTest(unittest.TestCase):
    def write_snapshot(
        self,
        directory: Path,
        name: str,
        design_name: str,
        rects: list[dict[str, object]],
    ) -> Path:
        path = directory / name
        path.write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "semantic_database": {"design_name": design_name},
                    "pointer_order_views": {
                        "fixed_rects": [
                            {
                                "is_routing": True,
                                "layer_idx": 0,
                                "net_idx": -1,
                                "rects": rects,
                            }
                        ]
                    },
                }
            ),
            encoding="utf-8",
        )
        return path

    def test_equal_semantics_with_different_pointer_order_is_review(self) -> None:
        first_rect = {"real": [0, 0, 1, 1], "grid": [0, 0, 0, 0], "layer_idx": 0}
        second_rect = {"real": [2, 2, 3, 3], "grid": [1, 1, 1, 1], "layer_idx": 0}
        with tempfile.TemporaryDirectory() as temp_dir:
            directory = Path(temp_dir)
            native = self.write_snapshot(directory, "native.json", "gcd", [first_rect, second_rect])
            edadb = self.write_snapshot(directory, "edadb.json", "gcd", [second_rect, first_rect])
            semantic_failures, pointer_differences = compare(native, edadb)
        self.assertEqual([], semantic_failures)
        self.assertTrue(pointer_differences)

    def test_semantic_difference_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            directory = Path(temp_dir)
            native = self.write_snapshot(directory, "native.json", "native", [])
            edadb = self.write_snapshot(directory, "edadb.json", "edadb", [])
            semantic_failures, _ = compare(native, edadb)
        self.assertTrue(semantic_failures)

    def test_pointer_view_content_difference_fails(self) -> None:
        native_rect = {"real": [0, 0, 1, 1], "grid": [0, 0, 0, 0], "layer_idx": 0}
        edadb_rect = {"real": [0, 0, 2, 2], "grid": [0, 0, 0, 0], "layer_idx": 0}
        with tempfile.TemporaryDirectory() as temp_dir:
            directory = Path(temp_dir)
            native = self.write_snapshot(directory, "native.json", "gcd", [native_rect])
            edadb = self.write_snapshot(directory, "edadb.json", "gcd", [edadb_rect])
            semantic_failures, pointer_differences = compare(native, edadb)
        self.assertTrue(semantic_failures)
        self.assertEqual([], pointer_differences)


if __name__ == "__main__":
    unittest.main()
