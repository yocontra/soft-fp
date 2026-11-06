// Umbrella-header, storage-wrapper, and representative ABI call smoke test.
// Complete symbol coverage lives in check_abi_manifest.sh; numerical
// correctness is checked by the operation-specific tests.
//
// SPDX-License-Identifier: MIT

#include "soft_fp64/soft_fp64.h"

#include "test_check.h"
#include <cstdint>
#include <cstring>

namespace mf = soft_fp64;

int main() {
    mf::float64_t zero;
    SF_CHECK(zero.bits() == 0);

    mf::float64_t one_point_five(0x3FF8000000000000ULL);
    double as_host;
    static_assert(sizeof(double) == sizeof(uint64_t));
    const uint64_t bits = one_point_five.bits();
    std::memcpy(&as_host, &bits, sizeof(double));
    SF_CHECK(as_host == 1.5);

    mf::vec<int, 3> v{{1, 2, 3}};
    SF_CHECK(v[0] == 1 && v[1] == 2 && v[2] == 3);
    SF_CHECK(v.size() == 3);

    // Observable calls retain external references even in optimized builds.
    SF_CHECK(sf64_add(1.0, 2.0) == 3.0);
    SF_CHECK(sf64_fcmp(1.0, 1.0, 1)); // LLVM ordered-equal predicate
    SF_CHECK(sf64_from_f32(1.5f) == 1.5);
    SF_CHECK(sf64_sqrt(4.0) == 2.0);
    SF_CHECK(sf64_floor(1.5) == 1.0);
    SF_CHECK(sf64_isnan(1.0) == 0);

    return 0;
}
