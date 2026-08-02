#include "soft_fp128/soft_f128.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

namespace {
[[noreturn]] void fail() {
    __builtin_trap();
}

bool same(sf128_t a, sf128_t b) {
    return a.hi == b.hi && a.lo == b.lo;
}

sf128_t expand(const uint8_t* data, size_t size, size_t salt) {
    uint8_t bytes[sizeof(sf128_t)];
    for (size_t i = 0; i < sizeof(bytes); ++i)
        bytes[i] = data[(i + salt) % size] ^ static_cast<uint8_t>(salt * 29 + i * 17);
    sf128_t x;
    std::memcpy(&x, bytes, sizeof(x));
    return x;
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0)
        return 0;
    const sf128_t a = expand(data, size, 0), b = expand(data, size, 5), c = expand(data, size, 11);

    volatile uint64_t sink = 0;
    auto consume = [&](sf128_t x) { sink ^= x.hi ^ x.lo; };
    for (sf64_rounding_mode mode : {SF64_RNE, SF64_RTZ, SF64_RUP, SF64_RDN, SF64_RNA}) {
        consume(sf128_add_r(mode, a, b));
        consume(sf128_sub_r(mode, a, b));
        consume(sf128_mul_r(mode, a, b));
        consume(sf128_div_r(mode, a, b));
        consume(sf128_fma_r(mode, a, b, c));
        consume(sf128_sqrt_r(mode, a));
        consume(sf128_rint_r(mode, a));
    }
    (void)sf128_remainder(a, b);
    (void)sf128_to_f64(a);
    (void)sf128_to_i64(a);
    (void)sink;

    if (!sf128_isnan(a) && !sf128_isnan(b)) {
        if (!same(sf128_add(a, b), sf128_add(b, a)))
            fail();
        if (!same(sf128_mul(a, b), sf128_mul(b, a)))
            fail();
    }
    if (sf128_isfinite(a)) {
        const sf128_t zero = sf128_from_bits(0, 0);
        const sf128_t difference = sf128_sub(a, a);
        if (!sf128_eq(difference, zero))
            fail();
    }
    return 0;
}
