# Void and None Types

Uranite distinguishes between two forms of "absence": `Void` represents the absence of a return value (a function that produces no result), while `None` represents the absence of a value in a nullable context (an optional that holds nothing). They are separate `Type::Kind` entries, map to different LLVM types, serve different semantic roles, and interact differently with the type system.

This document covers both types in full — their type identities, LLVM representations, semantic roles, assignability rules, comparability behavior, the `is` keyword for None checks, the `Optional<T>` wrapper, truthiness coercion for control flow, and the OOP wrapper classes.

---

## Table of Contents

- [Type Identity](#type-identity)
  - [Void Identity](#void-identity)
  - [None Identity](#none-identity)
- [LLVM Representation](#llvm-representation)
  - [Void Mapping](#void-mapping)
  - [None Mapping](#none-mapping)
  - [Constant Emission](#constant-emission)
- [Semantic Roles](#semantic-roles)
  - [Void as Return Type](#void-as-return-type)
  - [None as Absent Value](#none-as-absent-value)
  - [The Distinction](#the-distinction)
- [Compilation Pipeline](#compilation-pipeline)
  - [Void Pipeline](#void-pipeline)
  - [None Pipeline](#none-pipeline)
- [The Optional Type](#the-optional-type)
  - [OptionalType Structure](#optionaltype-structure)
  - [Optional Syntax](#optional-syntax)
  - [None-to-Optional Assignability](#none-to-optional-assignability)
- [The `is` Keyword](#the-is-keyword)
  - [Identity Comparison](#identity-comparison)
  - [Negated Identity](#negated-identity)
  - [MIR Lowering](#mir-lowering)
  - [LLVM Emission](#llvm-emission)
- [Truthiness and Control Flow](#truthiness-and-control-flow)
  - [Pointer Truthiness](#pointer-truthiness)
  - [None in Conditionals](#none-in-conditionals)
- [Assignability Rules](#assignability-rules)
  - [Void Assignability](#void-assignability)
  - [None Assignability](#none-assignability)
  - [Optional Assignability](#optional-assignability)
- [Comparability Rules](#comparability-rules)
  - [None Comparability](#none-comparability)
  - [Optional Comparability](#optional-comparability)
- [The OOP Wrappers](#the-oop-wrappers)
  - [Void Class](#void-class)
  - [NoneType Class](#nonetype-class)
- [Examples](#examples)
  - [Void Return Functions](#void-return-functions)
  - [None and Optional Values](#none-and-optional-values)
  - [None Checks with `is`](#none-checks-with-is)
  - [Optional Chaining](#optional-chaining)
  - [Practical Usage](#practical-usage)

---

## Type Identity

### Void Identity

| Property | Value |
|---|---|
| Type Name | `void` |
| Kind | `Type::Kind::Void` |
| Qualified Name (primitive) | `uranite.builtin.void` |
| Qualified Name (OOP wrapper) | `uranite.language.void.Void` |
| LLVM Type | `void` |
| Literal | N/A — no Void literal exists |

Void has no runtime value. It exists purely in the type system to mark functions that return nothing.

### None Identity

| Property | Value |
|---|---|
| Type Name | `None` |
| Kind | `Type::Kind::None` |
| Qualified Name (OOP wrapper) | `uranite.language.none.NoneType` |
| LLVM Type | `ptr` (null pointer) |
| Literal | `None` (reserved identifier) |

None is a value — the null pointer. It can be stored in variables, passed as arguments, and compared with `==`, `!=`, and `is`.

---

## LLVM Representation

### Void Mapping

The `Void` type maps to `llvm::Type::getVoidTy(context)` — LLVM's native void type:

```
case semantic::Type::Kind::Void:
    return llvm::Type::getVoidTy(this->llvmContext)
```

Void values cannot be loaded, stored, or passed as arguments. A function with return type `Void` emits a bare `ret void` instruction with no return value.

Void-typed variables are skipped during allocation — `isVoidTy()` checks gate out `AllocateLocal`, `HeapAllocate`, and `StoreVariable` instructions when the allocated type is void:

```
if( variableType->isVoidTy() )
    return;
```

### None Mapping

The `None` type maps to `llvm::PointerType::getUnqual(context)` — an opaque pointer:

```
case semantic::Type::Kind::None:
    return llvm::PointerType::getUnqual(this->llvmContext)
```

At the LLVM level, `None` is the null pointer constant. It has the same type as `String`, class instance pointers, and other reference types — `ptr`. What distinguishes `None` is its value: always `null`.

### Constant Emission

`None` is emitted by `generateConstantNone()`:

```
ConstantPointerNull::get(PointerType::getUnqual(this->llvmContext))
→ setVariableValue(destination, constValue)
```

This produces the LLVM constant `null` — a pointer whose value is zero. There is no corresponding emission function for `Void` because Void has no value to emit.

---

## Semantic Roles

### Void as Return Type

`Void` marks functions that produce no result:

```uranite
public function greet( String name ) -> Void:
    puts( "Hello, " + name )
```

When a function's return type resolves to `Void`:
- The function body may omit the `return` statement entirely.
- If a `return` statement is present, it must have no value: `return` (not `return something`).
- The codegen emits `ret void` at function exit.
- Callers cannot use the function call as a value expression.

`Void` is classified by `isPrimitive()` — it appears in the list: `Kind::Void || Kind::Bool || Kind::Integer || Kind::Float || Kind::Char || Kind::String`.

### None as Absent Value

`None` represents the absence of a value in a nullable context:

```uranite
?String name = None
```

`None` is a reserved identifier (alongside `self`, `True`, and `False`) registered at `qualname::identifier::None`. When the semantic analyzer encounters a `NoneLiteral` AST node, it resolves its type via `typeRegistry.getNone()`:

```
case ast::Node::Kind::NoneLiteral:
    expressionType = this->typeRegistry.getNone()
```

The `getNone()` method returns a lazily-constructed singleton:

```
static TypeSharedPointer noneType = std::make_shared<Type>(Type::Kind::None, "None")
return noneType
```

### The Distinction

| Property | Void | None |
|---|---|---|
| Purpose | No return value | Absent value in nullable slot |
| Has a runtime value? | No | Yes (null pointer) |
| Can be stored in a variable? | No | Yes |
| Can be compared? | No | Yes (`==`, `!=`, `is`) |
| Can be passed as argument? | No | Yes |
| LLVM representation | `void` (no type) | `ptr` (null pointer) |
| Used with Optional? | Yes (assignable to any Optional) | Yes (assignable to any Optional) |

The key distinction: Void is a type-level concept with no value. None is a value-level concept — it is the null pointer constant, typed as `Kind::None`, and can participate in expressions.

---

## Compilation Pipeline

### Void Pipeline

Void does not flow through the expression pipeline. It exists only as a return type annotation:

```
Source:  -> Void
Parser:  VoidType annotation on FunctionDeclaration
Semantic: resolves to typeRegistry.getVoid() → Type(Kind::Void, "void", "uranite.builtin.void")
HIR:     function return type = Void
MIR:     function return type = Void
Codegen: FunctionType::get(voidTy, params, false) → ret void
```

No `ConstantVoid` MIR instruction exists. No `generateConstantVoid()` codegen function exists.

### None Pipeline

None flows through the full expression pipeline as a literal:

```
Source:  None
Lexer:   Identifier token, value = "None"
Parser:  NoneLiteral AST node
Semantic: type = typeRegistry.getNone() → Type(Kind::None, "None")
HIR:     HIRNoneLiteral
MIR:     ConstantNone instruction, destinationVariable = _const_none
Codegen: ConstantPointerNull::get(PointerType::getUnqual(context))
LLVM IR: ptr null
```

---

## The Optional Type

### OptionalType Structure

`OptionalType` wraps any type to make it nullable — capable of holding either a value of the inner type or `None`:

```
struct OptionalType : Type {
    TypeSharedPointer inner;

    OptionalType(TypeSharedPointer inner)
        : Type(Type::Kind::Optional, fmt::format("Optional<{}>", inner->name))
        , inner(std::move(inner)) {}

    std::string toString() const override {
        return "?" + this->inner->toString();
    }
};
```

Key properties:
- Kind is `Type::Kind::Optional`.
- Display name format: `Optional<InnerType>`.
- `toString()` produces shorthand: `?InnerType` (e.g., `?String`, `?I64`).
- Created via `typeRegistry.makeOptional(inner)`.

### Optional Syntax

The `?` prefix denotes an optional type:

```uranite
?String maybeName = "Alice"
?I64 maybeCount = None
?Boolean maybeFlag = None
```

The `?T` syntax is sugar for `Optional<T>` — a type that can hold either a `T` value or `None`.

### None-to-Optional Assignability

`None` (and `Void`) are unconditionally assignable to any `Optional<T>`:

```
if( target->kind == Type::Kind::Optional ) {
    if( source->isVoid() || source->isNone() ) {
        return true;
    }
    ...
}
```

This rule enables the fundamental `?T variable = None` pattern. A non-optional value of type `T` is also assignable to `Optional<T>` — the assignability check unwraps the optional and compares the inner type:

```
OptionalTypeSharedPointer targetOptionalType = std::static_pointer_cast<OptionalType>(target);
return this->isAssignable(targetOptionalType->inner, source);
```

---

## The `is` Keyword

### Identity Comparison

The `is` keyword performs identity comparison — it checks whether two values are the same object (pointer equality), not whether they have equal contents. Its primary use is None checks:

```uranite
?String value = getSomething()
if value is None:
    puts( "No value" )
```

The `is` keyword is a binary operator parsed at the same precedence level as comparison operators. The semantic analyzer returns `Bool` for any `is` expression:

```
case token::Type::KeywordIs:
    return this->typeRegistry.getBool()
```

### Negated Identity

The `is not` construct negates the identity check:

```uranite
if value is not None:
    puts( value )
```

The parser handles `is not` by detecting `KeywordIs` followed by `KeywordNot`:

```
if( kind == token::Type::KeywordIs && this->current().type == token::Type::KeywordNot ) {
    isNegated = true;
    this->advance();
}
```

When negated, the parser wraps the binary expression in a `UnaryExpression` with `KeywordNot`:

```
left = BinaryExpression(KeywordIs, left, right)
left = UnaryExpression(KeywordNot, left, prefix=true)   — wraps the is-check
```

This produces `not (value is None)` semantically.

### MIR Lowering

The `is` operator lowers to a `CompareEqual` MIR instruction — the same instruction used for `==`:

```
case token::Type::KeywordIs:
    instructionKind = MIRInstructionKind::CompareEqual
```

At the MIR level, `is` and `==` are identical. The distinction exists only at the semantic level (the analyzer permits `is` with `None` comparisons that might otherwise fail type checking).

### LLVM Emission

Since `is` lowers to `CompareEqual`, and None is a null pointer, a None check like `value is None` becomes:

```
ICmpEQ(value, ConstantPointerNull)
```

This is a direct pointer equality check — exactly what identity comparison means.

For `is not None`, the `LogicalNot` wrapper produces:

```
ICmpNE(ICmpEQ(value, null), true)   — or equivalently —
ICmpNE(value, ConstantPointerNull)
```

---

## Truthiness and Control Flow

### Pointer Truthiness

The `generateBranchConditional()` function coerces non-boolean values to `i1` for branch conditions. Since `None` maps to `ptr`, it follows the pointer truthiness path:

```
if( conditionValue->getType()->isPointerTy() ) {
    conditionValue = ICmpNE(
        conditionValue,
        ConstantPointerNull::get(cast<PointerType>(conditionValue->getType())),
        "cond.bool"
    )
}
```

A non-null pointer is truthy. A null pointer (None) is falsy.

### None in Conditionals

This truthiness coercion means optional values can be used directly in conditionals without explicit `is not None` checks:

```uranite
?String name = getOptionalName()
if name:
    puts( name )
```

The `if name:` branch is taken when `name` is a non-null pointer (i.e., holds a value). When `name` is `None` (null), the branch is skipped.

The complete truthiness table for pointer/None context:

| Value | LLVM Representation | Truthiness |
|---|---|---|
| Non-null pointer (has value) | `ptr != null` | Truthy |
| `None` (null pointer) | `ptr null` | Falsy |

---

## Assignability Rules

### Void Assignability

```
if( target->isVoid() && source->isVoid() ) {
    return true;
}
```

Void is only assignable to Void. This rule exists for consistency — in practice, Void values are never assigned because they cannot be stored in variables.

The `isVoid()` predicate checks three conditions:

```
bool isVoid() const {
    return this->kind == Kind::Void ||
        this->qualified == qualname::Void ||
        this->qualified == qualname::PrimVoid;
}
```

This matches:
- Primitive `void` type (`Kind::Void`, qualified `"uranite.builtin.void"`)
- OOP wrapper `Void` class (qualified `"uranite.language.void.Void"`)

### None Assignability

None has no special assignability rule in `isAssignable()` beyond its interaction with Optional. None is assignable to:
- `Optional<T>` for any `T` — via the Optional assignability rule (`source->isNone() → true`)
- `Object` — via the universal Object supertype rule
- Another `None` — via identity match

None is **not** directly assignable to concrete types like `String`, `I64`, or `Boolean`. Assigning `None` to a non-optional variable is a type error.

### Optional Assignability

The Optional assignability rules form a small hierarchy:

| Source | Target | Assignable? | Rule |
|---|---|---|---|
| `None` | `?T` | Yes | `source->isNone()` check |
| `Void` | `?T` | Yes | `source->isVoid()` check |
| `T` | `?T` | Yes | Unwrap target, check `isAssignable(inner, source)` |
| `?T` | `?T` | Yes | Unwrap both, check `isAssignable(targetInner, sourceInner)` |
| `?T` | `T` | Yes | Unwrap source, check `isAssignable(target, sourceInner)` |

The bidirectional unwrapping means Optional types are flexible — a `?String` can be passed where a `String` is expected (with the implicit risk of None dereference), and a `String` can be assigned to a `?String` slot.

---

## Comparability Rules

### None Comparability

None is universally comparable — it can be compared with any type:

```
if( x->isNone() || y->isNone() ) {
    return true;
}
```

This allows patterns like `value == None` and `value != None` regardless of `value`'s type. The semantic analyzer also has a special bypass for None comparisons with equality operators:

```
bool isNoneComparison = rightSideType->isNone() || leftSideType->isNone();
if( isEqualityOp && isNoneComparison ) {
    return this->typeRegistry.getBool();
}
```

This bypass runs before the general `isComparable()` check, ensuring None comparisons never trigger "cannot compare types" errors.

### Optional Comparability

Optional types are comparable with None, Void, and their inner types:

```
if( x->kind == Type::Kind::Optional && ( y->isVoid() || y->isNone() ) ) {
    return true;
}
if( y->kind == Type::Kind::Optional && ( x->isVoid() || x->isNone() ) ) {
    return true;
}
```

Optional-to-Optional comparisons unwrap and compare inner types:

```
if( x->kind == Type::Kind::Optional ) {
    OptionalTypeSharedPointer optionalType = std::static_pointer_cast<OptionalType>(x);
    return this->isComparable(optionalType->inner, y);
}
```

---

## The OOP Wrappers

### Void Class

The `Void` class in `stdlibs/language/void.urn`:

```
final class Void

    public function toString( self ) -> String:
        return "Void"
```

A `final` class with no fields. It exists solely as the OOP wrapper for the `void` primitive type. Its only method, `toString()`, returns the string `"Void"`.

The `Void` class is part of the `oopWrapperQualified` set and maps to `primitivesTypes` via:

```
{ qualname::classes::Void::Name, this->voidType }
```

The wrapper mapping in `toLLVMType()` recognizes `Void` and maps class instances named `"Void"` back to `llvm::Type::getVoidTy()`.

### NoneType Class

The `NoneType` class in `stdlibs/language/none.urn`:

```
final class NoneType

    public function toString( self ) -> String:
        return "None"

    public function equals( self, ?NoneType other ) -> Boolean:
        return True
```

A `final` class with no fields. Two methods:

- `toString()` — returns the string `"None"`.
- `equals(other)` — always returns `True`. Since all `None` values are the same null pointer, any two NoneType instances are inherently equal.

The `equals` method accepts `?NoneType` (optional NoneType) as its parameter, allowing both `None` and `NoneType` instances as arguments.

`NoneType` is part of the `oopWrapperQualified` set with qualified name `"uranite.language.none.NoneType"`. In the `primitivesTypes` map, `NoneType` maps to `voidType`:

```
{ qualname::classes::nonetype::Name, this->voidType }
```

---

## Examples

### Void Return Functions

```uranite
from uranite.io.console import puts

public function logMessage( String message ) -> Void:
    puts( "[LOG] " + message )

public function processItems( I64 count ) -> Void:
    I64 index = 0
    while index < count:
        logMessage( "Processing item" )
        index = index + 1

public function main() -> I32:
    logMessage( "Starting" )
    processItems( 3 )
    logMessage( "Done" )
    return 0
```

### None and Optional Values

```uranite
from uranite.io.console import puts

public function findUser( String name ) -> ?String:
    if name == "admin":
        return "Administrator"
    return None

public function main() -> I32:
    ?String admin = findUser( "admin" )
    ?String guest = findUser( "guest" )

    if admin is not None:
        puts( admin )

    if guest is None:
        puts( "Guest not found" )

    return 0
```

### None Checks with `is`

```uranite
from uranite.io.console import puts

public function describe( ?I64 value ) -> String:
    if value is None:
        return "absent"
    return "present"

public function safeAdd( ?I64 left, ?I64 right ) -> ?I64:
    if left is None or right is None:
        return None
    return left + right

public function main() -> I32:
    ?I64 present = 42
    ?I64 absent = None

    puts( describe( present ) )
    puts( describe( absent ) )

    ?I64 sum = safeAdd( present, present )
    if sum is not None:
        puts( "Sum computed" )

    ?I64 noSum = safeAdd( present, absent )
    if noSum is None:
        puts( "Cannot add with None" )

    return 0
```

### Optional Chaining

```uranite
from uranite.io.console import puts

public function getConfig( String key ) -> ?String:
    if key == "host":
        return "localhost"
    if key == "port":
        return "8080"
    return None

public function getPort() -> ?I64:
    ?String portStr = getConfig( "port" )
    if portStr is None:
        return None
    return 8080

public function main() -> I32:
    ?String host = getConfig( "host" )
    ?String missing = getConfig( "database" )

    if host:
        puts( host )

    if missing:
        puts( "Found database config" )
    else:
        puts( "No database config" )

    ?I64 port = getPort()
    if port is not None:
        puts( "Port configured" )

    return 0
```

### Practical Usage

```uranite
from uranite.io.console import puts

public function firstNonNone( ?String first, ?String second, ?String fallback ) -> String:
    if first is not None:
        return first
    if second is not None:
        return second
    if fallback is not None:
        return fallback
    return "default"

public function formatOptional( String label, ?String value ) -> String:
    if value is None:
        return label + ": <none>"
    return label + ": " + value

public function countPresent( ?I64 alpha, ?I64 beta, ?I64 gamma ) -> I64:
    I64 count = 0
    if alpha is not None:
        count = count + 1
    if beta is not None:
        count = count + 1
    if gamma is not None:
        count = count + 1
    return count

public function main() -> I32:
    String result = firstNonNone( None, "backup", "fallback" )
    puts( result )

    puts( formatOptional( "name", "Alice" ) )
    puts( formatOptional( "email", None ) )

    I64 present = countPresent( 1, None, 3 )
    puts( "Present values found" )

    ?String truthy = "hello"
    ?String falsy = None

    if truthy:
        puts( "Truthy branch taken" )
    if not falsy:
        puts( "Falsy branch skipped correctly" )

    return 0
```

This example demonstrates first-non-None selection with nullable parameters, optional value formatting with fallback strings, presence counting across multiple optionals, and truthiness-based branching where non-null pointers are truthy and None is falsy — all compiled to null pointer constants and `ICmpNE`/`ICmpEQ` comparisons with zero runtime overhead beyond the branch instruction itself.
