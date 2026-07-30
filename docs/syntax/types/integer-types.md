# Integer Types

Uranite provides 12 integer types spanning 4 bit widths and 2 signedness categories. Every integer literal defaults to `I64` (signed 64-bit). Smaller or unsigned types require explicit type annotation. All integers follow two's complement representation and map directly to LLVM integer types with no runtime boxing or heap allocation.

This document covers the complete signed and unsigned integer inventory, LLVM representation and memory layout, arithmetic and overflow semantics, bitwise operations, type coercion during mixed-width arithmetic, explicit casting via the `as` keyword, and the assignability rules that govern implicit conversions.

---

## Table of Contents

- [Signed Integer Types](#signed-integer-types)
  - [I8](#i8)
  - [I16](#i16)
  - [I32](#i32)
  - [I64](#i64)
  - [Signed Aliases](#signed-aliases)
- [Unsigned Integer Types](#unsigned-integer-types)
  - [U8](#u8)
  - [U16](#u16)
  - [U32](#u32)
  - [U64](#u64)
  - [Unsigned Aliases](#unsigned-aliases)
- [LLVM Representation](#llvm-representation)
  - [Type Mapping](#type-mapping)
  - [The IntegerType Struct](#the-integertype-struct)
  - [Memory Layout and Alignment](#memory-layout-and-alignment)
- [Value Ranges](#value-ranges)
  - [Signed Ranges](#signed-ranges)
  - [Unsigned Ranges](#unsigned-ranges)
- [Arithmetic Semantics](#arithmetic-semantics)
  - [Basic Arithmetic Operations](#basic-arithmetic-operations)
  - [Wrapping Overflow](#wrapping-overflow)
  - [Division and Modulo](#division-and-modulo)
  - [Mixed-Width Operand Coercion](#mixed-width-operand-coercion)
- [Bitwise Operations](#bitwise-operations)
  - [Binary Bitwise Operators](#binary-bitwise-operators)
  - [Shift Operations](#shift-operations)
  - [Bitwise NOT](#bitwise-not)
- [Casting and Type Promotion](#casting-and-type-promotion)
  - [The as Keyword](#the-as-keyword)
  - [Widening (Sign Extension)](#widening-sign-extension)
  - [Narrowing (Truncation)](#narrowing-truncation)
  - [Integer-to-Float Conversion](#integer-to-float-conversion)
  - [Float-to-Integer Conversion](#float-to-integer-conversion)
  - [Implicit Store Coercion](#implicit-store-coercion)
- [Assignability Rules](#assignability-rules)
  - [Integer-to-Integer Assignability](#integer-to-integer-assignability)
  - [Integer OOP Wrapper Assignability](#integer-oop-wrapper-assignability)
  - [Integer-to-Float Assignability](#integer-to-float-assignability)
  - [The isComparable Relation](#the-iscomparable-relation)
- [The OOP Wrapper Hierarchy](#the-oop-wrapper-hierarchy)
  - [Signed Wrapper Hierarchy](#signed-wrapper-hierarchy)
  - [Unsigned Wrapper Hierarchy](#unsigned-wrapper-hierarchy)
  - [Byte Wrapper](#byte-wrapper)
  - [Inherited Methods](#inherited-methods)
- [Examples](#examples)
  - [Declarations and Annotations](#declarations-and-annotations)
  - [Arithmetic and Overflow](#arithmetic-and-overflow)
  - [Bitwise Operations Examples](#bitwise-operations-examples)
  - [Casting Between Widths](#casting-between-widths)
  - [Practical Usage](#practical-usage)

---

## Signed Integer Types

### I8

8-bit signed integer. Range: -128 to 127. LLVM type: `i8`. OOP wrapper class: `final class I8 extends Int` in `stdlibs/language/i8.urn`. Qualified name: `uranite.language.i8.I8`.

### I16

16-bit signed integer. Range: -32,768 to 32,767. LLVM type: `i16`. OOP wrapper class: `final class I16 extends Int` in `stdlibs/language/i16.urn`. Qualified name: `uranite.language.i16.I16`.

### I32

32-bit signed integer. Range: -2,147,483,648 to 2,147,483,647. LLVM type: `i32`. OOP wrapper class: `final class I32 extends Int` in `stdlibs/language/i32.urn`. Qualified name: `uranite.language.i32.I32`.

The `I32` constructor widens its parameter to `I64` internally (`self.value = value as I64`) because all signed integer wrappers share the `Int` base class, which stores a `protect I64 value` field.

### I64

64-bit signed integer. Range: -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807. LLVM type: `i64`. OOP wrapper class: `final class I64 extends Int` in `stdlibs/language/i64.urn`. Qualified name: `uranite.language.i64.I64`.

This is the default integer type. All undecorated integer literals are typed as `I64`.

### Signed Aliases

| Alias | Resolves To | Qualified Name |
|---|---|---|
| `Int` | `I64` (64-bit signed) | `uranite.language.int.Int` |
| `Integer` | `I32` (32-bit signed) | `uranite.language.integer.Integer` |
| `Long` | `I64` (64-bit signed) | `uranite.language.long.Long` |

`Int` and `Long` both resolve to the same `IntegerType(64, true)` instance in the type registry. `Integer` resolves to `IntegerType(32, true)`.

---

## Unsigned Integer Types

### U8

8-bit unsigned integer. Range: 0 to 255. LLVM type: `i8`. OOP wrapper class: `final class U8` in `stdlibs/language/u8.urn`. Qualified name: `uranite.language.u8.U8`.

### U16

16-bit unsigned integer. Range: 0 to 65,535. LLVM type: `i16`. OOP wrapper class: `final class U16` in `stdlibs/language/u16.urn`. Qualified name: `uranite.language.u16.U16`.

### U32

32-bit unsigned integer. Range: 0 to 4,294,967,295. LLVM type: `i32`. OOP wrapper class: `final class U32` in `stdlibs/language/u32.urn`. Qualified name: `uranite.language.u32.U32`.

### U64

64-bit unsigned integer. Range: 0 to 18,446,744,073,709,551,615. LLVM type: `i64`. OOP wrapper class: `final class U64 extends UInt` in `stdlibs/language/u64.urn`. Qualified name: `uranite.language.u64.U64`.

### Unsigned Aliases

| Alias | Resolves To | Qualified Name |
|---|---|---|
| `UInt` | `U64` (64-bit unsigned) | `uranite.language.uint.UInt` |
| `Byte` | `U8` (8-bit unsigned) | `uranite.language.byte.Byte` |

`UInt` resolves to `IntegerType(64, false)`. `Byte` resolves to `IntegerType(8, false)` and provides additional bitwise methods not present on `UInt`.

---

## LLVM Representation

### Type Mapping

All integer types map to LLVM integer types of matching bit width. Signed and unsigned types of the same width produce the same LLVM type — signedness is tracked by the compiler's semantic layer, not by LLVM IR.

| Uranite Type | LLVM IR Type | Bit Width |
|---|---|---|
| `I8`, `U8`, `Byte` | `i8` | 8 |
| `I16`, `U16` | `i16` | 16 |
| `I32`, `U32`, `Integer` | `i32` | 32 |
| `I64`, `U64`, `Int`, `Long`, `UInt` | `i64` | 64 |

The `toLLVMType()` function in `MIRCodegen` maps `Type::Kind::Integer` using a switch on `bitWidth`:

```
bitWidth 8   → llvm::Type::getInt8Ty(context)
bitWidth 16  → llvm::Type::getInt16Ty(context)
bitWidth 32  → llvm::Type::getInt32Ty(context)
bitWidth 64  → llvm::Type::getInt64Ty(context)
default      → llvm::Type::getInt64Ty(context)
```

For OOP wrapper class types (e.g., `I64`, `UInt`), `toLLVMType()` recognizes qualified names and maps directly to the corresponding native LLVM integer type, bypassing struct pointer indirection.

### The IntegerType Struct

The compiler represents integer types internally using the `IntegerType` struct, defined in `src/uranite/semantic/typeref.hpp`:

```
struct IntegerType : Type
    int bitWidth       — number of bits (8, 16, 32, or 64)
    bool isSigned      — true for signed, false for unsigned
```

The constructor auto-generates the type name from signedness and width: `"i"` or `"u"` followed by the bit count. For example, `IntegerType(32, true)` creates a type named `"i32"`, and `IntegerType(16, false)` creates `"u16"`.

### Memory Layout and Alignment

Integer values occupy exactly their declared bit width in memory. LLVM handles alignment according to the target data layout, which on x86-64 Linux typically aligns integers to their natural boundary:

| Type | Storage Size | Typical Alignment |
|---|---|---|
| `I8` / `U8` | 1 byte | 1 byte |
| `I16` / `U16` | 2 bytes | 2 bytes |
| `I32` / `U32` | 4 bytes | 4 bytes |
| `I64` / `U64` | 8 bytes | 8 bytes |

Stack-allocated integers use `alloca` instructions. The LLVM backend respects the target triple's ABI conventions for register allocation and function parameter passing.

---

## Value Ranges

### Signed Ranges

Signed integers use two's complement representation. The range for an N-bit signed integer is -2^(N-1) to 2^(N-1) - 1.

| Type | Minimum | Maximum |
|---|---|---|
| `I8` | -128 | 127 |
| `I16` | -32,768 | 32,767 |
| `I32` | -2,147,483,648 | 2,147,483,647 |
| `I64` | -9,223,372,036,854,775,808 | 9,223,372,036,854,775,807 |

### Unsigned Ranges

Unsigned integers represent non-negative values only. The range for an N-bit unsigned integer is 0 to 2^N - 1.

| Type | Minimum | Maximum |
|---|---|---|
| `U8` | 0 | 255 |
| `U16` | 0 | 65,535 |
| `U32` | 0 | 4,294,967,295 |
| `U64` | 0 | 18,446,744,073,709,551,615 |

---

## Arithmetic Semantics

### Basic Arithmetic Operations

Integer arithmetic maps to dedicated MIR instruction kinds, which the codegen emits as LLVM IR instructions:

| Uranite Operator | MIR Instruction | LLVM IR Instruction | Label |
|---|---|---|---|
| `+` | `AddInteger` | `CreateAdd` | `"add.i"` |
| `-` | `SubtractInteger` | `CreateSub` | `"sub.i"` |
| `*` | `MultiplyInteger` | `CreateMul` | `"mul.i"` |
| `/` | `DivideInteger` | `CreateSDiv` | `"div.i"` |
| `%` | `ModuloInteger` | `CreateSRem` | `"mod.i"` |

### Wrapping Overflow

Uranite uses LLVM's default wrapping arithmetic for addition, subtraction, and multiplication. The `CreateAdd`, `CreateSub`, and `CreateMul` calls are emitted **without** the `nsw` (no signed wrap) or `nuw` (no unsigned wrap) flags. This means:

- Integer overflow wraps around silently using two's complement arithmetic.
- Adding 1 to `I8` value 127 produces -128.
- Subtracting 1 from `U8` value 0 produces 255.
- Multiplying two large `I64` values wraps without trapping.

This is a deliberate design choice: wrapping semantics match C behavior and produce the fastest possible machine code. No overflow traps are inserted, no exceptions are thrown, and no result promotion occurs.

The compiler does not insert `nsw`/`nuw` flags because these flags enable LLVM to assume overflow is undefined behavior, which would allow aggressive optimizations that silently break programs relying on wrapping. By omitting these flags, Uranite guarantees defined behavior on overflow.

### Division and Modulo

Integer division uses LLVM's `CreateSDiv` (signed division) and integer modulo uses `CreateSRem` (signed remainder). Both instructions produce signed results.

**Division by zero:** The current compiler does not insert explicit zero-divisor checks before division or modulo operations. Dividing by zero produces undefined behavior at the hardware level — on most architectures, this causes a SIGFPE signal that terminates the program. The CLAUDE.md notes that auto-inserted zero-checks are part of the design, but the MIR codegen currently emits raw `SDiv`/`SRem` without guards.

### Mixed-Width Operand Coercion

When two integer operands of different bit widths appear in an arithmetic expression, the codegen automatically widens the narrower operand to match the wider one before performing the operation. This is done via sign extension (`CreateSExt`):

```
left  = i16 value
right = i64 value
→ left is sign-extended to i64 via CreateSExt
→ arithmetic proceeds on two i64 operands
```

The coercion logic checks:

1. If both operands are integers with different bit widths, compare `leftBits` and `rightBits`.
2. If `leftBits < rightBits`, sign-extend the left operand to the right operand's type.
3. If `rightBits < leftBits`, sign-extend the right operand to the left operand's type.

This coercion also handles pointer operands (converted to `i64` via `CreatePtrToInt`) and float-to-integer mismatches (converted via `CreateFPToSI`). The result type of the operation matches the wider operand's type.

---

## Bitwise Operations

### Binary Bitwise Operators

Bitwise operations apply to integer operands and follow the same mixed-width coercion rules as arithmetic — the narrower operand is sign-extended to match the wider one before the operation.

| Uranite Operator | MIR Instruction | LLVM IR Instruction | Label |
|---|---|---|---|
| `&` | `BitwiseAnd` | `CreateAnd` | `"band"` |
| `\|` | `BitwiseOr` | `CreateOr` | `"bor"` |
| `^` | `BitwiseXor` | `CreateXor` | `"bxor"` |

### Shift Operations

| Uranite Operator | MIR Instruction | LLVM IR Instruction | Label |
|---|---|---|---|
| `<<` | `ShiftLeft` | `CreateShl` | `"shl"` |
| `>>` | `ShiftRight` | `CreateAShr` | `"shr"` |

Right shift uses `CreateAShr` (arithmetic shift right), which preserves the sign bit. This means negative values remain negative after a right shift — the vacated high bits are filled with copies of the sign bit, not zeros. This is consistent with signed-integer semantics.

Left shift uses `CreateShl`, which shifts bits left and fills the vacated low bits with zeros.

### Bitwise NOT

The unary bitwise NOT operator (`~`) inverts all bits of its operand. It maps to `CreateNot` in LLVM IR, which computes `XOR operand, -1`. If the operand is a pointer, it is first converted to `i64` via `CreatePtrToInt`.

---

## Casting and Type Promotion

### The as Keyword

Explicit type conversion uses the `as` keyword. The semantic analyzer validates that the source and target types are compatible, and the codegen emits the appropriate LLVM instruction.

```uranite
I64 wide = 42
I32 narrow = wide as I32
I8 tiny = narrow as I8
F64 floating = wide as F64
```

The `generateCastType()` function in `MIRCodegen` handles all cast combinations based on the source and target LLVM types.

### Widening (Sign Extension)

When casting from a smaller integer to a larger integer, the codegen emits `CreateSExt` (sign extension). This preserves the numeric value by replicating the sign bit into the new high bits.

```
source: i8 value -5  (binary: 11111011)
target: i32
→ CreateSExt → i32 value -5 (binary: 11111111 11111111 11111111 11111011)
```

| Source | Target | Instruction |
|---|---|---|
| `I8` → `I16` | 8-bit → 16-bit | `CreateSExt` |
| `I8` → `I32` | 8-bit → 32-bit | `CreateSExt` |
| `I8` → `I64` | 8-bit → 64-bit | `CreateSExt` |
| `I16` → `I32` | 16-bit → 32-bit | `CreateSExt` |
| `I16` → `I64` | 16-bit → 64-bit | `CreateSExt` |
| `I32` → `I64` | 32-bit → 64-bit | `CreateSExt` |

Widening is always safe — no information is lost.

### Narrowing (Truncation)

When casting from a larger integer to a smaller integer, the codegen emits `CreateTrunc` (truncation). This discards the high bits, which may change the numeric value.

```
source: i64 value 300
target: i8
→ CreateTrunc → i8 value 44 (300 mod 256 = 44)
```

| Source | Target | Instruction |
|---|---|---|
| `I64` → `I32` | 64-bit → 32-bit | `CreateTrunc` |
| `I64` → `I16` | 64-bit → 16-bit | `CreateTrunc` |
| `I64` → `I8` | 64-bit → 8-bit | `CreateTrunc` |
| `I32` → `I16` | 32-bit → 16-bit | `CreateTrunc` |
| `I32` → `I8` | 32-bit → 8-bit | `CreateTrunc` |
| `I16` → `I8` | 16-bit → 8-bit | `CreateTrunc` |

Narrowing requires the explicit `as` keyword. The compiler does not insert range-check guards — if the value exceeds the target type's range, the high bits are silently discarded.

### Integer-to-Float Conversion

Casting an integer to a floating-point type uses `CreateSIToFP` (signed integer to floating point). For compile-time constants, the conversion is folded directly.

```
source: i64 value 42
target: double
→ CreateSIToFP → double 42.0
```

For constant integers, the codegen extracts the value via `getSExtValue()`, casts to `double` in C++, and creates a `ConstantFP` directly — avoiding a runtime instruction entirely.

### Float-to-Integer Conversion

Casting a floating-point value to an integer uses `CreateFPToSI` (floating point to signed integer). The fractional part is truncated toward zero.

```
source: double 3.99
target: i64
→ CreateFPToSI → i64 value 3
```

For constant floats, the conversion is folded at compile time via `convertToDouble()` and a C++ `static_cast<int64_t>`.

### Implicit Store Coercion

When storing a value to a variable whose type differs from the value's type, the codegen automatically coerces the value. This happens at the `StoreVariable` instruction level:

- Integer-to-integer with different widths: `CreateSExt` (widening) or `CreateTrunc` (narrowing).
- Integer-to-float: `CreateSIToFP`.
- Float-to-integer: `CreateFPToSI`.
- Pointer-to-pointer: `CreateBitCast`.
- Pointer-to-integer: `CreatePtrToInt`.
- Integer-to-pointer: `CreateIntToPtr`.

This means storing an `I8` value into an `I64` variable automatically sign-extends, and storing an `I64` value into an `I8` variable automatically truncates — no explicit `as` cast required at the store site.

---

## Assignability Rules

### Integer-to-Integer Assignability

The `Registry::isAssignable()` method treats all integer types as mutually assignable. If both the target and source have `Type::Kind::Integer`, the method returns `true` unconditionally:

```
if( target->kind == Type::Kind::Integer && source->kind == Type::Kind::Integer ) {
    return true;
}
```

This means the semantic analyzer permits assigning any integer type to any other integer type without requiring an explicit cast. Width mismatches are resolved at the codegen level via automatic sign extension or truncation.

### Integer OOP Wrapper Assignability

OOP wrapper classes for integers are also cross-assignable. The `isAssignable()` method checks if both target and source qualified names are in the `integerOopQualified` set:

```
if( targetIsIntOop && sourceIsIntOop ) → true
```

This covers all combinations of `Int`, `I8`, `I16`, `I32`, `I64`, `Integer`, `Long`, `Byte`, `UInt`, `U8`, `U16`, `U32`, `U64`.

### Integer-to-Float Assignability

Integers are assignable to floating-point types (both primitive and OOP wrapper):

```
if( target->isFloatingPoint() && source->isIntegral() ) → true
```

This also works for integer OOP to float OOP:

```
if( targetIsFloatOop && sourceIsIntOop ) → true
```

The reverse (float-to-integer) is also permitted, allowing explicit truncation.

### The isComparable Relation

The `isComparable()` method determines whether two types can appear in comparison expressions (`==`, `<`, `>`, etc.). For integers, comparability is guaranteed by the numeric check:

```
if( x->isNumeric() && y->isNumeric() ) → true
```

Since `isNumeric()` returns `true` for both `isIntegral()` and `isFloatingPoint()`, any integer can be compared with any other integer or any floating-point type.

---

## The OOP Wrapper Hierarchy

### Signed Wrapper Hierarchy

All signed integer wrapper classes extend the `Int` base class:

```
Int (base class)
├── I8 extends Int
├── I16 extends Int
├── I32 extends Int
└── I64 extends Int
```

The `Int` base class declares a `protect I64 value` field. All signed integer wrappers store their value in this shared 64-bit field, with narrower types widening upon construction (e.g., `I32` constructor performs `self.value = value as I64`).

### Unsigned Wrapper Hierarchy

All unsigned integer wrapper classes extend the `UInt` base class:

```
UInt (base class)
├── U8 extends UInt
├── U16 extends UInt
├── U32 extends UInt
└── U64 extends UInt
```

The `UInt` base class declares a `protect U64 value` field. It provides arithmetic methods (`add`, `subtract`, `multiply`, `divide`, `modulo`), comparison methods (`equals`, `compareTo`), and bitwise methods (`bitwiseAnd`, `bitwiseOr`, `bitwiseXor`) that return `UInt` instances.

### Byte Wrapper

The `Byte` class is a standalone `final class` (does not extend `UInt`). It wraps a `protect U8 value` field and provides:

- `getValue()` — returns the underlying `U8` value
- `toString()` — string representation
- `toInt()` — converts to `I32` via `self.value as I32`
- `bitwiseAnd(other)` — bitwise AND with another `Byte`
- `bitwiseOr(other)` — bitwise OR with another `Byte`
- `bitwiseXor(other)` — bitwise XOR with another `Byte`

### Inherited Methods

**From Int (signed wrappers):**

| Method | Signature | Description |
|---|---|---|
| `getValue()` | `() -> I64` | Return underlying value |
| `toString()` | `() -> String` | String representation |
| `abs()` | `() -> Int` | Absolute value |
| `negate()` | `() -> Int` | Sign inversion |
| `add(other)` | `(Int) -> Int` | Addition |
| `subtract(other)` | `(Int) -> Int` | Subtraction |
| `multiply(other)` | `(Int) -> Int` | Multiplication |
| `divide(other)` | `(Int) -> Int` | Integer division |
| `modulo(other)` | `(Int) -> Int` | Remainder |
| `equals(other)` | `(Int) -> Boolean` | Equality check |
| `compareTo(other)` | `(Int) -> I32` | Returns -1, 0, or 1 |
| `min(other)` | `(Int) -> Int` | Smaller of two values |
| `max(other)` | `(Int) -> Int` | Larger of two values |

**From UInt (unsigned wrappers):**

| Method | Signature | Description |
|---|---|---|
| `getValue()` | `() -> U64` | Return underlying value |
| `toString()` | `() -> String` | String representation |
| `add(other)` | `(UInt) -> UInt` | Addition |
| `subtract(other)` | `(UInt) -> UInt` | Subtraction |
| `multiply(other)` | `(UInt) -> UInt` | Multiplication |
| `divide(other)` | `(UInt) -> UInt` | Integer division |
| `modulo(other)` | `(UInt) -> UInt` | Remainder |
| `equals(other)` | `(UInt) -> Boolean` | Equality check |
| `compareTo(other)` | `(UInt) -> I32` | Returns -1, 0, or 1 |
| `bitwiseAnd(other)` | `(UInt) -> UInt` | Bitwise AND |
| `bitwiseOr(other)` | `(UInt) -> UInt` | Bitwise OR |
| `bitwiseXor(other)` | `(UInt) -> UInt` | Bitwise XOR |

---

## Examples

### Declarations and Annotations

```uranite
I8 temperature = 25
I16 altitude = 8848
I32 population = 1400000000
I64 distance = 149597870700

U8 red = 255
U16 port = 8080
U32 ipAddress = 3232235777
U64 fileSize = 1099511627776

Int counter = 0
Integer index = 0
Long timestamp = 1722345600000
UInt mask = 0xFFFFFFFFFFFFFFFF
Byte flag = 0xFF
```

### Arithmetic and Overflow

```uranite
I8 maxByte = 127
I8 overflow = maxByte + 1

U8 minByte = 0
U8 underflow = minByte - 1

I64 largeProduct = 3000000000 * 3000000000

I64 quotient = 17 / 5
I64 remainder = 17 % 5

I64 negativeDiv = -17 / 5
I64 negativeRem = -17 % 5
```

### Bitwise Operations Examples

```uranite
I64 bitwiseAndResult = 0xFF00 & 0x0F0F
I64 bitwiseOrResult = 0xFF00 | 0x0F0F
I64 bitwiseXorResult = 0xFF00 ^ 0x0F0F
I64 bitwiseNotResult = ~0xFF

I64 shiftedLeft = 1 << 10
I64 shiftedRight = 1024 >> 3

U8 redChannel = 255
U8 greenChannel = 128
Byte redByte = new Byte( redChannel )
Byte greenByte = new Byte( greenChannel )
Byte combined = redByte.bitwiseOr( greenByte )
```

### Casting Between Widths

```uranite
I64 large = 1000
I32 medium = large as I32
I16 small = medium as I16
I8 tiny = small as I8

I8 narrow = 42
I16 wider = narrow as I16
I32 wider32 = narrow as I32
I64 widest = narrow as I64

I64 intValue = 42
F64 floatValue = intValue as F64

F64 pi = 3.14159
I64 truncated = pi as I64

U8 unsigned8 = 200
U64 unsigned64 = unsigned8 as U64
U8 backToSmall = unsigned64 as U8
```

### Practical Usage

```uranite
from uranite.io.console import puts

public function absoluteValue( I64 number ) -> I64:
    if number < 0:
        return -number
    return number

public function isPowerOfTwo( U64 number ) -> Boolean:
    if number == 0:
        return False
    return (number & (number - 1)) == 0

public function countSetBits( U64 number ) -> I32:
    I32 count = 0
    U64 current = number
    while current > 0:
        if (current & 1) == 1:
            count = count + 1
        current = current >> 1
    return count

public function clampToU8( I64 value ) -> U8:
    if value < 0:
        return 0 as U8
    if value > 255:
        return 255 as U8
    return value as U8

public function extractByte( U32 word, I32 position ) -> U8:
    I32 shiftAmount = position * 8
    U32 shifted = word >> shiftAmount
    U32 masked = shifted & 0xFF
    return masked as U8

public function main() -> I32:
    I64 absResult = absoluteValue( -42 )
    puts( absResult.toString() )

    Boolean powerCheck = isPowerOfTwo( 64 )
    puts( powerCheck.toString() )

    I32 setBits = countSetBits( 0xFF )
    puts( setBits.toString() )

    U8 clamped = clampToU8( 300 )
    puts( clamped.toString() )

    U8 secondByte = extractByte( 0xDEADBEEF, 1 )
    puts( secondByte.toString() )

    return 0
```

This example demonstrates signed arithmetic with `absoluteValue()`, bitwise power-of-two detection, bit counting via shift-and-mask loop, safe narrowing with clamping, and byte extraction from a 32-bit word — all compiled to direct LLVM integer instructions with zero OOP wrapper overhead.
