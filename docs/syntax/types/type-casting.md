# Type Casting

Uranite supports explicit type casting through the `as` keyword and implicit type coercion through the assignability system and codegen-level widening. Explicit casts use the `expression as TargetType` syntax, parsed as a binary-precedence operator (precedence 12, highest). The `as` expression produces a `CastExpression` AST node that flows through HIR (`HIRCast`) and MIR (`MIRInstructionKind::CastType`) to LLVM IR, where the MIR codegen selects the appropriate LLVM conversion instruction based on source and target type pairs — sign extension, truncation, integer-to-float, float-to-integer, float-to-float widening/narrowing, pointer-to-integer, integer-to-pointer, and pointer-to-pointer bitcast. Semantic analysis performs no type safety validation on explicit casts — it resolves the target type and trusts the programmer. Implicit coercion occurs at two levels: the assignability system in `isAssignable` permits numeric widening (integer-to-integer, integer-to-float, float-to-float) and OOP wrapper interchangeability, while the legacy AST codegen `generateImplicitCast` inserts LLVM conversion instructions at call sites and assignments when LLVM types mismatch.

This document covers the `as` keyword syntax with operator precedence, the `CastExpression` AST node, parsing implementation (binary expression dispatch for `KeywordAs`), semantic analysis (expression analysis and target type resolution without validation), the `HIRCast` node with HIR validation, MIR lowering to `CastType` instruction, MIR codegen LLVM instruction selection (10 conversion paths), implicit numeric coercion in the assignability system, the legacy `generateImplicitCast` method (unsigned-aware widening, struct spilling, interface fat pointer wrapping), return value coercion in MIR codegen, and practical examples.

---

## Table of Contents

- [Overview](#overview)
- [Explicit Cast Syntax](#explicit-cast-syntax)
  - [The `as` Keyword](#the-as-keyword)
  - [Operator Precedence](#operator-precedence)
  - [Cast Targets](#cast-targets)
- [AST Representation](#ast-representation)
  - [CastExpression](#castexpression)
- [Parsing Implementation](#parsing-implementation)
- [Semantic Analysis](#semantic-analysis)
- [Compilation Pipeline](#compilation-pipeline)
  - [HIR Stage — HIRCast](#hir-stage--hircast)
  - [HIR Validation](#hir-validation)
  - [MIR Lowering — CastType](#mir-lowering--casttype)
  - [MIR Codegen — generateCastType](#mir-codegen--generatecasttype)
- [Implicit Numeric Coercion](#implicit-numeric-coercion)
  - [Assignability Rules](#assignability-rules)
  - [OOP Wrapper Interchangeability](#oop-wrapper-interchangeability)
- [Legacy AST Codegen — generateImplicitCast](#legacy-ast-codegen--generateimplicitcast)
- [Return Value Coercion](#return-value-coercion)
- [LLVM Instruction Summary](#llvm-instruction-summary)
- [Examples](#examples)
  - [Numeric Casts](#numeric-casts)
  - [Pointer Casts](#pointer-casts)
  - [Implicit Widening](#implicit-widening)

---

## Overview

Uranite has two type conversion mechanisms:

| Mechanism | Syntax | Safety | When Applied |
|---|---|---|---|
| Explicit cast | `expr as Type` | Unchecked — programmer responsibility | When programmer writes `as` |
| Implicit coercion | None (automatic) | Safe — numeric widening only | Assignment, parameter passing, return values |

Explicit casts can perform narrowing (lossy) conversions. Implicit coercion only performs widening (lossless) or equivalent conversions.

---

## Explicit Cast Syntax

### The `as` Keyword

The `as` keyword converts a value from one type to another:

```
I64 integerValue = 42
F64 floatValue = integerValue as F64
I32 narrowed = integerValue as I32
```

The left operand is any expression. The right operand is a type annotation parsed by `parseTypeNode()`.

### Operator Precedence

The `as` keyword has precedence 12 — the highest precedence among binary-position operators, in `src/uranite/parser/parser.cpp:131-132`:

```cpp
case token::Type::KeywordAs:
    return 12;
```

Precedence table context:

| Precedence | Operators |
|---|---|
| 12 | `as` (type cast) |
| 11 | `**` (power) |
| 10 | `*`, `/`, `%` |
| 9 | `+`, `-` |
| 7 | `==`, `!=`, `is` |
| 3 | `\|` (bitwise OR) |
| 2 | `and` |
| 1 | `or` |

Because `as` has highest precedence, it binds tighter than any arithmetic or logical operator. `x + y as F64` parses as `x + (y as F64)`, not `(x + y) as F64`.

### Cast Targets

The target type can be any valid type expression:

```
value as I32
value as F64
value as *mut U8
value as &I64
pointer as I64
integer as *U8
```

---

## AST Representation

### CastExpression

Defined in `src/uranite/ast/node.hpp:1289-1309`:

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
| `targetType` | `TypeNodeSharedPointer` | Target type annotation node |

---

## Parsing Implementation

Cast expressions are parsed inside the binary expression parsing loop, in `src/uranite/parser/parser.cpp:2154-2159`:

```cpp
if( kind == token::Type::KeywordAs ) {
    lookup::SourceSharedPointer source = this->current().source;
    this->advance();
    ast::nodes::TypeNodeSharedPointer type = this->parseTypeNode();
    left = std::make_shared<ast::nodes::CastExpression>(
        left, type, source );
    continue;
}
```

When the parser encounters the `as` keyword during binary expression parsing:

1. Record the source location.
2. Advance past the `as` token.
3. Parse the target type via `parseTypeNode()` — supports all type syntax including pointers, references, generics, unions, and optionals.
4. Wrap the left-hand expression and the target type in a `CastExpression` node.
5. `continue` the loop — the cast result becomes the new `left` for further binary operations.

Because `as` has the highest binary precedence (12), it captures its left operand before any lower-precedence operator can. Chaining casts is valid: `x as I32 as F64` parses as `(x as I32) as F64`.

---

## Semantic Analysis

Cast expression analysis in `src/uranite/semantic/analyzer.cpp:2138-2142`:

```cpp
case ast::Node::Kind::CastExpression: {
    ast::nodes::CastExpression& castExpression =
        static_cast<ast::nodes::CastExpression&>( *expression );
    this->analyzeExpression( castExpression.expression );
    expressionType = this->resolveType( castExpression.targetType );
    break;
}
```

The semantic analyzer performs two operations:

1. **Analyze source expression**: Ensures the source expression is well-typed. Its type is computed but not checked against the target.
2. **Resolve target type**: Converts the target type annotation node into a `TypeSharedPointer`.

The analyzer does **not** validate whether the cast is safe or meaningful. Any type can be cast to any other type — the programmer is responsible for ensuring the cast is valid. Invalid casts (e.g., casting a `String` to `I64`) will produce incorrect LLVM IR or runtime crashes rather than compile-time errors.

The expression type of a `CastExpression` is always the resolved target type.

---

## Compilation Pipeline

### HIR Stage — HIRCast

The `HIRCast` node preserves the source expression and target type through HIR, defined in `src/uranite/ir/hir.hpp:762-776`:

```cpp
struct HIRCast : HIRNode {

    HIRNodeSharedPointer sourceExpression;
    semantic::TypeSharedPointer targetCastType;

    HIRCast(
        HIRNodeSharedPointer sourceExpression,
        semantic::TypeSharedPointer targetCastType,
        const lookup::SourceSharedPointer& sourceLocation
    ) : HIRNode( HIRNodeKind::Cast, targetCastType, sourceLocation ),
        sourceExpression( std::move( sourceExpression ) ),
        targetCastType( std::move( targetCastType ) ) {
    }

};
```

HIR lowering from AST in `src/uranite/ir/hir/lowering.cpp:1034-1038`:

```cpp
case ast::Node::Kind::CastExpression: {
    ast::nodes::CastExpression& castExpression =
        static_cast<ast::nodes::CastExpression&>( *expression );
    HIRNodeSharedPointer sourceExpression =
        this->lowerExpression( castExpression.expression );
    semantic::TypeSharedPointer targetType =
        this->resolveTypeNode( castExpression.targetType );
    return std::make_shared<HIRCast>(
        std::move( sourceExpression ), targetType, expression->source );
}
```

The source expression is recursively lowered to HIR, and the target type is resolved from the AST type node.

### HIR Validation

The HIR validator checks for null source expressions in `src/uranite/ir/hir/validator.cpp:494-502`:

```cpp
case HIRNodeKind::Cast: {
    HIRCast& castNode = static_cast<HIRCast&>( *expression );
    if( castNode.sourceExpression == nullptr ) {
        this->addError(
            "cast has null source expression",
            castNode.sourceLocation );
    }
    else {
        this->validateExpression( castNode.sourceExpression );
    }
    break;
}
```

Validates that the source expression exists and recursively validates it. No type compatibility check is performed at this stage.

### MIR Lowering — CastType

MIR lowering converts `HIRCast` to a `CastType` instruction in `src/uranite/ir/mir/lowering.cpp:2423-2433`:

```cpp
case hir::HIRNodeKind::Cast: {
    hir::HIRCast& castNode =
        static_cast<hir::HIRCast&>( *hirExpression );
    MIRVariableIdentifier sourceVariable =
        this->lowerExpression( castNode.sourceExpression );
    MIRInstruction castInstruction( MIRInstructionKind::CastType );
    castInstruction.sourceOperands.push_back( sourceVariable );
    castInstruction.castTargetType = castNode.targetCastType;
    castInstruction.operandType = castNode.resolvedType;
    castInstruction.sourceLocation = castNode.sourceLocation;
    MIRVariableIdentifier resultVariable =
        this->currentFunction->allocateVariable(
            "_cast", castNode.resolvedType, false );
    castInstruction.destinationVariable = resultVariable;
    return this->emitInstruction( castInstruction );
}
```

The MIR instruction carries:
- `sourceOperands[0]`: the source variable to cast from
- `castTargetType`: the semantic target type
- `destinationVariable`: the result variable allocated with the target type

### MIR Codegen — generateCastType

The `generateCastType` method in `src/uranite/ir/mir/codegen.cpp:2413-2494` selects the appropriate LLVM instruction based on source and target LLVM type pairs:

```cpp
void MIRCodegen::generateCastType(
    const MIRInstruction& instruction ) {
    llvm::Value* sourceValue =
        this->loadVariableValue( instruction.sourceOperands[0] );
    llvm::Type* targetType =
        this->toLLVMType( instruction.castTargetType );
    llvm::Type* sourceType = sourceValue->getType();
    llvm::Value* result = nullptr;

    if( sourceType == targetType ) {
        result = sourceValue;
    }
    // ... type-pair dispatch ...
    if( result != nullptr ) {
        this->setVariableValue(
            instruction.destinationVariable, result );
    }
}
```

The conversion dispatch table:

| Source Type | Target Type | LLVM Instruction | Name |
|---|---|---|---|
| Same type | Same type | No-op (identity) | — |
| Integer (narrow) | Integer (wide) | `CreateSExt` | `sext` |
| Integer (wide) | Integer (narrow) | `CreateTrunc` | `trunc` |
| Integer | Float | `CreateSIToFP` (or constant fold) | `sitofp` |
| Float | Integer | `CreateFPToSI` (or constant fold) | `fptosi` |
| Float (narrow) | Float (wide) | `CreateFPExt` | `fpext` |
| Float (wide) | Float (narrow) | `CreateFPTrunc` | `fptrunc` |
| Pointer | Pointer | `CreateBitCast` | `bitcast` |
| Pointer | Integer | `CreatePtrToInt` | `ptrtoint` |
| Integer | Pointer | `CreateIntToPtr` | `inttoptr` |
| Float | Pointer | `CreateFPToSI` + `CreateIntToPtr` | `fp.toInt` + `fp.toPtr` |
| Pointer | Float | `CreatePtrToInt` + `CreateSIToFP` | `ptr.toInt` + `ptr.toFp` |

**Constant folding**: When the source value is a compile-time constant (`ConstantInt` or `ConstantFP`), the codegen performs the conversion at compile time rather than emitting a runtime instruction. For example, casting constant integer `42` to `F64` produces `ConstantFP::get(targetType, 42.0)` directly.

**Two-step conversions**: Float-to-pointer and pointer-to-float require an intermediate integer conversion. `Float → Pointer` first converts to `i64` via `FPToSI`, then to pointer via `IntToPtr`. `Pointer → Float` reverses: `PtrToInt` to `i64`, then `SIToFP` to the target float type.

**Fallback**: If no conversion rule matches, the source value is used as-is (identity fallback).

---

## Implicit Numeric Coercion

### Assignability Rules

The `isAssignable` method in `src/uranite/semantic/typeref.cpp:340-380` permits implicit numeric conversions without explicit `as` casts:

**Integer-to-integer widening**:

```cpp
if( target->isIntegral() && source->isIntegral() ) {
    return true;
}
```

Any integer type is assignable to any other integer type. This covers both widening (`I8` → `I64`) and narrowing (`I64` → `I32`). The assignability check does not distinguish safe widening from unsafe narrowing — both are permitted at the semantic level. The codegen handles the actual conversion instruction.

**Integer-to-float promotion**:

```cpp
if( target->isFloatingPoint() && source->isIntegral() ) {
    return true;
}
```

Any integer type is assignable to any float type. `I64` → `F64` is valid without a cast.

**Float-to-float widening**:

```cpp
if( target->isFloatingPoint() && source->isFloatingPoint() ) {
    return true;
}
```

Any float type is assignable to any other float type. `F32` → `F64` (widening) and `F64` → `F32` (narrowing) are both permitted.

**Float-to-integer truncation**:

```cpp
if( target->isIntegral() && source->isFloatingPoint() ) {
    return true;
}
```

Float types are assignable to integer types. This is a potentially lossy conversion (truncation of fractional part) but is allowed without explicit cast.

**Float widening by bit width**:

```cpp
if( target->kind == Type::Kind::Float &&
    source->kind == Type::Kind::Float ) {
    FloatTypeSharedPointer targetFloatType =
        std::static_pointer_cast<FloatType>( target );
    FloatTypeSharedPointer sourceFloatType =
        std::static_pointer_cast<FloatType>( source );
    if( targetFloatType->bitWidth >= sourceFloatType->bitWidth ) {
        return true;
    }
}
```

Additional float-specific check: wider float targets accept narrower float sources.

### OOP Wrapper Interchangeability

In `src/uranite/semantic/typeref.cpp:341-370`:

```cpp
if( target->kind == Type::Kind::Class &&
    source->kind == Type::Kind::Class ) {
    bool targetIsIntOop = qualname::isIntegerOop( target->qualified );
    bool sourceIsIntOop = qualname::isIntegerOop( source->qualified );
    bool targetIsFloatOop = qualname::isFloatOop( target->qualified );
    bool sourceIsFloatOop = qualname::isFloatOop( source->qualified );
    if( ( targetIsIntOop && sourceIsIntOop ) ||
        ( targetIsFloatOop && sourceIsFloatOop ) ||
        ( targetIsFloatOop && sourceIsIntOop ) ) {
        return true;
    }
}
```

OOP wrapper classes for numeric types are interchangeable:
- Any integer wrapper (`I8`, `I16`, `I32`, `I64`, `U8`, `U16`, `U32`, `U64`) is assignable to any other integer wrapper.
- Any float wrapper (`F32`, `F64`) is assignable to any other float wrapper.
- Integer wrappers are assignable to float wrappers (promotion).
- Float wrappers are assignable to integer wrappers (reverse path checked separately).

---

## Legacy AST Codegen — generateImplicitCast

The legacy AST codegen path includes `generateImplicitCast` in `src/uranite/codegen/codegen.cpp:4704-4769`, which inserts conversion instructions when LLVM types mismatch at call sites and assignments:

```cpp
llvm::Value* LLVMCodegen::generateImplicitCast(
    llvm::Value* value, llvm::Type* targetType,
    bool isUnsigned ) {
    if( value->getType() == targetType ) {
        return value;
    }
    // ... conversion dispatch ...
}
```

Key differences from `generateCastType`:

| Feature | `generateCastType` (MIR) | `generateImplicitCast` (Legacy) |
|---|---|---|
| Trigger | Explicit `as` cast | Automatic at type mismatch |
| Unsigned awareness | Always signed (`CreateSExt`) | Respects `isUnsigned` flag (`CreateZExt` vs `CreateSExt`) |
| Struct spilling | Not handled | Spills struct values to alloca for pointer targets |
| Interface wrapping | Not handled | Wraps class pointers into interface fat pointers |
| Null pointer | Not handled | Converts `ConstantPointerNull` to target pointer type |

The `isUnsigned` parameter controls whether integer widening uses zero extension (`ZExt`) or sign extension (`SExt`). This is determined by the caller based on whether the source type is an unsigned integer type (`U8`, `U16`, `U32`, `U64`, `Byte`, `Char`).

---

## Return Value Coercion

MIR codegen coerces return values to match the expected function return type in `generateReturnValue` at `src/uranite/ir/mir/codegen.cpp:5506-5555`. The method handles special cases:

**Generator functions**: Instead of emitting a LLVM `ret`, stores `true` to the generator `done` flag and branches to the exit block.

**Async wrapper functions**: Converts the return value to `i64` for the async completion call:
- Integer values: `CreateIntCast` to `i64`
- Pointer values: `CreatePtrToInt` to `i64`
- Float values: `CreateFPToSI` to `i64`

Then calls `uraniteTaskComplete(taskPtr, completionValue)` to signal task completion.

**Regular functions**: The return value is coerced to match `expectedReturnType` using `CreateBitCast` when the LLVM types differ (e.g., pointer subtype differences).

---

## LLVM Instruction Summary

Complete mapping of Uranite cast operations to LLVM instructions:

| Uranite Operation | LLVM Instruction | Direction | Lossy |
|---|---|---|---|
| `I8 as I64` | `sext i8 to i64` | Widening | No |
| `I64 as I8` | `trunc i64 to i8` | Narrowing | Yes |
| `U8 as U64` (implicit) | `zext i8 to i64` | Widening | No |
| `I64 as F64` | `sitofp i64 to double` | Promotion | Possible (precision) |
| `F64 as I64` | `fptosi double to i64` | Truncation | Yes (fractional lost) |
| `F32 as F64` | `fpext float to double` | Widening | No |
| `F64 as F32` | `fptrunc double to float` | Narrowing | Yes |
| `*T as *U` | `bitcast ptr to ptr` | Reinterpret | N/A |
| `*T as I64` | `ptrtoint ptr to i64` | Extract address | No |
| `I64 as *T` | `inttoptr i64 to ptr` | Forge pointer | N/A |
| `F64 as *T` | `fptosi` + `inttoptr` | Two-step | Yes |
| `*T as F64` | `ptrtoint` + `sitofp` | Two-step | Possible |

---

## Examples

### Numeric Casts

Explicit type conversion between numeric types:

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 bigInteger = 1000
    I32 smallInteger = bigInteger as I32
    F64 floatValue = bigInteger as F64
    I64 truncated = 3.14 as I64

    puts(smallInteger.toString())
    puts(floatValue.toString())
    puts(truncated.toString())

    return 0
```

**Compilation trace**:
1. **Parser**: `bigInteger as I32` — `as` at precedence 12 captures `bigInteger` as left operand, `I32` as target type via `parseTypeNode()`. Creates `CastExpression(IdentifierExpression("bigInteger"), SimpleTypeNode("I32"))`.
2. **Semantic analysis**: Analyzes `bigInteger` (type `I64`), resolves target `I32`. Expression type set to `I32`.
3. **HIR**: `HIRCast(sourceExpression=HIRIdentifier("bigInteger"), targetCastType=I32Type)`.
4. **MIR**: `CastType` instruction with `sourceOperands=[bigInteger_var]`, `castTargetType=I32`, `destinationVariable=_cast_var`.
5. **MIR codegen**: Source is `i64`, target is `i32`. `i64 > i32` → `CreateTrunc(sourceValue, i32, "trunc")`. Result stored in destination variable.
6. `bigInteger as F64`: Source `i64`, target `double` → `CreateSIToFP(sourceValue, double, "sitofp")`.
7. `3.14 as I64`: Source is `ConstantFP` → constant folded to `ConstantInt::get(i64, 3, true)`. No runtime instruction emitted.

### Pointer Casts

Casting between pointer types and integers:

```
package examples

from uranite.memory import Memory
from uranite.io.console import puts

public function main() -> I32:
    Memory<I64> buffer = new Memory<I64>(4)
    *mut I64 rawPointer = buffer.addressOf(0)

    I64 addressValue = rawPointer as I64
    puts(addressValue.toString())

    unsafe:
        *mut I64 reconstructed = addressValue as *mut I64
        *reconstructed = 42
        I64 readBack = *reconstructed
        puts(readBack.toString())

    return 0
```

**Compilation trace**:
1. `rawPointer as I64`: Source is `ptr` (pointer), target is `i64` → `CreatePtrToInt(sourceValue, i64, "ptrtoint")`. Extracts the raw memory address as an integer.
2. `addressValue as *mut I64`: Source is `i64`, target is `ptr` (pointer) → `CreateIntToPtr(sourceValue, ptr, "inttoptr")`. Reconstructs a pointer from an integer address. The dereference operations require `unsafe` because the result is a raw pointer.

### Implicit Widening

Numeric values widened automatically without `as`:

```
package examples

from uranite.io.console import puts

public function computeAverage(F64 first, F64 second) -> F64:
    return (first + second) / 2.0

public function main() -> I32:
    I64 integerA = 10
    I64 integerB = 20

    F64 average = computeAverage(integerA, integerB)
    puts(average.toString())

    I32 small = 127
    I64 wide = small

    return 0
```

**Compilation trace**:
1. `computeAverage(integerA, integerB)`: Parameters expect `F64`, arguments are `I64`. Assignability check: `isAssignable(F64, I64)` → `target.isFloatingPoint() && source.isIntegral()` → `true`. At codegen, `generateImplicitCast` inserts `CreateSIToFP(integerA, double, "signed_integer_to_floating_point")` for each argument.
2. `I64 wide = small`: Target `I64`, source `I32`. `isAssignable(I64, I32)` → `target.isIntegral() && source.isIntegral()` → `true`. At codegen, `CreateSExt(small, i64, "sign_extend")` widens the 32-bit value to 64-bit.
