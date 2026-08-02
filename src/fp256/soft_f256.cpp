#include "soft_fp256/soft_f256.h"

#include "big_uint.h"

#include <cstring>

using soft_fp::fp256_internal::Big;
using namespace soft_fp::fp256_internal;

namespace {
constexpr int kFrac = 236;
constexpr int kBias = 262143;
constexpr int kEmin = 1 - kBias;
constexpr uint32_t kExpMax = (1u << 19) - 1;
constexpr uint64_t kTopFracMask = (uint64_t{1} << 44) - 1;

struct Decoded {
    bool sign;
    uint32_t field;
    int exponent;
    Big<8> sig;
    bool zero;
    bool inf;
    bool nan;
    bool snan;
};

void raise_flags(sf64_fe_state_t* state, bool explicit_state, unsigned flags) {
    if (flags == 0)
        return;
    if (explicit_state) {
        if (state != nullptr)
            state->flags |= flags;
    } else {
        sf64_fe_raise(flags);
    }
}

Decoded decode(sf256_t x) {
    Decoded d{};
    d.sign = (x.limb[3] >> 63) != 0;
    d.field = static_cast<uint32_t>((x.limb[3] >> 44) & kExpMax);
    for (unsigned i = 0; i < 4; ++i)
        d.sig.v[i] = x.limb[i];
    d.sig.v[3] &= kTopFracMask;
    const bool fraction_zero = zero(d.sig);
    d.zero = d.field == 0 && fraction_zero;
    d.inf = d.field == kExpMax && fraction_zero;
    d.nan = d.field == kExpMax && !fraction_zero;
    d.snan = d.nan && !bit(d.sig, kFrac - 1);
    if (!d.zero && !d.inf && !d.nan) {
        if (d.field == 0) {
            const int shift = kFrac - highest(d.sig);
            d.sig = shl(d.sig, static_cast<unsigned>(shift));
            d.exponent = kEmin - shift;
        } else {
            set_bit(d.sig, kFrac);
            d.exponent = static_cast<int>(d.field) - kBias;
        }
    }
    return d;
}

sf256_t raw(bool sign, uint32_t exponent, const Big<8>& fraction) {
    sf256_t out{};
    for (unsigned i = 0; i < 4; ++i)
        out.limb[i] = fraction.v[i];
    out.limb[3] &= kTopFracMask;
    out.limb[3] |= static_cast<uint64_t>(exponent) << 44;
    out.limb[3] |= static_cast<uint64_t>(sign) << 63;
    return out;
}

sf256_t zero_value(bool sign) {
    return raw(sign, 0, Big<8>{});
}
sf256_t inf_value(bool sign) {
    return raw(sign, kExpMax, Big<8>{});
}
sf256_t default_nan() {
    Big<8> f;
    set_bit(f, kFrac - 1);
    return raw(false, kExpMax, f);
}
sf256_t quiet_nan(sf256_t x) {
    x.limb[3] |= uint64_t{1} << 43;
    return x;
}

sf256_t adjacent(sf256_t x, bool toward_positive) {
    const bool sign = (x.limb[3] >> 63) != 0;
    const bool increment_bits = toward_positive != sign;
    if (increment_bits) {
        for (unsigned i = 0; i < 4; ++i)
            if (++x.limb[i] != 0)
                break;
    } else {
        for (unsigned i = 0; i < 4; ++i) {
            const uint64_t old = x.limb[i];
            --x.limb[i];
            if (old != 0)
                break;
        }
    }
    return x;
}

sf256_t propagate_nan(sf256_t a, sf256_t b, sf64_fe_state_t* state, bool explicit_state) {
    const Decoded da = decode(a), db = decode(b);
    if (da.snan || db.snan)
        raise_flags(state, explicit_state, SF64_FE_INVALID);
    if (da.nan)
        return quiet_nan(a);
    if (db.nan)
        return quiet_nan(b);
    return default_nan();
}

bool increment(Big<8>& a) {
    for (unsigned i = 0; i < 8; ++i) {
        if (++a.v[i] != 0)
            return false;
    }
    return true;
}

bool round_up(bool sign, bool guard, bool sticky, bool odd, sf64_rounding_mode mode) {
    if (!guard && !sticky)
        return false;
    switch (mode) {
    case SF64_RTZ:
        return false;
    case SF64_RUP:
        return !sign;
    case SF64_RDN:
        return sign;
    case SF64_RNA:
        return guard;
    case SF64_RNE:
    default:
        return guard && (sticky || odd);
    }
}

sf256_t pack_extended(bool sign, Big<8> ext, int exponent, sf64_rounding_mode mode,
                      sf64_fe_state_t* state, bool explicit_state) {
    if (zero(ext))
        return zero_value(sign);
    if (exponent < kEmin) {
        ext = shr_jam(ext, static_cast<unsigned>(kEmin - exponent));
        exponent = kEmin;
    }

    const bool guard = bit(ext, 2);
    const bool sticky = bit(ext, 1) || bit(ext, 0);
    Big<8> sig = shr(ext, 3);
    const bool inexact = guard || sticky;
    if (round_up(sign, guard, sticky, bit(sig, 0), mode))
        increment(sig);
    if (bit(sig, kFrac + 1)) {
        sig = shr(sig, 1);
        ++exponent;
    }
    if (exponent > kBias) {
        raise_flags(state, explicit_state, SF64_FE_OVERFLOW | SF64_FE_INEXACT);
        const bool to_inf = mode == SF64_RNE || mode == SF64_RNA || (mode == SF64_RUP && !sign) ||
                            (mode == SF64_RDN && sign);
        if (to_inf)
            return inf_value(sign);
        Big<8> max;
        for (unsigned i = 0; i < 3; ++i)
            max.v[i] = ~uint64_t{0};
        max.v[3] = kTopFracMask;
        return raw(sign, kExpMax - 1, max);
    }
    if (inexact) {
        unsigned flags = SF64_FE_INEXACT;
        /* IEEE-754 tininess-after-rounding: a midpoint immediately below
         * the minimum normal that rounds up to that normal is not an
         * underflow. */
        if (exponent == kEmin && !bit(sig, kFrac))
            flags |= SF64_FE_UNDERFLOW;
        raise_flags(state, explicit_state, flags);
    }
    if (exponent == kEmin && !bit(sig, kFrac))
        return raw(sign, 0, sig);
    sig.v[kFrac / 64] &= ~(uint64_t{1} << (kFrac % 64));
    return raw(sign, static_cast<uint32_t>(exponent + kBias), sig);
}

sf256_t pack_scaled(bool sign, Big<8> mag, int scale, sf64_rounding_mode mode,
                    sf64_fe_state_t* state, bool explicit_state) {
    const int msb = highest(mag);
    if (msb < 0)
        return zero_value(sign);
    const int target = kFrac + 3;
    Big<8> ext = msb > target ? shr_jam(mag, static_cast<unsigned>(msb - target))
                              : shl(mag, static_cast<unsigned>(target - msb));
    return pack_extended(sign, ext, scale + msb, mode, state, explicit_state);
}

Big<8> frame(const Big<8>& value, int scale, int common_scale) {
    if (zero(value))
        return {};
    const int shift = scale - common_scale;
    return shift >= 0 ? shl(value, static_cast<unsigned>(shift))
                      : shr_jam(value, static_cast<unsigned>(-shift));
}

sf256_t add_impl(sf64_rounding_mode mode, sf256_t a, sf256_t b, sf64_fe_state_t* state,
                 bool explicit_state) {
    Decoded da = decode(a), db = decode(b);
    if (da.nan || db.nan)
        return propagate_nan(a, b, state, explicit_state);
    if (da.inf || db.inf) {
        if (da.inf && db.inf && da.sign != db.sign) {
            raise_flags(state, explicit_state, SF64_FE_INVALID);
            return default_nan();
        }
        return da.inf ? a : b;
    }
    if (da.zero && db.zero)
        return zero_value(da.sign == db.sign ? da.sign : mode == SF64_RDN);
    if (da.zero)
        return b;
    if (db.zero)
        return a;
    const int difference =
        da.exponent > db.exponent ? da.exponent - db.exponent : db.exponent - da.exponent;
    if (difference <= 274) {
        const int common = da.exponent < db.exponent ? da.exponent : db.exponent;
        const Big<8> ea = shl(da.sig, static_cast<unsigned>(da.exponent - common));
        const Big<8> eb = shl(db.sig, static_cast<unsigned>(db.exponent - common));
        Big<8> exact;
        bool exact_sign;
        if (da.sign == db.sign) {
            exact = add(ea, eb);
            exact_sign = da.sign;
        } else {
            const int cmp = compare(ea, eb);
            if (cmp == 0)
                return zero_value(mode == SF64_RDN);
            if (cmp > 0) {
                exact = sub(ea, eb);
                exact_sign = da.sign;
            } else {
                exact = sub(eb, ea);
                exact_sign = db.sign;
            }
        }
        return pack_scaled(exact_sign, exact, common - kFrac, mode, state, explicit_state);
    }
    const bool a_dominates = da.exponent > db.exponent;
    const sf256_t dominant = a_dominates ? a : b;
    const bool dominant_sign = a_dominates ? da.sign : db.sign;
    const bool delta_sign = a_dominates ? db.sign : da.sign;
    bool step = false;
    bool toward_positive = !delta_sign;
    if (mode == SF64_RUP)
        step = !delta_sign;
    else if (mode == SF64_RDN)
        step = delta_sign;
    else if (mode == SF64_RTZ) {
        step = dominant_sign != delta_sign;
        toward_positive = dominant_sign;
    }
    raise_flags(state, explicit_state, SF64_FE_INEXACT);
    return step ? adjacent(dominant, toward_positive) : dominant;
}

sf256_t mul_impl(sf64_rounding_mode mode, sf256_t a, sf256_t b, sf64_fe_state_t* state,
                 bool explicit_state) {
    const Decoded da = decode(a), db = decode(b);
    const bool sign = da.sign != db.sign;
    if (da.nan || db.nan)
        return propagate_nan(a, b, state, explicit_state);
    if ((da.inf && db.zero) || (db.inf && da.zero)) {
        raise_flags(state, explicit_state, SF64_FE_INVALID);
        return default_nan();
    }
    if (da.inf || db.inf)
        return inf_value(sign);
    if (da.zero || db.zero)
        return zero_value(sign);
    const Big<8> product = multiply(widen<4>(da.sig), widen<4>(db.sig));
    return pack_scaled(sign, product, da.exponent + db.exponent - 2 * kFrac, mode, state,
                       explicit_state);
}

sf256_t div_impl(sf64_rounding_mode mode, sf256_t a, sf256_t b, sf64_fe_state_t* state,
                 bool explicit_state) {
    Decoded da = decode(a), db = decode(b);
    const bool sign = da.sign != db.sign;
    if (da.nan || db.nan)
        return propagate_nan(a, b, state, explicit_state);
    if ((da.inf && db.inf) || (da.zero && db.zero)) {
        raise_flags(state, explicit_state, SF64_FE_INVALID);
        return default_nan();
    }
    if (da.inf)
        return inf_value(sign);
    if (db.inf)
        return zero_value(sign);
    if (db.zero) {
        raise_flags(state, explicit_state, SF64_FE_DIVBYZERO);
        return inf_value(sign);
    }
    if (da.zero)
        return zero_value(sign);
    int exponent = da.exponent - db.exponent;
    Big<8> rem = da.sig;
    if (compare(rem, db.sig) < 0) {
        rem = shl(rem, 1);
        --exponent;
    }
    Big<8> q;
    for (int pos = kFrac + 3; pos >= 0; --pos) {
        if (compare(rem, db.sig) >= 0) {
            rem = sub(rem, db.sig);
            set_bit(q, pos);
        }
        if (pos != 0)
            rem = shl(rem, 1);
    }
    if (!zero(rem))
        q.v[0] |= 1;
    return pack_scaled(sign, q, exponent - kFrac - 3, mode, state, explicit_state);
}

sf256_t sqrt_impl(sf64_rounding_mode mode, sf256_t x, sf64_fe_state_t* state, bool explicit_state) {
    Decoded d = decode(x);
    if (d.nan)
        return propagate_nan(x, x, state, explicit_state);
    if (d.sign && !d.zero) {
        raise_flags(state, explicit_state, SF64_FE_INVALID);
        return default_nan();
    }
    if (d.inf || d.zero)
        return x;
    if ((d.exponent & 1) != 0) {
        d.sig = shl(d.sig, 1);
        --d.exponent;
    }
    const Big<8> rad = shl(d.sig, kFrac + 6);
    Big<8> rem, root;
    for (int i = kFrac + 3; i >= 0; --i) {
        rem = shl(rem, 2);
        if (bit(rad, static_cast<unsigned>(i * 2 + 1)))
            rem.v[0] |= 2;
        if (bit(rad, static_cast<unsigned>(i * 2)))
            rem.v[0] |= 1;
        Big<8> trial = shl(root, 2);
        trial.v[0] |= 1;
        if (compare(rem, trial) >= 0) {
            rem = sub(rem, trial);
            root = shl(root, 1);
            root.v[0] |= 1;
        } else
            root = shl(root, 1);
    }
    if (!zero(rem))
        root.v[0] |= 1;
    return pack_scaled(false, root, d.exponent / 2 - kFrac - 3, mode, state, explicit_state);
}

sf256_t fma_impl(sf64_rounding_mode mode, sf256_t a, sf256_t b, sf256_t c, sf64_fe_state_t* state,
                 bool explicit_state) {
    const Decoded da = decode(a), db = decode(b), dc = decode(c);
    if (da.nan || db.nan || dc.nan) {
        sf256_t ab = propagate_nan(a, b, state, explicit_state);
        return (da.nan || db.nan) ? ab : propagate_nan(c, c, state, explicit_state);
    }
    if ((da.inf && db.zero) || (db.inf && da.zero)) {
        raise_flags(state, explicit_state, SF64_FE_INVALID);
        return default_nan();
    }
    const bool product_sign = da.sign != db.sign;
    if (da.inf || db.inf) {
        if (dc.inf && dc.sign != product_sign) {
            raise_flags(state, explicit_state, SF64_FE_INVALID);
            return default_nan();
        }
        return inf_value(product_sign);
    }
    if (dc.inf)
        return c;
    if (da.zero || db.zero)
        return add_impl(mode, zero_value(product_sign), c, state, explicit_state);
    const Big<8> product = multiply(widen<4>(da.sig), widen<4>(db.sig));
    const int ps = da.exponent + db.exponent - 2 * kFrac;
    if (dc.zero)
        return pack_scaled(product_sign, product, ps, mode, state, explicit_state);
    const int cs = dc.exponent - kFrac;
    const int top = (ps + highest(product) > cs + highest(dc.sig)) ? ps + highest(product)
                                                                   : cs + highest(dc.sig);
    const int common = top - 509;
    Big<8> pf = frame(product, ps, common), cf = frame(dc.sig, cs, common), mag;
    bool sign;
    if (product_sign == dc.sign) {
        mag = add(pf, cf);
        sign = product_sign;
    } else {
        const int cmp = compare(pf, cf);
        if (cmp == 0)
            return zero_value(mode == SF64_RDN);
        if (cmp > 0) {
            mag = sub(pf, cf);
            sign = product_sign;
        } else {
            mag = sub(cf, pf);
            sign = dc.sign;
        }
    }
    return pack_scaled(sign, mag, common, mode, state, explicit_state);
}

uint64_t f64_bits(double x) {
    uint64_t b;
    std::memcpy(&b, &x, 8);
    return b;
}
double f64_value(uint64_t b) {
    double x;
    std::memcpy(&x, &b, 8);
    return x;
}

double to_f64_impl(sf64_rounding_mode mode, sf256_t x, sf64_fe_state_t* state,
                   bool explicit_state) {
    Decoded d = decode(x);
    if (d.nan) {
        if (d.snan)
            raise_flags(state, explicit_state, SF64_FE_INVALID);
        uint64_t payload = (d.sig.v[3] >> 32) & 0x0007FFFFFFFFFFFFULL;
        return f64_value((uint64_t(d.sign) << 63) | 0x7FF8000000000000ULL | payload);
    }
    if (d.inf)
        return f64_value((uint64_t(d.sign) << 63) | 0x7FF0000000000000ULL);
    if (d.zero)
        return f64_value(uint64_t(d.sign) << 63);
    int exponent = d.exponent;
    Big<8> ext = shr_jam(d.sig, kFrac - 55);
    if (exponent < -1022) {
        ext = shr_jam(ext, static_cast<unsigned>(-1022 - exponent));
        exponent = -1022;
    }
    bool guard = bit(ext, 2), sticky = bit(ext, 1) || bit(ext, 0);
    Big<8> sig = shr(ext, 3);
    const bool inexact = guard || sticky;
    if (round_up(d.sign, guard, sticky, bit(sig, 0), mode))
        increment(sig);
    if (bit(sig, 53)) {
        sig = shr(sig, 1);
        ++exponent;
    }
    if (exponent > 1023) {
        raise_flags(state, explicit_state, SF64_FE_OVERFLOW | SF64_FE_INEXACT);
        const bool inf = mode == SF64_RNE || mode == SF64_RNA || (mode == SF64_RUP && !d.sign) ||
                         (mode == SF64_RDN && d.sign);
        return f64_value((uint64_t(d.sign) << 63) |
                         (inf ? 0x7FF0000000000000ULL : 0x7FEFFFFFFFFFFFFFULL));
    }
    if (inexact) {
        const bool tiny_after_rounding = exponent == -1022 && !bit(sig, 52);
        raise_flags(state, explicit_state,
                    SF64_FE_INEXACT | (tiny_after_rounding ? SF64_FE_UNDERFLOW : 0));
    }
    uint64_t field =
        (exponent == -1022 && !bit(sig, 52)) ? 0 : static_cast<uint64_t>(exponent + 1023);
    return f64_value((uint64_t(d.sign) << 63) | (field << 52) | (sig.v[0] & 0x000FFFFFFFFFFFFFULL));
}

Big<8> rounded_integer(const Decoded& d, sf64_rounding_mode mode, bool& inexact) {
    Big<8> mag;
    if (d.exponent >= kFrac)
        return shl(d.sig, static_cast<unsigned>(d.exponent - kFrac));
    const unsigned shift = static_cast<unsigned>(kFrac - d.exponent);
    mag = shr(d.sig, shift);
    const bool guard = shift > 0 && bit(d.sig, shift - 1);
    const bool sticky = shift > 1 && any_low(d.sig, shift - 1);
    inexact = guard || sticky;
    if (round_up(d.sign, guard, sticky, bit(mag, 0), mode))
        increment(mag);
    return mag;
}

sf256_t rint_impl(sf64_rounding_mode mode, sf256_t x, sf64_fe_state_t* state, bool explicit_state) {
    const Decoded d = decode(x);
    if (d.nan) {
        if (d.snan)
            raise_flags(state, explicit_state, SF64_FE_INVALID);
        return quiet_nan(x);
    }
    if (d.inf || d.zero || d.exponent >= kFrac)
        return x;
    bool inexact = false;
    Big<8> mag = rounded_integer(d, mode, inexact);
    if (inexact)
        raise_flags(state, explicit_state, SF64_FE_INEXACT);
    return pack_scaled(d.sign, mag, 0, SF64_RNE, state, explicit_state);
}

sf256_t remainder_impl(sf256_t a, sf256_t b, sf64_fe_state_t* state, bool explicit_state) {
    const Decoded da = decode(a), db = decode(b);
    if (da.nan || db.nan)
        return propagate_nan(a, b, state, explicit_state);
    if (da.inf || db.zero) {
        raise_flags(state, explicit_state, SF64_FE_INVALID);
        return default_nan();
    }
    if (da.zero || db.inf)
        return a;
    const int difference = da.exponent - db.exponent;
    Big<8> rem = da.sig;
    bool quotient_odd = false;
    bool complement = false;
    if (difference >= 0) {
        for (int i = difference; i >= 0; --i) {
            quotient_odd = compare(rem, db.sig) >= 0;
            if (quotient_odd)
                rem = sub(rem, db.sig);
            if (i)
                rem = shl(rem, 1);
        }
        Big<8> twice = shl(rem, 1);
        const int half = compare(twice, db.sig);
        complement = half > 0 || (half == 0 && quotient_odd);
    } else if (difference == -1) {
        const int half = compare(da.sig, db.sig);
        complement = half > 0; /* exact half chooses the even integer zero */
        rem = da.sig;
    } else
        return a;
    bool sign = da.sign;
    int scale = db.exponent - kFrac;
    if (difference == -1)
        scale = da.exponent - kFrac;
    if (complement) {
        if (difference == -1) {
            Big<8> divisor = shl(db.sig, 1);
            rem = sub(divisor, rem);
        } else
            rem = sub(db.sig, rem);
        sign = !sign;
    }
    return pack_scaled(sign, rem, scale, SF64_RNE, state, explicit_state);
}

uint64_t to_u64_impl(sf64_rounding_mode mode, sf256_t x, sf64_fe_state_t* state,
                     bool explicit_state) {
    const Decoded d = decode(x);
    if (d.nan) {
        raise_flags(state, explicit_state, SF64_FE_INVALID);
        return 0;
    }
    if (d.inf || d.sign) {
        raise_flags(state, explicit_state, SF64_FE_INVALID);
        return d.sign ? 0 : UINT64_MAX;
    }
    if (d.zero)
        return 0;
    if (d.exponent > 63) {
        raise_flags(state, explicit_state, SF64_FE_INVALID);
        return UINT64_MAX;
    }
    bool inexact = false;
    Big<8> mag = rounded_integer(d, mode, inexact);
    if (highest(mag) > 63) {
        raise_flags(state, explicit_state, SF64_FE_INVALID);
        return UINT64_MAX;
    }
    if (inexact)
        raise_flags(state, explicit_state, SF64_FE_INEXACT);
    return mag.v[0];
}

int64_t to_i64_impl(sf64_rounding_mode mode, sf256_t x, sf64_fe_state_t* state,
                    bool explicit_state) {
    const Decoded d = decode(x);
    if (d.nan) {
        raise_flags(state, explicit_state, SF64_FE_INVALID);
        return 0;
    }
    if (d.inf) {
        raise_flags(state, explicit_state, SF64_FE_INVALID);
        return d.sign ? INT64_MIN : INT64_MAX;
    }
    if (d.zero)
        return 0;
    if (d.exponent > 63) {
        raise_flags(state, explicit_state, SF64_FE_INVALID);
        return d.sign ? INT64_MIN : INT64_MAX;
    }
    bool inexact = false;
    Big<8> mag = rounded_integer(d, mode, inexact);
    if (highest(mag) > 63 || (!d.sign && bit(mag, 63)) ||
        (d.sign && bit(mag, 63) && mag.v[0] != UINT64_C(0x8000000000000000))) {
        raise_flags(state, explicit_state, SF64_FE_INVALID);
        return d.sign ? INT64_MIN : INT64_MAX;
    }
    if (inexact)
        raise_flags(state, explicit_state, SF64_FE_INEXACT);
    if (d.sign)
        return mag.v[0] == UINT64_C(0x8000000000000000) ? INT64_MIN
                                                        : -static_cast<int64_t>(mag.v[0]);
    return static_cast<int64_t>(mag.v[0]);
}

} // namespace

extern "C" sf256_t sf256_from_bits(uint64_t w3, uint64_t w2, uint64_t w1, uint64_t w0) {
    return sf256_t{{w0, w1, w2, w3}};
}
extern "C" uint64_t sf256_bits(sf256_t x, unsigned word) {
    return word < 4 ? x.limb[word] : 0;
}
extern "C" sf256_t sf256_from_u64(uint64_t x) {
    Big<8> m;
    m.v[0] = x;
    return pack_scaled(false, m, 0, SF64_RNE, nullptr, false);
}
extern "C" sf256_t sf256_from_i64(int64_t x) {
    bool s = x < 0;
    uint64_t m = s ? uint64_t(0) - static_cast<uint64_t>(x) : static_cast<uint64_t>(x);
    Big<8> b;
    b.v[0] = m;
    return pack_scaled(s, b, 0, SF64_RNE, nullptr, false);
}
extern "C" sf256_t sf256_from_f64(double x) {
    uint64_t b = f64_bits(x);
    bool s = b >> 63;
    uint32_t e = (b >> 52) & 0x7FF;
    uint64_t f = b & 0xFFFFFFFFFFFFFULL;
    if (e == 0x7FF) {
        if (!f)
            return inf_value(s);
        Big<8> p;
        p.v[0] = f;
        p = shl(p, kFrac - 52);
        set_bit(p, kFrac - 1);
        return raw(s, kExpMax, p);
    }
    if (e == 0 && !f)
        return zero_value(s);
    int ex;
    Big<8> m;
    m.v[0] = f;
    if (e) {
        m.v[0] |= uint64_t{1} << 52;
        ex = int(e) - 1023;
    } else {
        int sh = 52 - highest(m);
        m = shl(m, sh);
        ex = -1022 - sh;
    }
    return pack_scaled(s, m, ex - 52, SF64_RNE, nullptr, false);
}
extern "C" double sf256_to_f64_r_ex(sf64_rounding_mode m, sf256_t x, sf64_fe_state_t* s) {
    return to_f64_impl(m, x, s, true);
}
extern "C" double sf256_to_f64_r(sf64_rounding_mode m, sf256_t x) {
    return to_f64_impl(m, x, nullptr, false);
}
extern "C" double sf256_to_f64(sf256_t x) {
    return to_f64_impl(SF64_RNE, x, nullptr, false);
}
extern "C" uint64_t sf256_to_u64_r_ex(sf64_rounding_mode m, sf256_t x, sf64_fe_state_t* s) {
    return to_u64_impl(m, x, s, true);
}
extern "C" uint64_t sf256_to_u64_r(sf64_rounding_mode m, sf256_t x) {
    return to_u64_impl(m, x, nullptr, false);
}
extern "C" uint64_t sf256_to_u64(sf256_t x) {
    return to_u64_impl(SF64_RTZ, x, nullptr, false);
}
extern "C" int64_t sf256_to_i64_r_ex(sf64_rounding_mode m, sf256_t x, sf64_fe_state_t* s) {
    return to_i64_impl(m, x, s, true);
}
extern "C" int64_t sf256_to_i64_r(sf64_rounding_mode m, sf256_t x) {
    return to_i64_impl(m, x, nullptr, false);
}
extern "C" int64_t sf256_to_i64(sf256_t x) {
    return to_i64_impl(SF64_RTZ, x, nullptr, false);
}
extern "C" uint32_t sf256_to_u32(sf256_t x) {
    uint64_t v = sf256_to_u64(x);
    if (v > UINT32_MAX) {
        sf64_fe_raise(SF64_FE_INVALID);
        return UINT32_MAX;
    }
    return static_cast<uint32_t>(v);
}
extern "C" int32_t sf256_to_i32(sf256_t x) {
    int64_t v = sf256_to_i64(x);
    if (v > INT32_MAX) {
        sf64_fe_raise(SF64_FE_INVALID);
        return INT32_MAX;
    }
    if (v < INT32_MIN) {
        sf64_fe_raise(SF64_FE_INVALID);
        return INT32_MIN;
    }
    return static_cast<int32_t>(v);
}

#define BIN(name, impl)                                                                            \
    extern "C" sf256_t sf256_##name##_r_ex(sf64_rounding_mode m, sf256_t a, sf256_t b,             \
                                           sf64_fe_state_t* s) {                                   \
        return impl(m, a, b, s, true);                                                             \
    }                                                                                              \
    extern "C" sf256_t sf256_##name##_r(sf64_rounding_mode m, sf256_t a, sf256_t b) {              \
        return impl(m, a, b, nullptr, false);                                                      \
    }                                                                                              \
    extern "C" sf256_t sf256_##name(sf256_t a, sf256_t b) {                                        \
        return impl(SF64_RNE, a, b, nullptr, false);                                               \
    }
BIN(add, add_impl)
extern "C" sf256_t sf256_sub_r_ex(sf64_rounding_mode m, sf256_t a, sf256_t b, sf64_fe_state_t* s) {
    b.limb[3] ^= uint64_t{1} << 63;
    return add_impl(m, a, b, s, true);
}
extern "C" sf256_t sf256_sub_r(sf64_rounding_mode m, sf256_t a, sf256_t b) {
    b.limb[3] ^= uint64_t{1} << 63;
    return add_impl(m, a, b, nullptr, false);
}
extern "C" sf256_t sf256_sub(sf256_t a, sf256_t b) {
    return sf256_sub_r(SF64_RNE, a, b);
}
BIN(mul, mul_impl)
BIN(div, div_impl)
#undef BIN
extern "C" sf256_t sf256_sqrt_r_ex(sf64_rounding_mode m, sf256_t x, sf64_fe_state_t* s) {
    return sqrt_impl(m, x, s, true);
}
extern "C" sf256_t sf256_sqrt_r(sf64_rounding_mode m, sf256_t x) {
    return sqrt_impl(m, x, nullptr, false);
}
extern "C" sf256_t sf256_sqrt(sf256_t x) {
    return sqrt_impl(SF64_RNE, x, nullptr, false);
}
extern "C" sf256_t sf256_fma_r_ex(sf64_rounding_mode m, sf256_t a, sf256_t b, sf256_t c,
                                  sf64_fe_state_t* s) {
    return fma_impl(m, a, b, c, s, true);
}
extern "C" sf256_t sf256_fma_r(sf64_rounding_mode m, sf256_t a, sf256_t b, sf256_t c) {
    return fma_impl(m, a, b, c, nullptr, false);
}
extern "C" sf256_t sf256_fma(sf256_t a, sf256_t b, sf256_t c) {
    return fma_impl(SF64_RNE, a, b, c, nullptr, false);
}
extern "C" sf256_t sf256_remainder_ex(sf256_t a, sf256_t b, sf64_fe_state_t* s) {
    return remainder_impl(a, b, s, true);
}
extern "C" sf256_t sf256_remainder(sf256_t a, sf256_t b) {
    return remainder_impl(a, b, nullptr, false);
}
extern "C" sf256_t sf256_rint_r_ex(sf64_rounding_mode m, sf256_t x, sf64_fe_state_t* s) {
    return rint_impl(m, x, s, true);
}
extern "C" sf256_t sf256_rint_r(sf64_rounding_mode m, sf256_t x) {
    return rint_impl(m, x, nullptr, false);
}
extern "C" sf256_t sf256_rint(sf256_t x) {
    return rint_impl(SF64_RNE, x, nullptr, false);
}
extern "C" int sf256_isnan(sf256_t x) {
    return decode(x).nan;
}
extern "C" int sf256_isinf(sf256_t x) {
    return decode(x).inf;
}
extern "C" int sf256_isfinite(sf256_t x) {
    Decoded d = decode(x);
    return !d.inf && !d.nan;
}
extern "C" int sf256_isnormal(sf256_t x) {
    uint32_t e = decode(x).field;
    return e && e != kExpMax;
}
extern "C" int sf256_signbit(sf256_t x) {
    return x.limb[3] >> 63;
}
extern "C" int sf256_eq(sf256_t a, sf256_t b) {
    Decoded da = decode(a), db = decode(b);
    if (da.nan || db.nan)
        return 0;
    if (da.zero && db.zero)
        return 1;
    for (int i = 3; i >= 0; --i)
        if (a.limb[i] != b.limb[i])
            return 0;
    return 1;
}
extern "C" int sf256_lt(sf256_t a, sf256_t b) {
    Decoded da = decode(a), db = decode(b);
    if (da.nan || db.nan)
        return 0;
    if (da.zero && db.zero)
        return 0;
    if (da.sign != db.sign)
        return da.sign;
    int cmp = 0;
    for (int i = 3; i >= 0; --i) {
        uint64_t av = a.limb[i] & ((i == 3) ? ~(uint64_t{1} << 63) : ~uint64_t{0}),
                 bv = b.limb[i] & ((i == 3) ? ~(uint64_t{1} << 63) : ~uint64_t{0});
        if (av != bv) {
            cmp = av < bv ? -1 : 1;
            break;
        }
    }
    return da.sign ? cmp > 0 : cmp < 0;
}
extern "C" int sf256_le(sf256_t a, sf256_t b) {
    Decoded da = decode(a), db = decode(b);
    return (!da.nan && !db.nan) && (sf256_lt(a, b) || sf256_eq(a, b));
}
