// libFuzzer target for sf64_remainder(x, y).
//
// sf64_remainder implements IEEE-754 `remainder` (quotient rounded to
// nearest-even). The mathematical result is exactly representable: the
// reduction `x - n*y` is computed with a single rounding step and n is
// chosen s.t. |r| <= |y|/2. MPFR supplies an independent exact oracle;
// platform libm is deliberately not used because some implementations lose
// the quotient parity for extreme exponent gaps and subnormal divisors.
//
// Consumes 16 bytes (two f64 bit-patterns). We gate on finite inputs
// (both operands must be finite, y != 0) because:
//   * `remainder(x, 0) = NaN` — we check that separately.
//   * `remainder(+/-inf, y) = NaN` — ditto.
//   * `remainder(x, +/-inf) = x` for finite x — ditto.

#include <soft_fp64/soft_f64.h>

#include <mpfr.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

double bits_to_double(uint64_t bits) {
    double d;
    std::memcpy(&d, &bits, sizeof(d));
    return d;
}

uint64_t double_to_bits(double d) {
    uint64_t b;
    std::memcpy(&b, &d, sizeof(b));
    return b;
}

bool is_nan(double d) {
    uint64_t b = double_to_bits(d);
    return ((b >> 52) & 0x7ffu) == 0x7ffu && (b & 0x000f'ffff'ffff'ffffULL) != 0;
}

struct MpfrRemainderOracle {
    mpfr_t x;
    mpfr_t y;
    mpfr_t result;

    MpfrRemainderOracle() {
        mpfr_init2(x, 53);
        mpfr_init2(y, 53);
        mpfr_init2(result, 53);
    }

    ~MpfrRemainderOracle() {
        mpfr_clear(x);
        mpfr_clear(y);
        mpfr_clear(result);
    }

    double operator()(double a, double b) {
        mpfr_set_d(x, a, MPFR_RNDN);
        mpfr_set_d(y, b, MPFR_RNDN);
        mpfr_remainder(result, x, y, MPFR_RNDN);
        return mpfr_get_d(result, MPFR_RNDN);
    }
};

[[noreturn]] void fuzz_fail(const char* reason) {
    std::fprintf(stderr, "fuzz_remainder: %s\n", reason);
    __builtin_trap();
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 16)
        return 0;

    uint64_t xb, yb;
    std::memcpy(&xb, data, 8);
    std::memcpy(&yb, data + 8, 8);
    const double x = bits_to_double(xb);
    const double y = bits_to_double(yb);

    const double got = sf64_remainder(x, y);

    // NaN propagation & special-case table.
    if (is_nan(x) || is_nan(y)) {
        if (!is_nan(got))
            fuzz_fail("remainder(NaN,*) or (*,NaN) not NaN");
        return 0;
    }
    if (__builtin_isinf(x)) {
        if (!is_nan(got))
            fuzz_fail("remainder(+/-inf, y) not NaN");
        return 0;
    }
    if (y == 0.0) {
        if (!is_nan(got))
            fuzz_fail("remainder(x, 0) not NaN");
        return 0;
    }
    if (__builtin_isinf(y)) {
        // remainder(x, +/-inf) = x for finite x.
        if (double_to_bits(got) != double_to_bits(x))
            fuzz_fail("remainder(x, inf) != x");
        return 0;
    }

    // Finite non-zero y, finite x: MPFR computes the exact IEEE remainder.
    static MpfrRemainderOracle oracle_state;
    const double oracle = oracle_state(x, y);

    // Bit-exact match required. NaN-equivalence wrapper not needed because
    // neither side can be NaN here (all NaN cases returned above).
    if (double_to_bits(got) != double_to_bits(oracle)) {
        fuzz_fail("sf64_remainder disagrees bit-exact with MPFR");
    }

    return 0;
}
