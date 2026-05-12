// OpenCL compatibility ABI smoke + FTZ edge tests.
//
// SPDX-License-Identifier: MIT

#include "soft_fp64/soft_f64.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef SOFT_FP64_FTZ_MODE
#error "SOFT_FP64_FTZ_MODE must be defined by CMake for test_ocl_mode"
#endif

namespace {

uint64_t bits(double x) {
    uint64_t b;
    std::memcpy(&b, &x, sizeof(b));
    return b;
}

double from_bits(uint64_t b) {
    double x;
    std::memcpy(&x, &b, sizeof(x));
    return x;
}

void check_bits(double got, uint64_t expect, const char* label) {
    const uint64_t got_bits = bits(got);
    if (got_bits != expect) {
        std::fprintf(stderr, "FAIL [%s]: got=0x%016llx expect=0x%016llx\n", label,
                     (unsigned long long)got_bits, (unsigned long long)expect);
        std::abort();
    }
}

void check_true(bool condition, const char* label) {
    if (!condition) {
        std::fprintf(stderr, "FAIL [%s]\n", label);
        std::abort();
    }
}

void check_close(double got, double expect, double tol, const char* label) {
    if (std::isnan(got) || std::fabs(got - expect) > tol) {
        std::fprintf(stderr, "FAIL [%s]: got=%.17g expect=%.17g tol=%.17g\n", label, got, expect,
                     tol);
        std::abort();
    }
}

} // namespace

int main() {
    constexpr uint64_t kPosZero = 0x0000000000000000ULL;
    constexpr uint64_t kNegZero = 0x8000000000000000ULL;
    constexpr uint64_t kDenormMin = 0x0000000000000001ULL;
    constexpr uint64_t kNegDenormMin = 0x8000000000000001ULL;
    constexpr uint64_t kOne = 0x3FF0000000000000ULL;
    constexpr uint64_t kTwo = 0x4000000000000000ULL;
    constexpr uint64_t kFour = 0x4010000000000000ULL;
    constexpr uint64_t kQuarter = 0x3FD0000000000000ULL;
    constexpr uint64_t kPosInf = 0x7FF0000000000000ULL;

    const double denorm = from_bits(kDenormMin);
    const double neg_denorm = from_bits(kNegDenormMin);

#if SOFT_FP64_FTZ_MODE
    check_bits(sf64_ocl_add(denorm, from_bits(kPosZero)), kPosZero,
               "FTZ input/output: add(denorm,+0)");
    check_bits(sf64_ocl_neg(denorm), kNegZero, "FTZ signed-zero output: neg(denorm)");
    check_true(sf64_ocl_fcmp(denorm, from_bits(kPosZero), 1) == 1, "FTZ compare: denorm == +0");

    int exp = 123;
    check_bits(sf64_ocl_frexp(denorm, &exp), kPosZero, "FTZ frexp mantissa");
    check_true(exp == 0, "FTZ frexp exponent");
#else
    check_bits(sf64_ocl_add(denorm, from_bits(kPosZero)), kDenormMin,
               "non-FTZ input/output: add(denorm,+0)");
    check_true(sf64_ocl_fcmp(denorm, from_bits(kPosZero), 1) == 0, "non-FTZ compare: denorm != +0");
#endif

    check_bits(sf64_native_recip(from_bits(kFour)), kQuarter, "native_recip(4)");
    check_bits(sf64_native_divide(from_bits(kOne), from_bits(kPosZero)), kPosInf,
               "native_divide(1,+0)");
    check_bits(sf64_native_sqrt(from_bits(kFour)), kTwo, "native_sqrt(4)");
    check_close(sf64_native_sin(0.5), std::sin(0.5), 1e-9, "native_sin(0.5)");
    check_close(sf64_native_cos(0.5), std::cos(0.5), 1e-9, "native_cos(0.5)");
    check_close(sf64_native_tan(0.5), std::tan(0.5), 1e-9, "native_tan(0.5)");
    check_close(sf64_native_exp(1.0), std::exp(1.0), 1e-6, "native_exp(1)");
    check_close(sf64_native_exp2(3.0), 8.0, 1e-5, "native_exp2(3)");
    check_close(sf64_native_log(from_bits(kFour)), std::log(4.0), 1e-6, "native_log(4)");
    check_close(sf64_native_log2(from_bits(kFour)), 2.0, 1e-5, "native_log2(4)");
    check_close(sf64_native_log10(100.0), 2.0, 1e-5, "native_log10(100)");
    check_close(sf64_native_powr(from_bits(kFour), from_bits(0x3FE0000000000000ULL)), 2.0, 1e-5,
                "native_powr(4,0.5)");

    std::printf("test_ocl_mode: PASSED (ftz=%d)\n", SOFT_FP64_FTZ_MODE);
    return 0;
}
