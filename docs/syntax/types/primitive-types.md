# Primitive Types

Uranite provides a fixed set of built-in scalar types that form the foundation of the type system. Every primitive type has a known size, a defined value range, and zero-cost wrapper methods inherited from the [Language Types](../../stdlib/language/README.md) standard library.

---

## Table of Contents

- [Primitive Types](#primitive-types)
  - [Table of Contents](#table-of-contents)
  - [Integer Types](#integer-types)
  - [Floating-Point Types](#floating-point-types)
  - [Boolean Type](#boolean-type)
  - [Character Type](#character-type)
  - [String Type](#string-type)
  - [Void and None](#void-and-none)

---

## Integer Types

| Type | Size | Range |
|---|---|---|
| `I8` | 8-bit signed | -128 to 127 |
| `I16` | 16-bit signed | -32,768 to 32,767 |
| `I32` | 32-bit signed | -2,147,483,648 to 2,147,483,647 |
| `I64` | 64-bit signed | -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807 |
| `U8` | 8-bit unsigned | 0 to 255 |
| `U16` | 16-bit unsigned | 0 to 65,535 |
| `U32` | 32-bit unsigned | 0 to 4,294,967,295 |
| `U64` | 64-bit unsigned | 0 to 18,446,744,073,709,551,615 |

**Convenience names:** `Int` and `Integer` are 64-bit signed (`I64`). `UInt` is 64-bit unsigned (`U64`). `Byte` is 8-bit unsigned (`U8`). `Long` is 64-bit signed (`I64`).

Integer arithmetic uses wrapping behavior for addition, subtraction, and multiplication. Division and modulo include automatic zero-checks.

For methods and operations, see [Integer](../../stdlib/language/integer.md).

---

## Floating-Point Types

| Type | Size | Precision |
|---|---|---|
| `F32` | 32-bit | Single-precision IEEE 754 |
| `F64` | 64-bit | Double-precision IEEE 754 |

**Convenience names:** `Float` and `Double` are both 64-bit (`F64`).

Untyped float literals default to `F64`. Float operations follow IEEE 754 semantics.

For methods and operations, see [Floating](../../stdlib/language/floating.md).

---

## Boolean Type

The `Boolean` type holds exactly two values: `True` and `False`. Both are case-sensitive keywords. Lowercase `true` and `false` are not valid.

Uranite requires explicit Boolean conditions — there is no implicit truthiness. Logical operators `and`, `or`, and `not` operate exclusively on `Boolean` values and evaluate both operands.

For methods and operations, see [Boolean](../../stdlib/language/boolean.md).

---

## Character Type

The `Char` type represents a single 32-bit Unicode code point. Character literals use single quotes: `'A'`, `'\n'`, `'A'`.

For methods and operations, see [Char](../../stdlib/language/char.md).

---

## String Type

The `String` type holds immutable UTF-8 encoded text. String literals use double quotes: `"hello"`. Strings support escape sequences (`\n`, `\t`, `\xHH`, `\uXXXX`).

For methods and operations, see [String](../../stdlib/language/string.md).

---

## Void and None

`Void` is the return type for functions that produce no value. It cannot be used as a variable type.

`None` represents the absence of a value. It is the only value assignable to optional types (`?T`). `None` is not assignable to non-optional types.

For details, see [Void and None Types](void-and-none-types.md).
