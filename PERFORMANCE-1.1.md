# Version 1.1 performance measurements

These measurements were recorded for the 2026-04-24 release and preserved from
its changelog. They are historical observations, not current performance
claims or portable guarantees. Raw run artifacts were not linked in that
release record.

## Recorded configuration

The release notes identify Apple M2 Max local hardware, a Release build, and
`--min-time-ms=500`, compared with the version 1.0 baseline. The 1.1 TLS column
became the version 1.1 committed baseline. CI used a different Apple M-series
runner, so these timings should not be compared across machines as a regression
measurement. Values are nanoseconds per operation unless only a delta was
recorded. Empty cells mean the release notes did not provide that measurement.

| op       | 1.0    | 1.1 disabled | 1.1 tls (committed baseline) |
|----------|--------|--------------|------------------------------|
| `add`    | 10.89  | 11.50 (+6%)  | 12.45 (+14%)                 |
| `sub`    | 11.14  | 11.32 (+2%)  | 13.48 (+21%)                 |
| `mul`    | 5.19   | 5.56 (+7%)   | 6.47 (+25%)                  |
| `div`    | 16.87  | 18.25 (+8%)  | 18.69 (+11%)                 |
| `fma`    | 17.93  | 19.00 (+6%)  | 20.00 (+12%)                 |
| `to_i32` | 4.89   | 2.68 (−45%)  | 7.35 (+50%)                  |
| `pow`    | 1324.2 | 1685.7 (+27%)| 1722.1 (+30%)                |
| `log`    | 472.1  | 368.1 (−22%) | 380.2 (−19%)                 |
| `exp`    | 256.5  | 216.0 (−16%) | 221.9 (−13%)                 |
| `exp2`   | 274.6  | 211.7 (−23%) | 220.8 (−20%)                 |
| `cbrt`   | 1109.0 | 879.5 (−21%) | 898.0 (−19%)                 |
| `sin`    | 446.5  | 404.1 (−9%)  | 405.8 (−9%)                  |
| `cos`    | 524.7  | 447.6 (−15%) | 449.7 (−14%)                 |
| `tan`    | —      | −18% vs 1.0  | −17% vs 1.0                  |
| `sinh` / `cosh` / `tanh` | | −14 to −15% | −14 to −15%        |
| `asinh` / `acosh` / `atanh` | | −16 to −20% | −13 to −16%     |

## Changes measured

Version 1.1 added exception flags and non-RNE rounding modes. SLEEF's internal
double-double helpers switched to inline RNE arithmetic, with a local flag
accumulator flushed at the public entry point. Several transcendental timings
improved; `pow` and some inexpensive core operations regressed in the recorded
results above. The notes attributed some cost to TLS access and remaining
cross-translation-unit calls, but the table alone does not isolate those costs.

The earlier narrative described 2.68 ns as a version 1.0 `to_i32` baseline;
that conflicts with its table, where 2.68 ns is the 1.1 disabled-fenv result and
4.89 ns is the 1.0 result. The original table is preserved here without that
incorrect comparison.

## Regression policy introduced in 1.1

The release refreshed the baseline to include the intended fenv and rounding
costs. It added an optional absolute budget for operations whose baseline is
below 15 ns: CI selected 5 ns alongside its 20% relative threshold. The option
is disabled by default in the comparison script. See [bench/README.md](bench/README.md)
for the current command, input requirements, and baseline-update procedure.

The release notes also recorded `floor` measurements with disabled fenv at
`--min-time-ms=2000`: three samples were within +10% of the version 1.0 baseline.
As with the table, that observation applies only to the recorded configuration.
