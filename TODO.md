# TODO

This is the single source of truth for unfinished work owned by `soft-fp`.
Completed work belongs in `CHANGELOG.md`; downstream compiler and runtime
integration belongs in the consuming project.

There are no known correctness or release-blocking defects in the supported
2.0 API. The items below extend that API or strengthen its validation. Do not
add placeholder symbols: a new operation is public only when its implementation,
accuracy contract, documentation, and oracle tests land together.

## Wider-format transcendentals

Add format-specific transcendental libraries for binary128 and binary256. The
initial surface should mirror the strict binary64 functions where the
mathematical operation is meaningful: trigonometric and inverse trigonometric,
hyperbolic and inverse hyperbolic, exponential and logarithmic, power and root,
error, and gamma families.

Acceptance criteria:

- Implement argument reduction, constants, and polynomial or rational kernels
  at the destination format's precision; do not forward through host `libm`,
  binary64, `long double`, or compiler-native floating-point types.
- Specify special-case, signed-zero, NaN-payload, rounding, and exception-flag
  behavior for every public function.
- Establish per-function ULP contracts using independent MPFR oracles at a
  precision comfortably above the destination format.
- Gate difficult regions separately, including reduction boundaries, poles,
  zero crossings, subnormal outputs, and overflow/underflow transitions.
- Extend the optimized-IR audit so every new production source is proven free
  of host floating-point arithmetic.
- Add C ABI coverage, fuzz targets, benchmarks, API documentation, and ABI
  manifest entries in the same change as each function family.

## Complete the conversion matrix

Grow the wider core APIs from the current fixed-width integer and binary64
conversions to a symmetric conversion surface.

Acceptance criteria:

- Add bit-exact binary32 conversions for binary128 and binary256 in all five
  rounding modes, including caller-owned exception-state variants.
- Add direct binary128-to-binary256 and binary256-to-binary128 conversions;
  they must not round through binary64.
- Design explicit word-struct ABIs for signed and unsigned 128-bit and 256-bit
  integers, then add integer conversions without relying on compiler-native
  `__int128` types.
- Make the `sf128_*` and `sf256_*` narrowing APIs structurally consistent,
  including rounding-mode and `_ex` variants for every supported width.
- Validate exact results and flags against MPFR and an independent integer
  oracle across boundaries, randomized inputs, every rounding mode, NaNs,
  infinities, and saturation cases.

## Wider-format performance work

The binary128 and binary256 cores are correctness-first implementations and do
not yet have checked-in performance baselines.

Acceptance criteria:

- Extend the benchmark harness to cover every public core operation for both
  formats without allowing constant folding or dead-code elimination.
- Record reproducible per-platform metadata and establish non-flaky regression
  gates on at least Linux x86-64 and macOS arm64.
- Profile division, square root, remainder, and FMA, then optimize algorithmic
  bottlenecks without weakening bit-exactness or the no-host-FP rule.
- Add comparative measurements against maintained arbitrary-precision or
  software-floating-point implementations, clearly separating informational
  comparisons from regression gates.

## Portability and release coverage

Broaden validation beyond the current macOS arm64, Linux x86-64, and Windows
MSVC configurations before claiming additional target support.

Acceptance criteria:

- Add at least one big-endian compile-and-test job and verify the explicit word
  ABIs remain byte-order independent.
- Add a 32-bit target job to exercise limb arithmetic where the native word
  size is smaller than `uint64_t`.
- Add a freestanding or GPU-oriented compile job with TLS and the host floating
  environment unavailable, using only caller-owned `_ex` state.
- Exercise installed `soft_fp`, `soft_fp64`, `soft_fp128`, and `soft_fp256`
  packages from both C and C++ downstream projects; add pkg-config metadata for
  the wider libraries if that integration path is retained.
- Produce and test relocatable 2.x release archives, generate API documentation,
  verify installed license/attribution files, and document the release signing
  and provenance process.

## Maintenance rules

- Every bug fix needs a minimal regression test that fails before the fix.
- Every public ABI addition must update the ABI manifest and install-consumer
  tests.
- Dependency revisions and CI actions remain pinned and are updated through a
  reviewed change with the full validation matrix.
- Performance baseline changes require repeated measurements and an explanation
  in the commit or pull request; never raise a threshold merely to make CI pass.
