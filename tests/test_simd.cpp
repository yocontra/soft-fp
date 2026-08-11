#include "soft_fp/simd.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace simd = soft_fp::simd;

int main() {
    static_assert(std::is_standard_layout_v<simd::pack<double, 8>>);
    static_assert(std::is_trivially_copyable_v<simd::pack<double, 8>>);
    static_assert(sizeof(simd::pack<double, 8>) == 8 * sizeof(double));

    const double left_data[8] = {1.0, 2.0, 3.0, 4.0, -1.0, -2.0, -3.0, -4.0};
    const double right_data[8] = {8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0};
    const auto left = simd::load<double, 8>(left_data);
    const auto right = simd::load<double, 8>(right_data);
    const auto sum =
        simd::transform(left, right, [](double a, double b) { return sf64_add(a, b); });
    const auto fused = simd::transform(
        left, right, sum, [](double a, double b, double c) { return sf64_fma(a, b, c); });

    double stored[8]{};
    simd::store(stored, fused);
    for (std::size_t lane = 0; lane < 8; ++lane)
        assert(stored[lane] == sf64_fma(left_data[lane], right_data[lane],
                                        sf64_add(left_data[lane], right_data[lane])));

    simd::pack<sf64_fe_state_t, 8> states{};
    const auto quotient =
        simd::transform_indexed(left, right, [&states](std::size_t lane, double a, double b) {
            return sf64_div_r_ex(SF64_RNE, a, b, &states[lane]);
        });
    for (std::size_t lane = 0; lane < 8; ++lane) {
        assert(quotient[lane] == sf64_div(left_data[lane], right_data[lane]));
        constexpr unsigned all_flags = SF64_FE_INVALID | SF64_FE_DIVBYZERO | SF64_FE_OVERFLOW |
                                       SF64_FE_UNDERFLOW | SF64_FE_INEXACT;
        assert((states[lane].flags & ~all_flags) == 0);
    }

#if SOFT_FP_HAS_FP128
    simd::pack<sf128_t, 4> wide128{};
    for (std::size_t lane = 0; lane < 4; ++lane)
        wide128[lane] = sf128_from_u64(static_cast<uint64_t>(lane + 1));
    const auto words128 = simd::split(wide128);
    const auto roundtrip128 = simd::combine(words128);
    const auto doubled128 = simd::transform(roundtrip128, roundtrip128, sf128_add);
    for (std::size_t lane = 0; lane < 4; ++lane)
        assert(sf128_to_u64(doubled128[lane]) == 2 * (lane + 1));
#endif

#if SOFT_FP_HAS_FP256
    simd::pack<sf256_t, 4> wide256{};
    for (std::size_t lane = 0; lane < 4; ++lane)
        wide256[lane] = sf256_from_u64(static_cast<uint64_t>(lane + 1));
    const auto words256 = simd::split(wide256);
    const auto roundtrip256 = simd::combine(words256);
    const auto squared256 = simd::transform(roundtrip256, roundtrip256, sf256_mul);
    for (std::size_t lane = 0; lane < 4; ++lane)
        assert(sf256_to_u64(squared256[lane]) == (lane + 1) * (lane + 1));
#endif

    return 0;
}
