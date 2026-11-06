# Contributing to soft-fp

This file defines the shared development and review requirements. Local agent
configuration should link here rather than duplicate them.

## Build and check

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
prek run --all-files
```

Also run Debug and the configurations affected by the change. The
[CI workflow](.github/workflows/ci.yml) defines the platform, exception-mode,
sanitizer, coverage, fuzz, install, and performance checks. Release publication
follows [RELEASING.md](RELEASING.md).

## Implementation constraints

- Production arithmetic uses integer operations. Floating types in the binary64
  ABI carry bit patterns; they must not introduce host floating-point arithmetic,
  comparisons, conversions, libm calls, intrinsics, or inline assembly. SLEEF
  operations use the software arithmetic and comparison helpers. This rule
  also applies to code adapted from upstream.
- Keep the target's floating-point contraction and fast-math restrictions.
  Do not enable contraction through per-file flags, pragmas, or attributes.
  `sf64_fma` computes an integer product and alignment followed by one rounding;
  it does not depend on host FMA instructions.
- Run `scripts/check_no_host_fp.sh` in source and optimized-IR modes as CI does.
  The source scan catches prohibited patterns; the IR audit catches operations
  introduced by optimization. Neither a regex nor a passing sample proves
  numerical correctness. Reproduce false positives and improve the check
  instead of adding exclusions.
- Public C symbols use `sf64_*`, `sf128_*`, or `sf256_*`. Keep cross-translation-
  unit helpers hidden. Update the ABI manifest and install-consumer tests for
  public additions. Berkeley SoftFloat backend symbols and state must remain
  isolated under the production prefix.
- Preserve upstream copyright and license notices. Record the source file,
  pinned revision, or adapted function in `NOTICE`. Do not reformat vendored
  trees or claim attribution for code that is not used.
- Use C++17 and the checked-in clang-format configuration. New source files
  carry an SPDX identifier for their actual license. Document public operations
  with their semantics, precision contract, and special cases.

## Numerical validation

A failing numerical test requires an implementation fix or an explicitly
reviewed change to the public contract. Do not make a failure disappear by:

- widening a tolerance, flattening NaN payloads or signed zero, replacing exact
  checks with approximate checks, or moving inputs into a weaker test tier;
- skipping or deleting oracle rows, reducing a corpus, changing a seed to avoid
  failures, narrowing exhaustive coverage, or silently sharding it;
- lowering oracle precision, substituting host libm for MPFR, or rounding an
  oracle through a narrower type before the specified final conversion;
- adding input-specific exceptions keyed to failing corpus rows, adjusting
  upstream coefficients to fit observed results, or generating expected output
  from the implementation under test;
- replacing library symbols with stubs or overrides, or disabling test,
  sanitizer, fuzz, coverage, or benchmark checks because they fail.

Binary64 MPFR references use at least 200-bit precision; wider-format references
must exceed the destination precision. Expected-bit tables cite TestFloat,
MPFR computation, or an IEEE-754 derivation. Tests call the real library.
Keep runtime expectations active in Release builds as well as Debug.

The binary64 accuracy gates are `BIT_EXACT` (bitwise equality), `U10` (4 ULP),
`U35` (8 ULP), and `GAMMA` (1024 ULP), with per-operation domains in the tests
and README. Do not introduce report-only tiers into the correctness suite.
A function outside its contract must be fixed, explicitly marked experimental
and excluded from shipped accuracy claims, or removed through the appropriate
API change. Do not widen its gate. An experimental diagnostic is not a passing
correctness test.

For ULP failures, record input bits, actual bits, oracle bits, and ULP delta.
Check normalization, guard/sticky bits, ties, subnormal transitions, signed
zeros, and NaN handling before changing the algorithm.

Every sweep counts every input. Nonfinite results must be classified and
compared, including signs of infinity and zero; they cannot be silently dropped.
Wide exponent ranges need logarithmic sampling. Preserve the existing
trivial-match limits that reject mostly-NaN or mostly-infinite sweeps. State the
sample count and bounds accurately. Consistency checks such as `sincos` versus
`sin`/`cos` supplement independent oracles rather than replace them.

Fuzz targets may use looser crash-hunting budgets only when their headers state
the budget and rationale. Accuracy regressions belong in the oracle suite.
The only existing leak-sanitizer exemption is `ASAN_OPTIONS=detect_leaks=0`
for MPFR caches; do not extend it to conceal failures.

## Claims and performance

Precision claims must identify the operation, tested domain, oracle, precision,
and enforcing test. Distinguish finite test coverage from a full-domain proof,
and dated observations from release guarantees. The README claim checker binds
its named power-sweep table to the corresponding executable definitions; other
claims still require review.

Performance claims need reproducible measurements and machine/build metadata.
Never refresh `bench/baseline.json`, raise a threshold, increase a noise floor,
add skip lists, or alter compiler/timing settings to conceal a regression.
Baseline updates require a hardware-class change or a reviewed deliberate
performance/methodology change. Keep CI regression checks enforcing.

## Independent review

Changes under `src/`, `tests/`, `fuzz/`, `.github/workflows/`, to `.clang-format`,
to benchmark baselines/comparison logic, or to compile, sanitizer, or ctest
arguments require independent review before completion:

1. Run the full unfiltered ctest suite and retain its literal result.
2. Give a fresh reviewer the diff and these requirements, without an author's
   explanation of why the change is correct. Ask for evidence of weakened
   validation or incorrect numerical behavior, with file/line references.
3. Report the reviewer's findings verbatim. Fix each finding, or rebut it with
   a specific implementation, upstream, or specification citation. Reproduce
   claims in their original scope before changing code or rejecting them.
4. Changes to `tests/testfloat/`, `tests/mpfr/`, or `bench/baseline.json` need
   a second independent reviewer.

Documentation-only changes outside the paths above do not require numerical
review. Build/script changes that do not affect compilation, test execution,
coverage, or benchmark policy can use ordinary review. Preserve behavioral
regression tests for fixes; do not label numerical changes as style-only work.

## Reporting

Report only commands run in the current session. A subset run is useful for
iteration but is not a passing full suite. Include the final ctest Passed/Failed
line when reporting success and the comparison table for a performance claim.
Call out edits to tests, fuzzers, SLEEF sources, benchmark baselines/comparison
logic, and compile/sanitizer/ctest settings. If a failure cannot be fixed, report
it explicitly and leave the check failing.
