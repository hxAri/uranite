# Type Identity

Uranite determines whether two types represent the same logical entity through a multi-tier identity system built on fully-qualified names. Every type carries a `qualified` field — a globally unique string following the `package.module.TypeName` convention — that serves as the authoritative identity key. When qualified names are available on both operands, they are compared directly; when absent (primitives, error-recovery types, or monomorphized generics whose qualified name was not yet propagated), the system falls back to package+name comparison, then short name comparison. This identity system underpins all type equality checks in the compiler: the `Type::equals` virtual method used for exact-match detection in `isAssignable`, the `typeIdentityMatch` static helper used during inheritance and interface traversal, the `is` keyword for runtime identity/null checks, the type registry's dual-map lookup and registration, and qualified name construction during the two-pass registration phase.

This document covers the `Type` base struct and its `Kind` enumeration, the three fields that compose type identity (`name`, `package`, `qualified`), the `Type::equals` virtual method with its three-tier resolution strategy, the `typeIdentityMatch` static helper, qualified name construction in `registerTypeDeclaration`, the `Registry` dual-map architecture (`userTypesType` and `primitivesTypes`) with `lookupType` three-tier resolution, the `registerType` method and its automatic alias creation, the `is` keyword syntax and its compilation to pointer equality via `CompareEqual`, and how type identity differs from type compatibility.

---

## Table of Contents

- [Overview](#overview)
- [Type Base Struct](#type-base-struct)
  - [Kind Enumeration](#kind-enumeration)
  - [Identity Fields](#identity-fields)
  - [Helper Predicates](#helper-predicates)
- [Type::equals — Structural Identity](#typeequals--structural-identity)
- [typeIdentityMatch — Lightweight Identity](#typeidentitymatch--lightweight-identity)
- [Qualified Name Construction](#qualified-name-construction)
  - [The buildQualified Lambda](#the-buildqualified-lambda)
  - [Registration Per Declaration Kind](#registration-per-declaration-kind)
  - [Nested Type Qualified Names](#nested-type-qualified-names)
- [Type Registry Architecture](#type-registry-architecture)
  - [Dual-Map Design](#dual-map-design)
  - [Primitive Type Initialization](#primitive-type-initialization)
  - [lookupType — Three-Tier Resolution](#lookuptype--three-tier-resolution)
  - [registerType — Registration and Aliasing](#registertype--registration-and-aliasing)
- [The `is` Keyword — Runtime Identity](#the-is-keyword--runtime-identity)
  - [Syntax](#syntax)
  - [Parsing](#parsing)
  - [Semantic Analysis](#semantic-analysis)
  - [MIR Lowering](#mir-lowering)
  - [MIR Codegen](#mir-codegen)
- [Identity vs. Compatibility vs. Equality](#identity-vs-compatibility-vs-equality)
- [Examples](#examples)
  - [Identity Checks with None](#identity-checks-with-none)
  - [Qualified Name Resolution](#qualified-name-resolution)
  - [Type Registry Lookup](#type-registry-lookup)

---

## Overview

Uranite distinguishes three levels of type comparison:

| Level | Mechanism | Question Answered |
|---|---|---|
| **Type identity** | `Type::equals`, `typeIdentityMatch` | Are these the same logical type? |
| **Type compatibility** | `Registry::isAssignable` | Can this value be stored/passed as that type? |
| **Value equality** | `==` operator, `Equatable` interface | Do these two values have the same content? |

Type identity is the strictest — it requires the same type definition, not just compatible structure. `I32` and `I64` are compatible (assignable) but not identical. `I64` (the OOP wrapper from `uranite.language.i64`) and `i64` (the primitive from `uranite.builtin`) are distinct types with different identity but are compatible via primitive-OOP bridging.

---

## Type Base Struct

### Kind Enumeration

Every type in Uranite carries a `Kind` tag defined in `src/uranite/semantic/typeref.hpp:43-70`:

```cpp
enum class Kind {
    Array,
    Bool,
    Callable,
    Char,
    Class,
    Enum,
    Error,
    Float,
    Function,
    Future,
    Generator,
    GenericParameter,
    Integer,
    Interface,
    Meta,
    None,
    Optional,
    Pointer,
    Reference,
    String,
    Struct,
    Trait,
    Tuple,
    Union,
    Unresolved,
    Void
};
```

The `Kind` tag is the first discriminator in identity checks — two types with different kinds are never identical (with one exception: `typeIdentityMatch` does not check kind, allowing cross-kind comparisons during interface traversal).

26 distinct kinds cover the full type system. `Error` is a sentinel for error recovery. `Unresolved` marks types not yet resolved during analysis. `None` represents the absence of a value, distinct from `Void` (which represents no return type).

### Identity Fields

The `Type` base struct carries three fields that compose identity, defined in `src/uranite/semantic/typeref.hpp:73-82`:

```cpp
Kind kind;
std::string name;
std::string package;
std::string qualified;
```

| Field | Description | Example |
|---|---|---|
| `kind` | Discriminator tag | `Kind::Class` |
| `name` | Short identifier | `"I64"` |
| `package` | Module package path | `"uranite.language.i64"` |
| `qualified` | Fully-qualified name (authoritative key) | `"uranite.language.i64.I64"` |

The constructor defaults `qualified` to `name` when no qualified name is provided:

```cpp
Type( Kind kind, const std::string& name,
      const std::string& package = "",
      const std::string& qualified = "" )
    : kind( kind ), name( name ), package( package ),
      qualified( qualified.empty() ? name : qualified ) {
}
```

This means primitives created without explicit qualified names (e.g., `Type(Kind::Bool, "bool")`) will have `qualified == "bool"` — identical to `name`. The `registerTypeDeclaration` phase explicitly sets `qualified` to the full `package.module.TypeName` form for user-defined types.

### Helper Predicates

The `Type` struct provides helper predicates that check identity by kind and/or qualified name:

```cpp
bool isBool() const {
    return this->kind == Kind::Bool ||
           this->qualified == qualname::Boolean;
}
bool isFloatingPoint() const {
    return this->kind == Kind::Float ||
           qualname::isFloatOop( this->qualified );
}
bool isIntegral() const {
    return this->kind == Kind::Integer ||
           qualname::isIntegerOop( this->qualified );
}
bool isVoid() const {
    return this->kind == Kind::Void ||
           this->qualified == qualname::Void ||
           this->qualified == qualname::PrimVoid;
}
bool isNone() const {
    return this->kind == Kind::None;
}
bool isPrimitive() const {
    return this->kind == Kind::Void ||
           this->kind == Kind::Bool ||
           this->kind == Kind::Integer ||
           this->kind == Kind::Float ||
           this->kind == Kind::Char ||
           this->kind == Kind::String;
}
```

These predicates bridge the primitive-OOP duality: `isBool()` returns `true` for both the primitive `bool` (kind `Bool`) and the OOP wrapper `Boolean` (kind `Class`, qualified `"uranite.language.boolean.Boolean"`). `isIntegral()` returns `true` for both `i64` (kind `Integer`) and `I64`/`Int`/`Long` (kind `Class`, qualified names in the `integerOopQualified` set).

---

## Type::equals — Structural Identity

The virtual `equals` method in `src/uranite/semantic/typeref.hpp:109-120` performs three-tier identity resolution:

```cpp
virtual bool equals( const std::shared_ptr<Type>& other ) const {
    if( other == nullptr || this->kind != other->kind ) {
        return false;
    }
    if( this->qualified.empty() == false &&
        other->qualified.empty() == false ) {
        return this->qualified == other->qualified;
    }
    if( this->package.empty() == false &&
        other->package.empty() == false ) {
        return this->package == other->package &&
               this->name == other->name;
    }
    return this->name == other->name;
}
```

**Tier 1 — Kind gate**: If the other type is null or has a different `kind`, the types are not identical. This prevents a `Class` kind and an `Interface` kind from ever being `equals`, even if they share a name.

**Tier 2 — Qualified name**: If both types have non-empty `qualified` fields, string-compare them. This is the authoritative path. `"uranite.language.i64.I64"` and `"uranite.language.i32.I32"` are different types even though both have kind `Class`.

**Tier 3 — Package + name**: If qualified names are absent but packages are set, compare both fields. This handles transitional states where `qualified` has not been propagated yet.

**Tier 4 — Short name only**: When neither qualified name nor package is available, compare short names. This is the fallback for error-recovery types, freshly created primitive types, and certain monomorphized generics.

The method is `virtual` — derived types could override it for structural comparison (e.g., `ArrayType` comparing element types), though the current implementation uses the base `equals` for all type kinds.

---

## typeIdentityMatch — Lightweight Identity

The `typeIdentityMatch` static helper in `src/uranite/semantic/typeref.cpp:103-108`:

```cpp
static bool typeIdentityMatch(
    const TypeSharedPointer& left,
    const TypeSharedPointer& right ) {
    if( left->qualified.empty() == false &&
        right->qualified.empty() == false ) {
        return left->qualified == right->qualified;
    }
    return left->name == right->name;
}
```

Two-tier only — no `kind` check, no `package` check. This is intentional: `typeIdentityMatch` is used inside `isAssignable` for comparing types during inheritance chain walking and interface conformance checking, where the types being compared may have different `Kind` values (e.g., a `ClassType` base class pointer matched against an `InterfaceType` target).

The lack of a `kind` gate means `typeIdentityMatch("Foo", Class)` matches `typeIdentityMatch("Foo", Interface)` if both have the same qualified name. This is correct for the contexts where it is used — the caller has already validated the structural relationship.

---

## Qualified Name Construction

### The buildQualified Lambda

During the registration phase, `registerTypeDeclaration` in `src/uranite/semantic/analyzer.cpp:4604-4616` constructs qualified names using a lambda:

```cpp
std::function<std::string( const std::string& )>
    buildQualified = [&]( const std::string& typeName )
        -> std::string {
    if( parentQualified.empty() == false ) {
        return fmt::format( "{}.{}", parentQualified, typeName );
    }
    if( this->currentPackageName.empty() == false ) {
        return fmt::format( "{}.{}",
            this->currentPackageName, typeName );
    }
    return typeName;
};
```

The qualified name is constructed by joining:
1. The parent qualified name (for nested types) — or the current package name (for top-level types).
2. The type's short name.

For a class `Point` in package `myapp.geometry`, the qualified name becomes `"myapp.geometry.Point"`. For a nested class `Iterator` inside `ArrayList` in package `uranite.collection.array-list`, the qualified name becomes `"uranite.collection.array-list.ArrayList.Iterator"`.

### Registration Per Declaration Kind

Each declaration kind (Class, Struct, Enum, Interface, Trait) follows the same pattern in `registerTypeDeclaration`:

1. Build the qualified name via `buildQualified(declarationName)`.
2. Look up any existing registration via `lookupType(declarationName)`.
3. If already registered (from an imported module), update the `astDeclaration` pointer and propagate qualified/package fields if empty.
4. If not registered, create a new type struct, set all identity fields, register via `registerType`.

For classes (lines 4622-4657):

```cpp
ClassTypeSharedPointer classType =
    std::make_shared<ClassType>( classDeclaration.name );
classType->astDeclaration = &classDeclaration;
classType->isAbstract = classDeclaration.isAbstract;
classType->isFinal = classDeclaration.isFinal;
classType->isReadonly = classDeclaration.isReadonly;
classType->package = this->currentPackageName;
classType->qualified = thisQualified;
for( auto& genericParameter :
     classDeclaration.genericParameters ) {
    classType->genericParameters.push_back(
        std::make_shared<GenericParameterType>(
            genericParameter->name ) );
}
this->typeRegistry.registerType(
    classDeclaration.name, classType );
```

The same pattern applies to Struct (lines 4659-4688), Enum (lines 4690-4709), Interface (lines 4711-4737), and Trait (lines 4739+).

### Nested Type Qualified Names

Nested declarations receive their parent's qualified name as prefix:

```cpp
for( ast::nodes::DeclarationSharedPointer& nestedDeclaration :
     classDeclaration.nestedDeclarations ) {
    this->registerTypeDeclaration(
        nestedDeclaration, thisQualified );
}
```

If `ArrayList` has qualified name `"uranite.collection.array-list.ArrayList"`, a nested class `ArrayListIterator` would get qualified name `"uranite.collection.array-list.ArrayList.ArrayListIterator"`.

---

## Type Registry Architecture

### Dual-Map Design

The `Registry` class in `src/uranite/semantic/typeref.hpp:1121-1470` maintains two separate type maps:

```cpp
std::unordered_map<std::string, TypeSharedPointer> primitivesTypes;
std::unordered_map<std::string, TypeSharedPointer> userTypesType;
std::unordered_map<std::string, std::string> typeAliases;
```

| Map | Content | Populated |
|---|---|---|
| `primitivesTypes` | Built-in primitive and OOP wrapper types | Constructor |
| `userTypesType` | User-defined and imported types | `registerType` calls |
| `typeAliases` | Short name to qualified name mappings | `registerType`, `registerAlias` |

`userTypesType` is checked first during lookup, `primitivesTypes` as fallback. This allows user-defined types to shadow primitives.

### Primitive Type Initialization

The `Registry` constructor in `src/uranite/semantic/typeref.cpp:27-91` initializes all built-in types with explicit qualified names from `qualnames.hpp`:

```cpp
this->integer64Type =
    std::make_shared<IntegerType>( 64, true );
this->integer64Type->package = qualname::primitives::Package;
this->integer64Type->qualified = qualname::primitives::I64;
```

The `primitivesTypes` map maps OOP wrapper short names to their corresponding primitive type objects:

```cpp
this->primitivesTypes = {
    { "Boolean", this->booleanType },
    { "Byte", this->unsigned8Type },
    { "Char", this->charType },
    { "Double", this->float64Type },
    { "F32", this->float32Type },
    { "F64", this->float64Type },
    { "Float", this->float64Type },
    { "I16", this->integer16Type },
    { "I32", this->integer32Type },
    { "I64", this->integer64Type },
    { "I8", this->integer8Type },
    { "Int", this->integer64Type },
    { "Integer", this->integer32Type },
    { "Long", this->integer64Type },
    { "NoneType", this->voidType },
    { "String", this->stringType },
    { "U16", this->unsigned16Type },
    { "U32", this->unsigned32Type },
    { "U64", this->unsigned64Type },
    { "U8", this->unsigned8Type },
    { "UInt", this->unsigned64Type },
    { "Void", this->voidType }
};
```

Note that `"Int"`, `"Long"`, and `"I64"` all map to the same `integer64Type` instance. `"Float"`, `"Double"`, and `"F64"` all map to the same `float64Type` instance. This means identity comparison via `equals` treats them as identical (same pointer or same qualified name).

### lookupType — Three-Tier Resolution

The `lookupType` method in `src/uranite/semantic/typeref.cpp:593-606`:

```cpp
TypeSharedPointer Registry::lookupType(
    const std::string& name ) const {
    auto typeIterator = this->userTypesType.find( name );
    if( typeIterator != this->userTypesType.end() ) {
        return typeIterator->second;
    }
    auto aliasIterator = this->typeAliases.find( name );
    if( aliasIterator != this->typeAliases.end() ) {
        typeIterator =
            this->userTypesType.find( aliasIterator->second );
        if( typeIterator != this->userTypesType.end() ) {
            return typeIterator->second;
        }
    }
    return this->lookupPrimitive( name );
}
```

Resolution order:

1. **User types map** — direct lookup by name.
2. **Alias resolution** — resolve alias to qualified name, then look up the qualified name in user types.
3. **Primitive types map** — fallback for built-in type names.

This three-tier approach ensures that:
- User-defined types shadow built-ins with the same name.
- Aliases created by `registerType` (short name to qualified name) work transparently.
- OOP wrapper names (`"I64"`, `"String"`) resolve even if not explicitly imported.

### registerType — Registration and Aliasing

The `registerType` method in `src/uranite/semantic/typeref.cpp:652-658`:

```cpp
void Registry::registerType(
    const std::string& name, TypeSharedPointer type ) {
    this->userTypesType[name] = type;
    if( type->qualified.empty() == false &&
        type->qualified != name ) {
        this->userTypesType[type->qualified] = type;
        this->typeAliases[name] = type->qualified;
    }
}
```

When a type has a qualified name that differs from its short name, `registerType` performs three operations:

1. **Register by short name**: `userTypesType["Point"] = pointType`.
2. **Register by qualified name**: `userTypesType["myapp.geometry.Point"] = pointType`.
3. **Create alias**: `typeAliases["Point"] = "myapp.geometry.Point"`.

Both entries in `userTypesType` point to the same `TypeSharedPointer` instance. This means `lookupType("Point")` and `lookupType("myapp.geometry.Point")` return the exact same object — pointer identity, not just value identity.

---

## The `is` Keyword — Runtime Identity

### Syntax

The `is` keyword performs identity comparison — pointer equality for reference types, value equality for primitives, and null checks for optional types:

```
value is None
value is not None
objectA is objectB
```

The `is not` form is the negated variant. There is no `is` with type operands — `is` compares values, not types (unlike Python's `isinstance`).

### Parsing

In `src/uranite/parser/parser.cpp:124`, `is` has precedence 7 (same as comparison operators `==`, `!=`, `<`, `>`):

```cpp
case token::Type::KeywordIs:
    return 7;
```

The parser handles `is not` as a two-token sequence in `src/uranite/parser/parser.cpp:2185-2194`:

```cpp
if( kind == token::Type::KeywordIs &&
    this->current().type == token::Type::KeywordNot ) {
    isNegated = true;
    this->advance();
}
int nextMinPrecedence =
    ( kind == token::Type::Power ) ? precedence : precedence + 1;
ast::nodes::ExpressionSharedPointer right =
    this->parsePrecedenceExpression( nextMinPrecedence );
left = std::make_shared<ast::nodes::BinaryExpression>(
    kind, left, right, source );
if( isNegated ) {
    left = std::make_shared<ast::nodes::UnaryExpression>(
        token::Type::KeywordNot, left, true, source );
}
```

The parser creates a `BinaryExpression` with `KeywordIs` as the operation. For `is not`, it wraps the binary expression in a `UnaryExpression` with `KeywordNot` — the negation is applied after the identity check, not as a separate operator.

### Semantic Analysis

In `src/uranite/semantic/analyzer.cpp:1211-1213`:

```cpp
case token::Type::KeywordIs: {
    return this->typeRegistry.getBool();
}
```

No type compatibility check — `is` always produces `Bool` regardless of operand types. Any two values can be compared with `is`.

### MIR Lowering

In `src/uranite/ir/mir/lowering.cpp:3368`:

```cpp
case token::Type::KeywordIs:
    instructionKind = MIRInstructionKind::CompareEqual;
    break;
```

The `is` keyword maps directly to `CompareEqual` — the same MIR instruction as `==`. At the MIR level, identity comparison and equality comparison are the same operation.

### MIR Codegen

The `generateComparison` method in `src/uranite/ir/mir/codegen.cpp:2170` handles `CompareEqual`:

For **integer operands**: `CreateICmpEQ` — exact bitwise comparison.

For **floating-point operands**: `CreateFCmpOEQ` — ordered floating-point equality.

For **pointer operands** (the common case for `is` with reference types): both operands are pointers, and `CreateICmpEQ` on the pointer values checks whether they point to the same object.

For **string-like operands**: a special path detects when both operands might be strings (i64 values above 4096, the page threshold). It generates a branch: if both values are above the page threshold (likely pointers to string data), call `strcmp`; otherwise fall back to integer comparison. This handles the dual representation where small integers and string pointers share the `i64` type:

```cpp
llvm::Value* bothAbove = this->irBuilder.CreateAnd(
    leftAbove, rightAbove, "eq.bothptr" );
// Branch: bothAbove ? strcmp(leftPtr, rightPtr) : ICmpEQ
```

For `is` with `None`, the right operand is typically a null pointer or zero value, and the comparison reduces to `CreateICmpEQ(value, 0)` — a null pointer check.

---

## Identity vs. Compatibility vs. Equality

| Aspect | Type Identity | Type Compatibility | Value Equality |
|---|---|---|---|
| Mechanism | `Type::equals`, `typeIdentityMatch` | `Registry::isAssignable` | `==` operator |
| Level | Compile-time type system | Compile-time type system | Runtime value comparison |
| Relationship | Symmetric | Directional (`target` ← `source`) | Symmetric |
| `I32` vs `I64` | Not identical | Compatible (both directions) | N/A (different types) |
| `I64` prim vs `I64` OOP | Not identical (different Kind) | Compatible (bridging) | N/A (different types) |
| `Int` vs `I64` OOP | Not identical (different qualified) | Compatible (both map to i64) | N/A |
| `?I64` vs `I64` | Not identical (different Kind) | Compatible (optional unwrap) | N/A |
| `ArrayList<I64>` vs `ArrayList` | Identical (AST declaration match) | Compatible | N/A |
| `is` keyword | Runtime pointer comparison | N/A | Identity, not content |
| `==` operator | N/A | N/A | Content via `Equatable` |

Key distinctions:
- **Identity is strict**: Two types are identical only if they represent the exact same type definition.
- **Compatibility is permissive**: Many non-identical types are compatible (numeric widening, interface conformance, optional wrapping).
- **`is` checks pointer identity**: `a is b` asks "do these two variables point to the same object in memory?", not "do they have the same value?".
- **`==` checks value equality**: `a == b` dispatches to the `Equatable.equals` method, which compares content.

---

## Examples

### Identity Checks with None

Using `is` for null/None checking on optional values:

```
package examples

from uranite.io.console import puts

public function findUser(
    String username) -> ?String:
    if username == "admin":
        return "Administrator"
    return None

public function main() -> I32:
    ?String result = findUser("admin")

    if result is not None:
        puts("Found: " + result)

    ?String missing = findUser("unknown")

    if missing is None:
        puts("User not found")

    return 0
```

**Identity trace**:
1. `result is not None`: Parser creates `BinaryExpression(KeywordIs, result, None)`, then wraps in `UnaryExpression(KeywordNot, ...)`. Semantic analysis returns `Bool`. MIR lowering produces `CompareEqual` instruction. MIR codegen emits `ICmpEQ(resultValue, 0)` — checking if the optional's value pointer is null. The `KeywordNot` wrapper produces `Xor(result, true)` to negate.
2. `missing is None`: Same path without the negation wrapper. `ICmpEQ(missingValue, 0)` returns `true` when the optional holds no value.

### Qualified Name Resolution

How type identity resolves across modules:

```
package myapp.models

public class User:
    public String displayName
    public I64 userId

    public function User(
        self, String displayName, I64 userId) -> Void:
        self.displayName = displayName
        self.userId = userId
```

```
package myapp.services

from myapp.models import User

public function createUser(
    String displayName, I64 userId) -> User:
    return new User(displayName, userId)

public function processUser(User targetUser) -> Void:
    User localRef = targetUser
```

**Identity trace**:
1. `User` in `myapp.models` gets qualified name `"myapp.models.User"` via `buildQualified("User")` with `currentPackageName = "myapp.models"`.
2. `registerType("User", userType)` creates entries: `userTypesType["User"] = userType`, `userTypesType["myapp.models.User"] = userType`, `typeAliases["User"] = "myapp.models.User"`.
3. In `myapp.services`, the `import User` statement imports the type. `lookupType("User")` finds it in `userTypesType`.
4. `User localRef = targetUser`: Both have type `User` with qualified `"myapp.models.User"`. `isAssignable` calls `target->equals(source)` — both `kind == Class`, both `qualified == "myapp.models.User"` — `true` via tier 2.

### Type Registry Lookup

How the three-tier lookup resolves different name forms:

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 explicitInt = 42
    Int aliasedInt = 100
    i64 primitiveInt = 200

    puts(explicitInt.toString())
    return 0
```

**Lookup trace**:
1. `I64`: `lookupType("I64")` — checks `userTypesType` first. If `I64` OOP wrapper was imported from `uranite.language.i64`, finds it there. If not imported, falls through to `lookupPrimitive("I64")` which returns the `integer64Type` instance (qualified `"uranite.builtin.i64"`).
2. `Int`: `lookupType("Int")` — `primitivesTypes["Int"]` returns `integer64Type`. Same instance as `I64` primitive lookup. `Int.equals(I64)` — both have kind `Integer`, both qualified `"uranite.builtin.i64"` — identical.
3. `i64` in type position: parsed as a type annotation, resolved via `resolveType` which maps the lowercase token to `IntegerType(64, true)` — the same `integer64Type` instance.
4. When the `I64` OOP wrapper class is imported (via `from uranite.language.i64 import I64`), `lookupType("I64")` returns the class instance with qualified `"uranite.language.i64.I64"` — this is a different type identity from the primitive `"uranite.builtin.i64"`, though they are compatible via primitive-OOP bridging in `isAssignable`.
