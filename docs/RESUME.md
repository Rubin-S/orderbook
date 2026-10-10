# Résumé and interview notes

This can demonstrate C++ systems fundamentals when you understand the engine,
can explain its trade-offs, and have personally implemented meaningful work.
The supplied archive already contained the engine and its tests. Replay,
benchmarks, CI, and documentation were added here with AI assistance; describe
your contribution accurately and follow any relevant application disclosure
requirements.

## Conservative project description

**C++20 Order Book — C++, STL, CMake, testing, performance measurement**

> Extended an educational price-time order book project with deterministic CSV
> replay, bounded workload benchmarks, and GCC/Clang sanitizer build checks.

Use “extended” only for parts you personally extend. If you are still studying
the existing implementation, describe it as a learning project and state the
specific changes you made. Do not claim the imported engine as original work.

After implementing and verifying those parts yourself, choose relevant bullets:

- Implemented price/FIFO matching, GTC/IOC behavior, and cancel/replace semantics
  using ordered price maps, FIFO lists, and an order-ID index.
- Added deterministic replay with strict numeric validation, line-numbered
  errors, exact-output fixtures, and tests for partial fills and priority.
- Designed reproducible insertion, matching, modification, and mixed workloads;
  reported throughput and p50/p99 latency with independently checked outcomes.
- Validated behavior with targeted and randomized differential tests, optimized
  GCC/Clang builds, and ASan/UBSan checks.

The first bullet applies only if you implement the engine yourself.
[Hosted CI passed](https://github.com/Rubin-S/orderbook/actions/runs/38057029270)
for GCC Release, Clang Release, and Clang ASan/UBSan. Do not claim
coverage percentages, an optimization speedup, production exchange readiness,
or competitive HFT latency. If citing a measurement, name the scenario, machine,
Release configuration, and method from [BENCHMARKS.md](BENCHMARKS.md); a raw number
without those qualifiers is easy to misinterpret.

## Be ready to explain

Why ordered maps and lists? What is the actual cancellation complexity? How does
modification change FIFO priority? Why is the execution price the resting price?
What does IOC discard? Why disable book copying? How do differential tests help?
What allocations and clock overhead are included in the benchmark? What happens
if memory allocation fails halfway through matching?

Credit `working_orderbook_cpp20.zip` and the learning reference
[Tzadiko/Orderbook](https://github.com/Tzadiko/Orderbook) in the public repository.
The README already records that provenance. A useful next personal contribution
is a measured optimization with a documented baseline and correctness checks;
choose it only after understanding the existing behavior.
