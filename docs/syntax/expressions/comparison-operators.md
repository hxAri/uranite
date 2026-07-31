# Comparison Operators

Uranite provides six comparison operators: equality (`==`), inequality (`!=`), less-than (`<`), greater-than (`>`), less-or-equal (`<=`), and greater-or-equal (`>=`). All comparisons return `Boolean`. These operators work natively on numeric primitives and OOP wrappers, support `None` equality checks, dispatch to operator interface methods (`Equatable.equals`, `Comparable.lessThan`/`greaterThan`/`lessOrEqual`/`greaterOrEqual`) for class types, and enforce semantic comparability via `Registry::isComparable`. Comparisons parse as `BinaryExpression` nodes at precedence 6 (equality) and 7 (relational), flow through `analyzeBinaryExpression` with multi-path type validation, lower to typed MIR comparison instructions (`CompareEqual`, `CompareNotEqual`, `CompareLessThan`, `CompareGreaterThan`, `CompareLessEqual`, `CompareGreaterEqual`), and emit LLVM IR via `generateComparison` with automatic type coercion, string-aware pointer comparison using `strcmp` with a page-threshold heuristic (4096), and separate integer/float comparison predicates.

---

## Table of Contents

- [Operator Reference](#operator-reference)
- [Precedence](#precedence)
- [Syntax](#syntax)
- [Semantic Analysis](#semantic-analysis)
  - [Operator Interface Dispatch](#operator-interface-dispatch)
  - [None Comparisons](#none-comparisons)
  - [Interface Equality](#interface-equality)
  - [Comparability Fallback](#comparability-fallback)
- [MIR Lowering](#mir-lowering)
- [MIR Codegen — generateComparison](#mir-codegen--generatecomparison)
  - [Type Coercion](#type-coercion)
  - [String-Aware Comparison](#string-aware-comparison)
  - [Integer Comparisons](#integer-comparisons)
  - [Float Comparisons](#float-comparisons)
- [Operator Interface Mapping Table](#operator-interface-mapping-table)
- [Related: is Operator](#related-is-operator)
- [Related: in Operator](#related-in-operator)
- [Examples](#examples)

---

## Operator Reference

| Operator | Operation | Token Type | MIR Instruction |
|---|---|---|---|
| `==` | Equality | `Equal` | `CompareEqual` |
| `!=` | Inequality | `NotEqual` | `CompareNotEqual` |
| `<` | Less-than | `LessThan` | `CompareLessThan` |
| `>` | Greater-than | `GreaterThan` | `CompareGreaterThan` |
| `<=` | Less-or-equal | `LessThanEqual` | `CompareLessEqual` |
| `>=` | Greater-or-equal | `GreaterThanEqual` | `CompareGreaterEqual` |

---

## Precedence

From `getOperatorPrecedenceValue` at `src/uranite/parser/parser.cpp:111-152`:

| Precedence | Operators | Category |
|---|---|---|
| 7 | `<`, `>`, `<=`, `>=`, `is`, `in`, `instanceof`, `subclassof` | Relational |
| 6 | `==`, `!=` | Equality |

Equality operators bind looser than relational operators. Both are left-associative. Expression `a < b == c` parses as `(a < b) == c`.

---

## Syntax

```
Boolean equal = x == y
Boolean notEqual = x != y
Boolean less = x < y
Boolean greater = x > y
Boolean lessOrEqual = x <= y
Boolean greaterOrEqual = x >= y

if score >= threshold:
    pass

Boolean isNone = value == None
```

---

## Semantic Analysis

At `src/uranite/semantic/analyzer.cpp:1143-1191`, comparison operators follow a multi-path validation strategy.

### Operator Interface Dispatch

At `analyzer.cpp:1149-1172`, for class/struct operands, the analyzer checks interface implementation:

```cpp
case token::Type::Equal:
case token::Type::GreaterThan:
case token::Type::GreaterThanEqual:
case token::Type::LessThan:
case token::Type::LessThanEqual:
case token::Type::NotEqual: {
    if( leftSideType->kind == Type::Kind::Class ||
        leftSideType->kind == Type::Kind::Struct ) {
        ClassTypeSharedPointer leftSideClassType =
            std::dynamic_pointer_cast<ClassType>(
                leftSideType );
        if( leftSideClassType ) {
            bool isEqualityOp =
                expression.operation ==
                    token::Type::Equal ||
                expression.operation ==
                    token::Type::NotEqual;
            bool isRelationalOp =
                expression.operation ==
                    token::Type::LessThan ||
                // ... other relational checks ...
            bool isOopWrapperType =
                qualname::isOopWrapper(
                    leftSideClassType->qualified );
            if( isEqualityOp &&
                ( leftSideClassType
                    ->implementsInterface(
                        qualname::Equatable ) ||
                  isOopWrapperType ) ) {
                return this->typeRegistry.getBool();
            }
            if( isRelationalOp &&
                ( leftSideClassType
                    ->implementsInterface(
                        qualname::Comparable ) ||
                  isOopWrapperType ) ) {
                return this->typeRegistry.getBool();
            }
        }
    }
```

Resolution order for class types:
1. **OOP wrapper shortcut**: If left operand is an OOP wrapper (e.g., `I64`, `String`), all comparisons are allowed immediately.
2. **Equatable interface**: For `==`/`!=`, check if left class implements `Equatable`. If yes, return `Boolean`.
3. **Comparable interface**: For `<`/`>`/`<=`/`>=`, check if left class implements `Comparable`. If yes, return `Boolean`.
4. **Method-without-interface warning**: If the class has an `equals` method but does not implement `Equatable`, emit diagnostic error. Same for comparison methods without `Comparable`.

### None Comparisons

At `analyzer.cpp:1174-1180`:

```cpp
bool isEqualityOp =
    expression.operation == token::Type::Equal ||
    expression.operation == token::Type::NotEqual;
bool isNoneComparison =
    rightSideType->isNone() ||
    leftSideType->isNone();
if( isEqualityOp && isNoneComparison ) {
    return this->typeRegistry.getBool();
}
```

`None` can be compared for equality with any type. `value == None` and `value != None` are always valid. Relational comparisons (`<`, `>`, etc.) with `None` are not allowed and fall through to the comparability check.

### Interface Equality

At `analyzer.cpp:1181-1186`:

```cpp
if( leftSideType->kind == Type::Kind::Interface ) {
    bool isEqualityOp =
        expression.operation == token::Type::Equal ||
        expression.operation == token::Type::NotEqual;
    if( isEqualityOp ) {
        return this->typeRegistry.getBool();
    }
}
```

Interface-typed values support equality comparison unconditionally — since the concrete type is not known at compile time, equality is always permitted.

### Comparability Fallback

At `analyzer.cpp:1187-1191`:

```cpp
if( this->typeRegistry.isComparable(
        leftSideType, rightSideType ) == false ) {
    std::string compareErrorMessage = fmt::format(
        "cannot compare types \"{}\" and \"{}\"",
        leftSideType->toString(),
        rightSideType->toString() );
    this->diagnostic.error(
        expression.source, compareErrorMessage );
}
return this->typeRegistry.getBool();
```

Final fallback: `Registry::isComparable` performs symmetric bidirectional `isAssignable` check. If neither direction is assignable, an error is emitted. Result is always `Boolean` regardless of error.

---

## MIR Lowering

At `src/uranite/ir/mir/lowering.cpp:3360-3365`, the `lowerBinaryOperation` method maps comparison token types to MIR instructions:

```cpp
case token::Type::Equal:
    instructionKind = MIRInstructionKind::CompareEqual;
    break;
case token::Type::NotEqual:
    instructionKind = MIRInstructionKind::CompareNotEqual;
    break;
case token::Type::LessThan:
    instructionKind = MIRInstructionKind::CompareLessThan;
    break;
case token::Type::GreaterThan:
    instructionKind = MIRInstructionKind::CompareGreaterThan;
    break;
case token::Type::LessThanEqual:
    instructionKind = MIRInstructionKind::CompareLessEqual;
    break;
case token::Type::GreaterThanEqual:
    instructionKind = MIRInstructionKind::CompareGreaterEqual;
    break;
```

The MIR instruction stores both source operand variables, the resolved result type, and allocates a `_binop` destination variable.

---

## MIR Codegen — generateComparison

At `src/uranite/ir/mir/codegen.cpp:2100-2274`, `generateComparison` handles all comparison instructions with automatic type coercion and string-aware logic.

### Type Coercion

Before comparison, operands with mismatched LLVM types are coerced at `codegen.cpp:2110-2135`:

| Left Type | Right Type | Coercion |
|---|---|---|
| Integer | Float | `CreateSIToFP` — promote integer to double |
| Float | Integer | `CreateSIToFP` — promote integer to double |
| Integer (N bits) | Integer (M bits) | `CreateSExt` — sign-extend narrower to wider |
| Pointer | Integer | `CreateIntToPtr` — cast integer to pointer |
| Integer | Pointer | `CreateIntToPtr` — cast integer to pointer |

```cpp
if( leftOperand->getType() !=
    rightOperand->getType() ) {
    if( leftOperand->getType()->isFloatingPointTy() ||
        rightOperand->getType()->isFloatingPointTy() ) {
        llvm::Type* doubleTy =
            llvm::Type::getDoubleTy( this->llvmContext );
        if( leftOperand->getType()->isIntegerTy() ) {
            leftOperand = this->irBuilder.CreateSIToFP(
                leftOperand, doubleTy, "cmp.itof" );
        }
        if( rightOperand->getType()->isIntegerTy() ) {
            rightOperand = this->irBuilder.CreateSIToFP(
                rightOperand, doubleTy, "cmp.itof" );
        }
    }
    else if( leftOperand->getType()->isIntegerTy() &&
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
                "cmp.sext" );
        }
        else {
            rightOperand = this->irBuilder.CreateSExt(
                rightOperand, leftOperand->getType(),
                "cmp.sext" );
        }
    }
}
```

### String-Aware Comparison

For `CompareEqual` and `CompareNotEqual`, the codegen implements a string-aware comparison heuristic at `codegen.cpp:2137-2165`:

**Detection**: A comparison is flagged as "may be string comparison" when:
1. At least one operand's semantic type is `String`, `GenericParameter`, or a class named "String".
2. Neither operand is a small constant (value between -1 and 255, which indicates a char or boolean, not a string pointer).

**Mechanism** (for `CompareEqual` at `codegen.cpp:2174-2203`):

```
if both operands are i64:
    page_threshold = 4096
    if both values >= 4096:
        interpret as pointers → call strcmp
        result = (strcmp == 0)
    else:
        integer comparison (ICmpEQ)
    merge results via PHI node
```

This heuristic works because Uranite represents strings as heap pointers (always above page boundary 4096), while small integers and None (value 0) fall below. The codegen creates three basic blocks:

1. **`eq.strcmp.call`**: Convert both i64 values to pointers via `IntToPtr`, call `strcmp`, compare result to 0.
2. **`eq.int.cmp`**: Standard `ICmpEQ` integer comparison (for non-pointer values).
3. **`eq.merge`**: PHI node selecting between `strcmp` result and integer comparison.

The same pattern applies to `CompareNotEqual` with `ICmpNE` and `strcmp != 0`.

### Integer Comparisons

For non-string integer values:

| Instruction | LLVM Builder | Predicate |
|---|---|---|
| `CompareEqual` | `CreateICmpEQ` | Signed equal |
| `CompareNotEqual` | `CreateICmpNE` | Signed not-equal |
| `CompareLessThan` | `CreateICmpSLT` | Signed less-than |
| `CompareGreaterThan` | `CreateICmpSGT` | Signed greater-than |
| `CompareLessEqual` | `CreateICmpSLE` | Signed less-or-equal |
| `CompareGreaterEqual` | `CreateICmpSGE` | Signed greater-or-equal |

All integer comparisons use **signed** predicates (S prefix).

### Float Comparisons

| Instruction | LLVM Builder | Predicate |
|---|---|---|
| `CompareEqual` | `CreateFCmpOEQ` | Ordered equal |
| `CompareNotEqual` | `CreateFCmpONE` | Ordered not-equal |
| `CompareLessThan` | `CreateFCmpOLT` | Ordered less-than |
| `CompareGreaterThan` | `CreateFCmpOGT` | Ordered greater-than |
| `CompareLessEqual` | `CreateFCmpOLE` | Ordered less-or-equal |
| `CompareGreaterEqual` | `CreateFCmpOGE` | Ordered greater-or-equal |

Float comparisons use **ordered** predicates (O prefix) — NaN comparisons return `false`. `NaN == NaN` is `false`, `NaN != NaN` is `true`, following IEEE 754 semantics.

---

## Operator Interface Mapping Table

From `FullOperatorMappings` at `src/uranite/semantic/qualnames.hpp:518-530`:

| Token | Interface | Method | Negate Result |
|---|---|---|---|
| `==` | `Equatable` | `equals` | `false` |
| `!=` | `Equatable` | `equals` | `true` |
| `<` | `Comparable` | `lessThan` | `false` |
| `>` | `Comparable` | `greaterThan` | `false` |
| `<=` | `Comparable` | `lessOrEqual` | `false` |
| `>=` | `Comparable` | `greaterOrEqual` | `false` |

The `negateResult` field is `true` for `!=` — the `equals` method is called and the result is logically negated. This avoids requiring a separate `notEquals` method.

---

## Related: is Operator

The `is` keyword has precedence 7 (same as relational operators) and maps to `CompareEqual` MIR instruction. It is semantically identical to `==` at the MIR level but carries different intent: `is` tests identity (reference equality), while `==` tests value equality. See [Type Identity](../types/type-identity.md) for full specification.

```
if value is None:
    pass

if value is not None:
    pass
```

`is not` parses as `BinaryExpression(KeywordIs, ...)` wrapped in `UnaryExpression(KeywordNot, ...)` — a negation wrapper around the `is` check.

---

## Related: in Operator

The `in` keyword has precedence 7 and tests container membership at `src/uranite/semantic/analyzer.cpp:1193-1209`:

```cpp
case token::Type::KeywordIn: {
    if( rightSideType->kind == Type::Kind::Class ||
        rightSideType->kind == Type::Kind::Struct ) {
        ClassTypeSharedPointer rightSideClassType =
            std::dynamic_pointer_cast<ClassType>(
                rightSideType );
        if( rightSideClassType ) {
            if( rightSideClassType->implementsInterface(
                    qualname::Indexable ) ) {
                return this->typeRegistry.getBool();
            }
            if( rightSideClassType->findMethod(
                    qualname::classes::string::methods
                        ::Contains ) ) {
                // warning: has "contains" but no Indexable
                return this->typeRegistry.getBool();
            }
        }
    }
    // error: does not support "in" operator
    return this->typeRegistry.getBool();
}
```

The `in` operator requires the right operand to implement the `Indexable` interface (which provides a `contains` method). Returns `Boolean`.

```
if "hello" in myList:
    pass

if key in myMap:
    pass
```

---

## Examples

### Primitive Comparisons

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 score = 85
    I64 threshold = 70

    if score >= threshold:
        puts("passed")

    if score == 100:
        puts("perfect")

    if score != 0:
        puts("attempted")

    return 0
```

**Compilation trace**:
1. **Semantic analysis**: `score >= threshold` — both `I64`, `isComparable` returns `true`. Result: `Boolean`.
2. **MIR lowering**: `CompareGreaterEqual` instruction with both variables as source operands.
3. **LLVM codegen**: `CreateICmpSGE` — signed greater-or-equal comparison. Returns `i1` (1-bit integer).

### Float Comparisons

```
package examples

public function main() -> I32:
    F64 pi = 3.14159
    F64 approx = 3.14

    if approx < pi:
        pass

    if pi == 3.14159:
        pass

    return 0
```

**Compilation trace**:
1. **MIR codegen**: `approx < pi` emits `CreateFCmpOLT` (ordered less-than). `pi == 3.14159` emits `CreateFCmpOEQ` (ordered equal). Both follow IEEE 754 — NaN values produce `false` for all ordered comparisons.

### None Equality

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    ?String name = None

    if name == None:
        puts("no name")

    if name != None:
        puts(name)

    return 0
```

**Compilation trace**:
1. **Semantic analysis**: `name == None` — right is `None` type, `isNoneComparison` is `true`. Equality with `None` is always allowed. Result: `Boolean`.
2. **MIR codegen**: Standard `ICmpEQ` — `None` is represented as integer 0, so comparison checks if the optional value is null.

### String Comparison

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    String greeting = "hello"
    String other = "world"

    if greeting == other:
        puts("same")

    if greeting != "hello":
        puts("different")

    return 0
```

**Compilation trace**:
1. **MIR codegen**: `greeting == other` — both operands have `String` type, `mayBeStringComparison` is `true`. Codegen emits the page-threshold heuristic:
   - Check if both i64 values `>= 4096` (above page boundary, likely heap pointers).
   - If both above: `IntToPtr` both, call `strcmp`, compare result to 0.
   - If either below: direct `ICmpEQ` (handles `None` or small integer cases).
   - PHI merge selects correct result.

### Operator Interface — Custom Equatable

```
package examples

from uranite.io.console import puts

public interface Equatable<T>:
    public function equals(self, T other) -> Boolean;

public class Point implements Equatable<Point>:
    public I64 xCoord
    public I64 yCoord

    public function Point(
        self, I64 xCoord, I64 yCoord) -> Void:
        self.xCoord = xCoord
        self.yCoord = yCoord

    public function equals(self, Point other) -> Boolean:
        return self.xCoord == other.xCoord and
               self.yCoord == other.yCoord

public function main() -> I32:
    Point pointA = new Point(1, 2)
    Point pointB = new Point(1, 2)

    if pointA == pointB:
        puts("equal points")

    if pointA != pointB:
        puts("different points")

    return 0
```

**Compilation trace**:
1. **Semantic analysis**: `pointA == pointB` — left is `Point` (class). Equality op detected. `Point.implementsInterface("uranite.language.Equatable")` returns `true`. Result: `Boolean`.
2. **MIR lowering**: For class types implementing `Equatable`, the `==` operator dispatches to the `equals` method via `CallFunction` MIR instruction through interface table virtual dispatch.
3. For `!=`, the `equals` method is called and the result is logically negated (since `negateResult = true` in `FullOperatorMappings`).
