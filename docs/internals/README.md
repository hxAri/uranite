# Compiler Internals

A contributor guide to the Uranite compiler architecture. This document covers the full compilation pipeline, the major subsystems, and the development conventions enforced across the C++ codebase.

---

## Table of Contents

- [Compilation Pipeline](#compilation-pipeline)
- [Source Layout](#source-layout)
- [Lexer](#lexer)
- [Parser](#parser)
- [Semantic Analyzer](#semantic-analyzer)
  - [Two-Pass Analysis](#two-pass-analysis)
  - [Type Registry](#type-registry)
- [Borrow Checker](#borrow-checker)
- [HIR (High-Level IR)](#hir-high-level-ir)
  - [HIR Lowering](#hir-lowering)
  - [HIR Validation](#hir-validation)
- [MIR (Mid-Level IR)](#mir-mid-level-ir)
  - [MIR Lowering](#mir-lowering)
  - [MIR Liveness and Borrow Checking](#mir-liveness-and-borrow-checking)
  - [MIR Optimization](#mir-optimization)
  - [MIR Codegen](#mir-codegen)
- [Runtime](#runtime)
  - [Exception Runtime and Shadow Stack](#exception-runtime-and-shadow-stack)
  - [C Runtime Libraries](#c-runtime-libraries)
  - [Async Runtime](#async-runtime)
- [Module System](#module-system)
- [Qualnames Registry](#qualnames-registry)
- [Coding Conventions](#coding-conventions)
  - [C++ Style Rules](#c-style-rules)
  - [Uranite Source Style](#uranite-source-style)
- [Building and Testing](#building-and-testing)

---

## Compilation Pipeline

```
Source (.urn)
    │
    ▼
  Lexer ─────────── Token stream
    │
    ▼
  Parser ────────── AST (Abstract Syntax Tree)
    │
    ▼
  Semantic Analyzer ─ Type-annotated AST (two-pass: registration + validation)
    │
    ▼
  Borrow Checker ── Ownership verification
    │
    ▼
  HIR Lowering ──── HIR (High-Level IR: preserves loops, match, classes)
    │
    ▼
  HIR Validation ── Structural correctness checks
    │
    ▼
  MIR Lowering ──── MIR (Mid-Level IR: flattened CFG, linear instructions)
    │
    ▼
  MIR Liveness ──── Variable liveness analysis
    │
    ▼
  MIR Borrow Check  Ownership rules on MIR
    │
    ▼
  MIR Optimization   Dead code, constant folding at MIR level
    │
    ▼
  MIR Codegen ────── LLVM IR generation
    │
    ▼
  LLVM Backend ──── Object code (with LLVM optimization passes -O0 through -Ofast)
    │
    ▼
  Linker ──────────  Native executable
```

The MIR path is the production pipeline. The AST-direct codegen (`codegen/codegen.cpp`) and AST optimizer (`optimizer/optimizer.cpp`) are legacy and strictly bypassed.

---

## Source Layout

```
src/uranite/
├── ast/             # AST node definitions
├── backend/         # LLVM backend utilities
├── codegen/         # Legacy AST-direct codegen (bypassed)
├── common/          # Shared utilities
├── compiler/        # Driver: orchestrates the full pipeline
├── descriptor/      # Builtin type descriptors (OOP wrappers)
├── diagnostic/      # Error reporting and diagnostics
├── ir/
│   ├── hir/         # HIR nodes, lowering, validation
│   └── mir/         # MIR nodes, lowering, codegen, borrow, liveness
├── lexer/           # Tokenization
├── lookup/          # Source location tracking
├── optimizer/       # Legacy AST optimizer (bypassed)
├── parser/          # Recursive descent parser
├── semantic/        # Semantic analysis, type system, borrow checker
│   ├── analyzer.cpp # Two-pass semantic analyzer (~5.4k lines)
│   ├── typeref.cpp  # Type reference and registry
│   ├── qualnames.hpp# Centralized name registry
│   └── borrow.cpp   # AST-level borrow checking
├── token/           # Token types and definitions
└── visitors/        # AST visitor infrastructure
```

The largest files by line count:

| File | Lines | Role |
|---|---|---|
| `ir/mir/codegen.cpp` | ~7,900 | MIR to LLVM IR codegen |
| `semantic/analyzer.cpp` | ~5,400 | Two-pass semantic analysis |
| `ir/mir/lowering.cpp` | ~4,000 | AST/HIR to MIR lowering |
| `parser/parser.cpp` | ~3,000 | Recursive descent parser |
| `ir/hir/lowering.cpp` | ~1,300 | AST to HIR lowering |

---

## Lexer

**File:** `src/uranite/lexer/lexer.cpp`

Converts source text into a flat token stream. Handles indentation tracking for block structure. Token types are defined in `src/uranite/token/token.hpp`.

Key tokens: `LeftBrace`/`RightBrace` (used for `export {}`, `import {}`, kwargs markers), `Colon` (block start), `Indent`/`Dedent` (block boundaries).

---

## Parser

**File:** `src/uranite/parser/parser.cpp`

Recursive descent parser producing an AST. Node types are defined in `src/uranite/ast/node.hpp`.

Notable parsing responsibilities:
- Indentation-based block parsing (colon + indent / dedent)
- Expression parsing with operator precedence
- Collection literals: `[...]` (array), `{k: v}` (map), `{v}` (set)
- Comprehension expressions: `[expr for var in iter]`
- Generic type parameters: `ClassName<TypeArg>`
- Backed enum variant parsing: `unit Name value`

---

## Semantic Analyzer

**File:** `src/uranite/semantic/analyzer.cpp`

### Two-Pass Analysis

**Pass 1 — Registration (`analyzeModuleRegistration`):** Eagerly registers all type declarations, class definitions, function signatures, and symbols per module. This allows forward references and cross-module imports.

**Pass 2 — Validation:** Full type checking, generic substitution, operator interface resolution, method dispatch validation, and expression type inference.

### Type Registry

**File:** `src/uranite/semantic/typeref.cpp`, `src/uranite/semantic/typeref.hpp`

The `Registry` maintains two maps:
1. `userTypesType` — user-defined types (classes, interfaces, enums, structs). Checked first.
2. `primitivesTypes` — built-in primitive types. Fallback.

**Critical invariant:** Always compare types using `->qualified` (the fully-qualified name from `qualnames.hpp`), never `->name` (the short name). Short names can collide across packages.

---

## Borrow Checker

**File:** `src/uranite/semantic/borrow.cpp`

Operates on the type-annotated AST. Tracks ownership transfers, validates that moved variables are not used after the move, and ensures mutable borrows are exclusive.

A second borrow check pass runs at the MIR level (`ir/mir/borrow.cpp`) for additional precision after control flow is linearized.

---

## HIR (High-Level IR)

**Files:** `src/uranite/ir/hir.hpp`, `src/uranite/ir/hir/lowering.cpp`

HIR preserves high-level semantics: loops, match expressions, class method bodies, and resolved type information. It bridges the gap between the AST (which mirrors source syntax) and MIR (which mirrors machine execution).

### HIR Lowering

Converts the type-annotated AST into HIR nodes. Each AST expression and statement has a corresponding HIR representation with all type information resolved.

### HIR Validation

**File:** `src/uranite/ir/hir/validator.cpp`

Structural correctness checks on HIR: verifies that all branches terminate, types are consistent, and required fields are present.

---

## MIR (Mid-Level IR)

**Files:** `src/uranite/ir/mir.hpp`, `src/uranite/ir/mir/lowering.cpp`, `src/uranite/ir/mir/codegen.cpp`

MIR flattens the control flow into a CFG with basic blocks. Each block contains a linear sequence of instructions and ends with a terminator (branch, return, unreachable). Loops are desugared into header/body/update/exit blocks.

### MIR Lowering

Transforms HIR into MIR instruction sequences. Key responsibilities:
- Loop desugaring (while, for-in, for-range into CFG blocks)
- Collection literal desugaring (array/map/set literals into constructor + method call sequences)
- Operator desugaring (maps `+` to `Addable.add()`, `==` to `Equatable.equals()`, etc.)
- Variable allocation and SSA-like variable identifiers

Each MIR variable has a `MIRVariableDescriptor` in the function's `variableDescriptorTable` containing:
- `variableName` — source-level name
- `variableType` — semantic type (`semantic::TypeSharedPointer`), including generic parameter information

### MIR Liveness and Borrow Checking

Liveness analysis determines which variables are live at each program point. The MIR borrow checker uses this to verify ownership rules on the linearized control flow.

### MIR Optimization

Dead code elimination and constant folding at the MIR level, before LLVM IR generation.

### MIR Codegen

**File:** `src/uranite/ir/mir/codegen.cpp` (~7,900 lines)

The production codegen path. Maps MIR instructions to LLVM IR using the LLVM C++ API. Major subsystems:

**Eager pre-registration:** All classes and functions from the merged declaration list are pre-registered before any codegen begins. This ensures forward references resolve correctly.

**OOP wrapper handling:** Primitive types (I64, String, Boolean, etc.) have OOP wrapper classes defined in `stdlibs/language/`. The codegen intercepts method calls on these wrappers and emits inline LLVM IR rather than function calls (e.g., `I64.add()` becomes an `add` instruction).

**Operator interface dispatch:** Operators are resolved through interfaces (Addable, Subtractable, Equatable, Comparable, etc.). The codegen checks the concrete type and emits the appropriate LLVM instructions.

**Generic parameter handling:** Generic type parameters compile to `ptr` (opaque pointer) in LLVM IR. Methods on generic containers are **not monomorphized** — one shared function handles all instantiations. Primitive values in generic containers are stored via `inttoptr(value to ptr)`. The `variableDescriptorTable` preserves the semantic type for runtime conversion decisions.

**Virtual dispatch:** Interface method calls use an itable (interface table) for virtual dispatch. The codegen generates itable entries during class pre-registration.

**Shadow stack:** Every function emits `__uranite_push_frame` at entry and `__uranite_pop_frame` before every return. Return values are evaluated *before* the frame pop.

---

## Runtime

### Exception Runtime and Shadow Stack

`__uranite_throw` takes a Throwable pointer and type name string. The shadow stack enables structured unwinding through `try`/`except`/`finally` blocks. Runtime functions are defined in `src/uranite-crt/exception-runtime/`.

### C Runtime Libraries

Static C11 libraries linked automatically:

| Library | Purpose |
|---|---|
| `exception-runtime` | Exception throwing and unwinding |
| `async-runtime` | Async task scheduling bridge |
| `thread-runtime` | POSIX thread wrapper |
| `subprocess-runtime` | Process spawning |
| `ipc-runtime` | Inter-process communication |
| `ffi-runtime` | Foreign function interface |

### Async Runtime

The async runtime (`stdlibs/async/`) is written in pure Uranite using raw Linux syscalls and `asm volatile`. It has zero C runtime dependency. Components: epoll-based event loop, task scheduler, timer management.

---

## Module System

The compiler driver (`src/uranite/compiler/driver.cpp`) orchestrates module resolution:

1. Parse the `package` declaration and `import` statements.
2. Resolve imported modules from the standard library path or `-I` include paths.
3. Analyze imported modules first (registration pass) and cache their symbols.
4. Import cached types and symbols into the current module before its analysis pass.
5. Auto-import `stdlibs/async/runtime.urn` when `async` functions are present.

Declarations must be `public` to be importable (or within an `export {}` block for `protect`/`Default` visibility).

---

## Qualnames Registry

**File:** `src/uranite/semantic/qualnames.hpp`

All entity name strings used across the compiler are centralized in this file. The namespace hierarchy mirrors the language entity structure:

```
uranite::semantic::qualname::
├── identifier::     Self, None, TrueLiteral, FalseLiteral
├── functions::      main, parameter names
├── primitives::     qualified names for Bool, Char, String, Void, Error
├── classes::
│   ├── object::     Qualified, Name, methods::{ToString, HashCode, Equals}
│   ├── string::     Qualified, Name, methods::{Concat, Length, Contains, ...}
│   ├── [all OOP wrappers]:: Qualified, Name
│   ├── Memory, Arena, Args, Kwargs, Pair, ArrayList, HashMap, ...
│   └── Error types:: Exception, Throwable, ZeroDivisionError, ...
├── interfaces::
│   ├── addable::    Qualified, Name, methods::{Add}
│   ├── equatable::  Qualified, Name, methods::{Equals, NotEquals}
│   ├── comparable:: Qualified, Name, methods::{LessThan, GreaterThan, ...}
│   ├── iterable::   Qualified, Name, methods::{Iterator}
│   └── [all operator interfaces]
└── operatorMapping:: centralized operator-to-interface dispatch table
```

**Rule: Never use hardcoded entity name strings.** Every string comparison against a type name, method name, or identifier must reference a constant from `qualnames.hpp`. Hardcoded strings create fragile duck-typing where a typo silently breaks dispatch.

```cpp
// WRONG — hardcoded string, invisible to refactoring
if( methodName == "toString" ) { ... }

// CORRECT — references the centralized constant
if( methodName == semantic::qualname::classes::object::methods::ToString ) { ... }
```

---

## Coding Conventions

### C++ Style Rules

**Formatting:**
- No spaces before opening paren in control flow: `if( x )`, not `if ( x )`.
- Space inside parens when arguments are present: `foo( x, y )`. No space when empty: `foo()`.
- Allman-ish bracing with `} else {` on separate lines.

**Type discipline:**
- No `auto` — use concrete types everywhere.
- Use `this->` for all member function calls and member access.

**Comparisons:**
- Use `== false` and `== nullptr` instead of `!expr`.
- Example: `if( pointer == nullptr )` not `if( !pointer )`.

**Strings:**
- Use `fmt::format(...)` for string construction. Never use `+` concatenation in the compiler source.

**Naming:**
- All code under `uranite::` with subnamespaces matching directory structure: `uranite::parser`, `uranite::ir::hir`, `uranite::ir::mir`, `uranite::semantic`.

### Uranite Source Style

These rules apply to `.urn` files in `stdlibs/` and `examples/`:

**Naming:** Descriptive identifiers only. Never use single-letter or cryptic names (`i`, `j`, `ptr`, `val`). Use `slotIndex`, `byteIndex`, `elementCount`.

**Doccomments:** `"""..."""` placed *after* declarations. Must include `Parameters:`, `Returns:`, and `Complexity:` sections. No `@param` style.

**Imports:** Use comma-separated or brace-wrapped imports for multiple entities. Never one-per-line.

**Zero C Runtime:** Standard library modules must use pure Uranite and raw syscalls. No C runtime dependency in stdlib modules.

---

## Building and Testing

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build all targets
make -C build -j$(nproc)

# Run unit tests (152 tests)
./build/uranite-tests

# Run a single test suite
./build/uranite-tests --gtest_filter="ParserTest.*"

# Run a single test
./build/uranite-tests --gtest_filter="ParserTest.ParsesSimpleFunction"

# Compile and run an example
./build/uranite examples/tutorials/testing.urn -r

# Diagnostic inspection
./build/uranite source.urn --dump-mir --verbose
```
