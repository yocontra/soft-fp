#pragma once

/* IEEE-754 binary256 core C ABI: 1 sign, 19 exponent, 236 fraction bits.
 * Limbs are ordered least-significant first, independent of host byte order.
 * Arithmetic, conversion, remainder, sqrt, FMA and rint are implemented in
 * integer code for all five rounding modes. Arithmetic defaults to RNE;
 * integer narrowing defaults to truncation toward zero. Operations use the
 * configured sf64 fenv; `_ex` entries write caller-owned sticky flags.
 * Integer narrowing is deterministic: NaN is zero and out-of-range values
 * saturate while raising INVALID.
 *
 * This is deliberately a core arithmetic API. No binary256 transcendental
 * accuracy contract is published in 2.0, so no approximate or host-libm
 * placeholder symbols are exposed. */

#include "soft_fp64/soft_f64.h"

#include <stdint.h>

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC visibility push(default)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf256_t {
    uint64_t limb[4];
} sf256_t;
#ifdef __cplusplus
static_assert(sizeof(sf256_t) == 32, "sf256_t ABI must be 256 bits");
#else
_Static_assert(sizeof(sf256_t) == 32, "sf256_t ABI must be 256 bits");
#endif

sf256_t sf256_from_bits(uint64_t w3, uint64_t w2, uint64_t w1, uint64_t w0);
uint64_t sf256_bits(sf256_t x, unsigned word);
sf256_t sf256_from_i64(int64_t x);
sf256_t sf256_from_u64(uint64_t x);
sf256_t sf256_from_f64(double x);
double sf256_to_f64(sf256_t x);
double sf256_to_f64_r(sf64_rounding_mode mode, sf256_t x);
double sf256_to_f64_r_ex(sf64_rounding_mode mode, sf256_t x, sf64_fe_state_t* state);
int32_t sf256_to_i32(sf256_t x);
int64_t sf256_to_i64(sf256_t x);
uint32_t sf256_to_u32(sf256_t x);
uint64_t sf256_to_u64(sf256_t x);
int64_t sf256_to_i64_r(sf64_rounding_mode mode, sf256_t x);
uint64_t sf256_to_u64_r(sf64_rounding_mode mode, sf256_t x);
int64_t sf256_to_i64_r_ex(sf64_rounding_mode mode, sf256_t x, sf64_fe_state_t* state);
uint64_t sf256_to_u64_r_ex(sf64_rounding_mode mode, sf256_t x, sf64_fe_state_t* state);

sf256_t sf256_add(sf256_t a, sf256_t b);
sf256_t sf256_sub(sf256_t a, sf256_t b);
sf256_t sf256_mul(sf256_t a, sf256_t b);
sf256_t sf256_div(sf256_t a, sf256_t b);
sf256_t sf256_sqrt(sf256_t x);
sf256_t sf256_fma(sf256_t a, sf256_t b, sf256_t c);
sf256_t sf256_remainder(sf256_t a, sf256_t b);
sf256_t sf256_remainder_ex(sf256_t a, sf256_t b, sf64_fe_state_t* state);
sf256_t sf256_rint(sf256_t x);
sf256_t sf256_rint_r(sf64_rounding_mode mode, sf256_t x);
sf256_t sf256_rint_r_ex(sf64_rounding_mode mode, sf256_t x, sf64_fe_state_t* state);
sf256_t sf256_add_r(sf64_rounding_mode mode, sf256_t a, sf256_t b);
sf256_t sf256_sub_r(sf64_rounding_mode mode, sf256_t a, sf256_t b);
sf256_t sf256_mul_r(sf64_rounding_mode mode, sf256_t a, sf256_t b);
sf256_t sf256_div_r(sf64_rounding_mode mode, sf256_t a, sf256_t b);
sf256_t sf256_sqrt_r(sf64_rounding_mode mode, sf256_t x);
sf256_t sf256_fma_r(sf64_rounding_mode mode, sf256_t a, sf256_t b, sf256_t c);
sf256_t sf256_add_r_ex(sf64_rounding_mode mode, sf256_t a, sf256_t b, sf64_fe_state_t* state);
sf256_t sf256_sub_r_ex(sf64_rounding_mode mode, sf256_t a, sf256_t b, sf64_fe_state_t* state);
sf256_t sf256_mul_r_ex(sf64_rounding_mode mode, sf256_t a, sf256_t b, sf64_fe_state_t* state);
sf256_t sf256_div_r_ex(sf64_rounding_mode mode, sf256_t a, sf256_t b, sf64_fe_state_t* state);
sf256_t sf256_sqrt_r_ex(sf64_rounding_mode mode, sf256_t x, sf64_fe_state_t* state);
sf256_t sf256_fma_r_ex(sf64_rounding_mode mode, sf256_t a, sf256_t b, sf256_t c,
                       sf64_fe_state_t* state);

int sf256_isnan(sf256_t x);
int sf256_isinf(sf256_t x);
int sf256_isfinite(sf256_t x);
int sf256_isnormal(sf256_t x);
int sf256_signbit(sf256_t x);
int sf256_eq(sf256_t a, sf256_t b);
int sf256_lt(sf256_t a, sf256_t b);
int sf256_le(sf256_t a, sf256_t b);

#ifdef __cplusplus
} /* extern "C" */
#endif

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC visibility pop
#endif
