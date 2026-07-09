
<!--
@author hxAri (hxari)
@create 2025-02-24 15:15
@update 10-07-2026
@github https://github.com/uranite-lang/uranite

Uranite Copyright (c) 2025 - hxAri <hxari@proton.me>
Uranite Licence under GNU General Public Licence v3

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
any later version.

You should have received a copy of the GNU General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.
-->

<div align="center">

# Uranite

-

**Accelerated Execution Through Quantum Precision**

A low-level, high-productivity system programming language built on top of a C++17 and LLVM 19 backend.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

</div>

---

## Table of Contents
- [Uranite](#uranite)
  - [Table of Contents](#table-of-contents)
  - [Language Overview \& Philosophy](#language-overview--philosophy)
  - [Concrete Language Features (Fully Implemented)](#concrete-language-features-fully-implemented)
    - [Smart Memory Management](#smart-memory-management)
    - [Object-Oriented Architecture](#object-oriented-architecture)
    - [Ergonomic Standard Library](#ergonomic-standard-library)
    - [Unified Error Hierarchy](#unified-error-hierarchy)
    - [Additional Language Capabilities](#additional-language-capabilities)
  - [Compiler Architecture \& Pipeline State](#compiler-architecture--pipeline-state)
    - [Active Compilation Pipeline](#active-compilation-pipeline)
    - [HIR and MIR Pipeline](#hir-and-mir-pipeline)
  - [Verification \& Testing Status](#verification--testing-status)
    - [GoogleTest Suite](#googletest-suite)
    - [Module Integration Tests](#module-integration-tests)
  - [Repository Development Infrastructure](#repository-development-infrastructure)
    - [Building from Source](#building-from-source)
    - [CLI Usage](#cli-usage)
  - [Language Comparison Matrix](#language-comparison-matrix)
  - [Key Architectural Distinctions](#key-architectural-distinctions)
  - [Warning](#warning)
  - [Issues](#issues)
  - [Support](#support)
  - [Licence](#licence)

## Language Overview & Philosophy

Uranite is a statically typed, ahead-of-time compiled language that generates native machine code through LLVM 19. It targets the design space between Python's readability and C's performance: code reads like a high-level scripting language but compiles to bare-metal executables with no interpreter, virtual machine, or garbage collector in the execution path.

The two defining syntax decisions are:

- **Indentation-based block structure.** Blocks open with a colon and an indent level increase. No curly braces `{}` exist in the language grammar. Scope is determined by whitespace depth, identical to Python's model.
- **Textual logical operators.** Boolean logic uses `and`, `or`, and `not` instead of `&&`, `||`, and `!`. This matches the language's design goal of reducing punctuation noise in systems code.

```uranite
package main

from uranite.io.console import puts

public function main() -> I32:
    puts( "Hello, Uranite!" )
    return 0
```

```bash
./build/uranite hello.urn -o hello && ./hello
# Hello, Uranite!
```

---

## Concrete Language Features (Fully Implemented)

### Smart Memory Management

Uranite tracks ownership and move semantics at compile time with zero garbage collection and no required lifetime annotations. The borrow checker detects use-after-move and double-free violations before code generation. Scope unwinding is deterministic: `defer` executes deferred actions in LIFO order before function return, and `finally` guarantees execution after `try`/`except` blocks regardless of the error path. Raw typed memory is accessible through the `Memory<T>` compiler intrinsic, which maps directly to heap allocation.

```uranite
function processData( I64 size ) -> Void:
    Memory<I64> buffer = new Memory<I64>( size )
    defer:
        delete buffer
    # buffer is guaranteed to be freed on any exit path
```

### Object-Oriented Architecture

The type system supports single class inheritance, multiple interface implementation, traits, structs, enums with unit variants and backed values, and generics monomorphized at compile time. Classes support virtual dispatch, `Readonly` and `final` modifiers, abstract methods, constructor property promotion, and nested declarations. Type identity comparisons throughout the compiler use fully qualified names (`qualname`) to prevent namespace collisions and structural type confusion.

```uranite
public Readonly class Point:
    public I64 coordX
    public I64 coordY

    public function Point( self, I64 coordX, I64 coordY ) -> Void:
        self.coordX = coordX
        self.coordY = coordY

public interface Drawable:
    public function draw( self ) -> Void;
```

### Ergonomic Standard Library

The standard library is written entirely in Uranite. Core I/O, threading, networking, and filesystem operations run on raw Linux syscalls with zero C runtime dependency. Collections, string utilities, regex, HTTP, and cryptographic primitives deliver a Python-like developer experience built over raw-metal performance primitives.

| Module | Contents |
| --- | --- |
| `async/` | Pure-Uranite async runtime: epoll, timerfd, context switching, task scheduler (no C runtime) |
| `cli/` | Command-line argument parsing |
| `collection/` | `ArrayList<E>`, `HashMap<K,V>`, `HashSet<E>`, `LinkedList<E>`, `Args<T>`, `Kwargs<K,V>` |
| `convert/` | Type conversion: `intToString`, `floatToString`, `stringToInt` |
| `coroutine/` | `Future<T>`, `Task<T>`, `Scheduler`, `AsyncChannel<T>` interfaces and implementations |
| `crypto/` | AES cipher, keystore, CSPRNG |
| `datetime/` | Date and time utilities |
| `errors/` | `Throwable`, `Error`, `Exception`, `Warning`, `LookupError`, `IndexError`, `KeyError`, `TypeError`, `ValueError`, `StateError` (auto-imported) |
| `ffi/` | Foreign function interface: dynamic library loading, symbol resolution |
| `fiber/` | Cooperative lightweight threads via register save/restore |
| `functions/` | String utilities: `equals`, `indexOf`, `substring`, `trim`, `toUpper`, `toLower`, `repeat`, `reverse` |
| `io/` | Console I/O, file I/O, streams, buffered readers/writers, paths, directories |
| `iterators/` | `Iterator`, `Iterable`, `Traversable` interfaces |
| `language/` | OOP wrapper types: `I64`, `String`, `Float`, `Boolean`, `Char`, `U8`, `Double`, and others |
| `logging/` | Structured logging with multiple handlers and formatters |
| `math/` | `ArithmeticError`, `ZeroDivisionError`, `OverflowError`, `UnderflowError` (auto-imported) |
| `memory/` | `Memory<T>` compiler intrinsic for raw typed memory |
| `net/` | TCP socket wrappers, HTTP client/server, request/response parsing |
| `operators/` | `Comparable`, `Equatable`, `Hashable` for operator overloading |
| `os/` | Bare-metal OS primitives: arch, bus, crypto, debug, drivers, filesystems, HAL, IPC, memory, networking, power, scheduler, security, sync, syscalls, tasks, timers, VFS |
| `process/` | Multiprocessing with `ProcessPoolExecutor` and shared memory |
| `regexp/` | Backtracking regex engine with character classes, anchors, alternation, quantifiers |
| `string/` | `StringBuilder` for efficient mutable string construction |
| `subprocess/` | Process management via fork/execve/wait with pipe I/O (zero C runtime) |
| `testing/` | `assertTrue`, `assertFalse`, `assertEqualI64`, `assertEqualStr`, `TestCase`, `TestRunner` |
| `threading/` | `ThreadPoolExecutor`, `Mutex`, `RwLock`, `Atomic`, `Channel`, `Barrier`, `CondVar`, `Once` |
| `ipc/` | Inter-process communication via shared-memory message passing |
| `kernel/` | High-level kernel facade re-exporting OS subsystem modules |
| `security/` | Security primitives: capabilities, identity, handle management |
| `sys/` | System-level interfaces: process management, syscall wrappers, error types |
| `time/` | Timer utilities |
| `web/` | HTTP server framework with router, route matching, session management |
| `adelia/` | Color management library: RGB/HSL/HSV/CMYK, color distance, palette generation |

### Unified Error Hierarchy

The standard library provides a structured exception hierarchy rooted in a `Throwable` interface. `Error`, `Exception`, and `Warning` each implement `Throwable` directly. Domain-specific errors branch from `Error`: `LookupError` serves as the unifying base for `IndexError` and `KeyError`; `TypeError`, `ValueError`, `StateError`, `CapacityError`, and `NotImplementedError` each capture distinct failure modes. Arithmetic errors (`ZeroDivisionError`, `OverflowError`, `UnderflowError`) are auto-imported by the compiler and inserted as runtime safety checks before every division and modulo operation.

Unhandled exceptions print a full diagnostic: shadow call stack with file:line:column per frame, the exception type name and message, and a 3-line source context window highlighting the throw site.

```uranite
from uranite.collection.array-list import ArrayList

function getElement( ArrayList<I64> items, I64 index ) -> I64:
    try:
        return items.get( index )
    except IndexError error:
        return -1
```

### Additional Language Capabilities

- **Nullable types** with `?Type` syntax and `None` literal; `is None` / `is not None` checks
- **Pattern matching** via `match` expressions and statements with enum variant dispatch
- **Inline assembly** using `asm volatile` with full LLVM constraint syntax
- **C FFI** through `extern function` declarations
- **`addressof` operator** returning the raw `I64` address of any expression
- **Comprehension expressions** for inline collection construction
- **Range expressions** with `..` (exclusive) and `...` (inclusive)

---

## Compiler Architecture & Pipeline State

### Active Compilation Pipeline

The production pipeline compiles `.urn` source files through the following stages:

```
Source (.urn)
    |
    v
  Lexer ---- INDENT/DEDENT token emission (Python-style block structure)
    |
    v
  Parser --- Recursive descent, builds AST
    |
    v
  Semantic - Two-pass: register type declarations, then type-check bodies
    |
    v
  Borrow --- Ownership validation, move tracking, use-after-move detection
    |
    v
  Optimizer  AST-level: constant folding, DCE, strength reduction, tail-call optimization
    |
    v
  Codegen -- LLVM IR generation (~9320 lines)
    |
    v
  Linker --- Links runtime libraries, produces native executable
```

The pipeline is orchestrated by `compiler::Driver` in `src/uranite/compiler/driver.cpp`. Six static C11 runtime libraries link automatically: exception handling (shadow call stack, unhandled exception reporting), async event loop (legacy, kept for backwards compatibility), thread spawning (pthread), subprocess management (fork/exec), IPC (shared memory), and FFI (dynamic library loading).

| Binary | Purpose |
|--------|---------|
| `uranite` | Compiler: `.urn` source to native executable |
| `uranite-tests` | GoogleTest suite for compiler internals |
| `uranite-fmt` | Code formatter with comment preservation and style linting |
| `uranite-doc` | Documentation generator from `"""..."""` doccomments |
| `uranite-pkg` | Package manager with dependency resolution and semantic versioning |

### HIR and MIR Pipeline

A high-level IR pipeline is implemented with an experimental MIR→LLVM code generation path. HIR/MIR stages run when `--verbose`, `--dump-hir`, `--dump-mir`, or `--use-mir` flags are active. The `--use-mir` flag enables MIR-based code generation, bypassing the AST-direct codegen path entirely.

```
AST → HIR Lowering → HIR Validation → MIR Lowering (CFG)
    → MIR Liveness Analysis → MIR Borrow Checker → MIR Optimization
    → MIR → LLVM Codegen (--use-mir)
```

- **HIR** (High-Level IR) preserves high-level semantics (loops, match, classes) with resolved types. Desugars elif chains to nested conditionals and unifies all loop forms. 60 node types. Accessible via `--dump-hir`.
- **MIR** (Mid-Level IR) flattens the HIR tree into a control-flow graph of basic blocks with linear instruction sequences. Each block terminates with exactly one control flow instruction. Accessible via `--dump-mir`.
- **MIR Analysis** provides backward dataflow liveness analysis (fixed-point iteration over def/use sets), forward dataflow ownership verification (use-after-move and double-free detection), and five optimization passes (dead store elimination, copy propagation, constant folding, block merging, unreachable block elimination).
- **MIR Codegen** (experimental, `--use-mir`) generates LLVM IR from MIR. Passes 11/11 benchmarks and 164/203 module integration tests. Runtime-verified for functions, recursion, loops, conditionals, boolean logic, bitwise ops, float arithmetic, extern calls, division/modulo, class constructors, method calls, and field access. OOP wrapper classes not yet supported on this path.

---

## Verification & Testing Status

The repository maintains a rigorous two-tier testing strategy. Both tiers achieve a **100% pass rate** with zero known failures or regressions.

### GoogleTest Suite

152 unit tests across 12 test suites covering lexer, parser, semantic analysis, type system, borrow checker, optimizer, generics, integration, HIR lowering, MIR lowering, and MIR analysis. All 152 tests pass.

```bash
./build/uranite-tests
# [==========] 152 tests from 12 test suites ran.
# [  PASSED  ] 152 tests.

# Run a single test
./build/uranite-tests --gtest_filter="HIRLoweringTest.LowersSimpleFunction"
```

### Module Integration Tests

212 `.urn` source files in `testing/` exercise every major language feature end-to-end: control flow, object dispatch, enums, generics, collections, borrow-checker validations, exception handling, inline assembly, async/await, module imports, CLI argument handling, HTTP networking, cryptography, threading, and subprocess management. All 212 compile and run successfully with zero runtime failures, completely clearing all legacy regressions.

```bash
# Compile all module tests
for f in testing/test-*.urn; do
    timeout 10 ./build/uranite "$f" 2>&1 | tail -1
done

# Runtime verification (compile + execute + check exit code)
for f in testing/test-*.urn; do
    name=$(basename "$f" .urn)
    timeout 10 ./build/uranite "$f" -o /tmp/ae_test 2>/dev/null
    if [ $? -ne 0 ]; then echo "COMPILE FAIL: $name"; continue; fi
    timeout 10 /tmp/ae_test > /dev/null 2>&1
    if [ $? -ne 0 ]; then echo "RUNTIME FAIL: $name"; fi
done
```

---

## Repository Development Infrastructure

### Building from Source

**Prerequisites:** C++17 compiler (GCC 12+ or Clang 19+), LLVM 19 (via `llvm-config`), CMake 3.22+, fmt, spdlog, argparse, GoogleTest, pthread (thread runtime), rt (IPC runtime), dl (FFI runtime).

```bash
# Debian/Ubuntu
sudo apt-get install -y llvm-19-dev libfmt-dev libspdlog-dev libgtest-dev cmake g++

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
make -C build -j$(nproc)
```

Produces `./build/uranite`, `./build/uranite-tests`, `./build/uranite-fmt`, `./build/uranite-doc`, and `./build/uranite-pkg`.

### CLI Usage

```bash
# Compile to executable
./build/uranite source.urn -o program

# Compile and run immediately
./build/uranite -r source.urn

# Emit LLVM IR
./build/uranite source.urn --emit-llvm -o output.ll

# Diagnostic dump modes
./build/uranite source.urn --dump-tokens
./build/uranite source.urn --dump-ast
./build/uranite source.urn --dump-hir
./build/uranite source.urn --dump-mir

# Verbose compilation (shows each pipeline stage including MIR analysis)
./build/uranite source.urn --verbose -o program
```

---

## Language Comparison Matrix

To understand where Uranite stands in the modern engineering landscape, here is an objective, factual architectural comparison against existing ecosystems. Uranite uniquely bridges the gap by combining **bare-metal LLVM performance and compile-time ownership safety** with the **clean developer ergonomics of indentation-based languages**.

| Language | Compilation / Runtime Type | Memory Management Model | Syntax Style | Error Handling Paradigm | Type System Safety | Developer Ergonomics |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Uranite** | **AOT Native (LLVM 19)** | **Ownership & Move (No GC, No Lifetime Tags)** | **Indentation-Based** | **Strict Exception Hierarchy (`LookupError`)** | **Static (Fully Qualified Name Check)** | **High (Pythonic/Frictionless)** |
| **C / C++** | AOT Native | Manual / RAII | Curly Braces | Error Codes / Uncaught Exceptions | Static (Weak Name Resolution Danger) | Low to Medium (Verbose) |
| **C#** | Managed (CLR VM / JIT) | Garbage Collection (GC) | Curly Braces | Structured Exceptions | Static | High |
| **Go** | AOT Native | Garbage Collection (GC) | Curly Braces | Explicit Multi-Value Returns | Static | Medium to High |
| **Java** | Managed (JVM / JIT) | Garbage Collection (GC) | Curly Braces | Checked & Unchecked Exceptions | Static | Medium (Ceremony-Heavy) |
| **JavaScript**| Interpreted / JIT | Garbage Collection (GC) | Curly Braces | Dynamic Exceptions | Dynamic | High |
| **Nim** | AOT (via C/C++ backend) | Destructors / ARC / ORC | Indentation-Based | Exceptions | Static | High |
| **PHP** | Interpreted / JIT | GC (Reference Counting) | Curly Braces | Exceptions / Mixed Errors | Dynamic / Gradual | High |
| **Python** | Interpreted / JIT (CPython) | GC (Ref Counting + Cycle Detector) | Indentation-Based | Dynamic Exceptions | Dynamic | Absolute High |
| **Rust** | AOT Native (LLVM) | Ownership & Move (With Lifetime Tags) | Curly Braces | Explicit `Result<T, E>` / Monadic | Static (Rigorous Structural) | Medium (Steep Learning Curve) |
| **TypeScript**| Transpiled to JS | Garbage Collection (Runtime GC) | Curly Braces | Runtime Exceptions | Static (Structural Compile-Time) | High |
| **Zig** | AOT Native | Manual (Explicit Allocator Passing) | Curly Braces | Error Return Statuses | Static | Medium |

---

## Key Architectural Distinctions

* **Uranite vs. Python / Nim**: While sharing the clean, high-productivity aesthetic of indentation-based code blocks, Uranite executes directly on bare metal via LLVM with zero garbage collection overhead, unlike Python (VM-bound) or Nim (historically reliant on runtime memory management strategies).
* **Uranite vs. Rust**: Uranite enforces safe move semantics and strict ownership verification at compile time to eliminate double-free bugs. However, it completely bypasses Rust's verbose and steep compile-time lifetime annotation syntax through its unique internal semantic tracking engine.
* **Uranite vs. C++ / Zig**: Uranite guarantees out-of-the-box memory safety without forcing the engineer to manually manage pointers or explicitly pass allocators through every single call stack level, while enforcing rigid type validation via Fully Qualified Names (`qualname`) to avoid structural type confusion.
* **Uranite vs. Managed Languages (Go, Java, C#)**: Uranite eliminates runtime jitter, garbage collection pauses, and fat execution binaries by shipping a zero-C-runtime native footprint suited for high-throughput, predictable production systems.

---

## Warning

**Uranite is a research-oriented compiler in active development.** It has not been audited for security, memory safety, or performance stability. Do not use it for production systems. Syntax and APIs are subject to breaking changes without notice. Generated code may contain bugs or undefined behavior.

---

## Issues

If you encounter crashes, incorrect LLVM IR, or unexpected behavior:

1. Search the [Issue Tracker](https://github.com/uranite-lang/uranite/issues).
2. [Open a New Issue](https://github.com/uranite-lang/uranite/issues/new) with your environment (OS, LLVM version), a minimal `.urn` snippet, and expected vs actual output.

---

## Support

Contributions of any size are appreciated.
[paypal.me/hxAri](https://paypal.me/hxAri)

---

## Licence

All **[Uranite](https://github.com/uranite-lang/uranite)** source code is licensed under the [GNU General Public License v3](https://www.gnu.org/licenses/gpl-3.0).
