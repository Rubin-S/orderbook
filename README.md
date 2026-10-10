# C++ Limit Order Book

A personal C++20 order book project, being implemented from scratch as a learning
and portfolio project. Inspired by [Tzadiko/Orderbook](https://github.com/Tzadiko/Orderbook)
and TheCodingJesus's walkthrough.

**Status: starter scaffold only.** Matching logic, domain types, tests, and
benchmarks have not been implemented. The executable currently returns success
without output. The reference implementation has not been copied into this repo.

## Build and run

Requirements: a C++20 compiler, CMake 3.25 or newer, and Ninja. All three are
already installed on the machine where this starter was set up. There are no
third-party dependencies or automatic package downloads.

From the project directory:

```bash
cmake --preset debug
cmake --build --preset debug
./build/debug/orderbook
```

Release and sanitizer builds:

```bash
cmake --preset release
cmake --build --preset release

cmake --preset sanitize
cmake --build --preset sanitize
./build/sanitize/orderbook
```

The sanitizer preset uses the installed Clang compiler and enables
AddressSanitizer and UndefinedBehaviorSanitizer. This avoids the missing GCC
sanitizer runtime packages on this machine. Link thread-dependent code through the existing `orderbook_core`
target, which already links CMake's portable threads target.

## Where to code

```text
include/orderbook/   Empty headers using the reference project's filenames
src/Orderbook.cpp   Your implementation
src/main.cpp        Your demo or command-line interface
tests/              Your tests and their CMake registration
docs/ROADMAP.md     Suggested implementation and portfolio milestones
build/              Generated files only; ignored by Git
```

Header names mirror the reference so you can follow the walkthrough. Include a
public header with `#include "orderbook/Order.h"`. Headers in that same folder
can include one another with `#include "Order.h"`.

Add any new implementation files explicitly to `orderbook_core` in the root
`CMakeLists.txt`. Keep the demo in `main.cpp` so your tests can link the core
library independently.

## Tests and editor

CTest is wired up, but there are no tests yet. See `tests/README.md` for suggested
cases and registration instructions. After registering tests, run
`ctest --preset debug` or `ctest --preset sanitize`. An empty test suite reports
an error deliberately, rather than suggesting the project is tested.

Open this directory in VS Code. The workspace recommends the C/C++ and CMake
Tools extensions; it does not install them automatically. `Ctrl+Shift+B` builds
Debug, and F5 runs the GDB configuration. Other tasks run the program or tests.
You can also use any editor and the terminal commands above.

Format C++ files with `clang-format -i path/to/file.cpp`. The workspace carries
the project's formatting rules. Build output stays inside `build/`; remove that
directory whenever you want to reclaim space. CMake recreates it as needed.

## Portfolio notes

Keep this README accurate as you implement features. Record your own design
choices, test coverage, and reproducible measurements before adding performance
claims. Credit the reference and clearly describe your own extensions. The
roadmap includes suggestions for making the finished project demonstrably yours.

This starter is intended for a fresh local Git repository on `main`, with no
remote configured. Publish to your own repository when you are ready.
