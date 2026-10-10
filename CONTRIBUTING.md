# Contributing

Keep the project focused on a single-symbol, single-threaded order book. Explain
matching-policy changes and update examples and documentation with the code.
There are no downloaded product or test dependencies.

## Build and verify

From the repository root, with CMake 3.25+, Ninja, and a C++20 compiler:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Add a behavioral regression test before a fix; confirm that it fails for the
intended reason. Use real engine/stream behavior and hand-derived expected
results. Avoid performance thresholds and assertions about a specific duration.
Keep Release checks active independently of `NDEBUG`.

Before submitting changes, run the full suite with `release`, `clang-release`,
and `sanitize` presets, using the same configure/build/ctest sequence. Sanitizers
require Clang and its ASan/UBSan runtimes. If a restricted local tracing sandbox
blocks LeakSanitizer, use `ASAN_OPTIONS=detect_leaks=0 ctest --preset sanitize`
only for that run and record the limitation; do not suppress it in CI or presets.
GitHub Actions runs the full suite for GCC Release, Clang Release, and Clang
ASan/UBSan. Hosted execution begins when the repository is pushed.

## Test responsibilities

- `orderbook_tests`: targeted matching/validation/snapshot tests and 12,000 seeded
  differential operations against an independent vector matcher.
- `replay_tests`: grammar, ranges, priority, accounting, depth, and line errors.
- `replay_cli`: exact file/stdin fixture output and CLI failures, using source-root
  fixture paths from the build directory.
- `benchmark_tests`: option boundaries, reproducibility, lengths 1–65 in all four
  scenarios, independently calculated outcomes, and percentile/metric behavior.
- `benchmark_smoke`: the executable at 120 operations, one repetition, seed 42.

## Formatting and evidence

Use the repository's `.clang-format` for C++ changes. For the added tooling:

```bash
clang-format --dry-run --Werror tools/*.hpp tools/*.cpp benchmarks/*.hpp benchmarks/*.cpp tests/replay_tests.cpp tests/benchmark_tests.cpp
```

Keep generated files inside ignored `build/` directories and remove them when
finished if storage is constrained. Do not commit binaries or local editor
caches. Record compiler, flags, CPU/OS, revision, command, seed, and all repetitions
when publishing benchmark results. Preserve source attribution and distinguish
imported, assisted, and personally implemented contributions.
