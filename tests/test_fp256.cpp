#include "soft_fp256/soft_f256.h"

#include <cassert>
#include <cmath>
#include <limits>

namespace {
sf256_t bits(uint64_t hi, uint64_t w2 = 0, uint64_t w1 = 0, uint64_t lo = 0) {
    return sf256_from_bits(hi, w2, w1, lo);
}
void expect(sf256_t x, sf256_t y) {
    for (unsigned i = 0; i < 4; ++i)
        assert(sf256_bits(x, i) == sf256_bits(y, i));
}
} // namespace

int main() {
    const sf256_t one = bits(0x3FFFF00000000000ULL);
    const sf256_t two = bits(0x4000000000000000ULL);
    const sf256_t three = bits(0x4000080000000000ULL);
    const sf256_t four = bits(0x4000100000000000ULL);
    expect(sf256_add(one, two), three);
    expect(sf256_sub(three, one), two);
    expect(sf256_mul(two, two), four);
    expect(sf256_div(four, two), two);
    expect(sf256_sqrt(four), two);
    expect(sf256_fma(one, two, one), three);
    expect(sf256_remainder(three, two), bits(0xBFFFF00000000000ULL));
    expect(sf256_remainder(bits(0x4000140000000000ULL), two), one); /* 5 rem 2 */
    expect(sf256_rint(sf256_from_f64(2.5)), two);
    expect(sf256_rint_r(SF64_RNA, sf256_from_f64(2.5)), three);
    assert(sf256_to_i64(sf256_from_f64(-2.5)) == -2);
    assert(sf256_to_i64(sf256_from_f64(2.75)) == 2);
    assert(sf256_to_i64_r(SF64_RNA, sf256_from_f64(-2.5)) == -3);
    assert(sf256_to_u64(sf256_from_f64(42.0)) == 42);

    const sf256_t half_ulp = bits(0x3FF1200000000000ULL);
    expect(sf256_add_r(SF64_RNE, one, half_ulp), one);
    sf256_t next = one;
    next.limb[0] = 1;
    expect(sf256_add_r(SF64_RUP, one, half_ulp), next);

    const sf256_t min_sub = bits(0, 0, 0, 1);
    const sf256_t max_sub = bits(0x00000FFFFFFFFFFFULL, ~uint64_t{0}, ~uint64_t{0}, ~uint64_t{0});
    const sf256_t min_normal = bits(0x0000100000000000ULL);
    expect(sf256_add(max_sub, min_sub), min_normal);
    expect(sf256_sub(min_normal, min_sub), max_sub);
    expect(sf256_div_r(SF64_RNE, min_sub, two), bits(0));
    expect(sf256_div_r(SF64_RUP, min_sub, two), min_sub);
    sf64_fe_state_t state{0};
    const sf256_t previous_one =
        bits(0x3FFFEFFFFFFFFFFFULL, ~uint64_t{0}, ~uint64_t{0}, ~uint64_t{0});
    expect(sf256_mul_r_ex(SF64_RNE, min_normal, previous_one, &state), min_normal);
    assert(state.flags == SF64_FE_INEXACT); /* tiny before, normal after rounding */

    state.flags = 0;
    const sf256_t double_min_normal = sf256_from_f64(std::numeric_limits<double>::min());
    const sf256_t half_double_min_sub = bits(uint64_t(262143 - 1075) << 44);
    const sf256_t double_normal_midpoint = sf256_sub(double_min_normal, half_double_min_sub);
    assert(sf256_to_f64_r_ex(SF64_RNE, double_normal_midpoint, &state) ==
           std::numeric_limits<double>::min());
    assert(state.flags == SF64_FE_INEXACT); /* same rule when narrowing to f64 */

    const sf256_t max_finite =
        bits(0x7FFFEFFFFFFFFFFFULL, ~uint64_t{0}, ~uint64_t{0}, ~uint64_t{0});
    expect(sf256_remainder(max_finite, one), bits(0));
    expect(sf256_add_r(SF64_RTZ, max_finite, max_finite), max_finite);
    assert(sf256_isinf(sf256_add(max_finite, max_finite)));

    const double values[] = {0.0,
                             -0.0,
                             1.0,
                             -1.0,
                             std::numeric_limits<double>::min(),
                             std::numeric_limits<double>::denorm_min(),
                             std::numeric_limits<double>::max()};
    for (double value : values) {
        double roundtrip = sf256_to_f64(sf256_from_f64(value));
        assert(roundtrip == value);
        assert(std::signbit(roundtrip) == std::signbit(value));
    }

    state.flags = 0;
    sf256_t inf = sf256_div_r_ex(SF64_RNE, one, bits(0), &state);
    assert(sf256_isinf(inf));
    assert(state.flags == SF64_FE_DIVBYZERO);
    state.flags = 0;
    (void)sf256_to_f64_r_ex(SF64_RNE, bits(0x7FFFF00000000000ULL, 0, 0, 1), &state);
    assert(state.flags == SF64_FE_INVALID);
    assert(sf256_isfinite(one) && sf256_isnormal(one));
    assert(sf256_eq(one, one) && sf256_lt(one, two) && sf256_le(one, one));
}
