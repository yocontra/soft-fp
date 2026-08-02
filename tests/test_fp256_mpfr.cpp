#include "soft_fp256/soft_f256.h"

#include <gmp.h>
#include <mpfr.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <random>

namespace {
constexpr int kFrac = 236, kBias = 262143;

mpfr_rnd_t mp_round(sf64_rounding_mode m) {
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
            return mpfr_##name(r, a, b, mp_round(m));                                              \
        mpfr_round_nearest_away_begin(r);                                                          \
        return mpfr_round_nearest_away_end(r, mpfr_##name(r, a, b, MPFR_RNDN));                    \
    }
REF2(add)
REF2(sub)
REF2(mul)
REF2(div)
#undef REF2
int ref_sqrt(mpfr_ptr r, mpfr_srcptr a, sf64_rounding_mode m) {
    return m == SF64_RNA ? mpfr_round_nearest_away(mpfr_sqrt, r, a) : mpfr_sqrt(r, a, mp_round(m));
}
int ref_fma(mpfr_ptr r, mpfr_srcptr a, mpfr_srcptr b, mpfr_srcptr c, sf64_rounding_mode m) {
    if (m != SF64_RNA)
        return mpfr_fma(r, a, b, c, mp_round(m));
    mpfr_round_nearest_away_begin(r);
    return mpfr_round_nearest_away_end(r, mpfr_fma(r, a, b, c, MPFR_RNDN));
}
int ref_rint(mpfr_ptr r, mpfr_srcptr a, sf64_rounding_mode m) {
    if (m != SF64_RNA)
        return mpfr_rint(r, a, mp_round(m));
    return mpfr_round(r, a);
}

void to_mpfr(mpfr_t out, sf256_t x) {
    uint64_t words[4] = {x.limb[0], x.limb[1], x.limb[2], x.limb[3] & 0x00000FFFFFFFFFFFULL};
    const unsigned field = static_cast<unsigned>((x.limb[3] >> 44) & 0x7FFFFu);
    if (field)
        words[3] |= uint64_t{1} << 44;
    mpz_t z;
    mpz_init(z);
    mpz_import(z, 4, -1, sizeof(uint64_t), 0, 0, words);
    mpfr_set_z(out, z, MPFR_RNDN);
    const int exponent = field ? int(field) - kBias : 1 - kBias;
    mpfr_mul_2si(out, out, exponent - kFrac, MPFR_RNDN);
    if (x.limb[3] >> 63)
        mpfr_neg(out, out, MPFR_RNDN);
    mpz_clear(z);
}

sf256_t from_mpfr(const mpfr_t x) {
    if (mpfr_zero_p(x))
        return sf256_from_bits(mpfr_signbit(x) ? 0x8000000000000000ULL : 0, 0, 0, 0);
    mpz_t z;
    mpz_init(z);
    mpfr_exp_t scale = mpfr_get_z_2exp(z, x);
    bool sign = mpz_sgn(z) < 0;
    if (sign)
        mpz_neg(z, z);
    const size_t n = mpz_sizeinbase(z, 2);
    const int exponent = int(scale) + int(n) - 1;
    if (n < 237)
        mpz_mul_2exp(z, z, 237 - n);
    uint64_t words[4]{};
    size_t count = 0;
    mpz_export(words, &count, -1, sizeof(uint64_t), 0, 0, z);
    mpz_clear(z);
    words[3] &= 0x00000FFFFFFFFFFFULL;
    words[3] |= uint64_t(exponent + kBias) << 44;
    if (sign)
        words[3] |= uint64_t{1} << 63;
    return sf256_from_bits(words[3], words[2], words[1], words[0]);
}

bool same(sf256_t a, sf256_t b) {
    for (unsigned i = 0; i < 4; ++i)
        if (a.limb[i] != b.limb[i])
            return false;
    return true;
}

sf256_t random_normal(std::mt19937_64& rng, bool positive = false) {
    sf256_t x{{rng(), rng(), rng(), rng() & 0x00000FFFFFFFFFFFULL}};
    const int exponent = int(rng() % 2001) - 1000;
    x.limb[3] |= uint64_t(exponent + kBias) << 44;
    if (!positive && (rng() & 1))
        x.limb[3] |= uint64_t{1} << 63;
    return x;
}

void fail(const char* op, sf64_rounding_mode mode, sf256_t got, sf256_t expected) {
    std::fprintf(
        stderr,
        "%s mode=%d got=%016llx %016llx %016llx %016llx expected=%016llx %016llx %016llx %016llx\n",
        op, int(mode), (unsigned long long)got.limb[3], (unsigned long long)got.limb[2],
        (unsigned long long)got.limb[1], (unsigned long long)got.limb[0],
        (unsigned long long)expected.limb[3], (unsigned long long)expected.limb[2],
        (unsigned long long)expected.limb[1], (unsigned long long)expected.limb[0]);
    std::abort();
}
} // namespace

int main() {
    std::mt19937_64 rng(0x5F256ULL);
    mpfr_t a, b, c, r;
    mpfr_inits2(800, a, b, c, (mpfr_ptr)0);
    mpfr_init2(r, 237);
    const sf64_rounding_mode modes[] = {SF64_RNE, SF64_RTZ, SF64_RUP, SF64_RDN, SF64_RNA};
    for (auto mode : modes)
        for (int i = 0; i < 1000; ++i) {
            sf256_t sa = random_normal(rng), sb = random_normal(rng), sc = random_normal(rng);
            to_mpfr(a, sa);
            to_mpfr(b, sb);
            to_mpfr(c, sc);
#define CHECK(name, expr, call)                                                                    \
    do {                                                                                           \
        expr;                                                                                      \
        sf256_t expected = from_mpfr(r);                                                           \
        sf256_t got = call;                                                                        \
        if (!same(got, expected))                                                                  \
            fail(name, mode, got, expected);                                                       \
    } while (0)
            CHECK("add", ref_add(r, a, b, mode), sf256_add_r(mode, sa, sb));
            CHECK("sub", ref_sub(r, a, b, mode), sf256_sub_r(mode, sa, sb));
            CHECK("mul", ref_mul(r, a, b, mode), sf256_mul_r(mode, sa, sb));
            CHECK("div", ref_div(r, a, b, mode), sf256_div_r(mode, sa, sb));
            CHECK("fma", ref_fma(r, a, b, c, mode), sf256_fma_r(mode, sa, sb, sc));
            CHECK("rint", ref_rint(r, a, mode), sf256_rint_r(mode, sa));
            if (mode == SF64_RNE)
                CHECK("remainder", mpfr_remainder(r, a, b, MPFR_RNDN), sf256_remainder(sa, sb));
            sf256_t sp = random_normal(rng, true);
            to_mpfr(a, sp);
            CHECK("sqrt", ref_sqrt(r, a, mode), sf256_sqrt_r(mode, sp));
#undef CHECK
        }
    mpfr_clears(a, b, c, r, (mpfr_ptr)0);
}
