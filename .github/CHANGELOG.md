# Changelog

## v1.1.0-2026.7 2026-07-15

**Added**
- Enum variant access in MIR codegen path — backed values and discriminants resolve correctly instead of returning `0`
- C ABI-compliant `main` function in MIR codegen — returns `i32` with `(i32, ptr)` parameters
- Enum variant registration as module constants during MIR lowering phase
- Imported enum variant resolution via `EnumType` semantic info with AST backed value fallback
- `Memory<T>` compiler intrinsic in MIR codegen — constructor emits `calloc(capacity, sizeof(T))`, `get`/`set`/`free`/`copyTo` emit inline GEP+load/store
- `Arena<T>` compiler intrinsic in MIR codegen — struct `{ptr, i64 count, i64 capacity}` with `alloc`/`freeAll`/`destroy`/`count` inlined
- `String.format` intrinsic in MIR codegen — compile-time `{}` placeholder→`%ld`/`%f` rewrite with snprintf
- `Object.toString` stub in MIR codegen — identity function returning self pointer, unblocks `writeArgsToFd` linkage
- Inline `puts`/`putsln`/`putserr`/`putserrln` in MIR codegen — direct `write` syscalls with snprintf-based type coercion (integer `%ld`, float `%f`, bool `True`/`False`)
- Compound assignment operators (`+=`, `-=`, `*=`, `/=`) in MIR lowering — emits load+arithmetic+store instead of plain store
- `AddressOf` instruction handler — emits `ptrtoint` for pointers, `sext` for integers
- `TakeReference` instruction handler — returns raw pointer value
- `DereferencePointer` instruction handler — loads from pointer with proper type

**Changed**
- MIR lowering `lowerFieldAccess` intercepts enum type field access before emitting `ComputeFieldAddress`
- MIR codegen pre-registration loop and `generateFunction` both detect `main` and override signature
- `AddressOf`, `TakeReference`, `DereferencePointer`, `InstanceOfCheck` separated from `InlineAssembly` no-op fall-through handler

**Fixed**
- `DmaDirection.ToDevice` and similar enum variant accesses returning `0` regardless of actual backed value
- MIR-compiled binaries exiting with garbage codes (e.g. `176`) due to `main` returning `void` instead of `i32 0`
- Hello-world example returning non-zero exit code (e.g. `176`) when compiled with `--use-mir` — now exits `0`
- All C-style for-loops (`for I64 i = 0; i < n; i++`) hanging infinitely — compound assignment `i += 1` was stored as literal `1` instead of `i + 1`
- `Memory<Double>` element type resolution — `Memory.set` infers element type from stored value when `memoryElementTypes` has no entry (fixes `-nan` in spectral-norm)
- `undefined reference to 'Object.toString'` linker error caused by valid IR in `writeArgsToFd` exposing unresolved method call

**Issues**
- `InstanceOfCheck` always returns false in MIR codegen
- `InlineAssembly`, `DeferPush`/`DeferEmit`, `CallVirtual`, `InvokeFunction`/`LandingPad` remain as no-op stubs
- Generic type parameter method calls emit unresolved names (`E.hashCode`, `K.toString`) — blocks generic collection benchmarks
- `binary-trees` segfaults during Arena-based tree construction
- `mandelbrot` segfaults after header output

**Notes**
- MIR example results: 6/11 pass (hello-world, colorize, fannkuch-redux, n-body, spectral-norm, stress-memory)
- 3 compile failures (fasta, k-nucleotide, stress-collections) — generic method resolution
- 152/152 unit tests pass

## v1.0.0-2026.1 2026-07-13

**Added**
- Full compilation pipeline: Source → Lexer → Parser → Semantic Analyzer → Borrow Checker → AST Optimizer → LLVM Codegen → Linker
- Indentation-based syntax with INDENT/DEDENT token emission
- Ownership and move semantics with compile-time borrow checker (use-after-move, double-free detection)
- Object-oriented type system: classes, interfaces, traits, structs, enums (unit variants, backed values), generics (monomorphized)
- Single class inheritance, multiple interface implementation, virtual dispatch, `Readonly`/`final` modifiers, abstract methods
- Nullable types (`?Type`), pattern matching (`match`), range expressions (`..`/`...`), comprehension expressions
- Unified exception hierarchy: `Throwable`, `Error`, `Exception`, `Warning`, domain-specific errors (`LookupError`, `IndexError`, `KeyError`, `TypeError`, `ValueError`, `StateError`)
- Shadow call stack with full diagnostic on unhandled exceptions (file:line:column per frame, source context window)
- Runtime arithmetic safety checks: zero-division guard before every `div`/`mod` operation
- Inline assembly with full LLVM constraint syntax
- Multi-architecture inline assembly via arch blocks (x86-64, aarch64) across 46 stdlib modules
- C FFI through `extern function` declarations
- `addressof` operator returning raw `I64` address
- AST optimizer: constant folding, dead code elimination, strength reduction, tail-call optimization
- HIR pipeline: AST → HIR lowering → HIR validation (60 node types, preserves high-level semantics)
- MIR pipeline: HIR → MIR CFG lowering → liveness analysis → MIR borrow checker → MIR optimization (5 passes)
- Experimental MIR → LLVM codegen path (`--use-mir` flag)
- Six C11 static runtime libraries: exception handling, async event loop, thread spawning, subprocess management, IPC, FFI
- Pure-Uranite async runtime via raw Linux syscalls (epoll, timerfd, context switching) — zero C runtime dependency
- Standard library written entirely in Uranite with zero C runtime dependency: `collection/`, `io/`, `threading/`, `net/`, `crypto/`, `regexp/`, `subprocess/`, `process/`, `ffi/`, `fiber/`, `coroutine/`, `convert/`, `datetime/`, `string/`, `logging/`, `testing/`, `os/`, `web/`, `ipc/`, `security/`, `sys/`, `kernel/`, `adelia/`
- OOP wrapper types for primitives: `I64`, `String`, `Float`, `Boolean`, `Char`, `U8`, `Double`, etc.
- Five compiler binaries: `uranite`, `uranite-tests`, `uranite-fmt`, `uranite-doc`, `uranite-pkg`
- 152 GoogleTest unit tests across 12 test suites (100% pass rate)
- 212 module integration tests covering all major language features (100% pass rate)
- Cross-compilation target triple support with architecture-specific inline assembly block selection
- REPL mode (`--repl`)
- Diagnostic dump modes: `--dump-tokens`, `--dump-ast`, `--dump-hir`, `--dump-mir`, `--emit-llvm`

**Notes**
- MIR codegen path is experimental — passes 11/11 benchmarks and 164/203 module tests on AST path
- OOP wrapper classes not yet supported on MIR codegen path
- Language is research-oriented and not production-ready
