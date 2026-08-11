#pragma once

/* Portable lane-wise integration helpers for SIMD and SIMT consumers.
 *
 * The containers are ordinary aggregate types: they do not select an ISA,
 * allocate storage, or change the scalar ABI. `transform` accepts lambdas,
 * function objects, and scalar soft-fp entry points. Use transform_indexed
 * when each lane needs its own sf64_fe_state_t.
 */

#include "soft_fp64/config.h"
#include "soft_fp64/soft_f64.h"
#include "soft_fp64/vec.h"

#if SOFT_FP_HAS_FP128
#include "soft_fp128/soft_f128.h"
#endif

#if SOFT_FP_HAS_FP256
#include "soft_fp256/soft_f256.h"
#endif

#include <cstddef>
#include <cstdint>

namespace soft_fp::simd {

template <typename T, std::size_t N> using pack = soft_fp64::vec<T, N>;

using soft_fp64::load;
using soft_fp64::store;
using soft_fp64::transform;
using soft_fp64::transform_indexed;

#if SOFT_FP_HAS_FP128
template <std::size_t N> struct fp128_words {
    pack<uint64_t, N> lo{};
    pack<uint64_t, N> hi{};
};

template <std::size_t N> constexpr fp128_words<N> split(const pack<sf128_t, N>& values) noexcept {
    fp128_words<N> words{};
    for (std::size_t lane = 0; lane < N; ++lane) {
        words.lo[lane] = values[lane].lo;
        words.hi[lane] = values[lane].hi;
    }
    return words;
}

template <std::size_t N> constexpr pack<sf128_t, N> combine(const fp128_words<N>& words) noexcept {
    pack<sf128_t, N> values{};
    for (std::size_t lane = 0; lane < N; ++lane)
        values[lane] = sf128_t{words.lo[lane], words.hi[lane]};
    return values;
}
#endif

#if SOFT_FP_HAS_FP256
template <std::size_t N> struct fp256_words {
    pack<uint64_t, N> limb0{};
    pack<uint64_t, N> limb1{};
    pack<uint64_t, N> limb2{};
    pack<uint64_t, N> limb3{};
};

template <std::size_t N> constexpr fp256_words<N> split(const pack<sf256_t, N>& values) noexcept {
    fp256_words<N> words{};
    for (std::size_t lane = 0; lane < N; ++lane) {
        words.limb0[lane] = values[lane].limb[0];
        words.limb1[lane] = values[lane].limb[1];
        words.limb2[lane] = values[lane].limb[2];
        words.limb3[lane] = values[lane].limb[3];
    }
    return words;
}

template <std::size_t N> constexpr pack<sf256_t, N> combine(const fp256_words<N>& words) noexcept {
    pack<sf256_t, N> values{};
    for (std::size_t lane = 0; lane < N; ++lane) {
        values[lane].limb[0] = words.limb0[lane];
        values[lane].limb[1] = words.limb1[lane];
        values[lane].limb[2] = words.limb2[lane];
        values[lane].limb[3] = words.limb3[lane];
    }
    return values;
}
#endif

} // namespace soft_fp::simd
