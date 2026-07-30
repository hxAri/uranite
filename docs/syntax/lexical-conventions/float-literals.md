# Float Literals

Uranite supports floating-point literals with decimal notation and optional scientific exponent notation. All float literals are stored as IEEE 754 double-precision values (`double`, 64-bit) and typed as `F64` during semantic analysis. This document specifies the lexer scanning rules, the decimal-point disambiguation logic, underscore separator handling, scientific notation syntax, type assignment, and LLVM code generation.

---

## Table of Contents

- [Overview](#overview)
- [Anatomy of a Float Literal](#anatomy-of-a-float-literal)
- [Lexer Scanning Rules](#lexer-scanning-rules)
  - [Integer-to-Float Transition](#integer-to-float-transition)
  - [The Decimal Point Disambiguation](#the-decimal-point-disambiguation)
  - [Scientific Notation](#scientific-notation)
  - [Type Suffix Scanning](#type-suffix-scanning)
  - [Complete Scan Flowchart](#complete-scan-flowchart)
- [Underscore Separators](#underscore-separators)
- [The Parsing Pipeline](#the-parsing-pipeline)
  - [Stage 1: Lexer Scanning](#stage-1-lexer-scanning)
  - [Stage 2: Parser Conversion](#stage-2-parser-conversion)
  - [Stage 3: Semantic Type Assignment](#stage-3-semantic-type-assignment)
  - [Stage 4: HIR Representation](#stage-4-hir-representation)
  - [Stage 5: MIR Code Generation](#stage-5-mir-code-generation)
- [Default Type: F64](#default-type-f64)
  - [OOP Wrapper Resolution](#oop-wrapper-resolution)
  - [Contextual Typing](#contextual-typing)
  - [The Float and Double Aliases](#the-float-and-double-aliases)
- [The FloatType System](#the-floattype-system)
- [IEEE 754 Compliance](#ieee-754-compliance)
- [Precision and Range](#precision-and-range)
- [Examples](#examples)
  - [Valid Float Literals](#valid-float-literals)
  - [Invalid Float Literals](#invalid-float-literals)
  - [Edge Cases](#edge-cases)
  - [Practical Usage](#practical-usage)

---

## Overview

A floating-point literal represents a real number with a fractional component, an exponent, or both. The lexer produces a `LiteralFloat` token when it detects either a decimal point followed by a digit or an exponent marker (`e`/`E`) during numeric scanning.

| Component | Required | Description |
|---|---|---|
| Integer part | Yes | One or more decimal digits before the decimal point. |
| Decimal point | Conditional | Required if no exponent. The `.` character. |
| Fractional part | Conditional | One or more decimal digits after the decimal point. |
| Exponent marker | Optional | `e` or `E`, optionally followed by `+` or `-` and digits. |

At least one of the decimal point or exponent marker must be present to distinguish a float from an integer. The literal `42` is an integer; `42.0`, `42e0`, and `42.0e0` are floats.

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

The decimal point (`.`) separates the integer part from the fractional part. The exponent marker (`e` or `E`) introduces the power-of-10 exponent. The mathematical value of this literal is `3141.592653 × 10^(-2) = 31.41592653`.

---

## Lexer Scanning Rules

The lexer does not have a dedicated entry point for float literals. Float scanning is an extension of integer scanning — the `readNumber()` method begins by scanning a decimal digit sequence, then checks for float transition markers.

### Integer-to-Float Transition

After scanning the integer part (a sequence of digits and underscores), the lexer checks two conditions to determine if the literal is a float:

**Condition 1 — Decimal point followed by digit:**

```
if current character is '.' and next character is a digit:
    set isFloatingPoint = true
    append '.' to value
    advance past '.'
    scan fractional digits and underscores
```

**Condition 2 — Exponent marker:**

```
if current character is 'e' or 'E':
    set isFloatingPoint = true
    append 'e'/'E' to value
    advance past exponent marker
    if next character is '+' or '-':
        append sign to value
        advance past sign
    scan exponent digits
```

If neither condition is met, the literal remains an integer.

### The Decimal Point Disambiguation

The lexer must distinguish between a decimal point in a float literal and a dot operator (`.`) used for member access. It does this with a one-character lookahead:

```
current = '.'
peek(1) = next character
```

- If `peek()` returns a digit (`0`-`9`): the `.` is a decimal point. The literal transitions to a float.
- If `peek()` returns anything else (letter, underscore, dot, whitespace, etc.): the `.` is a dot operator. The lexer stops scanning the number and returns it as an integer.

This means:

| Source | Tokens |
|---|---|
| `42.0` | `LiteralFloat("42.0")` |
| `42.5` | `LiteralFloat("42.5")` |
| `42.toString()` | `LiteralInteger("42")` `Dot` `Identifier("toString")` `LeftParenthesis` `RightParenthesis` |
| `42..50` | `LiteralInteger("42")` `DoubleDot` `LiteralInteger("50")` |

The critical case is `42.toString()` — the `.` is followed by `t` (not a digit), so the lexer treats `42` as an integer and `.` as a dot operator. Method calls on integer literals work correctly because of this lookahead.

**Trailing decimal points are not supported.** The literal `42.` (dot followed by non-digit) produces `LiteralInteger("42")` followed by `Dot`. There is no "float with implicit zero fractional part" syntax.

**Leading decimal points are not supported.** The literal `.5` does not start with a digit, so the lexer does not dispatch to `readNumber()`. Instead, `.` is tokenized as a `Dot` operator, and `5` is tokenized as `LiteralInteger("5")`. To write a float less than 1.0, use `0.5`.

### Scientific Notation

After scanning the integer part and (optionally) the fractional part, the lexer checks for an exponent marker:

```
if current character is 'e' or 'E':
    mark as floating point
    append 'e'/'E'
    advance
    if current character is '+' or '-':
        append sign
        advance
    scan consecutive digits (no underscores in exponent)
```

The exponent marker is case-insensitive (`e` and `E` are equivalent). The sign is optional — a missing sign defaults to positive. The exponent digits do **not** support underscore separators.

Examples:

| Literal | Value | Components |
|---|---|---|
| `1e10` | 10,000,000,000.0 | Integer `1`, exponent `+10` |
| `2.5E3` | 2,500.0 | Integer `2`, fraction `.5`, exponent `+3` |
| `1.0e-7` | 0.0000001 | Integer `1`, fraction `.0`, exponent `-7` |
| `6.022e+23` | 6.022 × 10²³ | Integer `6`, fraction `.022`, exponent `+23` |

A literal with only an exponent and no decimal point (e.g., `42e5`) is still classified as a float. The `isFloatingPoint` flag is set when the exponent marker is encountered, regardless of whether a decimal point was present.

### Type Suffix Scanning

After scanning all numeric components (integer part, optional fraction, optional exponent), the lexer checks if the next character is alphabetic. If so, it reads consecutive alphanumeric characters as a type suffix and appends them to the token value:

```
Source:  3.14f32
Token:   LiteralFloat, value = "3.14f32"
```

As with integer suffixes, the parser does not currently use float suffixes for type inference. All float literals are typed as `F64` regardless of any suffix. The suffix is preserved in the raw token string but ignored during numeric conversion.

### Complete Scan Flowchart

The full scanning sequence for a numeric literal:

```
1. Check base prefix (0b, 0o, 0x) → if found, scan as integer and return
2. Scan decimal digits and underscores → integer part
3. Check for '.' followed by digit:
   → Yes: append '.', scan fractional digits/underscores, set float flag
   → No:  skip (integer so far)
4. Check for 'e' or 'E':
   → Yes: append marker, optional sign, scan exponent digits, set float flag
   → No:  skip
5. Check for alphabetic suffix → append if present
6. Return LiteralFloat if float flag set, LiteralInteger otherwise
```

Note that hexadecimal, octal, and binary literals (step 1) always return as integers — they cannot have decimal points or exponents. Float literals are exclusively decimal-based.

---

## Underscore Separators

Underscores are supported in the integer part and the fractional part of a float literal. They are stripped during scanning, exactly as with integer literals:

```
Source:  3.141_592_653
Token:   LiteralFloat, value = "3.141592653"

Source:  1_000_000.50
Token:   LiteralFloat, value = "1000000.50"

Source:  1_234.567_890
Token:   LiteralFloat, value = "1234.567890"
```

Underscores are **not** supported in the exponent part. The exponent scanning loop uses `std::isdigit()` only — no underscore check:

```
Source:  1.0e1_0
Scanned: exponent digits stop at '1', underscore terminates exponent
Result:  LiteralFloat, value = "1.0e1" (the "_0" is not part of the literal)
```

The `_0` after the exponent would be scanned as a separate identifier token (`_0`).

---

## The Parsing Pipeline

### Stage 1: Lexer Scanning

The `readNumber()` method produces a `LiteralFloat` token with a raw string value. Underscores have been stripped, but all other characters (digits, decimal point, exponent marker, sign, suffix) are preserved:

| Source | Token Value |
|---|---|
| `3.14` | `"3.14"` |
| `2.718_28` | `"2.71828"` |
| `1e10` | `"1e10"` |
| `6.022e+23` | `"6.022e+23"` |
| `1_000.50` | `"1000.50"` |

### Stage 2: Parser Conversion

When the parser encounters a `LiteralFloat` token, it converts the raw string to a `double` using `std::stod()`:

```
std::string raw = current token value
double value = std::stod(raw)
return FloatLiteralExpression(raw, source, value)
```

The conversion is straightforward — `std::stod()` handles decimal notation, scientific notation, and signs. The resulting `FloatLiteralExpression` AST node stores both the raw string representation and the parsed `double` value.

Unlike integer parsing, there is no base-detection logic. All float literals are decimal, so `std::stod()` is called directly on the raw string without prefix stripping.

### Stage 3: Semantic Type Assignment

The semantic analyzer processes `FloatLiteral` expression nodes:

1. Look up the `F64` class type via `typeRegistry.lookupType("F64")`.
2. If found, use the class type (OOP wrapper `uranite.language.f64.F64`).
3. If not found (fallback), use the primitive float type `typeRegistry.getFloat64()` (primitive `uranite.builtin.f64`).

Every float literal receives the type `F64`. There is no literal-level distinction between `F32` and `F64`.

### Stage 4: HIR Representation

The HIR (High-level IR) preserves the float value in an `HIRFloatLiteral` node:

| Field | Type | Description |
|---|---|---|
| `floatValue` | `double` | The parsed 64-bit floating-point value. |
| `rawRepresentation` | `std::string` | The original source text of the literal. |
| `resolvedType` | `TypeSharedPointer` | The `F64` type from semantic analysis. |

The raw representation is carried through the IR for diagnostic messages and debug output. The numeric value is used for code generation.

### Stage 5: MIR Code Generation

The `generateConstantFloat()` method in MIR codegen maps the float value to an LLVM constant:

```
llvm::ConstantFP::get(
    llvm::Type::getDoubleTy(context),
    instruction.floatConstantValue
)
```

The LLVM type is always `double` (IEEE 754 binary64). The `ConstantFP` value is stored as the variable's runtime representation. For global constants, the same `ConstantFP::get()` call produces the initializer value.

---

## Default Type: F64

### OOP Wrapper Resolution

Every float literal is typed as `F64` — the 64-bit double-precision OOP wrapper class. The semantic analyzer resolves this by looking up "F64" in the type registry, which resolves to `uranite.language.f64.F64`.

The OOP wrapper provides methods like `toString()`, `hashCode()`, `equals()`, and arithmetic operator implementations through interface dispatch. Float literals have method access:

```uranite
F64 value = 3.14
String text = value.toString()
Boolean finite = value.isFinite()
F64 rounded = value.round()
```

If the OOP wrapper is not available, the semantic analyzer falls back to the primitive `uranite.builtin.f64` type.

### Contextual Typing

When a float literal is assigned to a variable with a narrower type annotation, implicit narrowing occurs:

```uranite
F32 single = 3.14
F64 precise = 3.14159265358979
```

The literal `3.14` is parsed as `double(3.14)` and initially typed as `F64`. Assignment to an `F32` variable triggers implicit narrowing. The LLVM backend emits an `fptrunc` instruction to convert from `double` to `float` at the instruction selection level.

### The Float and Double Aliases

The type registry maps several names to the same underlying float types:

| Name | Resolves To | Bit Width |
|---|---|---|
| `F32` | `uranite.language.f32.F32` | 32 |
| `Float` | `uranite.language.float.Float` | 64 |
| `F64` | `uranite.language.f64.F64` | 64 |
| `Double` | `uranite.language.double.Double` | 64 |

Note that `Float` resolves to 64-bit precision (same as `F64` and `Double`), not 32-bit. This differs from languages like Java where `float` is 32-bit. In Uranite, `Float` is an alias for double-precision. Use `F32` explicitly when 32-bit single-precision is needed.

---

## The FloatType System

The compiler represents floating-point types through the `FloatType` struct, which extends the base `Type` with a single field:

| Field | Type | Description |
|---|---|---|
| `bitWidth` | `int` | Number of bits: 32 or 64. |

The type registry pre-creates both standard float types during initialization:

| Type Name | Primitive Qualified Name | Bit Width | LLVM Type |
|---|---|---|---|
| `F32` | `uranite.builtin.f32` | 32 | `float` |
| `F64` | `uranite.builtin.f64` | 64 | `double` |

The `FloatType` constructor auto-generates the type name from bit width: `"f32"` or `"f64"`.

Type compatibility follows width ordering. An `F32` value can be implicitly widened to `F64` (lossless), but narrowing from `F64` to `F32` may lose precision.

---

## IEEE 754 Compliance

Uranite float operations rely on IEEE 754 semantics as implemented by the LLVM backend and the target hardware. This means:

- **Rounding**: Default rounding mode is round-to-nearest-even.
- **Special values**: `+Infinity`, `-Infinity`, and `NaN` are representable and produced by operations like division by zero or `0.0 / 0.0`.
- **Signed zero**: Both `+0.0` and `-0.0` exist. They compare as equal (`0.0 == -0.0` is `True`).
- **No automatic overflow checks**: Float arithmetic does not raise exceptions on overflow — it produces `Infinity`. This differs from integer arithmetic, where the compiler inserts wrapping arithmetic and zero-division checks.

The OOP wrapper classes provide methods for inspecting special values:

```uranite
F64 value = 1.0 / 0.0
Boolean inf = value.isInfinite()
Boolean nan = value.isNaN()
Boolean fin = value.isFinite()
```

---

## Precision and Range

The `F64` type (IEEE 754 binary64) provides:

| Property | Value |
|---|---|
| Total bits | 64 |
| Sign bit | 1 |
| Exponent bits | 11 |
| Significand bits | 52 (+ 1 implicit) |
| Decimal precision | ~15-17 significant digits |
| Minimum positive normal | ~2.225 × 10⁻³⁰⁸ |
| Maximum finite | ~1.798 × 10³⁰⁸ |
| Smallest subnormal | ~4.941 × 10⁻³²⁴ |

The `F32` type (IEEE 754 binary32) provides:

| Property | Value |
|---|---|
| Total bits | 32 |
| Sign bit | 1 |
| Exponent bits | 8 |
| Significand bits | 23 (+ 1 implicit) |
| Decimal precision | ~6-9 significant digits |
| Minimum positive normal | ~1.175 × 10⁻³⁸ |
| Maximum finite | ~3.403 × 10³⁸ |

Float literals always start at `F64` precision. Source text like `3.14159265358979323846` is converted to a `double` by `std::stod()`, which gives approximately 15-17 digits of precision. Digits beyond that are rounded according to IEEE 754 rules.

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

F64 integer_exponent = 1e10
F64 negative_exponent = 5e-3
```

### Invalid Float Literals

The following are **not** valid float literals:

```
.5          → leading dot is a Dot token, not a decimal point
42.         → trailing dot followed by non-digit; produces integer 42 + Dot
0x1.5       → hex literals cannot have decimal points
0b1.0       → binary literals cannot have decimal points
1.0e         → exponent marker with no digits (std::stod fails)
1.0e1_0     → underscore not supported in exponent part
```

To represent `0.5`, write `0.5` with an explicit leading zero. To represent `42.0`, include the trailing zero explicitly.

### Edge Cases

```uranite
F64 almostZero = 0.0
F64 negativeZero = -0.0
F64 verySmall = 1e-300
F64 veryLarge = 1.7976931348623157e308
```

The literal `-0.0` is parsed as the unary negation operator (`-`) applied to the float literal `0.0`. The lexer produces `Minus` `LiteralFloat("0.0")`, and the parser constructs a negation expression. At the LLVM level, this produces IEEE 754 negative zero.

The literal `1.7976931348623157e308` is near the maximum representable `F64` value. Values beyond this magnitude overflow to `Infinity` during `std::stod()` conversion — the parser does not currently catch this overflow or produce a diagnostic.

### Practical Usage

```uranite
from uranite.io.console import puts

public const F64 GRAVITATIONAL_CONSTANT = 6.674e-11
public const F64 SPEED_OF_LIGHT = 299_792_458.0
public const F64 PI = 3.14159265358979

public function circleArea( F64 radius ) -> F64:
    return PI * radius * radius

public function kinetic( F64 mass, F64 velocity ) -> F64:
    return 0.5 * mass * velocity * velocity

public function main() -> I32:
    F64 area = circleArea( 5.0 )
    puts( area.toString() )

    F64 energy = kinetic( 2.5, 10.0 )
    puts( energy.toString() )

    return 0
```

This example demonstrates decimal float literals, scientific notation for physical constants, underscore grouping in large values, and method calls on float values through the OOP wrapper.
