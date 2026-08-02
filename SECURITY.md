# Security

soft-fp is a pure-compute math library. Arithmetic paths perform no I/O or
heap allocation. The default exception mode shared by all three formats uses
thread-local state; caller-owned explicit state is available for runtimes
that cannot use TLS. Inputs are untrusted bit patterns, so memory safety,
undefined behavior, denial-of-service through pathological workloads, and
incorrect results at a trust boundary are security issues.

CI runs sanitizer-backed fuzz targets and independent TestFloat/MPFR oracle
tests. Exhaustive `f32 ↔ f64` round-trip is a separately selectable long test
(`SOFT_FP64_BUILD_EXHAUSTIVE=ON`) and runs in scheduled CI.

If you believe you have found a vulnerability (undefined behavior on
specific inputs, a buffer overflow in a helper, a signed overflow trip
under UBSan), report it by opening a GitHub issue or emailing
**yo@contra.io**. Bug reports from fuzzer corpora are welcome.

## Supported Versions

The current 2.x release line and `main` receive security fixes. The legacy
1.x line is unsupported.
