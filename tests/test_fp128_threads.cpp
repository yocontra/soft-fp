#include "soft_fp128/soft_f128.h"

#include <atomic>
#include <cassert>
#include <thread>

int main() {
    std::atomic<bool> start{false};
    std::atomic<int> failures{0};
    const sf128_t zero = sf128_from_bits(0, 0);
    const sf128_t one = sf128_from_i32(1);
    auto worker = [&](bool invalid) {
        while (!start.load(std::memory_order_acquire))
            ;
        for (int i = 0; i < 10000; ++i) {
            sf64_fe_clear(SF64_FE_INVALID | SF64_FE_DIVBYZERO | SF64_FE_OVERFLOW |
                          SF64_FE_UNDERFLOW | SF64_FE_INEXACT);
            if (invalid)
                (void)sf128_div(zero, zero);
            else
                (void)sf128_div(one, zero);
            const unsigned expected = invalid ? SF64_FE_INVALID : SF64_FE_DIVBYZERO;
            if (sf64_fe_getall() != expected)
                failures.fetch_add(1, std::memory_order_relaxed);
        }
    };
    std::thread a(worker, true), b(worker, false);
    start.store(true, std::memory_order_release);
    a.join();
    b.join();
    assert(failures.load() == 0);
}
