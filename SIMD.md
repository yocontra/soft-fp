# SIMD and SIMT integration

`soft-fp` 2.0 provides a portable lane API for applying every scalar operation
to fixed-size packs. It is designed for CPU SIMD dispatch, GPU/SIMT adapters,
and callers that want one data layout across binary64, binary128, and
binary256. The helpers live in `soft_fp/simd.h` and require C++17.

The lane API is an integration surface, not a new floating-point contract.
Each lane calls the corresponding scalar function and therefore has exactly
the same result bits, rounding behavior, NaN policy, and exception behavior.

## Guarantees

- `soft_fp::simd::pack<T, N>` is an allocation-free aggregate with contiguous
  lane storage and no hidden padding beyond that required by `T`.
- Any positive compile-time lane count is accepted. Common hardware widths
  such as 2, 4, 8, and 16 do not require separate library builds.
- For the soft-fp POD value types, `load`, `store`, and unary, binary, ternary,
  and indexed `transform` operations do not allocate, lock, or access host
  floating-point arithmetic themselves. A transform's exception specification
  follows its callable.
- `fp128_words<N>` and `fp256_words<N>` plus `split`/`combine` convert between
  the public array-of-structures values and word-plane structure-of-arrays
  layouts. Word planes can be loaded directly into an ISA-specific integer
  vector type without depending on host byte order.
- Indexed transforms make a distinct `sf64_fe_state_t` available to each lane.
  Exception flags therefore never need a shared reduction unless the caller
  explicitly wants aggregate flags.
- Pack results are lane-order deterministic. Divergent special cases affect
  performance only; they cannot change another lane's value.

These guarantees are covered by `test_simd` in the default, disabled-fenv, and
explicit-fenv CI configurations.

## Basic use

```cpp
#include <soft_fp/simd.h>

namespace simd = soft_fp::simd;

simd::pack<double, 8> a = simd::load<double, 8>(input_a);
simd::pack<double, 8> b = simd::load<double, 8>(input_b);

auto sum = simd::transform(a, b, [](double x, double y) {
    return sf64_add(x, y);
});

simd::store(output, sum);
```

Use an indexed transform for independent IEEE-754 flags:

```cpp
simd::pack<sf64_fe_state_t, 8> state{};
auto quotient = simd::transform_indexed(
    a, b, [&state](std::size_t lane, double x, double y) {
        return sf64_div_r_ex(SF64_RNE, x, y, &state[lane]);
    });
```

Load every input pack before storing output when input and output ranges may
overlap. Exact in-place operation is safe under that pattern because packs own
their lane values.

## Wider formats and word planes

The public `sf128_t` and `sf256_t` types are stable, trivially copyable word
structures. A normal transform is sufficient for portable lane-wise work:

```cpp
simd::pack<sf128_t, 4> x = /* ... */;
auto square = simd::transform(x, x, sf128_mul);
```

An ISA adapter normally wants one vector register per word rather than one
struct per lane. `split` transposes a pack into that layout and `combine`
reverses the operation:

```cpp
auto words = simd::split(x); // words.lo and words.hi are uint64_t packs
auto roundtrip = simd::combine(words);
```

The binary256 equivalent exposes `limb0` through `limb3`, always least-
significant word first.

## Exception-state configuration

For vector loops, build with one of these modes:

- `-DSOFT_FP64_FENV=explicit` keeps flags and uses caller-owned per-lane
  states through `_ex` operations. This is the recommended correctness mode.
- `-DSOFT_FP64_FENV=disabled` removes flag writes when flags are not needed and
  gives optimizers the least stateful kernel.
- The default `tls` mode is thread-safe, but its sticky per-thread OR is an
  observable loop-carried side effect. It is suitable when a whole pack should
  contribute to one aggregate flag word, but it can inhibit auto-vectorization.

The binary128 implementation encapsulates Berkeley SoftFloat state in hidden
thread-local storage and restores it around each call, including `_ex` calls.
This preserves lane and thread correctness but can prevent a compiler from
forming native vector instructions across binary128 calls. Use word planes to
connect an ISA-specific binary128 kernel when native vector execution is
required.

## What “SIMD-friendly” does and does not mean

The public data model, lane traversal, state ownership, and word layouts are
SIMD-friendly and ISA-neutral. The library does not claim that a compiler will
turn every exact scalar algorithm into branchless hardware SIMD. Division,
square root, remainder, gradual underflow, and NaN handling contain unavoidable
data-dependent control flow; the portable 2.0 implementation may scalarize
those lanes. Link-time optimization and a disabled or explicit fenv improve
the compiler's options, but generated code must be measured on the target.

Native AVX2/AVX-512, Arm SVE, RISC-V V, and GPU-specific kernels can implement
the same pack/word-plane contract without changing application data layouts or
the scalar ABI. Such kernels must pass the same TestFloat and MPFR oracles lane
by lane before they can replace the portable implementation.
