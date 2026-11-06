#!/usr/bin/env python3
"""Check the README's named power sweeps against executable MPFR call sites.

This intentionally supports the three documented power sweeps, not arbitrary
C++ or every numerical statement in the README. It checks operation, oracle,
tier, sampling, bounds, and sample count; comments cannot satisfy a claim.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
NUM = r"-?(?:\d+\.\d*|\d*\.\d+|\d+)(?:[eE][-+]?\d+)?"
NAMES = ("pow", "pow-xbig", "pow-ybig")


def code_without_comments(source: str) -> str:
    # Preserve quoted text so URLs and comment-like strings are not stripped.
    token = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/'
    return re.sub(token, lambda m: " " if m[0].startswith(("//", "/*")) else m[0], source)


def constant(source: str, declaration: str) -> int:
    matches = re.findall(declaration, source)
    if len(matches) != 1:
        raise ValueError("missing or ambiguous constant: " + declaration)
    return int(matches[0])


def read_sweeps(source: str) -> dict[str, list[str]]:
    code = code_without_comments(source)
    precision = constant(code, r"constexpr\s+mpfr_prec_t\s+ORACLE_PREC\s*=\s*(\d+)\s*;")
    count = constant(code, r"constexpr\s+int\s+N_RAND\s*=\s*(\d+)\s*;")
    tier = constant(code, r"\bU35\s*=\s*(\d+)\s*,")
    if not re.search(r"constexpr\s+Tier\s+U35\s*=\s*Tier::U35\s*;", code):
        raise ValueError("U35 alias must refer to the release tier")
    sweeps = {}
    for name in NAMES:
        # Deliberately match a registered call and its real lambda/oracle, not
        # nearby numbers. Changes to this supported call form require review.
        pattern = (
            r'results\s*\.\s*push_back\s*\(\s*(sweep2_log_uniform|sweep2_log)\s*\('
            + r'\s*"' + re.escape(name) + r'"\s*,\s*U35\s*,\s*'
            + r'\[\]\s*\(\s*double\s+x\s*,\s*double\s+y\s*\)\s*'
            + r'\{\s*return\s+sf64_pow\s*\(\s*x\s*,\s*y\s*\)\s*;\s*\}\s*,\s*mpfr_pow\s*,'
            + r'\s*(' + NUM + r')\s*,\s*(' + NUM + r')\s*,\s*false\s*,'
            + r'\s*(' + NUM + r')\s*,\s*(' + NUM + r')'
            + r'\s*(,\s*true)?\s*(?:,\s*0x[0-9a-fA-F]+ULL)?\s*\)\s*\)\s*;'
        )
        matches = list(re.finditer(pattern, code))
        if len(matches) != 1:
            raise ValueError(f"{name}: expected one registered sf64_pow / mpfr_pow sweep")
        method, xlo, xhi, ylo, yhi, signed = matches[0].groups()
        if (method == "sweep2_log") != bool(signed):
            raise ValueError(f"{name}: unsupported sign policy")
        # Verify the named driver uses the advertised loop and samplers.
        driver = re.search(r"Stats\s+" + method + r"\([^{}]+\)\s*\{([\s\S]*?)\n\}", code)
        body = re.sub(r"\s+", "", driver[1]) if driver else ""
        required = ["for(inti=0;i<N_RAND;++i)", "rng.log_uniform(xlo_abs,xhi_abs)",
                    "record(s,x,y,soft(x,y),ref2(mref,x,y));"]
        required += (["rng.log_uniform(ylo_abs,yhi_abs)", "if(y_sym&&(rng.next()&1))y=-y;"]
                     if signed else ["rng.uniform(ylo,yhi)"])
        if any(fragment not in body for fragment in required):
            raise ValueError(f"{name}: driver sampling/count changed; review its README contract")
        sweeps[name] = ["log", f"[{xlo}, {xhi}]", "signed log" if signed else "uniform",
                        f"[{ylo}, {yhi}]", f"U35 ({tier} ULP)", f"MPFR ({precision} bits)", str(count)]
    return sweeps


def range_values(text: str) -> tuple[float, float] | None:
    match = re.fullmatch(r"\[\s*(" + NUM + r")\s*,\s*(" + NUM + r")\s*\]", text)
    return tuple(map(float, match.groups())) if match else None


def check(readme: str, source: str) -> list[str]:
    try:
        sweeps = read_sweeps(source)
    except ValueError as error:
        return [f"tests/mpfr/test_mpfr_diff.cpp: {error}"]
    rows: dict[str, list[tuple[int, list[str]]]] = {name: [] for name in NAMES}
    for line_no, line in enumerate(readme.splitlines(), 1):
        if not line.lstrip().startswith("|"):
            continue
        fields = [field.strip().strip("`") for field in line.strip().strip("|").split("|")]
        if fields and fields[0] in rows:
            rows[fields[0]].append((line_no, fields[1:]))
    failures = []
    for name, expected in sweeps.items():
        if len(rows[name]) != 1:
            failures.append(f"README.md: expected exactly one power-sweep table row for {name}")
            continue
        line_no, actual = rows[name][0]
        if len(actual) != len(expected):
            failures.append(f"README.md:{line_no}: {name}: expected seven contract fields")
            continue
        for index, (got, want) in enumerate(zip(actual, expected)):
            equal = range_values(got) == range_values(want) if index in (1, 3) else got == want
            if not equal:
                failures.append(f"README.md:{line_no}: {name}: expected {want!r}, found {got!r}")
    return failures


def main() -> int:
    try:
        failures = check((REPO / "README.md").read_text(encoding="utf-8"),
                         (REPO / "tests/mpfr/test_mpfr_diff.cpp").read_text(encoding="utf-8"))
    except OSError as error:
        print(f"check_readme_claim_parity: {error}", file=sys.stderr)
        return 2
    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    print("check_readme_claim_parity: three named power sweeps match their MPFR contracts")
    return 0


if __name__ == "__main__":
    sys.exit(main())
