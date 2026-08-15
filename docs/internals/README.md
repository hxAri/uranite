# Compiler Internals

A contributor guide to the Uranite compiler architecture. This document covers the full compilation pipeline, the major subsystems, and the development conventions enforced across the C++ codebase.

---

## Table of Contents

- [Compiler Internals](#compiler-internals)
  - [Table of Contents](#table-of-contents)
  - [Compilation Pipeline](#compilation-pipeline)
  - [Source Layout](#source-layout)
  - [Coding Conventions](#coding-conventions)
  - [Building and Testing](#building-and-testing)
  - [Subpages](#subpages)

---

## Compilation Pipeline

```
Source (.urn)
    |
    v
  Lexer ------------- Token stream
    |
    v
  Parser ------------ AST (Abstract Syntax Tree)
    |
    v
  Semantic Analyzer - Type-annotated AST (two-pass: registration + validation)
    |
    v
  Borrow Checker ---- Ownership verification
    |
    v
  HIR Lowering ------ HIR (High-Level IR: preserves loops, match, classes)
    |
    v
  HIR Validation ---- Structural correctness checks
    |
    v
  MIR Lowering ------ MIR (Mid-Level IR: flattened CFG, linear instructions)
    |
    v
  MIR Liveness ------ Variable liveness analysis
    |
    v
  MIR Borrow Check -- Ownership rules on MIR
    |
    v
  MIR Optimization -- Dead code, constant folding at MIR level
    |
    v
  MIR Codegen ------- LLVM IR generation
    |
    v
  LLVM Backend ------ Object code (with LLVM optimization passes)
    |
    v
  Linker ------------ Native executable
```

The MIR path is the production pipeline. The AST-direct codegen and AST optimizer are legacy and strictly bypassed.

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
├── token/           # Token types and definitions
└── visitors/        # AST visitor infrastructure
```

---

## Coding Conventions

**Formatting:**
- No spaces before opening paren in control flow: `if( x )`, not `if ( x )`.
- Space inside parens when arguments are present: `foo( x, y )`. No space when empty: `foo()`.
- Allman-ish bracing with `} else {` on separate lines.

**Type discipline:**
- No `auto` — use concrete types everywhere.
- Use `this->` for all member function calls and member access.

**Comparisons:**
- Use `== false` and `== nullptr` instead of `!expr`.

**Strings:**
- Use `fmt::format(...)` for string construction. Never use `+` concatenation.

**Naming:**
- All code under `uranite::` with subnamespaces matching directory structure.

---

## Building and Testing

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
make -C build -j$(nproc)
./build/uranite-tests
./build/uranite-tests --gtest_filter="ParserTest.*"
./build/uranite source.urn --dump-mir --verbose
```

---

## Subpages

| Page | Description |
|---|---|
| [Pipeline Overview](pipeline-overview.md) | Detailed walkthrough of each compilation stage |
| [Lexer](lexer/README.md) | Tokenization, indentation tracking, token types |
| [Parser](parser/README.md) | Recursive descent parsing, AST construction |
| [Semantic Analysis](semantic-analysis/README.md) | Two-pass analysis, type registry, generic substitution |
| [HIR](hir/README.md) | High-level IR: lowering and validation |
| [MIR](mir/README.md) | Mid-level IR: lowering, CFG, optimization, borrow checking |
| [Codegen](codegen/README.md) | LLVM IR generation from MIR |
| [Runtime](runtime/README.md) | Exception runtime, shadow stack, async runtime, CRT libraries |
| [Module System](module-system/README.md) | Module resolution, eager import, symbol caching |
