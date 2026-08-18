# Integer Literals

Uranite supports integer literals in four number bases: decimal, hexadecimal, octal, and binary. All integer literals default to the `I64` type (signed 64-bit integer). This document specifies the syntax for each base, underscore separators, type assignment, narrowing conversions, overflow behavior, and the complete integer type system.

---

## Table of Contents

- [Integer Literals](#integer-literals)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Decimal Literals](#decimal-literals)
  - [Hexadecimal Literals](#hexadecimal-literals)
  - [Octal Literals](#octal-literals)
  - [Binary Literals](#binary-literals)
  - [Underscore Separators](#underscore-separators)
    - [Placement Rules](#placement-rules)
    - [Recommended Grouping](#recommended-grouping)
  - [Negative Integers](#negative-integers)
  - [Default Type: I64](#default-type-i64)
    - [Method Access on Integers](#method-access-on-integers)
    - [Type Narrowing Through Annotations](#type-narrowing-through-annotations)
  - [The Integer Type System](#the-integer-type-system)
    - [Signed Types](#signed-types)
    - [Unsigned Types](#unsigned-types)
    - [Alias Types](#alias-types)
    - [Choosing an Integer Type](#choosing-an-integer-type)
  - [Overflow and Range](#overflow-and-range)
    - [I64 Range](#i64-range)
    - [All Integer Ranges](#all-integer-ranges)
    - [Narrowing Overflow](#narrowing-overflow)
    - [Arithmetic Overflow](#arithmetic-overflow)
  - [Leading Zero Is Decimal, Not Octal](#leading-zero-is-decimal-not-octal)
  - [Examples](#examples)
    - [Valid Integer Literals](#valid-integer-literals)
    - [Invalid Integer Literals](#invalid-integer-literals)
    - [Practical Usage](#practical-usage)

---

## Overview

An integer literal is a sequence of digits optionally preceded by a base prefix (`0x`, `0o`, `0b`). Underscores may appear anywhere within the digit sequence as visual separators and are silently stripped before the value is computed.

| Base | Prefix | Valid Digits | Example |
|---|---|---|---|
| Decimal | (none) | `0`-`9` | `42`, `1000`, `999_999` |
| Hexadecimal | `0x` or `0X` | `0`-`9`, `a`-`f`, `A`-`F` | `0xFF`, `0x2A`, `0X1F4` |
| Octal | `0o` or `0O` | `0`-`7` | `0o52`, `0O777`, `0o17` |
| Binary | `0b` or `0B` | `0`, `1` | `0b101010`, `0B1111_0000` |

Regardless of the base used in source code, the resulting value is always the same signed 64-bit integer. `42`, `0x2A`, `0o52`, and `0b101010` all produce the same value.

---

## Decimal Literals

Decimal literals consist of one or more digits `0`-`9`, optionally separated by underscores. No prefix is required:

```uranite
I64 zero = 0
I64 answer = 42
I64 million = 1_000_000
I64 maxPort = 65535
```

Decimal is the default base. Any numeric literal that does not start with `0x`, `0o`, or `0b` is interpreted as decimal. If a decimal digit sequence is followed by `.` and another digit, the literal transitions to a floating-point literal instead (covered in the [Float Literals](float-literals.md) document). Similarly, an `e` or `E` after digits begins scientific notation, which also produces a float.

---

## Hexadecimal Literals

Hexadecimal literals begin with the prefix `0x` or `0X`, followed by one or more hex digits (`0`-`9`, `a`-`f`, `A`-`F`), optionally separated by underscores:

```uranite
I64 red = 0xFF0000
I64 permissions = 0x1A4
I64 mask = 0xFF_FF_FF_FF
I64 address = 0xDEAD_BEEF
```

Hexadecimal digits are case-insensitive. `0xFF`, `0XFF`, `0xff`, and `0Xff` all produce the same numeric value (255).

Hexadecimal is the natural choice for color values, memory addresses, bitmasks, and any context where values are conventionally expressed in base 16:

```uranite
I64 white = 0xFFFFFF
I64 transparent = 0x00000000
I64 channelMask = 0xFF
```

---

## Octal Literals

Octal literals begin with the prefix `0o` or `0O`, followed by one or more octal digits (`0`-`7`), optionally separated by underscores:

```uranite
I64 fileMode = 0o644
I64 fullPerms = 0o777
I64 readOnly = 0o444
```

Uranite uses the explicit `0o` prefix for octal, not the C-style leading-zero convention. A literal `0644` is a **decimal** number with value 644, not an octal number with value 420. This eliminates the common source of bugs in C where leading zeros silently change the numeric base. See [Leading Zero Is Decimal, Not Octal](#leading-zero-is-decimal-not-octal).

Octal is primarily used for Unix file permissions, where the three-digit grouping (owner/group/other) maps naturally to octal:

| Octal | Binary | Meaning |
|---|---|---|
| `0o7` | `111` | Read + Write + Execute |
| `0o6` | `110` | Read + Write |
| `0o5` | `101` | Read + Execute |
| `0o4` | `100` | Read only |
| `0o0` | `000` | No permissions |

---

## Binary Literals

Binary literals begin with the prefix `0b` or `0B`, followed by one or more binary digits (`0` or `1`), optionally separated by underscores:

```uranite
I64 flags = 0b1010
I64 byteMask = 0b1111_0000
I64 singleBit = 0b0000_0000_0000_0001
I64 pattern = 0B10101010
```

Binary literals are particularly useful for bit flags, hardware register values, and bitmask operations where individual bit positions carry meaning:

```uranite
const I64 FLAG_READ = 0b0000_0100
const I64 FLAG_WRITE = 0b0000_0010
const I64 FLAG_EXEC = 0b0000_0001

public function hasFlag( I64 mode, I64 flag ) -> Boolean:
    return ( mode & flag ) != 0
```

The underscore separators in binary literals are especially valuable — `0b1111_0000_1010_0101` is far more readable than `0b1111000010100101`.

---

## Underscore Separators

Underscores within numeric literals serve as visual separators to improve readability. They are silently stripped before the value is computed. The source text `1_000_000` and `1000000` produce exactly the same value.

### Placement Rules

Underscores can appear anywhere within the digit sequence after the base prefix:

| Literal | Value | Notes |
|---|---|---|
| `1_000` | 1000 | Standard grouping |
| `1_000_000` | 1000000 | Multiple groups |
| `0xFF_FF` | 65535 | Hex grouping |
| `0b1111_0000` | 240 | Binary byte grouping |
| `1__000` | 1000 | Multiple consecutive underscores (accepted, unusual) |
| `0x_FF` | 255 | Leading underscore after prefix (accepted, not recommended) |

An underscore cannot appear **before** the first digit of a literal. A bare `_` at the start of a token is interpreted as the beginning of an identifier, not a number. The tokens `_42` and `_0xFF` are identifiers, not integer literals.

### Recommended Grouping

Group digits in patterns that match each base's conventional formatting:

- **Decimal:** Groups of three (`1_000_000_000`)
- **Hexadecimal:** Groups of two or four (`0xFF_FF`, `0x0000_FFFF`)
- **Binary:** Groups of four or eight (`0b1111_0000`, `0b10101010_11001100`)
- **Octal:** Groups of three (`0o777_000`)

```uranite
I64 population = 8_000_000_000
I64 colorValue = 0xFF_00_FF
I64 bitmask = 0b1111_0000_1010_0101
I64 largeOctal = 0o777_777
```

---

## Negative Integers

Negative integer values are expressed using the unary minus operator applied to a positive literal:

```uranite
I64 negative = -42
I64 minByte = -128
I64 temperature = -15
```

The minus sign is not part of the literal itself — it is a separate unary operator. This means `-42` is parsed as the operator `-` applied to the literal `42`, not as a single negative literal token. In practice, this distinction is invisible to the programmer — the compiler evaluates the negation at compile time and produces the expected negative value.

The minimum value of `I64` is -9,223,372,036,854,775,808. This value can be written directly:

```uranite
I64 minimum = -9_223_372_036_854_775_808
```

---

## Default Type: I64

Every integer literal defaults to the `I64` type — a signed 64-bit integer. This is true regardless of the base used or the magnitude of the value. The literals `42`, `0x2A`, `0o52`, and `0b101010` all produce an `I64` value.

### Method Access on Integers

Because `I64` is an object type (not a raw primitive), integer values have method access. Every integer literal supports methods inherited from the `Object` class and the arithmetic interfaces:

```uranite
I64 value = 42
String text = value.toString()
I64 hash = value.hashCode()
Boolean same = value.equals( 42 )
```

The `toString()` method converts the integer to its decimal string representation. The `hashCode()` method returns a hash value suitable for use in hash-based collections. The `equals()` method performs value comparison.

### Type Narrowing Through Annotations

When an integer literal is assigned to a variable with a narrower type annotation, the compiler performs implicit narrowing conversion:

```uranite
I64 full = 42
I32 medium = 42
I16 small = 100
I8 tiny = 7
U64 unsigned = 255
U8 byte = 0xFF
```

In each case, the literal is initially an `I64` value. The variable's type annotation determines the final storage type. The compiler handles the conversion automatically — no explicit cast is required.

If the literal value exceeds the range of the target type, the value is silently truncated. Writing `I8 overflow = 300` compiles without error, but the stored value wraps around within the `I8` range (-128 to 127). See [Narrowing Overflow](#narrowing-overflow).

---

## The Integer Type System

Uranite provides a complete set of fixed-width integer types in both signed and unsigned variants.

### Signed Types

Signed integers use two's complement representation. The most significant bit is the sign bit.

| Type | Bits | Minimum | Maximum |
|---|---|---|---|
| `I8` | 8 | -128 | 127 |
| `I16` | 16 | -32,768 | 32,767 |
| `I32` | 32 | -2,147,483,648 | 2,147,483,647 |
| `I64` | 64 | -9,223,372,036,854,775,808 | 9,223,372,036,854,775,807 |

### Unsigned Types

Unsigned integers represent only non-negative values. All bits contribute to the magnitude.

| Type | Bits | Minimum | Maximum |
|---|---|---|---|
| `U8` | 8 | 0 | 255 |
| `U16` | 16 | 0 | 65,535 |
| `U32` | 32 | 0 | 4,294,967,295 |
| `U64` | 64 | 0 | 18,446,744,073,709,551,615 |

### Alias Types

Three additional integer types provide platform-aware or semantic naming:

| Type | Description |
|---|---|
| `Int` | Platform-width signed integer (64-bit on supported systems). |
| `UInt` | Platform-width unsigned integer (64-bit on supported systems). |
| `Byte` | Unsigned 8-bit integer for raw byte data. |

### Choosing an Integer Type

Use `I64` (or `Int`) as the default integer type for most purposes. Choose a specific width only when:

- **Interfacing with external APIs** that require a specific width (e.g., `I32` for system call return values)
- **Working with binary data** where field sizes are fixed (e.g., `U8` for individual bytes, `U16` for network port numbers)
- **Memory-constrained collections** where millions of values benefit from smaller storage (e.g., `I8` for status codes in a large array)
- **Bitwise operations** where the width affects mask behavior (e.g., `U32` for 32-bit hash values)

```uranite
I64 generalPurpose = 42
I32 exitCode = 0
U16 portNumber = 8080
U8 singleByte = 0xFF
I8 statusCode = -1
```

---

## Overflow and Range

### I64 Range

The default `I64` type represents a signed 64-bit integer:

| Bound | Value | Expression |
|---|---|---|
| Minimum | -9,223,372,036,854,775,808 | -2^63 |
| Maximum | 9,223,372,036,854,775,807 | 2^63 - 1 |

Any literal within this range is correctly represented. Literals outside this range cause a compilation error.

### All Integer Ranges

| Type | Minimum | Maximum |
|---|---|---|
| `I8` | -128 | 127 |
| `I16` | -32,768 | 32,767 |
| `I32` | -2,147,483,648 | 2,147,483,647 |
| `I64` | -9,223,372,036,854,775,808 | 9,223,372,036,854,775,807 |
| `U8` | 0 | 255 |
| `U16` | 0 | 65,535 |
| `U32` | 0 | 4,294,967,295 |
| `U64` | 0 | 18,446,744,073,709,551,615 |

### Narrowing Overflow

When an integer literal is assigned to a type narrower than `I64`, the value must fit within the target type's range. If it does not, the value is silently truncated to the target width using two's complement wrapping:

```uranite
I8 wrapped = 200
```

The value 200 exceeds the `I8` range (-128 to 127). The stored value wraps to -56 (200 - 256). This compiles without error or warning. To avoid surprises, ensure that literal values fit within the declared type's range.

### Arithmetic Overflow

Uranite provides wrapping arithmetic for integer addition, subtraction, and multiplication. When an operation exceeds the type's range, the result wraps around using two's complement semantics rather than producing an error at runtime.

Division and modulo operations include automatic zero-checks. Dividing by zero raises a `ZeroDivisionError` at runtime rather than producing undefined behavior:

```uranite
public function safeDivide( I64 numerator, I64 denominator ) -> I64:
    return numerator / denominator
```

If `denominator` is zero, the compiler's auto-inserted zero-check raises `ZeroDivisionError` before the division executes.

---

## Leading Zero Is Decimal, Not Octal

Uranite does not follow the C convention where a leading zero makes a literal octal. A literal `0644` is decimal 644, not octal 420. Only the explicit `0o` prefix triggers octal interpretation:

| Literal | Base | Value |
|---|---|---|
| `644` | Decimal | 644 |
| `0644` | Decimal | 644 |
| `0o644` | Octal | 420 |

This design eliminates a notorious class of bugs from C, where code like `int perms = 0755;` silently becomes octal 493 instead of decimal 755. In Uranite, `I64 perms = 0755` is unambiguously decimal 755, and `I64 perms = 0o755` is explicitly octal 493.

The rule is simple: if there is no prefix, the literal is decimal. Always.

---

## Examples

### Valid Integer Literals

```uranite
I64 decimal = 42
I64 negative = -17
I64 zero = 0
I64 large = 9_223_372_036_854_775_807

I64 hex = 0xFF
I64 hexUpper = 0XAB
I64 hexGrouped = 0xFF_FF_FF_FF

I64 octal = 0o755
I64 octalGrouped = 0o777_000

I64 binary = 0b1010
I64 binaryGrouped = 0b1111_0000_1010_0101
I64 binaryUpper = 0B1100_0011
```

All of these compile without error. Each produces a valid `I64` value.

### Invalid Integer Literals

The following are **not** valid integer literals:

| Literal | Problem |
|---|---|
| `0x` | Hex prefix with no digits. |
| `0b2` | `2` is not a valid binary digit. Only `0` and `1` are accepted. |
| `0o8` | `8` is not a valid octal digit. Only `0`-`7` are accepted. |
| `_42` | Underscore at the start is an identifier character, not a number start. |

Note that `0789` is a valid **decimal** literal with value 789. The leading `0` is part of the decimal digit sequence. Only the explicit `0o` prefix triggers octal parsing.

### Practical Usage

A complete example demonstrating integer literals across all four bases in realistic contexts:

```uranite
from uranite.io.console import puts

const I64 MAX_CONNECTIONS = 1024
const I64 DEFAULT_BUFFER = 4_096
const I64 COLOR_MASK = 0x00FF_FFFF
const I64 FLAG_READ = 0b0000_0100
const I64 FLAG_WRITE = 0b0000_0010
const I64 FLAG_EXEC = 0b0000_0001

public function hasPermission( I64 mode, I64 flag ) -> Boolean:
    return ( mode & flag ) != 0

public function extractRed( I64 color ) -> I64:
    return ( color >> 16 ) & 0xFF

public function extractGreen( I64 color ) -> I64:
    return ( color >> 8 ) & 0xFF

public function extractBlue( I64 color ) -> I64:
    return color & 0xFF

public function main() -> I32:
    I64 permissions = 0o755
    if hasPermission( permissions, FLAG_READ ):
        puts( "read access granted" )

    I64 skyBlue = 0x87_CE_EB
    I64 red = extractRed( skyBlue )
    I64 green = extractGreen( skyBlue )
    I64 blue = extractBlue( skyBlue )
    puts( "Red: " + red.toString() )
    puts( "Green: " + green.toString() )
    puts( "Blue: " + blue.toString() )

    I64 combined = FLAG_READ | FLAG_WRITE
    puts( "Combined flags: " + combined.toString() )

    return 0
```

This example demonstrates decimal constants with underscore grouping, hexadecimal color values with bit-shifting extraction, binary bit flags with bitwise OR combination, and octal file permissions — each base used in its natural domain.
