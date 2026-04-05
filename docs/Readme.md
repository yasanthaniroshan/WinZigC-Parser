# WinzigC Parser

Initial repo for WinzigC parser, a comprehensive, synergistic, holistic, fault-tolerant, next-gen, AI-enabled parser for parsing. It can also make coffee.

## Building the project

### Prerequisites

- **CMake** 3.16 or newer
- A **C++17** toolchain (**GCC** or **Clang**)
- **Ninja** (optional but recommended): `brew install ninja`, `sudo apt install ninja-build`, etc.

Dependencies (**CLI11**, **spdlog**, **GoogleTest**) are fetched automatically by CMake via `FetchContent`.

### Configure and build

From the repository root:

```bash
cmake -G Ninja -B build
ninja -C build
```

Without Ninja (uses your platform’s default generator, often Makefiles):

```bash
cmake -B build
cmake --build build
```

### Run tests

After a successful build, either:

```bash
ctest --test-dir build
```

or run the test binary directly:

```bash
./build/tests
```

`ctest` runs the same executable but registers each GoogleTest case separately (useful in IDEs and CI). `./build/tests` runs all tests in one process.

### Run the parser

The `parser` target links the `utils` library. Ensure a `logs` directory exists if file logging is enabled (the logger writes under `logs/`).

```bash
./build/parser --version
```

### Coverage (GCC / Clang only)

Instrumentation uses **`--coverage`** (gcov-style). It is **not** supported with **MSVC**; use **GCC or Clang** (including **Clang on Windows**) if you need coverage.

Configure with coverage enabled, then build:

```bash
cmake -G Ninja -B build -DENABLE_COVERAGE=ON
ninja -C build
```

**You must run the tests before capturing coverage**, so that `.gcda` files are produced next to the object files under `build/`.

```bash
ctest --test-dir build
# or: ./build/tests
```

Install **lcov** if needed (`brew install lcov`, `sudo apt install lcov`). Capture and trim noise (test-only sources); adjust filters as you prefer:

```bash
lcov --capture \
     --directory build \
     --output-file coverage.info \
     --ignore-errors mismatch,source,gcov

lcov --remove coverage.info '*/tests/*' -o coverage.info
```