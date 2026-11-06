# Changelog

All notable changes to `soft-fp` land here. Format loosely follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Entries describe the corresponding release. Historical measurements are tied
to their recorded configurations; the current accuracy gates are in `README.md`.

## [Unreleased]

No unreleased changes.

## [2.0.2] — 2026-11-05

Validation and documentation fixes; the numerical algorithms and public ABI
are unchanged.

### Fixed

- Moved macOS CI jobs from the retired `macos-14` image to `macos-26`,
  retaining Apple Silicon coverage and the existing benchmark budgets.
- Kept runtime test expectations active in Release builds and replaced the
  ineffective address-only linkage smoke with checks that exercise the API.
- Made benchmark comparisons reject incomplete or malformed measurements by
  default; filtered local comparisons require explicit partial-run opt-in.
- Bound README power-accuracy claims to named executable sweeps, including
  sampling, bounds, oracle, tier, and sample count.
- Corrected build recipes, precision descriptions, FMA/power explanations,
  and test names. Removed task narration and duplicated implementation comments.

### Documentation corrections

- Earlier version 1.1 notes claimed measured power accuracy across the full
  double range. The checked-in bounded sweeps do not establish that claim;
  current documentation states their exact domains instead.
- Historical `hypot` wording mixed correct rounding with a one-ULP claim. The
  documented MPFR gate is U10 (4 ULP) on the tested domain.
- Previous descriptions of dedicated DLMF Taylor windows and a Lanczos
  implementation for `lgamma` were inaccurate. The implementation uses the
  SLEEF `gammak` polynomial/recurrence/Stirling kernel.
- The version 1.1 performance discussion is now a dated note preserving its
  measurements and correcting an inconsistent `to_i32` baseline comparison.

## [2.0.1] — 2026-08-11

Patch release for freestanding device-library integration and release-gate
portability. The numerical API and precision contracts are unchanged.

### Fixed

- Removed SLEEF's dynamically initialized infinity variable. Installed-source
  consumers such as AdaptiveCpp's Metal libkernel now receive pure,
  integer-derived bit casts without `llvm.global_ctors`, C++ guard state, or
  runtime initialization in any SLEEF translation unit.
- Extended the optimized LLVM IR audit to reject global constructors and C++
  guard routines in production sources, preventing device-incompatible dynamic
  initialization from returning.
- Made the no-host-FP audit's optional compiler arguments safe under the system
  Bash 3.2 shipped by supported macOS runners.

## [2.0.0] — 2026-08-11

First suite-wide release of `soft-fp`. Version 2 retains the complete
binary64 API and adds production binary128 and binary256 IEEE-754 cores in
the same repository, build, install, and package surface.

### Added

- Unified `soft_fp` 2.0 package and umbrella target, retaining the binary64
  compatibility package.
- Production IEEE binary128 and binary256 core libraries with explicit word
  ABIs, all IEEE rounding modes, caller-owned exception state, conversions,
  comparisons, remainder, square root, fused multiply-add, and oracle tests.
- Generated feature/semantic configuration, C11 API smoke coverage,
  deterministic source manifests, and an emitted-LLVM-IR host-FP gate.
- MPFR differential suites and sanitizer fuzz targets for binary128 and
  binary256, plus binary128 thread-isolation coverage.
- Stable `soft_fp::fp64`, `soft_fp::fp128`, `soft_fp::fp256`, and
  `soft_fp::soft_fp` CMake targets, plus the `soft_fp/soft_fp.h` umbrella
  header and `find_package(soft_fp 2 CONFIG)` package.
- ABI-manifest enforcement that checks every public declaration against the
  installed archives and rejects leaked Berkeley SoftFloat backend state.
- A C++17 SIMD/SIMT integration header with arbitrary fixed lane counts,
  allocation-free load/store and transform helpers, per-lane explicit
  exception state, and binary128/binary256 structure-of-arrays word planes.
- `SIMD.md` defining the portable lane guarantees, fenv configuration,
  aliasing rules, wider-format layouts, and the boundary between the portable
  implementation and target-specific native vector kernels.

### Changed

- Renamed the top-level CMake project to `soft_fp` and set its package version
  to 2.0.0. The legacy `soft_fp64` CMake package, target, header paths,
  archive, and `sf64_*` C ABI remain supported.
- Replaced the native 128-bit binary64 helper dependency with portable
  two-limb integer arithmetic and a base-2^32 division implementation.
- Made the production source install manifest explicit and installed the
  SLEEF and Berkeley SoftFloat license and attribution files.
- Replaced the misleading `sleef_stubs.cpp` filename with
  `sleef_special.cpp`; these are real, accuracy-gated binary64 special
  functions, not placeholders.
- Updated immutable GitHub Actions pins to the Node 24-based checkout and
  artifact actions, eliminating the deprecated Node 20 compatibility path.

### Fixed

- Removed the compiler-native `__uint128_t` dependency from binary64.
- Prevented release optimizers from reconstructing host floating-point
  instructions while retaining an inline bit-carrier boundary; the IR gate
  now checks the actual `-O3` release level.
- Made `_ex` symbols consistently linkable in every fenv configuration.
- Corrected binary256 tininess-after-rounding flags at the minimum-normal
  boundary and isolated Berkeley SoftFloat's public symbols and state behind
  the binary128 backend prefix.
- Corrected `lgamma_r` sign output at NaNs and poles, C header compatibility,
  package feature visibility, attribution installation, and exception and
  integer-conversion documentation.
- Removed the empty experimental test and made exhaustive binary32 round-trip
  coverage independent of libFuzzer.
- Corrected GNU attribute placement, guarded compiler-specific visibility and
  TLS attributes, and fixed shift-count narrowing so GCC and MSVC warning-as-
  error builds compile cleanly.
- Corrected Berkeley SoftFloat helper linkage so binary128 works in Debug,
  sanitizer, and coverage builds rather than only when optimization happened
  to inline every helper.
- Removed undefined float-to-unsigned conversion from the narrow-integer fuzz
  harness and replaced the platform-libm remainder oracle with MPFR for
  extreme exponent-gap and subnormal cases.
- Made the reentrancy harness request an ASan-safe worker stack on Darwin,
  whose default secondary-thread stack is too small for instrumented Debug
  transcendental frames.
- Corrected the top-level attribution to identify Berkeley SoftFloat as the
  production binary128 backend as well as an oracle dependency.

### Validation

- Passed 38,783,837 Berkeley TestFloat 3e vectors across the binary64 core,
  including result bits and exception flags.
- Passed independent MPFR differential suites for binary64 transcendental
  accuracy and every binary128/binary256 core operation in all five rounding
  modes.
- Passed the optimized LLVM-IR no-host-floating-point audit across every
  production source, C11/ABI/install consumers, alternate fenv/OpenCL/FTZ
  configurations, sanitizer fuzz targets, documentation checks, and the
  performance regression gate.
- Passed lane-container and word-plane round trips across binary64, binary128,
  and binary256 in the default, explicit-fenv, and disabled-fenv test matrices.

## [1.3.0] — 2026-05-12

Additive release on top of 1.2. Adds the OpenCL C compatibility ABI,
the OpenCL `native_*` spelling tier, explicit-state remainder variants,
and an opt-in IEEE 754 §6.2.3 sNaN payload propagation policy.

### Added

- **OpenCL C compatibility ABI (`SOFT_FP64_OCL=on`).** New optional
  `src/ocl.cpp` emits a parallel `sf64_ocl_*` surface that forwards to
  the strict `sf64_*` implementation while applying the configured
  OpenCL denormal policy at ABI entry/exit. `SOFT_FP64_FTZ=off|on`
  controls whether binary64 subnormal inputs and outputs on that
  OpenCL surface are preserved or flushed to signed zero; the base
  `sf64_*` ABI remains subnormal-preserving. The same build emits a
  loose native tier for
  `native_{sin,cos,tan,exp,exp2,exp10,log,log2,log10,sqrt,rsqrt,recip,divide,powr}`
  as `sf64_native_*`; transcendental native entries use shorter
  reduction / polynomial paths instead of aliasing the u10 SLEEF cores
  (`sqrt`, `rsqrt`, `recip`, and `divide` intentionally reuse strict
  arithmetic cores). `tests/test_ocl_mode.cpp` gates FTZ-on and FTZ-off
  behavior for subnormal edges plus native smoke cases, `run_testfloat`
  replays arithmetic streams through the OCL ABI with the configured FTZ
  policy, and `test_mpfr_diff` adds an OCL flushed-edge row when
  `SOFT_FP64_OCL=on`.
- **`sf64_fmod_ex` / `sf64_remainder_ex`.** The exact SLEEF-side
  remainder functions now have caller-state ABI variants, closing the
  explicit-fenv gap called out by the OpenCL-mode work. Their normal
  finite paths still swallow inner arithmetic flags per IEEE §5.3.1;
  only observable `INVALID` raises are flushed into TLS or the caller's
  `sf64_fe_state_t`. `tests/testfloat/run_testfloat.cpp` now routes
  `f64_rem` through `sf64_remainder_ex` under
  `SOFT_FP64_FENV=explicit`, so value and `fl2` flag checks run in that
  mode too.
- **`SOFT_FP64_SNAN=quiet|propagate` build option.** Default `quiet`
  preserves pre-1.2 behaviour at the four sites where soft-fp64 today
  emits a non-source-derived NaN (`sf64_sub` / `sf64_sub_r` corrupting
  source-NaN sign via the `b ^ kSignMask` fast-path; `sf64_fmod` /
  `sf64_remainder` returning canonical qNaN on any NaN input).
  `propagate` is IEEE 754-2008 §6.2.3-strict: the source NaN's sign
  and payload bits 50:0 survive every op, with bit 51 (quiet bit)
  forced. Wired as `SOFT_FP64_SNAN_PROPAGATE=0|1` PRIVATE on the lib
  target (does not leak into install consumers' compile lines).
  Implementation diff is bounded: the propagation paths
  (`propagate_nan`, `sqrt`, `fma`, f32 widen / narrow) already
  preserved sign + payload via `from_bits(src | kQuietNaNBit)` in
  both modes, so no patch was needed there — only `sf64_internal_sub_rne`
  (`src/internal_arith.h`), the new `sub_r_impl` helper consolidating
  `sf64_sub_r` + `sf64_sub_r_ex` in `src/arithmetic.cpp`, and the
  fmod / remainder NaN-result sites via the new
  `propagate_nan_xy(x, y)` helper in `src/sleef/sleef_internal.h`.
- **`tests/test_snan_payload.cpp` — bit-exact sNaN sign / payload probe.**
  Asserts the contract under both `SOFT_FP64_SNAN_PROPAGATE` values via
  `#if`-gated bit comparisons against hand-derived patterns (no oracle
  involved — the §6.2.3 contract is implementation-defined and the
  test pins it directly). Covers `add` / `sub` / `mul` / `div` / `sqrt`
  / `fma` / `from_f32` / `to_f32` / `fmod` / `remainder` with both
  signs of sNaN (`0x7FF00000DEADBEEF`, `0xFFF00000CAFEBABE`); spot-
  checks `SF64_FE_INVALID` is still raised on sNaN inputs.
- **CI cells `build-test-snan-propagate` and `build-test-ocl-ftz`.**
  The sNaN cell exercises `-DSOFT_FP64_SNAN=propagate`; the OpenCL cell
  exercises `-DSOFT_FP64_OCL=on -DSOFT_FP64_FTZ=on`. The default matrix
  covers quiet/subnormal-preserving behavior.

### Fixed

- **`sf64_sub(x, sNaN)` corrupted the source NaN's sign under
  `SOFT_FP64_SNAN=propagate`.** `sf64_internal_sub_rne` and
  `sf64_sub_r` historically implemented `a - b` as
  `add(a, neg(b))` via an unconditional `bb ^ kSignMask`. When `b`
  was the source NaN that won the §6.2.3 selection, the result
  emerged with the sign bit flipped — a §6.2.3 violation. Both
  paths now run a sNaN-aware fast-path under propagate mode that
  raises `SF64_FE_INVALID` and returns `propagate_nan(ab, bb)`
  before the XOR sees `b`. The non-NaN hot loop is unchanged in
  both modes; under the default `quiet` mode the historical
  sign-flip behaviour is preserved bit-exactly (and pinned by the
  new probe test) so this is purely additive.
- **`sf64_fmod` / `sf64_remainder` discarded source NaN sign +
  payload under `SOFT_FP64_SNAN=propagate`.** Both ops returned
  `qNaN()` (`0x7FF8000000000000`) on any NaN input, losing the
  source NaN's bits. The new `propagate_nan_xy(x, y)` helper in
  `src/sleef/sleef_internal.h` selects the source NaN per §6.2.3
  (prefer `x` if NaN, else `y`) and OR's the quiet bit; under
  `quiet` mode it falls back to `qNaN()` exactly. INVALID raise
  on sNaN remains in place at both call sites.

## [1.2.0] — 2026-04-27

Substantial release. Closes the AGX recursion-hang surfaced by
AdaptiveCpp's Metal pipeline, pulls all the open `Post-1.1` numerical
work onto the shipped surface, lands the `SOFT_FP64_FENV=explicit`
caller-state ABI that GPU / SIMT consumers need (Metal, WebGPU, OpenCL
device code — anywhere `thread_local` is unavailable), and extracts
the bundled `adapters/acpp_metal/` glue out into AdaptiveCpp's own
tree so soft-fp64 returns to being a generic IEEE-754 binary64
soft-float library that knows nothing about its consumers. The
`CMakeLists.txt` project version was also bumped from the stale `1.0.0`
pin to `1.2.0` so `find_package(soft_fp64)` reports the right value.

### Fixed

- **`SF64_NO_OPT` hardening on AGX cycle-risk ABI bodies.** Clang `-O3`
  InstCombine pattern-matches the bit-twiddle bodies of `sf64_fabs`,
  `sf64_copysign`, `sf64_neg`, and the inlined predicates inside
  `sf64_fcmp` back into `llvm.fabs.f64` / `llvm.copysign.f64` /
  `fneg double` / `fcmp <pred> double`. AdaptiveCpp's Metal emitter
  then routes those intrinsics through `__acpp_sscp_*_f64` wrappers
  whose bodies delegate back to the same `sf64_*` symbols — infinite
  mutual recursion that silently watchdog-hangs Apple Silicon AGX
  (`kIOGPUCommandBufferCallbackErrorHang`). New `SF64_NO_OPT
  __attribute__((optnone))` macro (clang-only; defined in
  `include/soft_fp64/defines.h`) applied to the four cycle-risk
  function definitions keeps the integer ops intact through
  optimization. Verified via `llvm-dis` on the staged adapter
  sources: each of `@sf64_fabs` / `@sf64_copysign` / `@sf64_neg` /
  `@sf64_fcmp` emits pure integer ops with zero `llvm.fabs.f64` /
  `llvm.copysign.f64` / `fneg double` / `fcmp <pred> double` in its
  body.
- **`sf64_*` raises `SF64_FE_INVALID` on sNaN inputs.** Per IEEE 754
  §7.2 every arithmetic / sqrt / fma / convert / fmod / remainder
  operation that takes a sNaN operand must raise INVALID. Previously
  the public ops quietly routed the sNaN through `propagate_nan()`
  with no flag side-effect. Now wired through every relevant path —
  `sf64_internal_{add,mul,div,sqrt,fma}_rne`, the mode-parametric
  `_r` paths in `arithmetic.cpp`, `sqrt_fma.cpp`, the f32→f64 / f64→
  f32 paths in `convert.cpp`, and `sf64_fmod` / `sf64_remainder` in
  `src/sleef/sleef_inv_hyp_pow.cpp`. `sf64_neg` / `sf64_fabs` /
  `sf64_copysign` deliberately do **not** raise per IEEE 754 §6.3
  (non-arithmetic operations).
- **TestFloat sNaN-input carve-out lifted.** Removed
  `is_snan_f64_bits` / `is_snan_f32_bits` helpers and the
  `if (kFlagsActive && !has_snan_input)` guard from all 9 runners in
  `tests/testfloat/run_testfloat.cpp`. Every Berkeley TestFloat
  vector — including sNaN-input rows that previously skipped the
  `fl2` flag check — now goes through the unconditional gate. The
  earlier 1.1.0 prose claim of "full-corpus validation against
  TestFloat 3e's `fl2` column (7.16M vectors)" is now factually true.
- **`CMakeLists.txt` project version pinned at `1.0.0` through 1.1.0.**
  Bumped to `1.2.0` in this release; `find_package(soft_fp64)` now
  reports the shipped tag. The 1.0.0 pin had survived the 1.1.0
  release commit, so anyone consuming via CMake saw `1.0.0` against a
  v1.1.0 archive.

### Added

- **`tests/test_fenv_threads.cpp` — TLS accumulator thread-safety test.**
  Two-thread stress: Thread A raises only `INVALID` (`sf64_div(0,0)`),
  Thread B raises only `DIVBYZERO` (`sf64_div(1.0, 0.0)`); 10 000
  iterations per thread; `std::atomic` rendezvous (no `sleep()`); per-
  iter clear-then-op-then-assert-exact-bit. Asserts no cross-thread
  bit leakage. Compiles to a no-op stub under
  `SOFT_FP64_FENV=disabled`. ThreadSanitizer-clean on AppleClang.
- **Portable 64×64 → 128 multiply for `sf64_fma`.** Previously
  `__uint128_t` was assumed; MSVC, some wasm32 toolchains, and 32-bit
  MCU SDKs do not provide it. New `mul64x64_to_128` primitive in
  `src/internal.h` carries either a native `__uint128_t` multiply or
  a portable schoolbook (four 32×32→64 partial products carry-
  propagated through the middle column, mirroring Berkeley SoftFloat
  3e `s_mul64To128.c`). Selection gate: native if `__SIZEOF_INT128__`
  is defined and `SF64_FORCE_PORTABLE_U128` is not. The new
  `build-test-portable-u128` CI cell (Ubuntu 24.04, clang Release,
  `-DSF64_FORCE_PORTABLE_U128=1`) runs the full ctest tree under the
  schoolbook path so it stays linked, exercised, and bit-for-bit
  identical to the native path.
- **Internal classify / manipulate helpers.** New
  `src/internal_classify.h` exposes hidden-visibility, header-inlined
  lifts of `sf64_{fcmp,trunc,ldexp,frexp,fabs}` so SLEEF DD primitives
  avoid the cross-TU public-ABI cost on these entries. `sf64_pow`
  alone made ~12 cross-TU calls to these symbols (each a full call
  frame plus a `__tlv_get_addr` roundtrip in tls fenv mode). After
  the rewire `sf64_pow` body's `bl` census drops from 12 to 3
  (remaining: `sf64_internal_logk_dd`, `sf64_internal_expk_dd`, and
  the fenv TLS thunk). Public `sf64_{fcmp,trunc,fabs,ldexp,frexp,
  neg}` unchanged. The `SF64_NO_OPT` optnone hardening on the
  cycle-risk public entries is preserved — internal helpers are a
  parallel surface, not a replacement.
- **`SOFT_FP64_FENV=explicit` caller-state ABI.** Parallel
  `sf64_*_ex(..., sf64_fe_state_t* state)` surface mirroring the
  default arithmetic / sqrt / fma / convert ABI. The state pointer
  receives the OR'd flag bits directly (a null pointer drops flags);
  thread-local storage is omitted entirely under explicit mode, so
  Apple Metal / WebGPU / OpenCL device kernels — where
  `thread_local` is not available — can finally observe fenv flags.
  Includes `sf64_fe_*_ex(state, ...)` for raise/clear/save/restore/
  getall/test, plus `sf64_*_r_ex` mode-parametric variants and
  `sf64_from_f32_ex`. The default TLS surface is unchanged under
  `SOFT_FP64_FENV=tls` and compiles to no-op stubs under `explicit`.
  New `build-test-fenv-explicit` CI cell exercises the `_ex` ABI
  end-to-end (38 M+ TestFloat vectors, full flag check on the
  arithmetic / sqrt / fma / convert surface).

### Tightened

- **`erf`, `erfc`, `tgamma`, `lgamma` → U10 (≤ 4 ULP).** Ports of
  SLEEF 3.6 `xerf_u1` / `xerfc_u15` / `xtgamma_u1` / `xlgamma_u1`
  faithfully into `src/sleef/sleef_stubs.cpp`; coefficients
  transcribed verbatim from upstream `sleefdp.c`. Polynomial
  evaluation uses the existing `poly_array` helper (Horner) instead
  of upstream's Estrin POLY21 — value-equivalent but slightly looser
  intermediate rounding, which is why the target tier is U10 not u1.
  Three new cross-TU internal helpers (hidden visibility,
  `sf64_internal_*` prefix; not on the public ABI surface) underpin
  the ports: `sf64_internal_expk2_dd` (DD → DD exp),
  `sf64_internal_logk2_dd` (DD → DD log) in
  `src/sleef/sleef_inv_hyp_pow.cpp`, and `sf64_internal_sinpik_dd`
  (double → DD sinπ) in `src/sleef/sleef_trig.cpp`. New DD
  primitives `ddabs_dd_dd` and `ddsub_dd_dd_dd` added to
  `src/sleef/sleef_common.h`. Was GAMMA (≤ 1024 ULP). Measured ULP
  vs MPFR 200-bit (n = 10 000 each, see
  `tests/mpfr/test_mpfr_diff.cpp`):

  | Function | Worst observed ULP |
  |----------|--------------------|
  | `erf`     | 1                  |
  | `erfc`    | 1                  |
  | `tgamma`  | 1                  |
  | `lgamma`  | 1 (zero-free tail) |
  | `lgamma_r`| 1 (zero-free tail) |

- **`sf64_lgamma` zero-crossings on `(0.5, 3)` graduated to the
  shipped suite.** Was report-only in
  `tests/experimental/experimental_precision.cpp`; now gated in
  `tests/mpfr/test_mpfr_diff.cpp` as `lgamma_zeros` /
  `lgamma_r_zeros` at GAMMA. The pre-fix baseline absolute error
  exceeded 76 000 ULP near the zeros; the SLEEF u1 port drops that
  to ≤ 4 ULP absolute (within U10 in absolute terms). The sweep
  stays at GAMMA because the ULP *ratio* is unbounded as
  `|lgamma| → 0` near `x = 1` and `x = 2`, even though the absolute
  number is comfortably inside U10.

### Removed

- **Bundled `adapters/acpp_metal/` directory.** The `__acpp_sscp_*_f64`
  and `__acpp_sscp_soft_f64_*` forwarders that lived under it are
  AdaptiveCpp ABI symbols, not soft-fp64's, and now live in
  AdaptiveCpp's own tree (`yocontra/AdaptiveCpp` on the
  `fork-safe-metal` branch). Deleted from this repo: the adapter
  `CMakeLists.txt`, the staging-rewrite helpers under `cmake/`
  (`rewrite_sleef_include.cmake`, `stage_internal_fenv.cmake`,
  `check_forwarder_symbols.cmake`), the forwarder TUs
  (`acpp_metal_math.cpp`, `acpp_metal_primitives.cpp`), and the
  adapter `README.md`.
- **`SOFT_FP64_BUILD_ACPP_METAL_ADAPTER` CMake option and the
  `soft_fp64_acpp_metal_stage` target.** Both gone. Top-level
  `CMakeLists.txt` no longer references the adapter directory.
- **`acpp-metal-smoke` CI job** in `.github/workflows/ci.yml`. The
  bitcode-link smoke test moves to AdaptiveCpp's own CI alongside
  the absorbed glue.

### Layout contract for downstream source consumers

Consumers that need to compile the `sf64_*` bodies themselves
(AdaptiveCpp's libkernel bitcode build globs `.cpp` files directly
into a SYCL/SSCP target rather than linking the host archive) can
rely on the following, stable for the v1.2.x line:

- `src/*.cpp` — arithmetic, compare, convert, sqrt/fma, rounding,
  classify, fenv. Hidden-visibility internal headers ride alongside
  (`internal*.h`) and are not promoted to the public include set.
- `src/sleef/*.cpp` — SLEEF-derived transcendentals (exp/log, trig,
  inv/hyp/pow, stubs). See `src/sleef/NOTICE` for attribution.
- `include/soft_fp64/*.h` — public C ABI; unchanged.

When consumed via `find_package(soft_fp64)`, the install also
exposes the source tree under `${CMAKE_INSTALL_DATADIR}/soft_fp64/src`
and surfaces `soft_fp64_SOURCE_DIR` / `soft_fp64_SLEEF_SOURCE_DIR` /
`soft_fp64_INCLUDE_DIR` as resolved absolute paths in
`soft_fp64Config.cmake`.

### Repository

- **Renamed `yocontra/soft-fp64` → `yocontra/soft-fp`.** The repo now hosts
  a soft-float library suite; `soft-fp64` is the first (and currently
  only) shipped library. A `soft-fp128` sibling lands later in this same
  tree (see `TODO.md`). Existing surface is unchanged: CMake project name
  stays `soft_fp64`, the artifact stays `libsoft_fp64.a`, `find_package
  (soft_fp64)` still works, the public `sf64_*` C ABI prefix is stable,
  and the `include/soft_fp64/` header path is unchanged. Only the
  GitHub URL moves.

[Unreleased]: https://github.com/yocontra/soft-fp/compare/v2.0.2...HEAD
[2.0.2]: https://github.com/yocontra/soft-fp/releases/tag/v2.0.2
[2.0.1]: https://github.com/yocontra/soft-fp/releases/tag/v2.0.1
[2.0.0]: https://github.com/yocontra/soft-fp/releases/tag/v2.0.0
[1.3.0]: https://github.com/yocontra/soft-fp/releases/tag/v1.3.0
[1.2.0]: https://github.com/yocontra/soft-fp/releases/tag/v1.2.0

## [1.1.0] — 2026-04-24

Additive release on top of 1.0. Three integrity-layer features land:
non-RNE rounding modes, IEEE-754 exception flags with thread-local
`fenv`, and the precision closures that were parked non-blocking in 1.0.
No breaking changes — the default `sf64_*` surface keeps RNE semantics
and silent (non-raising) exception behavior when `SOFT_FP64_FENV=disabled`.

### Added

- **Non-RNE rounding modes.** `sf64_*_r(mode, …)` surface on every
  round-affected op. Covered entries: `sf64_add_r`, `sf64_sub_r`,
  `sf64_mul_r`, `sf64_div_r`, `sf64_sqrt_r`, `sf64_fma_r`,
  `sf64_to_f32_r`, `sf64_to_{i8,i16,i32,i64,u8,u16,u32,u64}_r`,
  `sf64_rint_r`. The `sf64_rounding_mode` enum lives in
  `include/soft_fp64/rounding_mode.h` and matches IEEE-754 §4.3:
  `SF64_RNE`, `SF64_RTZ`, `SF64_RUP`, `SF64_RDN`, `SF64_RNA`.
  Bit-exact vs. MPFR 200-bit and Berkeley TestFloat 3e in all five
  modes (`tests/mpfr/test_mpfr_diff.cpp` sweeps per-mode,
  `tests/testfloat/run_testfloat.cpp` replays per-mode TestFloat
  vectors). Ops whose result is mode-independent — `neg`, `fabs`,
  `copysign`, compares, `ldexp`, `frexp`, classify, `fmod`,
  `remainder`, `floor`, `ceil`, `trunc`, `round` — do **not** get
  `_r` variants by design.
- **IEEE-754 exception flags + thread-local `fenv`.** `sf64_fe_*`
  surface: `SF64_FE_{INVALID,DIVBYZERO,OVERFLOW,UNDERFLOW,INEXACT}`
  flag bits, `sf64_fe_getall` / `sf64_fe_test` / `sf64_fe_raise` /
  `sf64_fe_clear` stickies, and `sf64_fe_save` / `sf64_fe_restore`
  opaque-state snapshots. Flag storage is per-thread
  (`thread_local unsigned`). Build option `SOFT_FP64_FENV` selects
  `tls` (default on hosted builds), `disabled` (all `sf64_fe_*`
  compile to no-ops; raise-sites compile out for zero runtime cost),
  or `explicit` (reserved for a caller-provided-state ABI in a future
  release; compiles as `disabled` today). Bit layout matches
  `<fenv.h>` conventions. Every raise site (INVALID on `sqrt(<0)` /
  `0×∞` fma / `fmod(x,0)` / NaN-to-int / OOR-to-int, DIVBYZERO on
  finite/0 division, OVERFLOW on exp-field saturation, UNDERFLOW on
  tiny-before-rounding+inexact, INEXACT on nonzero guard-or-sticky
  in the round-pack) is gated by `test_fenv.cpp` spot-checks plus
  full-corpus validation against Berkeley TestFloat 3e's `fl2`
  column (7.16M vectors).
- **`sf64_fma` 0×∞ with NaN addend.** IEEE-754 §7.2 requires
  `INVALID` to signal when the (a·b) sub-operation is 0×∞,
  regardless of `c`. `src/sqrt_fma.cpp` now raises `INVALID` on
  that path before NaN propagation short-circuits.
- **`sf64_fmod` / `sf64_remainder` exact-result flags.** IEEE-754
  §5.3.1 specifies these ops as exact: only `INVALID` (on
  `x=±inf` or `y=0`) may be raised. Both functions now snapshot
  the `fenv` before their internal sub/div scratch ops and restore
  afterward, preventing spurious `INEXACT` / `UNDERFLOW` leakage.
- **`logk_dd` DD-Horner rewrite.** `src/sleef/sleef_inv_hyp_pow.cpp`
  now evaluates the log tail polynomial in full double-double Horner
  form (~100+ bits of accumulated precision) instead of running the
  tail against `x².hi` as a plain double (capped at ~2⁻⁵⁶). Main
  consumer is `sf64_pow` — worst-case drops from ~40 ULP to ≤4 ULP
  in the near-unit-base × huge-exponent corner. The `tests/mpfr/`
  `sf64_pow` sweeps remain U35 (≤8 ULP) as a documented ceiling;
  measured worst-case is now inside U10.
- **`sf64_sinh` overflow-boundary refinement.** The large-|x| branch
  now handles `|x| ∈ (709.78, 710.4758]` by evaluating `expk_dd` on
  a DD pair `(a - kL2U, -kL2L)` rather than flushing to ±inf at
  `log(DBL_MAX)`. Previously sinh returned ±inf in that window
  (`exp(|x|)` would overflow before the ×½), now it returns the
  correct finite ~±DBL_MAX result. Threshold is
  `log(2·DBL_MAX) ≈ 710.4758600739439`.
- **Payne–Hanek deep-reduction breadth.** `test_transcendental_libm.cpp`
  `ks[]` extended from `{2⁴⁰, 2⁴⁵, 2⁵⁰}` to include `2⁵⁰⁰` and `2⁹⁰⁰`,
  matching the coverage already in `tests/test_coverage_mpfr.cpp`.
  Adds a libm-oracle signal for the deep-reduction regime.
- **TestFloat `fl2` oracle parity.** `tests/testfloat/run_testfloat.cpp`
  now parses Berkeley SoftFloat's `fl2` flag column and gates every
  vector on it (INEXACT / UNDERFLOW / OVERFLOW / DIVBYZERO / INVALID).
  TestFloat is generated with `-tininessbefore` (IEEE §7.5 tiny-
  before-rounding, matching MIPS/RISC-V) and `-exact` (IEEE §7.1
  INEXACT on lossy int truncation). sNaN-input rows are skipped
  pending sNaN payload preservation in 1.2.
- **New test harnesses.** `tests/test_rounding_modes.cpp` exercises
  every `_r` entry across the five modes; `tests/test_fenv.cpp`
  spot-checks every flag-raise site plus thread-isolation of the
  TLS accumulator (worker thread raises on its own state while
  main thread observes its own independent state).
- **Lint job migration.** CI `lint` job switched from ad-hoc
  `clang-format-19` apt install to [`prek`](https://github.com/j178/prek)
  running a pinned `.pre-commit-config.yaml`. Clang-format version
  stays locked to v19; rules are now co-located with the local
  pre-commit hook.

### Changed

- **`sf64_pow` documented precision.** Doxygen on
  `include/soft_fp64/soft_f64.h` records the measured power error;
  shipped tier stays U35 (≤8 ULP) per the three bounded-window
  sweeps, with a note that measured worst-case is now inside U10
  (≤4 ULP) across the full double range thanks to the `logk_dd`
  rewrite. **Correction in 2.0.2:** the bounded sweeps do not establish
  full-range accuracy; that statement was unsupported.
- **Experimental carve-out: `sf64_lgamma` on `[0.5, 3)`** remains
  report-only in `tests/experimental/experimental_precision.cpp`.
  The `logk_dd` fix did not close this — the blow-up is algorithmic
  (lgamma vanishes at x=1 and x=2, so the ULP ratio grows
  unboundedly against the double-floor absolute error). The proper
  fix is a zero-centered Taylor expansion, tracked in `TODO.md`
  for a future release.
- `sf64_floor` disabled-mode delta from 1.0 baseline is
  jitter-dominated at short run lengths (remeasured at
  `--min-time-ms=2000`, 3 samples, all within +10%).

### Performance

- Inlined RNE arithmetic in SLEEF's double-double helpers and accumulated flags
  locally before flushing to TLS at each public entry point.
- Refreshed the baseline for the added fenv/rounding costs and introduced the
  optional cheap-operation absolute budget (5 ns in CI, alongside 20%).
- Preserved the release's timings and configuration in
  [the version 1.1 performance note](PERFORMANCE-1.1.md).

### Integrity guarantees

- Every guarantee from 1.0 is preserved bit-for-bit. The default
  `sf64_*` surface remains RNE; `_r` variants are additive.
- Under `SOFT_FP64_FENV=disabled`, `sf64_fe_*` surface compiles to
  no-ops and every raise site compiles out — `install-smoke`
  confirms the archive still exports only `sf64_*` symbols.
- Under `SOFT_FP64_FENV=tls`, the flag accumulator is strictly
  per-thread. `test_fenv.cpp` verifies independent accumulation
  across concurrent worker threads.
- Oracle `fl2` gating is full-corpus, not sampled. Any TestFloat
  row whose observed `sf64_fe_*` bits disagree with Berkeley's
  expected-flag column fails ctest — no skip lists, no row
  filtering, no loosened bounds.
- **SLEEF transcendental carve-out removed.** 1.0 and earlier
  shipped a port that converted upstream SLEEF's `+ - * / fma sqrt
  floor ldexp` into `sf64_*` calls but left raw `<`, `>`, `==`,
  `!=`, `(double)int` in place — these relied on the frontend
  (AdaptiveCpp's SSCP emitter) lowering `fcmp.f64` / `sitofp` to
  soft-ops on no-fp64 targets. 1.1 finishes the port: every
  relational comparison goes through the `sleef::lt_` / `le_` /
  `gt_` / `ge_` / `eq_` / `ne_` helpers (wrapping `sf64_fcmp` with
  the LLVM predicate encoding) and every integer → double cast goes
  through `sf64_from_i64`. The README claim "no hidden dependency
  on the host FPU" is now mechanically true across all of `src/`,
  not just the bit-exact core. Enforced by
  `scripts/check_no_host_fp.sh` in prek + CI.

### Not included — tracked for 1.2+ in `TODO.md`

- sNaN payload preservation (depends on `SOFT_FP64_SNAN_PROPAGATE`
  build option + wired TestFloat sNaN vectors; v1.2 target).
- `soft-fp128` sibling package.
- `__acpp_sscp_lgamma_r_f64` adapter forwarding (blocks on
  `sf64_lgamma_r` core entry).
- `sf64_lgamma` zero-crossing sweep on `[0.5, 3)` — requires a
  zero-centered Taylor rewrite at `x=1` and `x=2`, not better log
  precision.

[1.1.0]: https://github.com/yocontra/soft-fp/releases/tag/v1.1.0

## [1.0.0] — 2026-04-21

First stable release. Ships a complete `sf64_*` IEEE-754 binary64 surface
built entirely on 32/64-bit integer ops (no host-FPU dependency) plus the
AdaptiveCpp Metal adapter that lets fp64-gated GPU kernels on Apple
Silicon resolve against soft-fp64 instead of trapping.

### Added

- **`sf64_*` public ABI.** `extern "C"` entry points in
  `include/soft_fp64/soft_f64.h`, hidden-by-default visibility, enforced
  by the `install-smoke` CI job (`nm -g` on the archive rejects any
  non-`sf64_*` export).
- **Arithmetic — BIT_EXACT.** `add`, `sub`, `mul`, `div`, `rem`, `neg`,
  `sqrt`, `fma`, `fmod`, `remainder`. Gated against host FPU + Berkeley
  TestFloat 3e vectors by `tests/test_arithmetic_exact.cpp` and
  `tests/test_sqrt_fma_exact.cpp`; `fmod` / `remainder` additionally
  gated 0-ULP against MPFR in `tests/mpfr/test_mpfr_diff.cpp` at tier
  `BIT_EXACT = 0` across x ∈ [-1e15, 1e15], y ∈ [1, 1e10] (quotient
  bit-count ≳ 2⁵⁰).
- **Conversion — BIT_EXACT.** Full matrix `i{8,16,32,64} ↔ f64`,
  `u{8,16,32,64} ↔ f64`, `f32 ↔ f64`, including exhaustive 2³² `f32 →
  f64 → f32` round-trip. Gated by `tests/test_convert_widths.cpp`.
- **Compare + classify — BIT_EXACT.** All 16 IEEE-754 `fcmp` predicates,
  `fmin` / `fmax` (IEEE and non-IEEE variants including `fmin_precise` /
  `fmax_precise`), `fdim`, `maxmag`, `minmag`, `nextafter`, `hypot`,
  `isnan`, `isinf`, `isfinite`, `isnormal`, `signbit`, `fabs`,
  `copysign`. Gated by `tests/test_compare_all_predicates.cpp` +
  TestFloat vectors.
- **Rounding — BIT_EXACT.** `floor`, `ceil`, `trunc`, `rint`, `round`,
  `fract`, `modf`, `ldexp`, `frexp`, `ilogb`, `logb`. Gated by
  `tests/test_rounding_edges.cpp`.
- **Transcendentals — U10 (≤4 ULP vs MPFR 200-bit).** `sin`, `cos`,
  `sincos`, `asin`, `acos`, `atan`, `exp`, `exp2`, `exp10`, `expm1`,
  `log`, `log2`, `log10`, `log1p`, `cbrt`, `cosh`, `acosh`, `atanh`.
  Gated by `tests/mpfr/test_mpfr_diff.cpp` (independent oracle)
  plus `tests/test_transcendental_libm.cpp` (libm consistency).
- **Transcendentals — U35 (≤8 ULP vs MPFR 200-bit).** `tan`, `atan2`,
  `sinh`, `tanh`, `asinh`, `pi`-variants (`sinpi`, `cospi`, `tanpi`,
  `asinpi`, `acospi`, `atanpi`, `atan2pi`), and the `pow` / `rootn`
  family (`pow`, `powr`, `pown`, `rootn`) over three overlapping
  bounded windows.
  Near-unit-base × huge-exponent corner is documented as
  out-of-region, tracked in `TODO.md`.
- **`erf` / `erfc` / `tgamma` / `lgamma` / `lgamma_r` — GAMMA
  (≤1024 ULP vs MPFR 200-bit).** Shippable ranges per the README tiers
  table. `lgamma` zero-crossings `[0.5, 3)` are parked in the
  report-only `tests/experimental/` sweep (not a shipped tier claim)
  pending the `logk_dd` rewrite tracked in `TODO.md`.
- **CI oracle stack.** Host FPU, Berkeley TestFloat 3e
  (`tests/testfloat/`), MPFR 200-bit (`tests/mpfr/`,
  `ORACLE_PREC = 200`), and libFuzzer crash-hunt targets (`fuzz/`).
- **Bench regression gate.** `bench_soft_fp64` + `bench/compare.py`
  enforce 20% threshold against `bench/baseline.json` (macos-14 M-series)
  on every PR.
- **CMake + pkg-config integration.** `find_package(soft_fp64 CONFIG)`
  and `pkg-config --cflags --libs soft_fp64`; `install-smoke` CI job
  exercises both.
- **`scripts/pre_push.sh` local guard.** Runs full `ctest` +
  `clang-format-19 --dry-run --Werror` + the bench regression gate at
  `--threshold=0.10`.
- **AdaptiveCpp Metal SSCP adapter** (`adapters/acpp_metal/`). Opt-in
  (`-DSOFT_FP64_BUILD_ACPP_METAL_ADAPTER=ON`). Stages a flat directory
  of soft-fp64 sources plus one-line forwarders for every required
  `__acpp_sscp_soft_f64_*` primitive (all 23) and the full optional
  `__acpp_sscp_*_f64` math surface (transcendentals, rounding, hypot,
  fmod / remainder, fract / frexp / modf / ldexp / ilogb, pown /
  rootn, classify predicates). Ready to be consumed via
  `-DACPP_METAL_EXTERNAL_FP64_DIR=<staged dir>` by downstream
  consumers such as pg_accel on Apple Silicon. Pinned against
  AdaptiveCpp `fork-safe-metal` at
  `c86d474a3f1fb06705679efa527ea262b5a991cf`; CI smoke job
  `acpp-metal-smoke` rebuilds `libkernel-metal-bitcode` against the
  staged dir on macos-14. Adapter emits no public ABI — forwarders
  live only in the Metal libkernel bitcode, `install-smoke`'s
  `sf64_*`-only archive grep is unaffected.

### Integrity guarantees

- `sf64_*` returns identical bits on every target for every input.
  No host-FPU dependency (SLEEF `src/sleef/*.cpp` raw-`double` helpers
  are the single carve-out, contractually lowered by the SSCP emitter
  on fp64-less targets).
- `-ffp-contract=off` is enforced globally on the library target to
  prevent `a*b+c` from fusing into `llvm.fma.f64` and closing a
  link-time cycle against `sf64_fma`.
- No silent skip paths in oracle sweeps. Every test that declares a
  corpus size in its file header runs that size.

### Not included — tracked for 1.1+ in `TODO.md`

- Non-RNE rounding modes (`sf64_*_r(mode, …)`).
- IEEE exception flags + thread-local fenv.
- sNaN payload preservation.
- `soft-fp128` sibling package.
- `logk_dd` DD-Horner rewrite (moves worst-case `pow` from ~40 ULP to
  ≤4 ULP in the near-unit-base × huge-exponent corner; closes the
  `lgamma (0.5, 3)` zero-crossing sweep).

[1.0.0]: https://github.com/yocontra/soft-fp/releases/tag/v1.0.0
