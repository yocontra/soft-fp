# TODO

Single source of truth for open work. Items closed at 1.0 / 1.1 / 1.2 / 1.3
are recorded in `CHANGELOG.md`, not here.

This file tracks work owned by this repo. Downstream integration patches
belong in the consumer project that wires `sf64_*` into its own compiler,
runtime, or kernel ABI.

## Post-1.3

### `soft-fp128` sibling library

**What.** Same design playbook (Mesa arithmetic port + SLEEF / DD-style
transcendentals + TestFloat + MPFR oracle) extended to 113-bit
significand. Static archive `libsoft_fp128.a` alongside `libsoft_fp64.a`
in this same `yocontra/soft-fp` repo. Public C ABI prefix `sf128_*`
mirroring `sf64_*` (arithmetic, compare, full conversion matrix
including `f64 ↔ f128` and `i128 ↔ f128`, sqrt, fma, rounding,
classify, transcendentals). u10 transcendentals gated against MPFR
300-bit; bit-exact arithmetic gated against TestFloat fp128 vectors.

**Why it matters.** Same consumers (GPU / MCU / wasm targets without
hardware fp128) need it. Decoupled from fp64 on the release timeline,
but co-located so the build infrastructure, oracle setup, benchmark
harness, and ABI conventions are shared rather than re-invented in a
sibling repo.

**What's needed.** When work starts, introduce a top-level layout
split — likely `fp64/` and `fp128/` subtrees with a top-level
`CMakeLists.txt` orchestrating both — and grow the existing
`adapters/` and `tests/testfloat/` / `tests/mpfr/` infrastructure to
exercise both precisions. Until then, the repo stays flat with `src/`
+ `include/soft_fp64/` at root; restructuring before fp128 has files
to put somewhere is premature. SLEEF doesn't ship fp128 polynomials,
so the transcendental story will draw from a different reference (Sun
fdlibm-q, Boost.Multiprecision-derived coefficient sets, or DD/QD
arithmetic on top of `sf64_*` — TBD when the work starts).
