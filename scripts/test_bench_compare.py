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


if __name__ == "__main__":
    unittest.main()
