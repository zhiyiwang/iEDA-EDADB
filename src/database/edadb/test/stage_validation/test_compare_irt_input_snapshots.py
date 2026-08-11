#!/usr/bin/env python3

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from compare_irt_input_snapshots import compare


class CompareIrtInputSnapshotsTest(unittest.TestCase):
    def write_environment(
        self,
        directory: Path,
        name: str,
        die: list[int],
        shapes: list[list[object]],
    ) -> Path:
        path = directory / name
        path.write_text(
            json.dumps([{"die": die}, {"env_shape": {"obs": {"shape": shapes}}}]),
            encoding="utf-8",
        )
        return path

    def test_equal_environment_passes(self) -> None:
        shape = [0, 0, 10, 10, "Metal1"]
        with tempfile.TemporaryDirectory() as temp_dir:
            directory = Path(temp_dir)
            native = self.write_environment(directory, "native.json", [0, 0, 100, 100], [shape])
            edadb = self.write_environment(directory, "edadb.json", [0, 0, 100, 100], [shape])
            failures = compare(native, edadb)
        self.assertEqual([], failures)

    def test_die_difference_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            directory = Path(temp_dir)
            native = self.write_environment(directory, "native.json", [0, 0, 100, 100], [])
            edadb = self.write_environment(directory, "edadb.json", [0, 0, 200, 200], [])
            failures = compare(native, edadb)
        self.assertTrue(failures)

    def test_shape_order_difference_is_reported(self) -> None:
        first = [0, 0, 10, 10, "Metal1"]
        second = [20, 20, 30, 30, "Metal1"]
        with tempfile.TemporaryDirectory() as temp_dir:
            directory = Path(temp_dir)
            native = self.write_environment(directory, "native.json", [0, 0, 100, 100], [first, second])
            edadb = self.write_environment(directory, "edadb.json", [0, 0, 100, 100], [second, first])
            failures = compare(native, edadb)
        self.assertTrue(any("order differs" in failure for failure in failures))


if __name__ == "__main__":
    unittest.main()
