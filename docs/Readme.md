# WinzigC Parser

[![C++ CI](https://github.com/yasanthaniroshan/WinZigC-Parser/actions/workflows/ci.yaml/badge.svg)](https://github.com/yasanthaniroshan/WinZigC-Parser/actions/workflows/ci.yaml)
[![Release](https://github.com/yasanthaniroshan/WinZigC-Parser/actions/workflows/release.yaml/badge.svg)](https://github.com/yasanthaniroshan/WinZigC-Parser/actions/workflows/release.yaml)
![GitHub release](https://img.shields.io/github/v/release/yasanthaniroshan/WinZigC-Parser)
![Linux](https://img.shields.io/badge/Linux-supported-success)
![macOS](https://img.shields.io/badge/macOS-supported-success)
![Windows](https://img.shields.io/badge/Windows-supported-success)

A compiler for the WinZigC language. It runs a five-stage pipeline — **tokenize → parse → semantic analysis → optimize → code generation** — turning a `.winzig` source file into stack-machine assembly. The abstract syntax tree can also be printed for debugging.

The stages:

1. **Tokenizer** — lexes the source into tokens.
2. **Parser** — recursive-descent parser that builds an abstract syntax tree.
3. **Semantic analyzer** — builds a symbol table and enforces scope/type rules (see `docs/AttributeGrammer.md`).
4. **Optimizer** — an AST-level pre-pass (`preOptimize`) followed by an assembly-level peephole post-pass (`postOptimize`), selected by `-O` (see [Optimization](#optimization)).
5. **Code generator** — emits stack-machine assembly (see `docs/instruction_set.md`) to an output file (`output.asm` by default).

When `--ast` is passed, the pipeline stops after parsing and prints the tree.

## Project layout

| Path | Purpose |
|------|---------|
| `app/` | `winzigc` executable entry point |
| `include/` | Public headers (`common/`, `utils/`, `tokenizer/`, `parser/`, `semantic_analyzer/`, `optimizer/`, `code_generator/`) |
| `src/` | Library implementations |
| `tests/unit/` | GoogleTest unit tests |
| `tests/grammar/` | Small grammar fixtures (`.winzig` + `.tree`) |
| `tests/integration/` | Full-program golden trees |
| `winzig_test_programs/` | Sample WinZigC programs and their expected `.tree` output |
| `docs/` | Grammar PDFs, instruction set, attribute grammar, and this guide |
| `build/` | Out-of-tree build directory (created by CMake; gitignored) |
| `logs/` | Log files when file logging is enabled (gitignored) |

### CMake targets

| Target | Type | Description |
|--------|------|-------------|
| `winzigc` | executable | CLI driver (tokenize → parse → analyze → optimize → codegen) |
| `parser` | static library | Parser + AST (`tree`) |
| `tokenizer` | static library | Lexer |
| `semantic_analyzer` | static library | Symbol table + semantic checks |
| `optimizer` | static library | AST pre-pass + assembly peephole post-pass |
| `code_generator` | static library | Stack-machine code emission |
| `utils` | static library | Argparser, file reader |
| `winzig_log` | static library | Logging (spdlog) |
| `tree` | static library | AST node utilities |
| `common` | interface | Shared headers (`Result`, `Error`) |
| `tests` | executable | Unit / grammar tests |

Dependencies (**CLI11**, **spdlog**, **GoogleTest**) are fetched automatically via CMake `FetchContent`.

## Building

### Prerequisites

- **CMake** 3.16 or newer
- A **C++17** toolchain (**GCC** or **Clang**)
- **Ninja** (recommended): `brew install ninja`, `sudo apt install ninja-build`, etc.
- **Git** (optional but recommended): embeds the short commit hash in `--version`

### Configure and build

From the repository root:

```bash
cmake -G Ninja -B build 
ninja -C build
```

Without Ninja:

```bash
cmake -B build
cmake --build build
```

Artifacts:

- `./build/winzigc` — compiler CLI
- `./build/tests` — test runner

### Reconfigure after CMake changes

If you edit `CMakeLists.txt` or add/remove sources, re-run configure (Ninja will often pick this up automatically; when in doubt):

```bash
cmake -G Ninja -B build
ninja -C build
```

## Cleaning

### Incremental clean (keep CMake cache and dependencies)

Removes object files and binaries under `build/`, but keeps `CMakeCache.txt` and FetchContent downloads:

```bash
ninja -C build clean
```

Equivalent:

```bash
cmake --build build --target clean
```

### Full clean (recommended after branch switches or weird build errors)

Deletes the entire build tree (including generated `version.h`, coverage data, and re-downloaded third-party sources on the next configure):

```bash
rm -rf build
```

Then configure and build again:

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

### Clean logs

```bash
rm -rf logs/*
```

Create the directory again if your logger expects it:

```bash
mkdir -p logs
```

## Running `winzigc`

Ensure a `logs/` directory exists if file logging is enabled.

### Options

| Flag | Description |
|------|-------------|
| `-v`, `--version` | Print version metadata and exit |
| `-l`, `--log-level <LEVEL>` | `DEBUG`, `INFO`, `WARN`, or `ERROR` |
| `-i`, `--input-file <path>` | Input `.winzig` file |
| `-o`, `--output-file <path>` | Output path for generated assembly (default: `output.asm`) |
| `-O`, `--optimization-level <LEVEL>` | `O0`, `O1`, or `O2` (default: `O1`) — see [Optimization](#optimization) |
| `-a`, `--ast` | Print the abstract syntax tree and stop (skips analysis/codegen) |
| `<input>` | Positional input file (alternative to `-i`) |
| `-h`, `--help` | Show CLI help (provided by CLI11) |

### Examples

```bash
./build/winzigc --version

./build/winzigc -l DEBUG tests/grammar/const_integer.winzig

./build/winzigc -a -i tests/grammar/string_node.winzig

./build/winzigc tests/grammar/const_list.winzig -o out.txt

./build/winzigc tests/grammar/const_list.winzig -O O2
```

On success the process writes stack-machine assembly to the output file (`output.asm` by default) and exits with code `0`; on tokenize/parse/semantic/codegen/IO errors it exits with `1` and logs the error.

## Optimization

The optimizer runs in two stages, both selected by the `-O` level. Optimizations are **behavior-preserving**: a program's observable output is identical at every level (enforced by the differential tests under `tests/optimization/`, which compile each program at `O0`/`O1`/`O2` and check the stack-machine output matches).

| Level | What runs |
|-------|-----------|
| `O0` | Nothing — the front end's output is emitted verbatim. |
| `O1` | Dead-code elimination only: unused globals, then unused locals, then unused functions. |
| `O2` | `O1`, plus constant propagation, constant folding, algebraic simplification, dead-branch and unreachable-code elimination (run to a fixpoint), **and** the assembly peephole pass. |

**Stage 1 — `preOptimize` (AST level).** Operates on the abstract syntax tree before code generation:

- **Dead-code elimination** — removes unused globals/locals/functions and compacts the symbol table so they drop out of `.data` and stack frames.
- **Constant & copy propagation** — a variable assigned exactly once (to a constant, or to another variable) is replaced at its uses.
- **Constant folding** — arithmetic, relational, and boolean expressions over literals collapse to a single value (e.g. `6 * 4 + 2` → `26`). Named constants (`Max`, `true`/`false`, enum literals) resolve to their integer ordinal.
- **Algebraic simplification** — identities such as `x + 0`, `x * 1`, `x / 1` → `x`, and `x * 0` → `0`.
- **Dead-branch / unreachable-code elimination** — `if`/`while` conditions that fold to a constant are resolved, and statements after a `return`/`exit` are dropped.

**Stage 2 — `postOptimize` (assembly peephole, `O2` only).** A small sliding-window pass over the emitted, label-based assembly — it catches redundancy that only exists after the tree is lowered to instructions. Removed patterns:

- `lit 0; add` / `lit 0; subtract` (x ± 0)
- `lit 1; multiply` / `lit 1; divide` (x · 1, x / 1)
- `negate; negate`
- `load v; save v` self-copy (the reverse, `save v; load v`, is **kept** — `save` has a side effect)
- `goto L` where `L` labels the next instruction (a fall-through)

It runs on the *labeled* form (not the raw line-numbered stream), so deleting an instruction can never corrupt a branch target, and it never cancels a pair across a jump target.

## Tests

After a successful build:

```bash
ctest --test-dir build
```

or run the test binary directly:

```bash
./build/tests
```

`ctest` registers each GoogleTest case separately (useful in IDEs and CI). `./build/tests` runs all tests in one process.

Grammar fixtures live under `tests/grammar/` (`.winzig` sources and matching `.tree` goldens). Integration goldens are under `tests/integration/`. Differential optimization tests under `tests/optimization/` compile each sample at `O0`/`O1`/`O2` and run the output through the stack machine (`scripts/machine.py`) to confirm every level produces identical output.

## Coverage (GCC / Clang only)

Instrumentation uses **`--coverage`** (gcov-style). It is **not** supported with **MSVC**; use **GCC or Clang** if you need coverage.

Configure with coverage enabled, then build:

```bash
cmake -G Ninja -B build -DENABLE_COVERAGE=ON
ninja -C build
```

**Run tests before capturing coverage** so `.gcda` files are written under `build/`:

```bash
ctest --test-dir build
# or: ./build/tests
```

Install **lcov** if needed (`brew install lcov`, `sudo apt install lcov`). Capture and trim test-only paths:

```bash
lcov --capture \
     --directory build \
     --output-file coverage.info \
     --ignore-errors mismatch,source,gcov

lcov --remove coverage.info '*/tests/*' -o coverage.info

genhtml coverage.info --output-directory coverage-html
```

Open `coverage-html/index.html` in a browser to view line coverage.

To drop coverage instrumentation, do a **full clean** and reconfigure without `-DENABLE_COVERAGE=ON`.

## CI and releases

- **CI** (`.github/workflows/ci.yaml`): runs on every push and pull request — configure, build with coverage, run tests, capture lcov, post a coverage summary (job summary and PR comment), and upload an HTML report artifact (`coverage-report`).
- **Release** (`.github/workflows/release.yaml`): triggered by pushing a version tag (`v*`, e.g. `v0.1.1`) or manually from the Actions tab. Builds `winzigc` for Linux, macOS, and Windows and attaches binaries to a GitHub Release.

```bash
git tag v0.1.1
git push origin v0.1.1
```

## Grammar reference

- `docs/WinZigC_Grammar.pdf` — language grammar
- `docs/WinzigC_Lex.pdf` — lexical rules
- `docs/AttributeGrammer.md` — semantic rules enforced by the analyzer
- `docs/instruction_set.md` — stack-machine instruction set emitted by codegen
