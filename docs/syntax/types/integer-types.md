# Integer Types

---

## Table of Contents

- [Integer Types](#integer-types)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Signed Integer Types](#signed-integer-types)
    - [I8](#i8)
    - [I16](#i16)
    - [I32](#i32)
    - [I64](#i64)
    - [Int](#int)
    - [Integer](#integer)
    - [Long](#long)
  - [Unsigned Integer Types](#unsigned-integer-types)
    - [U8](#u8)
    - [U16](#u16)
    - [U32](#u32)
    - [U64](#u64)
    - [UInt](#uint)
    - [Byte](#byte)
  - [Default Integer Inference](#default-integer-inference)
  - [Value Ranges](#value-ranges)
    - [Signed Ranges](#signed-ranges)
    - [Unsigned Ranges](#unsigned-ranges)
  - [Integer Literals](#integer-literals)
    - [Decimal Literals](#decimal-literals)
    - [Hexadecimal Literals](#hexadecimal-literals)
    - [Octal Literals](#octal-literals)
    - [Binary Literals](#binary-literals)
  - [Arithmetic Operations](#arithmetic-operations)
    - [Basic Arithmetic](#basic-arithmetic)
    - [Compound Assignment](#compound-assignment)
    - [Unary Negation](#unary-negation)
    - [Division Semantics](#division-semantics)
    - [Modulo Semantics](#modulo-semantics)
  - [Overflow Behavior](#overflow-behavior)
    - [Wrapping Arithmetic](#wrapping-arithmetic)
    - [When Overflow Matters](#when-overflow-matters)
  - [Bitwise Operations](#bitwise-operations)
    - [Bitwise AND OR and XOR](#bitwise-and-or-and-xor)
    - [Shift Operations](#shift-operations)
    - [Bitwise NOT](#bitwise-not)
    - [Bitwise Methods](#bitwise-methods)
  - [Type Casting](#type-casting)
    - [The as Keyword](#the-as-keyword)
    - [Widening Conversions](#widening-conversions)
    - [Narrowing Conversions](#narrowing-conversions)
    - [Integer to Float](#integer-to-float)
    - [Float to Integer](#float-to-integer)
    - [Integer to Character](#integer-to-character)
    - [Mixed-Width Arithmetic](#mixed-width-arithmetic)
  - [Integer Methods](#integer-methods)
    - [Signed Integer Methods](#signed-integer-methods)
    - [Value Query Methods](#value-query-methods)
    - [Arithmetic Methods](#arithmetic-methods)
    - [Comparison Methods](#comparison-methods)
    - [Bitwise Methods Reference](#bitwise-methods-reference)
    - [Conversion Methods](#conversion-methods)
    - [Calling Methods on Literals](#calling-methods-on-literals)
  - [Practical Examples](#practical-examples)
    - [Power of Two Detection](#power-of-two-detection)
    - [Bit Counting](#bit-counting)
    - [Value Clamping](#value-clamping)
    - [Temperature Conversion with Integers](#temperature-conversion-with-integers)
    - [Byte Extraction](#byte-extraction)

---

## Overview

Uranite provides twelve integer types across four bit widths and two signedness categories. Integer types store whole numbers with fixed storage sizes, making them ideal for counters, indices, arithmetic, memory addresses, bitwise manipulation, and any computation involving discrete values.

| Category | Types | Bit Widths |
|---|---|---|
| Signed | `I8`, `I16`, `I32`, `I64`, `Int`, `Integer`, `Long` | 8, 16, 32, 64 |
| Unsigned | `U8`, `U16`, `U32`, `U64`, `UInt`, `Byte` | 8, 16, 32, 64 |

Signed integers can represent negative, zero, and positive values. Unsigned integers represent non-negative values only, doubling the positive range for the same bit width. All integer types use two's complement representation and compile to native machine instructions with zero overhead.

---

## Signed Integer Types

### I8

An 8-bit signed integer. Stores values from **-128** to **127**.

```uranite
I8 temperature = -40
I8 exitCode = 0
I8 maxI8 = 127
```

Use `I8` when memory is constrained and values fit within the 8-bit range, such as raw protocol fields, small flags, or tightly packed data structures.

### I16

A 16-bit signed integer. Stores values from **-32,768** to **32,767**.

```uranite
I16 elevation = 8848
I16 signalStrength = -72
I16 offset = -1024
```

Use `I16` for medium-range values like audio samples, sensor readings, or network protocol fields where 32-bit storage is wasteful.

### I32

A 32-bit signed integer. Stores values from **-2,147,483,648** to **2,147,483,647**.

```uranite
I32 population = 1400000000
I32 fileDescriptor = 3
I32 errorCode = -1
```

`I32` is the standard choice for general-purpose integer work when the full 64-bit range is unnecessary. The `main()` function returns `I32` as its exit code.

### I64

A 64-bit signed integer. Stores values from **-9,223,372,036,854,775,808** to **9,223,372,036,854,775,807**.

```uranite
I64 worldPopulation = 8000000000
I64 nanoseconds = 1625000000000
I64 fileSize = 4294967296
```

`I64` is the default integer type. When you write a bare integer literal without a type annotation, the compiler infers `I64`. This makes `I64` the workhorse type for most integer work — counters, indices, sizes, timestamps, and general arithmetic.

### Int

A 64-bit signed integer. `Int` is the natural, readable name for the most commonly used integer type.

```uranite
Int count = 42
Int total = count + 10
```

`Int` and `I64` are the same type at every level — declaration, assignment, method calls, and storage. You can use them interchangeably in any context. Most Uranite code uses `Int` for readability.

### Integer

A 32-bit signed integer. `Integer` provides an explicit, readable name for 32-bit integer values.

```uranite
Integer statusCode = 200
Integer retryCount = 3
```

`Integer` and `I32` are the same type. Use whichever name reads better in context.

### Long

A 64-bit signed integer. `Long` provides an explicit name emphasizing the 64-bit width.

```uranite
Long bigNumber = 9223372036854775807
Long timestamp = 1625000000000
```

`Long` and `I64` are the same type. Some developers prefer `Long` when the 64-bit width is semantically important.

---

## Unsigned Integer Types

Unsigned integers store non-negative whole numbers only. They provide a larger positive range than their signed counterparts of the same bit width, at the cost of not representing negative values.

### U8

An 8-bit unsigned integer. Stores values from **0** to **255**.

```uranite
U8 redChannel = 255
U8 asciiCode = 65
U8 bitmask = 0xFF
```

`U8` is the natural type for raw byte data — pixel channels, ASCII values, binary protocol bytes, and byte-level buffers.

### U16

A 16-bit unsigned integer. Stores values from **0** to **65,535**.

```uranite
U16 portNumber = 8080
U16 packetLength = 1500
U16 characterCode = 9829
```

### U32

A 32-bit unsigned integer. Stores values from **0** to **4,294,967,295**.

```uranite
U32 colorValue = 16777215
U32 filePermissions = 493
U32 checksum = 3735928559
```

### U64

A 64-bit unsigned integer. Stores values from **0** to **18,446,744,073,709,551,615**.

```uranite
U64 hashValue = 4294967296
U64 memorySize = 8589934592
U64 uniqueIdentifier = 1000000000000
```

### UInt

A 64-bit unsigned integer. `UInt` provides a readable name for the default unsigned integer type.

```uranite
UInt counter = 0
UInt bufferSize = 4096
```

`UInt` and `U64` are the same type.

### Byte

An 8-bit unsigned integer. `Byte` provides a semantically clear name for byte-level data.

```uranite
Byte rawByte = 171
Byte nullTerminator = 0
Byte flag = 0xAB
```

`Byte` and `U8` are the same type. `Byte` is preferred when working with raw binary data, byte buffers, or I/O operations because the name communicates intent more clearly.

---

## Default Integer Inference

When you write a bare integer literal without a type annotation, Uranite always infers `I64`:

```uranite
function example() -> Void:
    I64 explicit = 42
    Int alsoExplicit = 42
```

Both produce identical results. The literal `42` is always `I64` unless assigned to a variable with a different integer type annotation, in which case the compiler checks that the value fits within the target type's range and performs the appropriate conversion automatically.

To store a value in a smaller or unsigned type, provide an explicit type annotation:

```uranite
I8 small = 42
U16 port = 8080
I32 code = 200
```

---

## Value Ranges

### Signed Ranges

Signed integers use two's complement representation. The range for an N-bit signed integer is **-2^(N-1)** to **2^(N-1) - 1**.

| Type | Minimum | Maximum |
|---|---|---|
| `I8` | -128 | 127 |
| `I16` | -32,768 | 32,767 |
| `I32` / `Integer` | -2,147,483,648 | 2,147,483,647 |
| `I64` / `Int` / `Long` | -9,223,372,036,854,775,808 | 9,223,372,036,854,775,807 |

### Unsigned Ranges

Unsigned integers represent non-negative values only. The range for an N-bit unsigned integer is **0** to **2^N - 1**.

| Type | Minimum | Maximum |
|---|---|---|
| `U8` / `Byte` | 0 | 255 |
| `U16` | 0 | 65,535 |
| `U32` | 0 | 4,294,967,295 |
| `U64` / `UInt` | 0 | 18,446,744,073,709,551,615 |

---

## Integer Literals

Integer values can be written in four numeric bases. All literal forms produce the same underlying value — only the representation in source code differs.

### Decimal Literals

Standard base-10 notation. This is the most common form:

```uranite
I64 count = 42
I64 million = 1000000
I64 negative = -500
```

### Hexadecimal Literals

Prefixed with `0x` or `0X`. Digits `0`-`9` and `a`-`f` (case-insensitive). Widely used for bit patterns, memory addresses, and color values:

```uranite
I64 hexValue = 0xFF
I64 colorRed = 0xFF0000
U32 address = 0xDEADBEEF
U8 mask = 0x0F
```

### Octal Literals

Prefixed with `0o` or `0O`. Digits `0`-`7`. Commonly used for file permissions:

```uranite
I64 permissions = 0o755
I64 readOnly = 0o444
```

### Binary Literals

Prefixed with `0b` or `0B`. Digits `0` and `1`. Useful for visualizing bit patterns:

```uranite
I64 flags = 0b10101010
I64 singleBit = 0b00000001
```

---

## Arithmetic Operations

### Basic Arithmetic

Uranite provides five arithmetic operators for integers:

| Operator | Operation | Example |
|---|---|---|
| `+` | Addition | `10 + 3` produces `13` |
| `-` | Subtraction | `10 - 3` produces `7` |
| `*` | Multiplication | `10 * 3` produces `30` |
| `/` | Division | `10 / 3` produces `3` |
| `%` | Modulo (remainder) | `10 % 3` produces `1` |

```uranite
I64 sum = 100 + 50
I64 difference = 100 - 50
I64 product = 100 * 50
I64 quotient = 100 / 50
I64 remainder = 100 % 50
```

Both operands must be the same type. If you need to combine integers of different widths, use the `as` keyword to convert one operand explicitly, or let the compiler auto-coerce (see [Mixed-Width Arithmetic](#mixed-width-arithmetic)).

### Compound Assignment

Compound assignment operators combine an arithmetic operation with assignment:

| Operator | Equivalent |
|---|---|
| `+=` | `value = value + operand` |
| `-=` | `value = value - operand` |
| `*=` | `value = value * operand` |
| `/=` | `value = value / operand` |

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 value = 10
    value += 5
    puts( value.toString() )
    value -= 3
    puts( value.toString() )
    value *= 2
    puts( value.toString() )
    value /= 4
    puts( value.toString() )
    return 0
```

This outputs:

```
15
12
24
6
```

### Unary Negation

The unary minus operator `-` inverts the sign of an integer value:

```uranite
I64 positive = 42
I64 negative = -positive
```

To negate a literal and then call a method on it, wrap the negated literal in parentheses:

```uranite
I64 absolute = (-15).abs()
```

Without parentheses, the minus sign would bind to `15` first, and the method call would apply incorrectly.

### Division Semantics

Integer division truncates toward zero, discarding the fractional part:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 quotient = 17 / 5
    puts( quotient.toString() )
    I64 negativeDiv = -17 / 5
    puts( negativeDiv.toString() )
    return 0
```

This outputs:

```
3
-3
```

Both `17 / 5` and `-17 / 5` truncate toward zero. Positive results round down; negative results round toward zero (up in absolute magnitude).

**Division by zero** causes the program to terminate with a hardware fault. Always validate divisors before performing division.

### Modulo Semantics

The modulo operator `%` returns the remainder after integer division. The sign of the result matches the sign of the dividend (left operand):

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 remainder = 17 % 5
    puts( remainder.toString() )
    I64 negativeRem = -17 % 5
    puts( negativeRem.toString() )
    return 0
```

This outputs:

```
2
-2
```

The relationship between division and modulo always holds: `dividend == (dividend / divisor) * divisor + (dividend % divisor)`.

---

## Overflow Behavior

### Wrapping Arithmetic

Integer arithmetic in Uranite uses wrapping overflow. When an operation produces a result that exceeds the type's range, the value wraps around according to two's complement rules:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I8 maxI8 = 127
    I8 overflow = maxI8 + 1
    puts( overflow.toString() )
    I64 maxI64 = 9223372036854775807
    I64 overflowed = maxI64 + 1
    puts( overflowed.toString() )
    return 0
```

This outputs:

```
-128
-9223372036854775808
```

Adding 1 to the maximum `I8` value (127) wraps to the minimum `I8` value (-128). Adding 1 to the maximum `I64` value wraps to the minimum `I64` value. This wrapping behavior is deterministic and consistent — the result is always the mathematical result modulo 2^N, where N is the bit width.

### When Overflow Matters

Wrapping overflow is silent — no error is raised, no exception is thrown. This means your program continues running with a wrapped value that may be far from the mathematically correct result. Overflow matters most in:

- **Loop counters** that might exceed their type's range
- **Accumulator variables** summing many values
- **Multiplication** of values near the type's limits
- **Subtraction** on unsigned types that might go below zero

When overflow is a concern, either use a wider type or add explicit range checks before the operation:

```uranite
from uranite.io.console import puts

public function safeAdd( I64 leftValue, I64 rightValue, I64 maxLimit ) -> I64:
    if rightValue > 0 and leftValue > maxLimit - rightValue:
        return maxLimit
    return leftValue + rightValue

public function main() -> I32:
    I64 result = safeAdd( 9223372036854775800, 100, 9223372036854775807 )
    puts( result.toString() )
    return 0
```

---

## Bitwise Operations

Bitwise operations manipulate individual bits within integer values. They compile directly to single machine instructions and are essential for low-level programming — flag management, masking, protocol encoding, and hardware interaction.

### Bitwise AND OR and XOR

| Operator | Name | Description |
|---|---|---|
| `&` | AND | Each result bit is 1 only if both input bits are 1 |
| `\|` | OR | Each result bit is 1 if either input bit is 1 |
| `^` | XOR | Each result bit is 1 if the input bits differ |

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 andResult = 0xFF00 & 0x0F0F
    puts( "AND: " + andResult.toString() )
    I64 orResult = 0xFF00 | 0x0F0F
    puts( "OR: " + orResult.toString() )
    I64 xorResult = 0xFF00 ^ 0x0F0F
    puts( "XOR: " + xorResult.toString() )
    return 0
```

This outputs:

```
AND: 3840
OR: 65295
XOR: 61455
```

In hexadecimal, `0xFF00 & 0x0F0F` is `0x0F00` (3840), `0xFF00 | 0x0F0F` is `0xFF0F` (65295), and `0xFF00 ^ 0x0F0F` is `0xF00F` (61455).

### Shift Operations

| Operator | Name | Description |
|---|---|---|
| `<<` | Left shift | Shifts bits left, filling vacated positions with zeros |
| `>>` | Right shift | Shifts bits right, preserving the sign bit (arithmetic shift) |

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 shiftedLeft = 1 << 10
    puts( "1 << 10 = " + shiftedLeft.toString() )
    I64 shiftedRight = 1024 >> 3
    puts( "1024 >> 3 = " + shiftedRight.toString() )
    return 0
```

This outputs:

```
1 << 10 = 1024
1024 >> 3 = 128
```

Left shifting by N positions multiplies by 2^N. Right shifting by N positions divides by 2^N (with truncation toward negative infinity for signed values).

The right shift operator is an **arithmetic shift** — for negative values, the vacated high bits are filled with 1s (preserving the sign), not 0s. This means negative values remain negative after a right shift.

### Bitwise NOT

The unary `~` operator inverts every bit of its operand — each 0 becomes 1 and each 1 becomes 0:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 notResult = ~0xFF
    puts( notResult.toString() )
    return 0
```

This outputs `-256`. The value `0xFF` (binary: 8 ones) is inverted to produce a 64-bit value with all bits set except the lowest 8, which in two's complement is -256.

### Bitwise Methods

In addition to operators, integer objects provide named methods for bitwise operations:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 hexLiteral = 0xFF
    I64 masked = hexLiteral.bitwiseAnd( 0x0F )
    puts( masked.toString() )
    I64 ored = hexLiteral.bitwiseOr( 0xF00 )
    puts( ored.toString() )
    I64 xored = hexLiteral.bitwiseXor( 0xFF )
    puts( xored.toString() )
    I64 shifted = 1.shiftLeft( 8 )
    puts( shifted.toString() )
    I64 rightShifted = 256.shiftRight( 4 )
    puts( rightShifted.toString() )
    return 0
```

This outputs:

```
15
4095
0
256
16
```

The method forms are useful when working with variables where the intent reads more clearly as a method call than as a symbolic operator.

---

## Type Casting

### The as Keyword

Explicit type conversion between integer types uses the `as` keyword:

```uranite
I64 wide = 1000
I32 narrowed = wide as I32
I8 tiny = narrowed as I8
```

The `as` keyword tells the compiler exactly what conversion to perform. It is required when the conversion could lose information (narrowing) and is also accepted for widening conversions where you want to be explicit about intent.

### Widening Conversions

Widening moves a value from a smaller type to a larger type. No information is lost — the value is preserved exactly:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I8 narrow = 42
    I64 widened = narrow as I64
    puts( widened.toString() )
    return 0
```

This outputs `42`. The value is preserved perfectly because `I64` can represent every value that `I8` can.

Widening is always safe:

| From | To | Safety |
|---|---|---|
| `I8` | `I16`, `I32`, `I64` | Always safe |
| `I16` | `I32`, `I64` | Always safe |
| `I32` | `I64` | Always safe |
| `U8` | `U16`, `U32`, `U64` | Always safe |
| `U16` | `U32`, `U64` | Always safe |
| `U32` | `U64` | Always safe |

### Narrowing Conversions

Narrowing moves a value from a larger type to a smaller type. If the value exceeds the target type's range, the high bits are discarded (truncated):

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 large = 300
    I8 truncated = large as I8
    puts( "300 as I8: " + truncated.toString() )
    I64 negative = -5
    I8 narrowNeg = negative as I8
    puts( "-5 as I8: " + narrowNeg.toString() )
    return 0
```

This outputs:

```
300 as I8: 44
-5 as I8: -5
```

The value 300 does not fit in `I8` (range -128 to 127), so the high bits are discarded, producing 44 (300 modulo 256 = 44). The value -5 fits within `I8`'s range, so it is preserved exactly.

Narrowing is the programmer's responsibility. The compiler performs the truncation without error — it does not check whether the value fits.

### Integer to Float

Converting an integer to a floating-point type produces the closest representable float value:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 intValue = 42
    F64 floatValue = intValue as F64
    puts( floatValue.toString() )
    return 0
```

This outputs `42`. For most integers, the conversion is exact. Very large integers (beyond 2^53 for `F64`) may lose precision because floating-point types have limited mantissa bits.

### Float to Integer

Converting a float to an integer truncates the fractional part toward zero:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 pi = 3.14159
    I64 truncated = pi as I64
    puts( truncated.toString() )
    return 0
```

This outputs `3`. The fractional part `.14159` is discarded entirely — no rounding occurs. The result is always the integer part closest to zero: `3.99` becomes `3`, and `-3.99` becomes `-3`.

### Integer to Character

Integers can be converted to `Char` and vice versa. The integer value is interpreted as a Unicode code point:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I32 value = 65
    Char letter = value as Char
    puts( letter.toString() )
    Char charA = 'A'
    I32 codePoint = charA as I32
    puts( codePoint.toString() )
    return 0
```

This outputs:

```
A
65
```

The integer 65 is the Unicode code point for the letter 'A'. Converting back produces the original integer.

### Mixed-Width Arithmetic

When two integer operands of different bit widths appear in an arithmetic expression, the compiler automatically widens the narrower operand to match the wider one before performing the operation:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I8 small = 10
    I64 large = 20
    I64 sum = small + large
    puts( sum.toString() )
    return 0
```

This outputs `30`. The `I8` value is automatically widened to `I64` before the addition. The result type matches the wider operand.

This auto-coercion applies to all arithmetic and comparison operations. The narrower operand is always promoted to the wider type — never the reverse.

---

## Integer Methods

Every integer value in Uranite is an object with methods. These methods compile to native machine instructions — no heap allocation, no boxing, no virtual dispatch.

**Important:** When using the result of a method call in another method call, always store intermediate results in variables. Do not chain method calls (e.g., `value.abs().toString()`) — use a separate variable for each step.

### Signed Integer Methods

All signed integer types (`I8`, `I16`, `I32`, `I64`, `Int`, `Integer`, `Long`) share the following methods:

| Method | Return Type | Description |
|---|---|---|
| `getValue()` | `I64` | Returns the underlying numeric value |
| `toString()` | `String` | Returns the decimal string representation |
| `abs()` | `Int` | Returns the absolute value |
| `negate()` | `Int` | Returns the value with sign inverted |
| `add( Int other )` | `Int` | Returns the sum of self and other |
| `subtract( Int other )` | `Int` | Returns the difference |
| `multiply( Int other )` | `Int` | Returns the product |
| `divide( Int other )` | `Int` | Returns the integer quotient |
| `modulo( Int other )` | `Int` | Returns the remainder |
| `equals( Int other )` | `Boolean` | Returns `True` if values are equal |
| `compareTo( Int other )` | `I32` | Returns -1, 0, or 1 |
| `min( Int other )` | `Int` | Returns the smaller of two values |
| `max( Int other )` | `Int` | Returns the larger of two values |
| `isZero()` | `Boolean` | Returns `True` if value is zero |
| `isPositive()` | `Boolean` | Returns `True` if value is positive |
| `isNegative()` | `Boolean` | Returns `True` if value is negative |
| `bitwiseAnd( Int other )` | `Int` | Bitwise AND |
| `bitwiseOr( Int other )` | `Int` | Bitwise OR |
| `bitwiseXor( Int other )` | `Int` | Bitwise XOR |
| `shiftLeft( Int amount )` | `Int` | Left bit shift |
| `shiftRight( Int amount )` | `Int` | Right bit shift |
| `toFloat()` | `F64` | Converts to floating-point |
| `toDouble()` | `F64` | Converts to `F64` |
| `hash()` | `I64` | Returns a hash code for the value |

### Value Query Methods

The value query methods let you inspect the sign and magnitude of an integer without arithmetic:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 value = -42
    Boolean isNeg = value.isNegative()
    puts( "isNegative: " + isNeg.toString() )
    Boolean isPos = value.isPositive()
    puts( "isPositive: " + isPos.toString() )
    Boolean isZ = value.isZero()
    puts( "isZero: " + isZ.toString() )
    I64 absolute = value.abs()
    puts( "abs: " + absolute.toString() )
    I64 negated = value.negate()
    puts( "negate: " + negated.toString() )
    return 0
```

This outputs:

```
isNegative: True
isPositive: False
isZero: False
abs: 42
negate: 42
```

`abs()` returns the magnitude (always non-negative). `negate()` inverts the sign — negative becomes positive, positive becomes negative.

### Arithmetic Methods

The arithmetic methods provide named alternatives to the symbolic operators:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 valueA = 10
    I64 valueB = 3
    I64 sum = valueA.add( valueB )
    puts( "add: " + sum.toString() )
    I64 difference = valueA.subtract( valueB )
    puts( "subtract: " + difference.toString() )
    I64 product = valueA.multiply( valueB )
    puts( "multiply: " + product.toString() )
    I64 quotient = valueA.divide( valueB )
    puts( "divide: " + quotient.toString() )
    I64 remainder = valueA.modulo( valueB )
    puts( "modulo: " + remainder.toString() )
    return 0
```

This outputs:

```
add: 13
subtract: 7
multiply: 30
divide: 3
modulo: 1
```

These methods behave identically to the `+`, `-`, `*`, `/`, `%` operators. They exist for cases where method syntax reads more clearly, such as when building fluent computation pipelines through intermediate variables.

### Comparison Methods

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 value = -42
    Boolean eq = value.equals( -42 )
    puts( "equals -42: " + eq.toString() )
    I32 comparison = value.compareTo( 0 )
    puts( "compareTo 0: " + comparison.toString() )
    I64 smaller = value.min( -100 )
    puts( "min with -100: " + smaller.toString() )
    I64 larger = value.max( -100 )
    puts( "max with -100: " + larger.toString() )
    return 0
```

This outputs:

```
equals -42: True
compareTo 0: -1
min with -100: -100
max with -100: -42
```

`compareTo()` returns `-1` if self is less than the argument, `0` if equal, and `1` if greater. `min()` returns the smaller of the two values. `max()` returns the larger.

### Bitwise Methods Reference

The bitwise methods mirror the bitwise operators but use named method calls:

| Method | Equivalent Operator |
|---|---|
| `bitwiseAnd( other )` | `self & other` |
| `bitwiseOr( other )` | `self \| other` |
| `bitwiseXor( other )` | `self ^ other` |
| `shiftLeft( amount )` | `self << amount` |
| `shiftRight( amount )` | `self >> amount` |

See the [Bitwise Methods](#bitwise-methods) section above for usage examples.

### Conversion Methods

Integer objects provide named methods for type conversion:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 intValue = 42
    F64 asFloat = intValue.toFloat()
    puts( asFloat.toString() )
    F64 asDouble = intValue.toDouble()
    puts( asDouble.toString() )
    return 0
```

This outputs:

```
42
42
```

`toFloat()` and `toDouble()` both produce `F64` values. They are equivalent to `value as F64`.

### Calling Methods on Literals

You can call methods directly on integer literals without storing them in a variable first:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String text = 42.toString()
    puts( text )
    Boolean zero = 0.isZero()
    puts( zero.toString() )
    I64 absolute = (-15).abs()
    puts( absolute.toString() )
    return 0
```

This outputs:

```
42
True
15
```

Positive literals work directly (`42.toString()`). Negative literals require parentheses (`(-15).abs()`) so the compiler treats the negation and literal as a single expression before the method call.

---

## Practical Examples

### Power of Two Detection

A positive integer is a power of two if and only if it has exactly one bit set. The expression `number & (number - 1)` clears the lowest set bit — if the result is zero, only one bit was set:

```uranite
from uranite.io.console import puts

public function isPowerOfTwo( I64 number ) -> Boolean:
    if number <= 0:
        return False
    I64 masked = number & (number - 1)
    if masked == 0:
        return True
    return False

public function main() -> I32:
    Boolean pow2 = isPowerOfTwo( 64 )
    puts( "64 is power of 2: " + pow2.toString() )
    Boolean notPow2 = isPowerOfTwo( 65 )
    puts( "65 is power of 2: " + notPow2.toString() )
    return 0
```

This outputs:

```
64 is power of 2: True
65 is power of 2: False
```

### Bit Counting

Count the number of set bits (1-bits) in an integer using a shift-and-mask loop:

```uranite
from uranite.io.console import puts

public function countSetBits( I64 number ) -> I64:
    I64 count = 0
    I64 current = number
    while current > 0:
        I64 bit = current & 1
        if bit == 1:
            count = count + 1
        current = current >> 1
    return count

public function main() -> I32:
    I64 bits = countSetBits( 255 )
    puts( "bits set in 255: " + bits.toString() )
    I64 bitsInTen = countSetBits( 10 )
    puts( "bits set in 10: " + bitsInTen.toString() )
    return 0
```

This outputs:

```
bits set in 255: 8
bits set in 10: 2
```

The value 255 is `0xFF` (eight 1-bits). The value 10 is `0b1010` (two 1-bits).

### Value Clamping

Restrict a value to a specific range, returning the boundary value if the input falls outside:

```uranite
from uranite.io.console import puts

public function clampToRange( I64 value, I64 minimum, I64 maximum ) -> I64:
    if value < minimum:
        return minimum
    if value > maximum:
        return maximum
    return value

public function main() -> I32:
    I64 clamped = clampToRange( 300, 0, 255 )
    puts( "300 clamped to 0-255: " + clamped.toString() )
    I64 negative = clampToRange( -50, 0, 255 )
    puts( "-50 clamped to 0-255: " + negative.toString() )
    I64 inRange = clampToRange( 100, 0, 255 )
    puts( "100 clamped to 0-255: " + inRange.toString() )
    return 0
```

This outputs:

```
300 clamped to 0-255: 255
-50 clamped to 0-255: 0
100 clamped to 0-255: 100
```

### Temperature Conversion with Integers

Integer division truncates, making integer-only temperature conversion approximate. This example demonstrates how truncation affects results and how to work around it:

```uranite
from uranite.io.console import puts

public function celsiusToFahrenheitApprox( I64 celsius ) -> I64:
    return celsius * 9 / 5 + 32

public function fahrenheitToCelsiusApprox( I64 fahrenheit ) -> I64:
    return (fahrenheit - 32) * 5 / 9

public function main() -> I32:
    I64 boiling = celsiusToFahrenheitApprox( 100 )
    puts( "100C = " + boiling.toString() + "F" )
    I64 freezing = celsiusToFahrenheitApprox( 0 )
    puts( "0C = " + freezing.toString() + "F" )
    I64 body = celsiusToFahrenheitApprox( 37 )
    puts( "37C = " + body.toString() + "F" )
    I64 backToC = fahrenheitToCelsiusApprox( 98 )
    puts( "98F = " + backToC.toString() + "C" )
    return 0
```

This outputs:

```
100C = 212F
0C = 32F
37C = 98F
98F = 36C
```

Note that 98F converts back to 36C instead of 37C — this is the truncation effect of integer division. For exact conversions, use floating-point types.

### Byte Extraction

Extract individual bytes from a multi-byte integer using shifts and masks — a common pattern when working with binary protocols, color values, or network packet parsing:

```uranite
from uranite.io.console import puts

public function extractByte( I64 word, I64 position ) -> I64:
    I64 shiftAmount = position * 8
    I64 shifted = word >> shiftAmount
    I64 masked = shifted & 0xFF
    return masked

public function main() -> I32:
    I64 color = 0xFF8040
    I64 red = extractByte( color, 2 )
    puts( "red: " + red.toString() )
    I64 green = extractByte( color, 1 )
    puts( "green: " + green.toString() )
    I64 blue = extractByte( color, 0 )
    puts( "blue: " + blue.toString() )
    return 0
```

This outputs:

```
red: 255
green: 128
blue: 64
```

The color value `0xFF8040` has red in the highest byte (0xFF = 255), green in the middle byte (0x80 = 128), and blue in the lowest byte (0x40 = 64). Each `extractByte` call shifts the desired byte into the lowest position and masks off everything else with `& 0xFF`.
