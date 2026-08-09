# Type Casting

---

## Table of Contents

- [Type Casting](#type-casting)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [The as Keyword](#the-as-keyword)
    - [Basic Syntax](#basic-syntax)
    - [Operator Precedence](#operator-precedence)
  - [Integer to Integer](#integer-to-integer)
    - [Widening](#widening)
    - [Narrowing](#narrowing)
    - [Narrowing and Overflow](#narrowing-and-overflow)
    - [Signed and Unsigned](#signed-and-unsigned)
  - [Integer to Float](#integer-to-float)
  - [Float to Integer](#float-to-integer)
    - [Truncation Toward Zero](#truncation-toward-zero)
    - [Negative Float Truncation](#negative-float-truncation)
  - [Float to Float](#float-to-float)
    - [F32 to F64 Widening](#f32-to-f64-widening)
    - [F64 to F32 Narrowing](#f64-to-f32-narrowing)
  - [Char Conversions](#char-conversions)
    - [Char to Integer](#char-to-integer)
    - [Integer to Char](#integer-to-char)
  - [Literal Casts](#literal-casts)
  - [Chained Casts](#chained-casts)
  - [Casting in Expressions](#casting-in-expressions)
    - [Arithmetic with Mixed Types](#arithmetic-with-mixed-types)
    - [Casting in Function Arguments](#casting-in-function-arguments)
  - [Conversion Reference](#conversion-reference)
    - [Lossless Conversions](#lossless-conversions)
    - [Lossy Conversions](#lossy-conversions)
    - [Complete Cast Table](#complete-cast-table)
  - [Practical Examples](#practical-examples)
    - [Temperature Conversion](#temperature-conversion)
    - [Integer Average](#integer-average)
    - [Byte Clamping](#byte-clamping)
    - [Floating-Point Division](#floating-point-division)

---

## Overview

Uranite uses the `as` keyword for explicit type conversion between numeric types, character types, and pointer types. Every cast is explicit — the programmer writes `as` to indicate the conversion.

```uranite
I64 integerValue = 42
F64 floatValue = integerValue as F64
```

The `as` keyword converts the value on the left to the type on the right. Some conversions are lossless (the value is preserved exactly), while others are lossy (information may be lost). Uranite does not prevent lossy conversions — the programmer is responsible for choosing appropriate types.

---

## The as Keyword

### Basic Syntax

The `as` keyword appears between an expression and a target type:

```uranite
expression as TargetType
```

The left side is any expression that produces a value. The right side is a type name. The result is a new value of the target type:

```uranite
I64 bigValue = 1000
I32 smallValue = bigValue as I32
F64 floatValue = bigValue as F64
```

### Operator Precedence

The `as` keyword has the highest precedence among binary operators — higher than arithmetic, comparison, and logical operators. This means `as` binds tighter than everything else:

```uranite
I64 valueA = 10
I64 valueB = 3
F64 result = valueA as F64 + valueB as F64
```

This expression is parsed as `(valueA as F64) + (valueB as F64)`, not as `valueA as (F64 + valueB) as F64`. Each `as` cast is applied to its immediate left operand before the addition occurs.

| Precedence | Operators |
|---|---|
| Highest | `as` (type cast) |
| | `**` (power) |
| | `*`, `/`, `%` |
| | `+`, `-` |
| | `==`, `!=`, `<`, `>`, `<=`, `>=`, `is` |
| | `and` |
| Lowest | `or` |

Because `as` binds tightest, you rarely need parentheses around a cast. But when you want to cast the result of a larger expression, parentheses are necessary:

```uranite
I64 total = valueA + valueB
F64 castAfterAdd = total as F64
```

---

## Integer to Integer

### Widening

Casting a smaller integer type to a larger one preserves the value exactly:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I8 small = 42
    I16 medium = small as I16
    I32 mid = small as I32
    I64 wide = small as I64
    puts( "I8 to I16: " + medium.toString() )
    puts( "I8 to I32: " + mid.toString() )
    puts( "I8 to I64: " + wide.toString() )
    return 0
```

This outputs:

```
I8 to I16: 42
I8 to I32: 42
I8 to I64: 42
```

Widening is always safe — a smaller type fits entirely within a larger type, so no information is lost.

### Narrowing

Casting a larger integer type to a smaller one truncates the value to fit the smaller type's bit width:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 bigValue = 1000
    I32 smallValue = bigValue as I32
    puts( "I64 1000 to I32: " + smallValue.toString() )
    I32 mid = 255
    I16 narrowed = mid as I16
    puts( "I32 255 to I16: " + narrowed.toString() )
    return 0
```

This outputs:

```
I64 1000 to I32: 1000
I32 255 to I16: 255
```

When the value fits within the target type's range, narrowing preserves it. When it does not fit, the value wraps around.

### Narrowing and Overflow

When a value exceeds the target type's range, the excess bits are discarded. The result wraps according to the target type's bit width:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 bigValue = 1000
    I8 tinyValue = bigValue as I8
    puts( "I64 1000 to I8: " + tinyValue.toString() )
    return 0
```

This outputs `I64 1000 to I8: -24`. The value 1000 in binary is `1111101000`. Truncated to 8 bits: `11101000`, which is -24 in signed two's complement.

This is expected behavior for narrowing casts — the extra bits are simply dropped. Always verify that the source value fits within the target type's range before narrowing, or accept the wrapping behavior.

### Signed and Unsigned

Casting between signed and unsigned integer types of the same width reinterprets the bit pattern:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    U8 byteVal = 200
    U32 wideUnsigned = byteVal as U32
    puts( "U8 200 to U32: " + wideUnsigned.toString() )
    U64 bigUnsigned = 1000
    U8 truncUnsigned = bigUnsigned as U8
    puts( "U64 1000 to U8: " + truncUnsigned.toString() )
    return 0
```

This outputs:

```
U8 200 to U32: 4294967240
U64 1000 to U8: 232
```

The `U8` value 200 is sign-extended when widened, producing a large number in the `U32` representation. When working with unsigned types, be aware that widening casts use sign extension — the value may not be what you expect if the source byte has its high bit set.

---

## Integer to Float

Casting an integer to a floating-point type converts the integer value to its floating-point representation:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 intVal = 42
    F64 floatVal = intVal as F64
    puts( "I64 42 to F64: " + floatVal.toString() )
    I32 intSmall = 100
    F32 floatSmall = intSmall as F32
    puts( "I32 100 to F32: " + floatSmall.toString() )
    return 0
```

This outputs:

```
I64 42 to F64: 42
I32 100 to F32: 100
```

For most integer values encountered in practice, the conversion is exact. Very large integers (beyond 2^53 for `F64` or 2^24 for `F32`) may lose precision because floating-point types have a limited number of significant digits.

---

## Float to Integer

### Truncation Toward Zero

Casting a floating-point value to an integer type truncates the fractional part — the digits after the decimal point are discarded. The result is the integer part, truncated toward zero:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 pi = 3.14
    I64 truncated = pi as I64
    puts( "3.14 as I64: " + truncated.toString() )
    F64 big = 99.99
    I64 truncBig = big as I64
    puts( "99.99 as I64: " + truncBig.toString() )
    return 0
```

This outputs:

```
3.14 as I64: 3
99.99 as I64: 99
```

Truncation always moves toward zero — `3.14` becomes `3`, not `4`. This is different from rounding. Use `round()`, `floor()`, or `ceil()` methods on the float value if you need different rounding behavior.

### Negative Float Truncation

For negative values, truncation also moves toward zero — the result is closer to zero than the original value:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 negative = -3.7
    I64 truncNeg = negative as I64
    puts( "-3.7 as I64: " + truncNeg.toString() )
    return 0
```

This outputs `-3.7 as I64: -3`. The value -3.7 truncates to -3, not -4. Truncation toward zero means `-3.7` loses the `.7` and keeps `-3`.

---

## Float to Float

### F32 to F64 Widening

Casting `F32` to `F64` extends precision. The value is preserved exactly:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F32 small = 1.5
    F64 wide = small as F64
    puts( "F32 1.5 to F64: " + wide.toString() )
    return 0
```

This outputs `F32 1.5 to F64: 1.5`. Widening from `F32` (32-bit, ~7 significant digits) to `F64` (64-bit, ~15 significant digits) is always lossless.

### F64 to F32 Narrowing

Casting `F64` to `F32` reduces precision. Values that cannot be represented exactly in `F32` are rounded to the nearest representable value:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 precise = 3.14159
    F32 narrow = precise as F32
    puts( "F64 to F32: " + narrow.toString() )
    return 0
```

This outputs `F64 to F32: 3.14159`. For values within `F32` range and precision, the narrowing may appear lossless in printed output. For values with more than ~7 significant digits, the trailing digits will differ.

---

## Char Conversions

### Char to Integer

Casting `Char` to an integer type extracts the Unicode code point as a numeric value:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Char letter = 'A'
    I32 code = letter as I32
    puts( "A as I32: " + code.toString() )
    I64 wideCode = letter as I64
    puts( "A as I64: " + wideCode.toString() )
    return 0
```

This outputs:

```
A as I32: 65
A as I64: 65
```

The character `'A'` has code point 65. Casting to any integer type produces that numeric value.

### Integer to Char

Casting an integer to `Char` interprets the integer value as a Unicode code point:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I32 bCode = 66
    Char fromCode = bCode as Char
    puts( "66 as Char: " + fromCode.toString() )
    return 0
```

This outputs `66 as Char: B`. Code point 66 corresponds to the character `B`.

---

## Literal Casts

The `as` keyword can be applied directly to literal values:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 fromFloat = 3.14 as I64
    puts( "3.14 as I64: " + fromFloat.toString() )
    F64 fromInt = 100 as F64
    puts( "100 as F64: " + fromInt.toString() )
    Char exclaim = 33 as Char
    puts( "33 as Char: " + exclaim.toString() )
    return 0
```

This outputs:

```
3.14 as I64: 3
100 as F64: 100
33 as Char: !
```

When the source is a compile-time constant, the conversion is performed at compile time — no runtime instruction is emitted.

---

## Chained Casts

Multiple `as` casts can be applied in sequence. Each cast operates on the result of the previous one:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 original = 42
    I32 mid = original as I32
    I64 back = mid as I64
    puts( "I64 to I32 to I64: " + back.toString() )
    return 0
```

This outputs `I64 to I32 to I64: 42`. The value 42 fits within `I32`, so the round-trip preserves it. For values that overflow during the intermediate narrowing step, the round-trip will not preserve the original value.

Store each intermediate cast result in its own variable. Do not write `original as I32 as I64` inline — use separate variables for each step to ensure correct behavior.

---

## Casting in Expressions

### Arithmetic with Mixed Types

When performing arithmetic between different numeric types, cast one or both operands to a common type:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 valueA = 10
    I64 valueB = 3
    F64 divResult = valueA as F64 / valueB as F64
    puts( "10.0 / 3.0: " + divResult.toString() )
    return 0
```

This outputs `10.0 / 3.0: 3.33333`. Without the casts, `10 / 3` would perform integer division and produce `3`. Casting both operands to `F64` first ensures floating-point division.

Because `as` has the highest precedence, `valueA as F64 / valueB as F64` is parsed as `(valueA as F64) / (valueB as F64)` — each operand is cast independently before the division.

### Casting in Function Arguments

Cast values when passing them to functions that expect a different type:

```uranite
from uranite.io.console import puts

public function takeFloat( F64 value ) -> Void:
    puts( "float: " + value.toString() )

public function takeWide( I64 value ) -> Void:
    puts( "wide: " + value.toString() )

public function main() -> I32:
    I32 intVal = 42
    takeFloat( intVal as F64 )
    I8 small = 10
    takeWide( small as I64 )
    return 0
```

This outputs:

```
float: 42
wide: 10
```

Each argument is cast to the parameter type before the function call.

---

## Conversion Reference

### Lossless Conversions

These conversions always preserve the value exactly:

| From | To | Condition |
|---|---|---|
| `I8` | `I16`, `I32`, `I64` | Always lossless |
| `I16` | `I32`, `I64` | Always lossless |
| `I32` | `I64` | Always lossless |
| `U8` | `U16`, `U32`, `U64` | Always lossless |
| `U16` | `U32`, `U64` | Always lossless |
| `U32` | `U64` | Always lossless |
| `F32` | `F64` | Always lossless |
| `Char` | `I32`, `I64` | Always lossless |

### Lossy Conversions

These conversions may lose information:

| From | To | What Is Lost |
|---|---|---|
| `I64` | `I32`, `I16`, `I8` | High bits truncated; value wraps |
| `I32` | `I16`, `I8` | High bits truncated; value wraps |
| `I16` | `I8` | High bits truncated; value wraps |
| `F64` | `I64`, `I32` | Fractional part discarded (truncated toward zero) |
| `F32` | `I64`, `I32` | Fractional part discarded (truncated toward zero) |
| `F64` | `F32` | Precision reduced (~15 digits to ~7 digits) |
| `I64` | `F64` | Precision lost for values beyond 2^53 |
| `I64` | `F32` | Precision lost for values beyond 2^24 |

### Complete Cast Table

| Source | Target | Description |
|---|---|---|
| Integer (narrow) | Integer (wide) | Sign-extends to fill wider type |
| Integer (wide) | Integer (narrow) | Truncates high bits |
| Integer | `F64` / `F32` | Converts integer to floating-point representation |
| `F64` / `F32` | Integer | Truncates fractional part toward zero |
| `F32` | `F64` | Extends to double precision |
| `F64` | `F32` | Reduces to single precision |
| `Char` | Integer | Extracts code point value |
| Integer | `Char` | Interprets integer as code point |

---

## Practical Examples

### Temperature Conversion

Convert between Celsius and Fahrenheit using integer input and floating-point arithmetic:

```uranite
from uranite.io.console import puts

public function celsiusToFahrenheit( I64 celsius ) -> F64:
    F64 temp = celsius as F64
    return temp * 9.0 / 5.0 + 32.0

public function fahrenheitToCelsius( I64 fahrenheit ) -> F64:
    F64 temp = fahrenheit as F64
    return (temp - 32.0) * 5.0 / 9.0

public function main() -> I32:
    F64 boiling = celsiusToFahrenheit( 100 )
    puts( "100C = " + boiling.toString() + "F" )
    F64 freezing = celsiusToFahrenheit( 0 )
    puts( "0C = " + freezing.toString() + "F" )
    F64 body = fahrenheitToCelsius( 98 )
    puts( "98F = " + body.toString() + "C" )
    return 0
```

This outputs:

```
100C = 212F
0C = 32F
98F = 36.6667C
```

The integer input is cast to `F64` before arithmetic to ensure floating-point division. Without the cast, `9 / 5` would produce `1` (integer division), giving incorrect results.

### Integer Average

Compute the average of integer values with a floating-point result:

```uranite
from uranite.io.console import puts

public function average( I64 valueA, I64 valueB, I64 valueC ) -> F64:
    I64 total = valueA + valueB + valueC
    F64 sum = total as F64
    return sum / 3.0

public function main() -> I32:
    F64 avg = average( 10, 20, 30 )
    puts( "average of 10,20,30: " + avg.toString() )
    F64 avg2 = average( 1, 2, 3 )
    puts( "average of 1,2,3: " + avg2.toString() )
    return 0
```

This outputs:

```
average of 10,20,30: 20
average of 1,2,3: 2
```

The integer sum is cast to `F64` before dividing by `3.0`. This ensures the division is floating-point. The sum itself is computed as integer arithmetic (no precision loss for the addition), and only the final division uses floating-point.

### Byte Clamping

Clamp an integer value to the valid byte range (0 through 255):

```uranite
from uranite.io.console import puts

public function clampToByte( I64 value ) -> I64:
    if value < 0:
        return 0
    if value > 255:
        return 255
    return value

public function main() -> I32:
    I64 clamped = clampToByte( 300 )
    puts( "clamp 300: " + clamped.toString() )
    I64 clampedNeg = clampToByte( -5 )
    puts( "clamp -5: " + clampedNeg.toString() )
    I64 inRange = clampToByte( 128 )
    puts( "clamp 128: " + inRange.toString() )
    return 0
```

This outputs:

```
clamp 300: 255
clamp -5: 0
clamp 128: 128
```

This is safer than casting directly to `U8`, which truncates without warning. By clamping first, out-of-range values are handled explicitly rather than wrapping silently.

### Floating-Point Division

Use casting to perform floating-point division on integer values:

```uranite
from uranite.io.console import puts

public function percentage( I64 part, I64 whole ) -> F64:
    F64 partFloat = part as F64
    F64 wholeFloat = whole as F64
    return partFloat / wholeFloat * 100.0

public function main() -> I32:
    F64 pct1 = percentage( 3, 4 )
    puts( "3/4: " + pct1.toString() + "%" )
    F64 pct2 = percentage( 1, 3 )
    puts( "1/3: " + pct2.toString() + "%" )
    F64 pct3 = percentage( 7, 7 )
    puts( "7/7: " + pct3.toString() + "%" )
    return 0
```

This outputs:

```
3/4: 75%
1/3: 33.3333%
7/7: 100%
```

Both operands are cast to `F64` before division. Without casting, `3 / 4` would produce `0` (integer division truncates toward zero). Casting ensures the fractional result is preserved.
