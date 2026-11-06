// SPDX-License-Identifier: MIT
#pragma once

#include <cstdio>
#include <cstdlib>

namespace soft_fp_test {
[[noreturn]] inline void fail(const char* expression, const char* file, int line) {
    std::fprintf(stderr, "%s:%d: check failed: %s\n", file, line, expression);
    std::exit(EXIT_FAILURE);
}
} // namespace soft_fp_test

// Unlike assert, test expectations must still execute when NDEBUG is defined.
#define SF_CHECK(...)                                                                              \
    do {                                                                                           \
        if (!(__VA_ARGS__))                                                                        \
            soft_fp_test::fail(#__VA_ARGS__, __FILE__, __LINE__);                                  \
    } while (false)
