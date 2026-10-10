# Order book portfolio upgrade implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans
> (recommended here) or superpowers:subagent-driven-development to implement this
> plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add deterministic replay, reproducible benchmarks, compiler/sanitizer CI,
and accurate GitHub/résumé documentation to the existing order book.

**Architecture:** Preserve the header-only engine and its API. A small support
library parses and executes replay commands; a separate benchmark module creates
bounded workloads and measures execution through the same command interface.
Thin executable entry points handle CLI arguments and exit codes.

**Tech stack:** C++20 standard library, CMake, Ninja, CTest, GCC, Clang, GDB,
ASan/UBSan, GitHub Actions. CMake presets require version 3.25 or newer.

**Spec:** `docs/PORTFOLIO_DESIGN.md`, approved by the user on 10 October 2026.

## Global constraints

- Keep the supplied engine's matching semantics and public API.
- Retain the existing Git history and recovery stash containing earlier edits.
- Use the C++ standard library and existing CMake/Ninja toolchain; do not download
  product dependencies.
- Keep generated files in ignored build directories; remove temporary setup
  scripts, staging copies, and test builds after verification.
- Preserve single-symbol, single-threaded behavior; add no network services.
- The user will publish the repository and update their résumé themselves.
- Do not invent authorship, measurements, coverage percentages, or CI results.
- Do not select a new software license or change global editor settings.

## Review focus

- Malformed numeric input, signs, overflow, CRLF, whitespace, and extra fields:
  reject invalid commands with an accurate line number (Task 1).
- Replay errors after earlier successful commands: retain emitted records, exit
  unsuccessfully, and omit a misleading success summary (Task 1).
- Very small or incomplete workload cycles: exactly the requested number of
  commands, no invalid cancel/modify operations, and correct final state (Task 2).
- Timing resolution and argument/resource boundaries: reject invalid options,
  never divide by zero, and define percentiles for small sample sets (Task 2).
- Build modes and working directories: assertions remain active in Release,
  fixture paths work outside the source directory, and CI exercises real
  executables and tests (Tasks 1 and 3).

## File responsibilities

- `tools/replay.hpp`, `tools/replay.cpp`: command records, strict parsing,
  execution, stream replay, summaries, and line-aware errors.
- `tools/replay_main.cpp`: file/stdin selection, help, and exit codes.
- `examples/session.csv`, `examples/session.expected`: hand-checked demo fixture.
- `tests/replay_tests.cpp`, `tests/check_replay.cmake`: behavior and CLI checks.
- `benchmarks/benchmark.hpp`, `benchmarks/benchmark.cpp`: options, deterministic
  workload generation, state verification, timing, and metric output.
- `benchmarks/benchmark_main.cpp`, `tests/benchmark_tests.cpp`: CLI and tests.
- `.github/workflows/ci.yml`: GCC/Clang Release and Clang sanitizer jobs.
- `README.md`, `docs/DESIGN.md`, `docs/BENCHMARKS.md`, `docs/RESUME.md`,
  `CONTRIBUTING.md`: public usage, rationale, measurements, and contribution notes.
- `CMakeLists.txt`, `CMakePresets.json`, `.vscode/`: build/test/editor integration.

### Task 1: Replay support and executable

**Files:** Create `tools/replay.hpp`, `tools/replay.cpp`, `tools/replay_main.cpp`,
`examples/session.csv`, `examples/session.expected`, `tests/replay_tests.cpp`,
and `tests/check_replay.cmake`; modify `CMakeLists.txt`.

**Interfaces:** In namespace `orderbook_tools`, produce `Action`, `Command`,
`CommandResult` (trades and optional cancellation result), `ReplaySummary`
(command/trade/executed-quantity/resting-order counts), and `ReplayError` with
`Line() const noexcept`. Functions:

```cpp
Command ParseCommand(std::string_view line);
CommandResult ExecuteCommand(OrderBook& book, const Command& command);
ReplaySummary Replay(std::istream& input, std::ostream& output);
```

The replay layer consumes the existing `OrderBook` API. Benchmark code in Task 2
will consume `Command` and `ExecuteCommand`; keep CLI and file I/O outside them.

- [ ] Write replay tests before implementation. Exercise real books/streams:
  FIFO and partial fills; GTC remainder; IOC discard; cancel/not-found; modify
  priority; sorted depth; blank/comment lines; CRLF; field whitespace; and errors
  for duplicate IDs, missing modifications, malformed fields, and numeric limits.
  Use hand-derived trade/output fixtures, including:

  ```text
  ADD,101,SELL,10000,10,GTC
  ADD,102,SELL,10010,5,GTC
  ADD,103,BUY,10020,12,IOC
  ```

  Expect trades `(103,101,10000,10)` and `(103,102,10010,2)`, one resting order,
  and ask depth `(10010,3)`. Empty input must produce a zero summary. After a valid
  command followed by bad input on line 2, expect `ReplayError::Line() == 2` and
  no final summary. Explicitly test maximum uint64 IDs, uint32 quantity limits,
  price overflow, negative unsigned values, `12x`, and missing/extra fields.
- [ ] Register the test target against the intended support API and run it.
  Expected: compilation fails because replay support is absent; after introducing
  the interface, behavioral tests must fail until parsing/execution is implemented.
- [ ] Implement strict full-field numeric parsing and the specified token grammar.
  Preserve engine validation. Ignore only blank/full-comment lines; add the
  physical line number to parser/engine errors. Do not emit a success summary
  after an error. Treat an unknown cancellation as a normal not-found result.
- [ ] Implement `orderbook_replay <file|->` and `--help`. Use exit 0 for success
  and help, exit 2 for arguments, input/open/read errors, or rejected commands.
  Output records: `TRADE,bid_id,ask_id,execution_price,quantity`,
  `CANCEL,id,CANCELLED|NOT_FOUND`, `SUMMARY,commands,trades,resting_orders`, then
  `BID,price,quantity` and `ASK,price,quantity` in book order.
- [ ] Add the example fixture and a CMake executable check comparing its exact
  output. Check file and stdin input, missing file, malformed input, and bad
  arguments. Fixture paths must be absolute/source-root based.
- [ ] Run `cmake --preset debug`, `cmake --build --preset debug`, and
  `ctest --preset debug`. Expected: existing engine tests and new replay/CLI
  tests pass. Commit: `feat: add deterministic CSV order replay`.

### Task 2: Benchmark workloads and measurements

**Files:** Create `benchmarks/benchmark.hpp`, `benchmarks/benchmark.cpp`,
`benchmarks/benchmark_main.cpp`, and `tests/benchmark_tests.cpp`; modify
`CMakeLists.txt`.

**Interfaces:** Consume Task 1's commands/executor. In `orderbook_tools`, produce
`BenchmarkOptions`, `Scenario`, `Workload`, `Measurement`, and:

```cpp
BenchmarkOptions ParseBenchmarkOptions(std::span<const std::string_view> args);
Workload MakeWorkload(Scenario scenario, std::size_t count, std::uint64_t seed);
Measurement Measure(const Workload& workload, std::size_t repetition);
void RunBenchmarks(const BenchmarkOptions& options, std::ostream& output);
```

`Workload` carries commands and independently derived expected trade/quantity/
resting counts and depth. `Measurement` carries elapsed time, throughput, p50/p99,
sample count, and validated result counts. No file parsing or printing is timed.

- [ ] Write failing tests for argument validation, workload execution/accounting,
  same-seed reproducibility, different-seed prices, incomplete cycles, and metric
  output. For all four scenarios, test counts 1 through 65 against hand-derived
  outcomes. Check the mixed six-command cycle yields two trades, executed
  quantity 12, no resting orders, and empty depth. For percentile samples
  `{40,10,30,20}`, expect nearest-rank p50 20 and p99 40; a single sample 7
  yields both percentiles 7. Never assert a particular measured duration.
- [ ] Run the benchmark test target before implementation. Expected: missing
  support/API failure, followed by behavioral failures until implemented.
- [ ] Generate four bounded workloads before timing, using `mt19937_64` outputs
  mapped with modulo arithmetic for cross-library seed reproducibility:
  `resting` adds 32 same-side orders then cancels them; `matching` alternates one
  resting sell and one full-fill IOC buy; `modify` repeats add/modify/cancel;
  `mixed` repeats two sells (10 and 5), modifies the second to 7, buys 12 IOC,
  cancels the remaining second sell, then submits an unmatched IOC sell.
  Seeded prices vary by cycle within a small positive range; a completed cycle
  leaves the book empty. Truncate only at command boundaries, use unique IDs,
  and independently derive expected final depth for partial cycles.
- [ ] Implement `orderbook_bench` options: operations default 100000, range
  1–1000000; repetitions default 3, range 1–30; seed default 42, any unsigned
  64-bit decimal value. Reject duplicates, missing values, unknown options,
  negative/overflowing numbers, and nonnumeric tails; help exits 0, errors 2.
- [ ] Warm up on a separate untimed book. Use `steady_clock` for a throughput
  pass without per-operation clocks and a separate latency pass with one sample
  per command. Allocate sample storage beforehand; sort after timing. Use
  nearest-rank p50/p99, check elapsed time before division, and verify result
  counts/depth after both passes. Include execution/result-accounting cost but
  exclude command generation, printing, final validation, and book destruction.
- [ ] Emit compiler/build/seed metadata and machine-readable CSV rows per
  scenario/repetition with operations, trades, executed quantity, final resting
  count, elapsed ns, operations/second, p50 ns, p99 ns, and latency sample count.
  Add a small CTest smoke run using 120 operations, one repetition, seed 42.
- [ ] Run the full Debug suite. Expected: engine, replay, benchmark behavior,
  and smoke tests pass with finite metrics and correct accounting. Commit:
  `feat: add reproducible order book benchmarks`.

### Task 3: Compiler/sanitizer CI and editor integration

**Files:** Create `.github/workflows/ci.yml`; modify `CMakePresets.json`,
`.vscode/tasks.json`, and `.vscode/launch.json`.

**Interfaces:** Consume the executables, test names, and presets from Tasks 1–2.
Produce a workflow running that same full suite and project-local editor tasks.

- [ ] Add a push/pull-request workflow with `contents: read` permissions, GCC
  Release, Clang Release, and Clang sanitizer configurations. Verify the checkout
  action against its official release before selecting/pinning it. Configure,
  build, and run the full CTest suite; enable normal sanitizer behavior in CI.
- [ ] Add project-local replay/debug and Release benchmark tasks. Preserve the
  current demo configuration and avoid automatic benchmarks on editor opening.
- [ ] Run the full local suite in Debug, GCC Release, Clang Release, and Clang
  ASan/UBSan. Expected: every configuration passes; the preexisting randomized
  test remains active in Release. Disable only leak detection for sandbox runs
  if required and record that restriction. Validate workflow syntax locally;
  hosted workflow execution remains pending until the user's push.
- [ ] Commit: `ci: test GCC Clang and sanitizer builds`.

### Task 4: Measured evidence, documentation, review, and cleanup

**Files:** Modify `README.md`; create `docs/DESIGN.md`, `docs/BENCHMARKS.md`,
`docs/RESUME.md`, and `CONTRIBUTING.md`; retire the completed proposal/plan files.

**Interfaces:** Consume implemented CLI/output contracts and successful test
evidence. Produce accurate public documentation and a clean, reviewed worktree.

- [ ] Run the Release benchmark with 100000 operations, three repetitions, seed
  42. Record actual rows and compiler, flags, CPU, OS, revision, and date in
  `docs/BENCHMARKS.md`. Explain separate throughput/latency passes, warmup,
  nearest-rank percentiles, clock/accounting overhead, bounded workloads, and
  machine-specific results. No performance threshold or speedup claim.
- [ ] Rewrite the README with implemented features, one copyable replay example,
  commands for build/tests/benchmarks, and links to design/results/contribution
  notes. Preserve archive/reference attribution and educational limitations.
- [ ] Write `docs/DESIGN.md` explaining ordered price maps, FIFO lists, the ID
  hash index, iterator ownership, noncopyable books, snapshot isolation,
  cancel/replace policy, and average/worst-case complexity without calling
  cancellation wholly constant-time when removing an empty map level costs
  logarithmic time. Explain allocation and exception-safety limitations.
- [ ] Write `CONTRIBUTING.md` with focused test/build/format guidance and
  `docs/RESUME.md` with conservative feature-based wording, attribution, and
  explicit instructions to claim only personal contributions and measured data.
- [ ] Execute every documented quick-start command, compare replay output to its
  fixture, inspect the final diff, and complete a fresh whole-branch code review
  using the selected execution workflow. Fix material findings with reproducing
  tests, then rerun the full affected configurations. Commit documentation and
  verified review fixes with messages describing their final changes.
- [ ] Replace the temporary proposal with the finished design documentation;
  remove this process plan from the checkout once its purpose is complete.
  Preserve history and recovery stash. Remove temporary worktrees/scripts,
  staged copies, and generated builds, integrating verified changes into the
  user's existing project folder according to the chosen workflow.
- [ ] Expected final state: documented replay and benchmark commands work, all
  local suites pass, no temporary artifacts remain, and hosted CI is accurately
  described as configured but awaiting the user's push.

## Review and execution choice

The spec is approved; this implementation plan still needs user review. Native
execution is recommended because the tasks share a command interface and the
project is small. Native means the current agent implements the tasks and uses
one fresh reviewer at the end. Subagent-driven execution uses separate
implementers/reviewers per task and costs more coordination and context.
