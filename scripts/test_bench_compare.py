#!/usr/bin/env python3
"""Exercise benchmark gate input validation and unchanged regression budgets."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

DEFAULT_BASELINE = object()
COMPARE = Path(__file__).resolve().parents[1] / "bench" / "compare.py"
AGGREGATE = COMPARE.with_name("aggregate.py")


def document(*rows):
    return {"schema": "soft-fp64.bench.v1", "results": list(rows)}


def row(name="add", ns=10.0):
    return {"name": name, "ns_per_op": ns}


class BenchCompareTests(unittest.TestCase):
    def compare(self, current, baseline=DEFAULT_BASELINE, *options):
        if baseline is DEFAULT_BASELINE:
            baseline = document(row())
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            current_path = root / "current.json"
            baseline_path = root / "baseline.json"
            current_path.write_text(json.dumps(current), encoding="utf-8")
            baseline_path.write_text(json.dumps(baseline), encoding="utf-8")
            return subprocess.run(
                [sys.executable, str(COMPARE), str(current_path), str(baseline_path), *options],
                capture_output=True,
                text=True,
                check=False,
            )

    def test_complete_comparison_and_regression(self):
        self.assertEqual(self.compare(document(row())).returncode, 0)
        result = self.compare(document(row(ns=12)))
        self.assertEqual(result.returncode, 1)
        self.assertIn("REGRESSION", result.stdout)

    def test_missing_operations_require_explicit_partial_mode(self):
        baseline = document(row(), row("mul"))
        self.assertEqual(self.compare(document(row()), baseline).returncode, 2)
        result = self.compare(document(row()), baseline, "--allow-partial")
        self.assertEqual(result.returncode, 0)
        self.assertIn("PASS (partial comparison)", result.stdout)
        self.assertIn("mul", result.stdout)
        self.assertEqual(
            self.compare(document(row(ns=12)), baseline, "--allow-partial").returncode, 1
        )

    def test_empty_and_disjoint_runs_never_pass(self):
        for options in [(), ("--allow-partial",)]:
            for current in [document(), document(row("new"))]:
                with self.subTest(options=options, current=current):
                    self.assertEqual(self.compare(current, DEFAULT_BASELINE, *options).returncode, 2)
        self.assertEqual(self.compare(document(row()), document()).returncode, 2)

    def test_invalid_rows_rejected_in_both_inputs(self):
        invalid = [
            [], None, {}, {"schema": "wrong", "results": [row()]},
            document(None), document(1), document({}), document({"name": "add"}),
            document(row(name="")), document(row(name="  ")),
            document(row(name=4)), document(row(), row()),
        ]
        invalid += [document(row(ns=value)) for value in
                    [None, "10", True, 0, -1, float("nan"), float("inf"), -float("inf"), 10**400]]
        for value in invalid:
            with self.subTest(value=value):
                for current, baseline in [(value, document(row())), (document(row()), value)]:
                    result = self.compare(current, baseline)
                    self.assertEqual(result.returncode, 2, result.stderr)
                    self.assertNotIn("PASS", result.stdout)
                    self.assertNotIn("Traceback", result.stderr)

    def test_invalid_options_cannot_disable_gate(self):
        for option, values in [
            ("--threshold", ["nan", "inf", "-inf", "0", "-1"]),
            ("--cheap-op-absolute-budget", ["nan", "inf", "-inf", "-1"]),
        ]:
            for value in values:
                with self.subTest(option=option, value=value):
                    result = self.compare(document(row(ns=20)), DEFAULT_BASELINE, f"{option}={value}")
                    self.assertEqual(result.returncode, 2)

    def test_noise_floor_does_not_allow_missing_or_invalid_data(self):
        baseline = document(row(ns=1), row("mul"))
        self.assertEqual(self.compare(document(row("mul")), baseline).returncode, 2)
        self.assertEqual(self.compare(document(row(ns=float("nan")), row("mul")), baseline).returncode, 2)
        self.assertEqual(self.compare(document(row(ns=100), row("mul")), baseline).returncode, 0)

    def test_existing_budgets_and_boundaries(self):
        options = ("--threshold=0.20", "--cheap-op-absolute-budget=5.0")
        for base, current, expected in [(10, 15, 0), (10, 15.1, 1), (15, 19, 1), (20, 24, 0), (20, 24.1, 1)]:
            with self.subTest(base=base, current=current):
                result = self.compare(document(row(ns=current)), document(row(ns=base)), *options)
                self.assertEqual(result.returncode, expected, result.stdout)

    def test_new_operations_are_informational_when_baseline_is_complete(self):
        result = self.compare(document(row(), row("new")))
        self.assertEqual(result.returncode, 0)
        self.assertIn("New ops not in baseline", result.stdout)


class BenchAggregateTests(unittest.TestCase):
    def aggregate(self, samples):
        with tempfile.TemporaryDirectory() as directory:
            paths = []
            for index, sample in enumerate(samples):
                path = Path(directory) / f"run-{index}.json"
                path.write_text(json.dumps(sample), encoding="utf-8")
                paths.append(str(path))
            result = subprocess.run(
                [sys.executable, str(AGGREGATE), *paths],
                capture_output=True, text=True, check=False,
            )
            return result, json.loads(result.stdout) if result.returncode == 0 else None

    def test_requires_exactly_three_samples(self):
        for count in (0, 1, 2, 4):
            with self.subTest(count=count):
                result, aggregate = self.aggregate([document(row())] * count)
                self.assertEqual(result.returncode, 2)
                self.assertIsNone(aggregate)

    def test_repeated_file_and_malformed_json_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            paths = [Path(directory) / f"run-{i}.json" for i in range(3)]
            for path in paths:
                path.write_text(json.dumps(document(row())), encoding="utf-8")
            repeated = subprocess.run(
                [sys.executable, str(AGGREGATE), str(paths[0]), str(paths[0]), str(paths[2])],
                capture_output=True, text=True, check=False,
            )
            self.assertEqual(repeated.returncode, 2)
            self.assertEqual(repeated.stdout, "")
            paths[1].write_text("{broken", encoding="utf-8")
            malformed = subprocess.run(
                [sys.executable, str(AGGREGATE), *map(str, paths)],
                capture_output=True, text=True, check=False,
            )
            self.assertEqual(malformed.returncode, 2)
            self.assertEqual(malformed.stdout, "")

    def test_preserves_entire_minimum_row_per_operation(self):
        first = {**row(ns=11), "iterations": 111, "mops_per_sec": 90.9}
        best = {**row(ns=9), "iterations": 222, "mops_per_sec": 111.1}
        result, aggregate = self.aggregate([
            document(first, row("mul", 8)),
            document(best, row("mul", 7)),
            document(row(ns=10), row("mul", 6)),
        ])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(aggregate["results"], [best, row("mul", 6)])
        self.assertEqual(len(aggregate["aggregation"]["samples"]), 3)

    def test_bad_sample_fails_even_when_other_samples_are_valid(self):
        bad_samples = [
            {}, document(), document(None), document({}),
            document(row(), row()), document(row(ns=0)), document(row(ns=-1)),
            document(row(ns=True)), document(row(ns="10")),
            document(row(ns=float("nan"))), document(row(ns=float("inf"))),
            document(row("other")), document(row(), row("extra")),
        ]
        for sample in bad_samples:
            for position in range(3):
                with self.subTest(sample=sample, position=position):
                    samples = [document(row()) for _ in range(3)]
                    samples[position] = sample
                    result, aggregate = self.aggregate(samples)
                    self.assertEqual(result.returncode, 2, result.stderr)
                    self.assertEqual(result.stdout, "")
                    self.assertIsNone(aggregate)

    def test_sustained_regression_fails_but_single_spike_is_not_estimated(self):
        compare = BenchCompareTests().compare
        for values, expected in [([27, 26, 25], 1), ([20, 100, 20], 0)]:
            with self.subTest(values=values):
                result, aggregate = self.aggregate([document(row(ns=value)) for value in values])
                self.assertEqual(result.returncode, 0, result.stderr)
                gate = compare(aggregate, document(row(ns=20)),
                               "--threshold=0.20", "--cheap-op-absolute-budget=5.0")
                self.assertEqual(gate.returncode, expected, gate.stdout)


if __name__ == "__main__":
    unittest.main()
