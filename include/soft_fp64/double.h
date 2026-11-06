#pragma once

// Binary64 bit-pattern storage for compiler adapters.
//
// SPDX-License-Identifier: MIT

#include "defines.h"

#include <cstdint>

namespace soft_fp64 {

// Stores the IEEE-754 encoding as an integer; arithmetic uses the sf64_* ABI.
class float64_t {
  public:
    float64_t() = default;
    explicit constexpr float64_t(uint64_t bits) noexcept : data_(bits) {}

    constexpr uint64_t bits() const noexcept { return data_; }

  private:
    uint64_t data_{0};
};

static_assert(sizeof(float64_t) == 8, "float64_t must be exactly 64 bits");

} // namespace soft_fp64
