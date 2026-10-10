# Implementation roadmap

Everything below is planned work for you to implement, not a feature claim.
Choose your own interfaces and keep tests close to each behavior you add.

1. **Domain types.** Define IDs, prices, quantities, sides, orders, modifications,
   trades, and depth snapshots. Decide and document price representation and
   validation rules.
2. **Resting orders.** Store bids and asks by price, preserve FIFO within a price
   level, and index orders by ID. Test insertion and depth consistency.
3. **Matching.** Implement price/time priority, full and partial fills, and
   matching across levels. Verify quantity conservation and resulting trades.
4. **Cancellation and modification.** Define unknown-ID and duplicate-ID behavior.
   Document whether a modification loses queue priority; test that choice.
5. **Additional order types.** The reference defines GoodTillCancel, FillAndKill,
   FillOrKill, GoodForDay, and Market. Implement and test them one at a time.
6. **Lifecycle.** If following the reference's expiry/background-thread features,
   make shutdown deterministic and expiry testable without long sleeps. Document
   your thread-safety contract.
7. **Demonstration.** Add a repeatable command-line scenario or input replay so a
   reviewer can see trades and depth without reading the whole implementation.

## Make the finished portfolio project your own

Pick a small number of additions you can explain well:

- Deterministic replay with a fixed workload and verifiable final state.
- Property tests for quantity conservation, FIFO, and level aggregates.
- CI that builds and runs real tests on both GCC and Clang.
- Benchmarks comparing data structure choices on identical workloads.

For benchmarks, record the compiler, build flags, CPU, workload, seed, operation
count, and timing method. Separate setup time from measured operations. Do not
claim a latency or throughput figure until you can reproduce it.

Before sharing the project, update the README with implemented behavior, an
example run, build/test commands, design trade-offs, and measured results if any.
Keep the original reference credited. Use your Git history to show incremental
implementation; do not describe this initial scaffold as a completed engine.
