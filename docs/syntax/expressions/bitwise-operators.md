# Bitwise Operators

Uranite provides six bitwise operators: AND (`&`), OR (`|`), XOR (`^`), NOT (`~`), left shift (`<<`), and right shift (`>>`). All bitwise operators require integer operands and produce integer results. The semantic analyzer enforces integer-only operands via `isIntegral()` checks. Binary bitwise operators parse at precedences 5 (`&`), 4 (`^`), and 3 (`|`), while shifts parse at precedence 8 — all as `BinaryExpression` nodes. Bitwise NOT (`~`) parses as a prefix `UnaryExpression`. At MIR level, they map to `BitwiseAnd`, `BitwiseOr`, `BitwiseXor`, `BitwiseNot`, `ShiftLeft`, and `ShiftRight` instructions. At LLVM codegen, `generateBitwise` emits the corresponding LLVM bitwise intrinsics with automatic pointer-to-integer conversion, integer width sign-extension for mismatched operand sizes, and arithmetic right shift for `>>`.

---

## Table of Contents

- [Operator Reference](#operator-reference)
- [Precedence](#precedence)
- [Syntax](#syntax)
- [Semantic Analysis](#semantic-analysis)
- [MIR Lowering](#mir-lowering)
- [MIR Codegen — generateBitwise](#mir-codegen--generatebitwise)
  - [BitwiseNot Codegen](#bitwisenot-codegen)
  - [Binary Bitwise Codegen](#binary-bitwise-codegen)
  - [Type Coercion](#type-coercion)
- [Shift Operators](#shift-operators)
- [Examples](#examples)

---

## Operator Reference

| Operator | Operation | Token Type | MIR Instruction | Precedence |
|---|---|---|---|---|
| `&` | Bitwise AND | `Ampersand` | `BitwiseAnd` | 5 |
| `\|` | Bitwise OR | `Pipe` | `BitwiseOr` | 3 |
| `^` | Bitwise XOR | `Caret` | `BitwiseXor` | 4 |
| `~` | Bitwise NOT | `Tilde` | `BitwiseNot` | Prefix unary |
| `<<` | Left shift | `ShiftLeft` | `ShiftLeft` | 8 |
| `>>` | Right shift | `ShiftRight` | `ShiftRight` | 8 |

---

## Precedence

From `getOperatorPrecedenceValue` at `src/uranite/parser/parser.cpp:111-148`:

```cpp
case token::Type::Ampersand:
    return 5;
case token::Type::Caret:
    return 4;
case token::Type::Pipe:
    return 3;
case token::Type::ShiftLeft:
case token::Type::ShiftRight:
    return 8;
```

| Precedence | Operators | Category |
|---|---|---|
| 8 | `<<`, `>>` | Shift |
| 5 | `&` | Bitwise AND |
| 4 | `^` | Bitwise XOR |
| 3 | `\|` | Bitwise OR |

Shifts bind tighter than all other bitwise operators. `&` binds tighter than `^`, which binds tighter than `|`. This follows C-family convention.

Expression `a | b & c ^ d` parses as `a | ((b & c) ^ d)`.

Expression `a << 2 & mask` parses as `(a << 2) & mask`.

Bitwise NOT (`~`) is a prefix unary parsed by `parseUnaryExpression` — binds tighter than any binary operator.

---

## Syntax

```
I64 masked = value & 0xFF
I64 combined = flagA | flagB
I64 toggled = flags ^ mask
I64 inverted = ~value
I64 shifted = value << 4
I64 extracted = value >> 8
```

---

## Semantic Analysis

### Binary Bitwise — &, |, ^, <<, >>

At `src/uranite/semantic/analyzer.cpp:1132-1141`:

```cpp
case token::Type::Ampersand:
case token::Type::Caret:
case token::Type::Pipe:
case token::Type::ShiftLeft:
case token::Type::ShiftRight: {
    if( leftSideType->isIntegral() &&
        rightSideType->isIntegral() ) {
        return leftSideType;
    }
    this->diagnostic.error( expression.source,
        "bitwise operators require integer operands" );
    return this->typeRegistry.getError();
}
```

Both operands must pass `isIntegral()` — primitive integer types (`I8`, `I16`, `I32`, `I64`, `U8`, `U16`, `U32`, `U64`) and integer-kinded types. Float, string, boolean, class, and other non-integer types produce an error.

Result type is the **left operand type**. No automatic widening to `I64` — if both operands are `I16`, result is `I16`.

### Unary Bitwise — ~

At `src/uranite/semantic/analyzer.cpp:3982-3988`:

```cpp
case token::Type::Tilde: {
    if( operandType->isIntegral() == false ) {
        this->diagnostic.error( expression.source,
            "bitwise not requires integer operand" );
        return this->typeRegistry.getError();
    }
    return operandType;
}
```

Operand must be an integer type. Result type matches operand type.

---

## MIR Lowering

### Binary Bitwise

At `src/uranite/ir/mir/lowering.cpp:3355-3359`:

```cpp
case token::Type::Ampersand:
    instructionKind = MIRInstructionKind::BitwiseAnd;
    break;
case token::Type::Pipe:
    instructionKind = MIRInstructionKind::BitwiseOr;
    break;
case token::Type::Caret:
    instructionKind = MIRInstructionKind::BitwiseXor;
    break;
case token::Type::ShiftLeft:
    instructionKind = MIRInstructionKind::ShiftLeft;
    break;
case token::Type::ShiftRight:
    instructionKind = MIRInstructionKind::ShiftRight;
    break;
```

Standard binary instruction pattern: two source operands, `_binop` destination variable.

### Unary Bitwise

At `src/uranite/ir/mir/lowering.cpp:3427`:

```cpp
case token::Type::Tilde:
    instructionKind = MIRInstructionKind::BitwiseNot;
    break;
```

Unary instruction: one source operand, one destination variable.

---

## MIR Codegen — generateBitwise

At `src/uranite/ir/mir/codegen.cpp:2340-2410`, `generateBitwise` handles all bitwise instructions.

### BitwiseNot Codegen

At `codegen.cpp:2344-2359`:

```cpp
if( instruction.instructionKind ==
    MIRInstructionKind::BitwiseNot ) {
    llvm::Value* operand = this->loadVariableValue(
        instruction.sourceOperands[0] );
    if( operand->getType()->isPointerTy() ) {
        operand = this->irBuilder.CreatePtrToInt(
            operand,
            llvm::Type::getInt64Ty( this->llvmContext ),
            "bnot.ptoi" );
    }
    llvm::Value* result =
        this->irBuilder.CreateNot( operand, "bnot" );
    this->setVariableValue(
        instruction.destinationVariable, result );
    return;
}
```

Pointer operands are first converted to `i64` via `CreatePtrToInt` before applying bitwise NOT. This enables `~ptr` patterns in unsafe code. For integer operands, `CreateNot` directly flips all bits (XOR with all-ones).

### Binary Bitwise Codegen

At `codegen.cpp:2388-2407`:

```cpp
switch( instruction.instructionKind ) {
    case MIRInstructionKind::BitwiseAnd:
        result = this->irBuilder.CreateAnd(
            leftOperand, rightOperand, "band" );
        break;
    case MIRInstructionKind::BitwiseOr:
        result = this->irBuilder.CreateOr(
            leftOperand, rightOperand, "bor" );
        break;
    case MIRInstructionKind::BitwiseXor:
        result = this->irBuilder.CreateXor(
            leftOperand, rightOperand, "bxor" );
        break;
    case MIRInstructionKind::ShiftLeft:
        result = this->irBuilder.CreateShl(
            leftOperand, rightOperand, "shl" );
        break;
    case MIRInstructionKind::ShiftRight:
        result = this->irBuilder.CreateAShr(
            leftOperand, rightOperand, "shr" );
        break;
}
```

| MIR Instruction | LLVM Builder | LLVM IR | Description |
|---|---|---|---|
| `BitwiseAnd` | `CreateAnd` | `and` | Bitwise AND |
| `BitwiseOr` | `CreateOr` | `or` | Bitwise OR |
| `BitwiseXor` | `CreateXor` | `xor` | Bitwise XOR |
| `ShiftLeft` | `CreateShl` | `shl` | Logical left shift |
| `ShiftRight` | `CreateAShr` | `ashr` | **Arithmetic** right shift |

Right shift uses `CreateAShr` (arithmetic shift right) — the sign bit is preserved. This means negative values remain negative after right shift: `-8 >> 1` produces `-4`, not a large positive number. There is no unsigned/logical right shift operator.

### Type Coercion

Before binary bitwise operations, type mismatches are resolved at `codegen.cpp:2369-2386`:

1. **Pointer to integer**: Pointer operands are converted to `i64` via `CreatePtrToInt`. This enables bitwise operations on raw pointers in unsafe code.

2. **Integer width mismatch**: If both operands are integers with different bit widths, the narrower operand is sign-extended (`CreateSExt`) to match the wider operand.

```cpp
llvm::Type* i64Ty =
    llvm::Type::getInt64Ty( this->llvmContext );
if( leftOperand->getType()->isPointerTy() ) {
    leftOperand = this->irBuilder.CreatePtrToInt(
        leftOperand, i64Ty, "bw.ptoi" );
}
if( rightOperand->getType()->isPointerTy() ) {
    rightOperand = this->irBuilder.CreatePtrToInt(
        rightOperand, i64Ty, "bw.ptoi" );
}
if( leftOperand->getType() !=
    rightOperand->getType() ) {
    if( leftOperand->getType()->isIntegerTy() &&
        rightOperand->getType()->isIntegerTy() ) {
        unsigned leftBits =
            leftOperand->getType()
                ->getIntegerBitWidth();
        unsigned rightBits =
            rightOperand->getType()
                ->getIntegerBitWidth();
        if( leftBits < rightBits ) {
            leftOperand = this->irBuilder.CreateSExt(
                leftOperand, rightOperand->getType(),
                "bw.sext" );
        }
        else {
            rightOperand = this->irBuilder.CreateSExt(
                rightOperand, leftOperand->getType(),
                "bw.sext" );
        }
    }
}
```

---

## Shift Operators

### Left Shift — <<

Left shift inserts zero bits on the right. `value << n` multiplies `value` by 2^n (for non-overflowing cases).

```
I64 result = 1 << 10
```

LLVM: `shl i64 1, 10` produces `1024`.

Shifting by a count >= bit width of the type is undefined behavior in LLVM. Uranite does not insert bounds checks on shift amounts.

### Right Shift — >>

Right shift uses **arithmetic** shift (`ashr`) — the sign bit is replicated. This preserves the sign of negative values.

```
I64 positive = 256 >> 4
I64 negative = -256 >> 4
```

- `256 >> 4` = `16` (zero-fill from left)
- `-256 >> 4` = `-16` (sign-bit-fill from left)

There is no unsigned/logical right shift operator (`>>>`) in Uranite.

---

## Examples

### Bit Masking

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 value = 0xDEADBEEF
    I64 lowByte = value & 0xFF
    I64 highNibble = (value >> 4) & 0x0F

    puts(lowByte.toString())
    puts(highNibble.toString())

    return 0
```

**Compilation trace**:
1. **Semantic analysis**: `value & 0xFF` — both `I64`, `isIntegral()` passes. Result: `I64`.
2. **MIR lowering**: `BitwiseAnd` with two source operands.
3. **LLVM codegen**: `and i64 %value, 255`.

### Flag Operations

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 READ = 1
    I64 WRITE = 2
    I64 EXECUTE = 4

    I64 permissions = READ | WRITE
    Boolean canExecute = (permissions & EXECUTE) != 0

    I64 allPermissions = READ | WRITE | EXECUTE
    I64 noWrite = allPermissions ^ WRITE

    puts(permissions.toString())
    puts(noWrite.toString())

    return 0
```

**Compilation trace**:
1. **Parsing**: `READ | WRITE` — `|` at precedence 3. `permissions & EXECUTE` — `&` at precedence 5 (tighter than `!=` at 6). Parses as `(permissions & EXECUTE) != 0`.
2. **MIR**: `BitwiseOr` for `|`, `BitwiseAnd` for `&`, `BitwiseXor` for `^`.
3. **LLVM**: `or i64`, `and i64`, `xor i64`.

### Bitwise NOT

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 mask = 0xFF
    I64 inverted = ~mask

    puts(inverted.toString())

    return 0
```

**Compilation trace**:
1. **Semantic analysis**: `~mask` — operand `I64`, `isIntegral()` passes. Result: `I64`.
2. **MIR lowering**: `BitwiseNot` with one source operand.
3. **LLVM codegen**: `CreateNot` emits `xor i64 %mask, -1` (XOR with all-ones).

### Shift Operations

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 value = 1

    I64 shifted = value << 10
    puts(shifted.toString())

    I64 large = 1024
    I64 halved = large >> 1
    puts(halved.toString())

    I64 negative = -1024
    I64 negHalved = negative >> 1
    puts(negHalved.toString())

    return 0
```

**Compilation trace**:
1. **MIR**: `ShiftLeft` and `ShiftRight` instructions.
2. **LLVM**: `shl i64 1, 10` = `1024`. `ashr i64 1024, 1` = `512`. `ashr i64 -1024, 1` = `-512` (arithmetic, sign-preserving).

### Error Case — Non-Integer Operands

```
package examples

public function main() -> I32:
    F64 value = 3.14
    I64 result = value & 0xFF
    return 0
```

**Diagnostic output**:
```
error: bitwise operators require integer operands
```

Float types are rejected by `isIntegral()`. Bitwise operators work exclusively on integer types.
