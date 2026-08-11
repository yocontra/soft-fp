#pragma once

// Attribute macros shared across the soft-fp64 surface.
//
// SPDX-License-Identifier: MIT

#if defined(__clang__) || defined(__GNUC__)
#define SF64_ALWAYS_INLINE __attribute__((always_inline)) inline
#define SF64_NOINLINE __attribute__((noinline))
#define SF64_EXPORT __attribute__((visibility("default")))
#define SF64_HIDDEN __attribute__((visibility("hidden")))
#define SF64_TLS_INITIAL_EXEC __attribute__((tls_model("initial-exec")))
#elif defined(_MSC_VER)
#define SF64_ALWAYS_INLINE __forceinline
#define SF64_NOINLINE __declspec(noinline)
#if defined(SOFT_FP_BUILD_SHARED)
#define SF64_EXPORT __declspec(dllexport)
#elif defined(SOFT_FP_USE_SHARED)
#define SF64_EXPORT __declspec(dllimport)
#else
#define SF64_EXPORT
#endif
#define SF64_HIDDEN
#define SF64_TLS_INITIAL_EXEC
#else
#define SF64_ALWAYS_INLINE inline
#define SF64_NOINLINE
#define SF64_EXPORT
#define SF64_HIDDEN
#define SF64_TLS_INITIAL_EXEC
#endif

// SF64_SLEEF_INLINE: separate inline marker for SLEEF's transcendental
// scaffolding (DD polynomial arithmetic helpers `ddmul_*`, `ddadd_*`,
// `ddsqu_*` and the polynomial / argument-reduction helpers `sinpik_dd`,
// `expk_dd`, `logk_dd`, `atan2k_u1_dd`, …). Defaults to `SF64_ALWAYS_INLINE`
// — the inlining is correctness-critical when fenv tracking is on (the
// `sf64_internal_fe_acc&` parameter has to flow back to the caller's
// stack). On the AdaptiveCpp Metal libkernel path, fenv is disabled
// (`SOFT_FP64_FENV_MODE=0`) so the alwaysinline rationale doesn't apply,
// and the build defines `SF64_DISABLE_SLEEF_INLINE` to collapse this to
// plain `inline`. Without that override, clang -O3 inlines DD helpers
// transitively into `sf64_sin` / `sf64_cos` / `sf64_atan`, producing
// ~80K-line LLVM functions that the Metal source emitter dumps verbatim
// into MSL — `xcrun metal` then sits in its optimizer for 10+ minutes
// per such kernel. With outlined helpers, `sf64_sin` shrinks to ~500
// lines and Metal compiles in seconds.
//
// Bit-exact arithmetic helpers in `internal_arith.h` (`arith_add_magnitudes_rne`,
// `arith_sub_magnitudes_rne`, `arith_pack_normal`, etc.) keep
// `SF64_ALWAYS_INLINE` because their `sf64_internal_fe_acc&` argument has
// to inline-propagate the caller's `thread`-AS alloca to a `device`-AS
// pointer in MSL — without alwaysinline, the Metal source emitter
// produces calls to functions whose declared parameter AS doesn't match
// the caller's pointer AS, and Metal compilation rejects them with
// "cannot pass pointer to default address space as a pointer to address
// space 'device'". SLEEF helpers don't need this because they're called
// from already-outlined `sf64_*` functions whose alloca-rooted accumulator
// gets AS-inferred at the standalone function level.
#if defined(SF64_DISABLE_SLEEF_INLINE)
// On the AdaptiveCpp Metal libkernel path the SLEEF surface is consumed
// after a per-kernel JIT O3 pass that aggressively inlines `linkonce_odr`
// functions whenever the call site cost heuristic says yes — even
// without `__attribute__((always_inline))`. Plain `inline` produces
// `inlinehint` linkonce_odr in the bitcode, which the JIT inliner
// then folds back into `sf64_sin` / `sf64_cos` / `sf64_atan` —
// producing 80K+ line MSL functions that hang `xcrun metal` for 10+
// minutes. Mark `SF64_SLEEF_INLINE` as `noinline` instead: the helpers
// stay outlined as standalone functions and Metal compiles in seconds.
// Functional correctness: noinline + ODR linkonce semantics still
// guarantee one definition wins per program, fenv accumulator pointer
// flows through the function call boundary correctly when fenv mode
// is `0` (no thread_local TLS to thread through). On non-Metal builds
// the `SF64_DISABLE_SLEEF_INLINE` flag is unset and these collapse to
// `SF64_ALWAYS_INLINE` — preserving the historical behavior for
// non-AdaptiveCpp consumers (host CPU validation, MPFR oracle, etc.).
// `inline` for ODR-link semantics on header-defined helpers + `noinline`
// to defeat the JIT inliner heuristic.
#define SF64_SLEEF_INLINE __attribute__((noinline)) inline
#define SF64_SLEEF_NOINLINE SF64_NOINLINE
#else
#define SF64_SLEEF_INLINE SF64_ALWAYS_INLINE
#define SF64_SLEEF_NOINLINE
#endif

// Disable function-body optimizations. Used on the bit-twiddle ABI entries
// (sf64_fabs / sf64_copysign / sf64_neg / sf64_fcmp) where clang -O3's
// InstCombine pattern-matches the integer ops back into llvm.fabs.f64 /
// llvm.copysign.f64 / fneg double / fcmp <pred> double. AdaptiveCpp's MSL
// emitter then routes those intrinsics back through __acpp_sscp_*_f64
// wrappers whose bodies forward to sf64_*, producing infinite mutual
// recursion that hangs the AGX command buffer (kIOGPUCommandBufferCallback
// ErrorHang). Apply only to the four cycle-risk bodies — bodies are 1–10
// lines of bit ops, so the optimizer cost is negligible. This is deliberately
// Clang-only: GCC builds do not feed the Metal pipeline, and GCC rejects this
// attribute placement under -Wattributes.
#if defined(__clang__)
#define SF64_NO_OPT __attribute__((optnone))
#define SF64_BITCAST_BOUNDARY SF64_ALWAYS_INLINE
#elif defined(__GNUC__)
#define SF64_NO_OPT
#define SF64_BITCAST_BOUNDARY SF64_ALWAYS_INLINE
#elif defined(_MSC_VER)
#define SF64_NO_OPT
#define SF64_BITCAST_BOUNDARY __declspec(noinline) inline
#else
#define SF64_NO_OPT
#define SF64_BITCAST_BOUNDARY inline
#endif

// Entry points consumed by AdaptiveCpp's MSL emitter are `extern "C"` with a
// predefined symbol prefix. Consumers that want to hook into AdaptiveCpp set
// this to the emitter's expected attribute (e.g. HIPSYCL_SSCP_BUILTIN) in
// their own build; stand-alone builds fall back to `extern "C"` + always-inline.
#ifndef SF64_ABI
#ifdef __cplusplus
#define SF64_ABI extern "C" SF64_ALWAYS_INLINE
#else
#define SF64_ABI static inline
#endif
#endif
