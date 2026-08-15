# Types

Uranite is statically typed. Every variable, parameter, return value, and expression has a type determined at compile time. The compiler rejects programs with type mismatches before any code runs.

---

## Table of Contents

- [Types](#types)
  - [Table of Contents](#table-of-contents)
  - [Type System Overview](#type-system-overview)
  - [Type Categories](#type-categories)
  - [At a Glance](#at-a-glance)
  - [Subpage Index](#subpage-index)

---

## Type System Overview

**Explicit annotations.** Variable declarations, function parameters, and return types all require explicit type annotations. There is no `var` or `let` keyword that infers the type from the right-hand side.

**Diamond inference.** The one exception is constructor calls for generic types, where `<>` tells the compiler to infer type arguments from the assignment context: `ArrayList<String> names = new ArrayList<>()`.

**Ownership-aware.** Types interact with the ownership model. Move semantics are the default for non-primitive types — assigning transfers ownership. The borrow checker enforces that references do not outlive the values they reference.

**Zero-cost wrappers.** Primitive types have corresponding wrapper classes in the [standard library](../../stdlib/language/README.md) (`Boolean`, `Int`, `Float`, `Char`, `String`). These carry no runtime overhead — calling methods on primitive values produces identical machine code to bare operations.

**Numeric widening.** Smaller numeric types widen implicitly to larger numeric types. Narrowing requires an explicit `as` cast.

---

## Type Categories

**Primitive types** are the built-in scalar types: signed integers (`I8`, `I16`, `I32`, `I64`), unsigned integers (`U8`, `U16`, `U32`, `U64`), floating-point (`F32`, `F64`), `Boolean`, `Char`, `String`, and `Void`. Named convenience types include `Int` (64-bit signed), `UInt` (64-bit unsigned), `Byte` (unsigned 8-bit), `Float` (64-bit), and `Double` (64-bit).

**Optional types** use the `?T` prefix syntax to allow `None` as a value. A `?String` variable can hold either a `String` value or `None`.

**Union types** allow a variable to hold one of several possible types, written with the pipe operator.

**Pointer and reference types** provide raw pointers and borrowed references for ownership-aware memory management.

**Type aliases** use the `type` keyword to create alternative names for existing types.

**Type casting** uses the `as` keyword for explicit conversions between compatible types.

For detailed documentation on each OOP wrapper type (`Boolean`, `Int`, `Float`, `Char`, `String`, and others), see the [Standard Library Language Types](../../stdlib/language/README.md) section.

---

## At a Glance

```uranite
I64 count = 42
F64 ratio = 3.14
Boolean active = True
Char letter = 'A'
String name = "hello"

?String maybe = None

I8 small = 10
I64 widened = small
```

---

## Subpage Index

| Document | Description |
|---|---|
| [Primitive Types](primitive-types.md) | Overview of all built-in scalar types, their sizes, ranges, and relationships. |
| [Void and None Types](void-and-none-types.md) | The `Void` return type and the `None` literal for absent values. |
| [Optional Types](optional-types.md) | The `?T` syntax for values that may be `None`, None checks, and safe access patterns. |
| [Union Types](union-types.md) | Multi-type alternatives with the pipe operator. |
| [Pointer and Reference Types](pointer-and-reference-types.md) | Raw pointers, borrowed references, and their role in ownership. |
| [Type Aliases](type-aliases.md) | The `type` keyword for creating alternative names for existing types. |
| [Type Casting](type-casting.md) | The `as` keyword for explicit type conversions between compatible types. |
| [Type Compatibility](type-compatibility.md) | Rules for assignment compatibility, numeric widening, inheritance, and interface conformance. |
| [Type Identity](type-identity.md) | How types are compared for equality and what makes two type references the same type. |
| [Type Inference](type-inference.md) | Diamond syntax `<>`, inference from assignment context, and its boundaries. |
