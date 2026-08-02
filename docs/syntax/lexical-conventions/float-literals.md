# Float Literals

Uranite supports floating-point literals with decimal notation and optional scientific exponent notation. All float literals default to the `F64` type — a 64-bit IEEE 754 double-precision value. This document specifies the syntax for standard decimal floats, scientific notation, underscore separators, type assignment, narrowing conversions, IEEE 754 behavior, precision, and special values.

---

## Table of Contents

- [Float Literals](#float-literals)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Anatomy of a Float Literal](#anatomy-of-a-float-literal)
  - [Standard Decimal Floats](#standard-decimal-floats)
  - [Scientific Notation](#scientific-notation)
    - [Exponent Sign](#exponent-sign)
    - [Integer-Only Scientific Notation](#integer-only-scientific-notation)
  - [Underscore Separators](#underscore-separators)
    - [Where Underscores Are Allowed](#where-underscores-are-allowed)
    - [Exponent Restriction](#exponent-restriction)
  - [Decimal Point Rules](#decimal-point-rules)
    - [No Leading Decimal Point](#no-leading-decimal-point)
    - [No Trailing Decimal Point](#no-trailing-decimal-point)
    - [Decimal Point vs Member Access](#decimal-point-vs-member-access)
    - [Decimal Point vs Range Operator](#decimal-point-vs-range-operator)
  - [Negative Floats](#negative-floats)
  - [Default Type: F64](#default-type-f64)
    - [Method Access on Floats](#method-access-on-floats)
    - [Type Narrowing Through Annotations](#type-narrowing-through-annotations)
  - [The Float Type System](#the-float-type-system)
    - [Float Types](#float-types)
    - [Alias Types](#alias-types)
    - [Choosing a Float Type](#choosing-a-float-type)
  - [IEEE 754 Compliance](#ieee-754-compliance)
    - [Rounding](#rounding)
    - [Special Values](#special-values)
    - [Signed Zero](#signed-zero)
    - [No Automatic Overflow Checks](#no-automatic-overflow-checks)
    - [Inspecting Special Values](#inspecting-special-values)
  - [Precision and Range](#precision-and-range)
    - [F64 Precision](#f64-precision)
    - [F32 Precision](#f32-precision)
    - [Digit Significance](#digit-significance)
  - [Float Literals Are Decimal Only](#float-literals-are-decimal-only)
  - [Examples](#examples)
    - [Valid Float Literals](#valid-float-literals)
    - [Invalid Float Literals](#invalid-float-literals)
    - [Edge Cases](#edge-cases)
    - [Practical Usage](#practical-usage)

---

## Overview

A floating-point literal represents a real number with a fractional component, an exponent, or both. At least one of a decimal point (followed by a digit) or an exponent marker (`e`/`E`) must be present to distinguish a float from an integer.

| Component | Required | Description |
|---|---|---|
| Integer part | Yes | One or more decimal digits before the decimal point. |
| Decimal point | Conditional | The `.` character, required if no exponent is present. |
| Fractional part | Conditional | One or more decimal digits after the decimal point. |
| Exponent marker | Optional | `e` or `E`, optionally followed by `+` or `-` and digits. |

The literal `42` is an integer. The literals `42.0`, `42e0`, and `42.0e0` are all floats. The distinction is purely syntactic — the presence of `.` (followed by a digit) or `e`/`E` makes a numeric literal a float.

---

## Anatomy of a Float Literal

A float literal consists of up to four parts:

```
    3_141.592_653e-2
    ^^^^^ ^^^^^^^ ^^
      |      |     |
      |      |     +--- exponent: 'e', sign '-', digits '2'
      |      +--------- fractional part: '592653' (underscores stripped)
      +---------------- integer part: '3141' (underscores stripped)
```

The decimal point (`.`) separates the integer part from the fractional part. The exponent marker (`e` or `E`) introduces the power-of-10 exponent. The mathematical value of this literal is `3141.592653 * 10^(-2) = 31.41592653`.

---

## Standard Decimal Floats

Decimal float literals consist of an integer part, a decimal point, and a fractional part:

```uranite
F64 pi = 3.14159265358979
F64 euler = 2.71828
F64 zero = 0.0
F64 fraction = 0.001
F64 large = 1000000.0
F64 precise = 1.23456789012345
```

Both the integer part and the fractional part must contain at least one digit. The integer part appears before the decimal point, and the fractional part appears after it. Together they define the numeric value in standard positional notation.

The simplest float literal is `0.0` — zero expressed as a floating-point value. Every float literal with a decimal point requires at least one digit on each side of the point.

---

## Scientific Notation

Scientific notation allows compact representation of very large or very small numbers. An exponent marker (`e` or `E`) follows the mantissa (integer part and optional fractional part), followed by an optional sign and one or more exponent digits:

```uranite
F64 avogadro = 6.022e23
F64 planck = 6.626e-34
F64 charge = 1.602E-19
F64 speed = 3.0e+8
F64 kilo = 1e3
```

The exponent marker is case-insensitive — `e` and `E` are equivalent. The value of `6.022e23` is `6.022 * 10^23`.

### Exponent Sign

The exponent sign is optional. When omitted, the exponent is positive:

| Literal | Exponent | Value |
|---|---|---|
| `1e10` | +10 | 10,000,000,000.0 |
| `1e+10` | +10 | 10,000,000,000.0 |
| `1e-10` | -10 | 0.0000000001 |
| `2.5E3` | +3 | 2,500.0 |
| `1.0e-7` | -7 | 0.0000001 |

Explicit `+` and implicit positive produce identical results. Use the sign that makes the literal most readable in context — scientific constants typically include the sign for clarity (`6.022e+23`), while small values naturally use the minus (`1.602e-19`).

### Integer-Only Scientific Notation

A literal with an exponent marker but no decimal point is still a float. The exponent marker alone is sufficient to make the literal floating-point:

```uranite
F64 million = 1e6
F64 tiny = 5e-3
F64 hundred = 1E2
```

The literal `1e6` produces the float value `1000000.0`, not the integer `1000000`. If an integer value is desired, use an integer literal instead: `I64 million = 1_000_000`.

---

## Underscore Separators

Underscores within float literals serve as visual separators, identical to their behavior in [integer literals](integer-literals.md). They are silently stripped before the value is computed.

### Where Underscores Are Allowed

Underscores can appear within the integer part and the fractional part of a float literal:

| Literal | Value | Notes |
|---|---|---|
| `3.141_592_653` | 3.141592653 | Fractional grouping |
| `1_000_000.0` | 1000000.0 | Integer part grouping |
| `1_234.567_890` | 1234.56789 | Both parts grouped |
| `123_456_789.123_456` | 123456789.123456 | Large number with grouping |

Recommended grouping for float literals follows the same convention as integers — groups of three digits:

```uranite
F64 distance = 149_597_870.700
F64 population = 8_000_000_000.0
F64 precise = 3.141_592_653_589_793
```

### Exponent Restriction

Underscores are **not** supported in the exponent part of a float literal. Only digits (and an optional leading sign) are valid after the `e`/`E` marker:

| Literal | Result |
|---|---|
| `1.0e10` | Valid: exponent is 10 |
| `1.0e+23` | Valid: exponent is +23 |
| `1.0e1_0` | Invalid: underscore terminates the exponent at `1` |

When an underscore appears in the exponent position, the exponent scanning stops at the underscore. The `_0` portion is not part of the float literal and will cause a compilation error. Always write exponent digits without underscores.

---

## Decimal Point Rules

The decimal point in Uranite has specific constraints that differ from some other languages.

### No Leading Decimal Point

A float literal must begin with a digit, not a decimal point. The syntax `.5` is not a valid float literal:

```uranite
F64 half = 0.5
```

Writing `.5` without a leading digit produces a syntax error because the `.` character is interpreted as a member access operator, not a decimal point. Always include an explicit leading zero for fractional values less than 1.0.

### No Trailing Decimal Point

A decimal point must be followed by at least one digit. The syntax `42.` is not a valid float literal:

```uranite
F64 answer = 42.0
```

Writing `42.` without a trailing digit produces `42` as an integer followed by the `.` operator. Always include an explicit trailing zero.

### Decimal Point vs Member Access

When a number is followed by `.` and then a non-digit character, the `.` is interpreted as the member access operator, not a decimal point. This allows method calls directly on integer literals:

| Source | Interpretation |
|---|---|
| `42.0` | Float literal with value 42.0 |
| `42.5` | Float literal with value 42.5 |
| `42.toString()` | Integer `42`, member access `.`, method call `toString()` |

The rule is simple: `.` followed by a digit starts the fractional part of a float. `.` followed by anything else is member access. This one-character lookahead makes both float literals and method calls on integers unambiguous.

### Decimal Point vs Range Operator

The range operator `..` is two consecutive dot characters. When a number is followed by `..`, the dots form the range operator, not a decimal point:

| Source | Interpretation |
|---|---|
| `0..10` | Integer `0`, range operator `..`, integer `10` |
| `0.0` | Float literal with value 0.0 |

The range operator always uses integers. Float ranges are not supported because the range `..` operator produces discrete values, and floating-point numbers are continuous.

---

## Negative Floats

Negative float values use the unary minus operator applied to a positive literal:

```uranite
F64 negative = -3.14
F64 absoluteZero = -273.15
F64 tiny = -1.0e-10
```

The minus sign is a separate unary operator, not part of the literal itself. `-3.14` is parsed as the operator `-` applied to the literal `3.14`. The compiler evaluates this at compile time and produces the expected negative value.

Negative zero is a valid IEEE 754 value:

```uranite
F64 negativeZero = -0.0
```

The value `-0.0` compares as equal to `0.0` (the expression `0.0 == -0.0` evaluates to `True`), but they are distinct at the bit level. See [Signed Zero](#signed-zero).

---

## Default Type: F64

Every float literal defaults to the `F64` type — a 64-bit double-precision floating-point number. This is true regardless of the magnitude of the value or whether scientific notation is used. The literals `3.14`, `1e10`, and `6.022e+23` all produce an `F64` value.

### Method Access on Floats

Because `F64` is an object type, float values have method access. Every float literal supports methods inherited from the `Object` class and the arithmetic interfaces:

```uranite
F64 value = 3.14
String text = value.toString()
I64 hash = value.hashCode()
Boolean same = value.equals( 3.14 )
```

The `toString()` method converts the float to its decimal string representation. The `hashCode()` method returns a hash value suitable for hash-based collections. The `equals()` method performs value comparison.

Additional methods specific to floating-point values are available:

```uranite
F64 result = 1.0 / 0.0
Boolean infinite = result.isInfinite()
Boolean notANumber = result.isNaN()
Boolean finite = result.isFinite()
```

These methods inspect the IEEE 754 classification of the value — whether it is a normal finite number, positive or negative infinity, or Not-a-Number.

### Type Narrowing Through Annotations

When a float literal is assigned to a variable with a narrower type annotation, the compiler performs implicit narrowing:

```uranite
F64 doublePrecision = 3.14159265358979
F32 singlePrecision = 3.14
```

The literal is initially an `F64` value. The variable's type annotation determines the final storage type. Assignment to an `F32` variable triggers implicit narrowing from 64-bit to 32-bit precision. No explicit cast is required.

Narrowing from `F64` to `F32` may lose precision. The value `3.14159265358979` stored in an `F32` variable retains only approximately 6-9 significant digits of precision.

---

## The Float Type System

Uranite provides two fixed-width floating-point types and three alias names.

### Float Types

| Type | Bits | IEEE 754 Format | Decimal Precision |
|---|---|---|---|
| `F32` | 32 | binary32 (single) | ~6-9 significant digits |
| `F64` | 64 | binary64 (double) | ~15-17 significant digits |

Both types use IEEE 754 representation. `F32` is single-precision (32 bits total: 1 sign, 8 exponent, 23 significand). `F64` is double-precision (64 bits total: 1 sign, 11 exponent, 52 significand).

### Alias Types

Three alias names map to the two underlying float types:

| Type | Description |
|---|---|
| `Float` | 64-bit floating-point type. Note: unlike Java or C where `float` is 32-bit, Uranite `Float` is 64-bit double-precision. |
| `Double` | 64-bit double-precision floating-point type. |

Use `F32` explicitly when 32-bit single-precision is needed.

### Choosing a Float Type

Use `F64` (or `Float`) as the default floating-point type for most purposes. Choose `F32` only when:

- **Interfacing with external APIs** that require single-precision values (e.g., GPU shader inputs, audio sample buffers)
- **Memory-constrained collections** where millions of values benefit from smaller storage (e.g., vertex positions in a 3D mesh)
- **Hardware requirements** where single-precision operations are explicitly needed

In general, the precision loss from `F32` is rarely worth the memory savings for small numbers of values. Default to `F64`:

```uranite
F64 generalPurpose = 3.14
F64 scientificValue = 6.022e23
F32 gpuCoordinate = 1.5
F32 audioSample = 0.75
```

---

## IEEE 754 Compliance

Uranite floating-point operations follow IEEE 754 semantics. This determines rounding behavior, special value handling, and the results of edge-case arithmetic.

### Rounding

The default rounding mode is round-to-nearest-even (also called "banker's rounding"). When a value falls exactly halfway between two representable numbers, it rounds to the value whose least significant bit is zero. This minimizes cumulative rounding bias over many operations.

### Special Values

IEEE 754 defines three categories of special values that Uranite fully supports:

| Value | Produced By | Behavior |
|---|---|---|
| Positive infinity (`+Inf`) | `1.0 / 0.0`, overflow | Greater than every finite number |
| Negative infinity (`-Inf`) | `-1.0 / 0.0`, negative overflow | Less than every finite number |
| Not-a-Number (`NaN`) | `0.0 / 0.0`, invalid operations | Not equal to anything, including itself |

The NaN inequality property is a critical edge case: `NaN != NaN` evaluates to `True`, and `NaN == NaN` evaluates to `False`. This is defined by IEEE 754, not a Uranite-specific behavior. To check whether a value is NaN, use the `isNaN()` method rather than equality comparison:

```uranite
F64 result = 0.0 / 0.0
Boolean wrong = result == result
Boolean correct = result.isNaN()
```

The variable `wrong` is `False` (NaN is not equal to itself). The variable `correct` is `True`.

### Signed Zero

IEEE 754 distinguishes between `+0.0` and `-0.0`. Both compare as equal:

```uranite
F64 positive = 0.0
F64 negative = -0.0
Boolean same = positive == negative
```

The variable `same` is `True`. Despite comparing as equal, the two zeros are distinct at the bit level and can produce different results in certain operations (e.g., `1.0 / 0.0` produces `+Inf` while `1.0 / -0.0` produces `-Inf`).

### No Automatic Overflow Checks

Unlike integer arithmetic, where the compiler inserts automatic zero-division checks, floating-point arithmetic does not raise exceptions on overflow or division by zero. Instead, it produces IEEE 754 special values:

| Operation | Result |
|---|---|
| `1.0 / 0.0` | `+Inf` |
| `-1.0 / 0.0` | `-Inf` |
| `0.0 / 0.0` | `NaN` |
| `1e308 * 10.0` | `+Inf` (overflow) |
| `-1e308 * 10.0` | `-Inf` (negative overflow) |

This differs from integer division, where dividing by zero raises a `ZeroDivisionError` at runtime. Float division by zero silently produces infinity or NaN — no runtime error occurs. Guard against this in application code when zero denominators are possible:

```uranite
public function safeDivide( F64 numerator, F64 denominator ) -> F64:
    if denominator == 0.0:
        return 0.0
    return numerator / denominator
```

### Inspecting Special Values

The float wrapper classes provide methods for inspecting IEEE 754 classification:

```uranite
F64 value = 1.0 / 0.0

Boolean infinite = value.isInfinite()
Boolean nan = value.isNaN()
Boolean finite = value.isFinite()
```

| Method | Returns `True` When |
|---|---|
| `isFinite()` | Value is a normal number or zero (not infinity, not NaN) |
| `isInfinite()` | Value is positive or negative infinity |
| `isNaN()` | Value is Not-a-Number |

These methods are the correct way to test for special values. Do not use equality comparison for NaN detection — NaN is not equal to itself by IEEE 754 definition.

---

## Precision and Range

### F64 Precision

The `F64` type (IEEE 754 binary64) provides:

| Property | Value |
|---|---|
| Total bits | 64 |
| Sign bit | 1 |
| Exponent bits | 11 |
| Significand bits | 52 (+ 1 implicit) |
| Decimal precision | ~15-17 significant digits |
| Minimum positive normal | ~2.225 x 10^-308 |
| Maximum finite | ~1.798 x 10^308 |
| Smallest subnormal | ~4.941 x 10^-324 |

### F32 Precision

The `F32` type (IEEE 754 binary32) provides:

| Property | Value |
|---|---|
| Total bits | 32 |
| Sign bit | 1 |
| Exponent bits | 8 |
| Significand bits | 23 (+ 1 implicit) |
| Decimal precision | ~6-9 significant digits |
| Minimum positive normal | ~1.175 x 10^-38 |
| Maximum finite | ~3.403 x 10^38 |

### Digit Significance

Float literals always start at `F64` precision, which represents approximately 15-17 significant decimal digits. Digits beyond this limit are rounded according to IEEE 754 rules:

```uranite
F64 precise = 3.14159265358979323846
```

This literal has 21 significant digits, but `F64` can only represent approximately 17. The trailing digits (`3846`) are rounded away. The stored value is `3.141592653589793` (16 digits).

When narrowed to `F32`, the precision drops to approximately 7 significant digits:

```uranite
F32 rough = 3.14159265358979
```

The stored value is approximately `3.1415927` — only about 7 digits survive the narrowing.

For applications requiring more than 17 significant digits (e.g., financial calculations, arbitrary-precision mathematics), floating-point types are not appropriate. Use integer arithmetic with explicit scaling, or a dedicated high-precision library.

---

## Float Literals Are Decimal Only

Float literals are exclusively decimal-based. Hexadecimal, octal, and binary prefixes cannot produce float values:

| Literal | Type | Reason |
|---|---|---|
| `3.14` | Float | Decimal with fractional part |
| `1e10` | Float | Decimal with exponent |
| `0xFF` | Integer | Hex prefix, no decimal/exponent possible |
| `0o77` | Integer | Octal prefix, no decimal/exponent possible |
| `0b1010` | Integer | Binary prefix, no decimal/exponent possible |

Writing `0xFF.5` or `0b1010.1` is not valid — the decimal point after a non-decimal prefix is interpreted as the member access operator, not a fractional separator.

To express a hex value as a float, convert through an integer variable using the `as` cast:

```uranite
I64 hexValue = 0xFF
F64 floatValue = hexValue as F64
```

---

## Examples

### Valid Float Literals

```uranite
F64 pi = 3.14159265358979
F64 euler = 2.718_281_828
F64 zero = 0.0
F64 fraction = 0.001
F64 large = 1_000_000.0
F64 grouped = 123_456.789_012

F64 avogadro = 6.022e23
F64 planck = 6.626e-34
F64 charge = 1.602E-19
F64 speed = 3.0e+8

F64 integerExponent = 1e10
F64 negativeExponent = 5e-3
```

All of these compile without error. Each produces a valid `F64` value.

### Invalid Float Literals

The following are **not** valid float literals:

| Literal | Problem |
|---|---|
| `.5` | Leading dot without integer part. Write `0.5` instead. |
| `42.` | Trailing dot without fractional part. Write `42.0` instead. |
| `0x1.5` | Hex literals cannot have decimal points. |
| `0b1.0` | Binary literals cannot have decimal points. |
| `1.0e` | Exponent marker with no digits. |
| `1.0e1_0` | Underscores not supported in exponent part. |

### Edge Cases

```uranite
F64 almostZero = 0.0
F64 negativeZero = -0.0
F64 verySmall = 1e-300
F64 veryLarge = 1.7976931348623157e308
F64 oneExponent = 1e0
```

The literal `1e0` equals `1.0` — any number raised to the zero power is one. Despite being mathematically equal to the integer `1`, `1e0` is a float because of the exponent marker.

The literal `1.7976931348623157e308` is near the maximum representable `F64` value. Values beyond this magnitude overflow to positive infinity.

### Practical Usage

A complete example demonstrating float literals across standard decimal, scientific notation, and method access:

```uranite
from uranite.io.console import puts

const F64 GRAVITATIONAL_CONSTANT = 6.674e-11
const F64 SPEED_OF_LIGHT = 299_792_458.0
const F64 PI = 3.14159265358979

public function circleArea( F64 radius ) -> F64:
    return PI * radius * radius

public function circumference( F64 radius ) -> F64:
    return 2.0 * PI * radius

public function kineticEnergy( F64 mass, F64 velocity ) -> F64:
    return 0.5 * mass * velocity * velocity

public function gravitationalForce( F64 massA, F64 massB, F64 distance ) -> F64:
    return GRAVITATIONAL_CONSTANT * massA * massB / ( distance * distance )

public function main() -> I32:
    F64 radius = 5.0
    F64 area = circleArea( radius )
    F64 circ = circumference( radius )
    puts( "Area: " + area.toString() )
    puts( "Circumference: " + circ.toString() )

    F64 energy = kineticEnergy( 2.5, 10.0 )
    puts( "Kinetic energy: " + energy.toString() )

    F64 earthMass = 5.972e24
    F64 moonMass = 7.342e22
    F64 earthMoonDistance = 3.844e8
    F64 force = gravitationalForce( earthMass, moonMass, earthMoonDistance )
    puts( "Earth-Moon gravitational force: " + force.toString() )

    F64 ratio = force / energy
    if ratio.isFinite():
        puts( "Ratio is finite: " + ratio.toString() )

    F64 dangerous = energy / 0.0
    if dangerous.isInfinite():
        puts( "Division by zero produced infinity" )

    return 0
```

This example demonstrates decimal float literals with underscore grouping, scientific notation for physical constants, method calls on float values (`toString()`, `isFinite()`, `isInfinite()`), arithmetic operations, and the IEEE 754 behavior of float division by zero producing infinity rather than a runtime error.
