// Bit-exact sNaN sign/payload propagation tests.
//
// The four propagation paths in soft-fp64 — propagate_nan (used by add/mul/
// div), the unary sqrt path, the three-input fma path, and the f32 widen/
// narrow path — already preserve sign + payload bits 50:0 of the source NaN
// in both modes (they all emit `from_bits(src | kQuietNaNBit)`, which only
// forces bit 51).
//
// The two paths that *do* differ between modes are:
//   sub(a, b)  via the b-operand sign XOR — when `b` is sNaN, today's quiet
//              mode produces the source-NaN payload but with the **sign
//              inverted** (because the XOR runs before the NaN dispatch).
//              Under SOFT_FP64_SNAN_PROPAGATE=1 the sign survives.
//   fmod / remainder — both currently `return qNaN()` (canonical) on any
//              NaN input. Under SOFT_FP64_SNAN_PROPAGATE=1 they propagate
//              the source NaN's sign + payload with bit 51 forced.
//
// Each assertion is gated on `SOFT_FP64_SNAN_PROPAGATE` so the same TU
// builds and passes under both build options. The configure-time value
// selects which branch is compiled in; CI's `build-test-snan-propagate`
// cell exercises the `=1` branch end-to-end alongside the default `=0`
// branch in the main matrix.
//
// SPDX-License-Identifier: MIT

#include "host_oracle.h"
#include "soft_fp64/soft_f64.h"

#ifndef SOFT_FP64_SNAN_PROPAGATE
#error "SOFT_FP64_SNAN_PROPAGATE must be defined by CMake (0 = quiet, 1 = propagate)"
#endif
#ifndef SF64_TEST_FENV_MODE
#define SF64_TEST_FENV_MODE 1
#endif

namespace {

using host_oracle::bits;
using host_oracle::bits_f32;
using host_oracle::f32_from_bits;
using host_oracle::from_bits;

// Canonical sNaN bit patterns. Exp = 0x7FF, bit 51 (quiet) clear, payload
// non-zero so the value is a NaN (not an infinity). Both signs covered.
constexpr uint64_t kSNaNPosBits = 0x7FF00000DEADBEEFULL;
constexpr uint64_t kSNaNNegBits = 0xFFF00000CAFEBABEULL;
constexpr uint64_t kQuietBit = 0x0008000000000000ULL;
constexpr uint64_t kSignMask = 0x8000000000000000ULL;
constexpr uint64_t kFracMask = 0x000FFFFFFFFFFFFFULL;

// The canonical qNaN soft-fp64 emits when an operation has no source NaN to
// propagate (0×∞, ∞−∞, sqrt(-finite), fmod with sNaN under quiet mode, etc.).
constexpr uint64_t kCanonicalQNaN = 0x7FF8000000000000ULL;

// "Quiet" a NaN bit pattern: keep sign + payload 50:0, force bit 51.
constexpr uint64_t quieted(uint64_t snan_bits) {
    return snan_bits | kQuietBit;
}

void check_eq_bits(uint64_t got_bits, uint64_t expect_bits, const char* label) {
    if (got_bits != expect_bits) {
        std::fprintf(stderr, "FAIL [%s]: got=0x%016llx expect=0x%016llx (mode=%d)\n", label,
                     (unsigned long long)got_bits, (unsigned long long)expect_bits,
                     SOFT_FP64_SNAN_PROPAGATE);
        std::abort();
    }
}

void check_op(double got, uint64_t expect_bits, const char* label) {
    check_eq_bits(bits(got), expect_bits, label);
}

} // namespace

int main() {
    const double snan_pos = from_bits(kSNaNPosBits);
    const double snan_neg = from_bits(kSNaNNegBits);
    const double qnan_canonical = from_bits(kCanonicalQNaN);
    const double normal = 3.5;

    // ====================================================================
    // Mode-invariant: paths that already preserve source NaN sign/payload
    // ====================================================================
    //
    // These assertions hold in BOTH `quiet` and `propagate` mode. They pin
    // the propagate_nan / sqrt / fma / f32-convert paths. Those are already
    // §6.2.3-compliant and need no policy-specific patching.

    // ---- add: propagate_nan picks first NaN, OR's quiet bit ----
    check_op(sf64_add(snan_pos, normal), quieted(kSNaNPosBits), "add(sNaN+, x)");
    check_op(sf64_add(normal, snan_pos), quieted(kSNaNPosBits), "add(x, sNaN+)");
    check_op(sf64_add(snan_neg, normal), quieted(kSNaNNegBits), "add(sNaN-, x)");
    check_op(sf64_add(normal, snan_neg), quieted(kSNaNNegBits), "add(x, sNaN-)");
    // Both NaN: prefer a.
    check_op(sf64_add(snan_pos, snan_neg), quieted(kSNaNPosBits), "add(sNaN+, sNaN-)");
    check_op(sf64_add(snan_neg, snan_pos), quieted(kSNaNNegBits), "add(sNaN-, sNaN+)");

    // ---- mul: same propagate_nan path ----
    check_op(sf64_mul(snan_pos, normal), quieted(kSNaNPosBits), "mul(sNaN+, x)");
    check_op(sf64_mul(normal, snan_neg), quieted(kSNaNNegBits), "mul(x, sNaN-)");

    // ---- div: same ----
    check_op(sf64_div(snan_pos, normal), quieted(kSNaNPosBits), "div(sNaN+, x)");
    check_op(sf64_div(normal, snan_neg), quieted(kSNaNNegBits), "div(x, sNaN-)");

    // ---- sqrt: unary, no XOR — sign + payload always preserved ----
    check_op(sf64_sqrt(snan_pos), quieted(kSNaNPosBits), "sqrt(sNaN+)");
    check_op(sf64_sqrt(snan_neg), quieted(kSNaNNegBits), "sqrt(sNaN-)");

    // ---- fma: returns first NaN, OR'd with quiet bit ----
    check_op(sf64_fma(snan_pos, normal, normal), quieted(kSNaNPosBits), "fma(sNaN+, x, y)");
    check_op(sf64_fma(normal, snan_neg, normal), quieted(kSNaNNegBits), "fma(x, sNaN-, y)");
    check_op(sf64_fma(normal, normal, snan_pos), quieted(kSNaNPosBits), "fma(x, y, sNaN+)");

    // ---- sub(sNaN, x): sNaN is the a-operand, no XOR involved on a ----
    // The b-operand sign is XOR'd with kSignMask but `a` wins propagate_nan.
    check_op(sf64_sub(snan_pos, normal), quieted(kSNaNPosBits), "sub(sNaN+, x)");
    check_op(sf64_sub(snan_neg, normal), quieted(kSNaNNegBits), "sub(sNaN-, x)");

    // ---- sub(qNaN, sNaN): a wins → b's flipped sign is discarded ----
    check_op(sf64_sub(qnan_canonical, snan_pos), kCanonicalQNaN, "sub(qNaN, sNaN+)");
    check_op(sf64_sub(qnan_canonical, snan_neg), kCanonicalQNaN, "sub(qNaN, sNaN-)");

    // ---- f32 widen: sign + 23-bit f32 payload preserved, lifted to bits 51:29 ----
    {
        // f32 sNaN: exp=0xFF, frac high bit clear, payload non-zero.
        const uint32_t f32_snan_pos = 0x7F800ABCu;
        const uint32_t f32_snan_neg = 0xFF80F00Du;
        const uint64_t expect_pos = (uint64_t{0} << 63) | (uint64_t{0x7FFu} << 52) |
                                    (uint64_t{f32_snan_pos & 0x7FFFFFu} << 29) | kQuietBit;
        const uint64_t expect_neg = (uint64_t{1} << 63) | (uint64_t{0x7FFu} << 52) |
                                    (uint64_t{f32_snan_neg & 0x7FFFFFu} << 29) | kQuietBit;
        check_op(sf64_from_f32(f32_from_bits(f32_snan_pos)), expect_pos, "from_f32(sNaN+)");
        check_op(sf64_from_f32(f32_from_bits(f32_snan_neg)), expect_neg, "from_f32(sNaN-)");
    }

    // ---- f32 narrow: sign + top-23 payload preserved, bit 22 (f32 quiet) forced ----
    {
        const float got_pos = sf64_to_f32(snan_pos);
        const float got_neg = sf64_to_f32(snan_neg);
        const uint32_t expect_pos = (uint32_t{0} << 31) | (0xFFu << 23) |
                                    static_cast<uint32_t>((kSNaNPosBits >> 29) & 0x7FFFFFu) |
                                    0x400000u;
        const uint32_t expect_neg = (uint32_t{1} << 31) | (0xFFu << 23) |
                                    static_cast<uint32_t>((kSNaNNegBits >> 29) & 0x7FFFFFu) |
                                    0x400000u;
        if (bits_f32(got_pos) != expect_pos) {
            std::fprintf(stderr, "FAIL [to_f32(sNaN+)]: got=0x%08x expect=0x%08x\n",
                         bits_f32(got_pos), expect_pos);
            std::abort();
        }
        if (bits_f32(got_neg) != expect_neg) {
            std::fprintf(stderr, "FAIL [to_f32(sNaN-)]: got=0x%08x expect=0x%08x\n",
                         bits_f32(got_neg), expect_neg);
            std::abort();
        }
    }

    // ====================================================================
    // Mode-dependent: sub(x, sNaN) and fmod / remainder
    // ====================================================================

#if SOFT_FP64_SNAN_PROPAGATE == 0
    // Quiet mode (today's behavior).
    //
    // sub: bb is sign-flipped before the NaN dispatch sees it. When `b` is
    // the source NaN, the result inherits the FLIPPED sign. This is a
    // §6.2.3 violation — but it's the documented behavior under the
    // default option and is asserted here so a regression in either
    // direction breaks the build.
    check_op(sf64_sub(normal, snan_pos), quieted(kSNaNPosBits ^ kSignMask),
             "quiet: sub(x, sNaN+) [sign inverted]");
    check_op(sf64_sub(normal, snan_neg), quieted(kSNaNNegBits ^ kSignMask),
             "quiet: sub(x, sNaN-) [sign inverted]");

    // fmod / remainder: any NaN input → canonical qNaN, source NaN bits lost.
    check_op(sf64_fmod(snan_pos, normal), kCanonicalQNaN, "quiet: fmod(sNaN+, x)");
    check_op(sf64_fmod(normal, snan_pos), kCanonicalQNaN, "quiet: fmod(x, sNaN+)");
    check_op(sf64_fmod(snan_neg, normal), kCanonicalQNaN, "quiet: fmod(sNaN-, x)");
    check_op(sf64_remainder(snan_pos, normal), kCanonicalQNaN, "quiet: remainder(sNaN+, x)");
    check_op(sf64_remainder(normal, snan_neg), kCanonicalQNaN, "quiet: remainder(x, sNaN-)");

#elif SOFT_FP64_SNAN_PROPAGATE == 1
    // Propagate mode (§6.2.3 strict).
    //
    // sub: source NaN's sign survives.
    check_op(sf64_sub(normal, snan_pos), quieted(kSNaNPosBits),
             "propagate: sub(x, sNaN+) [sign preserved]");
    check_op(sf64_sub(normal, snan_neg), quieted(kSNaNNegBits),
             "propagate: sub(x, sNaN-) [sign preserved]");

    // fmod / remainder: pick a-operand if NaN, else b-operand.
    check_op(sf64_fmod(snan_pos, normal), quieted(kSNaNPosBits), "propagate: fmod(sNaN+, x)");
    check_op(sf64_fmod(normal, snan_pos), quieted(kSNaNPosBits), "propagate: fmod(x, sNaN+)");
    check_op(sf64_fmod(snan_neg, normal), quieted(kSNaNNegBits), "propagate: fmod(sNaN-, x)");
    check_op(sf64_fmod(snan_pos, snan_neg), quieted(kSNaNPosBits), "propagate: fmod(sNaN+, sNaN-)");
    check_op(sf64_remainder(snan_pos, normal), quieted(kSNaNPosBits),
             "propagate: remainder(sNaN+, x)");
    check_op(sf64_remainder(normal, snan_neg), quieted(kSNaNNegBits),
             "propagate: remainder(x, sNaN-)");

#else
#error "SOFT_FP64_SNAN_PROPAGATE must be 0 or 1"
#endif

    // The INVALID flag should be raised under either sNaN policy for all
    // sNaN-input ops when the legacy TLS fenv surface is active. Explicit and
    // disabled fenv builds intentionally make sf64_fe_getall() observe zero,
    // so this TLS-only spot-check is skipped there.
#if SF64_TEST_FENV_MODE == 1
    sf64_fe_clear(SF64_FE_INVALID | SF64_FE_DIVBYZERO | SF64_FE_OVERFLOW | SF64_FE_UNDERFLOW |
                  SF64_FE_INEXACT);
    (void)sf64_add(snan_pos, normal);
    SF64_CHECK((sf64_fe_getall() & SF64_FE_INVALID) != 0u);

    sf64_fe_clear(SF64_FE_INVALID | SF64_FE_DIVBYZERO | SF64_FE_OVERFLOW | SF64_FE_UNDERFLOW |
                  SF64_FE_INEXACT);
    (void)sf64_fmod(snan_pos, normal);
    SF64_CHECK((sf64_fe_getall() & SF64_FE_INVALID) != 0u);
#endif

    std::printf("test_snan_payload: PASSED (mode=%d)\n", SOFT_FP64_SNAN_PROPAGATE);
    return 0;
}
