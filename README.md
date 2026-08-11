# soft-fp

[![release](https://img.shields.io/github/v/release/yocontra/soft-fp)](https://github.com/yocontra/soft-fp/releases/latest)
[![CI](https://github.com/yocontra/soft-fp/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/yocontra/soft-fp/actions/workflows/ci.yml)

Portable, bit-exact IEEE-754 soft-float libraries in pure integer code —
for any target without hardware floating-point of the relevant width.

Current stable release: **2.0.0** (2026-08-11).

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

The 2.0 wider-format contract is the complete IEEE core (arithmetic,
conversion, rounding, comparison, classification, flags, remainder, sqrt,
and FMA). Binary64 additionally provides the documented libm/transcendental
surface. Binary128/binary256 transcendental symbols are intentionally absent
until they have format-specific algorithms and MPFR accuracy gates; the
project does not ship host-libm forwarders or placeholder approximations.

## What soft-fp64 does

Lots of modern hardware ships without fp64 units: Apple GPUs, most mobile
and tile-based GPUs, WebGPU / OpenGL ES class devices, many embedded DSPs,
FPGAs, and custom accelerators. When scientific, geospatial, cryptographic,
or scientific-computing code hits one of these targets, the compiler either
traps, silently truncates to `float`, or refuses to lower the code at all.

`soft-fp64` is a clean, header-plus-object C++17 implementation of the full
IEEE-754 binary64 surface, built entirely on 32/64-bit integer bit
operations. There is no hidden dependency on the host FPU. Any frontend
that can emit a call to an `extern "C"` symbol can get correct `double`
behavior on a device without a hardware fp64 unit. The public binary64 ABI
uses `double` as a 64-bit bit-pattern carrier, so the compiler/IR must still
be able to represent the binary64 type in signatures. No emitted arithmetic
instruction depends on hardware fp64; CI verifies optimized LLVM IR. Source
languages with no binary64 type at all (notably WGSL) need a frontend adapter
that maps their integer-pair representation to these helpers during lowering.

## Where this is useful

Targets and toolchains that currently have no good story for `double`
when the hardware lacks an fp64 unit. Examples include:

- **SYCL on Apple GPUs.** AdaptiveCpp-style Metal SSCP frontends can
  compile soft-fp64 into libkernel bitcode and lower `double` kernels
  to integer-backed helper calls (much slower than fp32, but correct).
- **`torch.float64` on MPS.** PyTorch's MPS backend errors on fp64.
  A dispatch path backed by this library would produce a working
  (slow) tensor.
- **WebGPU compiler runtimes.** WGSL has no `f64`, so direct source-level
  calls are impossible. A Tint/Naga-style lowering pass can represent values
  as integer pairs and map generated binary64 operations to this library's
  integer-only IR before final WGSL/SPIR-V emission.
- **Mesa drivers without fp64.** Lima, Panfrost, older Freedreno,
  LLVMpipe-on-WASM reject fp64 OpenCL kernels. Mesa's in-tree
  softfloat covers arithmetic but not transcendentals; this library
  covers the full surface.
- **compiler-rt / libgcc softfloat comparison.** The `__adddf3` /
  `__muldf3` / `__divdf3` builtins used on RISC-V without D, ARMv6-M,
  and WASM-softfp have known subnormal / NaN-payload corner cases.
  soft-fp64 is TestFloat- and MPFR-verified bit-for-bit on the same
  inputs, so it's usable as a differential oracle.
- **Independent reference.** Every op is gated in CI against Berkeley
  TestFloat 3e plus a 200-bit MPFR oracle, with the precision table
  published, so the library can serve as an oracle for other
  softfloat implementations.

## Who this is for

- **Compiler / runtime authors** retargeting language front-ends to hardware
  without fp64 (AdaptiveCpp's MSL emitter is the first consumer; anyone
  doing the same for Vulkan / WebGPU / a custom ISA can link against the
  same ABI).
- **Shader / kernel authors** who need correct `double` in a single hot path
  and don't want to carry a full libm port.
- **Scientific / geospatial / financial code** being cross-compiled to an
  fp64-less target (mobile, embedded, browser) that previously required
  giving up precision or the target.
- **Test oracles** — the implementation is also a clean, self-contained
  reference for anyone validating their own soft-fp64 code.

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

Every bound below is **CI-gated** against a 200-bit MPFR oracle by the
sweep named in the last column. The tier for each row is the tightest
bound ctest enforces; measured worst-case may be inside it, but the
guarantee is the gate.

Tiers: `BIT_EXACT` (0 ULP), `U10 = 4 ULP`, `U35 = 8 ULP`, `GAMMA = 1024 ULP`.

| Category | Examples | Tier | Range / oracle |
|---|---|---|---|
| Arithmetic | `add`, `sub`, `mul`, `div`, `rem`, `fma`, `sqrt`, `remainder`, `fmod` | **BIT_EXACT** | host FPU + TestFloat vectors + `test_arithmetic_exact.cpp`, `test_sqrt_fma_exact.cpp`; `fmod` / `remainder` also 0-ULP vs MPFR in `test_mpfr_diff.cpp` |
| Conversion | `i{8,16,32,64} ↔ f64`, `u{8,16,32,64} ↔ f64`, `f32 ↔ f64` | **BIT_EXACT** | `test_convert_widths.cpp`; exhaustive 2³² `f32 → f64 → f32` round-trip |
| Comparison / classification | `fcmp` (all 16 IEEE-754 predicates), `isnan`, `isinf`, `isfinite`, `isnormal`, `signbit`, `fabs`, `copysign` | **BIT_EXACT** | TestFloat vectors + `test_compare_all_predicates.cpp` |
| Rounding | `floor`, `ceil`, `trunc`, `rint`, `round`, `fract`, `modf`, `ldexp`, `frexp`, `ilogb`, `logb` | **BIT_EXACT** | `test_rounding_edges.cpp` |
| Transcendentals (u10) | `sin`, `cos`, `asin`, `acos`, `atan`, `atan2`, `exp`, `exp2`, `exp10`, `expm1`, `log`, `log2`, `log10`, `log1p`, `cbrt`, `cosh`, `acosh`, `atanh`, `sinpi`, `cospi`, `asinpi`, `acospi`, `atanpi`, `atan2pi` | **U10** ≤ 4 ULP | `test_mpfr_diff.cpp` |
| Transcendentals (u35) | `tan`, `tanpi`, `sinh`, `tanh`, `asinh` | **U35** ≤ 8 ULP | `test_mpfr_diff.cpp` |
| `pow` / `powr` / `pown` / `rootn` | `pow(x, y)`, `powr(x, y)`, `pown(x, n)`, `rootn(x, n)` | **U35** ≤ 8 ULP, bounded region | three overlapping windows, see note |
| `erf` | `erf(x)` | **U10** ≤ 4 ULP | `[-5, 5]`, `test_mpfr_diff.cpp` (SLEEF u1 port; measured worst 1 ULP) |
| `erfc` | `erfc(x)` | **U10** ≤ 4 ULP | `[-5, 27]` (full active range incl. deep tail; measured worst 1 ULP) |
| `tgamma` | `tgamma(x)` | **U10** ≤ 4 ULP | `[0.5, 170]` (through the overflow boundary; measured worst 1 ULP) |
| `lgamma`, `lgamma_r` | `lgamma(x)` | **U10** ≤ 4 ULP zero-free tail; **GAMMA** at zero-crossings | `[3, 1e4]` U10 Lanczos tail + `(0.5, 3)` GAMMA (ULP ratio unbounded as `\|lgamma\| → 0` near `x=1, 2`; absolute error stays inside U10) |

### `sf64_pow` — bounded-region U35

`sf64_pow` is composed as `pow(x, y) = exp(y · log(x))` with a double-double
`y · log(x)` intermediate collapsed to a single `double` before the final
`sf64_exp`. CI gates three overlapping sweeps at **U35 ≤ 8 ULP**:

- main: `x ∈ [1e-6, 1e6]`, `y ∈ [-50, 50]`
- x-wide: `x ∈ [1e-100, 1e100]`, `y ∈ [-5, 5]`
- y-wide: `x ∈ [1e-6, 1e3]`, `y ∈ [-100, 100]`

Across the full double range, 1.1's `logk_dd` DD-Horner rewrite brings
measured worst-case `sf64_pow` inside U10 (≤4 ULP); the shipped U35
tier remains the gated ceiling.

The previous
`sf64_lgamma` zero-crossing caveat on `(0.5, 3)` was closed by
zero-centered Taylor branches in `src/sleef/sleef_special.cpp` (DD-Horner
on the `log Γ(1+z)` and `log Γ(2+z)` series, DLMF §5.7.3, with windows
`|x-1| ≤ 0.25` and `|x-2| ≤ 0.5`); the sweep graduated to
`tests/mpfr/test_mpfr_diff.cpp` at `GAMMA` tier.

"Bit-exact" means every output matches an independent IEEE oracle in every
bit for the tested corpus, including signed zeros, subnormals, NaNs, and
infinities. It does not imply that finite testing proves every possible input;
the exhaustive and randomized oracle scopes are stated explicitly above.

## Performance

The self-contained harness in `bench/bench_soft_fp64.cpp` measures every
public fp64 operation. Performance depends strongly on the compiler and
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

The default `sf64_*` entry points round to nearest, ties-to-even (RNE).
Every round-affected op also has a `_r`-suffixed variant that takes an
explicit mode from `include/soft_fp64/rounding_mode.h` (matching
IEEE-754 §4.3):

```c
#include "soft_fp64/soft_f64.h"

double a = sf64_add_r(SF64_RTZ, 1.0, 0x1p-53);               // round toward zero
int32_t i = sf64_to_i32_r(SF64_RUP, 1.5);                    // 2
double r = sf64_rint_r(SF64_RDN, -0.5);                       // -1.0
```

Mode enum values: `SF64_RNE`, `SF64_RTZ`, `SF64_RUP`, `SF64_RDN`,
`SF64_RNA`. Every `_r` entry is bit-exact against MPFR 200-bit and
Berkeley TestFloat 3e in every mode. Ops whose result does not depend
on the rounding attribute (`neg`, `fabs`, `copysign`, compares,
`ldexp`, `frexp`, classify, `fmod`, `remainder`) have no `_r` form
by design.

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

Each GitHub release publishes an annotated version tag, reproducible tagged-
source archives, a SHA-256 manifest, generated API documentation, and an SPDX
SBOM. See `RELEASING.md` for the release and verification procedure.

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
