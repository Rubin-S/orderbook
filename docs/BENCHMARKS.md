# Benchmark baseline

## Reproduce

From the repository root:

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
./build/release/orderbook_bench --operations 100000 --repetitions 3 --seed 42
```

Recorded on 10 October 2026, against revision
`c3372a210ab08797b2e141d044564432baf44676` (the benchmark implementation is
unchanged from `40c90f8`). This is a local laptop baseline, not a comparison with
another engine or an optimization speedup.

| Setting | Recorded value |
| --- | --- |
| CPU | 13th Gen Intel Core i7-1360P, 12 cores / 16 logical CPUs |
| OS | Fedora Linux 44, x86_64; Linux 7.2.5-200.fc44.x86_64 |
| Compiler | GCC 16.2.1 20260819 (Red Hat 16.2.1-2) |
| Build | CMake 4.3.0, Ninja 1.13.2, Release, C++20 compile-feature requirement |
| Flags | `-O3 -DNDEBUG -Wall -Wextra -Wpedantic`; no custom ISA/optimization flags |
| Work | 100000 commands per scenario per pass, three repetitions, seed 42 |
| Scheduling | Ordinary local process; no affinity, frequency lock, or isolated CPU |

## Method and limits

Workloads are generated before timing using `mt19937_64` and modulo mapping to
positive prices 10000–10999 (the second level or modified price can be one tick
higher). The same count/seed yields the same commands across standard-library
implementations. IDs are unique per generated order; complete cycles empty the
book. Truncation at any command boundary is valid and checked.

| Scenario | Repeating pattern | Maximum live orders |
| --- | --- | --- |
| `resting` | Add 32 buys of quantity 3 at one price, then cancel all 32 | 32 |
| `matching` | Add one sell of 10, then buy 10 IOC at that price | 1 |
| `modify` | Add buy of 10, replace at price + 1 with quantity 7, cancel | 1 |
| `mixed` | Sell 10; sell 5 one tick higher; replace second with 7; buy 12 IOC; cancel remaining 5; unmatched sell IOC | 2 |

Each measurement warms up a separate book with the entire workload, then uses
fresh books for two passes. The throughput pass times all commands with only
one start/end clock pair. The latency pass takes one start/end pair per command,
with sample storage allocated beforehand and sorting afterward. Both passes
include engine dispatch, allocations, returned trade destruction, and result
accounting. Neither times generation, CSV parsing, printing, final state
validation, or book destruction. Timing uses `steady_clock` in nanoseconds.

Both passes (and warmup) verify trades, executed quantity, resting order count,
and sorted depth against expectations derived from cycle arithmetic, without
calling the engine to produce those expectations. CTest smoke runs impose no
timing threshold.

Percentiles use nearest rank: sort N samples and select `ceil(p*N/100)`, counted
from one. One sample supplies both p50 and p99. Samples include clock-call and
loop/dispatch overhead, with no subtraction or calibration; they are not pure
matching-engine instruction times. If aggregate elapsed time rounds to zero,
throughput is reported as zero rather than divided by zero.

These shallow, bounded, mostly cache-hot books do not represent deep market
books, adversarial distributions, network/gateway latency, or an exchange feed.
There is no CPU pinning, controlled background load, or turbo/power control.
The single-threaded cycles and laptop scheduling can produce substantial run
variation. Compare only like-for-like commands, settings, compiler, and hardware.

At count 100000, partial cycles intentionally leave 32 resting buys in `resting`,
one buy in `modify`, and one ask in `mixed`. Mixed completes 16667 sweeps,
producing 33334 trades and 200004 executed units. These counts follow the command
pattern, even though that final mixed cycle is incomplete.

## Recorded rows

Operations/second is command throughput from the aggregate pass. p50/p99 and
sample counts refer to the separate latency pass. All three repetitions are
retained below, including variation.

```csv
# compiler=GCC 16.2.1 20260819 (Red Hat 16.2.1-2)
# build=Release
# seed=42
scenario,repetition,operations,trades,executed_quantity,resting_orders,elapsed_ns,operations_per_second,p50_ns,p99_ns,latency_samples
resting,1,100000,0,0,32,5761978,17355151.30,54,101,100000
resting,2,100000,0,0,32,4783847,20903678.57,50,105,100000
resting,3,100000,0,0,32,4555690,21950571.70,56,86,100000
matching,1,100000,50000,500000,0,5683748,17594024.22,61,88,100000
matching,2,100000,50000,500000,0,5589811,17889692.51,67,94,100000
matching,3,100000,50000,500000,0,5326617,18773641.88,67,93,100000
modify,1,100000,0,0,1,4801016,20828924.54,47,79,100000
modify,2,100000,0,0,1,4654351,21485272.60,47,78,100000
modify,3,100000,0,0,1,4990074,20039782.98,48,80,100000
mixed,1,100000,33334,200004,1,4926326,20299103.23,41,111,100000
mixed,2,100000,33334,200004,1,5128497,19498890.22,46,113,100000
mixed,3,100000,33334,200004,1,5113116,19557545.73,45,103,100000
```
