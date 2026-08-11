#!/usr/bin/env python3

import importlib.util
import tempfile
import unittest
from pathlib import Path


SCRIPT_PATH = Path(__file__).with_name("summarize_irt_variability.py")
SPEC = importlib.util.spec_from_file_location("summarize_irt_variability", SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class SummarizeIrtVariabilityTest(unittest.TestCase):
    def test_group_statistics_and_runtime(self):
        statistics = MODULE.group_statistics([1, 2, 3], [4, 5, 6])
        self.assertEqual(statistics["native_mean"], 2.0)
        self.assertEqual(statistics["edadb_mean"], 5.0)
        self.assertEqual(statistics["mean_delta"], 3.0)
        self.assertEqual(statistics["mean_delta_percent"], 150.0)
        self.assertEqual(statistics["exact_two_sided_permutation_p_value"], 0.1)

        with tempfile.TemporaryDirectory() as directory:
            log_path = Path(directory) / "run.log"
            log_path.write_text(
                "[RT 20260811 20:32:58 id RTInterface.cpp:162 Info runRT] "
                "Completed (elapsed = 02:34:36, cpu = 07:37:46, mem = 2639.31MB)\n",
                encoding="utf-8",
            )
            self.assertEqual(MODULE.run_rt_elapsed_seconds(log_path), 9276)

    def test_observed_ranges_include_raw_samples_and_statistics(self):
        native_runs = [
            {"routing_metrics": {"wire_len": value}} for value in (10, 20, 30)
        ]
        edadb_runs = [
            {"routing_metrics": {"wire_len": value}} for value in (20, 30, 40)
        ]
        summary = MODULE.observed_ranges(native_runs, edadb_runs, "routing_metrics")["wire_len"]
        self.assertEqual(summary["native"], [10, 20, 30])
        self.assertEqual(summary["edadb"], [20, 30, 40])
        self.assertEqual(summary["native_observed_min"], 10)
        self.assertEqual(summary["native_observed_max"], 30)
        self.assertEqual(summary["edadb_outside_native_observed_range"], [40])
        self.assertEqual(summary["native_mean"], 20.0)
        self.assertEqual(summary["edadb_mean"], 30.0)

    def test_equal_three_by_three_design_has_twenty_partitions(self):
        design = MODULE.statistical_design(3, 3)
        self.assertEqual(design["permutation_partition_count"], 20)
        self.assertEqual(design["minimum_attainable_two_sided_p_value_for_equal_groups"], 0.1)


if __name__ == "__main__":
    unittest.main()
