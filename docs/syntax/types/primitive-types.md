# Primitive Types

Uranite's primitive types are the fundamental scalar types built into the compiler. They form the foundation of the type system — every composite type, collection, and user-defined class ultimately stores or operates on primitive values. Uranite provides 16 primitive types: 8 signed integers, 4 unsigned integers, 2 floating-point types, a boolean, and a character type. Additionally, `String`, `Void`, and `None` serve as built-in types with special semantics.

This document covers the complete primitive type inventory, the "everything is an object" philosophy, the zero-cost abstraction mechanism that makes method calls on primitives free, and the type registry architecture that bridges primitive identifiers to OOP wrapper classes.

---

## Table of Contents

- [Overview](#overview)
- [Primitive Type Inventory](#primitive-type-inventory)
  - [Signed Integer Types](#signed-integer-types)
  - [Unsigned Integer Types](#unsigned-integer-types)
  - [Floating-Point Types](#floating-point-types)
  - [Boolean Type](#boolean-type)
  - [Character Type](#character-type)
  - [String Type](#string-type)
  - [Void and None Types](#void-and-none-types)
- [Type Aliases](#type-aliases)
- [Everything is an Object](#everything-is-an-object)
  - [The OOP Wrapper Architecture](#the-oop-wrapper-architecture)
  - [The Wrapper Class Hierarchy](#the-wrapper-class-hierarchy)
  - [Method Calls on Primitives](#method-calls-on-primitives)
- [Zero-Cost Abstractions](#zero-cost-abstractions)
  - [The toLLVMType Mechanism](#the-tollvmtype-mechanism)
  - [OOP Wrapper Recognition](#oop-wrapper-recognition)
  - [No Boxing, No Heap Allocation](#no-boxing-no-heap-allocation)
- [The Type Registry](#the-type-registry)
  - [Primitive Type Initialization](#primitive-type-initialization)
  - [The primitivesTypes Map](#the-primitivestypes-map)
  - [Lookup and Resolution](#lookup-and-resolution)
  - [The isPrimitive Classification](#the-isprimitive-classification)
- [The OOP Wrapper Set](#the-oop-wrapper-set)
  - [The oopWrapperQualified Set](#the-oopwrapperqualified-set)
  - [The integerOopQualified Set](#the-integeroopqualified-set)
  - [The floatOopQualified Set](#the-floatoopqualified-set)
  - [The isOopWrapper Function](#the-isoopwrapper-function)
- [Builtin Identifiers](#builtin-identifiers)
- [Examples](#examples)
  - [Primitive Declarations](#primitive-declarations)
  - [Method Calls on Primitives](#method-calls-on-primitives-examples)
  - [Type Conversions](#type-conversions)
  - [Practical Usage](#practical-usage)

---

## Overview

| Category | Types | LLVM Width |
|---|---|---|
| Signed integers | `I8`, `I16`, `I32`, `I64` | 8, 16, 32, 64 bits |
| Unsigned integers | `U8`, `U16`, `U32`, `U64` | 8, 16, 32, 64 bits |
| Floating-point | `F32`, `F64` | 32, 64 bits |
| Boolean | `Boolean` | 1 bit (`i1`) |
| Character | `Char` | 32 bits (`i32`) |
| String | `String` | pointer (`ptr`) |
| Void | `Void` | `void` |
| None | `None` | null pointer (`ptr null`) |

Every primitive type exists in two forms: a low-level primitive kind (`Type::Kind::Integer`, `Type::Kind::Bool`, etc.) and an OOP wrapper class in `stdlibs/language/`. Both forms produce identical LLVM IR — the OOP wrapper carries no runtime cost.

---

## Primitive Type Inventory

### Signed Integer Types

| Type Name | Bit Width | Range | Qualified Name | LLVM Type |
|---|---|---|---|---|
| `I8` | 8 | -128 to 127 | `uranite.language.i8.I8` | `i8` |
| `I16` | 16 | -32,768 to 32,767 | `uranite.language.i16.I16` | `i16` |
| `I32` | 32 | -2,147,483,648 to 2,147,483,647 | `uranite.language.i32.I32` | `i32` |
| `I64` | 64 | -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807 | `uranite.language.i64.I64` | `i64` |

All integer literals default to `I64`. Smaller integer types require explicit type annotation or casting.

**Aliases for signed integers:**

| Alias | Resolves To | Description |
|---|---|---|
| `Int` | `I64` | Default signed integer. |
| `Integer` | `I32` | 32-bit signed integer. |
| `Long` | `I64` | Explicit 64-bit signed integer. |

### Unsigned Integer Types

| Type Name | Bit Width | Range | Qualified Name | LLVM Type |
|---|---|---|---|---|
| `U8` | 8 | 0 to 255 | `uranite.language.u8.U8` | `i8` |
| `U16` | 16 | 0 to 65,535 | `uranite.language.u16.U16` | `i16` |
| `U32` | 32 | 0 to 4,294,967,295 | `uranite.language.u32.U32` | `i32` |
| `U64` | 64 | 0 to 18,446,744,073,709,551,615 | `uranite.language.u64.U64` | `i64` |

**Aliases for unsigned integers:**

| Alias | Resolves To | Description |
|---|---|---|
| `UInt` | `U64` | Default unsigned integer. |
| `Byte` | `U8` | Single byte (unsigned 8-bit). |

### Floating-Point Types

| Type Name | Bit Width | Precision | Qualified Name | LLVM Type |
|---|---|---|---|---|
| `F32` | 32 | ~7 decimal digits | `uranite.language.f32.F32` | `float` |
| `F64` | 64 | ~15 decimal digits | `uranite.language.f64.F64` | `double` |

All float literals default to `F64`. `F32` requires explicit type annotation.

**Aliases for floating-point types:**

| Alias | Resolves To | Description |
|---|---|---|
| `Float` | `F64` | Default floating-point type. |
| `Double` | `F64` | Explicit 64-bit float. |

### Boolean Type

| Type Name | Bit Width | Values | Qualified Name | LLVM Type |
|---|---|---|---|---|
| `Boolean` | 1 | `True`, `False` | `uranite.language.boolean.Boolean` | `i1` |

The boolean primitive is `uranite.builtin.bool`. The OOP wrapper `Boolean` is a `final` class.

### Character Type

| Type Name | Bit Width | Range | Qualified Name | LLVM Type |
|---|---|---|---|---|
| `Char` | 32 | Unicode scalar values (U+0000 to U+10FFFF) | `uranite.language.char.Char` | `i32` |

The character primitive is `uranite.builtin.char`. The OOP wrapper `Char` is a `final` class with classification and case conversion methods.

### String Type

| Type Name | Representation | Qualified Name | LLVM Type |
|---|---|---|---|
| `String` | Null-terminated UTF-8 byte sequence | `uranite.language.string.String` | `ptr` |

Strings are immutable. The `String` OOP wrapper provides methods like `length()`, `concat()`, `contains()`, `toUpper()`, `toLower()`, `trim()`, `replace()`, `split()`, and `format()`.

### Void and None Types

| Type Name | Meaning | Qualified Name | LLVM Type |
|---|---|---|---|
| `Void` | No return value | `uranite.language.none.NoneType` / `uranite.builtin.void` | `void` |
| `None` | Absence of value | (singleton, no package) | `ptr null` |

`Void` indicates a function returns nothing. `None` is a literal representing "no value present" — it is assignable to optional types (`?T`) and produces a null pointer in LLVM IR. They serve different roles: `Void` is a return type annotation, `None` is a runtime value.

---

## Type Aliases

The type registry maps multiple names to the same underlying type. This means `Int`, `I64`, and `Long` all refer to the exact same `IntegerType(64, signed)` instance:

| Alias Group | Resolved Type | Bit Width |
|---|---|---|
| `Int`, `I64`, `Long` | `IntegerType(64, signed=true)` | 64 |
| `Integer`, `I32` | `IntegerType(32, signed=true)` | 32 |
| `UInt`, `U64` | `IntegerType(64, signed=false)` | 64 |
| `Byte`, `U8` | `IntegerType(8, signed=false)` | 8 |
| `Float`, `F64`, `Double` | `FloatType(64)` | 64 |
| `Void`, `NoneType` | `VoidType` | 0 |

These aliases are registered in `primitivesTypes` during registry construction, pointing different names to the same `TypeSharedPointer` instance.

---

## Everything is an Object

### The OOP Wrapper Architecture

Uranite follows the "everything is an object" philosophy. Every primitive type has a corresponding OOP wrapper class in `stdlibs/language/`. When you write `I64`, the semantic analyzer resolves it to the `I64` class — not a raw integer. This class has methods, a constructor, and participates in the type hierarchy.

| Primitive Kind | OOP Wrapper Class | Stdlib File |
|---|---|---|
| `Integer(64, signed)` | `I64 extends Int` | `stdlibs/language/i64.urn` |
| `Integer(32, signed)` | `I32 extends Int` | `stdlibs/language/i32.urn` |
| `Integer(16, signed)` | `I16 extends Int` | `stdlibs/language/i16.urn` |
| `Integer(8, signed)` | `I8 extends Int` | `stdlibs/language/i8.urn` |
| `Integer(64, unsigned)` | `U64` | `stdlibs/language/u64.urn` |
| `Integer(32, unsigned)` | `U32` | `stdlibs/language/u32.urn` |
| `Integer(16, unsigned)` | `U16` | `stdlibs/language/u16.urn` |
| `Integer(8, unsigned)` | `U8` / `Byte` | `stdlibs/language/u8.urn` / `byte.urn` |
| `Float(64)` | `F64 extends Float` | `stdlibs/language/f64.urn` |
| `Float(32)` | `F32 extends Float` | `stdlibs/language/f32.urn` |
| `Bool` | `Boolean` (final) | `stdlibs/language/boolean.urn` |
| `Char` | `Char` (final) | `stdlibs/language/char.urn` |
| `String` | `String` | `stdlibs/language/string.urn` |

The total set spans 24 stdlib files under `stdlibs/language/`, including base classes (`int.urn`, `float.urn`), aliases (`double.urn`, `integer.urn`, `long.urn`, `uint.urn`), and special types (`void.urn`, `none.urn`, `callable.urn`).

### The Wrapper Class Hierarchy

Integer and float wrappers form class hierarchies:

**Integer hierarchy:**

```
Int (base)
├── I8 extends Int
├── I16 extends Int
├── I32 extends Int
└── I64 extends Int
```

The `Int` base class stores a `protect I64 value` field and provides methods shared by all signed integer types: `getValue()`, `toString()`, `abs()`, `negate()`, `add()`, `subtract()`, `multiply()`, `divide()`, `modulo()`, `equals()`, `compareTo()`, `lessThan()`, `greaterThan()`, `bitwiseAnd()`, `bitwiseOr()`, `bitwiseXor()`, `shiftLeft()`, `shiftRight()`, `bitwiseNot()`, `isZero()`, `isPositive()`, `isNegative()`, `min()`, `max()`, `hashCode()`, and conversion methods (`toI64()`, `toI32()`, `toFloat()`, `toDouble()`).

**Float hierarchy:**

```
Float (base)
├── F32 extends Float
├── F64 extends Float
└── Double extends Float
```

The `Float` base class stores a `protect F64 value` field and provides: `getValue()`, `toString()`, `abs()`, `negate()`, `add()`, `subtract()`, `multiply()`, `divide()`, `modulo()`, `equals()`, `compareTo()`, `lessThan()`, `greaterThan()`, `isNaN()`, `isInfinite()`, `isFinite()`, `floor()`, `ceil()`, `round()`, `sqrt()`, `power()`, `isZero()`, `isPositive()`, `isNegative()`, `hashCode()`, and conversion methods (`toI64()`, `toInt()`, `toFloat()`, `toDouble()`).

### Method Calls on Primitives

Because all primitives are objects, method calls work directly on literal values:

```uranite
String text = 42.toString()
I64 absolute = (-15).abs()
Boolean zero = 0.isZero()
F64 rounded = 3.14159.round()
Boolean digit = '7'.isDigit()
Boolean empty = "".isEmpty()
```

The semantic analyzer resolves `42.toString()` by finding the `I64` class (since `42` is an integer literal typed as `I64`), locating the `toString()` method in its class hierarchy (inherited from `Int`), and type-checking the call. The codegen then emits native LLVM instructions — no method dispatch overhead occurs.

---

## Zero-Cost Abstractions

### The toLLVMType Mechanism

The key to zero-cost OOP wrappers is the `MIRCodegen::toLLVMType()` function. When mapping a semantic type to an LLVM type, this function recognizes all OOP wrapper class names and maps them directly to their native LLVM types — bypassing any struct or pointer indirection.

For a `Class` type kind, `toLLVMType()` checks the qualified name against every known OOP wrapper:

| Qualified Name Match | LLVM Type Emitted |
|---|---|
| `uranite.language.i64.I64`, `uranite.language.int.Int`, `uranite.language.long.Long` | `i64` |
| `uranite.language.i32.I32`, `uranite.language.integer.Integer` | `i32` |
| `uranite.language.i16.I16` | `i16` |
| `uranite.language.i8.I8`, `uranite.language.byte.Byte` | `i8` |
| `uranite.language.u64.U64`, `uranite.language.uint.UInt` | `i64` |
| `uranite.language.u32.U32` | `i32` |
| `uranite.language.u16.U16` | `i16` |
| `uranite.language.u8.U8` | `i8` |
| `uranite.language.f32.F32` | `float` |
| `uranite.language.f64.F64`, `uranite.language.float.Float`, `uranite.language.double.Double` | `double` |
| `uranite.language.char.Char` | `i32` |
| `uranite.language.boolean.Boolean` | `i1` |
| `uranite.language.string.String` | `ptr` |
| `uranite.builtin.Object` | `ptr` |
| `uranite.language.none.NoneType`, Void variants | `void` |

For any user-defined class that is **not** an OOP wrapper, `toLLVMType()` looks up an LLVM `StructType` by name and returns a pointer to it. Only OOP wrappers get the native-type shortcut.

### OOP Wrapper Recognition

Three helper functions in `qualnames.hpp` classify qualified names:

**`isOopWrapper(qualified)`**: Returns `true` if the qualified name is in the `oopWrapperQualified` set. This set contains 22 entries: all integer wrappers (signed and unsigned), all float wrappers, `Boolean`, `Byte`, `Char`, `Double`, `Float`, `NoneType`, `String`, and `Void`.

**`isIntegerOop(qualified)`**: Returns `true` if the qualified name is an integer OOP wrapper. The set contains 13 entries: `Int`, `I8`, `I16`, `I32`, `I64`, `Integer`, `Long`, `Byte`, `UInt`, `U8`, `U16`, `U32`, `U64`.

**`isFloatOop(qualified)`**: Returns `true` if the qualified name is a float OOP wrapper. The set contains 4 entries: `Float`, `F32`, `F64`, `Double`.

These functions are used throughout the compiler — in the semantic analyzer for type compatibility checks, in codegen for emission decisions, and in the borrow checker for determining value semantics.

### No Boxing, No Heap Allocation

When you write:

```uranite
I64 count = 42
String text = count.toString()
```

The compiler generates:

1. `count` is an `i64` constant `42` — a native 64-bit integer, not a heap-allocated object.
2. `count.toString()` is intercepted by the codegen as a method call on an OOP wrapper. The codegen emits the `snprintf` conversion directly — no virtual dispatch, no vtable lookup, no object allocation.

The result is identical to what a C compiler would produce for formatting an integer as a string. The OOP wrapper abstraction exists only during semantic analysis — it is completely erased by the time LLVM IR is emitted.

---

## The Type Registry

### Primitive Type Initialization

The `Registry` constructor creates all primitive type instances:

| Variable | Constructor | Kind |
|---|---|---|
| `booleanType` | `Type(Kind::Bool, "bool", "uranite.builtin", "uranite.builtin.bool")` | Bool |
| `charType` | `Type(Kind::Char, "char", "uranite.builtin", "uranite.builtin.char")` | Char |
| `stringType` | `Type(Kind::String, "str", "uranite.builtin", "uranite.builtin.str")` | String |
| `voidType` | `Type(Kind::Void, "void", "uranite.builtin", "uranite.builtin.void")` | Void |
| `integer8Type` | `IntegerType(8, true)` | Integer |
| `integer16Type` | `IntegerType(16, true)` | Integer |
| `integer32Type` | `IntegerType(32, true)` | Integer |
| `integer64Type` | `IntegerType(64, true)` | Integer |
| `unsigned8Type` | `IntegerType(8, false)` | Integer |
| `unsigned16Type` | `IntegerType(16, false)` | Integer |
| `unsigned32Type` | `IntegerType(32, false)` | Integer |
| `unsigned64Type` | `IntegerType(64, false)` | Integer |
| `float32Type` | `FloatType(32)` | Float |
| `float64Type` | `FloatType(64)` | Float |
| `objectType` | `ClassType("Object")` | Class |
| `errorType` | `Type(Kind::Error, "<error>", "uranite.builtin", ...)` | Error |

Each `IntegerType` stores its `bitWidth` and `isSigned` flag. Each `FloatType` stores its `bitWidth`. All are assigned the `uranite.builtin` package and their qualified primitive names.

### The primitivesTypes Map

After creating the type instances, the constructor populates the `primitivesTypes` hash map with 22 entries. Each entry maps an OOP wrapper class name to the corresponding primitive type instance:

| Key (Class Name) | Value (Points To) |
|---|---|
| `"Boolean"` | `booleanType` |
| `"Byte"` | `unsigned8Type` |
| `"Char"` | `charType` |
| `"Double"` | `float64Type` |
| `"F32"` | `float32Type` |
| `"F64"` | `float64Type` |
| `"Float"` | `float64Type` |
| `"I8"` | `integer8Type` |
| `"I16"` | `integer16Type` |
| `"I32"` | `integer32Type` |
| `"I64"` | `integer64Type` |
| `"Int"` | `integer64Type` |
| `"Integer"` | `integer32Type` |
| `"Long"` | `integer64Type` |
| `"NoneType"` | `voidType` |
| `"String"` | `stringType` |
| `"U8"` | `unsigned8Type` |
| `"U16"` | `unsigned16Type` |
| `"U32"` | `unsigned32Type` |
| `"U64"` | `unsigned64Type` |
| `"UInt"` | `unsigned64Type` |
| `"Void"` | `voidType` |

This is the bridge between OOP class names and primitive types. When the semantic analyzer encounters the type name "I64" in source code, `lookupType("I64")` finds the entry in `primitivesTypes` and returns the `integer64Type` instance.

### Lookup and Resolution

The `lookupType(name)` method implements the dual-map search:

1. Search `userTypesType` for a user-defined type matching the name.
2. If not found, search `primitivesTypes` for a built-in primitive.
3. If not found in either map, return `nullptr`.

This order means user-defined types take precedence over primitives. If a user defines a class named "I64" (strongly discouraged), it shadows the primitive.

### The isPrimitive Classification

The `Type::isPrimitive()` method returns `true` for six type kinds:

- `Kind::Void`
- `Kind::Bool`
- `Kind::Integer`
- `Kind::Float`
- `Kind::Char`
- `Kind::String`

This classification is used by the compiler for fast decisions about value semantics, storage requirements, and codegen strategy. Primitive types have value semantics (they are copied, not moved), occupy fixed storage, and emit as native LLVM types.

---

## The OOP Wrapper Set

### The oopWrapperQualified Set

A static set of 22 fully-qualified names identifies all OOP wrappers:

```
Boolean, Byte, Char, Double, F32, F64, Float,
I8, I16, I32, I64, Int, Integer, Long,
NoneType, String,
U8, U16, U32, U64, UInt, Void
```

### The integerOopQualified Set

A subset of 13 entries identifies integer OOP wrappers:

```
Int, I8, I16, I32, I64, Integer, Long, Byte,
UInt, U8, U16, U32, U64
```

### The floatOopQualified Set

A subset of 4 entries identifies float OOP wrappers:

```
Float, F32, F64, Double
```

### The isOopWrapper Function

```
isOopWrapper(qualified)  → checks oopWrapperQualified set
isIntegerOop(qualified)  → checks integerOopQualified set
isFloatOop(qualified)    → checks floatOopQualified set
```

These O(1) lookups are used throughout the compiler to decide whether a class type should be treated as a primitive for codegen purposes.

---

## Builtin Identifiers

The `builtinIdentifiers()` set lists all type names that the compiler treats as built-in. These names are pre-registered and cannot be redefined by user code without shadowing:

```
I64, I32, I16, I8, U64, U32, U16, U8,
Int, UInt, Float, Double, String, Boolean, Char, Void,
Object, Memory, Byte, Bool,
Args, Kwargs, Future, Generator,
Error, Exception, Warning, Throwable, Traceback,
ArithmeticError, ZeroDivisionError, OverflowError, UnderflowError,
None, True, False
```

This set is used by the semantic analyzer to distinguish built-in type names from user-defined identifiers.

---

## Examples

### Primitive Declarations

```uranite
I8 small = 127
I16 medium = 32767
I32 standard = 2147483647
I64 large = 9223372036854775807

U8 byte = 255
U16 port = 65535
U32 color = 16777215
U64 bigUnsigned = 18446744073709551615

F32 precise = 3.14
F64 doublePrecise = 3.141592653589793

Boolean active = True
Char letter = 'A'
String greeting = "hello, world"
```

### Method Calls on Primitives {#method-calls-on-primitives-examples}

```uranite
String intText = 42.toString()
I64 absolute = (-100).abs()
Boolean zero = 0.isZero()
Boolean positive = 42.isPositive()
Boolean negative = (-1).isNegative()

F64 rounded = 3.14159.round()
F64 floored = 3.7.floor()
F64 ceiled = 3.2.ceil()
F64 root = 16.0.sqrt()
Boolean nan = (0.0 / 0.0).isNaN()

Boolean alpha = 'A'.isAlpha()
Boolean digit = '5'.isDigit()
Char upper = 'a'.toUpper()
Char lower = 'Z'.toLower()

Boolean empty = "".isEmpty()
I64 length = "hello".length()
String upper = "hello".toUpper()
```

### Type Conversions

```uranite
I64 intValue = 42
F64 floatValue = intValue as F64

I32 narrowed = intValue as I32
I8 truncated = intValue as I8

F64 pi = 3.14
I64 truncatedPi = pi as I64

Char letter = 'A'
I32 codePoint = letter as I32
Char fromCode = 66 as Char

I64 fromFloat = 3.14.toI64()
F64 fromInt = 42.toDouble()
```

### Practical Usage

```uranite
from uranite.io.console import puts

public function fibonacci( I64 count ) -> I64:
    if count <= 1:
        return count
    I64 previous = 0
    I64 current = 1
    I64 index = 2
    while index <= count:
        I64 next = previous + current
        previous = current
        current = next
        index = index + 1
    return current

public function formatTemperature( F64 celsius ) -> String:
    F64 fahrenheit = celsius * 1.8 + 32.0
    String celsiusStr = celsius.toString()
    String fahrenheitStr = fahrenheit.round().toString()
    return celsiusStr + "C = " + fahrenheitStr + "F"

public function classifyCharacter( Char character ) -> String:
    if character.isAlpha():
        if character.isAlpha() and character >= 'A' and character <= 'Z':
            return "uppercase letter"
        return "lowercase letter"
    if character.isDigit():
        return "digit"
    if character.isWhitespace():
        return "whitespace"
    return "symbol"

public function main() -> I32:
    I64 fib10 = fibonacci( 10 )
    puts( fib10.toString() )

    String temp = formatTemperature( 100.0 )
    puts( temp )

    String kind = classifyCharacter( 'Z' )
    puts( kind )

    Boolean isLarge = fib10.isPositive() and fib10 > 50
    puts( isLarge.toString() )

    return 0
```

This example demonstrates integer method calls (`toString()`, `isPositive()`), float method calls (`round()`, `toString()`), character classification (`isAlpha()`, `isDigit()`, `isWhitespace()`), boolean method calls (`toString()`), and type conversions — all compiled to native LLVM instructions with zero overhead from the OOP wrapper layer.
