# Floating-Point Types

---

## Table of Contents

- [Floating-Point Types](#floating-point-types)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [F32](#f32)
  - [F64](#f64)
  - [Float](#float)
  - [Double](#double)
  - [Default Float Inference](#default-float-inference)
  - [Precision and Range](#precision-and-range)
  - [Float Literals](#float-literals)
    - [Decimal Notation](#decimal-notation)
    - [Scientific Notation](#scientific-notation)
    - [Negative Literals](#negative-literals)
  - [Arithmetic Operations](#arithmetic-operations)
    - [Basic Arithmetic](#basic-arithmetic)
    - [Compound Assignment](#compound-assignment)
    - [Unary Negation](#unary-negation)
    - [Division by Zero](#division-by-zero)
  - [Special Values](#special-values)
    - [NaN (Not a Number)](#nan-not-a-number)
    - [Infinity](#infinity)
    - [Negative Zero](#negative-zero)
    - [Detecting Special Values](#detecting-special-values)
    - [NaN Propagation](#nan-propagation)
    - [Infinity Arithmetic](#infinity-arithmetic)
  - [Comparison Behavior](#comparison-behavior)
    - [Standard Comparisons](#standard-comparisons)
    - [NaN in Comparisons](#nan-in-comparisons)
    - [Comparing with Equality Methods](#comparing-with-equality-methods)
  - [Rounding Methods](#rounding-methods)
    - [Floor](#floor)
    - [Ceil](#ceil)
    - [Round](#round)
    - [Rounding Negative Values](#rounding-negative-values)
  - [Mathematical Methods](#mathematical-methods)
    - [Absolute Value](#absolute-value)
    - [Square Root](#square-root)
    - [Exponentiation](#exponentiation)
    - [Sign Inversion](#sign-inversion)
  - [Type Casting](#type-casting)
    - [F32 to F64 Widening](#f32-to-f64-widening)
    - [F64 to F32 Narrowing](#f64-to-f32-narrowing)
    - [Integer to Float](#integer-to-float)
    - [Float to Integer](#float-to-integer)
  - [Float Methods Reference](#float-methods-reference)
    - [Classification Methods](#classification-methods)
    - [Arithmetic Methods](#arithmetic-methods)
    - [Comparison Methods](#comparison-methods)
    - [Rounding and Math Methods](#rounding-and-math-methods)
    - [Conversion Methods](#conversion-methods)
    - [Calling Methods on Literals](#calling-methods-on-literals)
  - [Practical Examples](#practical-examples)
    - [Distance Calculation](#distance-calculation)
    - [Temperature Conversion](#temperature-conversion)
    - [Safe Division](#safe-division)
    - [Statistical Average](#statistical-average)

---

## Overview

Uranite provides two floating-point types conforming to the IEEE 754 standard: `F32` (single-precision, 32-bit) and `F64` (double-precision, 64-bit). Two named types — `Float` and `Double` — provide readable alternatives for `F64`.

| Type | Bit Width | Decimal Digits | Approximate Range |
|---|---|---|---|
| `F32` | 32 | ~7 | ±3.4 x 10^38 |
| `F64` / `Float` / `Double` | 64 | ~15 | ±1.8 x 10^308 |

Floating-point types store real numbers with fractional parts. They are essential for scientific computing, geometry, physics simulations, financial calculations, signal processing, and any domain that works with continuous values.

Like all primitive types in Uranite, floating-point values are objects with methods — you can call `floor()`, `sqrt()`, `isNaN()` and more on any float value, including literals. These method calls compile to native hardware instructions with zero runtime overhead.

---

## F32

A 32-bit single-precision floating-point number. Provides approximately **7 decimal digits** of precision.

```uranite
F32 temperature = 98.6
F32 latitude = 37.7749
F32 probability = 0.95
```

`F32` uses less memory than `F64` (4 bytes vs 8 bytes) but sacrifices precision. Use `F32` when memory is constrained and 7-digit precision is sufficient, such as graphics coordinates, audio samples, or large arrays of measurements.

`F32` requires an explicit type annotation — float literals default to `F64`.

`F32` is a `final` class and cannot be subclassed.

---

## F64

A 64-bit double-precision floating-point number. Provides approximately **15 decimal digits** of precision.

```uranite
F64 pi = 3.141592653589793
F64 avogadro = 6.022e23
F64 planck = 1.6e-19
```

`F64` is the default floating-point type. When you write a bare float literal without a type annotation, the compiler infers `F64`. Use `F64` for scientific computing and any context where precision matters.

`F64` is a `final` class and cannot be subclassed.

---

## Float

A 64-bit double-precision floating-point number. `Float` provides the natural, readable name for the default floating-point type.

```uranite
Float velocity = 299792458.0
Float gravity = 9.80665
```

`Float` and `F64` are the same type at every level — declaration, assignment, method calls, and storage. Most Uranite code uses `Float` for readability unless a specific precision level is important.

---

## Double

A 64-bit double-precision floating-point number. `Double` provides an alternative name emphasizing double-precision.

```uranite
Double preciseResult = 1.7976931e308
Double tinyValue = 2.225e-308
```

`Double` and `F64` are the same type. `Double` is a `final` class.

---

## Default Float Inference

When you write a bare float literal without a type annotation, Uranite always infers `F64`:

```uranite
function example() -> Void:
    F64 explicit = 3.14
    Float alsoExplicit = 3.14
```

Both produce identical results. To store a value as `F32`, you must provide an explicit type annotation:

```uranite
F32 singlePrecision = 3.14
```

---

## Precision and Range

Floating-point types represent real numbers using a sign bit, an exponent, and a mantissa (significand). This representation can express very large and very small values, but with limited precision.

| Property | F32 | F64 |
|---|---|---|
| Sign bits | 1 | 1 |
| Exponent bits | 8 | 11 |
| Mantissa bits | 23 | 52 |
| Decimal digits of precision | ~7 | ~15 |
| Maximum value | ~3.4 x 10^38 | ~1.8 x 10^308 |
| Minimum positive normal | ~1.2 x 10^-38 | ~2.2 x 10^-308 |

**Precision limits** mean that not all decimal values can be represented exactly. For example, `0.1` has no exact binary floating-point representation — it is stored as the closest representable value, which introduces a tiny rounding error. This is inherent to all IEEE 754 floating-point systems, not specific to Uranite.

When comparing floating-point values, be aware that accumulated rounding errors can cause seemingly identical calculations to produce slightly different results. For exact equality checks, consider comparing within a tolerance range rather than using `==` directly.

---

## Float Literals

### Decimal Notation

Float literals require at least one digit on each side of the decimal point:

```uranite
F64 pi = 3.14159
F64 half = 0.5
F64 whole = 42.0
F64 tiny = 0.001
```

The decimal point distinguishes float literals from integer literals. The literal `42` is an `I64`, while `42.0` is an `F64`.

### Scientific Notation

Scientific notation uses `e` or `E` to specify a power of ten:

```uranite
F64 avogadro = 6.022e23
F64 electron = 1.6e-19
F64 speed = 2.998e8
```

The notation `6.022e23` means 6.022 × 10^23. A negative exponent (`e-19`) produces very small values. Scientific notation is useful for very large or very small values that would be unwieldy in decimal form.

Note that the compiler's `toString()` method may display large or small values in scientific notation:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 large = 299792458.0
    puts( large.toString() )
    return 0
```

This outputs `2.99792e+08` — the runtime chooses the most compact representation.

### Negative Literals

Negative float literals use the unary minus operator:

```uranite
F64 freezing = -273.15
F64 depth = -10994.0
```

---

## Arithmetic Operations

### Basic Arithmetic

Uranite provides four arithmetic operators for floating-point values:

| Operator | Operation | Example |
|---|---|---|
| `+` | Addition | `10.5 + 3.2` produces `13.7` |
| `-` | Subtraction | `10.5 - 3.2` produces `7.3` |
| `*` | Multiplication | `10.5 * 3.2` produces `33.6` |
| `/` | Division | `10.5 / 3.2` produces `3.28125` |

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 valueA = 10.5
    F64 valueB = 3.2
    F64 sum = valueA + valueB
    puts( "sum: " + sum.toString() )
    F64 difference = valueA - valueB
    puts( "diff: " + difference.toString() )
    F64 product = valueA * valueB
    puts( "product: " + product.toString() )
    F64 quotient = valueA / valueB
    puts( "quotient: " + quotient.toString() )
    return 0
```

This outputs:

```
sum: 13.7
diff: 7.3
product: 33.6
quotient: 3.28125
```

There is no modulo (`%`) operator for floating-point types. If you need the remainder of a float division, compute it manually: `remainder = dividend - (dividend / divisor).floor() * divisor`.

Both operands must be the same floating-point type. To combine a float with an integer, use the `as` keyword to convert the integer to a float first (see [Integer to Float](#integer-to-float)).

### Compound Assignment

Compound assignment operators combine an arithmetic operation with assignment:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 value = 10.0
    value += 5.5
    puts( value.toString() )
    value -= 3.0
    puts( value.toString() )
    value *= 2.0
    puts( value.toString() )
    value /= 4.0
    puts( value.toString() )
    return 0
```

This outputs:

```
15.5
12.5
25
6.25
```

### Unary Negation

The unary minus operator `-` inverts the sign of a floating-point value:

```uranite
F64 positive = 42.5
F64 negative = -positive
```

Negation handles all special values correctly: `-0.0` becomes `0.0`, negating infinity flips its sign, and negating NaN remains NaN.

### Division by Zero

Unlike integer division, floating-point division by zero is well-defined and does not crash the program:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 positiveInf = 1.0 / 0.0
    Boolean isInf = positiveInf.isInfinite()
    puts( "1.0 / 0.0 isInfinite: " + isInf.toString() )
    F64 negativeInf = -1.0 / 0.0
    Boolean isNegInf = negativeInf.isInfinite()
    puts( "-1.0 / 0.0 isInfinite: " + isNegInf.toString() )
    F64 nan = 0.0 / 0.0
    Boolean isNan = nan.isNaN()
    puts( "0.0 / 0.0 isNaN: " + isNan.toString() )
    return 0
```

This outputs:

```
1.0 / 0.0 isInfinite: True
-1.0 / 0.0 isInfinite: True
0.0 / 0.0 isNaN: True
```

Dividing a positive number by zero produces positive infinity. Dividing a negative number by zero produces negative infinity. Dividing zero by zero produces NaN. These results follow the IEEE 754 standard.

---

## Special Values

Floating-point arithmetic can produce three categories of special values. Understanding them is essential for writing robust numeric code.

### NaN (Not a Number)

NaN represents the result of an undefined or unrepresentable mathematical operation. It arises from:

- Dividing zero by zero: `0.0 / 0.0`
- Taking the square root of a negative number: `(-1.0).sqrt()`
- Subtracting infinity from infinity: `inf - inf`
- Multiplying infinity by zero: `inf * 0.0`

NaN has a unique property: it is never equal to anything, including itself. This means `nan == nan` is always `False`.

### Infinity

Infinity represents a value that exceeds the representable range. Both positive infinity and negative infinity exist:

- Dividing a positive number by zero produces positive infinity
- Dividing a negative number by zero produces negative infinity
- Arithmetic overflow can also produce infinity

Infinity propagates through addition and multiplication, but combining infinities in certain ways produces NaN.

### Negative Zero

Floating-point arithmetic distinguishes between `+0.0` and `-0.0`. They compare as equal but behave differently when used as divisors:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 posZero = 0.0
    F64 negZero = -0.0
    Boolean eq = posZero == negZero
    puts( "+0 == -0: " + eq.toString() )
    F64 negResult = 1.0 / negZero
    Boolean isNeg = negResult.isNegative()
    puts( "1/-0 isNegative: " + isNeg.toString() )
    return 0
```

This outputs:

```
+0 == -0: True
1/-0 isNegative: True
```

Positive and negative zero are equal under `==`, but `1.0 / -0.0` produces negative infinity while `1.0 / 0.0` produces positive infinity.

### Detecting Special Values

The `isNaN()`, `isInfinite()`, and `isFinite()` methods let you test for special values:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 nan = 0.0 / 0.0
    Boolean isNan = nan.isNaN()
    puts( "isNaN: " + isNan.toString() )
    F64 inf = 1.0 / 0.0
    Boolean isInf = inf.isInfinite()
    puts( "isInfinite: " + isInf.toString() )
    F64 normal = 42.0
    Boolean isFin = normal.isFinite()
    puts( "isFinite: " + isFin.toString() )
    Boolean nanFin = nan.isFinite()
    puts( "NaN isFinite: " + nanFin.toString() )
    Boolean infFin = inf.isFinite()
    puts( "Inf isFinite: " + infFin.toString() )
    return 0
```

This outputs:

```
isNaN: True
isInfinite: True
isFinite: True
NaN isFinite: False
Inf isFinite: False
```

`isFinite()` returns `True` only for normal numbers — it returns `False` for both NaN and infinity.

### NaN Propagation

Any arithmetic operation involving NaN produces NaN. Once NaN enters a computation, it "poisons" all subsequent results:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 nan = 0.0 / 0.0
    F64 nanAdd = nan + 42.0
    Boolean isStillNan = nanAdd.isNaN()
    puts( "NaN + 42 isNaN: " + isStillNan.toString() )
    return 0
```

This outputs `NaN + 42 isNaN: True`. The NaN value propagates through the addition, producing NaN regardless of the other operand.

This propagation behavior makes NaN useful as a sentinel — if any step in a multi-step calculation fails, the final result will be NaN, which you can check with a single `isNaN()` call at the end.

### Infinity Arithmetic

Infinity follows specific rules when combined with other values:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 inf = 1.0 / 0.0
    F64 infAdd = inf + 1.0
    Boolean isStillInf = infAdd.isInfinite()
    puts( "Inf + 1 isInfinite: " + isStillInf.toString() )
    F64 infMinusInf = inf - inf
    Boolean isNanResult = infMinusInf.isNaN()
    puts( "Inf - Inf isNaN: " + isNanResult.toString() )
    F64 infTimesZero = inf * 0.0
    Boolean isNanResult2 = infTimesZero.isNaN()
    puts( "Inf * 0 isNaN: " + isNanResult2.toString() )
    return 0
```

This outputs:

```
Inf + 1 isInfinite: True
Inf - Inf isNaN: True
Inf * 0 isNaN: True
```

Adding a finite value to infinity produces infinity. But subtracting infinity from infinity and multiplying infinity by zero are undefined operations that produce NaN.

---

## Comparison Behavior

### Standard Comparisons

Floating-point values support all six comparison operators:

| Operator | Meaning |
|---|---|
| `==` | Equal |
| `!=` | Not equal |
| `<` | Less than |
| `>` | Greater than |
| `<=` | Less than or equal |
| `>=` | Greater than or equal |

```uranite
F64 valueA = 3.14
F64 valueB = 2.71
Boolean less = valueA < valueB
Boolean greater = valueA > valueB
Boolean equal = valueA == 3.14
```

### NaN in Comparisons

NaN makes every comparison return `False`, except `!=` which returns `True`:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 nan = 0.0 / 0.0
    Boolean eqSelf = nan == nan
    puts( "NaN == NaN: " + eqSelf.toString() )
    return 0
```

This outputs `NaN == NaN: False`.

This is a fundamental property of IEEE 754: NaN is not equal to anything, including itself. This property is what makes `isNaN()` work — internally, it checks whether a value is not equal to itself.

All other comparisons involving NaN also return `False`:

- `NaN < 1.0` is `False`
- `NaN > 1.0` is `False`
- `NaN <= 1.0` is `False`
- `NaN >= 1.0` is `False`

The only comparison that returns `True` with NaN is `!=`, because NaN is not equal to anything: `NaN != 1.0` is `True`, and `NaN != NaN` is also `True`.

### Comparing with Equality Methods

The `equals()` and `compareTo()` methods provide named alternatives to operators:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 valueA = 3.14
    F64 valueB = 2.71
    Boolean eq = valueA.equals( 3.14 )
    puts( "equals 3.14: " + eq.toString() )
    I32 cmp = valueA.compareTo( valueB )
    puts( "compareTo: " + cmp.toString() )
    return 0
```

This outputs:

```
equals 3.14: True
compareTo: 1
```

`compareTo()` returns `-1` if self is less than the argument, `0` if equal, and `1` if greater.

---

## Rounding Methods

All floating-point types provide three rounding methods. Each returns a floating-point value (not an integer) representing the rounded result.

### Floor

`floor()` rounds toward negative infinity — always down:

```uranite
F64 value = 3.7
F64 floored = value.floor()
```

`floor(3.7)` produces `3`. `floor(3.0)` produces `3`. `floor(-3.2)` produces `-4`.

### Ceil

`ceil()` rounds toward positive infinity — always up:

```uranite
F64 value = 3.2
F64 ceiled = value.ceil()
```

`ceil(3.2)` produces `4`. `ceil(3.0)` produces `3`. `ceil(-3.7)` produces `-3`.

### Round

`round()` rounds to the nearest integer value, with ties rounding away from zero:

```uranite
F64 value = 3.7
F64 rounded = value.round()
```

`round(3.7)` produces `4`. `round(3.2)` produces `3`. `round(2.5)` produces `3` (tie breaks away from zero).

### Rounding Negative Values

Rounding negative values follows the same directional rules:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 negVal = -3.7
    F64 negFloored = negVal.floor()
    puts( "floor(-3.7): " + negFloored.toString() )
    F64 negCeiled = negVal.ceil()
    puts( "ceil(-3.7): " + negCeiled.toString() )
    F64 negRounded = negVal.round()
    puts( "round(-3.7): " + negRounded.toString() )
    return 0
```

This outputs:

```
floor(-3.7): -4
ceil(-3.7): -3
round(-3.7): -4
```

`floor()` moves toward negative infinity (more negative), so `floor(-3.7)` is `-4`. `ceil()` moves toward positive infinity (less negative), so `ceil(-3.7)` is `-3`. `round()` rounds to nearest, so `round(-3.7)` is `-4`.

---

## Mathematical Methods

### Absolute Value

`abs()` returns the magnitude of a floating-point value, stripping the sign:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 value = -42.5
    F64 absolute = value.abs()
    puts( "abs(-42.5): " + absolute.toString() )
    return 0
```

This outputs `abs(-42.5): 42.5`.

### Square Root

`sqrt()` returns the square root of a value:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 squareRoot = 144.0.sqrt()
    puts( "sqrt(144): " + squareRoot.toString() )
    F64 sqrtTwo = 2.0.power( 0.5 )
    puts( "sqrt(2): " + sqrtTwo.toString() )
    return 0
```

This outputs:

```
sqrt(144): 12
sqrt(2): 1.41421
```

Taking the square root of a negative number produces NaN:

```uranite
F64 sqrtNeg = (-1.0).sqrt()
Boolean isNan = sqrtNeg.isNaN()
```

`isNan` is `True` because the square root of a negative number is undefined in real arithmetic.

### Exponentiation

`power()` raises a value to an exponent:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 cubed = 2.0.power( 3.0 )
    puts( "2^3: " + cubed.toString() )
    F64 thousand = 10.0.power( 3.0 )
    puts( "10^3: " + thousand.toString() )
    return 0
```

This outputs:

```
2^3: 8
10^3: 1000
```

Both the base and the exponent must be floating-point values. Use `power(0.5)` to compute square roots as an alternative to `sqrt()`.

### Sign Inversion

`negate()` returns the value with its sign inverted:

```uranite
F64 value = -42.5
F64 negated = value.negate()
```

`negate(-42.5)` produces `42.5`. `negate(42.5)` produces `-42.5`. This is equivalent to the unary minus operator (`-value`).

---

## Type Casting

### F32 to F64 Widening

Widening from `F32` to `F64` preserves the value exactly — every `F32` value is representable as `F64`:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F32 single = 3.14
    F64 promoted = single as F64
    puts( "F32 to F64: " + promoted.toString() )
    return 0
```

This outputs `F32 to F64: 3.14`.

### F64 to F32 Narrowing

Narrowing from `F64` to `F32` may lose precision — the value is rounded to the nearest representable `F32` value:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 precise = 3.141592653589793
    F32 demoted = precise as F32
    puts( "F64 to F32: " + demoted.toString() )
    return 0
```

This outputs `F64 to F32: 3.14159`. The full 15-digit precision of `F64` is reduced to approximately 7 digits in `F32`.

Narrowing requires the explicit `as` keyword. The conversion is the programmer's explicit acknowledgment that precision may be lost.

### Integer to Float

Converting an integer to a floating-point type produces the closest representable float value:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 intValue = 42
    F64 floated = intValue as F64
    puts( "I64 to F64: " + floated.toString() )
    return 0
```

This outputs `I64 to F64: 42`. For most integers, the conversion is exact. Very large integers (beyond 2^53 for `F64`) may lose precision because `F64` has only 52 mantissa bits.

### Float to Integer

Converting a float to an integer truncates the fractional part toward zero — no rounding occurs:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 positive = 3.99
    I64 truncPos = positive as I64
    puts( "3.99 to I64: " + truncPos.toString() )
    F64 negative = -3.99
    I64 truncNeg = negative as I64
    puts( "-3.99 to I64: " + truncNeg.toString() )
    return 0
```

This outputs:

```
3.99 to I64: 3
-3.99 to I64: -3
```

The fractional part is always discarded toward zero. `3.99` becomes `3` (not `4`), and `-3.99` becomes `-3` (not `-4`).

If you want rounding instead of truncation, call `round()` before casting:

```uranite
F64 value = 3.7
F64 rounded = value.round()
I64 result = rounded as I64
```

This produces `4` instead of `3`.

---

## Float Methods Reference

Every floating-point value in Uranite is an object with methods. These methods compile to native hardware instructions — no heap allocation, no boxing, no virtual dispatch.

**Important:** When using the result of a method call in another method call, always store intermediate results in variables. Do not chain method calls — use a separate variable for each step.

All floating-point types (`F32`, `F64`, `Float`, `Double`) share the following methods:

### Classification Methods

| Method | Return Type | Description |
|---|---|---|
| `isNaN()` | `Boolean` | Returns `True` if value is Not-a-Number |
| `isInfinite()` | `Boolean` | Returns `True` if positive or negative infinity |
| `isFinite()` | `Boolean` | Returns `True` if neither NaN nor infinity |
| `isZero()` | `Boolean` | Returns `True` if value equals zero |
| `isPositive()` | `Boolean` | Returns `True` if strictly greater than zero |
| `isNegative()` | `Boolean` | Returns `True` if strictly less than zero |

### Arithmetic Methods

| Method | Return Type | Description |
|---|---|---|
| `add( Float other )` | `Float` | Returns the sum |
| `subtract( Float other )` | `Float` | Returns the difference |
| `multiply( Float other )` | `Float` | Returns the product |
| `divide( Float other )` | `Float` | Returns the quotient |
| `negate()` | `Float` | Returns the value with sign inverted |

### Comparison Methods

| Method | Return Type | Description |
|---|---|---|
| `equals( Float other )` | `Boolean` | Returns `True` if values are equal |
| `compareTo( Float other )` | `I32` | Returns -1, 0, or 1 |

### Rounding and Math Methods

| Method | Return Type | Description |
|---|---|---|
| `floor()` | `Float` | Rounds toward negative infinity |
| `ceil()` | `Float` | Rounds toward positive infinity |
| `round()` | `Float` | Rounds to nearest, ties away from zero |
| `abs()` | `Float` | Returns the absolute value |
| `sqrt()` | `Float` | Returns the square root |
| `power( Float exponent )` | `Float` | Returns self raised to a power |

### Conversion Methods

| Method | Return Type | Description |
|---|---|---|
| `getValue()` | `F64` | Returns the underlying numeric value |
| `toString()` | `String` | Returns the string representation |
| `toInt()` | `I64` | Converts to integer by truncation toward zero |
| `toFloat()` | `F64` | Returns the value as `F64` |
| `toDouble()` | `F64` | Returns the value as `F64` |
| `hash()` | `I64` | Returns a hash code for the value |

### Calling Methods on Literals

You can call methods directly on float literals:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 squareRoot = 144.0.sqrt()
    puts( squareRoot.toString() )
    F64 rounded = 3.14159.round()
    puts( rounded.toString() )
    return 0
```

This outputs:

```
12
3
```

Negative float literals require parentheses for method calls:

```uranite
F64 absolute = (-42.5).abs()
```

**Note:** Calling methods on parenthesized compound expressions (such as `(3.0 * 3.0 + 4.0 * 4.0).sqrt()`) may fail. Always store intermediate computation results in a variable first, then call the method on that variable:

```uranite
F64 sumOfSquares = 3.0 * 3.0 + 4.0 * 4.0
F64 hypotenuse = sumOfSquares.sqrt()
```

---

## Practical Examples

### Distance Calculation

Calculate the Euclidean distance between two points using the Pythagorean theorem:

```uranite
from uranite.io.console import puts

public function distanceBetween( F64 x1, F64 y1, F64 x2, F64 y2 ) -> F64:
    F64 deltaX = x2 - x1
    F64 deltaY = y2 - y1
    F64 sumOfSquares = deltaX * deltaX + deltaY * deltaY
    return sumOfSquares.sqrt()

public function main() -> I32:
    F64 distance = distanceBetween( 0.0, 0.0, 3.0, 4.0 )
    puts( "distance: " + distance.toString() )
    return 0
```

This outputs `distance: 5`. The classic 3-4-5 right triangle has a hypotenuse of 5.

### Temperature Conversion

Convert between Celsius and Fahrenheit using floating-point arithmetic:

```uranite
from uranite.io.console import puts

public function celsiusToFahrenheit( F64 celsius ) -> F64:
    return celsius * 1.8 + 32.0

public function fahrenheitToCelsius( F64 fahrenheit ) -> F64:
    return (fahrenheit - 32.0) / 1.8

public function main() -> I32:
    F64 boiling = celsiusToFahrenheit( 100.0 )
    puts( "100C = " + boiling.toString() + "F" )
    F64 freezing = celsiusToFahrenheit( 0.0 )
    puts( "0C = " + freezing.toString() + "F" )
    F64 body = celsiusToFahrenheit( 37.0 )
    puts( "37C = " + body.toString() + "F" )
    F64 backToC = fahrenheitToCelsius( 212.0 )
    puts( "212F = " + backToC.toString() + "C" )
    return 0
```

This outputs:

```
100C = 212F
0C = 32F
37C = 98.6F
212F = 100C
```

Unlike integer division, floating-point division preserves the fractional part, so the conversion is exact for these values.

### Safe Division

Guard against division by zero and invalid results using the classification methods:

```uranite
from uranite.io.console import puts

public function safeDivide( F64 numerator, F64 denominator ) -> F64:
    Boolean isZ = denominator.isZero()
    if isZ:
        return 0.0
    F64 result = numerator / denominator
    Boolean resultNan = result.isNaN()
    Boolean resultInf = result.isInfinite()
    if resultNan or resultInf:
        return 0.0
    return result

public function main() -> I32:
    F64 normal = safeDivide( 10.0, 3.0 )
    puts( "10/3: " + normal.toString() )
    F64 byZero = safeDivide( 10.0, 0.0 )
    puts( "10/0: " + byZero.toString() )
    F64 zeroByZero = safeDivide( 0.0, 0.0 )
    puts( "0/0: " + zeroByZero.toString() )
    return 0
```

This outputs:

```
10/3: 3.33333
10/0: 0
0/0: 0
```

The `safeDivide` function checks for zero denominator first, then validates the result is neither NaN nor infinity. This pattern prevents special values from propagating through a computation and producing unexpected results downstream.

### Statistical Average

Compute the average of a series of measurements, demonstrating accumulation and division:

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 sum = 0.0
    I64 count = 0

    F64 measurement1 = 23.4
    sum = sum + measurement1
    count = count + 1

    F64 measurement2 = 25.1
    sum = sum + measurement2
    count = count + 1

    F64 measurement3 = 22.8
    sum = sum + measurement3
    count = count + 1

    F64 measurement4 = 24.6
    sum = sum + measurement4
    count = count + 1

    F64 measurement5 = 23.9
    sum = sum + measurement5
    count = count + 1

    F64 countAsFloat = count as F64
    F64 average = sum / countAsFloat
    puts( "sum: " + sum.toString() )
    puts( "count: " + count.toString() )
    puts( "average: " + average.toString() )

    F64 floored = average.floor()
    puts( "floor: " + floored.toString() )
    F64 ceiled = average.ceil()
    puts( "ceil: " + ceiled.toString() )
    F64 rounded = average.round()
    puts( "round: " + rounded.toString() )
    I64 truncated = average.toInt()
    puts( "toInt: " + truncated.toString() )

    return 0
```

This program accumulates five measurements, computes their average, and demonstrates the different rounding behaviors applied to the result. The integer count must be explicitly converted to `F64` with `as F64` before division, because Uranite requires both operands to be the same type.
