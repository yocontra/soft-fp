#pragma once

// Internal bit helpers shared across every soft-fp64 translation unit.
// Header-only, all inline, no state. Consumers:
//
//   arithmetic.cpp   add/sub/mul/div/rem/neg
//   compare.cpp      fcmp (16 predicates), fmin_precise, fmax_precise
//   convert.cpp      iN/uN/f32 <-> f64
//   sqrt_fma.cpp     sqrt, rsqrt, fma
//   rounding.cpp     floor/ceil/trunc/round/rint/fract/modf/ldexp/frexp/ilogb/logb
//   classify.cpp     isnan/isinf/fabs/copysign/hypot/nextafter/etc.
//
// Algorithms below mirror Mesa's `src/compiler/glsl/float64.glsl` extract/pack
// routines, simplified to use native 64-bit integer arithmetic (MSL supports
// `ulong`, so Mesa's GLSL-era u32-pair tricks are unnecessary).
//
// SPDX-License-Identifier: MIT

#include "soft_fp64/defines.h"
#include "soft_fp64/rounding_mode.h"

#include <cstdint>
#include <cstring>

namespace soft_fp64::internal {

// ---- IEEE-754 binary64 constants ----------------------------------------

inline constexpr uint64_t kSignMask = 0x8000000000000000ULL;
inline constexpr uint64_t kExpMask = 0x7FF0000000000000ULL;
inline constexpr uint64_t kFracMask = 0x000FFFFFFFFFFFFFULL;
inline constexpr uint64_t kImplicitBit = 0x0010000000000000ULL; // bit 52
inline constexpr uint64_t kQuietNaNBit = 0x0008000000000000ULL; // bit 51
inline constexpr uint64_t kPositiveInf = 0x7FF0000000000000ULL;
inline constexpr uint64_t kNegativeInf = 0xFFF0000000000000ULL;
inline constexpr uint64_t kCanonicalNaN = 0x7FF8000000000000ULL; // quiet, sign=0

inline constexpr int kExpBias = 1023;
inline constexpr int kExpMax = 0x7FF; // all ones
inline constexpr int kFracBits = 52;
inline constexpr int kMantissaW = kFracBits + 1; // 53 with implicit

// ---- bit-casts ----------------------------------------------------------

// The volatile handoff on floating inputs prevents optimizer pattern matching from
// turning integer masks back into fcmp/fabs/copysign/fneg instructions. It
// remains inline so the boundary is only a compiler barrier, not an O0 call.
// Output conversion needs no barrier once its integer provenance is opaque.
// These are the only permitted operations on the host floating representation.
SF64_BITCAST_BOUNDARY uint64_t bits_of(double x) noexcept {
    uint64_t out;
    std::memcpy(&out, &x, sizeof(out));
    volatile uint64_t barrier = out;
    return barrier;
}

SF64_BITCAST_BOUNDARY double from_bits(uint64_t b) noexcept {
    double out;
    std::memcpy(&out, &b, sizeof(out));
    return out;
}

SF64_BITCAST_BOUNDARY uint32_t bits_of(float x) noexcept {
    uint32_t out;
    std::memcpy(&out, &x, sizeof(out));
    volatile uint32_t barrier = out;
    return barrier;
}

SF64_BITCAST_BOUNDARY float f32_from_bits(uint32_t b) noexcept {
    float out;
    std::memcpy(&out, &b, sizeof(out));
    return out;
}

// ---- field extraction ---------------------------------------------------

// Biased exponent field (0..2047). 0 = zero/subnormal, 2047 = inf/NaN.
SF64_ALWAYS_INLINE uint32_t extract_exp(uint64_t b) noexcept {
    return static_cast<uint32_t>((b >> 52) & 0x7FF);
}

SF64_ALWAYS_INLINE uint64_t extract_frac(uint64_t b) noexcept {
    return b & kFracMask;
}

SF64_ALWAYS_INLINE uint32_t extract_sign(uint64_t b) noexcept {
    return static_cast<uint32_t>(b >> 63);
}

// Pack sign/exp/frac into an IEEE-754 binary64. Assumes:
//   sign in {0, 1}
//   exp in [0, 2047] (biased)
//   frac in [0, (1<<52)-1]  (implicit bit NOT present)
SF64_ALWAYS_INLINE uint64_t pack(uint32_t sign, uint32_t exp, uint64_t frac) noexcept {
    return (static_cast<uint64_t>(sign & 1u) << 63) | (static_cast<uint64_t>(exp & 0x7FFu) << 52) |
           (frac & kFracMask);
}

// ---- classification (duplicated here so every TU is standalone) ---------

SF64_ALWAYS_INLINE bool is_nan_bits(uint64_t b) noexcept {
    return extract_exp(b) == kExpMax && extract_frac(b) != 0;
}

// Signaling NaN per IEEE 754-2008 §6.2.1: NaN bits with the most-significant
// fraction bit (bit 51, kQuietNaNBit) clear. Every arithmetic op that takes a
// sNaN input must raise SF64_FE_INVALID (IEEE 754 §7.2). The callers raise
// before forwarding to propagate_nan, which quiets the payload.
SF64_ALWAYS_INLINE bool is_snan_bits(uint64_t b) noexcept {
    return is_nan_bits(b) && (b & kQuietNaNBit) == 0;
}

SF64_ALWAYS_INLINE bool is_inf_bits(uint64_t b) noexcept {
    return extract_exp(b) == kExpMax && extract_frac(b) == 0;
}

SF64_ALWAYS_INLINE bool is_zero_bits(uint64_t b) noexcept {
    return (b & ~kSignMask) == 0;
}

SF64_ALWAYS_INLINE bool is_subnormal_bits(uint64_t b) noexcept {
    return extract_exp(b) == 0 && extract_frac(b) != 0;
}

SF64_ALWAYS_INLINE bool is_finite_bits(uint64_t b) noexcept {
    return extract_exp(b) != kExpMax;
}

// ---- shift helpers (Mesa float64.glsl `shift64RightJamming`) ------------

// Right-shift with "jamming": OR all shifted-out bits into the LSB. Used in
// add/sub to track the sticky bit for round-to-nearest-even.
SF64_ALWAYS_INLINE uint64_t shift_right_jamming(uint64_t x, int count) noexcept {
    if (count == 0)
        return x;
    if (count >= 64)
        return (x != 0) ? 1u : 0u;
    const uint64_t mask = (uint64_t{1} << count) - 1;
    return (x >> count) | ((x & mask) != 0 ? 1u : 0u);
}

// Left-shift by `count`, saturating at 63 bits. `count` must be in [0, 63].
SF64_ALWAYS_INLINE uint64_t shift_left(uint64_t x, int count) noexcept {
    return x << count;
}

// Count leading zeros on a 64-bit value. Returns 64 for x=0.
SF64_ALWAYS_INLINE int clz64(uint64_t x) noexcept {
    if (x == 0)
        return 64;
    int count = 0;
    if ((x >> 32) == 0) {
        count += 32;
        x <<= 32;
    }
    if ((x >> 48) == 0) {
        count += 16;
        x <<= 16;
    }
    if ((x >> 56) == 0) {
        count += 8;
        x <<= 8;
    }
    if ((x >> 60) == 0) {
        count += 4;
        x <<= 4;
    }
    if ((x >> 62) == 0) {
        count += 2;
        x <<= 2;
    }
    if ((x >> 63) == 0)
        ++count;
    return count;
}

// Count leading zeros on a 32-bit value. Returns 32 for x=0.
SF64_ALWAYS_INLINE int clz32(uint32_t x) noexcept {
    if (x == 0)
        return 32;
    int count = 0;
    if ((x >> 16) == 0) {
        count += 16;
        x <<= 16;
    }
    if ((x >> 24) == 0) {
        count += 8;
        x <<= 8;
    }
    if ((x >> 28) == 0) {
        count += 4;
        x <<= 4;
    }
    if ((x >> 30) == 0) {
        count += 2;
        x <<= 2;
    }
    if ((x >> 31) == 0)
        ++count;
    return count;
}

// ---- 64x64 -> 128 unsigned multiply -------------------------------------
//
// Returned as the (hi, lo) halves of the 128-bit product. Used by `sf64_fma`
// to form the exact 53x53 -> 106-bit product before alignment.
//
// Implemented with four 32x32 -> 64 partial products and explicit carry
// propagation. No compiler-native 128-bit integer extension is required.
struct U128Pair {
    uint64_t hi;
    uint64_t lo;
};

// Portable schoolbook 64x64 -> 128. Splits each operand into 32-bit halves
// and accumulates four 64-bit partial products, propagating carries through
// the middle column. Reference: Berkeley SoftFloat 3e `s_mul64To128.c`.
SF64_ALWAYS_INLINE U128Pair mul64x64_to_128(uint64_t a, uint64_t b) noexcept {
    const uint32_t a_lo = static_cast<uint32_t>(a);
    const uint32_t a_hi = static_cast<uint32_t>(a >> 32);
    const uint32_t b_lo = static_cast<uint32_t>(b);
    const uint32_t b_hi = static_cast<uint32_t>(b >> 32);

    // Four 32x32 -> 64 partial products.
    const uint64_t ll = static_cast<uint64_t>(a_lo) * static_cast<uint64_t>(b_lo);
    const uint64_t hl = static_cast<uint64_t>(a_hi) * static_cast<uint64_t>(b_lo);
    const uint64_t lh = static_cast<uint64_t>(a_lo) * static_cast<uint64_t>(b_hi);
    const uint64_t hh = static_cast<uint64_t>(a_hi) * static_cast<uint64_t>(b_hi);

    // Combine the two middle 64-bit products. SoftFloat detects the wraparound
    // by comparing the sum to one of the operands; equivalent to extracting
    // the carry-out of the unsigned add.
    const uint64_t mid = hl + lh;
    const uint64_t mid_carry = (mid < hl) ? (uint64_t{1} << 32) : 0u;

    // High 64 bits = hh + carry_from_middle_column + middle_high_half.
    uint64_t hi = hh + mid_carry + (mid >> 32);

    // Low 64 bits = ll + middle_low_half_shifted_into_low.
    const uint64_t mid_lo = mid << 32;
    const uint64_t lo = ll + mid_lo;
    if (lo < mid_lo) {
        // Carry out of the low-half add propagates into hi.
        hi += 1u;
    }

    U128Pair r;
    r.hi = hi;
    r.lo = lo;
    return r;
}

// Minimal unsigned 128-bit integer used by the binary64 core. All operations
// are expressed in two uint64_t limbs so this type works on MSVC, wasm32 and
// device compilers that do not expose __int128.
struct U128 {
    uint64_t hi = 0;
    uint64_t lo = 0;

    constexpr U128() noexcept = default;
    constexpr U128(uint64_t value) noexcept : lo(value) {}
    constexpr U128(uint64_t high, uint64_t low) noexcept : hi(high), lo(low) {}
    constexpr explicit operator uint64_t() const noexcept { return lo; }
    constexpr explicit operator bool() const noexcept { return hi != 0 || lo != 0; }
};

SF64_ALWAYS_INLINE bool operator==(U128 a, U128 b) noexcept {
    return a.hi == b.hi && a.lo == b.lo;
}
SF64_ALWAYS_INLINE bool operator!=(U128 a, U128 b) noexcept {
    return !(a == b);
}
SF64_ALWAYS_INLINE bool operator<(U128 a, U128 b) noexcept {
    return a.hi < b.hi || (a.hi == b.hi && a.lo < b.lo);
}
SF64_ALWAYS_INLINE bool operator>=(U128 a, U128 b) noexcept {
    return !(a < b);
}
SF64_ALWAYS_INLINE U128 operator|(U128 a, U128 b) noexcept {
    return U128{a.hi | b.hi, a.lo | b.lo};
}
SF64_ALWAYS_INLINE U128 operator&(U128 a, U128 b) noexcept {
    return U128{a.hi & b.hi, a.lo & b.lo};
}
SF64_ALWAYS_INLINE U128& operator|=(U128& a, U128 b) noexcept {
    a.hi |= b.hi;
    a.lo |= b.lo;
    return a;
}
SF64_ALWAYS_INLINE U128 operator+(U128 a, U128 b) noexcept {
    const uint64_t lo = a.lo + b.lo;
    return U128{a.hi + b.hi + static_cast<uint64_t>(lo < a.lo), lo};
}
SF64_ALWAYS_INLINE U128 operator-(U128 a, U128 b) noexcept {
    const uint64_t borrow = static_cast<uint64_t>(a.lo < b.lo);
    return U128{a.hi - b.hi - borrow, a.lo - b.lo};
}
SF64_ALWAYS_INLINE U128 operator<<(U128 a, int shift) noexcept {
    if (shift <= 0)
        return a;
    if (shift >= 128)
        return U128{};
    if (shift >= 64)
        return U128{a.lo << (shift - 64), 0};
    return U128{(a.hi << shift) | (a.lo >> (64 - shift)), a.lo << shift};
}
SF64_ALWAYS_INLINE U128 operator>>(U128 a, int shift) noexcept {
    if (shift <= 0)
        return a;
    if (shift >= 128)
        return U128{};
    if (shift >= 64)
        return U128{0, a.hi >> (shift - 64)};
    return U128{a.hi >> shift, (a.lo >> shift) | (a.hi << (64 - shift))};
}
SF64_ALWAYS_INLINE U128 operator*(U128 a, U128 b) noexcept {
    const U128Pair low_product = mul64x64_to_128(a.lo, b.lo);
    return U128{low_product.hi + a.hi * b.lo + a.lo * b.hi, low_product.lo};
}
SF64_ALWAYS_INLINE int highest_bit(U128 value) noexcept {
    if (value.hi != 0)
        return 127 - clz64(value.hi);
    return value.lo != 0 ? 63 - clz64(value.lo) : -1;
}

// Divide a 128-bit numerator by a 64-bit denominator using Knuth's normalized
// base-2^32 two-digit algorithm (Hacker's Delight, `divlu`). The binary64
// caller guarantees numerator.hi < denominator, denominator >= 2^32, and a
// 64-bit quotient. This avoids both compiler-native 128-bit integers and the
// 128 iterations of restoring bit division.
SF64_ALWAYS_INLINE uint64_t divmod_u128_u64(U128 numerator, uint64_t denominator,
                                            uint64_t& remainder) noexcept {
    constexpr uint64_t base = uint64_t{1} << 32;
    constexpr uint64_t mask = base - 1;
    const unsigned shift = static_cast<unsigned>(clz64(denominator));
    const uint64_t normalized_denominator = denominator << shift;
    const uint64_t denominator_hi = normalized_denominator >> 32;
    const uint64_t denominator_lo = normalized_denominator & mask;
    const uint64_t numerator_hi = (numerator.hi << shift) | (numerator.lo >> (64 - shift));
    const uint64_t numerator_lo = numerator.lo << shift;
    const uint64_t numerator_digit1 = numerator_lo >> 32;
    const uint64_t numerator_digit0 = numerator_lo & mask;

    uint64_t quotient_hi = numerator_hi / denominator_hi;
    uint64_t remainder_hat = numerator_hi - quotient_hi * denominator_hi;
    while (quotient_hi >= base ||
           quotient_hi * denominator_lo > base * remainder_hat + numerator_digit1) {
        --quotient_hi;
        remainder_hat += denominator_hi;
        if (remainder_hat >= base)
            break;
    }

    const uint64_t middle =
        numerator_hi * base + numerator_digit1 - quotient_hi * normalized_denominator;
    uint64_t quotient_lo = middle / denominator_hi;
    remainder_hat = middle - quotient_lo * denominator_hi;
    while (quotient_lo >= base ||
           quotient_lo * denominator_lo > base * remainder_hat + numerator_digit0) {
        --quotient_lo;
        remainder_hat += denominator_hi;
        if (remainder_hat >= base)
            break;
    }

    remainder = (middle * base + numerator_digit0 - quotient_lo * normalized_denominator) >> shift;
    return quotient_hi * base + quotient_lo;
}

// ---- canonical result builders ------------------------------------------

SF64_ALWAYS_INLINE double make_signed_zero(uint32_t sign) noexcept {
    return from_bits(static_cast<uint64_t>(sign & 1u) << 63);
}

SF64_ALWAYS_INLINE double make_signed_inf(uint32_t sign) noexcept {
    return from_bits(pack(sign, kExpMax, 0));
}

SF64_ALWAYS_INLINE double propagate_nan(uint64_t a_bits, uint64_t b_bits) noexcept {
    // Per IEEE 754-2008 §6.2.3: prefer the NaN of the signalling operand,
    // quiet the result. For a pair of quiet NaNs, pick `a`.
    const bool a_is_nan = is_nan_bits(a_bits);
    const bool b_is_nan = is_nan_bits(b_bits);
    if (a_is_nan && b_is_nan) {
        // Both NaN; prefer a, quieted.
        return from_bits(a_bits | kQuietNaNBit);
    }
    if (a_is_nan)
        return from_bits(a_bits | kQuietNaNBit);
    if (b_is_nan)
        return from_bits(b_bits | kQuietNaNBit);
    return from_bits(kCanonicalNaN);
}

SF64_ALWAYS_INLINE double canonical_nan() noexcept {
    return from_bits(kCanonicalNaN);
}

// ---- mode-parametrized rounding primitive -------------------------------
//
// Central decision point for the "should I round the truncated significand
// up by one ULP?" question. Every round-and-pack site (arithmetic add/sub/
// mul/div, sqrt, fma, and the convert.cpp u64 → f64 path) funnels through
// this primitive so non-RNE rounding modes reach every round-affected op
// uniformly.
//
// Inputs:
//   sign       : sign bit of the pre-rounding result (0 = +, 1 = -). Only
//                consumed by the directed modes (RUP / RDN).
//   round_bit  : the bit immediately below the target LSB (the "guard" bit).
//                Non-zero iff the discarded payload is at least half an ULP.
//   sticky     : non-zero iff any bit strictly below `round_bit` is set.
//   lsb        : the bit at the target LSB position — determines the RNE
//                tiebreak (round to the nearest even).
//   mode       : @ref sf64_rounding_mode
//
// Returns true iff the truncated significand should be incremented.
//
// The five modes decompose as:
//   RNE : round_bit && (sticky || lsb)        -- tiebreak to even LSB
//   RTZ : false                                -- always truncate
//   RUP : sign == 0 && (round_bit || sticky)   -- any non-zero residue bumps
//   RDN : sign != 0 && (round_bit || sticky)   --   (positives / negatives)
//   RNA : round_bit                            -- ties away from zero
SF64_ALWAYS_INLINE bool sf64_internal_should_round_up(uint32_t sign, bool round_bit, bool sticky,
                                                      bool lsb, sf64_rounding_mode mode) noexcept {
    switch (mode) {
    case SF64_RTZ:
        return false;
    case SF64_RUP:
        return (sign == 0u) && (round_bit || sticky);
    case SF64_RDN:
        return (sign != 0u) && (round_bit || sticky);
    case SF64_RNA:
        return round_bit;
    case SF64_RNE:
    default:
        return round_bit && (sticky || lsb);
    }
}

} // namespace soft_fp64::internal
