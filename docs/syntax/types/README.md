# Types

---

## Table of Contents

- [Types](#types)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Type System Philosophy](#type-system-philosophy)
    - [Static Typing](#static-typing)
    - [Explicit Type Annotations](#explicit-type-annotations)
    - [Diamond Inference](#diamond-inference)
    - [Ownership-Aware Types](#ownership-aware-types)
    - [Zero-Cost Wrapper Types](#zero-cost-wrapper-types)
  - [Type Categories](#type-categories)
    - [Primitive Types](#primitive-types)
    - [Composite Types](#composite-types)
    - [User-Defined Types](#user-defined-types)
    - [Function and Callable Types](#function-and-callable-types)
    - [Generic Types](#generic-types)
    - [Async Types](#async-types)
    - [Memory Types](#memory-types)
    - [Advanced Type Features](#advanced-type-features)
  - [Type Compatibility](#type-compatibility)
    - [Assignment Compatibility](#assignment-compatibility)
    - [Numeric Widening](#numeric-widening)
    - [None Assignment](#none-assignment)
    - [Inheritance and Interface Conformance](#inheritance-and-interface-conformance)
  - [Quick Reference](#quick-reference)
  - [Examples](#examples)
    - [Primitive Type Declarations](#primitive-type-declarations)
    - [Composite Type Declarations](#composite-type-declarations)
    - [Generic Type Usage](#generic-type-usage)
    - [Type Casting](#type-casting)
    - [Working with Optional Types](#working-with-optional-types)

---

## Overview

Uranite is a statically typed language with strict compile-time type checking. Every variable, parameter, return value, and expression has a type determined at compile time. The compiler rejects programs with type mismatches before any code runs. There is no dynamic typing, no implicit type coercion (except numeric widening), and no runtime type guessing.

This section documents the complete type system: primitive types, composite types, user-defined types, generics, type inference, and the rules that govern how types interact.

---

## Type System Philosophy

### Static Typing

Every expression in Uranite has a type known at compile time. The compiler verifies all type assignments, function calls, operator applications, and return values. If a type mismatch exists anywhere in the program, compilation fails with a clear error message.

```uranite
I64 count = 42
String name = "hello"
Boolean active = True
```

Each variable declaration specifies its type explicitly. The compiler verifies that the assigned value matches the declared type.

### Explicit Type Annotations

Uranite requires explicit type annotations on variable declarations, function parameters, and return types. Every variable has a declared type — there is no `var` or `let` keyword that infers the type from the right-hand side.

```uranite
public function greet( String name, I64 times ) -> String:
    String message = "Hello, " + name
    return message
```

Both parameters and the return type are explicitly annotated. The local variable `message` is declared with its type `String`.

### Diamond Inference

The one exception to explicit typing is diamond syntax for constructor calls. When creating an instance of a generic type, the type arguments can be omitted if the compiler can infer them from the variable declaration:

```uranite
ArrayList<String> names = new ArrayList<>()
HashMap<String, I64> scores = new HashMap<>()
```

The `<>` in `new ArrayList<>()` tells the compiler to infer the type parameters from the left-hand side of the assignment. The compiler resolves `ArrayList<>` to `ArrayList<String>` based on the declared variable type `ArrayList<String>`.

Diamond inference works only in assignment context — you cannot use `<>` in a return statement or function argument without a type annotation on the receiving side.

### Ownership-Aware Types

Types interact with Uranite's ownership model. Move semantics are the default — assigning a value of a non-primitive type to another variable transfers ownership. The borrow checker enforces that references do not outlive the values they reference, and the type system tracks mutability through pointer and reference types.

```uranite
String original = "hello"
String moved = move original
```

After the move, `original` is no longer valid. The ownership has been transferred to `moved`.

### Zero-Cost Wrapper Types

Primitive types have corresponding wrapper classes in the standard library (`Boolean`, `Int`, `Float`, `Char`, `String`, etc.). These wrapper types carry no runtime overhead — a `Boolean` value and a bare boolean produce identical machine code. There is no boxing, no heap allocation, and no indirection for wrapper types.

This means you can call methods on primitive values without any performance penalty:

```uranite
I64 value = -42
I64 absolute = value.abs()
String text = value.toString()
```

---

## Type Categories

### Primitive Types

The built-in scalar types that form the foundation of the type system.

| Document | Description |
|---|---|
| [Primitive Types](primitive-types.md) | Overview of all built-in scalar types, their sizes, ranges, and relationships. |
| [Integer Types](integer-types.md) | Signed integers (`I8`, `I16`, `I32`, `I64`), unsigned integers (`U8`, `U16`, `U32`, `U64`), and named types (`Int`, `UInt`, `Byte`). Overflow behavior, wrapping arithmetic, and integer operations. |
| [Floating-Point Types](floating-point-types.md) | `F32` and `F64` types, IEEE 754 semantics, precision rules, and named types (`Float`, `Double`). |
| [Boolean Type](boolean-type.md) | The `Boolean` type, `True` and `False` literals, logical operators (`and`, `or`, `not`), and truthiness rules. |
| [Char Type](char-type.md) | The `Char` type for 32-bit Unicode scalar values, character classification methods, and case conversion. |
| [String Type](string-type.md) | The `String` type for immutable UTF-8 text, string methods, concatenation, and the `format` method. |
| [Void and None Types](void-and-none-types.md) | The `Void` return type for functions that produce no value, and the `None` literal representing the absence of a value. |

### Composite Types

Types that combine or wrap other types.

| Document | Description |
|---|---|
| [Array Types](array-types.md) | Raw `Memory<T>` arrays with manual sizing, heap allocation, and element access. |
| [Tuple Types](tuple-types.md) | Fixed-size heterogeneous collections using `(a, b, c)` literal syntax. |
| [Optional Types](optional-types.md) | The `?T` syntax for values that may be `None`. Optional variable declarations, None checks, and safe access patterns. |
| [Union Types](union-types.md) | Multi-type alternatives allowing a value to hold one of several possible types. |

### User-Defined Types

Types declared by the programmer.

| Document | Description |
|---|---|
| [Class Types](class-types.md) | Class declarations with fields, methods, constructors, visibility modifiers (`public`, `protect`, `private`), inheritance (`extends`), and the `final` modifier. |
| [Struct Types](struct-types.md) | Value-type struct declarations with field layout and value semantics. |
| [Interface Types](interface-types.md) | Interface declarations defining method contracts, multiple interface implementation with `implements`, and interface inheritance. |
| [Enum Types](enum-types.md) | Enum declarations with `unit` variants, backed enums with explicit integer values, and enum methods. |

### Function and Callable Types

Types representing functions and callable references.

| Document | Description |
|---|---|
| [Function Types](function-types.md) | Function type signatures, parameter types, return types, and variadic function signatures. |
| [Callable Types](callable-types.md) | Higher-order callable references for passing functions as values, storing them in variables, and invoking them dynamically. |

### Generic Types

Types parameterized by other types.

| Document | Description |
|---|---|
| [Generic Types](generic-types.md) | Generic type parameters (`<T>`), type constraints with `where` clauses, generic classes and functions, and diamond inference. |

### Async Types

Types for asynchronous and lazy computation.

| Document | Description |
|---|---|
| [Future and Generator Types](future-and-generator-types.md) | `Future<T>` for async function return values and `Generator<T>` for lazy sequences produced by `yield`. |

### Memory Types

Types for low-level memory operations and references.

| Document | Description |
|---|---|
| [Pointer and Reference Types](pointer-and-reference-types.md) | Raw pointers, borrowed references, mutability tracking, and their role in ownership-aware memory management. |

### Advanced Type Features

Type system features for advanced use cases.

| Document | Description |
|---|---|
| [Meta Types](meta-types.md) | `Meta<T>` for treating types as values, enabling type-level programming and reflection. |
| [Type Aliases](type-aliases.md) | The `type` keyword for creating alternative names for existing types. |
| [Type Casting](type-casting.md) | The `as` keyword for explicit type conversions between compatible types. |
| [Type Compatibility](type-compatibility.md) | Rules governing which types can be assigned to which, including inheritance, interface conformance, and numeric widening. |
| [Type Identity](type-identity.md) | How types are compared for equality and what makes two type references refer to the same type. |
| [Type Inference](type-inference.md) | Diamond syntax, type inference from assignment context, and the boundaries of what the compiler can infer. |

---

## Type Compatibility

### Assignment Compatibility

A value of one type can be assigned to a variable of another type when the types are compatible. The core rules are:

- **Exact match**: A `String` value is assignable to a `String` variable.
- **Numeric widening**: A smaller integer type is assignable to a larger integer type. An integer is assignable to a float.
- **None to optional**: `None` is assignable to any optional type (`?T`).
- **Subclass to superclass**: A value of a subclass type is assignable to a variable of its superclass type.
- **Implementation to interface**: A value of a class that implements an interface is assignable to a variable of that interface type.
- **Any type to Object**: Every type is assignable to `Object`, the root of the class hierarchy.

### Numeric Widening

Smaller numeric types can be implicitly widened to larger numeric types without an explicit cast:

```uranite
I8 small = 42
I64 large = small

I64 integer = 100
F64 floating = integer
```

The widening is always safe — no precision is lost when widening an integer, and integer-to-float conversion preserves the value for integers within the float's precision range.

Narrowing (large type to small type) requires an explicit `as` cast and may lose data:

```uranite
I64 large = 300
I8 small = large as I8
```

### None Assignment

`None` is assignable to any optional type. It represents the absence of a value:

```uranite
?String maybeName = None
?I64 maybeCount = None
?ArrayList<String> maybeList = None
```

Assigning `None` to a non-optional type is a compile-time error:

```uranite
String name = None
```

This fails because `String` is not optional. Use `?String` to allow `None`.

### Inheritance and Interface Conformance

A subclass value is assignable to a superclass variable. A class that implements an interface is assignable to an interface variable:

```uranite
public class Animal:
    public String species

    public function Animal( self, String species ) -> Void:
        self.species = species

public class Dog extends Animal:
    public function Dog( self ) -> Void:
        parent( "Canis familiaris" )

public function main() -> I32:
    Animal pet = new Dog()
    return 0
```

The `Dog` value is assigned to an `Animal` variable because `Dog` extends `Animal`. The same principle applies to interfaces — a class value is assignable to a variable of any interface it implements.

---

## Quick Reference

| Type | Description | Example |
|---|---|---|
| `I8`, `I16`, `I32`, `I64` | Signed integers (8 to 64 bit) | `I64 count = 42` |
| `U8`, `U16`, `U32`, `U64` | Unsigned integers (8 to 64 bit) | `U8 byte = 255` |
| `Int` | Platform-width signed integer (64-bit) | `Int value = 100` |
| `UInt` | Platform-width unsigned integer (64-bit) | `UInt size = 50` |
| `Byte` | Unsigned 8-bit integer for raw byte data | `Byte raw = 0xFF` |
| `F32` | 32-bit single-precision float | `F32 precise = 1.5` |
| `F64` | 64-bit double-precision float | `F64 ratio = 3.14` |
| `Float` | 64-bit floating-point type | `Float value = 2.718` |
| `Double` | 64-bit double-precision floating-point type | `Double pi = 3.14159` |
| `Boolean` | Boolean truth value (`True` or `False`) | `Boolean active = True` |
| `Char` | 32-bit Unicode character | `Char letter = 'A'` |
| `String` | Immutable UTF-8 string | `String name = "hello"` |
| `Void` | No return value | `-> Void` |
| `None` | Absence of value | `?String empty = None` or `Optional<String> empty = None` |
| `Memory<T>` | Raw heap-allocated array | `Memory<I64> buf = [1, 2, 3]` |
| `?T` | Optional type (may be `None`) | `?String maybe = None` |
| `Optional<T>` | Optional type (may be `None`) | `Optional<String> maybe = None` |
| `ArrayList<E>` | Dynamic list | `ArrayList<I64> list = new ArrayList<>()` |
| `HashMap<K, V>` | Key-value map | `HashMap<String, I64> map = new HashMap<>()` |
| `HashSet<E>` | Unique element set | `HashSet<String> set = new HashSet<>()` |
| `Future<T>` | Async return value | `async function fetch() -> Future<String>:` |
| `Generator<T>` | Lazy yield sequence | Produced by functions using `yield` |

---

## Examples

### Primitive Type Declarations

```uranite
I64 count = 42
F64 ratio = 3.14
Boolean active = True
Char letter = 'A'
String name = "hello"
```

Every primitive type has a fixed size and well-defined behavior. Integer types have wrapping arithmetic by default. Float types follow IEEE 754 semantics. Boolean accepts only `True` or `False`.

### Composite Type Declarations

```uranite
from uranite.collection.array-list import ArrayList
from uranite.collection.hash-map import HashMap

Memory<I64> numbers = [1, 2, 3]
?String maybeName = None
ArrayList<String> names = new ArrayList<>()
HashMap<String, I64> scores = new HashMap<>()
```

`Memory<T>` provides raw heap-allocated arrays. The `?` prefix creates an optional type that can hold `None`. Generic collection types use diamond inference with `<>` to avoid repeating type arguments.

### Generic Type Usage

```uranite
public function firstElement<T>( ArrayList<T> list ) -> T:
    return list.get( 0 )

public function pairUp<A, B>( A first, B second ) -> Pair<A, B>:
    return new Pair<>( first, second )
```

Generic functions declare type parameters in angle brackets after the function name. The compiler infers concrete types at each call site based on the argument types.

### Type Casting

```uranite
I64 count = 42
F64 widened = count as F64

F64 precise = 3.99
I64 truncated = precise as I64

I64 large = 300
I8 narrow = large as I8
```

The `as` keyword performs explicit type conversion. Widening casts (small to large) are safe. Narrowing casts (large to small, float to int) may lose data — the value is truncated, not rounded.

### Working with Optional Types

```uranite
from uranite.io.console import puts

public function findUser( String name ) -> ?String:
    if name == "admin":
        return "Administrator"
    return None

public function main() -> I32:
    ?String result = findUser( "admin" )
    if result != None:
        puts( result )

    ?String missing = findUser( "nobody" )
    if missing == None:
        puts( "user not found" )
    return 0
```

Functions returning `?T` can return either a value of type `T` or `None`. Callers must check for `None` before using the value. The `==` and `!=` operators work with `None` on either side of the comparison.
