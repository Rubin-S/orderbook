# C++20 price-time order book

A single-symbol, single-threaded limit order book with deterministic CSV replay,
reproducible benchmarks, and correctness tests. Uses the C++20 standard library;
no third-party runtime or test dependencies.

The engine matches the best resting price first, FIFO within each price level,
and executes at the resting order's price. GTC remainders rest; IOC remainders are
discarded. Cancel/replace modifications reset priority. Prices are positive
integer ticks, with 32-bit quantities and 64-bit IDs and aggregated depth.

## Build and try it

Requires a C++20 compiler, CMake 3.25+, and Ninja. Run from the repository root:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./build/debug/orderbook_demo
./build/debug/orderbook_replay examples/session.csv
```

The replay example produces:

```text
TRADE,103,101,10000,10
TRADE,103,102,10010,2
SUMMARY,3,2,1
ASK,10010,3
```

Or submit your own commands through stdin:

```bash
./build/debug/orderbook_replay - <<'CSV'
ADD,101,SELL,10000,10,GTC
ADD,102,SELL,10010,5,GTC
ADD,103,BUY,10020,12,IOC
CSV
```

## Replay format

| Command | Fields |
| --- | --- |
| `ADD` | `ADD,id,BUY\|SELL,price,quantity,GTC\|IOC` |
| `MODIFY` | `MODIFY,id,BUY\|SELL,price,replacement_total_quantity` |
| `CANCEL` | `CANCEL,id` |

No header or CSV quoting. Blank lines and full-line `#` comments are ignored;
field whitespace and CRLF are accepted. Numeric fields use unsigned decimal
spelling with no signs or trailing characters. Prices must be 1–2147483647,
quantities 1–4294967295, and IDs 0–18446744073709551615.

Output contains trades and cancellation outcomes, then `SUMMARY,commands,trades,
resting_orders` and descending `BID,price,quantity` / ascending `ASK,price,quantity`
depth. Unknown cancels produce `NOT_FOUND`. Duplicate live IDs, missing modify
IDs, side changes, invalid values, and malformed commands produce a line-numbered
stderr error and exit 2. Earlier records remain; no success summary follows a
rejected input. Replay is sequential, without rollback or persistent state.
`--help` exits 0; file/open/read and argument errors exit 2.

`FillAndKill` in the C++ API means IOC, not all-or-none FOK. An ID can be reused
after it leaves the book. A modification preserves side/type but replaces total
quantity and resets filled history and FIFO priority. `TradeInfo::price_` is the
side's limit; `Trade::GetExecutionPrice()` is the execution price.

## Benchmarks

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
./build/release/orderbook_bench --operations 100000 --repetitions 3 --seed 42
```

Runs `resting`, `matching`, `modify`, and `mixed` workloads. CSV output reports
validated trade/state counts, aggregate throughput, and p50/p99 operation latency
from a separate pass. Defaults match the command above; operations range from
1 to 1000000, repetitions from 1 to 30, and seeds span uint64. Invalid, duplicate,
or missing options exit 2; `--help` exits 0.

[Measured baseline and methodology](docs/BENCHMARKS.md) explains precisely what
these small, bounded workloads measure. These results are machine-specific and
include command execution and result accounting.

## Verification and editors

Five CTest groups cover engine semantics and 12,000 randomized differential
operations, replay behavior, the executable fixture, benchmark workloads/options,
and a benchmark smoke run. Assertions in the original engine tests remain active
in Release; new tests use checks independent of `NDEBUG`.

```bash
cmake --preset clang-release
cmake --build --preset clang-release
ctest --preset clang-release

cmake --preset sanitize
cmake --build --preset sanitize
ctest --preset sanitize
```

The sanitizer preset requires Clang with ASan/UBSan runtimes. Local verification
passed all five groups in Debug, GCC Release, Clang Release, and Clang ASan/UBSan.
Leak detection was disabled only for the sandbox test process; the workflow uses
normal sanitizer settings and makes UBSan diagnostics fatal. A sixth test in
sanitizer builds intentionally triggers overflow to check this failure policy.
The GitHub Actions workflow is configured for both
Release compilers and sanitizers. The [first hosted run](https://github.com/Rubin-S/orderbook/actions/runs/38057029270)
passed all three configurations.

VS Code has project-local build, replay, test, benchmark, and GDB configurations.
`Ctrl+Shift+B` builds Debug; select demo or replay for F5. Extensions are
recommendations, not automatic installs. CLion can open `CMakeLists.txt`. Neovim
with clangd can use `--compile-commands-dir=build/debug` after configuration.

Generated files stay in ignored `build/` directories. Delete `build/` to reclaim
space and configure again before rebuilding.

## Design, scope, and provenance

- [Design and complexity](docs/DESIGN.md)
- [Contributing and test commands](CONTRIBUTING.md)
- [Résumé wording and contribution notes](docs/RESUME.md)

This educational engine omits networking, concurrency, persistence/recovery,
market data feeds, risk checks, fees, self-trade prevention, market/stop/iceberg
orders, and full rollback on allocation failure. Tick-size and symbol validation
belong upstream. It is not a production exchange.

The baseline engine, demo, and engine tests were imported from the supplied
`working_orderbook_cpp20.zip`. The original learning reference is
[Tzadiko/Orderbook](https://github.com/Tzadiko/Orderbook). Replay, benchmarks,
build/editor integration, CI configuration, and portfolio documentation were
added with AI assistance. Claims of personal implementation should reflect the
work you actually complete and can explain.
