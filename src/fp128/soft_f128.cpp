#include "soft_fp128/soft_f128.h"

extern "C" {
#include "platform.h"
#include "softfloat.h"
uint_fast8_t sf128_backend_get_rounding(void);
void sf128_backend_set_rounding(uint_fast8_t value);
uint_fast8_t sf128_backend_get_flags(void);
void sf128_backend_set_flags(uint_fast8_t value);
}

#include <cstring>

namespace {

float128_t backend(sf128_t x) {
    float128_t out;
#if defined(_WIN32) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    out.v[0] = x.lo;
    out.v[1] = x.hi;
#else
    out.v[0] = x.hi;
    out.v[1] = x.lo;
#endif
    return out;
}

sf128_t external(float128_t x) {
#if defined(_WIN32) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    return sf128_t{x.v[0], x.v[1]};
#else
    return sf128_t{x.v[1], x.v[0]};
#endif
}

uint_fast8_t backend_round(sf64_rounding_mode mode) {
    switch (mode) {
    case SF64_RTZ:
        return softfloat_round_minMag;
    case SF64_RUP:
        return softfloat_round_max;
    case SF64_RDN:
        return softfloat_round_min;
    case SF64_RNA:
        return softfloat_round_near_maxMag;
    case SF64_RNE:
    default:
        return softfloat_round_near_even;
    }
}

unsigned public_flags(uint_fast8_t flags) {
    unsigned out = 0;
    if (flags & softfloat_flag_invalid)
        out |= SF64_FE_INVALID;
    if (flags & softfloat_flag_infinite)
        out |= SF64_FE_DIVBYZERO;
    if (flags & softfloat_flag_overflow)
        out |= SF64_FE_OVERFLOW;
    if (flags & softfloat_flag_underflow)
        out |= SF64_FE_UNDERFLOW;
    if (flags & softfloat_flag_inexact)
        out |= SF64_FE_INEXACT;
    return out;
}

class BackendContext {
  public:
    BackendContext(sf64_rounding_mode mode, sf64_fe_state_t* state, bool explicit_state)
        : old_round_(sf128_backend_get_rounding()), old_flags_(sf128_backend_get_flags()),
          state_(state), explicit_(explicit_state) {
        sf128_backend_set_rounding(backend_round(mode));
        sf128_backend_set_flags(0);
    }
    ~BackendContext() {
        const unsigned flags = public_flags(sf128_backend_get_flags());
        sf128_backend_set_rounding(old_round_);
        sf128_backend_set_flags(old_flags_);
        if (explicit_) {
            if (state_ != nullptr)
                state_->flags |= flags;
        } else if (flags != 0) {
            sf64_fe_raise(flags);
        }
    }

  private:
    uint_fast8_t old_round_;
    uint_fast8_t old_flags_;
    sf64_fe_state_t* state_;
    bool explicit_;
};

using BinaryOp = float128_t (*)(float128_t, float128_t);
using UnaryOp = float128_t (*)(float128_t);

sf128_t binary(sf64_rounding_mode mode, sf128_t a, sf128_t b, sf64_fe_state_t* state,
               bool explicit_state, BinaryOp op) {
    BackendContext context(mode, state, explicit_state);
    return external(op(backend(a), backend(b)));
}

sf128_t unary(sf64_rounding_mode mode, sf128_t x, sf64_fe_state_t* state, bool explicit_state,
              UnaryOp op) {
    BackendContext context(mode, state, explicit_state);
    return external(op(backend(x)));
}

} // namespace

extern "C" sf128_t sf128_from_bits(uint64_t hi, uint64_t lo) {
    return sf128_t{lo, hi};
}
extern "C" uint64_t sf128_bits_hi(sf128_t x) {
    return x.hi;
}
extern "C" uint64_t sf128_bits_lo(sf128_t x) {
    return x.lo;
}

extern "C" sf128_t sf128_from_i32(int32_t x) {
    BackendContext c(SF64_RNE, nullptr, false);
    return external(i32_to_f128(x));
}
extern "C" sf128_t sf128_from_i64(int64_t x) {
    BackendContext c(SF64_RNE, nullptr, false);
    return external(i64_to_f128(x));
}
extern "C" sf128_t sf128_from_u32(uint32_t x) {
    BackendContext c(SF64_RNE, nullptr, false);
    return external(ui32_to_f128(x));
}
extern "C" sf128_t sf128_from_u64(uint64_t x) {
    BackendContext c(SF64_RNE, nullptr, false);
    return external(ui64_to_f128(x));
}
extern "C" sf128_t sf128_from_f64(double x) {
    uint64_t bits;
    std::memcpy(&bits, &x, 8);
    BackendContext c(SF64_RNE, nullptr, false);
    return external(f64_to_f128(float64_t{bits}));
}
extern "C" double sf128_to_f64_r_ex(sf64_rounding_mode m, sf128_t x, sf64_fe_state_t* s) {
    BackendContext c(m, s, true);
    const uint64_t bits = f128_to_f64(backend(x)).v;
    double out;
    std::memcpy(&out, &bits, 8);
    return out;
}
extern "C" double sf128_to_f64_r(sf64_rounding_mode m, sf128_t x) {
    BackendContext c(m, nullptr, false);
    const uint64_t bits = f128_to_f64(backend(x)).v;
    double out;
    std::memcpy(&out, &bits, 8);
    return out;
}
extern "C" double sf128_to_f64(sf128_t x) {
    return sf128_to_f64_r(SF64_RNE, x);
}

#define SF128_TO_INT(type, suffix, backend_name, low, high)                                        \
    static type sf128_to_##suffix##_impl(sf64_rounding_mode m, sf128_t x, sf64_fe_state_t* s,      \
                                         bool explicit_state) {                                    \
        BackendContext c(m, s, explicit_state);                                                    \
        const type out = static_cast<type>(backend_name(backend(x), backend_round(m), true));      \
        if (sf128_backend_get_flags() & softfloat_flag_invalid) {                                  \
            if (((x.hi >> 48) & 0x7FFFu) == 0x7FFFu &&                                             \
                ((x.hi & UINT64_C(0x0000FFFFFFFFFFFF)) || x.lo))                                   \
                return 0;                                                                          \
            return (x.hi >> 63) ? (low) : (high);                                                  \
        }                                                                                          \
        return out;                                                                                \
    }                                                                                              \
    extern "C" type sf128_to_##suffix##_r_ex(sf64_rounding_mode m, sf128_t x,                      \
                                             sf64_fe_state_t* s) {                                 \
        return sf128_to_##suffix##_impl(m, x, s, true);                                            \
    }                                                                                              \
    extern "C" type sf128_to_##suffix##_r(sf64_rounding_mode m, sf128_t x) {                       \
        return sf128_to_##suffix##_impl(m, x, nullptr, false);                                     \
    }                                                                                              \
    extern "C" type sf128_to_##suffix(sf128_t x) {                                                 \
        return sf128_to_##suffix##_r(SF64_RTZ, x);                                                 \
    }
SF128_TO_INT(int32_t, i32, f128_to_i32, INT32_MIN, INT32_MAX)
SF128_TO_INT(int64_t, i64, f128_to_i64, INT64_MIN, INT64_MAX)
SF128_TO_INT(uint32_t, u32, f128_to_ui32, 0, UINT32_MAX)
SF128_TO_INT(uint64_t, u64, f128_to_ui64, 0, UINT64_MAX)
#undef SF128_TO_INT

#define SF128_BINARY(name, backend_name)                                                           \
    extern "C" sf128_t sf128_##name##_r_ex(sf64_rounding_mode m, sf128_t a, sf128_t b,             \
                                           sf64_fe_state_t* s) {                                   \
        return binary(m, a, b, s, true, backend_name);                                             \
    }                                                                                              \
    extern "C" sf128_t sf128_##name##_r(sf64_rounding_mode m, sf128_t a, sf128_t b) {              \
        return binary(m, a, b, nullptr, false, backend_name);                                      \
    }                                                                                              \
    extern "C" sf128_t sf128_##name(sf128_t a, sf128_t b) {                                        \
        return sf128_##name##_r(SF64_RNE, a, b);                                                   \
    }
SF128_BINARY(add, f128_add)
SF128_BINARY(sub, f128_sub)
SF128_BINARY(mul, f128_mul)
SF128_BINARY(div, f128_div)
#undef SF128_BINARY

extern "C" sf128_t sf128_remainder_ex(sf128_t a, sf128_t b, sf64_fe_state_t* s) {
    return binary(SF64_RNE, a, b, s, true, f128_rem);
}
extern "C" sf128_t sf128_remainder(sf128_t a, sf128_t b) {
    return binary(SF64_RNE, a, b, nullptr, false, f128_rem);
}
extern "C" sf128_t sf128_sqrt_r_ex(sf64_rounding_mode m, sf128_t x, sf64_fe_state_t* s) {
    return unary(m, x, s, true, f128_sqrt);
}
extern "C" sf128_t sf128_sqrt_r(sf64_rounding_mode m, sf128_t x) {
    return unary(m, x, nullptr, false, f128_sqrt);
}
extern "C" sf128_t sf128_sqrt(sf128_t x) {
    return sf128_sqrt_r(SF64_RNE, x);
}
extern "C" sf128_t sf128_rint_r_ex(sf64_rounding_mode m, sf128_t x, sf64_fe_state_t* s) {
    BackendContext c(m, s, true);
    return external(f128_roundToInt(backend(x), backend_round(m), true));
}
extern "C" sf128_t sf128_rint_r(sf64_rounding_mode m, sf128_t x) {
    BackendContext c(m, nullptr, false);
    return external(f128_roundToInt(backend(x), backend_round(m), true));
}
extern "C" sf128_t sf128_rint(sf128_t x) {
    return sf128_rint_r(SF64_RNE, x);
}
extern "C" sf128_t sf128_fma_r_ex(sf64_rounding_mode m, sf128_t a, sf128_t b, sf128_t c,
                                  sf64_fe_state_t* s) {
    BackendContext ctx(m, s, true);
    return external(f128_mulAdd(backend(a), backend(b), backend(c)));
}
extern "C" sf128_t sf128_fma_r(sf64_rounding_mode m, sf128_t a, sf128_t b, sf128_t c) {
    BackendContext ctx(m, nullptr, false);
    return external(f128_mulAdd(backend(a), backend(b), backend(c)));
}
extern "C" sf128_t sf128_fma(sf128_t a, sf128_t b, sf128_t c) {
    return sf128_fma_r(SF64_RNE, a, b, c);
}

extern "C" int sf128_isnan(sf128_t x) {
    return ((x.hi >> 48) & 0x7FFFu) == 0x7FFFu &&
           ((x.hi & 0x0000FFFFFFFFFFFFULL) != 0 || x.lo != 0);
}
extern "C" int sf128_isinf(sf128_t x) {
    return ((x.hi >> 48) & 0x7FFFu) == 0x7FFFu && (x.hi & 0x0000FFFFFFFFFFFFULL) == 0 && x.lo == 0;
}
extern "C" int sf128_isfinite(sf128_t x) {
    return ((x.hi >> 48) & 0x7FFFu) != 0x7FFFu;
}
extern "C" int sf128_isnormal(sf128_t x) {
    const uint64_t e = (x.hi >> 48) & 0x7FFFu;
    return e != 0 && e != 0x7FFFu;
}
extern "C" int sf128_signbit(sf128_t x) {
    return (int)(x.hi >> 63);
}
extern "C" int sf128_eq(sf128_t a, sf128_t b) {
    BackendContext c(SF64_RNE, nullptr, false);
    return f128_eq(backend(a), backend(b));
}
extern "C" int sf128_lt(sf128_t a, sf128_t b) {
    BackendContext c(SF64_RNE, nullptr, false);
    return f128_lt(backend(a), backend(b));
}
extern "C" int sf128_le(sf128_t a, sf128_t b) {
    BackendContext c(SF64_RNE, nullptr, false);
    return f128_le(backend(a), backend(b));
}
