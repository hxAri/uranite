# Cast Expressions

A cast expression converts a value from one type to another using the `as` keyword. The syntax `expression as Type` produces a `CastExpression` AST node carrying the source expression and target type. Cast expressions have the highest precedence among binary operators (precedence 12, above `**` at 11), so `x + 1 as F64` parses as `x + (1 as F64)`. The semantic analyzer is lightweight — it analyzes the source expression, resolves the target type, and sets the expression type to the target without performing assignability checks. All conversion logic is deferred to LLVM code generation, where `generateCastType` dispatches across 10 type-pair conversion strategies: integer widening/narrowing via `SExt`/`Trunc`, integer-to-float via `SIToFP`, float-to-integer via `FPToSI`, float widening/narrowing via `FPExt`/`FPTrunc`, pointer-to-pointer via `BitCast`, pointer-to-integer via `PtrToInt`, integer-to-pointer via `IntToPtr`, and two composite paths for float-to-pointer and pointer-to-float that chain through an intermediate `i64`. Compile-time constant folding is applied when the source value is a known constant — `ConstantInt` and `ConstantFP` values are converted at compile time without emitting runtime instructions.

---

## Table of Contents

- [Syntax](#syntax)
- [Operator Precedence](#operator-precedence)
- [AST Representation — CastExpression](#ast-representation--castexpression)
- [Parsing](#parsing)
- [Semantic Analysis](#semantic-analysis)
- [HIR Representation — HIRCast](#hir-representation--hircast)
- [HIR Lowering](#hir-lowering)
- [MIR Lowering](#mir-lowering)
- [LLVM Code Generation — generateCastType](#llvm-code-generation--generatecasttype)
  - [Same Type — No-Op](#same-type--no-op)
  - [Integer to Integer](#integer-to-integer)
  - [Integer to Float](#integer-to-float)
  - [Float to Integer](#float-to-integer)
  - [Float to Float](#float-to-float)
  - [Pointer to Pointer](#pointer-to-pointer)
  - [Pointer to Integer](#pointer-to-integer)
  - [Integer to Pointer](#integer-to-pointer)
  - [Float to Pointer](#float-to-pointer)
  - [Pointer to Float](#pointer-to-float)
  - [Fallback — Identity](#fallback--identity)
- [Examples](#examples)

---

## Syntax

```
expression as TargetType
```

| Form | Description |
|---|---|
| `value as I32` | Cast to 32-bit integer |
| `value as F64` | Cast to 64-bit float |
| `value as Bool` | Cast to boolean |
| `pointer as I64` | Pointer to integer reinterpretation |
| `integer as Memory<I8>` | Integer to pointer reinterpretation |

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 integer = 42
    F64 floating = integer as F64
    puts(floating.toString())
    return 0
```

The `as` keyword is a binary operator that takes an expression on the left and a type on the right. Unlike function-style casts in other languages, `as` reads naturally left-to-right: `value as TargetType`.

---

## Operator Precedence

`as` has the highest binary operator precedence in Uranite (level 12), above all arithmetic, comparison, logical, and bitwise operators:

| Precedence | Operators |
|---|---|
| 12 | `as` (cast) |
| 11 | `**` (power) |
| 10 | `*`, `/`, `%` |
| 9 | `+`, `-` |
| 8 | `<<`, `>>` |
| 7 | `<`, `<=`, `>`, `>=`, `in`, `instanceof`, `is`, `subclassof` |
| 6 | `==`, `!=` |
| 5 | `&` (bitwise AND) |
| 4 | `^` (bitwise XOR) |
| 3 | `\|` (bitwise OR) |
| 2 | `and` |
| 1 | `or` |

This means cast binds tighter than any arithmetic or comparison:
- `x + 1 as F64` parses as `x + (1 as F64)` — the `1` is cast, then added to `x`.
- `x as I32 + y` parses as `(x as I32) + y` — `x` is cast, then added to `y`.
- `a as Bool and b` parses as `(a as Bool) and b`.

From `src/uranite/parser/parser.cpp:131-132`:

```cpp
case token::Type::KeywordAs:
    return 12;
```

---

## AST Representation — CastExpression

Defined at `src/uranite/ast/node.hpp:1289-1309`:

```cpp
struct CastExpression : Expression {

    ExpressionSharedPointer expression;
    TypeNodeSharedPointer targetType;

    CastExpression(
        ExpressionSharedPointer expression,
        TypeNodeSharedPointer targetType,
        const lookup::SourceSharedPointer& source
    ) : Expression( Node::Kind::CastExpression, source ),
        expression( std::move( expression ) ),
        targetType( std::move( targetType ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `expression` | `ExpressionSharedPointer` | Source expression being cast |
| `targetType` | `TypeNodeSharedPointer` | Target type node — `SimpleTypeNode` for plain types, `GenericTypeNode` for generic pointer types |

---

## Parsing

Cast expressions are parsed inside the precedence-climbing loop at `src/uranite/parser/parser.cpp:2154-2159`:

```cpp
if( kind == token::Type::KeywordAs ) {
    lookup::SourceSharedPointer source =
        this->current().source;
    this->advance();
    ast::nodes::TypeNodeSharedPointer type =
        this->parseTypeNode();
    left = std::make_shared<ast::nodes::CastExpression>(
        left, type, source );
    continue;
}
```

Parsing sequence:
1. The precedence-climbing loop reaches the `as` keyword with precedence 12.
2. The `as` token is consumed.
3. `parseTypeNode()` parses the target type — handles simple types (`I32`), generic types (`Memory<I8>`), and qualified types.
4. A `CastExpression` wraps the left-hand expression with the parsed target type.
5. The loop continues, allowing chained casts: `value as I32 as I64` parses as `(value as I32) as I64`.

Unlike binary arithmetic operators (which parse a right-hand expression via `parsePrecedenceExpression`), cast parses a **type** on the right side via `parseTypeNode`. This prevents ambiguity — `x as I64 + 1` cannot mean `x as (I64 + 1)` because `I64 + 1` is not a valid type.

---

## Semantic Analysis

At `src/uranite/semantic/analyzer.cpp:2138-2142`:

```cpp
case ast::Node::Kind::CastExpression: {
    ast::nodes::CastExpression& castExpression =
        static_cast<ast::nodes::CastExpression&>(
            *expression );
    this->analyzeExpression( castExpression.expression );
    expressionType =
        this->resolveType( castExpression.targetType );
    break;
}
```

Semantic analysis for casts is deliberately minimal:

1. **Analyze source expression** — ensures sub-expression is well-typed.
2. **Resolve target type** — looks up the target type in the type registry.
3. **Set expression type** to the resolved target type. No assignability check, no conversion validation.

Uranite treats `as` as an unchecked cast — the compiler trusts the programmer. Invalid conversions (e.g., casting a string to an integer) are not caught at compile time. At runtime, the LLVM codegen will produce whatever conversion is available for the source/target LLVM type pair, or fall back to a no-op identity pass-through.

This design matches Uranite's systems programming philosophy: `as` is a low-level type reinterpretation tool, not a safe conversion function. Safe conversions use constructor syntax (`new F64(intValue)`) or explicit method calls (`intValue.toFloat()`).

---

## HIR Representation — HIRCast

Defined at `src/uranite/ir/hir.hpp:762-776`:

```cpp
struct HIRCast : HIRNode {

    HIRNodeSharedPointer sourceExpression;
    semantic::TypeSharedPointer targetCastType;

    HIRCast(
        HIRNodeSharedPointer sourceExpression,
        semantic::TypeSharedPointer targetCastType,
        const lookup::SourceSharedPointer& sourceLocation
    ) : HIRNode( HIRNodeKind::Cast,
            targetCastType, sourceLocation ),
        sourceExpression( std::move( sourceExpression ) ),
        targetCastType( std::move( targetCastType ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `sourceExpression` | `HIRNodeSharedPointer` | Lowered source expression |
| `targetCastType` | `TypeSharedPointer` | Resolved semantic target type |

The `resolvedType` inherited from `HIRNode` is set to `targetCastType` — downstream consumers see the cast result as the target type.

---

## HIR Lowering

At `src/uranite/ir/hir/lowering.cpp:1034-1038`:

```cpp
case ast::Node::Kind::CastExpression: {
    ast::nodes::CastExpression& castExpression =
        static_cast<ast::nodes::CastExpression&>(
            *expression );
    HIRNodeSharedPointer sourceExpression =
        this->lowerExpression( castExpression.expression );
    semantic::TypeSharedPointer targetType =
        this->resolveTypeNode( castExpression.targetType );
    return std::make_shared<HIRCast>(
        std::move( sourceExpression ),
        targetType, expression->source );
}
```

Straightforward: lower source expression, resolve target type from AST type node to semantic type, create `HIRCast`. The `resolveTypeNode` call here performs the same type resolution as `resolveType` in semantic analysis — looking up the type in the registry.

---

## MIR Lowering

At `src/uranite/ir/mir/lowering.cpp:2423-2433`:

```cpp
case hir::HIRNodeKind::Cast: {
    hir::HIRCast& castNode =
        static_cast<hir::HIRCast&>( *hirExpression );
    MIRVariableIdentifier sourceVariable =
        this->lowerExpression( castNode.sourceExpression );
    MIRInstruction castInstruction(
        MIRInstructionKind::CastType );
    castInstruction.sourceOperands.push_back(
        sourceVariable );
    castInstruction.castTargetType =
        castNode.targetCastType;
    castInstruction.operandType = castNode.resolvedType;
    castInstruction.sourceLocation =
        castNode.sourceLocation;
    MIRVariableIdentifier resultVariable =
        this->currentFunction->allocateVariable(
            "_cast", castNode.resolvedType, false );
    castInstruction.destinationVariable = resultVariable;
    return this->emitInstruction( castInstruction );
}
```

MIR lowering produces a single `CastType` instruction:

| MIR Field | Value | Description |
|---|---|---|
| `instructionKind` | `CastType` | Cast instruction kind |
| `sourceOperands[0]` | Source variable | Value being cast |
| `castTargetType` | Target semantic type | Used by codegen to determine LLVM target type |
| `operandType` | Same as `castTargetType` | Resolved type from HIR |
| `destinationVariable` | `_cast` | Result variable with target type |

The `castTargetType` field on `MIRInstruction` is specific to cast instructions — it carries the semantic target type separately from `operandType` for codegen to resolve via `toLLVMType`.

---

## LLVM Code Generation — generateCastType

At `src/uranite/ir/mir/codegen.cpp:2413-2494`, `generateCastType` dispatches across 10 source-target type pair combinations. Each combination maps to a specific LLVM IR conversion instruction.

```cpp
void MIRCodegen::generateCastType(
    const MIRInstruction& instruction ) {
    if( instruction.destinationVariable ==
            INVALID_VARIABLE_IDENTIFIER ||
        instruction.sourceOperands.empty() ||
        instruction.castTargetType == nullptr ) {
        return;
    }
    llvm::Value* sourceValue = this->loadVariableValue(
        instruction.sourceOperands[0] );
    if( sourceValue == nullptr ) {
        return;
    }
    llvm::Type* targetType =
        this->toLLVMType( instruction.castTargetType );
    llvm::Type* sourceType = sourceValue->getType();
    llvm::Value* result = nullptr;
    // ... dispatch based on source/target type pairs
    if( result != nullptr ) {
        this->setVariableValue(
            instruction.destinationVariable, result );
    }
}
```

Guard checks: if destination variable is invalid, source operands are empty, or target type is null, the function returns immediately — no LLVM IR emitted.

### Same Type — No-Op

```cpp
if( sourceType == targetType ) {
    result = sourceValue;
}
```

When source and target LLVM types are identical, the value passes through unchanged. This handles redundant casts like `I64 as I64`.

### Integer to Integer

```cpp
else if( sourceType->isIntegerTy() &&
         targetType->isIntegerTy() ) {
    unsigned sourceBits =
        sourceType->getIntegerBitWidth();
    unsigned targetBits =
        targetType->getIntegerBitWidth();
    if( sourceBits < targetBits ) {
        result = this->irBuilder.CreateSExt(
            sourceValue, targetType, "sext" );
    }
    else {
        result = this->irBuilder.CreateTrunc(
            sourceValue, targetType, "trunc" );
    }
}
```

| Direction | LLVM Instruction | Description |
|---|---|---|
| Widening (`I8` → `I64`) | `SExt` (sign-extend) | Preserves sign bit, fills upper bits |
| Narrowing (`I64` → `I8`) | `Trunc` (truncate) | Drops upper bits, may lose data |

Sign extension is always used (never zero extension) — Uranite integers are signed. `I8(-1)` sign-extends to `I64(-1)`, not `I64(255)`.

### Integer to Float

```cpp
else if( sourceType->isIntegerTy() &&
         targetType->isFloatingPointTy() ) {
    if( llvm::ConstantInt* constInt =
            llvm::dyn_cast<llvm::ConstantInt>(
                sourceValue ) ) {
        double doubleValue =
            static_cast<double>(
                constInt->getSExtValue() );
        result = llvm::ConstantFP::get(
            targetType, doubleValue );
    }
    else {
        result = this->irBuilder.CreateSIToFP(
            sourceValue, targetType, "sitofp" );
    }
}
```

**Compile-time constant folding**: when the source is a `ConstantInt`, the conversion is performed at compile time — `getSExtValue()` extracts the signed value, casts to `double`, and creates a `ConstantFP`. No runtime instruction emitted.

**Runtime conversion**: `SIToFP` (signed integer to floating point). Converts the integer value to the nearest representable floating-point value.

### Float to Integer

```cpp
else if( sourceType->isFloatingPointTy() &&
         targetType->isIntegerTy() ) {
    if( llvm::ConstantFP* constFP =
            llvm::dyn_cast<llvm::ConstantFP>(
                sourceValue ) ) {
        int64_t intValue =
            static_cast<int64_t>(
                constFP->getValueAPF().convertToDouble() );
        result = llvm::ConstantInt::get(
            targetType, intValue, true );
    }
    else {
        result = this->irBuilder.CreateFPToSI(
            sourceValue, targetType, "fptosi" );
    }
}
```

**Compile-time constant folding**: `ConstantFP` values are converted at compile time via `convertToDouble()` then `static_cast<int64_t>`. Truncation toward zero (C semantics).

**Runtime conversion**: `FPToSI` (floating point to signed integer). Truncates toward zero. `3.7 as I64` produces `3`, `-2.9 as I64` produces `-2`.

### Float to Float

```cpp
else if( sourceType->isFloatingPointTy() &&
         targetType->isFloatingPointTy() ) {
    if( llvm::ConstantFP* constFP =
            llvm::dyn_cast<llvm::ConstantFP>(
                sourceValue ) ) {
        result = llvm::ConstantFP::get( targetType,
            constFP->getValueAPF().convertToDouble() );
    }
    else if( sourceType->getPrimitiveSizeInBits() <
             targetType->getPrimitiveSizeInBits() ) {
        result = this->irBuilder.CreateFPExt(
            sourceValue, targetType, "fpext" );
    }
    else {
        result = this->irBuilder.CreateFPTrunc(
            sourceValue, targetType, "fptrunc" );
    }
}
```

| Direction | LLVM Instruction | Description |
|---|---|---|
| Widening (`F32` → `F64`) | `FPExt` | Extends precision, no data loss |
| Narrowing (`F64` → `F32`) | `FPTrunc` | Reduces precision, may round |
| Constant | `ConstantFP::get` | Compile-time conversion |

### Pointer to Pointer

```cpp
else if( sourceType->isPointerTy() &&
         targetType->isPointerTy() ) {
    result = this->irBuilder.CreateBitCast(
        sourceValue, targetType, "bitcast" );
}
```

`BitCast` reinterprets the pointer without changing the bit pattern. Used for type-punning between pointer types: `Memory<I64>` to `Memory<I8>`, object pointers to opaque pointers.

### Pointer to Integer

```cpp
else if( sourceType->isPointerTy() &&
         targetType->isIntegerTy() ) {
    result = this->irBuilder.CreatePtrToInt(
        sourceValue, targetType, "ptrtoint" );
}
```

`PtrToInt` extracts the pointer's address as an integer. The target integer type determines width — `I64` captures the full 64-bit address, `I32` truncates it.

### Integer to Pointer

```cpp
else if( sourceType->isIntegerTy() &&
         targetType->isPointerTy() ) {
    result = this->irBuilder.CreateIntToPtr(
        sourceValue, targetType, "inttoptr" );
}
```

`IntToPtr` creates a pointer from an integer address. Used in systems programming for memory-mapped I/O and raw address manipulation.

### Float to Pointer

```cpp
else if( sourceType->isFloatingPointTy() &&
         targetType->isPointerTy() ) {
    llvm::Value* asInt =
        this->irBuilder.CreateFPToSI(
            sourceValue,
            llvm::Type::getInt64Ty( this->llvmContext ),
            "fp.toInt" );
    result = this->irBuilder.CreateIntToPtr(
        asInt, targetType, "fp.toPtr" );
}
```

Two-step composite conversion: float → `i64` via `FPToSI`, then `i64` → pointer via `IntToPtr`. The intermediate `i64` captures the truncated integer representation of the float value.

### Pointer to Float

```cpp
else if( sourceType->isPointerTy() &&
         targetType->isFloatingPointTy() ) {
    llvm::Value* asInt =
        this->irBuilder.CreatePtrToInt(
            sourceValue,
            llvm::Type::getInt64Ty( this->llvmContext ),
            "ptr.toInt" );
    result = this->irBuilder.CreateSIToFP(
        asInt, targetType, "ptr.toFp" );
}
```

Two-step composite conversion: pointer → `i64` via `PtrToInt`, then `i64` → float via `SIToFP`. The address value is reinterpreted as a signed integer, then converted to floating point.

### Fallback — Identity

```cpp
else {
    result = sourceValue;
}
```

For type combinations not covered by any specific conversion path, the source value passes through unchanged. This handles edge cases where LLVM types are structurally compatible but do not match any explicit conversion rule.

---

## Complete Conversion Matrix

| Source ↓ / Target → | Integer | Float | Pointer |
|---|---|---|---|
| **Integer** | `SExt` / `Trunc` | `SIToFP` (or constant fold) | `IntToPtr` |
| **Float** | `FPToSI` (or constant fold) | `FPExt` / `FPTrunc` (or constant fold) | `FPToSI` + `IntToPtr` |
| **Pointer** | `PtrToInt` | `PtrToInt` + `SIToFP` | `BitCast` |

---

## Examples

### Integer Widening

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I8 small = 42 as I8
    I64 large = small as I64
    puts(large.toString())
    return 0
```

**Compilation trace**:
1. **Parsing**: `42 as I8` — precedence 12, highest. `IntegerLiteral(42)` is left operand, `I8` parsed via `parseTypeNode()`. Creates `CastExpression(expression=42, targetType=I8)`. Then `small as I64` — another cast.
2. **Semantic analysis**: source expression analyzed. Target type `I8` resolved. Expression type = `I8`. Second cast: source type `I8`, target type `I64`.
3. **MIR lowering**: `CastType` instruction. `castTargetType = I8` for first cast, `I64` for second.
4. **LLVM codegen**:
   - `42 as I8`: source is `ConstantInt(i64, 42)`. Target is `i8`. Integer-to-integer: `sourceBits(64) > targetBits(8)`. `CreateTrunc(i64 42, i8)` → `i8 42`.
   - `small as I64`: source is `i8`. Target is `i64`. `sourceBits(8) < targetBits(64)`. `CreateSExt(i8 %small, i64)` → sign-extends to `i64 42`.

### Integer to Float

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 count = 7
    F64 ratio = count as F64
    puts(ratio.toString())
    return 0
```

**Compilation trace**:
1. **Parsing**: `count as F64`. Left = `IdentifierExpression("count")`, target = `F64`.
2. **Semantic analysis**: `count` resolves to `I64`. Expression type = `F64`.
3. **MIR lowering**: `CastType` with source operand = count variable, `castTargetType = F64`.
4. **LLVM codegen**: source type is `i64`, target type is `double`. Integer-to-float path. `count` is not a `ConstantInt` (it is a loaded variable), so runtime `CreateSIToFP(i64 %count, double)` emitted. Result: `double 7.0`.

### Compile-Time Constant Folding

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    F64 value = 100 as F64
    I64 truncated = 3.14 as I64
    puts(value.toString())
    puts(truncated.toString())
    return 0
```

**Compilation trace**:
1. `100 as F64`: source is `ConstantInt(i64, 100)`. Integer-to-float path detects `ConstantInt`. Compile-time: `getSExtValue() = 100`, `static_cast<double>(100) = 100.0`. `ConstantFP::get(double, 100.0)` — no runtime instruction.
2. `3.14 as I64`: source is `ConstantFP(double, 3.14)`. Float-to-integer path detects `ConstantFP`. Compile-time: `convertToDouble() = 3.14`, `static_cast<int64_t>(3.14) = 3`. `ConstantInt::get(i64, 3)` — no runtime instruction. Truncation toward zero.

### Float Precision Change

```
package examples

public function main() -> I32:
    F64 precise = 3.141592653589793
    F32 reduced = precise as F32
    F64 restored = reduced as F64
    return 0
```

**Compilation trace**:
1. `precise as F32`: source is `double`, target is `float`. Float-to-float path. `sourceBits(64) > targetBits(32)`. `CreateFPTrunc(double %precise, float)` — precision reduced to ~7 significant digits.
2. `reduced as F64`: source is `float`, target is `double`. `sourceBits(32) < targetBits(64)`. `CreateFPExt(float %reduced, double)` — precision extended, but lost digits not recovered. `restored ≈ 3.1415927410125732`, not the original value.

### Pointer Reinterpretation

```
package examples

public function main() -> I32:
    Memory<I64> buffer = new Memory<I64>(16)
    I64 address = buffer as I64
    Memory<I8> bytes = address as Memory<I8>
    return 0
```

**Compilation trace**:
1. `buffer as I64`: source is pointer type, target is `i64`. Pointer-to-integer path. `CreatePtrToInt(ptr %buffer, i64)` — extracts raw address.
2. `address as Memory<I8>`: source is `i64`, target is pointer type. Integer-to-pointer path. `CreateIntToPtr(i64 %address, ptr)` — reconstructs pointer from address.

### Cast Precedence in Expressions

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 result = 10 + 3 as I64
    puts(result.toString())
    return 0
```

**Compilation trace**:
1. **Parsing**: precedence-climbing loop. `as` has precedence 12, `+` has precedence 9. `3 as I64` binds first (precedence 12 > 9). Parsed as `10 + (3 as I64)`.
2. **Semantic analysis**: inner cast `3 as I64` resolves to `I64`. Outer `10 + castResult` — both `I64`, addition resolves normally.
3. **LLVM codegen**: `3 as I64` is same-type no-op (both `i64`). `10 + 3 = 13`.

### Chained Casts

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 value = 1000
    I8 narrow = value as I32 as I16 as I8
    puts(narrow.toString())
    return 0
```

**Compilation trace**:
1. **Parsing**: left-to-right chaining. `value as I32` creates first `CastExpression`. Result becomes left operand for `as I16`. Result becomes left operand for `as I8`. Parsed as `((value as I32) as I16) as I8`.
2. **LLVM codegen**: three sequential truncations:
   - `i64 1000` → `CreateTrunc(i64, i32)` → `i32 1000`
   - `i32 1000` → `CreateTrunc(i32, i16)` → `i16 1000`
   - `i16 1000` → `CreateTrunc(i16, i8)` → `i8 -24` (1000 mod 256 = 232, interpreted as signed = -24)

### Boolean Cast

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 nonzero = 42
    Bool flag = nonzero as Bool
    I64 back = flag as I64
    puts(back.toString())
    return 0
```

**Compilation trace**:
1. `nonzero as Bool`: source is `i64`, target is `i1`. Integer-to-integer path. `sourceBits(64) > targetBits(1)`. `CreateTrunc(i64 42, i1)` → `i1 0` (42 in binary: `...101010`, lowest bit is 0). Note: truncation to `i1` extracts only the least significant bit, not a truthiness check.
2. `flag as I64`: source is `i1`, target is `i64`. `sourceBits(1) < targetBits(64)`. `CreateSExt(i1 0, i64)` → `i64 0`.

This is an important distinction: `as Bool` is NOT a truthiness conversion. It truncates to 1 bit. For truthiness, use comparison: `nonzero != 0`.
