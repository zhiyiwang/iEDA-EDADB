"""Check experiment isolation and non-overwriting collection without running iEDA."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("profile_run", HERE / "run.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class WorkflowTests(unittest.TestCase):
    def test_environment(self):
        with patch.dict(os.environ, {"LD_PRELOAD": "/invalid/probe.so", "EDADB_LEAF_BATCH_READ": "1"}):
            environment = runner.environment()
        self.assertNotIn("LD_PRELOAD", environment)
        self.assertNotIn("EDADB_LEAF_BATCH_READ", environment)

    def test_existing_destination_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "existing"
            destination.mkdir()
            sentinel = destination / "keep"
            sentinel.write_text("unchanged")
            result = subprocess.run([sys.executable, str(HERE / "collect.py"),
                                     str(Path(directory) / "missing"), "--destination", str(destination)],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Destination already exists", result.stderr)
            self.assertEqual(sentinel.read_text(), "unchanged")

    def test_review_requires_explicit_batch(self):
        result = subprocess.run([sys.executable, str(HERE / "review.py")], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("required", result.stderr)


if __name__ == "__main__":
    unittest.main()
