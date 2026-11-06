# soft-fp64 bench harness

Microbench + regression gate for the public `sf64_*` entry points. No
external dependencies.

## Build

From the repo root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSOFT_FP64_BUILD_BENCH=ON
cmake --build build --target bench_soft_fp64
```

The binary lands at `build/bench/bench_soft_fp64`. `-DCMAKE_BUILD_TYPE=Release`
is mandatory — debug builds produce numbers that are not comparable to the
committed baseline.

## Run

```bash
# Pretty-print to stdout (default).
./build/bench/bench_soft_fp64

# Run only ops matching a substring.
./build/bench/bench_soft_fp64 --filter=log

# Longer per-op timing window (default 200ms).
./build/bench/bench_soft_fp64 --min-time-ms=1000

# Emit JSON for the regression gate. Schema is `soft-fp64.bench.v1`.
./build/bench/bench_soft_fp64 --json > /tmp/current.json
```

Flags:

| Flag                 | Default | Purpose                                             |
|----------------------|--------:|-----------------------------------------------------|
| `--min-time-ms=N`    |   `200` | Auto-scale iteration count to at least `N` ms/op.   |
| `--filter=substr`    |    none | Only run ops whose name contains `substr`.          |
| `--json`             |   false | Emit JSON to stdout instead of the human table.     |

## Regression gate (`compare.py`)

`compare.py` is a stdlib-only Python 3.10+ script. It compares a fresh JSON
run against the committed baseline. Exit status is 0 for a valid comparison
with no regressions, 1 for a regression, and 2 for invalid or incomplete input.

```bash
./build/bench/bench_soft_fp64 --json > /tmp/current.json
python3 bench/compare.py /tmp/current.json bench/baseline.json --threshold=0.05
```

Output is a Markdown table sorted by `abs(delta)`, with regressions and
improvements called out separately. The gate:

- matches ops by `name`;
- skips ops whose baseline `ns_per_op` is below a 2 ns noise floor
  (sub-2ns timings swing too much run-to-run to be a reliable signal);
- requires every baseline operation in the current run, including operations
  below the noise floor;
- rejects empty runs, malformed rows, duplicate names, and non-finite or
  non-positive `ns_per_op` values in either input;
- treats ops present in `current` but not in `baseline` as informational
  ("new op").

For a filtered local comparison, pass `--allow-partial`. It still requires
valid data and at least one operation shared with the baseline, reports omitted
operations, and labels success as a partial comparison. Do not use this option
in CI:

```bash
./build/bench/bench_soft_fp64 --filter=log --json > /tmp/log.json
python3 bench/compare.py /tmp/log.json bench/baseline.json --allow-partial
```

### CI usage

One-shot in CI:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSOFT_FP64_BUILD_BENCH=ON
cmake --build build --target bench_soft_fp64 -j
./build/bench/bench_soft_fp64 --json --min-time-ms=500 > current.json
python3 bench/compare.py current.json bench/baseline.json \
  --threshold=0.20 --cheap-op-absolute-budget=5.0
```

The PR-only `bench-regression` job runs on `macos-14`. It uses a 20% threshold
and permits an absolute increase of at most 5 ns for operations whose baseline
is below 15 ns. Operations at or above 15 ns use only the percentage gate;
operations below the 2 ns noise floor remain excluded from timing comparisons.

The script defaults to 5% with no absolute budget. `scripts/pre_push.sh` uses
10% with no absolute budget when a current snapshot is available; without one,
it prints instructions and skips benchmarking. A successful pre-push run alone
does not establish benchmark parity. Branch and tag CI do not run the PR-only
benchmark job, so release validation must also run the comparison explicitly.

## Updating `baseline.json`

Update a baseline only for a reviewed hardware change, an intentional
performance change, or a benchmark methodology change. Investigate regressions
before proposing a replacement; changing thresholds or replacing measurements
to hide a slowdown is not a fix.

The committed baseline records hardware, OS, compiler, build type, and sampling
method in its `environment` object. Its Apple M2 Max measurements are not a
claim that every `macos-14` runner is the same machine.

For a proposed refresh:

1. Use the same Release and fenv configuration as the gate. Close other heavy
   processes and keep the machine powered and thermally stable.
2. Collect three isolated runs at 1000 ms per operation, saving each original
   JSON file. Use a fresh directory so older measurements cannot enter the set.
3. Compare the runs to inspect their spread and compare each against the old
   baseline. Investigate inconsistent results before selecting a replacement.
4. For consistency with the committed method, select the minimum `ns_per_op`
   row for each operation across those three runs. This estimates the least
   interrupted observed execution; it does not estimate typical shared-runner
   latency. Keep the other fields from that same row.
5. Preserve the schema and add current `environment` metadata, including the
   fenv mode and the exact sampling method. Attach the original runs and a
   rationale to the review. Do not mix results from different machines or
   configurations into one baseline.

No baseline update is required for documentation or checker changes.

## Comparative bench (`bench_compare`)

`bench_compare` measures `sf64_*` against two vendored reference soft-float
libraries, giving an external baseline for relative-perf claims:

- [Berkeley SoftFloat 3e](http://www.jhauser.us/arithmetic/SoftFloat.html) —
  the canonical bit-exact IEEE-754 reference. C API. Core IEEE ops only
  (no transcendentals).
- [ckormanyos/soft_double](https://github.com/ckormanyos/soft_double) — a
  header-only C++ double implementation with a full `<cmath>`-style
  surface (sin/cos/exp/log/pow/sqrt/…).

Neither is pulled via `FetchContent` — that would gate every clean build
on network access. Instead a one-shot helper clones them into
`bench/external/`:

```bash
./bench/fetch_external.sh    # clones softfloat/ and soft_double/ if missing
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSOFT_FP64_BUILD_BENCH=ON
cmake --build build --target bench_compare -j
./build/bench/bench_compare --min-time-ms=500 --json > compare.json
```

The bench auto-detects which vendor libraries are present at configure
time (`bench/fetch_external.cmake`). If only one is vendored the bench
still builds, and the missing rows are simply absent from the output.

### Scope

| Op family      | sf64 | SoftFloat 3e | soft_double |
|----------------|:----:|:------------:|:-----------:|
| add/sub/mul/div/fma/sqrt | ✓ | ✓ | ✓ |
| sin/cos/tan/exp/log/pow/sinh/cosh/tanh | ✓ |   | ✓ |

SoftFloat 3e is deliberately IEEE-core-only upstream; `soft_double`
borrows its transcendentals from Boost.Math and is the only library with
a comparable transcendental surface. FMA on `soft_double` is emulated as
`a * b + c` (no native FMA entry point).

### CI

The `comparative-bench` job runs on macOS on `workflow_dispatch` and on
tag pushes (`v*`). It uploads `compare.json` as a workflow artifact; there is
no automatic GitHub Release attachment step. It is not a regression gate.
Record the vendor revisions and host configuration when using these measurements
for a performance comparison.
