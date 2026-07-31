# Logical Operators

Uranite provides three logical operators: `and`, `or`, and `not`. These are keyword-based — Uranite does not use `&&`, `||`, or `!` for logical operations. All logical operators work exclusively on `Boolean` operands and produce `Boolean` results. The semantic analyzer enforces boolean-only operands via `isAssignable` checks against the `Bool` type. `and` and `or` parse as binary expressions at precedences 2 and 1 respectively, while `not` parses as a prefix unary expression. At MIR level, they map to `LogicalAnd`, `LogicalOr`, and `LogicalNot` instructions. At LLVM codegen, `generateLogical` emits `CreateAnd`/`CreateOr` for binary logical ops (with automatic boolean coercion for wider-than-i1 integers) and `CreateNot`/`CreateICmpEQ` for logical negation (with pointer-null and integer-zero awareness).

---

## Table of Contents

- [Operator Reference](#operator-reference)
- [Precedence](#precedence)
- [Syntax](#syntax)
- [AST Representation](#ast-representation)
- [Semantic Analysis](#semantic-analysis)
  - [Binary Logical — and / or](#binary-logical--and--or)
  - [Unary Logical — not](#unary-logical--not)
- [MIR Lowering](#mir-lowering)
  - [Binary — LogicalAnd / LogicalOr](#binary--logicaland--logicalor)
  - [Unary — LogicalNot](#unary--logicalnot)
- [MIR Codegen — generateLogical](#mir-codegen--generatelogical)
  - [LogicalNot Codegen](#logicalnot-codegen)
  - [LogicalAnd / LogicalOr Codegen](#logicaland--logicalor-codegen)
  - [Boolean Coercion](#boolean-coercion)
- [Short-Circuit Evaluation](#short-circuit-evaluation)
- [Examples](#examples)

---

## Operator Reference

| Operator | Operation | Token Type | MIR Instruction | Precedence |
|---|---|---|---|---|
| `and` | Logical AND | `KeywordAnd` | `LogicalAnd` | 2 |
| `or` | Logical OR | `KeywordOr` | `LogicalOr` | 1 |
| `not` | Logical NOT | `KeywordNot` | `LogicalNot` | Prefix unary |

---

## Precedence

From `getOperatorPrecedenceValue` at `src/uranite/parser/parser.cpp:129-134`:

```cpp
case token::Type::KeywordAnd:
    return 2;
case token::Type::KeywordOr:
    return 1;
```

| Precedence | Operators |
|---|---|
| 2 | `and` |
| 1 | `or` (lowest binary precedence) |

`not` is a prefix unary operator parsed by `parseUnaryExpression` before any binary precedence climbing begins, giving it effectively higher precedence than all binary operators.

Precedence ordering: `not` binds tightest, then `and`, then `or` binds loosest.

Expression `a or b and not c` parses as `a or (b and (not c))`.

---

## Syntax

```
Boolean result = conditionA and conditionB
Boolean either = conditionA or conditionB
Boolean inverted = not conditionA

if isActive and not isExpired:
    pass

if hasPermission or isAdmin:
    pass
```

---

## AST Representation

Binary `and`/`or` use `BinaryExpression` (same node as arithmetic/comparison), defined at `src/uranite/ast/node.hpp:1197-1222`:

```cpp
struct BinaryExpression : Expression {
    ExpressionSharedPointer left;
    token::Type operation;
    ExpressionSharedPointer right;
};
```

With `operation` set to `token::Type::KeywordAnd` or `token::Type::KeywordOr`.

Unary `not` uses `UnaryExpression` at `src/uranite/ast/node.hpp:1888-1917`:

```cpp
struct UnaryExpression : Expression {
    bool isPrefix;
    ExpressionSharedPointer operand;
    token::Type operation;
};
```

With `operation = token::Type::KeywordNot` and `isPrefix = true`.

### Parsing not

At `src/uranite/parser/parser.cpp:2887-2891`:

```cpp
case token::Type::Bang:
case token::Type::KeywordNot: {
    token::Type negationOp = this->current().type;
    this->advance();
    return std::make_shared<ast::nodes::UnaryExpression>(
        negationOp,
        this->parseUnaryExpression(),
        true, unarySource );
}
```

Both `!` (`Bang`) and `not` (`KeywordNot`) are accepted as logical negation syntax. They produce identical `UnaryExpression` nodes with different `operation` token types but receive the same semantic treatment.

### is not Desugaring

The `is not` pattern at `parser.cpp:2185-2194` creates a `BinaryExpression(KeywordIs)` wrapped in `UnaryExpression(KeywordNot)`:

```cpp
bool isNegated = false;
if( kind == token::Type::KeywordIs &&
    this->current().type == token::Type::KeywordNot ) {
    isNegated = true;
    this->advance();
}
// ... parse right operand ...
left = std::make_shared<ast::nodes::BinaryExpression>(
    kind, left, right, source );
if( isNegated ) {
    left = std::make_shared<ast::nodes::UnaryExpression>(
        token::Type::KeywordNot, left, true, source );
}
```

---

## Semantic Analysis

### Binary Logical — and / or

At `src/uranite/semantic/analyzer.cpp:1214-1221`:

```cpp
case token::Type::KeywordAnd:
case token::Type::KeywordOr: {
    semantic::TypeSharedPointer boolType =
        this->typeRegistry.getBool();
    if( this->typeRegistry.isAssignable(
            boolType, leftSideType ) == false ||
        this->typeRegistry.isAssignable(
            boolType, rightSideType ) == false ) {
        this->diagnostic.error( expression.source,
            "logical operators require boolean operands" );
    }
    return this->typeRegistry.getBool();
}
```

Both operands must be assignable to `Boolean`. Non-boolean operands produce a diagnostic error. Result type is always `Boolean`.

Unlike many languages, Uranite does **not** support truthy/falsy semantics — `and`/`or` require actual `Boolean` values. `if count and name:` is an error; use `if count > 0 and name != None:` instead.

### Unary Logical — not

At `src/uranite/semantic/analyzer.cpp:3973-3981`:

```cpp
case token::Type::KeywordNot:
case token::Type::Bang: {
    semantic::TypeSharedPointer boolCheckType =
        this->typeRegistry.getBool();
    if( operandType->isBool() == false &&
        this->typeRegistry.isAssignable(
            boolCheckType, operandType ) == false ) {
        this->diagnostic.error( expression.source,
            "logical not requires boolean operand" );
        return this->typeRegistry.getError();
    }
    return this->typeRegistry.getBool();
}
```

Operand must be `Boolean` or assignable to `Boolean`. Error if not. Both `not` and `!` receive identical treatment.

---

## MIR Lowering

### Binary — LogicalAnd / LogicalOr

At `src/uranite/ir/mir/lowering.cpp:3366-3367`:

```cpp
case token::Type::KeywordAnd:
    instructionKind = MIRInstructionKind::LogicalAnd;
    break;
case token::Type::KeywordOr:
    instructionKind = MIRInstructionKind::LogicalOr;
    break;
```

Standard binary instruction: two source operands, one `_binop` destination variable with boolean result type.

### Unary — LogicalNot

At `src/uranite/ir/mir/lowering.cpp:3428`:

```cpp
case token::Type::KeywordNot:
    instructionKind = MIRInstructionKind::LogicalNot;
    break;
```

Unary instruction: one source operand, one destination variable. The `lowerUnaryOperation` method at `mir/lowering.cpp:3387` lowers the operand expression, then emits the `LogicalNot` instruction.

---

## MIR Codegen — generateLogical

At `src/uranite/ir/mir/codegen.cpp:2276-2338`, `generateLogical` handles all three logical instructions.

### LogicalNot Codegen

At `codegen.cpp:2280-2308`:

```cpp
if( instruction.instructionKind ==
    MIRInstructionKind::LogicalNot ) {
    llvm::Value* operand = this->loadVariableValue(
        instruction.sourceOperands[0] );
    llvm::Value* result = nullptr;
    if( operand->getType()->isIntegerTy( 1 ) ) {
        result = this->irBuilder.CreateNot(
            operand, "not" );
    }
    else if( operand->getType()->isIntegerTy() ) {
        result = this->irBuilder.CreateICmpEQ(
            operand,
            llvm::ConstantInt::get(
                operand->getType(), 0 ),
            "not" );
    }
    else if( operand->getType()->isPointerTy() ) {
        result = this->irBuilder.CreateICmpEQ(
            operand,
            llvm::ConstantPointerNull::get(
                llvm::cast<llvm::PointerType>(
                    operand->getType() ) ),
            "not" );
    }
    else {
        result = this->irBuilder.CreateNot(
            operand, "not" );
    }
    this->setVariableValue(
        instruction.destinationVariable, result );
    return;
}
```

Type-aware negation:

| Operand LLVM Type | LLVM Operation | Logic |
|---|---|---|
| `i1` (boolean) | `CreateNot` | Bitwise NOT on 1-bit — flips `true`/`false` |
| `iN` (wider integer) | `CreateICmpEQ(val, 0)` | Zero-test: nonzero becomes `false`, zero becomes `true` |
| Pointer | `CreateICmpEQ(val, null)` | Null-test: non-null becomes `false`, null becomes `true` |
| Other | `CreateNot` | Fallback bitwise NOT |

### LogicalAnd / LogicalOr Codegen

At `codegen.cpp:2310-2337`:

```cpp
llvm::Value* leftOperand = this->loadVariableValue(
    instruction.sourceOperands[0] );
llvm::Value* rightOperand = this->loadVariableValue(
    instruction.sourceOperands[1] );
// ... boolean coercion ...
llvm::Value* result = nullptr;
if( instruction.instructionKind ==
    MIRInstructionKind::LogicalAnd ) {
    result = this->irBuilder.CreateAnd(
        leftOperand, rightOperand, "and" );
}
else {
    result = this->irBuilder.CreateOr(
        leftOperand, rightOperand, "or" );
}
this->setVariableValue(
    instruction.destinationVariable, result );
```

| Instruction | LLVM Builder | Operation |
|---|---|---|
| `LogicalAnd` | `CreateAnd` | Bitwise AND on i1 values |
| `LogicalOr` | `CreateOr` | Bitwise OR on i1 values |

### Boolean Coercion

Before `and`/`or` operations, wider-than-i1 integer operands are coerced to boolean at `codegen.cpp:2318-2326`:

```cpp
if( leftOperand->getType()->isIntegerTy( 1 ) == false &&
    leftOperand->getType()->isIntegerTy() ) {
    leftOperand = this->irBuilder.CreateICmpNE(
        leftOperand,
        llvm::ConstantInt::get(
            leftOperand->getType(), 0 ),
        "lhs.bool" );
}
```

Same for right operand. This converts integer values to `i1` by comparing against zero: nonzero becomes `true`, zero becomes `false`. This coercion happens at LLVM IR level even though the semantic analyzer enforces boolean operands — it handles cases where the LLVM type is `i64` (e.g., OOP wrapper boolean values stored as 64-bit integers).

---

## Short-Circuit Evaluation

The current implementation does **not** use short-circuit evaluation for `and`/`or`. Both operands are fully evaluated before the logical operation. At LLVM IR level, `CreateAnd`/`CreateOr` are bitwise operations on `i1` values — both inputs must be computed.

This means side effects in the right operand always execute:

```
if isValid(data) and process(data):
    pass
```

`process(data)` executes regardless of `isValid(data)` result. This differs from languages like Python or C where `and` short-circuits.

---

## Examples

### Basic Logical Operations

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    Boolean hasAccess = true
    Boolean isActive = true
    Boolean isBlocked = false

    if hasAccess and isActive:
        puts("allowed")

    if hasAccess or isBlocked:
        puts("some access")

    if not isBlocked:
        puts("not blocked")

    return 0
```

**Compilation trace**:
1. **Semantic analysis**: `hasAccess and isActive` — both `Boolean`, passes `isAssignable(Bool, Bool)`. Result: `Boolean`. `not isBlocked` — operand is `Boolean`, passes `isBool()` check. Result: `Boolean`.
2. **MIR lowering**: `LogicalAnd` with two source operands. `LogicalNot` with one source operand.
3. **LLVM codegen**: `CreateAnd(hasAccess, isActive)` on `i1` values. `CreateNot(isBlocked)` on `i1`.

### Combined with Comparisons

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 age = 25
    Boolean hasLicense = true
    I64 score = 90

    if age >= 18 and hasLicense:
        puts("can drive")

    if score >= 90 or age < 16:
        puts("special case")

    if not (age < 18):
        puts("adult")

    return 0
```

**Compilation trace**:
1. **Parsing**: `age >= 18 and hasLicense` — precedence: `>=` (7) binds tighter than `and` (2). Parses as `(age >= 18) and hasLicense`.
2. **Semantic analysis**: `age >= 18` returns `Boolean`. `Boolean and Boolean` is valid. Result: `Boolean`.
3. **MIR**: `CompareGreaterEqual` instruction produces `_binop` boolean. `LogicalAnd` takes `_binop` and `hasLicense` as operands.

### Error Cases

```
package examples

public function main() -> I32:
    I64 count = 5
    String name = "hello"

    if count and name:
        pass

    return 0
```

**Diagnostic output**:
```
error: logical operators require boolean operands
```

Uranite does not support truthy/falsy semantics. Both operands of `and`/`or` must be `Boolean`. Use explicit comparisons: `count > 0 and name != None`.

### is not Pattern

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    ?String value = "hello"

    if value is not None:
        puts("has value")

    return 0
```

**Compilation trace**:
1. **Parser**: `is not` parsed as `BinaryExpression(KeywordIs, value, None)` wrapped in `UnaryExpression(KeywordNot, ...)`.
2. **MIR lowering**: `CompareEqual` instruction for `is`, then `LogicalNot` instruction on result.
3. **LLVM codegen**: `ICmpEQ` followed by `CreateNot` on `i1` result.
