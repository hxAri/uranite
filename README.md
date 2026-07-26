
<!--
@author hxAri (hxari)
@create 2025-02-24 15:15
@update 2026-07-26 23:30
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

# Uranite

**Accelerated Execution Through Quantum Precision**

A low-level, high-productivity system programming language built on top of a C++17 and LLVM 19 backend.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

## Table of Contents
- [Uranite](#uranite)
  - [Table of Contents](#table-of-contents)
  - [Language Overview \& Philosophy](#language-overview--philosophy)
  - [Compiler Architecture \& Pipeline](#compiler-architecture--pipeline)
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
./build/uranite --run --verbose hello.urn
# Hello, Uranite!
```

## Compiler Architecture & Pipeline

The compiler pipeline processes `.urn` source files through a multi-stage IR architecture:

```
Source (.urn)
    |
    v
  Lexer --------- INDENT/DEDENT token emission (Python-style block structure)
    |
    v
  Parser -------- Recursive descent, builds AST
    |
    v
  Semantic ------ Two-pass: register type declarations, then type-check bodies
    |
    v
  Borrow Check -- Ownership validation, move tracking, use-after-move detection
    |
    v
  HIR Lowering -- AST → High-Level IR with resolved types
    |
    v
  HIR Validation  Structural correctness checks
    |
    v
  MIR Lowering -- HIR → Control-Flow Graph of basic blocks
    |
    v
  MIR Analysis -- Liveness analysis, borrow checking, optimization
    |
    v
  MIR Codegen --- MIR → LLVM IR generation
    |
    v
  Linker -------- Links runtime libraries, produces native executable
```

The pipeline is orchestrated by `compiler::Driver` in `src/uranite/compiler/driver.cpp`. Six static C11 runtime libraries link automatically: exception handling (shadow call stack, unhandled exception reporting), async event loop (legacy, kept for backwards compatibility), thread spawning (pthread), subprocess management (fork/exec), IPC (shared memory), and FFI (dynamic library loading).

| Binary | Purpose |
|--------|---------|
| `uranite` | Compiler: `.urn` source to native executable |
| `uranite-tests` | GoogleTest suite for compiler internals |
| `uranite-fmt` | Code formatter with comment preservation and style linting |
| `uranite-doc` | Documentation generator from `"""..."""` doccomments |
| `uranite-pkg` | Package manager with dependency resolution and semantic versioning |

- **HIR** (High-Level IR) preserves high-level semantics (loops, match, classes) with resolved types. Desugars elif chains to nested conditionals and unifies all loop forms. 60 node types. Accessible via `--dump-hir`.
- **MIR** (Mid-Level IR) flattens the HIR tree into a control-flow graph of basic blocks with linear instruction sequences. Each block terminates with exactly one control flow instruction. Accessible via `--dump-mir`.
- **MIR Analysis** provides backward dataflow liveness analysis (fixed-point iteration over def/use sets), forward dataflow ownership verification (use-after-move and double-free detection), and five optimization passes (dead store elimination, copy propagation, constant folding, block merging, unreachable block elimination).
- **MIR Codegen** generates LLVM IR from MIR. Runtime-verified for functions, recursion, loops, conditionals, boolean logic, bitwise ops, float arithmetic, extern calls, division/modulo, class constructors, method calls, field access, OOP wrapper classes (via shared descriptor registry), virtual dispatch via interface tables, exception handling with shadow call stack, defer statements, keyword arguments, variadic parameters, generics, enums, async/await, and inline assembly.

## Repository Development Infrastructure

### Building from Source

**Prerequisites:** C++17 compiler (GCC 12+ or Clang 19+), LLVM 19 (via `llvm-config`), CMake 3.22+, fmt, spdlog, argparse, GoogleTest, pthread (thread runtime), rt (IPC runtime), dl (FFI runtime).

```bash
sudo apt install -y \
  g++ \
  cmake \
  llvm-19-dev \
  libfmt-dev \
  libspdlog-dev \
  libgtest-dev

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

## Key Architectural Distinctions

* **Uranite vs. Python / Nim**: While sharing the clean, high-productivity aesthetic of indentation-based code blocks, Uranite executes directly on bare metal via LLVM with zero garbage collection overhead, unlike Python (VM-bound) or Nim (historically reliant on runtime memory management strategies).
* **Uranite vs. Rust**: Uranite enforces safe move semantics and strict ownership verification at compile time to eliminate double-free bugs. However, it completely bypasses Rust's verbose and steep compile-time lifetime annotation syntax through its unique internal semantic tracking engine.
* **Uranite vs. C++ / Zig**: Uranite guarantees out-of-the-box memory safety without forcing the engineer to manually manage pointers or explicitly pass allocators through every single call stack level, while enforcing rigid type validation via Fully Qualified Names (`qualname`) to avoid structural type confusion.
* **Uranite vs. Managed Languages (Go, Java, C#)**: Uranite eliminates runtime jitter, garbage collection pauses, and fat execution binaries by shipping a zero-C-runtime native footprint suited for high-throughput, predictable production systems.

## Warning

**Uranite is a research-oriented compiler in active development.** It has not been audited for security, memory safety, or performance stability. Do not use it for production systems. Syntax and APIs are subject to breaking changes without notice. Generated code may contain bugs or undefined behavior.

## Issues

If you encounter crashes, incorrect LLVM IR, or unexpected behavior:

1. Search the [Issue Tracker](https://github.com/uranite-lang/uranite/issues).
2. [Open a New Issue](https://github.com/uranite-lang/uranite/issues/new) with your environment (OS, LLVM version), a minimal `.urn` snippet, and expected vs actual output.

## Support

Contributions of any size are appreciated.
[paypal.me/hxAri](https://paypal.me/hxAri)

## Licence

All **[Uranite](https://github.com/uranite-lang/uranite)** source code is licensed under the [GNU General Public License v3](https://www.gnu.org/licenses/gpl-3.0).
