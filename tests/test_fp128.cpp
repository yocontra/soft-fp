#include "soft_fp128/soft_f128.h"

#include <cassert>
#include <climits>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {
sf128_t bits(uint64_t hi, uint64_t lo = 0) {
    return sf128_from_bits(hi, lo);
}
void expect(sf128_t x, uint64_t hi, uint64_t lo = 0) {
    assert(sf128_bits_hi(x) == hi);
    assert(sf128_bits_lo(x) == lo);
}
} // namespace

int main() {
    const sf128_t one = bits(0x3FFF000000000000ULL);
    const sf128_t two = bits(0x4000000000000000ULL);
    const sf128_t three = bits(0x4000800000000000ULL);
    const sf128_t four = bits(0x4001000000000000ULL);

    expect(sf128_add(one, two), three.hi, three.lo);
    expect(sf128_sub(three, one), two.hi, two.lo);
    expect(sf128_mul(two, two), four.hi, four.lo);
    expect(sf128_div(four, two), two.hi, two.lo);
    expect(sf128_sqrt(four), two.hi, two.lo);
    expect(sf128_fma(one, two, one), three.hi, three.lo);
    expect(sf128_remainder(three, two), 0xBFFF000000000000ULL);
    assert(sf128_to_i32(sf128_from_i32(-42)) == -42);
    assert(sf128_to_i32(sf128_from_f64(2.75)) == 2);
    assert(sf128_to_u64(sf128_from_u64(UINT64_MAX)) == UINT64_MAX);
    expect(sf128_rint(sf128_from_f64(2.5)), two.hi);
    expect(sf128_rint_r(SF64_RNA, sf128_from_f64(2.5)), three.hi);
    assert(sf128_to_i64(bits(0x7FFF800000000000ULL)) == 0);
    assert(sf128_to_i64(bits(0x7FFF000000000000ULL)) == INT64_MAX);
    assert(sf128_to_i64(bits(0xFFFF000000000000ULL)) == INT64_MIN);
    sf64_fe_state_t convert_state{0};
    assert(sf128_to_i64_r_ex(SF64_RTZ, bits(0x7FFF000000000001ULL), &convert_state) == 0);
    assert(convert_state.flags == SF64_FE_INVALID);

    const sf128_t half_ulp = bits(0x3F8E000000000000ULL);
    expect(sf128_add_r(SF64_RNE, one, half_ulp), one.hi, one.lo);
    expect(sf128_add_r(SF64_RUP, one, half_ulp), one.hi, one.lo + 1);

    const double values[] = {0.0,
                             -0.0,
                             1.0,
                             -1.0,
                             std::numeric_limits<double>::min(),
                             std::numeric_limits<double>::denorm_min(),
                             std::numeric_limits<double>::max()};
    for (double value : values) {
        const double roundtrip = sf128_to_f64(sf128_from_f64(value));
        assert(std::signbit(roundtrip) == std::signbit(value));
        assert(roundtrip == value);
    }

    sf64_fe_state_t state{0};
    const sf128_t zero = bits(0);
    const sf128_t inf = sf128_div_r_ex(SF64_RNE, one, zero, &state);
    assert(sf128_isinf(inf));
    assert(state.flags == SF64_FE_DIVBYZERO);
    assert(sf128_isfinite(one));
    assert(sf128_isnormal(one));
    assert(sf128_eq(one, one));
    assert(sf128_lt(one, two));
    assert(sf128_le(one, one));
}
