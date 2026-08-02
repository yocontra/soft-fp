#include "soft_fp256/soft_f256.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

namespace {
[[noreturn]] void fail() {
    __builtin_trap();
}

bool same(sf256_t a, sf256_t b) {
    for (unsigned i = 0; i < 4; ++i)
        if (a.limb[i] != b.limb[i])
            return false;
    return true;
}

sf256_t expand(const uint8_t* data, size_t size, size_t salt) {
    uint8_t bytes[sizeof(sf256_t)];
    for (size_t i = 0; i < sizeof(bytes); ++i)
        bytes[i] = data[(i + salt) % size] ^ static_cast<uint8_t>(salt * 37 + i * 13);
    sf256_t x;
    std::memcpy(&x, bytes, sizeof(x));
    return x;
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0)
        return 0;
    const sf256_t a = expand(data, size, 0), b = expand(data, size, 7), c = expand(data, size, 19);

    volatile uint64_t sink = 0;
    auto consume = [&](sf256_t x) { sink ^= x.limb[0] ^ x.limb[1] ^ x.limb[2] ^ x.limb[3]; };
    for (sf64_rounding_mode mode : {SF64_RNE, SF64_RTZ, SF64_RUP, SF64_RDN, SF64_RNA}) {
        consume(sf256_add_r(mode, a, b));
        consume(sf256_sub_r(mode, a, b));
        consume(sf256_mul_r(mode, a, b));
        consume(sf256_div_r(mode, a, b));
        consume(sf256_fma_r(mode, a, b, c));
        consume(sf256_sqrt_r(mode, a));
        consume(sf256_rint_r(mode, a));
    }
    (void)sf256_remainder(a, b);
    (void)sf256_to_f64(a);
    (void)sf256_to_i64(a);
    (void)sink;

    if (!sf256_isnan(a) && !sf256_isnan(b)) {
        if (!same(sf256_add(a, b), sf256_add(b, a)))
            fail();
        if (!same(sf256_mul(a, b), sf256_mul(b, a)))
            fail();
    }
    if (sf256_isfinite(a)) {
        const sf256_t zero = sf256_from_bits(0, 0, 0, 0);
        const sf256_t difference = sf256_sub(a, a);
        if (!sf256_eq(difference, zero))
            fail();
    }
    return 0;
}
