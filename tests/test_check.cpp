// SPDX-License-Identifier: MIT
#include "test_check.h"

int main(int argc, char**) {
    int evaluated = 0;
    SF_CHECK(++evaluated == 1);
    if (evaluated != 1)
        return 2;
    SF_CHECK(argc == 1); // The negative CTest case passes an argument.
    return 0;
}
