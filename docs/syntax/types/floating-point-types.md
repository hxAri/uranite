# Floating-Point Types

Uranite provides two floating-point types: `F32` (single-precision, 32-bit) and `F64` (double-precision, 64-bit). All floating-point literals default to `F64`. Floating-point arithmetic follows the IEEE 754 standard, with special values (`NaN`, `Infinity`, `-Infinity`) handled natively by the LLVM backend. Like all primitive types, floating-point values carry zero-cost OOP wrappers with builtin methods that compile to LLVM intrinsics — no function calls, no heap allocation, no virtual dispatch.

This document covers the complete floating-point type inventory, LLVM representation, IEEE 754 compliance and special value semantics, arithmetic with compile-time constant folding, comparison predicates, casting between float widths and between float and integer, and the full set of builtin methods emitted as LLVM intrinsics.

---

## Table of Contents

- [Floating-Point Type Inventory](#floating-point-type-inventory)
  - [F32](#f32)
  - [F64](#f64)
  - [Aliases](#aliases)
- [LLVM Representation](#llvm-representation)
  - [Type Mapping](#type-mapping)
  - [The FloatType Struct](#the-floattype-struct)
  - [Memory Layout and Alignment](#memory-layout-and-alignment)
- [IEEE 754 Compliance](#ieee-754-compliance)
  - [Representation Format](#representation-format)
  - [Special Values](#special-values)
  - [NaN Semantics](#nan-semantics)
  - [Infinity Semantics](#infinity-semantics)
  - [Division by Zero](#division-by-zero)
- [Arithmetic Semantics](#arithmetic-semantics)
  - [Basic Arithmetic Operations](#basic-arithmetic-operations)
  - [Compile-Time Constant Folding](#compile-time-constant-folding)
  - [Mixed-Type Operand Coercion](#mixed-type-operand-coercion)
  - [Negation](#negation)
- [Comparison Semantics](#comparison-semantics)
  - [Ordered Comparisons](#ordered-comparisons)
  - [NaN in Comparisons](#nan-in-comparisons)
  - [Mixed Float-Integer Comparisons](#mixed-float-integer-comparisons)
- [Casting and Type Coercion](#casting-and-type-coercion)
  - [Float Widening (F32 to F64)](#float-widening-f32-to-f64)
  - [Float Narrowing (F64 to F32)](#float-narrowing-f64-to-f32)
  - [Integer to Float](#integer-to-float)
  - [Float to Integer](#float-to-integer)
  - [Implicit Store Coercion](#implicit-store-coercion)
- [Assignability Rules](#assignability-rules)
  - [Float-to-Float Assignability](#float-to-float-assignability)
  - [Integer-to-Float Assignability](#integer-to-float-assignability)
  - [Float-to-Integer Assignability](#float-to-integer-assignability)
  - [OOP Wrapper Assignability](#oop-wrapper-assignability)
- [Builtin Methods](#builtin-methods)
  - [Classification Methods](#classification-methods)
  - [Rounding Methods](#rounding-methods)
  - [Mathematical Methods](#mathematical-methods)
  - [Arithmetic Methods](#arithmetic-methods)
  - [Comparison Methods](#comparison-methods)
  - [Conversion Methods](#conversion-methods)
- [The OOP Wrapper Hierarchy](#the-oop-wrapper-hierarchy)
  - [Float Base Class](#float-base-class)
  - [F32 Wrapper](#f32-wrapper)
  - [F64 Wrapper](#f64-wrapper)
  - [Double Wrapper](#double-wrapper)
- [Examples](#examples)
  - [Declarations and Literals](#declarations-and-literals)
  - [Special Value Handling](#special-value-handling)
  - [Rounding and Math](#rounding-and-math)
  - [Casting Between Types](#casting-between-types)
  - [Practical Usage](#practical-usage)

---

## Floating-Point Type Inventory

### F32

32-bit single-precision floating-point. Approximately 7 decimal digits of precision. LLVM type: `float`. OOP wrapper class: `final class F32 extends Float` in `stdlibs/language/f32.urn`. Qualified name: `uranite.language.f32.F32`.

The `F32` constructor widens its parameter to `F64` internally (`self.value = value as F64`) because the `Float` base class stores a `protect F64 value` field for uniform arithmetic.

### F64

64-bit double-precision floating-point. Approximately 15 decimal digits of precision. LLVM type: `double`. OOP wrapper class: `final class F64 extends Float` in `stdlibs/language/f64.urn`. Qualified name: `uranite.language.f64.F64`.

This is the default floating-point type. All undecorated float literals (e.g., `3.14`, `2.718`) are typed as `F64`.

### Aliases

| Alias | Resolves To | Qualified Name |
|---|---|---|
| `Float` | `F64` (64-bit) | `uranite.language.float.Float` |
| `Double` | `F64` (64-bit) | `uranite.language.double.Double` |

Both `Float` and `Double` resolve to the same `FloatType(64)` instance in the type registry. `Float` is the base class in the OOP wrapper hierarchy, and `Double` is a `final class` extending `Float`.

---

## LLVM Representation

### Type Mapping

| Uranite Type | LLVM IR Type | C Equivalent |
|---|---|---|
| `F32` | `float` | `float` |
| `F64`, `Float`, `Double` | `double` | `double` |

The `toLLVMType()` function in `MIRCodegen` maps `Type::Kind::Float` using the `bitWidth` field:

```
bitWidth 32  → llvm::Type::getFloatTy(context)
bitWidth 64  → llvm::Type::getDoubleTy(context)
```

For OOP wrapper class types, `toLLVMType()` recognizes qualified names and maps directly:

| Qualified Name | LLVM Type |
|---|---|
| `uranite.language.f32.F32` | `float` |
| `uranite.language.f64.F64` | `double` |
| `uranite.language.float.Float` | `double` |
| `uranite.language.double.Double` | `double` |

### The FloatType Struct

The compiler represents floating-point types internally using the `FloatType` struct, defined in `src/uranite/semantic/typeref.hpp`:

```
struct FloatType : Type
    int bitWidth       — precision in bits (32 or 64)
```

The constructor auto-generates the type name: `"f"` followed by the bit count. `FloatType(32)` creates `"f32"`, and `FloatType(64)` creates `"f64"`. Unlike `IntegerType`, there is no signedness field — IEEE 754 floats are always signed.

### Memory Layout and Alignment

| Type | Storage Size | Typical Alignment (x86-64) |
|---|---|---|
| `F32` | 4 bytes | 4 bytes |
| `F64` | 8 bytes | 8 bytes |

`F32` uses the IEEE 754 binary32 format: 1 sign bit, 8 exponent bits, 23 mantissa bits. `F64` uses the IEEE 754 binary64 format: 1 sign bit, 11 exponent bits, 52 mantissa bits.

---

## IEEE 754 Compliance

### Representation Format

Uranite's floating-point types directly use IEEE 754 representation through LLVM. No custom floating-point format is employed — all arithmetic, comparison, and special value behavior follows the IEEE 754 standard as implemented by the target hardware.

| Property | F32 (binary32) | F64 (binary64) |
|---|---|---|
| Sign bits | 1 | 1 |
| Exponent bits | 8 | 11 |
| Mantissa bits | 23 | 52 |
| Exponent bias | 127 | 1023 |
| Decimal digits of precision | ~7 | ~15 |
| Maximum value | ~3.4028235 x 10^38 | ~1.7976931 x 10^308 |
| Minimum positive normal | ~1.1754944 x 10^-38 | ~2.2250739 x 10^-308 |
| Minimum positive subnormal | ~1.4 x 10^-45 | ~5.0 x 10^-324 |

### Special Values

IEEE 754 defines three categories of special values, all fully supported by Uranite:

**Positive and negative zero:** `+0.0` and `-0.0` are distinct bit patterns but compare as equal (`+0.0 == -0.0` is `True`).

**Infinity:** `+Infinity` and `-Infinity` result from overflow or division by zero. They propagate through arithmetic: `Infinity + 1.0` remains `Infinity`.

**NaN (Not a Number):** Results from undefined operations like `0.0 / 0.0` or `sqrt(-1.0)`. NaN propagates through all arithmetic and poisons comparisons.

### NaN Semantics

NaN follows IEEE 754 rules:

- NaN is not equal to anything, including itself: `NaN == NaN` is `False`.
- NaN is not less than, greater than, or equal to any value.
- Any arithmetic operation involving NaN produces NaN.
- `isNaN()` detects NaN via the self-inequality property.

The codegen implements `isNaN()` using `CreateFCmpUNO(self, self)` — an **unordered** comparison that returns `true` when either operand is NaN. Since both operands are the same value, this returns `true` only when the value is NaN.

### Infinity Semantics

Infinity follows IEEE 754 rules:

- `Infinity + Infinity` = `Infinity`
- `Infinity - Infinity` = `NaN`
- `Infinity * 0.0` = `NaN`
- `Infinity / Infinity` = `NaN`
- `1.0 / Infinity` = `0.0`
- `-Infinity < Infinity` is `True`

The codegen implements `isInfinite()` by computing `fabs(self) == +Infinity`:

1. `CreateUnaryIntrinsic(llvm::Intrinsic::fabs, self)` — takes absolute value.
2. `ConstantFP::getInfinity(primitiveType)` — creates the `+Infinity` constant.
3. `CreateFCmpOEQ(absVal, inf)` — ordered comparison returns `true` for both `+Infinity` and `-Infinity`.

### Division by Zero

Floating-point division by zero does **not** trap or throw an exception. Following IEEE 754:

- `1.0 / 0.0` produces `+Infinity`.
- `-1.0 / 0.0` produces `-Infinity`.
- `0.0 / 0.0` produces `NaN`.

The codegen uses `CreateFDiv` for float division, which maps directly to the hardware `fdiv` instruction. The hardware handles the special cases per IEEE 754 — no guard code is inserted.

This contrasts with integer division, where dividing by zero causes undefined behavior (typically SIGFPE). Float division by zero is always well-defined.

---

## Arithmetic Semantics

### Basic Arithmetic Operations

Floating-point arithmetic maps to dedicated MIR instruction kinds, which the codegen emits as LLVM IR instructions:

| Uranite Operator | MIR Instruction | LLVM IR Instruction | Label |
|---|---|---|---|
| `+` | `AddFloat` | `CreateFAdd` | `"add.f"` |
| `-` | `SubtractFloat` | `CreateFSub` | `"sub.f"` |
| `*` | `MultiplyFloat` | `CreateFMul` | `"mul.f"` |
| `/` | `DivideFloat` | `CreateFDiv` | `"div.f"` |

All float arithmetic follows IEEE 754 rounding rules (round-to-nearest, ties-to-even by default). No fast-math flags are set — the compiler does not enable `-ffast-math` style optimizations that could change NaN or infinity behavior.

### Compile-Time Constant Folding

When both operands of a floating-point operation are compile-time constants, the codegen folds the operation at compile time using LLVM's `APFloat` arbitrary-precision floating-point type. This eliminates the runtime instruction entirely.

For each arithmetic operation, the codegen checks if both operands are `ConstantFP`:

**Addition folding:**
1. Extract `APFloat` from both constants via `getValueAPF()`.
2. Call `res.add(right, rmNearestTiesToEven)` — IEEE 754 round-to-nearest.
3. Create a new `ConstantFP` from the result.

**Subtraction folding:** Uses `res.subtract(right, rmNearestTiesToEven)`.

**Multiplication folding:** Uses `res.multiply(right, rmNearestTiesToEven)`.

**Division folding:** Uses `res.divide(right, rmNearestTiesToEven)`.

If either operand is not a constant, the codegen falls through to the runtime `CreateFAdd`/`CreateFSub`/`CreateFMul`/`CreateFDiv` instruction.

### Mixed-Type Operand Coercion

When a floating-point operation has one integer and one float operand, the codegen coerces the integer operand to `double` before proceeding:

1. Detect that either operand is floating-point.
2. If the left operand is integer, convert via `CreateSIToFP(left, doubleTy)`.
3. If the right operand is integer, convert via `CreateSIToFP(right, doubleTy)`.
4. Proceed with the float arithmetic instruction.

Pointer operands are first converted to `i64` via `CreatePtrToInt`, then promoted to `double` if the other operand is floating-point.

### Negation

Float negation uses `CreateFNeg`, which flips the sign bit. This handles all special values correctly:

- `FNeg(+0.0)` = `-0.0`
- `FNeg(-0.0)` = `+0.0`
- `FNeg(NaN)` = `NaN` (with sign bit flipped)
- `FNeg(+Infinity)` = `-Infinity`

---

## Comparison Semantics

### Ordered Comparisons

Float comparisons use LLVM's **ordered** comparison predicates. "Ordered" means the comparison returns `false` if either operand is NaN.

| Uranite Operator | LLVM Predicate | Instruction | Label |
|---|---|---|---|
| `==` | `OEQ` (ordered equal) | `CreateFCmpOEQ` | `"eq"` |
| `!=` | `ONE` (ordered not equal) | `CreateFCmpONE` | `"ne"` |
| `<` | `OLT` (ordered less than) | `CreateFCmpOLT` | `"lt"` |
| `>` | `OGT` (ordered greater than) | `CreateFCmpOGT` | `"gt"` |
| `<=` | `OLE` (ordered less or equal) | `CreateFCmpOLE` | `"le"` |
| `>=` | `OGE` (ordered greater or equal) | `CreateFCmpOGE` | `"ge"` |

### NaN in Comparisons

Because Uranite uses ordered predicates, NaN comparisons follow IEEE 754:

- `NaN == NaN` → `False` (OEQ: both must be non-NaN and equal)
- `NaN != NaN` → `True` (ONE: at least one NaN or values differ)
- `NaN < 1.0` → `False`
- `NaN > 1.0` → `False`
- `NaN <= 1.0` → `False`
- `NaN >= 1.0` → `False`

The only predicate that returns `true` for NaN is `UNO` (unordered), used internally by `isNaN()`. All user-facing comparison operators use ordered predicates.

### Mixed Float-Integer Comparisons

When comparing a float and an integer, the codegen promotes the integer to `double` via `CreateSIToFP` before performing the float comparison. The result uses float comparison predicates (OEQ, OLT, etc.), not integer comparison predicates.

---

## Casting and Type Coercion

### Float Widening (F32 to F64)

Widening from `F32` to `F64` uses `CreateFPExt` (floating-point extension). This is a lossless conversion — every `F32` value is exactly representable as `F64`.

```
source: float 3.14
target: double
→ CreateFPExt → double 3.14 (exact)
```

For compile-time constants, the codegen extracts the value via `convertToDouble()` and creates a new `ConstantFP` directly.

### Float Narrowing (F64 to F32)

Narrowing from `F64` to `F32` uses `CreateFPTrunc` (floating-point truncation). This may lose precision — the value is rounded to the nearest representable `F32` value.

```
source: double 3.141592653589793
target: float
→ CreateFPTrunc → float 3.1415927 (rounded)
```

Narrowing requires the explicit `as` keyword. The compiler does not warn about precision loss — the cast is the programmer's explicit acknowledgment.

### Integer to Float

Converting an integer to a floating-point type uses `CreateSIToFP` (signed integer to floating point):

```
source: i64 42
target: double
→ CreateSIToFP → double 42.0
```

For compile-time constant integers, the conversion is folded: the codegen extracts the integer via `getSExtValue()`, casts to `double` in C++, and creates a `ConstantFP` directly.

Large integers may lose precision when converted to float. For example, `I64` values larger than 2^53 cannot be exactly represented as `F64` — the value is rounded to the nearest representable float.

### Float to Integer

Converting a floating-point value to an integer uses `CreateFPToSI` (floating point to signed integer). The fractional part is **truncated toward zero** (not rounded).

```
source: double 3.99
target: i64
→ CreateFPToSI → i64 3 (truncated, not rounded)

source: double -2.7
target: i64
→ CreateFPToSI → i64 -2 (truncated toward zero)
```

For compile-time constant floats, the conversion is folded via `convertToDouble()` and `static_cast<int64_t>`.

**Undefined behavior:** Converting `NaN`, `+Infinity`, or `-Infinity` to an integer produces undefined behavior. Converting a float value outside the target integer's range also produces undefined behavior.

### Implicit Store Coercion

When storing a float value to a variable of different float width, the codegen auto-coerces via `CreateFPCast`:

- Storing `float` to `double` variable → `CreateFPCast` (extends).
- Storing `double` to `float` variable → `CreateFPCast` (truncates).

Cross-domain coercion:

- Storing integer to float variable → `CreateSIToFP`.
- Storing float to integer variable → `CreateFPToSI`.

---

## Assignability Rules

### Float-to-Float Assignability

All float types are mutually assignable. The `isAssignable()` method returns `true` when both types are floating-point:

```
if( target->isFloatingPoint() && source->isFloatingPoint() ) → true
```

Width mismatches are resolved at the codegen level via `CreateFPExt` or `CreateFPTrunc`.

Additionally, for `Type::Kind::Float` specifically, the method checks:

```
if( targetFloatType->bitWidth >= sourceFloatType->bitWidth ) → true
```

### Integer-to-Float Assignability

Integers are assignable to floating-point types:

```
if( target->isFloatingPoint() && source->isIntegral() ) → true
if( target->kind == Type::Kind::Float && source->kind == Type::Kind::Integer ) → true
```

This allows implicit widening from any integer to any float.

### Float-to-Integer Assignability

Floats are assignable to integer types:

```
if( target->isIntegral() && source->isFloatingPoint() ) → true
```

This allows float-to-integer assignment, with truncation applied at the codegen level.

### OOP Wrapper Assignability

OOP wrapper classes for floats are cross-assignable:

```
if( targetIsFloatOop && sourceIsFloatOop ) → true
```

Integer OOP wrappers are assignable to float OOP wrappers:

```
if( targetIsFloatOop && sourceIsIntOop ) → true
```

---

## Builtin Methods

All builtin methods on float OOP wrappers are intercepted by the codegen and emitted as inline LLVM instructions or intrinsics. No actual function call is generated.

### Classification Methods

| Method | LLVM Implementation | Description |
|---|---|---|
| `isNaN()` | `CreateFCmpUNO(self, self)` | Returns `True` if value is NaN |
| `isInfinite()` | `fabs(self) == +Infinity` | Returns `True` if positive or negative infinity |
| `isFinite()` | `NOT(isInfinite OR isNaN)` | Returns `True` if neither NaN nor infinity |
| `isZero()` | `CreateFCmpOEQ(self, 0.0)` | Returns `True` if value equals zero |
| `isPositive()` | `CreateFCmpOGT(self, 0.0)` | Returns `True` if strictly greater than zero |
| `isNegative()` | `CreateFCmpOLT(self, 0.0)` | Returns `True` if strictly less than zero |

**`isNaN()` implementation detail:** Uses `CreateFCmpUNO(self, self)`, the unordered comparison predicate. IEEE 754 mandates that NaN is not equal to itself, so `UNO(x, x)` returns `true` only when `x` is NaN.

**`isInfinite()` implementation detail:** Computes `fabs(self)` via `llvm::Intrinsic::fabs`, creates `+Infinity` via `ConstantFP::getInfinity(primitiveType)`, then checks ordered equality. This catches both `+Infinity` and `-Infinity`.

**`isFinite()` implementation detail:** Computes `isInfinite OR isNaN`, then inverts with `CreateNot`. A value is finite if it is neither infinity nor NaN.

### Rounding Methods

| Method | LLVM Intrinsic | Description |
|---|---|---|
| `floor()` | `llvm::Intrinsic::floor` | Round toward negative infinity |
| `ceil()` | `llvm::Intrinsic::ceil` | Round toward positive infinity |
| `round()` | `llvm::Intrinsic::round` | Round to nearest, ties away from zero |

All three rounding methods emit `CreateUnaryIntrinsic` — a single LLVM intrinsic call that maps to a hardware instruction on modern architectures (e.g., `roundsd`/`roundss` on x86 with SSE4.1).

### Mathematical Methods

| Method | LLVM Intrinsic | Description |
|---|---|---|
| `sqrt()` | `llvm::Intrinsic::sqrt` | Square root |
| `power(exponent)` | `llvm::Intrinsic::pow` | Raise to power |
| `abs()` | `CreateFNeg` + `CreateSelect` | Absolute value |

**`abs()` implementation detail:** Does not use `llvm::Intrinsic::fabs`. Instead:
1. Compute `negValue = CreateFNeg(self)`.
2. Compute `isNeg = CreateFCmpOLT(self, 0.0)`.
3. `CreateSelect(isNeg, negValue, self)` — selects the negated value if negative.

**`sqrt()` on negative values:** Returns `NaN`, following IEEE 754.

**`power()` implementation detail:** Uses `CreateBinaryIntrinsic(llvm::Intrinsic::pow, self, exponent)`. Both operands must be floating-point.

### Arithmetic Methods

| Method | LLVM Implementation | Description |
|---|---|---|
| `add(other)` | `CreateFAdd(self, other)` | Addition |
| `subtract(other)` | `CreateFSub(self, other)` | Subtraction |
| `multiply(other)` | `CreateFMul(self, other)` | Multiplication |
| `divide(other)` | `CreateFDiv(self, other)` | Division |
| `negate()` | `CreateFNeg(self)` | Sign inversion |

These method-based arithmetic operations emit the same LLVM instructions as the infix operators.

### Comparison Methods

| Method | LLVM Implementation | Description |
|---|---|---|
| `equals(other)` | `CreateFCmpOEQ(self, other)` | Ordered equality |
| `compareTo(other)` | `gt - lt` via `FCmpOLT` + `FCmpOGT` | Returns -1, 0, or 1 |
| `greaterThan(other)` | `CreateFCmpOGT(self, other)` | Ordered greater than |
| `lessThan(other)` | `CreateFCmpOLT(self, other)` | Ordered less than |
| `greaterOrEqual(other)` | `CreateFCmpOGE(self, other)` | Ordered greater or equal |
| `lessOrEqual(other)` | `CreateFCmpOLE(self, other)` | Ordered less or equal |
| `min(other)` | `CreateFCmpOLT` + `CreateSelect` | Smaller of two values |
| `max(other)` | `CreateFCmpOGT` + `CreateSelect` | Larger of two values |

**`compareTo()` implementation detail:** Computes two boolean flags `isLt = FCmpOLT(self, other)` and `isGt = FCmpOGT(self, other)`, zero-extends both to `i64`, then subtracts: `gt - lt`. This produces -1 (less), 0 (equal or NaN), or 1 (greater).

### Conversion Methods

| Method | LLVM Implementation | Description |
|---|---|---|
| `toString()` | `snprintf`-style conversion | String representation |
| `toInt()` | `CreateFPToSI(self, i64)` | Truncate to 64-bit signed integer |
| `toFloat()` | `CreateSIToFP(self, double)` | Convert to `F64` (from integer wrappers) |
| `toDouble()` | `CreateSIToFP(self, double)` | Convert to `F64` (from integer wrappers) |
| `toF32()` | `value as F32` | Narrow to 32-bit (on F32 wrapper) |
| `toF64()` | identity return | Return underlying value (on F64 wrapper) |

---

## The OOP Wrapper Hierarchy

### Float Base Class

The `Float` class in `stdlibs/language/float.urn` is the base class for all floating-point wrappers:

```
class Float
    protect F64 value
```

It provides the full method set: `getValue()`, `toString()`, `abs()`, `negate()`, `add()`, `subtract()`, `multiply()`, `divide()`, `equals()`, `compareTo()`, `isNaN()`, `isInfinite()`, `floor()`, `ceil()`, `round()`, `toInt()`.

**`isNaN()` in stdlib:** Detects NaN via self-inequality: `return self.value != self.value`. This mirrors the IEEE 754 property that NaN is the only value not equal to itself. The codegen intercepts this call and emits `CreateFCmpUNO(self, self)` instead.

**`isInfinite()` in stdlib:** Uses the expression `self.value == self.value and (self.value - self.value) != 0.0`. If the value equals itself (not NaN) but subtracting it from itself does not produce zero (only infinity has this property), it is infinite. The codegen intercepts and emits the `fabs` + infinity comparison instead.

### F32 Wrapper

```
final class F32 extends Float
    constructor widens: self.value = value as F64
    toF32(): return self.value as F32
```

`F32` stores its value as `F64` internally. Narrowing back requires explicit `toF32()` or `as F32`.

### F64 Wrapper

```
final class F64 extends Float
    constructor: self.value = value (no conversion)
    toF64(): return self.value
```

### Double Wrapper

```
final class Double extends Float
    constructor: self.value = value (takes F64 parameter)
    toDouble(): return self.value
```

`Double` is functionally identical to `F64` — both are 64-bit and resolve to the same `FloatType(64)` in the type registry.

---

## Examples

### Declarations and Literals

```uranite
F32 temperature = 98.6
F64 pi = 3.141592653589793
Float gravity = 9.80665
Double lightSpeed = 299792458.0

F64 scientific = 6.022e23
F64 small = 1.6e-19
F64 negative = -273.15
```

### Special Value Handling

```uranite
F64 positiveInfinity = 1.0 / 0.0
F64 negativeInfinity = -1.0 / 0.0
F64 notANumber = 0.0 / 0.0

Boolean checkNaN = notANumber.isNaN()
Boolean checkInf = positiveInfinity.isInfinite()
Boolean checkFinite = pi.isFinite()

F64 nanPropagation = notANumber + 42.0
Boolean nanComparison = notANumber == notANumber

F64 infArithmetic = positiveInfinity + 1.0
F64 infSubtract = positiveInfinity - positiveInfinity
F64 infMultZero = positiveInfinity * 0.0
```

### Rounding and Math

```uranite
F64 value = 3.7

F64 floored = value.floor()
F64 ceiled = value.ceil()
F64 rounded = value.round()

F64 squareRoot = 144.0.sqrt()
F64 cubed = 2.0.power( 3.0 )
F64 absolute = (-42.5).abs()

Boolean positive = value.isPositive()
Boolean zero = 0.0.isZero()
Boolean negative = (-1.0).isNegative()
```

### Casting Between Types

```uranite
F32 single = 3.14
F64 promoted = single as F64

F64 precise = 3.141592653589793
F32 demoted = precise as F32

I64 integer = 42
F64 floated = integer as F64

F64 piValue = 3.14159
I64 truncated = piValue as I64

I32 smallInt = 100
F32 smallFloat = smallInt as F32
```

### Practical Usage

```uranite
from uranite.io.console import puts

public function distanceBetween( F64 x1, F64 y1, F64 x2, F64 y2 ) -> F64:
    F64 deltaX = x2 - x1
    F64 deltaY = y2 - y1
    F64 sumOfSquares = deltaX * deltaX + deltaY * deltaY
    return sumOfSquares.sqrt()

public function celsiusToFahrenheit( F64 celsius ) -> F64:
    return celsius * 1.8 + 32.0

public function safeDivide( F64 numerator, F64 denominator ) -> F64:
    if denominator.isZero():
        return 0.0
    F64 result = numerator / denominator
    if result.isNaN() or result.isInfinite():
        return 0.0
    return result

public function roundToDecimalPlaces( F64 value, I64 places ) -> F64:
    F64 factor = 10.0.power( places as F64 )
    F64 shifted = value * factor
    F64 rounded = shifted.round()
    return rounded / factor

public function main() -> I32:
    F64 distance = distanceBetween( 0.0, 0.0, 3.0, 4.0 )
    puts( distance.toString() )

    F64 tempF = celsiusToFahrenheit( 100.0 )
    puts( tempF.toString() )

    F64 safe = safeDivide( 10.0, 3.0 )
    puts( safe.toString() )

    F64 rounded = roundToDecimalPlaces( 3.14159, 2 )
    puts( rounded.toString() )

    F64 hypotenuse = (3.0 * 3.0 + 4.0 * 4.0).sqrt()
    puts( hypotenuse.toString() )

    return 0
```

This example demonstrates distance calculation using `sqrt()`, temperature conversion with float arithmetic, safe division with `isZero()`/`isNaN()`/`isInfinite()` guards, rounding to decimal places via `round()` and `power()`, and compile-time constant folding for literal arithmetic — all compiled to native LLVM float instructions and intrinsics with zero OOP wrapper overhead.
