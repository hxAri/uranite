# Advanced Concepts

This section covers language features that go beyond basic declarations, statements, and expressions: structured error handling, asynchronous programming, the module system, inline assembly, and foreign function interop.

---

## Table of Contents

- [Advanced Concepts](#advanced-concepts)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [At a Glance](#at-a-glance)
  - [Subpage Index](#subpage-index)

---

## Overview

**Error handling** uses `try`, `except`, `finally`, and `raise` for structured exception management. Functions declare the exceptions they can throw with the `raises` specifier in their signature. The `defer` statement guarantees cleanup code runs when a scope exits.

**Async** provides `async function` declarations that return `Future<T>` values. The `await` keyword suspends execution until a future completes. The async runtime is auto-imported by the compiler when async functions are used.

**Modules and packages** organize code into reusable units. Every source file starts with a `package` declaration. The `import` and `from ... import` statements bring symbols into scope. Access modifiers (`public`, `private`, `protect`) and `export` blocks control which declarations are visible outside their defining module.

**Inline assembly** embeds platform-specific machine instructions directly in Uranite source using the `asm` keyword. The `volatile` modifier prevents the compiler from optimizing away assembly blocks. Multi-architecture block forms enable platform-specific variants in a single declaration.

**Interop** provides a foreign function interface for calling native C libraries. The `extern function` declaration imports symbols from native code, with optional link-name aliasing and C-style variadic support.

---

## At a Glance

```uranite
package myapp

from uranite.io.console import puts

extern function abs( I64 value ) -> I64

public function risky( I64 value ) -> I64 raises Exception:
    if value == 0:
        raise new Exception( "zero" )
    return 100 / value

public function main() -> I32:
    try:
        I64 result = risky( 0 )
    except Exception as error:
        puts( "handled" )
    finally:
        puts( "cleanup" )

    I64 absolute = abs( -42 )
    puts( absolute.toString() )
    return 0
```

---

## Subpage Index

| Document | Description |
|---|---|
| [Error Handling](error-handling.md) | Exception hierarchy, `try`/`except`/`finally`, `raise`, and the `raises` specifier on function signatures. |
| [Async](async.md) | `async function`, `Future<T>` return types, `await`, and the auto-imported async runtime. |
| [Modules and Packages](modules-and-packages.md) | `package` declarations, `import` and `from ... import`, access modifiers, `export` blocks, and module resolution. |
| [Inline Assembly](inline-assembly.md) | The `asm` keyword, `volatile` assembly blocks, and multi-architecture block forms. |
| [Interop](interop.md) | `extern function` declarations, link-name aliasing, C-style variadic parameters, and dynamic symbol loading. |
