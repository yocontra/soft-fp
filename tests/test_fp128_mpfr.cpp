#include "soft_fp128/soft_f128.h"

#include <gmp.h>
#include <mpfr.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>

namespace {
constexpr int kFrac = 112, kBias = 16383;
mpfr_rnd_t rnd(sf64_rounding_mode m) {
    switch (m) {
    case SF64_RTZ:
        return MPFR_RNDZ;
    case SF64_RUP:
        return MPFR_RNDU;
    case SF64_RDN:
        return MPFR_RNDD;
    default:
        return MPFR_RNDN;
    }
}
#define REF2(name)                                                                                 \
    int ref_##name(mpfr_ptr r, mpfr_srcptr a, mpfr_srcptr b, sf64_rounding_mode m) {               \
        if (m != SF64_RNA)                                                                         \
            return mpfr_##name(r, a, b, rnd(m));                                                   \
        mpfr_round_nearest_away_begin(r);                                                          \
        return mpfr_round_nearest_away_end(r, mpfr_##name(r, a, b, MPFR_RNDN));                    \
    }
REF2(add)
REF2(sub)
REF2(mul)
REF2(div)
#undef REF2
int ref_sqrt(mpfr_ptr r, mpfr_srcptr a, sf64_rounding_mode m) {
    return m == SF64_RNA ? mpfr_round_nearest_away(mpfr_sqrt, r, a) : mpfr_sqrt(r, a, rnd(m));
}
int ref_fma(mpfr_ptr r, mpfr_srcptr a, mpfr_srcptr b, mpfr_srcptr c, sf64_rounding_mode m) {
    if (m != SF64_RNA)
        return mpfr_fma(r, a, b, c, rnd(m));
    mpfr_round_nearest_away_begin(r);
    return mpfr_round_nearest_away_end(r, mpfr_fma(r, a, b, c, MPFR_RNDN));
}
int ref_rint(mpfr_ptr r, mpfr_srcptr a, sf64_rounding_mode m) {
    if (m != SF64_RNA)
        return mpfr_rint(r, a, rnd(m));
    return mpfr_round(r, a);
}
void to_mpfr(mpfr_t out, sf128_t x) {
    uint64_t words[2] = {x.lo, x.hi & UINT64_C(0x0000FFFFFFFFFFFF)};
    unsigned field = (x.hi >> 48) & 0x7FFFu;
    if (field)
        words[1] |= UINT64_C(1) << 48;
    mpz_t z;
    mpz_init(z);
    mpz_import(z, 2, -1, sizeof(uint64_t), 0, 0, words);
    mpfr_set_z(out, z, MPFR_RNDN);
    mpfr_mul_2si(out, out, (field ? int(field) - kBias : 1 - kBias) - kFrac, MPFR_RNDN);
    if (x.hi >> 63)
        mpfr_neg(out, out, MPFR_RNDN);
    mpz_clear(z);
}
sf128_t from_mpfr(const mpfr_t x) {
    if (mpfr_zero_p(x))
        return sf128_from_bits(mpfr_signbit(x) ? UINT64_C(1) << 63 : 0, 0);
    mpz_t z;
    mpz_init(z);
    mpfr_exp_t scale = mpfr_get_z_2exp(z, x);
    bool sign = mpz_sgn(z) < 0;
    if (sign)
        mpz_neg(z, z);
    size_t n = mpz_sizeinbase(z, 2);
    int exponent = int(scale) + int(n) - 1;
    if (n < 113)
        mpz_mul_2exp(z, z, 113 - n);
    uint64_t words[2]{};
    size_t count = 0;
    mpz_export(words, &count, -1, sizeof(uint64_t), 0, 0, z);
    mpz_clear(z);
    words[1] &= UINT64_C(0x0000FFFFFFFFFFFF);
    words[1] |= uint64_t(exponent + kBias) << 48;
    if (sign)
        words[1] |= UINT64_C(1) << 63;
    return sf128_from_bits(words[1], words[0]);
}
sf128_t random_normal(std::mt19937_64& g, bool positive = false) {
    uint64_t hi = g() & UINT64_C(0x0000FFFFFFFFFFFF);
    int exponent = int(g() % 2001) - 1000;
    hi |= uint64_t(exponent + kBias) << 48;
    if (!positive && (g() & 1))
        hi |= UINT64_C(1) << 63;
    return sf128_from_bits(hi, g());
}
bool same(sf128_t a, sf128_t b) {
    return a.hi == b.hi && a.lo == b.lo;
}
[[noreturn]] void fail(const char* op, sf64_rounding_mode m, sf128_t g, sf128_t e) {
    std::fprintf(stderr, "%s mode=%d got=%016llx:%016llx expected=%016llx:%016llx\n", op, int(m),
                 (unsigned long long)g.hi, (unsigned long long)g.lo, (unsigned long long)e.hi,
                 (unsigned long long)e.lo);
    std::abort();
}
} // namespace
int main() {
    std::mt19937_64 gen(UINT64_C(0x5F128));
    mpfr_t a, b, c, r;
    mpfr_inits2(400, a, b, c, (mpfr_ptr)0);
    mpfr_init2(r, 113);
    const sf64_rounding_mode modes[] = {SF64_RNE, SF64_RTZ, SF64_RUP, SF64_RDN, SF64_RNA};
    for (auto mode : modes)
        for (int i = 0; i < 1000; ++i) {
            sf128_t sa = random_normal(gen), sb = random_normal(gen), sc = random_normal(gen);
            to_mpfr(a, sa);
            to_mpfr(b, sb);
            to_mpfr(c, sc);
#define CHECK(name, expr, call)                                                                    \
    do {                                                                                           \
        expr;                                                                                      \
        sf128_t expected = from_mpfr(r);                                                           \
        sf128_t got = call;                                                                        \
        if (!same(got, expected))                                                                  \
            fail(name, mode, got, expected);                                                       \
    } while (0)
            CHECK("add", ref_add(r, a, b, mode), sf128_add_r(mode, sa, sb));
            CHECK("sub", ref_sub(r, a, b, mode), sf128_sub_r(mode, sa, sb));
            CHECK("mul", ref_mul(r, a, b, mode), sf128_mul_r(mode, sa, sb));
            CHECK("div", ref_div(r, a, b, mode), sf128_div_r(mode, sa, sb));
            CHECK("fma", ref_fma(r, a, b, c, mode), sf128_fma_r(mode, sa, sb, sc));
            CHECK("rint", ref_rint(r, a, mode), sf128_rint_r(mode, sa));
            if (mode == SF64_RNE)
                CHECK("remainder", mpfr_remainder(r, a, b, MPFR_RNDN), sf128_remainder(sa, sb));
            sf128_t sp = random_normal(gen, true);
            to_mpfr(a, sp);
            CHECK("sqrt", ref_sqrt(r, a, mode), sf128_sqrt_r(mode, sp));
#undef CHECK
        }
    mpfr_clears(a, b, c, r, (mpfr_ptr)0);
}
