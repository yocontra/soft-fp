# soft-fp

[![release](https://img.shields.io/github/v/release/yocontra/soft-fp)](https://github.com/yocontra/soft-fp/releases/latest)
[![CI](https://github.com/yocontra/soft-fp/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/yocontra/soft-fp/actions/workflows/ci.yml)

Software binary64, binary128, and binary256 arithmetic implemented with
integer operations. Binary64 also includes transcendental functions with
the accuracy bounds listed below.

Current stable release: **2.0.2** (2026-11-05).

The repository hosts a suite of integer-only IEEE-754 implementations that
share build infrastructure, oracle setup (Berkeley TestFloat 3e + MPFR),
benchmark harness, and ABI conventions. Version 2 ships:

- **`soft-fp64`** — the full binary64 surface (arithmetic, compare, full
  width-matrix conversions, sqrt, fma, rounding, classification,
  transcendentals). Static archive `libsoft_fp64.a`, public `sf64_*`
  C ABI, and `find_package(soft_fp64)` compatibility package.
- **`soft-fp128`** — IEEE binary128 (1/15/112) core arithmetic,
  conversions, all five rounding modes, explicit exception state,
  remainder, square root, fused multiply-add, rounding-to-integer,
  comparison, and classification.
- **`soft-fp256`** — IEEE binary256 (1/19/236) core arithmetic with the
  same rounding and exception model, conversions, remainder, square root,
  fused multiply-add, rounding-to-integer, comparison, and classification.

`find_package(soft_fp)` and `soft_fp::soft_fp` select the complete suite.
The legacy binary64 package remains supported. Wider formats are enabled by
default and can be disabled with `SOFT_FP_BUILD_FP128=OFF` or
`SOFT_FP_BUILD_FP256=OFF`.

The wider-format APIs provide arithmetic, rounding, comparisons,
classification, exception flags, remainder, square root, and FMA. Their current
conversions cover fixed-width integers and binary64; binary32 and direct
binary128/binary256 conversions remain in `TODO.md`. Transcendental
functions are available only for binary64.

## Build and use

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

```c
#include <soft_fp64/soft_f64.h>

double sum = sf64_add(1.0, 2.0);
double root = sf64_sqrt(2.0);
```

The binary64 C ABI uses `double` to carry bits. A compiler or IR must be able
to represent that type in function signatures even when the target lacks fp64
instructions. CI checks optimized LLVM IR for host floating-point operations.
Languages without a binary64 type need a compiler adapter; linking this
library alone does not add that type to a language.

The existing AdaptiveCpp Metal integration is described below. PyTorch MPS
and WebGPU adapters are possible downstream projects, not integrations shipped
by this repository.

## Install

Three supported integration paths. Pick whichever matches your build.

### 1. CMake `find_package`

After `cmake --install`, consumers of all formats use:

```cmake
find_package(soft_fp 2 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE soft_fp::soft_fp)
```

Binary64-only compatibility:

```cmake
find_package(soft_fp64 REQUIRED)
target_link_libraries(my_app PRIVATE soft_fp64::soft_fp64)
```

### 2. Git submodule + `add_subdirectory`

No install step; vendor the source tree:

```cmake
add_subdirectory(extern/soft-fp64)
target_link_libraries(my_app PRIVATE soft_fp::soft_fp)
```

### 3. pkg-config

For non-CMake build systems:

```bash
cc $(pkg-config --cflags soft_fp64) my_app.c $(pkg-config --libs soft_fp64)
```

## Usage

Minimal working example:

```c
#include "soft_fp64/soft_f64.h"

int main(void) {
    double r = sf64_add(1.0, 2.0);   // 3.0
    double s = sf64_sqrt(2.0);       // 1.4142135623730951
    double t = sf64_sin(sf64_log(r));
    (void)s; (void)t;
    return 0;
}
```

For all formats, include `soft_fp/soft_fp.h`. Values wider than binary64 use
explicit, endian-independent word structs (`sf128_t` and `sf256_t`), so their
ABI never depends on `__float128` or `__uint128_t`.

```c
#include "soft_fp/soft_fp.h"

sf128_t q = sf128_from_f64(1.5);
sf128_t q2 = sf128_mul(q, q);

sf256_t o = sf256_from_i64(2);
sf64_fe_state_t flags = {0};
sf256_t root = sf256_sqrt_r_ex(SF64_RNE, o, &flags);
```

Every public symbol uses a stable `extern "C"` ABI. The only mutable state is
the documented thread-local exception environment; `_ex` entry points use
caller-owned sticky flags for runtimes that cannot provide TLS. See the
headers under `include/` for the full API.

### Integrating under a different symbol name

The library exports stable, vendor-neutral `sf64_*` symbols. If your
frontend's code generator emits calls under a different name (for example,
AdaptiveCpp's MSL emitter emits `__acpp_sscp_soft_f64_*`), don't rebuild
this library — add a thin shim in your frontend that forwards to the
`sf64_*` symbols:

```cpp
extern "C" double __acpp_sscp_soft_f64_add(double a, double b) {
    return sf64_add(a, b);
}
// …one line per symbol.
```

That keeps vendor-specific naming where it belongs (in the vendor's
frontend) and lets the library stay generic.

### OpenCL C compatibility ABI

For frontends that need OpenCL-style `double` behavior without changing the
strict IEEE `sf64_*` symbols, build with:

```bash
cmake -S . -B build-ocl -DSOFT_FP64_OCL=on -DSOFT_FP64_FTZ=on
```

This emits an additive `sf64_ocl_*` ABI that forwards through the same
verified implementation while applying the configured binary64 FTZ policy at
entry/exit. `SOFT_FP64_FTZ=on` flushes binary64 subnormal inputs and outputs
to signed zero on that OpenCL surface only; the base `sf64_*` ABI remains
subnormal-preserving. The same build also exports OpenCL spelling aliases for
`native_{sin,cos,tan,exp,exp2,exp10,log,log2,log10,sqrt,rsqrt,recip,divide,powr}`
as `sf64_native_*`. The transcendental native entries use a deliberately
looser implementation tier (shorter reduction / polynomial paths, with
strict fallbacks for extreme special cases) and do not carry the u10/u35
precision claims in the table below; `sqrt`, `rsqrt`, `recip`, and `divide`
reuse the strict cores.

### AdaptiveCpp Metal backend

The AdaptiveCpp Metal SSCP path — used by downstream GPU consumers on
Apple Silicon, where the GPU lacks native fp64 — used to ship as a
bundled adapter under `adapters/acpp_metal/`. As of v1.2.0 that glue
lives in AdaptiveCpp itself (the `__acpp_sscp_*_f64` forwarders are
AdaptiveCpp ABI symbols, not soft-fp64's public surface). See
[`yocontra/AdaptiveCpp`](https://github.com/yocontra/AdaptiveCpp) on
the `fork-safe-metal` branch for the integration; soft-fp64 is
consumed there as a generic source dependency via the public `sf64_*`
ABI plus the `src/` and `src/sleef/` source trees (exposed by
`find_package(soft_fp64)` as `soft_fp64_SOURCE_DIR` /
`soft_fp64_SLEEF_SOURCE_DIR`).

## Precision guarantees

The binary64 tests use the oracles named in the last column. Transcendental
accuracy sweeps use MPFR at 200-bit precision; core operations also use host
references and Berkeley TestFloat. These are bounds enforced on the tested
corpora, not exhaustive proofs over all binary64 inputs.

Tiers: `BIT_EXACT` (0 ULP), `U10 = 4 ULP`, `U35 = 8 ULP`, `GAMMA = 1024 ULP`.

| Category | Examples | Tier | Range / oracle |
|---|---|---|---|
| Arithmetic | `add`, `sub`, `mul`, `div`, `rem`, `fma`, `sqrt`, `remainder`, `fmod` | **BIT_EXACT** | host FPU + TestFloat vectors + `test_arithmetic_exact.cpp`, `test_sqrt_fma_exact.cpp`; `fmod` / `remainder` also 0-ULP vs MPFR in `test_mpfr_diff.cpp` |
| Conversion | `i{8,16,32,64} ↔ f64`, `u{8,16,32,64} ↔ f64`, `f32 ↔ f64` | **BIT_EXACT** | `test_convert_widths.cpp`; exhaustive 2³² `f32 → f64 → f32` round-trip |
| Comparison / classification | `fcmp` (all 16 IEEE-754 predicates), `isnan`, `isinf`, `isfinite`, `isnormal`, `signbit`, `fabs`, `copysign` | **BIT_EXACT** | TestFloat vectors + `test_compare_all_predicates.cpp` |
| Rounding | `floor`, `ceil`, `trunc`, `rint`, `round`, `fract`, `modf`, `ldexp`, `frexp`, `ilogb`, `logb` | **BIT_EXACT** | `test_rounding_edges.cpp` |
| Transcendentals (u10) | `sin`, `cos`, `asin`, `acos`, `atan`, `atan2`, `exp`, `exp2`, `exp10`, `expm1`, `log`, `log2`, `log10`, `log1p`, `cbrt`, `cosh`, `acosh`, `atanh`, `sinpi`, `cospi`, `asinpi`, `acospi`, `atanpi`, `atan2pi` | **U10** ≤ 4 ULP | `test_mpfr_diff.cpp` |
| Transcendentals (u35) | `tan`, `tanpi`, `sinh`, `tanh`, `asinh` | **U35** ≤ 8 ULP | `test_mpfr_diff.cpp` |
| General power | `pow(x, y)` | **U35** ≤ 8 ULP | three sampled windows, see note |
| `powr` | `powr(x, y)` | **U35** ≤ 8 ULP | `x` log-sampled in `[1e-6, 1e6]`, `y` uniform in `[-50, 50]`; `test_mpfr_diff.cpp` |
| `pown` | `pown(x, n)` | **U35** ≤ 8 ULP | `x` uniform in `[-4, 4]`, integer `n` in `[-10, 10]`; `test_mpfr_diff.cpp` |
| `rootn` | `rootn(x, n)` | **U35** ≤ 8 ULP | `x` log-sampled in `[0.01, 1e10]`, `n ∈ {2,3,4,5,7,11}`; `test_mpfr_diff.cpp` |
| `erf` | `erf(x)` | **U10** ≤ 4 ULP | `[-5, 5]`, `test_mpfr_diff.cpp` (SLEEF u1 port) |
| `erfc` | `erfc(x)` | **U10** ≤ 4 ULP | `[-5, 27]`, `test_mpfr_diff.cpp`, including the deep tail |
| `tgamma` | `tgamma(x)` | **U10** ≤ 4 ULP | `[0.5, 170]`, `test_mpfr_diff.cpp` |
| `lgamma`, `lgamma_r` | `lgamma(x)` | **U10** ≤ 4 ULP zero-free tail; **GAMMA** at zero-crossings | `[3, 1e4]` U10 tail + `[0.5, 3]` GAMMA near zeros at `x=1, 2`; `test_mpfr_diff.cpp` |

### `sf64_pow` — bounded-region U35

`sf64_pow` evaluates `exp(y · log(|x|))`, carrying the logarithm and product
in double-double form into the internal exponential kernel. Special-case and
sign handling surround that positive-magnitude calculation. CI gates these
named sweeps in `tests/mpfr/test_mpfr_diff.cpp`:

| Sweep | x sampling | x range | y sampling | y range | Tier | Oracle | Samples |
|---|---|---|---|---|---|---|---|
| `pow` | log | `[1e-6, 1e6]` | uniform | `[-50, 50]` | U35 (8 ULP) | MPFR (200 bits) | 10000 |
| `pow-xbig` | log | `[1e-100, 1e100]` | signed log | `[1e-5, 5]` | U35 (8 ULP) | MPFR (200 bits) | 10000 |
| `pow-ybig` | log | `[1e-6, 1e3]` | uniform | `[-100, 100]` | U35 (8 ULP) | MPFR (200 bits) | 10000 |

Signed-log sampling chooses a logarithmically distributed magnitude and a
random sign; the x-wide sweep excludes `|y| < 1e-5`. The README claim checker
binds these rows to their named sweep calls, sampling methods, tier, oracle,
and sample count. These results do not establish a bound over the full double
range.

`sf64_lgamma` and `sf64_lgamma_r` use SLEEF's shared `gammak` kernel and
reconstruct the result as `clc + log(abs(div))` in double-double arithmetic.
The central region uses branch-selected polynomials; larger positive inputs
use recurrence shifting and a Stirling-form tail. MPFR gates the near-zero
region `[0.5, 3]` at GAMMA and the `[3, 1e4]` tail at U10.

"Bit-exact" means every output matches an independent IEEE oracle in every
bit for the tested corpus, including signed zeros, subnormals, NaNs, and
infinities. It does not imply that finite testing proves every possible input;
the exhaustive and randomized oracle scopes are stated explicitly above.

## Performance

The harness in `bench/bench_soft_fp64.cpp` measures selected binary64
operations. Performance depends strongly on the compiler and
target, so this document does not publish portable timing claims. The
committed `bench/baseline.json` is an Apple M-series CI regression baseline,
not a cross-platform benchmark result. See `bench/README.md` for reproducible
build, measurement, comparison, and baseline-update instructions.

### Comparative bench

A separate harness `bench/bench_compare.cpp` measures `sf64_*` against
Berkeley SoftFloat 3e (core IEEE ops) and ckormanyos/soft_double (core
ops + transcendentals). Comparison libraries are vendored on demand via
`bench/fetch_external.sh`; the CI job `comparative-bench` runs on
manual workflow dispatch and uploads `compare.json` as an artifact.
See `bench/README.md` for the full comparison table and scope.
Informational — not a regression gate.

## Testing

The default suite combines complementary independent checks:

1. **Binary64 exactness** — edge corpora and randomized sweeps cover core
   arithmetic, conversions, all 16 comparison predicates, rounding, square
   root, and FMA. Berkeley TestFloat 3e contributes 38,783,837 generated
   vectors across every rounding mode, including exception flags.
2. **Binary64 transcendental accuracy** — a 200-bit MPFR oracle gates every
   published ULP contract, with dedicated sweeps for difficult regions such
   as reduction boundaries, poles, deep tails, and zero crossings.
3. **Binary128 and binary256 core exactness** — deterministic boundary suites
   cover encodings, special values, conversions, flags, and rounding. MPFR
   differential tests exercise every core arithmetic operation in all five
   rounding modes at the destination precision, with higher-precision source
   operands. Binary128 also has thread-isolation coverage.
4. **Structural checks** — the C11 consumer test includes the unified public
   surface, the ABI manifest rejects missing or leaked backend symbols, and an
   optimized LLVM-IR audit rejects host floating-point arithmetic in every
   production source.
5. **Long-running checks** — scheduled CI runs sanitizer-backed libFuzzer
   targets for all three formats and exhaustively round-trips all 2^32
   binary32 bit patterns through binary64.

See `tests/`, `tests/testfloat/`, `tests/mpfr/`, and `fuzz/` for the concrete
corpora and harnesses.

## Rounding modes

Core arithmetic defaults to nearest, ties-to-even (RNE); integer narrowing
defaults to truncation toward zero. Operations with an explicit rounding-mode
API provide an `_r`-suffixed variant that takes an
explicit mode from `include/soft_fp64/rounding_mode.h` (matching
IEEE-754 §4.3):

```c
#include "soft_fp64/soft_f64.h"

double a = sf64_add_r(SF64_RTZ, 1.0, 0x1p-53);               // round toward zero
int32_t i = sf64_to_i32_r(SF64_RUP, 1.5);                    // 2
double r = sf64_rint_r(SF64_RDN, -0.5);                       // -1.0
```

Mode enum values: `SF64_RNE`, `SF64_RTZ`, `SF64_RUP`, `SF64_RDN`,
`SF64_RNA`. The per-mode core tests compare result bits and flags with MPFR
and TestFloat. `neg`, `fabs`, `copysign`, comparisons, `frexp`, classification,
`fmod`, and `remainder` have no `_r` form. `ldexp` also has no `_r` form in
this API: it uses RNE when scaling loses bits at the subnormal boundary and
returns signed infinity on overflow.

## IEEE exception flags

Thread-local sticky flags for `INVALID`, `DIVBYZERO`, `OVERFLOW`,
`UNDERFLOW`, and `INEXACT`, exposed via:

```c
sf64_fe_clear(0x1Fu);                    // clear all flags
double r = sf64_div(1.0, 0.0);            // raises DIVBYZERO
unsigned f = sf64_fe_getall();            // f & SF64_FE_DIVBYZERO != 0
if (sf64_fe_test(SF64_FE_INVALID)) { …}

sf64_fe_state_t saved;
sf64_fe_save(&saved);                     // snapshot
// … scratch work that may raise flags …
sf64_fe_restore(&saved);                  // roll back
```

Flag storage is per-thread (`thread_local`). `SF64_FE_*` values form a stable
project-local bitmask; they are not guaranteed to equal any platform's
`<fenv.h>` constants, so bridges must map them by name. Build option
`SOFT_FP64_FENV`:

- `tls` (default on hosted builds) — thread-local accumulator.
- `disabled` — every raise-site compiles out and every `sf64_fe_*` entry
  becomes a no-op; zero runtime cost on the hot path.
- `explicit` — emits the caller-provided `sf64_*_ex` state ABI without
  thread-local storage; the legacy `sf64_fe_*` TLS surface is linkable but
  observably no-op.

```bash
cmake -S . -B build -DSOFT_FP64_FENV=disabled     # no flag overhead
cmake -S . -B build -DSOFT_FP64_FENV=explicit     # caller-state ABI
cmake -S . -B build                                # default: tls
```

Full-corpus flag parity is gated by `tests/testfloat/run_testfloat.cpp`
against Berkeley SoftFloat's `fl2` column (`-tininessbefore`, `-exact`).
sNaN-input rows go through the same flag gate; payload/sign policy is pinned
separately by `tests/test_snan_payload.cpp`.

## Non-goals

- No complex-number math.
- No fp16 / bfloat16 / decimal floating point.

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build -V
```

Binary128 and binary256 are built by default. Common configuration switches:

```bash
cmake -B build \
  -DSOFT_FP_BUILD_FP128=ON \
  -DSOFT_FP_BUILD_FP256=ON \
  -DSOFT_FP64_BUILD_TESTS=ON \
  -DSOFT_FP64_WERROR=ON
```

`SOFT_FP64_BUILD_EXHAUSTIVE`, `SOFT_FP64_BUILD_FUZZ`, and
`SOFT_FP64_BUILD_BENCH` enable the long exhaustive test, sanitizer fuzz
targets, and microbenchmarks respectively. Test and install rules default on
only when this is the top-level project, so `add_subdirectory` consumers do
not inherit project-only work.

The remaining non-blocking roadmap—wider transcendentals, conversion-matrix
extensions, native ISA kernels, wider benchmarks, and additional target
coverage—is tracked in the repository's `TODO.md`.

## SIMD and SIMT

The C++17 header `soft_fp/simd.h` provides allocation-free fixed-width packs,
lane-wise transforms for every scalar operation, per-lane explicit exception
state, and structure-of-arrays word planes for binary128 and binary256. It is
ISA-neutral and preserves the scalar bit/rounding contract lane by lane.

Use `SOFT_FP64_FENV=explicit` when every lane needs independent IEEE-754 flags,
or `SOFT_FP64_FENV=disabled` when flags are not needed. The default TLS mode is
thread-safe but its shared sticky flag reduction may inhibit compiler auto-
vectorization. See `SIMD.md` for examples, data-layout guarantees, and the
boundary between a SIMD-friendly API and target-specific native vector code.

## Release provenance

The release procedure requires an annotated version tag, reproducible tagged-
source archives, a SHA-256 manifest, generated API documentation, and an SPDX
SBOM. See `RELEASING.md` for the publication and verification steps; inspect a
specific release's assets for its available artifacts.

## License + attribution

MIT — see [`LICENSE`](LICENSE). Third-party code incorporated under their
respective licenses; full attribution in [`NOTICE`](NOTICE).

- Skeleton and public API shape derived from
  **[philipturner/metal-float64](https://github.com/philipturner/metal-float64)**
  (MIT, 2023).
- Arithmetic (add/sub/mul/div, sqrt, fma, compare, convert) ported from
  **Mesa** `src/compiler/glsl/float64.glsl` (BSD-3-Clause) and
  `src/compiler/nir/nir_lower_double_ops.c` (MIT).
- Transcendentals (sin/cos/tan/asin/acos/atan/exp/log/pow/erf/tgamma/…)
  ported from **SLEEF 3.6** `sleefinline_purec_scalar.h` + scalar sources
  (Boost-1.0).
- The production binary128 core uses an audited, symbol-isolated subset of
  **Berkeley SoftFloat 3e** (BSD-3-Clause); TestFloat 3e remains an independent
  conformance oracle.
