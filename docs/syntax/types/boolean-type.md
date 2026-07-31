# Boolean Type

Uranite's `Boolean` type represents truth values: `True` and `False`. It maps to LLVM's `i1` — a single-bit integer. Boolean values are the native result type of all comparison and logical operations. Control flow constructs (`if`, `while`) accept boolean conditions directly and apply truthiness coercion for non-boolean types.

This document covers the complete boolean type specification, LLVM representation, logical operator semantics and evaluation strategy, truthiness coercion for control flow, comparison operator emission mapping, and the OOP wrapper class.

---

## Table of Contents

- [Type Identity](#type-identity)
- [LLVM Representation](#llvm-representation)
  - [Type Mapping](#type-mapping)
  - [Constant Emission](#constant-emission)
  - [Memory Layout](#memory-layout)
- [Boolean Literals](#boolean-literals)
- [Logical Operators](#logical-operators)
  - [Logical AND](#logical-and)
  - [Logical OR](#logical-or)
  - [Logical NOT](#logical-not)
  - [Evaluation Strategy](#evaluation-strategy)
  - [Truthiness Coercion in Logical Operators](#truthiness-coercion-in-logical-operators)
- [Control Flow Integration](#control-flow-integration)
  - [Conditional Branching](#conditional-branching)
  - [Truthiness Coercion Rules](#truthiness-coercion-rules)
  - [Coercion Examples](#coercion-examples)
- [Comparison Operators](#comparison-operators)
  - [Integer Comparisons](#integer-comparisons)
  - [Floating-Point Comparisons](#floating-point-comparisons)
  - [String Comparisons](#string-comparisons)
  - [Pointer and None Comparisons](#pointer-and-none-comparisons)
  - [The is Keyword](#the-is-keyword)
  - [Mixed-Type Comparison Coercion](#mixed-type-comparison-coercion)
- [Assignability Rules](#assignability-rules)
- [The OOP Wrapper](#the-oop-wrapper)
  - [Boolean Class](#boolean-class)
  - [Builtin Methods](#builtin-methods)
- [Examples](#examples)
  - [Boolean Declarations](#boolean-declarations)
  - [Logical Operations](#logical-operations)
  - [Control Flow with Truthiness](#control-flow-with-truthiness)
  - [Comparison Chains](#comparison-chains)
  - [Practical Usage](#practical-usage)

---

## Type Identity

| Property | Value |
|---|---|
| Type Name | `Boolean` |
| Primitive Kind | `Type::Kind::Bool` |
| Qualified Name (primitive) | `uranite.builtin.bool` |
| Qualified Name (OOP wrapper) | `uranite.language.boolean.Boolean` |
| LLVM Type | `i1` |
| Literal Values | `True`, `False` |

The `isBool()` predicate on `Type` returns `true` when the kind is `Kind::Bool` or when the qualified name matches `uranite.builtin.bool`.

---

## LLVM Representation

### Type Mapping

The `Boolean` type maps to `llvm::Type::getInt1Ty(context)` — a 1-bit integer in LLVM IR. This mapping applies both to the primitive `Bool` kind and to the `Boolean` OOP wrapper class:

| Source Type | Condition | LLVM Type |
|---|---|---|
| `Type::Kind::Bool` | Always | `i1` |
| `Type::Kind::Class` | `className == "Boolean"` | `i1` |
| `Type::Kind::Integer` | `bitWidth == 1` | `i1` |

The `toLLVMType()` function recognizes the `Boolean` wrapper class qualified name and emits `i1` directly — no struct or pointer indirection.

### Constant Emission

Boolean constants are emitted as `ConstantInt::get(Int1Ty, value)`:

- `True` → `ConstantInt::get(Int1Ty, 1)` — the `i1` value `1`
- `False` → `ConstantInt::get(Int1Ty, 0)` — the `i1` value `0`

For global boolean constants, the initializer uses `mirGlobal.initialValue.booleanValue` to select `1` or `0`.

### Memory Layout

| Property | Value |
|---|---|
| Logical size | 1 bit |
| Storage size | 1 byte (LLVM `alloca` minimum) |
| Alignment | 1 byte |

Although `Boolean` is logically a single bit, LLVM allocates at least 1 byte for stack variables. When stored in structs or arrays, the backend may pack multiple booleans into a single byte depending on optimization level.

---

## Boolean Literals

`True` and `False` are keyword tokens recognized by the lexer. They are classified as `KeywordTrue` and `KeywordFalse` token types, parsed into `BoolLiteralExpression` AST nodes, and carry the literal boolean value through the entire pipeline:

```
Lexer   → KeywordTrue / KeywordFalse token
Parser  → BoolLiteralExpression(value: true/false)
HIR     → HIRBoolLiteral
MIR     → LoadConstant with booleanValue field
Codegen → ConstantInt::get(Int1Ty, 1/0)
```

---

## Logical Operators

Uranite uses keyword-based logical operators: `and`, `or`, `not`. The symbolic equivalents (`&&`, `||`, `!`) are not valid syntax.

### Logical AND

The `and` operator maps through the pipeline:

```
Token: KeywordAnd → MIR: LogicalAnd → Codegen: CreateAnd(left, right)
```

Result is `True` only when both operands are `True`.

| Left | Right | Result |
|---|---|---|
| `True` | `True` | `True` |
| `True` | `False` | `False` |
| `False` | `True` | `False` |
| `False` | `False` | `False` |

### Logical OR

The `or` operator maps through the pipeline:

```
Token: KeywordOr → MIR: LogicalOr → Codegen: CreateOr(left, right)
```

Result is `True` when either operand is `True`.

| Left | Right | Result |
|---|---|---|
| `True` | `True` | `True` |
| `True` | `False` | `True` |
| `False` | `True` | `True` |
| `False` | `False` | `False` |

### Logical NOT

The `not` operator is a unary prefix operator:

```
Token: KeywordNot → MIR: LogicalNot → Codegen: type-dependent
```

The codegen handles `not` differently based on operand type:

| Operand Type | LLVM Implementation | Description |
|---|---|---|
| `i1` (boolean) | `CreateNot(operand)` | Bitwise NOT on 1-bit integer (flips 0↔1) |
| Integer (wider than `i1`) | `CreateICmpEQ(operand, 0)` | Produces `True` if value is zero |
| Pointer | `CreateICmpEQ(operand, null)` | Produces `True` if pointer is null |
| Other | `CreateNot(operand)` | Fallback bitwise NOT |

This means `not` performs logical negation for booleans and truthiness inversion for other types.

### Evaluation Strategy

Uranite's `and` and `or` operators use **eager evaluation**, not short-circuit evaluation. Both operands are fully evaluated before the logical operation executes.

At the MIR level, `LogicalAnd` and `LogicalOr` are single instructions with two source operands. Both operands are lowered to MIR variables by `lowerExpression()` before the logical instruction is emitted. The codegen then applies `CreateAnd` or `CreateOr` — LLVM's bitwise AND and OR on `i1` values, which produce the same truth table as logical operations.

This means:

```uranite
Boolean result = expensiveCheck() and anotherCheck()
```

Both `expensiveCheck()` and `anotherCheck()` are called regardless of the first result. If `expensiveCheck()` returns `False`, `anotherCheck()` is still evaluated.

To achieve short-circuit behavior, use nested `if` statements:

```uranite
Boolean result = False
if expensiveCheck():
    if anotherCheck():
        result = True
```

### Truthiness Coercion in Logical Operators

When `and` or `or` operands are not `i1`, the codegen coerces them to boolean before applying the logical operation. If an operand is a wider integer type (e.g., `i32`, `i64`), it is converted via:

```
CreateICmpNE(operand, ConstantInt::get(operandType, 0), "lhs.bool")
```

This produces `True` (i1 `1`) if the integer is non-zero, `False` (i1 `0`) if zero. After coercion, `CreateAnd`/`CreateOr` operates on two `i1` values.

---

## Control Flow Integration

### Conditional Branching

The `if` and `while` constructs lower to `BranchConditional` MIR instructions. The `generateBranchConditional()` function loads the condition variable and creates a conditional branch:

```
CreateCondBr(conditionValue, trueBlock, falseBlock)
```

LLVM's `CreateCondBr` requires an `i1` operand. When the condition is not `i1`, the codegen applies truthiness coercion.

### Truthiness Coercion Rules

The `generateBranchConditional()` function checks if the condition value's type is `i1`. If not, it applies type-specific coercion:

| Condition Type | Coercion | "Truthy" When |
|---|---|---|
| `i1` (Boolean) | None — used directly | Value is `1` |
| Integer (`i8`, `i16`, `i32`, `i64`) | `CreateICmpNE(value, 0)` | Value is non-zero |
| Pointer | `CreateICmpNE(value, null)` | Pointer is non-null |
| Float (`float`, `double`) | `CreateFCmpONE(value, 0.0)` | Value is not `0.0` and not `NaN` |

All coerced results are `i1`, suitable for `CreateCondBr`.

**Integer truthiness:** Any non-zero integer is truthy. Zero is falsy. This includes all bit widths.

**Pointer truthiness:** Any non-null pointer is truthy. Null pointers are falsy. This is how `None` checks work in control flow — `None` produces a null pointer, which coerces to `False`.

**Float truthiness:** Uses ordered not-equal (`FCmpONE`), which returns `False` for both `0.0` and `NaN`. This means `NaN` is falsy, matching the convention that "unknown" values should not pass truth checks.

### Coercion Examples

```uranite
I64 count = 42
if count:
    puts("non-zero")
```

The condition `count` is an `i64`. The codegen emits `ICmpNE(count, 0)` to produce an `i1`, then branches on the result.

```uranite
?String name = None
if name:
    puts("has value")
```

The condition `name` is a pointer. The codegen emits `ICmpNE(name, null)` to produce an `i1`.

---

## Comparison Operators

All comparison operators produce `Boolean` (`i1`) results. The MIR lowering maps source operators to comparison instruction kinds, and the codegen emits type-appropriate LLVM comparison instructions.

### Integer Comparisons

| Uranite Operator | MIR Instruction | LLVM Instruction | Label |
|---|---|---|---|
| `==` | `CompareEqual` | `CreateICmpEQ` | `"eq"` |
| `!=` | `CompareNotEqual` | `CreateICmpNE` | `"ne"` |
| `<` | `CompareLessThan` | `CreateICmpSLT` | `"lt"` |
| `>` | `CompareGreaterThan` | `CreateICmpSGT` | `"gt"` |
| `<=` | `CompareLessEqual` | `CreateICmpSLE` | `"le"` |
| `>=` | `CompareGreaterEqual` | `CreateICmpSGE` | `"ge"` |

All relational comparisons use **signed** predicates (`SLT`, `SGT`, `SLE`, `SGE`). Unsigned comparisons are not exposed at the language level for relational operators.

### Floating-Point Comparisons

| Uranite Operator | MIR Instruction | LLVM Instruction | Label |
|---|---|---|---|
| `==` | `CompareEqual` | `CreateFCmpOEQ` | `"eq"` |
| `!=` | `CompareNotEqual` | `CreateFCmpONE` | `"ne"` |
| `<` | `CompareLessThan` | `CreateFCmpOLT` | `"lt"` |
| `>` | `CompareGreaterThan` | `CreateFCmpOGT` | `"gt"` |
| `<=` | `CompareLessEqual` | `CreateFCmpOLE` | `"le"` |
| `>=` | `CompareGreaterEqual` | `CreateFCmpOGE` | `"ge"` |

All float comparisons use **ordered** predicates — they return `False` if either operand is NaN.

### String Comparisons

String equality and inequality receive special treatment. When the codegen detects that operands may be strings (via variable type descriptors), it emits a multi-block comparison:

1. Check if both `i64` values are above the page threshold (4096) — values above this threshold are likely string pointers, not small integer constants.
2. If both are above threshold: convert to pointers, call `strcmp`, compare result to 0.
3. If not both above threshold: fall through to integer comparison (`ICmpEQ`/`ICmpNE`).
4. Merge results with a PHI node selecting from either path.

This heuristic avoids calling `strcmp` on small integer values that happen to share a variable type with strings.

### Pointer and None Comparisons

Pointer comparison uses `ICmpEQ`/`ICmpNE` on the raw pointer values. `None` produces a null pointer, so `value == None` compiles to `ICmpEQ(value, null)`.

### The is Keyword

The `is` keyword maps to `CompareEqual` at the MIR level — identical to `==`. It exists for readability in identity and `None` checks:

```uranite
if value is None:
    puts("absent")
```

### Mixed-Type Comparison Coercion

When comparing operands of different types, the codegen coerces before comparison:

| Left Type | Right Type | Coercion | Comparison Type |
|---|---|---|---|
| Integer | Integer (different width) | Narrow → `CreateSExt` to wider | `ICmpEQ`/`ICmpSLT`/etc. |
| Integer | Float | Integer → `CreateSIToFP(double)` | `FCmpOEQ`/`FCmpOLT`/etc. |
| Float | Integer | Integer → `CreateSIToFP(double)` | `FCmpOEQ`/`FCmpOLT`/etc. |
| Pointer | Integer | Integer → `CreateIntToPtr` | `ICmpEQ`/etc. |
| Integer | Pointer | Integer → `CreateIntToPtr` | `ICmpEQ`/etc. |

---

## Assignability Rules

The `Boolean` type participates in the standard assignability rules:

- `Boolean` is assignable to `Boolean` — trivial identity.
- `Boolean` is classified as `isPrimitive()` via `Kind::Bool`.
- The `isBool()` predicate checks both `Kind::Bool` and the qualified name `uranite.builtin.bool`.
- `Boolean` is part of the `oopWrapperQualified` set, so the OOP wrapper is recognized by `isOopWrapper()`.
- `Boolean` is **not** in `integerOopQualified` or `floatOopQualified` — it is not considered a numeric type.

The semantic analyzer enforces boolean type for logical operations. In `analyzeExpression`, `and`/`or` operators verify that both operands are assignable to `Boolean`:

```
if( operandType->isBool() == false && isAssignable( boolCheckType, operandType ) == false )
    → type error
```

---

## The OOP Wrapper

### Boolean Class

The `Boolean` class in `stdlibs/language/boolean.urn`:

```
final class Boolean
    protect Boolean value
```

It is a `final` class — no subclassing. The `value` field is typed as `Boolean` itself, which at the LLVM level is `i1`.

**Constructor:** `Boolean(self, Boolean value)` — wraps a truth value.

**Methods:**

| Method | Signature | Description |
|---|---|---|
| `getValue()` | `() -> Boolean` | Return underlying truth value |
| `toString()` | `() -> String` | Returns "True" or "False" |
| `negate()` | `() -> Boolean` | Returns opposite truth value |
| `logicalAnd(other)` | `(Boolean) -> Boolean` | Logical conjunction |
| `logicalOr(other)` | `(Boolean) -> Boolean` | Logical disjunction |

### Builtin Methods

The codegen intercepts OOP wrapper method calls on `Boolean` and emits inline instructions:

| Method | LLVM Implementation |
|---|---|
| `toString()` | `snprintf`-style "True"/"False" string emission |
| `negate()` | `CreateNot(self)` |
| `logicalAnd(other)` | `CreateAnd(self, other)` |
| `logicalOr(other)` | `CreateOr(self, other)` |
| `equals(other)` | `CreateICmpEQ(self, other)` |
| `hashCode()` | `CreateIntCast(self, i64)` — zero-extends `i1` to `i64` |
| `isZero()` | `CreateICmpEQ(self, 0)` |
| `isPositive()` | `CreateICmpSGT(self, 0)` |
| `isNegative()` | `CreateICmpSLT(self, 0)` |

---

## Examples

### Boolean Declarations

```uranite
Boolean active = True
Boolean disabled = False
Boolean ready = active and not disabled

Boolean yes = True
Boolean no = yes.negate()
String label = yes.toString()
```

### Logical Operations

```uranite
Boolean hasAccess = True
Boolean isAdmin = False
Boolean isOwner = True

Boolean canEdit = hasAccess and (isAdmin or isOwner)
Boolean cannotEdit = not canEdit

Boolean bothAdmin = isAdmin.logicalAnd( isOwner )
Boolean eitherAdmin = isAdmin.logicalOr( isOwner )

Boolean tripleCheck = hasAccess and isOwner and not isAdmin
```

### Control Flow with Truthiness

```uranite
from uranite.io.console import puts

I64 count = 42
if count:
    puts("count is non-zero")

I64 zero = 0
if not zero:
    puts("zero is falsy")

F64 temperature = 0.0
if not temperature:
    puts("temperature is zero or NaN")

?String name = "Alice"
if name:
    puts("name is present")

?String missing = None
if not missing:
    puts("missing is None (null pointer)")
```

### Comparison Chains

```uranite
I64 age = 25

Boolean isAdult = age >= 18
Boolean isSenior = age >= 65
Boolean isYoungAdult = age >= 18 and age < 30

F64 score = 85.5
Boolean passing = score >= 60.0
Boolean honors = score >= 90.0

String greeting = "hello"
Boolean isEmpty = greeting == ""
Boolean isHello = greeting == "hello"

?I64 value = None
Boolean isAbsent = value is None
```

### Practical Usage

```uranite
from uranite.io.console import puts

public function isLeapYear( I64 year ) -> Boolean:
    if year % 400 == 0:
        return True
    if year % 100 == 0:
        return False
    return year % 4 == 0

public function isValidTriangle( F64 sideA, F64 sideB, F64 sideC ) -> Boolean:
    if sideA <= 0.0 or sideB <= 0.0 or sideC <= 0.0:
        return False
    Boolean sumCheck1 = sideA + sideB > sideC
    Boolean sumCheck2 = sideA + sideC > sideB
    Boolean sumCheck3 = sideB + sideC > sideA
    return sumCheck1 and sumCheck2 and sumCheck3

public function clamp( I64 value, I64 lower, I64 upper ) -> I64:
    if value < lower:
        return lower
    if value > upper:
        return upper
    return value

public function findFirst( Memory<I64> elements, I64 size, I64 target ) -> I64:
    I64 index = 0
    while index < size:
        if elements[index] == target:
            return index
        index = index + 1
    return -1

public function main() -> I32:
    Boolean leap = isLeapYear( 2024 )
    puts( leap.toString() )

    Boolean valid = isValidTriangle( 3.0, 4.0, 5.0 )
    puts( valid.toString() )

    I64 clamped = clamp( 150, 0, 100 )
    puts( clamped.toString() )

    Boolean invalid = isValidTriangle( 1.0, 2.0, 10.0 )
    puts( invalid.toString() )

    Boolean notLeap = isLeapYear( 1900 )
    puts( notLeap.toString() )

    return 0
```

This example demonstrates boolean return values from predicate functions, multi-condition validation with `and`/`or`, control flow with integer comparisons, loop termination via boolean conditions, and string conversion of boolean results — all compiled to `i1` values with `CreateAnd`/`CreateOr`/`CreateICmpEQ` instructions and no OOP wrapper overhead.
