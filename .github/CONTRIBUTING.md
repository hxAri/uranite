# Contributing to Uranite

## Table of Contents
- [Contributing to Uranite](#contributing-to-uranite)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Before You Contribute](#before-you-contribute)
    - [Contributor License](#contributor-license)
    - [Code of Conduct](#code-of-conduct)
  - [Ways to Contribute](#ways-to-contribute)
  - [Getting Started](#getting-started)
    - [Prerequisites](#prerequisites)
    - [Building from Source](#building-from-source)
    - [Running Tests](#running-tests)
  - [Contribution Workflow](#contribution-workflow)
    - [Finding Something to Work On](#finding-something-to-work-on)
    - [Development Process](#development-process)
    - [Pull Request Process](#pull-request-process)
  - [Code Style](#code-style)
    - [C++ (Compiler Source)](#c-compiler-source)
    - [Uranite (.urn Modules)](#uranite-urn-modules)
  - [Commit Message Guidelines](#commit-message-guidelines)
  - [Testing Requirements](#testing-requirements)
  - [Documentation](#documentation)
  - [Questions and Communication](#questions-and-communication)

## Overview

Uranite welcomes contributions from everyone who wants to improve the language, compiler, standard library, tooling, or documentation. This guide explains how to set up your development environment, the workflow for submitting changes, and the standards your code must meet.

All contributions must follow the project's [Code of Conduct](CODE-OF-CONDUCT.md).

## Before You Contribute

### Contributor License

By submitting a pull request, you agree that your contribution is licensed under the [GNU General Public License v3](https://www.gnu.org/licenses/gpl-3.0) consistent with the rest of the project.

### Code of Conduct

All participants in the Uranite community must abide by the [Code of Conduct](CODE-OF-CONDUCT.md). Please read it before engaging with the project.

## Ways to Contribute

- **Bug reports:** File issues with minimal reproduction cases and environment details.
- **Bug fixes:** Submit patches for known issues.
- **New features:** Propose and implement language features, compiler improvements, or standard library additions.
- **Standard library:** Extend or improve modules in `stdlibs/`.
- **Testing:** Add test cases in `testing/` or expand the GoogleTest suite.
- **Documentation:** Improve doccomments, README, or inline explanations.
- **Tooling:** Improve the formatter (`uranite-fmt`), doc generator (`uranite-doc`), or package manager (`uranite-pkg`).

## Getting Started

### Prerequisites

- C++17 compiler (GCC 12+ or Clang 19+)
- LLVM 19 (via `llvm-config`)
- CMake 3.22+
- Libraries: fmt, spdlog, argparse, GoogleTest
- System: pthread, rt, dl

```bash
# Debian/Ubuntu
sudo apt-get install -y llvm-19-dev libfmt-dev libspdlog-dev libgtest-dev cmake g++
```

### Building from Source

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
make -C build -j$(nproc)
```

This produces:
- `./build/uranite` — the compiler
- `./build/uranite-tests` — GoogleTest suite
- `./build/uranite-fmt` — code formatter
- `./build/uranite-doc` — documentation generator
- `./build/uranite-pkg` — package manager

### Running Tests

```bash
# Full GoogleTest suite
./build/uranite-tests

# Single test
./build/uranite-tests --gtest_filter="ParserTest.ParsesSimpleFunction"

# All 212 module integration tests
for f in testing/test-*.urn; do
    result=$(./build/uranite "$f" 2>&1 | tail -1)
    echo "$(basename $f): $result"
done

# Runtime verification (compile + execute)
for f in testing/test-*.urn; do
    name=$(basename "$f" .urn)
    timeout 10 ./build/uranite "$f" -o /tmp/urn_test 2>/dev/null
    if [ $? -ne 0 ]; then echo "COMPILE FAIL: $name"; continue; fi
    timeout 10 /tmp/urn_test > /dev/null 2>&1
    if [ $? -ne 0 ]; then echo "RUNTIME FAIL: $name"; fi
done
```

All tests must pass before submitting a pull request. Both compile-time and runtime verification are required.

## Contribution Workflow

### Finding Something to Work On

1. Check the [Issue Tracker](https://github.com/uranite-lang/uranite/issues) for open issues.
2. Look for issues labeled `good first issue` or `help wanted`.
3. If you want to work on something not yet filed, open an issue first to discuss the approach.

### Development Process

1. Fork the repository from the `prod` branch.
2. Create a feature branch in your fork.
3. Make your changes in focused, logical commits.
4. Ensure all existing tests continue to pass (212/212 module tests, full GoogleTest suite).
5. Add tests for new functionality.
6. Run the full test suite before pushing.

### Pull Request Process

1. Push your branch to your fork.
2. Open a pull request against the **`dev`** branch.
3. Fill in the PR template with a clear description of what changed and why.
4. Ensure CI passes (all tests green, no compilation warnings).
5. Respond to review feedback promptly.
6. Once approved, a maintainer will merge your PR into `dev`.

Keep PRs focused. One logical change per PR. Large refactors should be discussed in an issue before implementation.

### Branch Restrictions

- **Fork from `prod`, push to `dev`.** All forks should be based on the stable `prod` branch. All pull requests must target `dev`.
- Direct pull requests to `prod` will be rejected without review.
- The `prod` branch is the stable release branch. Only maintainers merge `dev` into `prod` after verification and testing.
- Contributors must never push or open PRs directly against `prod`.
- The merge flow is: `prod` (fork) → `your-branch` (develop) → `dev` (PR) → `prod` (maintainer merge).

## Code Style

### C++ (Compiler Source)

All compiler source lives under `src/uranite/`. Follow these conventions strictly:

- **No spaces before opening paren in control flow:** `if( x )` not `if (x)`.
- **Spaces inside parens when args present:** `foo( x, y )` not `foo(x, y)`. Empty parens: `foo()`.
- **Explicit false/nullptr checks:** Use `== false` / `== nullptr` instead of `!expression`. Never use `== true`.
- **String formatting:** Use `fmt::format` for string building, not `+` concatenation. Store formatted result in a named variable.
- **No `auto`:** Use concrete types everywhere.
- **Allman-ish braces:** Closing brace on its own line, `else {` on a separate line after.
- **Explicit `this->`:** Use `this->` for all member function calls.
- **Sort conditions:** Alphabetically/numerically where applicable.
- **No comments unless WHY is non-obvious:** Default to no comments. If removing a comment would not confuse a future reader, do not write it.

### Uranite (.urn Modules)

Standard library modules in `stdlibs/` must follow:

- **Descriptive variable names:** Never use single-letter or cryptic abbreviations (`i`, `j`, `n`, `ch`, `ptr`, `val`, `buf`). Use context-appropriate names: `slotIndex`, `byteIndex`, `scanOffset`, `frameOffset`, `deviceIndex`.
- **Boolean keywords:** `and`/`or`/`not` — never `&&`/`||`/`!`.
- **Doccomments:** Use `"""..."""` AFTER declarations. Include `Parameters:`, `Returns:`, `Complexity:` sections where applicable.
- **Multi-entity imports:** Prefer comma-separated or brace-wrapped imports from the same module.
- **Zero C runtime:** All standard library modules must use raw syscalls and pure Uranite only. No libc dependency.
- **Multi-architecture assembly:** Use arch blocks for any inline assembly that differs between architectures:

```uranite
asm volatile:
    x86-64 "pause"
    aarch64 "yield"
```

- **Access modifiers:** All declarations intended for import must use explicit `public` access modifier.

## Commit Message Guidelines

Use descriptive commit messages that explain the change:

- **Format:** `[ACTION] Brief description` where ACTION is one of: `CREATE`, `UPDATE`, `REMOVE`, `FIX`, `REFACTOR`.
- **Subject line:** Under 72 characters.
- **Body (optional):** Explain WHY the change was made if not obvious from the subject.

Examples:
```
[CREATE] Created stdlibs/os/arch/aarch64/cpu.urn
[FIX] Fixed use-after-move detection in nested closures
[UPDATE] Updated spinlock to support x86-64 and aarch64 via arch blocks
```

## Testing Requirements

Every change must maintain the following invariants:

1. **212/212 module integration tests compile successfully.** No regressions.
2. **All GoogleTest unit tests pass.** No failures.
3. **Runtime verification:** New features must be tested at runtime, not just compilation. A test that compiles but segfaults at runtime is not passing.
4. **New features require test coverage:** Add a test file in `testing/` for new language features or stdlib modules.
5. **Benchmark validation:** For codegen changes, verify `examples/benchmarks/*.urn` still compile:

```bash
for f in examples/benchmarks/*.urn; do
    ./build/uranite "$f" 2>&1 | tail -1
done
```

## Documentation

- Use `"""..."""` doccomments after declarations in `.urn` files.
- Include `Parameters:`, `Returns:`, and `Complexity:` sections for public functions.
- Keep the README up to date when adding major features or changing the module table.
- Do not create separate documentation files unless explicitly requested — prefer doccomments in source.

## Questions and Communication

- **Bug reports and feature requests:** [GitHub Issues](https://github.com/uranite-lang/uranite/issues)
- **General questions:** Open a discussion on the repository
- **Security vulnerabilities:** Report privately to hxari@proton.me

Thank you for contributing to Uranite.
