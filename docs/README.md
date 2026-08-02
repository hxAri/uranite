# Uranite Programming Language

**Accelerated Execution Through Quantum Precision**

Uranite is a compiled, indentation-based, ownership-aware programming language that combines the readability of Python-style syntax with the performance and memory safety of systems languages. It compiles to native executables, delivers deterministic memory management through move semantics and borrow checking, and ships with a comprehensive standard library spanning collections, concurrency, networking, cryptography, and bare-metal system access.

---

## Table of Contents
- [Uranite Programming Language](#uranite-programming-language)
  - [Table of Contents](#table-of-contents)
  - [Core Philosophy](#core-philosophy)
    - [Readability is non-negotiable](#readability-is-non-negotiable)
    - [Memory safety without garbage collection](#memory-safety-without-garbage-collection)
    - [Native performance is the baseline](#native-performance-is-the-baseline)
    - [The standard library is the platform](#the-standard-library-is-the-platform)
  - [Key Features](#key-features)
    - [Type System](#type-system)
    - [Memory Model](#memory-model)
    - [Concurrency](#concurrency)
    - [Collections](#collections)
    - [Error Handling](#error-handling)
    - [Pattern Matching](#pattern-matching)
    - [Inline Assembly and Unsafe Code](#inline-assembly-and-unsafe-code)
    - [Interop](#interop)
  - [Quick Start](#quick-start)
  - [At a Glance](#at-a-glance)
  - [Documentation Map](#documentation-map)
    - [Getting Started](#getting-started)
    - [Language Syntax](#language-syntax)
    - [Standard Library Guide](#standard-library-guide)
    - [Toolchains](#toolchains)
    - [Compiler Internals](#compiler-internals)
  - [Standard Library Modules](#standard-library-modules)
  - [License](#license)

## Core Philosophy

### Readability is non-negotiable

Indentation-based blocks replace braces. Keyword-driven logic (`and`, `or`, `not`, `is`) replaces symbolic operators for boolean operations. Descriptive naming conventions are enforced by the formatter. Source code reads as a description of its own intent:

```uranite
public function processOrders( ArrayList<Order> orders ) -> I64:
    I64 totalValue = 0
    for Order order in orders:
        if order.status is not None and order.isPaid:
            totalValue = totalValue + order.amount
    return totalValue
```

No braces, no semicolons, no `&&` or `||`.

### Memory safety without garbage collection

Every value in Uranite has exactly one owner. Assignment transfers ownership by default. The compiler verifies at compile time that no value is used after it has been moved, no mutable reference coexists with other references, and every allocation is freed exactly once.

```uranite
ArrayList<String> original = new ArrayList<>()
original.add( "alpha" )
original.add( "beta" )

ArrayList<String> transferred = original
```

After the last line, `original` is invalidated. Any subsequent access to `original` produces a compile-time error. There is no garbage collector, no reference counting, and no runtime pause for memory reclamation.

### Native performance is the baseline

Uranite compiles to optimized native code with configurable optimization levels from debug builds through maximum throughput. The compiler inserts arithmetic overflow checks for integer operations and zero-division guards for division and modulo, while preserving IEEE 754 compliance for floating-point operations.

### The standard library is the platform

Uranite ships over 30 standard library modules covering everything from `ArrayList<E>` and `HashMap<K,V>` to raw Linux syscalls, epoll-based async I/O, POSIX threading, AES-256 encryption, HTTP clients, regular expressions, and process management. The async runtime is written entirely in pure Uranite with zero dependency on the C runtime. Standard library modules are first-class Uranite code, not bindings to external C libraries.

---

## Key Features

### Type System

Static typing with full type inference at construction sites via diamond syntax (`new ArrayList<>()`). Generics with type parameters and `where` clause constraints. Single inheritance for classes with multiple interface implementation. Traits for horizontal code reuse. Abstract classes for partial implementations. Backed enums with explicit discriminant values.

### Memory Model

Move-by-default ownership with compile-time borrow checking. Intrinsic `Memory<T>` for raw heap allocation. `Arena` allocator for batch allocation with single-point deallocation. `Slab` allocator for fixed-size object pools. Explicit `delete` for manual deallocation. `Droper` interface for custom cleanup logic. `unsafe` blocks for bypassing borrow checker constraints when necessary.

### Concurrency

`async`/`await` with `Future<T>` return types. Pure-Uranite async runtime built on Linux epoll with zero C runtime dependency. Coroutines and fibers for lightweight cooperative scheduling. POSIX thread wrappers with `Mutex`, `RwLock`, `Barrier`, `Atomic`, `CondVar`, and `Channel` primitives. `ThreadPool` for managed worker pools. `ScopedThread` for RAII-bound thread lifetimes.

### Collections

`ArrayList<E>`, `HashMap<K,V>`, `HashSet<E>`, `Tuple<E>`, `Generator<E>`, `Pair<K,V>`. Collection literals: `[1, 2, 3]` for arrays, `{"key": "value"}` for maps, `{1, 2, 3}` for sets. Comprehensions: `[x * 2 for I64 x in 0..10]`.

### Error Handling

Structured exception handling with `try`/`except`/`finally` blocks, typed `except` clauses for discriminating exception types, and `raise` for throwing. `defer` statements for guaranteed cleanup regardless of control flow. Exception specification via `raises` keyword on function signatures.

### Pattern Matching

`match` expressions for inline value mapping with `=>` arms and wildcard `*` fallback. `match` statements for multi-line block-bodied arms. `switch`/`case` statements for exhaustive integer and enum dispatch.

### Inline Assembly and Unsafe Code

`assembly` blocks for embedding platform-specific instructions. `unsafe` blocks for operations that bypass the borrow checker. `addressof` operator for obtaining raw pointers. `volatile` qualifier for memory-mapped I/O access.

### Interop

FFI module for calling C functions from shared libraries. `-l` linker flag for native library linking. Cross-compilation via `--target` triple. `extern` declarations for importing foreign function signatures.

---

## Quick Start

Install dependencies and build:

```bash
sudo apt install llvm-19-dev libfmt-dev libspdlog-dev cmake g++
git clone https://github.com/uranite-lang/uranite.git
cd uranite
cmake -B build -DCMAKE_BUILD_TYPE=Release
make -C build -j$(nproc)
```

Write your first program:

```uranite
package hello.world

from uranite.io.console import puts
from uranite.collection.array-list import ArrayList

public function main() -> I32:
    ArrayList<String> greetings = new ArrayList<>()
    greetings.add( "Hello" )
    greetings.add( "from" )
    greetings.add( "Uranite" )
    for String word in greetings:
        puts( word )
    return 0
```

Compile and run:

```bash
./build/uranite hello.urn -o hello
./hello
```

Or compile and run in one step:

```bash
./build/uranite -r hello.urn
```

---

## At a Glance

```uranite
package showcase

from uranite.io.console import puts
from uranite.collection.array-list import ArrayList
from uranite.collection.hash-map import HashMap
from uranite.errors.exception import Exception

enum Direction:
    unit North
    unit South
    unit East
    unit West

class Sensor:

    public String location
    public F64 reading

    public function Sensor( self, String location, F64 reading ) -> Void:
        self.location = location
        self.reading = reading

    public function describe( self ) -> String:
        return self.location + ": " + self.reading.toString()

interface Measurable:
    public function measure( self ) -> F64;

class TemperatureSensor extends Sensor implements Measurable:

    public function TemperatureSensor( self, String location, F64 reading ) -> Void:
        parent( location, reading )

    public override function measure( self ) -> F64:
        return self.reading * 1.8 + 32.0

async function fetchReadings( ArrayList<Sensor> sensors ) -> Future<ArrayList<String>>:
    ArrayList<String> results = new ArrayList<>()
    for Sensor sensor in sensors:
        results.add( sensor.describe() )
    return results

public function main() -> I32:
    TemperatureSensor kitchen = new TemperatureSensor( "kitchen", 22.5 )
    TemperatureSensor garage = new TemperatureSensor( "garage", 18.0 )

    F64 kitchenFahrenheit = kitchen.measure()
    puts( "Kitchen:", kitchenFahrenheit, "F" )

    String label = match kitchenFahrenheit in 72.5 => "perfect", * => "other"
    puts( label )

    HashMap<String, F64> readings = new HashMap<>()
    readings.put( kitchen.location, kitchen.reading )
    readings.put( garage.location, garage.reading )

    for String location, F64 temperature in readings:
        puts( location, "=", temperature )

    try:
        I64 result = 10 / 0
    except Exception as error:
        puts( "Caught:", error.message )
    finally:
        puts( "Cleanup complete" )

    return 0
```

This program demonstrates indentation-based blocks, classes with inheritance, interface implementation with `override`, `async` functions returning `Future<T>`, `match` expressions, `HashMap` iteration with key-value destructuring, `try`/`except`/`finally` error handling, and the `parent()` constructor delegation pattern.

---

## Documentation Map

### [Getting Started](getting-started/README.md)

Installation on Linux, macOS, and Windows. Platform support and architecture compatibility. Building the compiler and toolchain. Your first Uranite program from source file to native executable.

### [Language Syntax](syntax/README.md)

Complete language reference organized by topic. Covers lexical conventions, the type system, variables and constants, operators, expressions, control flow, functions, classes, structs, interfaces, traits, abstract classes, generics, enums, collections, memory and ownership, error handling, async and concurrency, modules and packages, inline assembly, and foreign function interop. Each topic includes grammar rules, semantic behavior, and working code examples.

### [Standard Library Guide](stdlib-guide/README.md)

Practical usage guides for the core standard library modules. Collections, strings, memory management, console and file I/O, error handling patterns, async programming, threading, testing, math, date and time, cryptography, networking, encoding, regular expressions, OS primitives, and FFI. Not an API reference (generate that with `uranite-doc`), but a guide to writing idiomatic Uranite.

### [Toolchains](toolchains/README.md)

Usage guides and flag references for the official Uranite development tools:

| Tool | Binary | Purpose |
|---|---|---|
| Compiler | `uranite` | Compile `.urn` source to native executables or object files |
| Formatter | `uranite-fmt` | Format and lint Uranite source files to canonical style |
| Package Manager | `uranite-pkg` | Project scaffolding, dependency resolution, and build orchestration |
| Doc Generator | `uranite-doc` | Extract doccomments into Markdown or HTML documentation |

### [Compiler Internals](internals/README.md)

Contributor guide to the compiler architecture and development conventions. Intended for developers working on the compiler itself, not for end-users writing Uranite programs.

---

## Standard Library Modules

Uranite ships standard library modules under the `uranite.*` namespace:

| Module | Description |
|---|---|
| `uranite.collection` | ArrayList, HashMap, HashSet, Tuple, Generator, Pair, Sequence interfaces |
| `uranite.language` | OOP wrappers for primitives (String, I8-I64, U8-U64, F32, F64, Boolean, Char) |
| `uranite.memory` | Memory intrinsic, Arena allocator, Slab allocator, Object base class |
| `uranite.io` | Console I/O, file reading/writing, buffered streams, filesystem operations |
| `uranite.errors` | Exception, Error, Warning, ValueError, IndexError, StateError, TypeError |
| `uranite.string` | String builder, string formatting utilities |
| `uranite.iterators` | Iterable and Iterator interfaces |
| `uranite.operators` | Addable, Subtractable, Equatable, Comparable, and other operator interfaces |
| `uranite.async` | Async runtime, epoll event loop, task scheduler, timers |
| `uranite.threading` | Thread, Mutex, RwLock, Barrier, Atomic, Channel, ThreadPool, CondVar |
| `uranite.coroutine` | Lightweight coroutines and cooperative scheduling |
| `uranite.fiber` | Fiber-based task scheduling and context switching |
| `uranite.testing` | Test runner, assertions (assertTrue, assertEqualI64, assertEqualString) |
| `uranite.math` | Mathematical functions and constants |
| `uranite.datetime` | Date, time, duration, and monotonic clock |
| `uranite.time` | Low-level time primitives and clock access |
| `uranite.crypto` | Cryptographic primitives (hashing, encryption, key derivation) |
| `uranite.net` | Networking primitives (sockets, addresses, protocols) |
| `uranite.encoding` | JSON, Base64, hex, and other encoding formats |
| `uranite.regexp` | Regular expression compilation and matching |
| `uranite.os` | OS-level primitives, environment variables, signal handling |
| `uranite.kernel` | Raw syscalls, hardware access, kernel interface |
| `uranite.sys` | System information and resource inspection |
| `uranite.ffi` | Foreign function interface for C interop |
| `uranite.cli` | Command-line argument parsing and help generation |
| `uranite.logging` | Structured logging with severity levels |
| `uranite.subprocess` | Process spawning with pipe redirection |
| `uranite.process` | Process management and lifecycle control |
| `uranite.ipc` | Inter-process communication primitives |
| `uranite.compression` | Data compression and decompression |
| `uranite.convert` | Type conversion utilities |
| `uranite.functions` | Higher-order function utilities |
| `uranite.requests` | HTTP client for web requests |
| `uranite.security` | Security primitives and access control |
| `uranite.web` | Web server and request handling |
| `uranite.adelia` | Application-level libraries |

---

## License

Uranite is free software distributed under the [GNU General Public License v3.0](https://www.gnu.org/licenses/gpl-3.0.html).

Copyright (c) 2025 - hxAri
