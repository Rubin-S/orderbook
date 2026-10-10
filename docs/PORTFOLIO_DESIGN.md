# Order book portfolio upgrade — proposed design

Date: 10 October 2026

Project: `/home/rubins/Developer/trading/orderbook`

Status: proposed for review. The features below have not been implemented yet.

## Purpose and constraints

Prepare the existing C++20 project for a public GitHub repository and an accurate
résumé entry. Give reviewers a useful demonstration, reproducible performance
measurements, and clear explanations of the engineering decisions.

Keep the supplied engine's matching semantics and public API. Retain the existing
Git history and the recovery stash containing the earlier local implementation.
Use the C++ standard library and existing CMake/Ninja toolchain; do not download
product dependencies. Keep generated files inside ignored build directories and
remove setup scripts, staging copies, and test builds after verification. The
user will publish the repository and update their résumé themselves.

## Approaches considered

1. **Documentation and CI only:** smallest change, but gives reviewers little
   beyond the supplied engine and tests.
2. **Replay, benchmarks, CI, and design documentation — recommended:** provides
   a usable original extension and measurable engineering work while keeping the
   project small enough to understand and explain.
3. **Exchange service with networking and concurrency:** substantially increases
   the scope and distracts from demonstrating a correct single-symbol book.

The recommended approach follows the improvements discussed in this chat.

## Replay tool

Add an `orderbook_replay` executable accepting one CSV file or `-` for standard
input. Replay commands in file order through the existing engine. Prices remain
integer ticks; quantities and IDs use the existing integer types.

The deliberately small CSV format has no quoting or header row:

```text
# Full-line comments and blank lines are ignored.
ADD,101,SELL,10000,10,GTC
ADD,102,SELL,10010,5,GTC
MODIFY,102,SELL,10010,7
CANCEL,102
ADD,103,BUY,10020,12,IOC
```

`ADD` takes ID, side (`BUY`/`SELL`), positive price, positive quantity, and type
(`GTC`/`IOC`). `MODIFY` takes ID, side, positive price, and replacement total
quantity. `CANCEL` takes one ID. Leading/trailing field whitespace is permitted;
unknown tokens, extra/missing fields, partial numeric parses, signed unsigned
values, and integer overflow are rejected. Existing engine validation continues
to govern duplicate live IDs, side changes, and modification of missing IDs.

Write deterministic trade records, cancellation results, a final command/trade/
resting-order summary, and sorted bid/ask depth to standard output. Report input
errors to standard error with the line number and return a nonzero exit status.
Earlier valid commands may already have produced output; replay is not a
transaction and does not persist state. Canceling an unknown ID is reported as
not found, consistent with the engine's existing return value.

Separate parsing/replay support from the CLI entry point so tests exercise real
behavior through in-memory streams. Include a small example input and its exact
expected output, plus a CTest check of the executable against that fixture.

## Reproducible benchmarks

Add an `orderbook_bench` executable with validated operation-count, repetition,
and seed options. Generate deterministic workloads before timing. Exercise
resting insertion/cancellation, crossing matches, cancel/replace modification,
and a bounded mix of operations; all generated commands must be valid.

Measure aggregate throughput in a pass without per-operation clock calls. Measure
operation latency in a separate pass and report p50 and p99 in nanoseconds, with
the sample count and clock-overhead limitation. Exclude input generation, file
parsing, console output, and book destruction from measured command execution.
Use `std::chrono::steady_clock`; consume results and verify final counts/state so
the workload cannot silently become a no-op.

Document exact commands, seed, workload, operation counts, compiler/version,
build configuration/flags, CPU, and measurement method. Record actual local
Release measurements after implementation, clearly labeled as machine-specific
baseline results. Do not claim production latency, competitive HFT performance,
or an optimization speedup without a measured comparison. Establishing this
baseline precedes any later optimization of the matching engine.

## CI and verification

Add a GitHub Actions workflow for pushes and pull requests. Build and run tests
with GCC and Clang in Release, plus a Clang ASan/UBSan configuration. Grant only
read access to repository contents. Run replay fixture checks and a small
benchmark smoke run; timing values are not pass/fail thresholds.

Keep the existing 12,000-operation randomized differential suite. Add meaningful
tests for replay ordering, partial fills, cancellation/modification, input errors,
integer boundaries, and exact demo output. Test benchmark argument validation,
deterministic workloads, operation accounting, and valid metric output without
asserting particular durations. Confirm newly added behavior fails its tests
before implementation, then run the full suite in all configurations.

Local sanitizer verification may need leak detection disabled inside the tool
sandbox; record that limitation. CI should use normal sanitizer behavior.

## Documentation and repository presentation

Rewrite the README around implemented features, a short replay example, build/
test/benchmark commands, and the single-symbol, single-threaded scope. Explain
the price-level maps, FIFO lists, ID index, iterator lifetime, snapshot ownership,
and operation complexity in a concise design document. Document limitations and
trade-offs instead of implying this is a production exchange.

Credit the supplied archive and the original Tzadiko/Orderbook reference. Add a
short contribution guide and résumé wording that describes demonstrated features
and lets the user claim only work they personally implemented or extended. Do
not invent authorship, measurements, coverage percentages, or CI results.

Keep the existing editor setup working with the new executables. Avoid unrelated
restructuring, extra services, dashboards, third-party dependencies, and changes
to global editor settings. Do not select a new software license on the user's
behalf or publish anything externally.

## Completion criteria

- Replay input produces the documented trades, final state, and error behavior.
- Benchmarks run reproducibly and report precisely defined metrics.
- Existing and new tests pass under Debug, Release, and Clang sanitizers.
- CI configuration covers both compilers; hosted CI remains unverified until push.
- Documentation matches the implemented project and measured evidence.
- The Git worktree is reviewed, earlier edits remain recoverable, and temporary
  artifacts/builds are removed.

Implementation starts after this spec and the subsequent implementation plan
have been reviewed, as required by the active project workflow skill.
