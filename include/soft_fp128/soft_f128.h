#pragma once

/* IEEE-754 binary128 core C ABI. Values are stored as explicit high/low
 * words; field meaning is independent of host byte order. Arithmetic,
 * conversion, remainder, sqrt, FMA and rint are bit-exact for all five
 * rounding modes. Arithmetic defaults to RNE; integer narrowing defaults to
 * truncation toward zero. Operations use the configured sf64 fenv; `_ex`
 * entries write only caller-owned sticky flags. NaNs are quieted and
 * signaling NaNs raise INVALID. Integer narrowing is deterministic: NaN is
 * zero and out-of-range values saturate while raising INVALID.
 *
 * This is deliberately a core arithmetic API. No binary128 transcendental
 * accuracy contract is published in 2.0, so no approximate or host-libm
 * placeholder symbols are exposed. */

#include "soft_fp64/rounding_mode.h"
#include "soft_fp64/soft_f64.h"

#include <stdint.h>

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC visibility push(default)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf128_t {
    uint64_t lo;
    uint64_t hi;
} sf128_t;
#ifdef __cplusplus
static_assert(sizeof(sf128_t) == 16, "sf128_t ABI must be 128 bits");
#else
_Static_assert(sizeof(sf128_t) == 16, "sf128_t ABI must be 128 bits");
#endif

sf128_t sf128_from_bits(uint64_t hi, uint64_t lo);
uint64_t sf128_bits_hi(sf128_t x);
uint64_t sf128_bits_lo(sf128_t x);

sf128_t sf128_from_i32(int32_t x);
sf128_t sf128_from_i64(int64_t x);
sf128_t sf128_from_u32(uint32_t x);
sf128_t sf128_from_u64(uint64_t x);
sf128_t sf128_from_f64(double x);
double sf128_to_f64(sf128_t x);
double sf128_to_f64_r(sf64_rounding_mode mode, sf128_t x);
double sf128_to_f64_r_ex(sf64_rounding_mode mode, sf128_t x, sf64_fe_state_t* state);
int32_t sf128_to_i32(sf128_t x);
int64_t sf128_to_i64(sf128_t x);
uint32_t sf128_to_u32(sf128_t x);
uint64_t sf128_to_u64(sf128_t x);
int32_t sf128_to_i32_r(sf64_rounding_mode mode, sf128_t x);
int64_t sf128_to_i64_r(sf64_rounding_mode mode, sf128_t x);
uint32_t sf128_to_u32_r(sf64_rounding_mode mode, sf128_t x);
uint64_t sf128_to_u64_r(sf64_rounding_mode mode, sf128_t x);
int32_t sf128_to_i32_r_ex(sf64_rounding_mode mode, sf128_t x, sf64_fe_state_t* state);
int64_t sf128_to_i64_r_ex(sf64_rounding_mode mode, sf128_t x, sf64_fe_state_t* state);
uint32_t sf128_to_u32_r_ex(sf64_rounding_mode mode, sf128_t x, sf64_fe_state_t* state);
uint64_t sf128_to_u64_r_ex(sf64_rounding_mode mode, sf128_t x, sf64_fe_state_t* state);

sf128_t sf128_add(sf128_t a, sf128_t b);
sf128_t sf128_sub(sf128_t a, sf128_t b);
sf128_t sf128_mul(sf128_t a, sf128_t b);
sf128_t sf128_div(sf128_t a, sf128_t b);
sf128_t sf128_remainder(sf128_t a, sf128_t b);
sf128_t sf128_remainder_ex(sf128_t a, sf128_t b, sf64_fe_state_t* state);
sf128_t sf128_sqrt(sf128_t x);
sf128_t sf128_fma(sf128_t a, sf128_t b, sf128_t c);
sf128_t sf128_rint(sf128_t x);

sf128_t sf128_add_r(sf64_rounding_mode mode, sf128_t a, sf128_t b);
sf128_t sf128_sub_r(sf64_rounding_mode mode, sf128_t a, sf128_t b);
sf128_t sf128_mul_r(sf64_rounding_mode mode, sf128_t a, sf128_t b);
sf128_t sf128_div_r(sf64_rounding_mode mode, sf128_t a, sf128_t b);
sf128_t sf128_sqrt_r(sf64_rounding_mode mode, sf128_t x);
sf128_t sf128_fma_r(sf64_rounding_mode mode, sf128_t a, sf128_t b, sf128_t c);
sf128_t sf128_rint_r(sf64_rounding_mode mode, sf128_t x);
sf128_t sf128_rint_r_ex(sf64_rounding_mode mode, sf128_t x, sf64_fe_state_t* state);

sf128_t sf128_add_r_ex(sf64_rounding_mode mode, sf128_t a, sf128_t b, sf64_fe_state_t* state);
sf128_t sf128_sub_r_ex(sf64_rounding_mode mode, sf128_t a, sf128_t b, sf64_fe_state_t* state);
sf128_t sf128_mul_r_ex(sf64_rounding_mode mode, sf128_t a, sf128_t b, sf64_fe_state_t* state);
sf128_t sf128_div_r_ex(sf64_rounding_mode mode, sf128_t a, sf128_t b, sf64_fe_state_t* state);
sf128_t sf128_sqrt_r_ex(sf64_rounding_mode mode, sf128_t x, sf64_fe_state_t* state);
sf128_t sf128_fma_r_ex(sf64_rounding_mode mode, sf128_t a, sf128_t b, sf128_t c,
                       sf64_fe_state_t* state);

int sf128_isnan(sf128_t x);
int sf128_isinf(sf128_t x);
int sf128_isfinite(sf128_t x);
int sf128_isnormal(sf128_t x);
int sf128_signbit(sf128_t x);
int sf128_eq(sf128_t a, sf128_t b);
int sf128_lt(sf128_t a, sf128_t b);
int sf128_le(sf128_t a, sf128_t b);

#ifdef __cplusplus
} /* extern "C" */
#endif

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC visibility pop
#endif
