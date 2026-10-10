# Tests

No tests or order book behavior are implemented yet. CTest is configured, and
`ctest --preset debug` intentionally reports an error until you register a test.

Write tests in this directory and register each executable with `add_test()` in
`CMakeLists.txt`. Link test targets to `orderbook_core`. You can start with plain
C++ checks that return a nonzero exit status on failure; no framework download is
required. Avoid relying only on `assert`, which is disabled in Release builds.

Suggested cases to implement alongside the corresponding behavior:

- Inserting a bid or ask without a match; querying an empty book.
- Price priority and FIFO ordering at the same price.
- Full fills, partial fills, and matching across several price levels.
- Cancellation, modification, duplicate IDs, and unknown IDs.
- Each order type, including insufficient liquidity and unfilled remainders.
- Quantity and depth consistency after every operation.
- Shutdown and expiry, if you implement background processing.

After implementing and registering tests:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```
