# Boolean and None Literals

Uranite provides three keyword-driven literal values: `True`, `False`, and `None`. Boolean literals represent truth values and map to LLVM `i1` constants. `None` represents the absence of a value and maps to an LLVM null pointer. All three are reserved keywords in the token registry — they are recognized by the lexer during identifier scanning and emitted as dedicated keyword tokens, not as general identifiers.

---

## Table of Contents

- [Overview](#overview)
- [Boolean Literals](#boolean-literals)
  - [Syntax and Casing](#syntax-and-casing)
  - [Lexer Recognition](#lexer-recognition)
  - [Parser Construction](#parser-construction)
  - [Semantic Type Assignment](#semantic-type-assignment)
  - [HIR Representation](#hir-representation)
  - [MIR Lowering](#mir-lowering)
  - [LLVM Code Generation](#llvm-code-generation)
- [Logical Operators](#logical-operators)
  - [The "and" Operator](#the-and-operator)
  - [The "or" Operator](#the-or-operator)
  - [The "not" Operator](#the-not-operator)
  - [Truthiness Coercion](#truthiness-coercion)
  - [Short-Circuit Semantics](#short-circuit-semantics)
- [The Boolean OOP Wrapper](#the-boolean-oop-wrapper)
  - [Class Structure](#class-structure)
  - [Methods](#methods)
- [None Literal](#none-literal)
  - [Syntax and Semantics](#syntax-and-semantics)
  - [Lexer Recognition](#none-lexer-recognition)
  - [Parser Construction](#none-parser-construction)
  - [Semantic Type Assignment](#none-semantic-type-assignment)
  - [HIR Representation](#none-hir-representation)
  - [MIR Lowering](#none-mir-lowering)
  - [LLVM Code Generation](#none-llvm-code-generation)
- [None vs Null Pointers](#none-vs-null-pointers)
- [Optional Types](#optional-types)
  - [The Optional Type Wrapper](#the-optional-type-wrapper)
  - [None Comparison](#none-comparison)
- [Type Summary](#type-summary)
- [Examples](#examples)
  - [Boolean Literals](#boolean-literal-examples)
  - [Logical Operations](#logical-operation-examples)
  - [None Handling](#none-handling-examples)
  - [Practical Usage](#practical-usage)

---

## Overview

| Literal | Token Type | AST Node | Semantic Type | LLVM IR |
|---|---|---|---|---|
| `True` | `KeywordTrue` | `BoolLiteralExpression(true)` | `Boolean` | `i1 1` |
| `False` | `KeywordFalse` | `BoolLiteralExpression(false)` | `Boolean` | `i1 0` |
| `None` | `KeywordNone` | `NoneLiteralExpression` | `None` | `null` (opaque pointer) |

All three are PascalCase keywords registered in the token keymaps hash table. The lexer's `readIdentifierOrKeyword()` method recognizes them during scanning — they are never treated as user identifiers.

---

## Boolean Literals

### Syntax and Casing

Boolean literals use PascalCase: `True` and `False`. This is mandatory — `true`, `false`, `TRUE`, and `FALSE` are not recognized as boolean literals. Lowercase `true` and `false` are parsed as regular identifiers, which will fail during semantic analysis if no such variable exists in scope.

```uranite
Boolean flag = True
Boolean empty = False
```

Uranite follows Python's casing convention for boolean literals, not C/Java's lowercase convention. This is consistent with the PascalCase convention used for `None` and type names.

### Lexer Recognition

The lexer does not have dedicated scanning logic for boolean literals. Instead, `True` and `False` are entries in the keymaps hash table:

| Key | Token Type |
|---|---|
| `"True"` | `KeywordTrue` |
| `"False"` | `KeywordFalse` |

When `readIdentifierOrKeyword()` scans an identifier, it looks up the result in the keymaps table. If the identifier matches "True" or "False", the token type is set to `KeywordTrue` or `KeywordFalse` respectively. No special-case code is needed — the hash table handles recognition in O(1).

### Parser Construction

The parser handles `KeywordTrue` and `KeywordFalse` tokens in the expression dispatch switch:

**For `KeywordFalse`:**
1. Advance past the token.
2. Return `BoolLiteralExpression(false, source)`.

**For `KeywordTrue`:**
1. Advance past the token.
2. Return `BoolLiteralExpression(true, source)`.

The `BoolLiteralExpression` AST node stores a single `bool value` field — a C++ `bool` that is either `true` or `false`.

### Semantic Type Assignment

The semantic analyzer processes `BooleanLiteral` nodes:

1. Look up the `Boolean` class type via `typeRegistry.lookupType("Boolean")` using `qualname::classes::boolean::Name`.
2. If found, use the class type (OOP wrapper `uranite.language.boolean.Boolean`).
3. If not found (fallback), use the primitive boolean type `typeRegistry.getBool()` (primitive `uranite.builtin.bool`).

This dual-lookup pattern — OOP wrapper first, primitive fallback second — is identical to the pattern used for `String`, `Char`, and all other literal types.

### HIR Representation

HIR preserves the boolean value in an `HIRBooleanLiteral` node:

| Field | Type | Description |
|---|---|---|
| `booleanValue` | `bool` | `true` or `false`. |
| `resolvedType` | `TypeSharedPointer` | The `Boolean` type from semantic analysis. |
| `sourceLocation` | `SourceSharedPointer` | Source file position. |

HIR lowering extracts `boolLiteral.value` from the AST node and passes it to the `HIRBooleanLiteral` constructor.

### MIR Lowering

MIR lowering creates a `ConstantBoolean` instruction:

1. Extract `booleanValue` from the `HIRBooleanLiteral` node.
2. Create a `MIRInstruction` with kind `ConstantBoolean`.
3. Set `booleanConstantValue` to the boolean value.
4. Set `operandType` to the resolved `Boolean` type.
5. Allocate a variable named `_const_bool` to hold the result.
6. Emit the instruction.

The `booleanConstantValue` field on `MIRInstruction` is typed as C++ `bool` and defaults to `false`.

### LLVM Code Generation

The `generateConstantBoolean()` method emits the LLVM IR:

```
llvm::Value* constValue = llvm::ConstantInt::get(
    llvm::Type::getInt1Ty( this->llvmContext ),
    instruction.booleanConstantValue ? 1 : 0
);
```

Boolean values are represented as LLVM `i1` (1-bit integer) constants:

| Uranite | LLVM Constant |
|---|---|
| `True` | `i1 1` |
| `False` | `i1 0` |

The `i1` type is the smallest integer type in LLVM IR. It can hold exactly two values: 0 and 1. This maps directly to the two boolean states.

---

## Logical Operators

Uranite uses English-word logical operators: `and`, `or`, and `not`. The symbolic equivalents (`&&`, `||`, `!`) are not valid syntax.

### The "and" Operator

The `and` operator performs logical conjunction. Both operands are evaluated, and the result is `True` only if both are `True`:

```uranite
Boolean result = True and False
```

At the LLVM level, `and` emits a `CreateAnd` instruction on `i1` operands:

```
%and = and i1 %lhs, %rhs
```

### The "or" Operator

The `or` operator performs logical disjunction. The result is `True` if either operand is `True`:

```uranite
Boolean result = False or True
```

At the LLVM level, `or` emits a `CreateOr` instruction on `i1` operands:

```
%or = or i1 %lhs, %rhs
```

### The "not" Operator

The `not` operator performs logical negation. It is a unary prefix operator that inverts the truth value:

```uranite
Boolean result = not True
```

The codegen for `not` handles three operand types:

| Operand Type | LLVM Operation | Description |
|---|---|---|
| `i1` (boolean) | `CreateNot` | Direct bitwise NOT on 1-bit value. Flips 0 to 1 and 1 to 0. |
| Integer (`i8`-`i64`) | `CreateICmpEQ(operand, 0)` | Compare against zero. Non-zero becomes `False`, zero becomes `True`. |
| Pointer | `CreateICmpEQ(operand, null)` | Compare against null pointer. Non-null becomes `False`, null becomes `True`. |

This type-aware dispatch means `not` works on non-boolean values by applying truthiness rules.

### Truthiness Coercion

When `and` or `or` receive non-boolean integer operands, the codegen automatically coerces them to `i1` before applying the logical operation:

- For non-`i1` integer operands: `CreateICmpNE(operand, 0)` — any non-zero value is `True`.

This coercion is applied independently to each operand. If one operand is already `i1`, only the other is coerced.

### Short-Circuit Semantics

At the MIR instruction level, `LogicalAnd` and `LogicalOr` are two-operand instructions. Both operands are lowered before the logical instruction is emitted. The current codegen evaluates both sides — the `CreateAnd`/`CreateOr` LLVM instructions are bitwise operations on `i1`, not control-flow-based short circuits.

---

## The Boolean OOP Wrapper

### Class Structure

The `Boolean` class (`uranite.language.boolean.Boolean`) is a `final` class — it cannot be subclassed. It wraps a primitive `Boolean` value:

```uranite
final class Boolean:
    protect Boolean value
    public function Boolean( self, Boolean value ) -> Void
```

The `value` field is `protect`, accessible only within the class. The constructor stores the truth value.

### Methods

| Method | Signature | Description |
|---|---|---|
| `getValue` | `() -> Boolean` | Return the underlying truth value. |
| `toString` | `() -> String` | Return `"True"` if the value is truthy, `"False"` otherwise. |
| `negate` | `() -> Boolean` | Return a new `Boolean` with the opposite truth value via `not self.value`. |
| `logicalAnd` | `(Boolean other) -> Boolean` | Return a new `Boolean` representing `self.value and other.value`. |
| `logicalOr` | `(Boolean other) -> Boolean` | Return a new `Boolean` representing `self.value or other.value`. |

All methods return new `Boolean` instances — the original is never modified. The `toString()` method returns the PascalCase strings "True" and "False", matching the literal syntax.

---

## None Literal

### Syntax and Semantics

`None` represents the absence of a value. It is a keyword literal with PascalCase casing, consistent with `True` and `False`:

```uranite
String? name = None
```

`None` is not a value in the traditional sense — it is a sentinel that indicates "no value is present." It is used with optional types (`?T`) to represent the empty state.

### None Lexer Recognition

Like `True` and `False`, `None` is an entry in the keymaps hash table:

| Key | Token Type |
|---|---|
| `"None"` | `KeywordNone` |

The lexer recognizes "None" during identifier scanning and emits a `KeywordNone` token. The identifier "None" cannot be used as a variable name, function name, or type name — it is permanently reserved.

### None Parser Construction

The parser handles `KeywordNone` in the expression dispatch:

1. Advance past the token.
2. Return `NoneLiteralExpression(source)`.

The `NoneLiteralExpression` AST node stores no value — it has only the source location inherited from `Expression`. There is nothing to store because `None` carries no data.

### None Semantic Type Assignment

The semantic analyzer assigns the `None` type directly:

```
expressionType = this->typeRegistry.getNone();
```

Unlike boolean, string, and char literals, `None` does not use the dual-lookup pattern (OOP wrapper then primitive fallback). There is no `None` OOP wrapper class. The type is a singleton created on first access:

```
TypeSharedPointer getNone() const {
    static TypeSharedPointer noneType = std::make_shared<Type>(
        Type::Kind::None, "None"
    );
    return noneType;
}
```

The `None` type has `Kind::None` and is checked via the `isNone()` method on `Type`.

### None HIR Representation

HIR stores `None` in an `HIRNoneLiteral` node:

| Field | Type | Description |
|---|---|---|
| `resolvedType` | `TypeSharedPointer` | The `None` type. |
| `sourceLocation` | `SourceSharedPointer` | Source file position. |

No value field exists — `HIRNoneLiteral` stores only type and location metadata.

### None MIR Lowering

MIR lowering creates a `ConstantNone` instruction:

1. Create a `MIRInstruction` with kind `ConstantNone`.
2. Set `operandType` to the resolved `None` type from the HIR node.
3. Allocate a variable named `_const_none` to hold the result.
4. Emit the instruction.

No constant value field is set — the instruction kind itself conveys the value.

### None LLVM Code Generation

The `generateConstantNone()` method emits a null pointer:

```
llvm::Value* constValue = llvm::ConstantPointerNull::get(
    llvm::PointerType::getUnqual( this->llvmContext )
);
```

`None` is represented as an opaque null pointer (`ptr null` in LLVM IR). `PointerType::getUnqual` creates an unqualified (address-space 0) opaque pointer type, and `ConstantPointerNull::get` creates the null constant for that type.

| Uranite | LLVM Constant |
|---|---|
| `None` | `ptr null` |

---

## None vs Null Pointers

`None` in Uranite is semantically different from null pointers in C or C++:

| Property | Uranite `None` | C/C++ `NULL`/`nullptr` |
|---|---|---|
| Type system integration | Has its own `Type::Kind::None` type. | Typed as `void*` or `nullptr_t`. |
| Assignment | Only assignable to optional types (`?T`). | Assignable to any pointer type. |
| Dereferencing | Cannot be dereferenced without explicit check. | Undefined behavior on dereference. |
| Comparison | Uses `==` / `!=` with None-specific semantic rules. | Pointer comparison. |
| Representation | `ConstantPointerNull` (opaque pointer). | `ConstantPointerNull` or integer 0. |

While the LLVM representation is the same (a null pointer), the semantic analyzer enforces type safety around `None`. The `None` type is compatible with optional types through the type system, preventing accidental null pointer usage on non-optional variables.

---

## Optional Types

### The Optional Type Wrapper

Optional types are declared with the `?` prefix syntax:

```uranite
String? maybeName = None
I64? maybeCount = None
```

The `?T` syntax creates an `OptionalType` wrapping the inner type `T`. The `OptionalType` struct stores the inner type and generates the name `"Optional<T>"`:

| Property | Value |
|---|---|
| Kind | `Type::Kind::Optional` |
| Name | `Optional<T>` (e.g., `Optional<String>`) |
| String representation | `?T` (e.g., `?String`) |
| Inner type | The wrapped `T` |

An optional type can hold either a value of type `T` or `None`.

### None Comparison

The semantic analyzer has special handling for `None` comparisons. When either operand of an equality check (`==` or `!=`) has the `None` type, the analyzer accepts the comparison and returns `Boolean`:

```uranite
if name == None:
    puts( "no name provided" )
```

This special case bypasses the normal type-compatibility check for comparisons. The `isNone()` method on `Type` is used to detect when either side is `None`, and the result type is always `Boolean`.

The `is` keyword can also be used for identity checks with `None`:

```uranite
if name is None:
    puts( "name is absent" )
```

---

## Type Summary

### Primitive Types

| Literal | Kind | Short Name | Qualified Name | LLVM Type | LLVM Width |
|---|---|---|---|---|---|
| `True` / `False` | `Type::Kind::Bool` | `bool` | `uranite.builtin.bool` | `i1` | 1 bit |
| `None` | `Type::Kind::None` | `None` | (no package) | `ptr` | pointer-sized |

### OOP Wrappers

| Literal | Wrapper Class | Qualified Name | Final |
|---|---|---|---|
| `True` / `False` | `Boolean` | `uranite.language.boolean.Boolean` | Yes |
| `None` | (none) | (no wrapper) | N/A |

`None` has no OOP wrapper — it is a type-system concept, not a value that needs methods.

---

## Examples

### Boolean Literal Examples

```uranite
Boolean isActive = True
Boolean isDeleted = False
Boolean isEnabled = True
Boolean isEmpty = False
```

### Logical Operation Examples

```uranite
Boolean both = True and True
Boolean either = False or True
Boolean neither = not True
Boolean complex = ( True or False ) and not False
```

| Expression | Result |
|---|---|
| `True and True` | `True` |
| `True and False` | `False` |
| `False or True` | `True` |
| `False or False` | `False` |
| `not True` | `False` |
| `not False` | `True` |
| `( True or False ) and not False` | `True` |

### None Handling Examples

```uranite
String? name = None
I64? count = None
Boolean? flag = None

String? greeting = "hello"
```

```uranite
if name == None:
    puts( "name is absent" )

if name != None:
    puts( "name is present" )

if name is None:
    puts( "name identity check" )
```

### Practical Usage

```uranite
from uranite.io.console import puts

public function findUser( String username ) -> String?:
    if username == "admin":
        return "Administrator"
    if username == "guest":
        return "Guest User"
    return None

public function greet( String? name ) -> String:
    if name is None:
        return "Hello, stranger!"
    return "Hello, " + name + "!"

public function checkPermissions( Boolean isAdmin, Boolean isActive ) -> Boolean:
    if not isActive:
        return False
    if isAdmin and isActive:
        return True
    return False

public function main() -> I32:
    String? user = findUser( "admin" )
    String message = greet( user )
    puts( message )

    String? unknown = findUser( "nobody" )
    String fallback = greet( unknown )
    puts( fallback )

    Boolean allowed = checkPermissions( True, True )
    Boolean denied = checkPermissions( True, False )

    if allowed:
        puts( "access granted" )
    if not denied:
        puts( "access denied" )

    Boolean result = True and ( not False or True )
    Boolean negated = result.negate().getValue()

    return 0
```

This example demonstrates optional return types with `None`, `None` identity checking via `is`, logical operator chains with `and`/`or`/`not`, boolean method calls on the OOP wrapper (`negate()`, `getValue()`), and conditional branching based on boolean values.
