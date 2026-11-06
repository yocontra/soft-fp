#!/usr/bin/env python3
"""Negative cases for the named power-sweep documentation guard."""

import subprocess
import sys
import unittest
from pathlib import Path

from check_readme_claim_parity import check

ROOT = Path(__file__).resolve().parent.parent
README = """
| Sweep | x sampling | x range | y sampling | y range | Tier | Oracle | Samples |
|---|---|---|---|---|---|---|---|
| `pow` | log | `[1e-6, 1e6]` | uniform | `[-50, 50]` | U35 (8 ULP) | MPFR (200 bits) | 10000 |
| `pow-xbig` | log | `[1e-100, 1e100]` | signed log | `[1e-5, 5]` | U35 (8 ULP) | MPFR (200 bits) | 10000 |
| `pow-ybig` | log | `[1e-6, 1e3]` | uniform | `[-100, 100]` | U35 (8 ULP) | MPFR (200 bits) | 10000 |
"""


class ClaimParityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / "tests/mpfr/test_mpfr_diff.cpp").read_text(encoding="utf-8")

    def test_current_contract(self):
        self.assertEqual(check(README, self.source), [])

    def test_current_readme(self):
        self.assertEqual(check((ROOT / "README.md").read_text(encoding="utf-8"), self.source), [])

    def test_cli_requires_explicit_text_encoding(self):
        result = subprocess.run(
            [sys.executable, "-X", "warn_default_encoding", "-W", "error::EncodingWarning",
             str(ROOT / "scripts/check_readme_claim_parity.py")],
            capture_output=True, encoding="utf-8", check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_numeric_equivalent_spellings(self):
        self.assertEqual(check(README.replace("[-50, 50]", "[-50.0, 5e1]"), self.source), [])

    def test_missing_or_duplicate_rows(self):
        row = next(line for line in README.splitlines() if line.startswith("| `pow`"))
        self.assertTrue(check(README.replace(row, ""), self.source))
        self.assertTrue(check(README + row, self.source))
        self.assertTrue(check("", self.source))

    def test_readme_contract_changes(self):
        for old, new in [("[1e-6, 1e6]", "[1e6, 1e-6]"), ("signed log", "uniform"),
                         ("8 ULP", "9 ULP"), ("200 bits", "53 bits"), ("10000", "4096")]:
            with self.subTest(new=new):
                self.assertTrue(check(README.replace(old, new), self.source))

    def test_source_contract_changes(self):
        for old, new in [("mpfr_pow,", "mpfr_exp,"), ("return sf64_pow(x, y);", "return sf64_powr(x, y);"),
                         ("U35 = 8,", "U35 = 9,"), ("ORACLE_PREC = 200", "ORACLE_PREC = 53"),
                         ("N_RAND = 10000", "N_RAND = 4096"),
                         ("mpfr_pow, 1e-100, 1e100", "mpfr_pow, 1e100, 1e-100"),
                         ("rng.uniform(ylo, yhi)", "rng.uniform(0.0, 1.0)"),
                         ("i < N_RAND", "i < 4096")]:
            with self.subTest(new=new):
                self.assertIn(old, self.source)
                self.assertTrue(check(README, self.source.replace(old, new)))

    def test_comment_only_evidence_fails(self):
        self.assertTrue(check(README, "/*\n" + self.source.replace("*/", "") + "\n*/"))

    def test_unrelated_call_does_not_replace_registered_sweep(self):
        self.assertTrue(check(README, self.source.replace('"pow-xbig"', '"other"')
                              + '\nunrelated(1e-100, 1e100);\n'))

    def test_duplicate_registered_sweep_fails(self):
        self.assertTrue(check(README, self.source + self.source))


if __name__ == "__main__":
    unittest.main()
