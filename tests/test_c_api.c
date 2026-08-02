/* Compile-and-link smoke test for the installed C ABI. */

#include "soft_fp/soft_fp.h"

#include <stdint.h>

int main(void) {
    sf64_fe_state_t state = {0u};
    const double sum = sf64_add_ex(1.0, 2.0, &state);
    const int32_t converted = sf64_to_i32(sum);
    sf64_fe_raise_ex(&state, SF64_FE_INEXACT);
    int wide_ok = 1;
#if SOFT_FP_HAS_FP128
    sf128_t q = sf128_add(sf128_from_i32(1), sf128_from_i32(2));
    wide_ok = wide_ok && sf128_to_i32(q) == 3;
#endif
#if SOFT_FP_HAS_FP256
    sf256_t o = sf256_add(sf256_from_i64(1), sf256_from_i64(2));
    wide_ok = wide_ok && sf256_to_i64(o) == 3;
#endif
    return converted == 3 && wide_ok && sf64_fe_test_ex(&state, SF64_FE_INEXACT) ? 0 : 1;
}
