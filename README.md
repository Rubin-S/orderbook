# C++20 price-time-priority order book

A corrected, self-contained, header-only C++20 matching engine based on the supplied implementation.

Imported from `working_orderbook_cpp20.zip`. The original learning reference is
[Tzadiko/Orderbook](https://github.com/Tzadiko/Orderbook).

## Build and run

From the project directory, use the included presets (CMake 3.25+ and Ninja):

```bash
cmake --preset debug
cmake --build --preset debug
./build/debug/orderbook_demo
ctest --preset debug

cmake --preset release
cmake --build --preset release
ctest --preset release
```

The sanitizer preset uses the installed Clang compiler, avoiding missing GCC
sanitizer runtime packages on this machine:

```bash
cmake --preset sanitize
cmake --build --preset sanitize
ctest --preset sanitize
```

> The tests explicitly re-enable `assert()` even when compiled under `Release` (`-DNDEBUG`), so validation also runs in optimized builds.

For a direct compiler build without CMake:

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic main.cpp -o orderbook_demo
./orderbook_demo

g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -I. tests/orderbook_tests.cpp -o orderbook_tests
./orderbook_tests
```

All dependencies are standard C++; setup requires no package downloads. Generated
CMake files stay under the Git-ignored `build/` directory. Delete `build/` whenever
you want to reclaim space, then rerun configure before building again.

## Editor setup

Open this project directory in your editor. In VS Code, the workspace recommends
C/C++ and CMake Tools: `Ctrl+Shift+B` builds Debug, F5 launches the demo under GDB,
and the **Test debug** task builds and runs the included tests. Extensions are
recommended rather than installed automatically.

CLion can open the top-level `CMakeLists.txt` as an existing project. For Neovim
with `clangd`, configure Debug first and point `clangd` at the compilation
database in `build/debug/` (for example, `--compile-commands-dir=build/debug`).
The build itself is independent of the editor.

## Semantics

- **Price**: signed 32-bit positive *integer ticks*, not floating point monetary values; e.g., if tick size is $0.01, `10000` means $100.00.
- **OrderType::GoodTillCancel (GTC)**: fills immediately against crossing resting orders, then rests any remainder.
- **OrderType::FillAndKill (IOC)**: fills immediately against crossing resting orders, then discards any remainder. It does **not** require full execution. If you need all-or-none Fill-Or-Kill (FOK), that is a separate order type requiring an available-volume precheck.
- **Price-time priority**: best resting price first, FIFO at a price; executions occur at the *resting* order's price.
- **TradeInfo::price_**: each side's *limit price*. Use `Trade::GetExecutionPrice()` for the single actual trade price.
- **Order IDs**: unique among *live/resting* orders. Once filled or canceled (including an IOC), an ID may be reused. A real exchange gateway typically enforces a separate client/order-ID policy.
- **CancelOrder(id)**: returns false if not a live order; true if canceled.
- **ModifyOrder(OrderModify{id, side, price, newTotalQuantity})**: cancel-replace; keeps the order type and side, replaces total quantity, resets time priority and filled history. Rejects invalid values or side changes without removing the existing order. Throws if order ID is not live. This is a simple **cancel/replace policy**, not exchange-specific amend-in-place semantics.
- **External pointer safety**: submitted `shared_ptr<Order>` is copied into the engine, and `Order::Fill` is private. `GetOrder` and `GetOrderInfos` return independent snapshots.
- **Aggregate depth**: accumulated with `uint64_t` (checks overflow), even though individual orders have `uint32_t` quantity.
- **Input validation**: rejects null orders, nonpositive prices, zero quantities, unknown enum values, and duplicate live IDs.
- **Not thread-safe**: synchronize all order-book access externally if using multiple threads.

## Layout

- `orderbook.hpp`: engine and API (header-only)
- `main.cpp`: demonstration with GTC asks + incoming IOC buy
- `tests/orderbook_tests.cpp`: targeted tests and a 12,000-step randomized differential test against an independent, simpler reference matcher
- `CMakeLists.txt`: C++20 demo and CTest build
- `CMakePresets.json`: Debug, Release, and Clang sanitizer configurations
- `.vscode/`: project-local build, run, test, and GDB tasks

## Scope / limitations

This is an educational **single-symbol limit order book**, not a production exchange. It intentionally omits message sequencing, trading sessions, matching-engine persistence/recovery, risk limits, external price tick validation, fees, market/stop/iceberg order types, self-trade prevention, event journaling, advanced allocation policies, live feed concurrency, and full transaction rollback on memory allocation failure. `Price` encodes already-normalized ticks; tick-size and symbol-level validation must be applied upstream. Returns only executions generated by the newly submitted or modified order. The matching engine is deterministic given the same sequence of commands.
