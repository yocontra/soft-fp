//
// OpenCL C compatibility wrappers.
//
// The strict `sf64_*` ABI remains IEEE-754/subnormal-preserving. This
// optional source file emits an additive `sf64_ocl_*` surface plus OpenCL
// `native_*` names. When SOFT_FP64_FTZ=on, binary64 subnormal inputs and
// outputs are flushed to signed zero at wrapper entry/exit; the underlying
// core implementation stays unchanged.
//
// SPDX-License-Identifier: MIT
//

#include "internal.h"
#include "soft_fp64/soft_f64.h"

#ifndef SOFT_FP64_FTZ_MODE
#define SOFT_FP64_FTZ_MODE 0
#endif

namespace {

SF64_ALWAYS_INLINE double ocl_in(double x) noexcept {
#if SOFT_FP64_FTZ_MODE
    const uint64_t b = soft_fp64::internal::bits_of(x);
    if (soft_fp64::internal::is_subnormal_bits(b)) {
        return soft_fp64::internal::from_bits(b & soft_fp64::internal::kSignMask);
    }
#endif
    return x;
}

SF64_ALWAYS_INLINE double ocl_out(double x) noexcept {
    return ocl_in(x);
}

SF64_ALWAYS_INLINE uint64_t abs_bits(double x) noexcept {
    return soft_fp64::internal::bits_of(x) & ~soft_fp64::internal::kSignMask;
}

SF64_ALWAYS_INLINE bool isnan_bits(double x) noexcept {
    return soft_fp64::internal::is_nan_bits(soft_fp64::internal::bits_of(x));
}

SF64_ALWAYS_INLINE bool isinf_bits(double x) noexcept {
    return soft_fp64::internal::is_inf_bits(soft_fp64::internal::bits_of(x));
}

SF64_ALWAYS_INLINE bool iszero_bits(double x) noexcept {
    return soft_fp64::internal::is_zero_bits(soft_fp64::internal::bits_of(x));
}

SF64_ALWAYS_INLINE double qnan() noexcept {
    return soft_fp64::internal::from_bits(soft_fp64::internal::kCanonicalNaN);
}

SF64_ALWAYS_INLINE bool olt(double a, double b) noexcept {
    return sf64_fcmp(a, b, 4) != 0;
}

SF64_ALWAYS_INLINE bool ogt(double a, double b) noexcept {
    return sf64_fcmp(a, b, 2) != 0;
}

SF64_ALWAYS_INLINE bool ole(double a, double b) noexcept {
    return sf64_fcmp(a, b, 5) != 0;
}

SF64_ALWAYS_INLINE double native_sin_kernel(double r) noexcept {
    const double r2 = sf64_mul(r, r);
    double p = 2.7557319223985890653e-6;
    p = sf64_fma(p, r2, -0.00019841269841269841270);
    p = sf64_fma(p, r2, 0.0083333333333333332177);
    p = sf64_fma(p, r2, -0.16666666666666665741);
    return sf64_fma(sf64_mul(sf64_mul(r, r2), p), 1.0, r);
}

SF64_ALWAYS_INLINE double native_cos_kernel(double r) noexcept {
    const double r2 = sf64_mul(r, r);
    double p = 2.4801587301587301566e-5;
    p = sf64_fma(p, r2, -0.0013888888888888889419);
    p = sf64_fma(p, r2, 0.041666666666666664354);
    p = sf64_fma(p, r2, -0.5);
    return sf64_fma(r2, p, 1.0);
}

SF64_ALWAYS_INLINE double native_sin_reduced(double x) noexcept {
    constexpr double kHalfPi = 1.57079632679489661923;
    constexpr double kInvHalfPi = 0.63661977236758134308;
    constexpr uint64_t kLargeTrigCutoff = 0x4130000000000000ULL; // 2^20

    if (isnan_bits(x) || isinf_bits(x)) {
        return qnan();
    }
    if (abs_bits(x) > kLargeTrigCutoff) {
        return sf64_ocl_sin(x);
    }

    const double qf = sf64_rint(sf64_mul(x, kInvHalfPi));
    const int q = sf64_to_i32(qf);
    const double r = sf64_fma(sf64_neg(qf), kHalfPi, x);
    int quadrant = q % 4;
    if (quadrant < 0) {
        quadrant += 4;
    }

    if (quadrant == 0) {
        return native_sin_kernel(r);
    }
    if (quadrant == 1) {
        return native_cos_kernel(r);
    }
    if (quadrant == 2) {
        return sf64_neg(native_sin_kernel(r));
    }
    return sf64_neg(native_cos_kernel(r));
}

SF64_ALWAYS_INLINE double native_cos_reduced(double x) noexcept {
    constexpr double kHalfPi = 1.57079632679489661923;
    constexpr double kInvHalfPi = 0.63661977236758134308;
    constexpr uint64_t kLargeTrigCutoff = 0x4130000000000000ULL; // 2^20

    if (isnan_bits(x) || isinf_bits(x)) {
        return qnan();
    }
    if (abs_bits(x) > kLargeTrigCutoff) {
        return sf64_ocl_cos(x);
    }

    const double qf = sf64_rint(sf64_mul(x, kInvHalfPi));
    const int q = sf64_to_i32(qf);
    const double r = sf64_fma(sf64_neg(qf), kHalfPi, x);
    int quadrant = q % 4;
    if (quadrant < 0) {
        quadrant += 4;
    }

    if (quadrant == 0) {
        return native_cos_kernel(r);
    }
    if (quadrant == 1) {
        return sf64_neg(native_sin_kernel(r));
    }
    if (quadrant == 2) {
        return sf64_neg(native_cos_kernel(r));
    }
    return native_sin_kernel(r);
}

SF64_ALWAYS_INLINE double native_exp_core(double x) noexcept {
    constexpr double kLn2 = 0.69314718055994530942;
    constexpr double kInvLn2 = 1.4426950408889634074;

    if (isnan_bits(x)) {
        return qnan();
    }
    if (isinf_bits(x)) {
        return soft_fp64::internal::bits_of(x) & soft_fp64::internal::kSignMask
                   ? 0.0
                   : sf64_ocl_div(1.0, 0.0);
    }
    if (ogt(x, 709.0)) {
        return sf64_ocl_div(1.0, 0.0);
    }
    if (olt(x, -745.0)) {
        return 0.0;
    }

    const double qf = sf64_rint(sf64_mul(x, kInvLn2));
    const int q = sf64_to_i32(qf);
    const double r = sf64_fma(sf64_neg(qf), kLn2, x);

    double p = 1.3888888888888889419e-3;
    p = sf64_fma(p, r, 8.3333333333333332177e-3);
    p = sf64_fma(p, r, 4.1666666666666664354e-2);
    p = sf64_fma(p, r, 0.16666666666666665741);
    p = sf64_fma(p, r, 0.5);
    p = sf64_fma(p, r, 1.0);
    p = sf64_fma(p, r, 1.0);
    return sf64_ldexp(p, q);
}

SF64_ALWAYS_INLINE double native_log_core(double x) noexcept {
    constexpr double kLn2 = 0.69314718055994530942;
    constexpr double kSqrtHalf = 0.70710678118654752440;

    if (isnan_bits(x) || olt(x, 0.0)) {
        return qnan();
    }
    if (iszero_bits(x)) {
        return sf64_neg(sf64_ocl_div(1.0, 0.0));
    }
    if (isinf_bits(x)) {
        return sf64_ocl_div(1.0, 0.0);
    }

    int e = 0;
    double m = sf64_frexp(x, &e);
    if (olt(m, kSqrtHalf)) {
        m = sf64_add(m, m);
        --e;
    }

    const double z = sf64_div(sf64_sub(m, 1.0), sf64_add(m, 1.0));
    const double z2 = sf64_mul(z, z);
    double p = 0.090909090909090911614;
    p = sf64_fma(p, z2, 0.11111111111111110494);
    p = sf64_fma(p, z2, 0.14285714285714284921);
    p = sf64_fma(p, z2, 0.19999999999999998335);
    p = sf64_fma(p, z2, 0.33333333333333331483);
    p = sf64_fma(p, z2, 1.0);
    const double log_m = sf64_mul(sf64_add(z, z), p);
    return sf64_fma(sf64_from_i32(e), kLn2, log_m);
}

} // namespace

#define SF64_OCL_UNARY(name)                                                                       \
    extern "C" double sf64_ocl_##name(double x) {                                                  \
        return ocl_out(sf64_##name(ocl_in(x)));                                                    \
    }

#define SF64_OCL_BINARY(name)                                                                      \
    extern "C" double sf64_ocl_##name(double a, double b) {                                        \
        return ocl_out(sf64_##name(ocl_in(a), ocl_in(b)));                                         \
    }

#define SF64_OCL_TO_INT(type, name)                                                                \
    extern "C" type sf64_ocl_##name(double x) {                                                    \
        return sf64_##name(ocl_in(x));                                                             \
    }

#define SF64_OCL_FROM_INT(type, name)                                                              \
    extern "C" double sf64_ocl_##name(type x) {                                                    \
        return ocl_out(sf64_##name(x));                                                            \
    }

SF64_OCL_BINARY(add)
SF64_OCL_BINARY(sub)
SF64_OCL_BINARY(mul)
SF64_OCL_BINARY(div)
SF64_OCL_BINARY(rem)

SF64_OCL_UNARY(neg)

extern "C" int sf64_ocl_fcmp(double a, double b, int pred) {
    return sf64_fcmp(ocl_in(a), ocl_in(b), pred);
}

SF64_OCL_BINARY(fmin_precise)
SF64_OCL_BINARY(fmax_precise)

extern "C" double sf64_ocl_from_f32(float x) {
    return ocl_out(sf64_from_f32(x));
}
extern "C" float sf64_ocl_to_f32(double x) {
    return sf64_to_f32(ocl_in(x));
}

SF64_OCL_FROM_INT(int8_t, from_i8)
SF64_OCL_FROM_INT(int16_t, from_i16)
SF64_OCL_FROM_INT(int32_t, from_i32)
SF64_OCL_FROM_INT(int64_t, from_i64)
SF64_OCL_FROM_INT(uint8_t, from_u8)
SF64_OCL_FROM_INT(uint16_t, from_u16)
SF64_OCL_FROM_INT(uint32_t, from_u32)
SF64_OCL_FROM_INT(uint64_t, from_u64)

SF64_OCL_TO_INT(int8_t, to_i8)
SF64_OCL_TO_INT(int16_t, to_i16)
SF64_OCL_TO_INT(int32_t, to_i32)
SF64_OCL_TO_INT(int64_t, to_i64)
SF64_OCL_TO_INT(uint8_t, to_u8)
SF64_OCL_TO_INT(uint16_t, to_u16)
SF64_OCL_TO_INT(uint32_t, to_u32)
SF64_OCL_TO_INT(uint64_t, to_u64)

SF64_OCL_UNARY(sqrt)
SF64_OCL_UNARY(rsqrt)

extern "C" double sf64_ocl_fma(double a, double b, double c) {
    return ocl_out(sf64_fma(ocl_in(a), ocl_in(b), ocl_in(c)));
}

SF64_OCL_UNARY(floor)
SF64_OCL_UNARY(ceil)
SF64_OCL_UNARY(trunc)
SF64_OCL_UNARY(round)
SF64_OCL_UNARY(rint)
SF64_OCL_UNARY(fract)

extern "C" double sf64_ocl_modf(double x, double* iptr) {
    double i = 0.0;
    const double r = sf64_modf(ocl_in(x), iptr != nullptr ? &i : iptr);
    if (iptr != nullptr) {
        *iptr = ocl_out(i);
    }
    return ocl_out(r);
}

extern "C" double sf64_ocl_ldexp(double x, int n) {
    return ocl_out(sf64_ldexp(ocl_in(x), n));
}

extern "C" double sf64_ocl_frexp(double x, int* exp) {
    return ocl_out(sf64_frexp(ocl_in(x), exp));
}

extern "C" int sf64_ocl_ilogb(double x) {
    return sf64_ilogb(ocl_in(x));
}

SF64_OCL_UNARY(logb)

extern "C" int sf64_ocl_isnan(double x) {
    return sf64_isnan(ocl_in(x));
}
extern "C" int sf64_ocl_isinf(double x) {
    return sf64_isinf(ocl_in(x));
}
extern "C" int sf64_ocl_isfinite(double x) {
    return sf64_isfinite(ocl_in(x));
}
extern "C" int sf64_ocl_isnormal(double x) {
    return sf64_isnormal(ocl_in(x));
}
extern "C" int sf64_ocl_signbit(double x) {
    return sf64_signbit(ocl_in(x));
}

SF64_OCL_UNARY(fabs)
SF64_OCL_BINARY(copysign)
SF64_OCL_BINARY(fmin)
SF64_OCL_BINARY(fmax)
SF64_OCL_BINARY(fdim)
SF64_OCL_BINARY(maxmag)
SF64_OCL_BINARY(minmag)
SF64_OCL_BINARY(nextafter)
SF64_OCL_BINARY(hypot)

SF64_OCL_UNARY(sin)
SF64_OCL_UNARY(cos)
SF64_OCL_UNARY(tan)

extern "C" void sf64_ocl_sincos(double x, double* s_out, double* c_out) {
    double s = 0.0;
    double c = 0.0;
    sf64_sincos(ocl_in(x), s_out != nullptr ? &s : nullptr, c_out != nullptr ? &c : nullptr);
    if (s_out != nullptr) {
        *s_out = ocl_out(s);
    }
    if (c_out != nullptr) {
        *c_out = ocl_out(c);
    }
}

SF64_OCL_UNARY(asin)
SF64_OCL_UNARY(acos)
SF64_OCL_UNARY(atan)
SF64_OCL_BINARY(atan2)
SF64_OCL_UNARY(sinpi)
SF64_OCL_UNARY(cospi)
SF64_OCL_UNARY(tanpi)
SF64_OCL_UNARY(asinpi)
SF64_OCL_UNARY(acospi)
SF64_OCL_UNARY(atanpi)
SF64_OCL_BINARY(atan2pi)

SF64_OCL_UNARY(sinh)
SF64_OCL_UNARY(cosh)
SF64_OCL_UNARY(tanh)
SF64_OCL_UNARY(asinh)
SF64_OCL_UNARY(acosh)
SF64_OCL_UNARY(atanh)

SF64_OCL_UNARY(exp)
SF64_OCL_UNARY(exp2)
SF64_OCL_UNARY(exp10)
SF64_OCL_UNARY(expm1)
SF64_OCL_UNARY(log)
SF64_OCL_UNARY(log2)
SF64_OCL_UNARY(log10)
SF64_OCL_UNARY(log1p)

SF64_OCL_BINARY(pow)
SF64_OCL_BINARY(powr)

extern "C" double sf64_ocl_pown(double x, int n) {
    return ocl_out(sf64_pown(ocl_in(x), n));
}

extern "C" double sf64_ocl_rootn(double x, int n) {
    return ocl_out(sf64_rootn(ocl_in(x), n));
}

SF64_OCL_UNARY(cbrt)
SF64_OCL_UNARY(erf)
SF64_OCL_UNARY(erfc)
SF64_OCL_UNARY(tgamma)
SF64_OCL_UNARY(lgamma)

extern "C" double sf64_ocl_lgamma_r(double x, int* sign) {
    return ocl_out(sf64_lgamma_r(ocl_in(x), sign));
}

SF64_OCL_BINARY(fmod)
SF64_OCL_BINARY(remainder)

extern "C" float sf64_ocl_to_f32_r(sf64_rounding_mode mode, double x) {
    return sf64_to_f32_r(mode, ocl_in(x));
}

extern "C" int8_t sf64_ocl_to_i8_r(sf64_rounding_mode mode, double x) {
    return sf64_to_i8_r(mode, ocl_in(x));
}
extern "C" int16_t sf64_ocl_to_i16_r(sf64_rounding_mode mode, double x) {
    return sf64_to_i16_r(mode, ocl_in(x));
}
extern "C" int32_t sf64_ocl_to_i32_r(sf64_rounding_mode mode, double x) {
    return sf64_to_i32_r(mode, ocl_in(x));
}
extern "C" int64_t sf64_ocl_to_i64_r(sf64_rounding_mode mode, double x) {
    return sf64_to_i64_r(mode, ocl_in(x));
}
extern "C" uint8_t sf64_ocl_to_u8_r(sf64_rounding_mode mode, double x) {
    return sf64_to_u8_r(mode, ocl_in(x));
}
extern "C" uint16_t sf64_ocl_to_u16_r(sf64_rounding_mode mode, double x) {
    return sf64_to_u16_r(mode, ocl_in(x));
}
extern "C" uint32_t sf64_ocl_to_u32_r(sf64_rounding_mode mode, double x) {
    return sf64_to_u32_r(mode, ocl_in(x));
}
extern "C" uint64_t sf64_ocl_to_u64_r(sf64_rounding_mode mode, double x) {
    return sf64_to_u64_r(mode, ocl_in(x));
}

extern "C" double sf64_native_sin(double x) {
    return ocl_out(native_sin_reduced(ocl_in(x)));
}
extern "C" double sf64_native_cos(double x) {
    return ocl_out(native_cos_reduced(ocl_in(x)));
}
extern "C" double sf64_native_tan(double x) {
    const double in = ocl_in(x);
    return ocl_out(sf64_div(native_sin_reduced(in), native_cos_reduced(in)));
}
extern "C" double sf64_native_exp(double x) {
    return ocl_out(native_exp_core(ocl_in(x)));
}
extern "C" double sf64_native_exp2(double x) {
    return ocl_out(native_exp_core(sf64_mul(ocl_in(x), 0.69314718055994530942)));
}
extern "C" double sf64_native_exp10(double x) {
    return ocl_out(native_exp_core(sf64_mul(ocl_in(x), 2.30258509299404568402)));
}
extern "C" double sf64_native_log(double x) {
    return ocl_out(native_log_core(ocl_in(x)));
}
extern "C" double sf64_native_log2(double x) {
    return ocl_out(sf64_mul(native_log_core(ocl_in(x)), 1.4426950408889634074));
}
extern "C" double sf64_native_log10(double x) {
    return ocl_out(sf64_mul(native_log_core(ocl_in(x)), 0.43429448190325182765));
}
extern "C" double sf64_native_sqrt(double x) {
    return sf64_ocl_sqrt(x);
}
extern "C" double sf64_native_rsqrt(double x) {
    return sf64_ocl_rsqrt(x);
}
extern "C" double sf64_native_recip(double x) {
    return sf64_ocl_div(1.0, x);
}
extern "C" double sf64_native_divide(double x, double y) {
    return sf64_ocl_div(x, y);
}
extern "C" double sf64_native_powr(double x, double y) {
    const double xb = ocl_in(x);
    const double yb = ocl_in(y);
    if (isnan_bits(xb) || isnan_bits(yb) || ole(xb, 0.0) || isinf_bits(xb) || isinf_bits(yb)) {
        return sf64_ocl_powr(x, y);
    }
    return ocl_out(native_exp_core(sf64_mul(yb, native_log_core(xb))));
}

#undef SF64_OCL_UNARY
#undef SF64_OCL_BINARY
#undef SF64_OCL_TO_INT
#undef SF64_OCL_FROM_INT
