#pragma once

#include <cstddef>
#include <cstdint>

namespace soft_fp::fp256_internal {

template <size_t N> struct Big {
    uint64_t v[N]{};
};

template <size_t N> bool zero(const Big<N>& a) {
    for (size_t i = 0; i < N; ++i)
        if (a.v[i] != 0)
            return false;
    return true;
}
template <size_t N> int compare(const Big<N>& a, const Big<N>& b) {
    for (size_t i = N; i-- > 0;) {
        if (a.v[i] < b.v[i])
            return -1;
        if (a.v[i] > b.v[i])
            return 1;
    }
    return 0;
}
template <size_t N> bool bit(const Big<N>& a, unsigned index) {
    return index < N * 64 && ((a.v[index / 64] >> (index % 64)) & 1u) != 0;
}
template <size_t N> void set_bit(Big<N>& a, unsigned index) {
    if (index < N * 64)
        a.v[index / 64] |= uint64_t{1} << (index % 64);
}
template <size_t N> int highest(const Big<N>& a) {
    for (size_t i = N; i-- > 0;) {
        if (a.v[i] != 0) {
            uint64_t word = a.v[i];
            unsigned position = 0;
            while (word >>= 1)
                ++position;
            return static_cast<int>(i * 64 + position);
        }
    }
    return -1;
}
template <size_t N> Big<N> add(const Big<N>& a, const Big<N>& b) {
    Big<N> r;
    uint64_t carry = 0;
    for (size_t i = 0; i < N; ++i) {
        const uint64_t x = a.v[i] + carry;
        const uint64_t c1 = x < a.v[i];
        r.v[i] = x + b.v[i];
        carry = c1 | static_cast<uint64_t>(r.v[i] < x);
    }
    return r;
}
template <size_t N> Big<N> sub(const Big<N>& a, const Big<N>& b) {
    Big<N> r;
    uint64_t borrow = 0;
    for (size_t i = 0; i < N; ++i) {
        const uint64_t x = a.v[i] - borrow;
        const uint64_t b1 = a.v[i] < borrow;
        r.v[i] = x - b.v[i];
        borrow = b1 | static_cast<uint64_t>(x < b.v[i]);
    }
    return r;
}
template <size_t N> Big<N> shl(const Big<N>& a, unsigned shift) {
    Big<N> r;
    if (shift >= N * 64)
        return r;
    const unsigned words = shift / 64, bits = shift % 64;
    for (size_t i = N; i-- > words;) {
        r.v[i] = a.v[i - words] << bits;
        if (bits != 0 && i > words)
            r.v[i] |= a.v[i - words - 1] >> (64 - bits);
    }
    return r;
}
template <size_t N> Big<N> shr(const Big<N>& a, unsigned shift) {
    Big<N> r;
    if (shift >= N * 64)
        return r;
    const unsigned words = shift / 64, bits = shift % 64;
    for (size_t i = 0; i + words < N; ++i) {
        r.v[i] = a.v[i + words] >> bits;
        if (bits != 0 && i + words + 1 < N)
            r.v[i] |= a.v[i + words + 1] << (64 - bits);
    }
    return r;
}
template <size_t N> bool any_low(const Big<N>& a, unsigned count) {
    if (count >= N * 64)
        return !zero(a);
    const unsigned words = count / 64, bits = count % 64;
    for (unsigned i = 0; i < words; ++i)
        if (a.v[i] != 0)
            return true;
    return bits != 0 && (a.v[words] & ((uint64_t{1} << bits) - 1)) != 0;
}
template <size_t N> Big<N> shr_jam(const Big<N>& a, unsigned shift) {
    if (shift == 0)
        return a;
    Big<N> r = shr(a, shift);
    if (any_low(a, shift))
        r.v[0] |= 1;
    return r;
}
template <size_t To, size_t From> Big<To> widen(const Big<From>& a) {
    Big<To> r;
    for (size_t i = 0; i < To && i < From; ++i)
        r.v[i] = a.v[i];
    return r;
}
template <size_t A, size_t B> Big<A + B> multiply(const Big<A>& a, const Big<B>& b) {
    // Base-2^32 schoolbook product. A 32x32 product plus one output digit
    // and carry fits exactly in uint64_t, avoiding compiler-native 128-bit
    // types while retaining O(A*B) complexity.
    uint32_t ad[A * 2]{}, bd[B * 2]{}, out[(A + B) * 2]{};
    for (size_t i = 0; i < A; ++i) {
        ad[2 * i] = static_cast<uint32_t>(a.v[i]);
        ad[2 * i + 1] = static_cast<uint32_t>(a.v[i] >> 32);
    }
    for (size_t i = 0; i < B; ++i) {
        bd[2 * i] = static_cast<uint32_t>(b.v[i]);
        bd[2 * i + 1] = static_cast<uint32_t>(b.v[i] >> 32);
    }
    for (size_t i = 0; i < A * 2; ++i) {
        uint64_t carry = 0;
        for (size_t j = 0; j < B * 2; ++j) {
            const uint64_t current = static_cast<uint64_t>(ad[i]) * bd[j] + out[i + j] + carry;
            out[i + j] = static_cast<uint32_t>(current);
            carry = current >> 32;
        }
        size_t k = i + B * 2;
        while (carry != 0 && k < (A + B) * 2) {
            const uint64_t current = static_cast<uint64_t>(out[k]) + carry;
            out[k++] = static_cast<uint32_t>(current);
            carry = current >> 32;
        }
    }
    Big<A + B> r;
    for (size_t i = 0; i < A + B; ++i)
        r.v[i] = static_cast<uint64_t>(out[2 * i]) | (static_cast<uint64_t>(out[2 * i + 1]) << 32);
    return r;
}

} // namespace soft_fp::fp256_internal
