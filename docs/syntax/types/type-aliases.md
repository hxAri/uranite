# Type Aliases

Uranite supports type aliases through the `type` keyword, allowing programmers to assign new names to existing types. A type alias declaration has the form `type AliasName = TargetType`, where `AliasName` becomes a synonym for `TargetType`. Aliases are transparent — the alias name resolves to the exact same `TypeSharedPointer` as the target type. There is no new type created; the alias is purely a naming convenience. The type registry maintains a `typeAliases` map (`shortName → qualifiedName`) that `lookupType` consults when a direct name lookup fails. Type aliases can also carry generic parameters via `type AliasName<T> = TargetType<T>`, parsed through the same generic parameter infrastructure used by classes and functions. At the compilation pipeline level, type aliases are fully erased during semantic analysis — they do not appear in HIR, MIR, or codegen because every reference to the alias name has already been resolved to the underlying type.

This document covers the `TypeAliasDeclaration` AST node with its fields, type alias syntax with access modifiers and generic parameters, parsing implementation (keyword dispatch, generic parameter parsing, assignment token, target type parsing), semantic analysis in both passes (eager registration in `analyzeModuleRegistration` and full analysis in `analyzeTypeAliasDeclaration`), the type registry alias mechanism (`typeAliases` map, `registerType` auto-aliasing, `registerAlias`, `resolveAlias`, `lookupType` alias fallback), module system integration (import/export of type alias declarations), AST printer output, and practical examples.

---

## Table of Contents

- [Overview](#overview)
- [Type Alias Syntax](#type-alias-syntax)
  - [Basic Aliases](#basic-aliases)
  - [Access Modifiers](#access-modifiers)
  - [Generic Aliases](#generic-aliases)
  - [Complex Target Types](#complex-target-types)
- [AST Representation](#ast-representation)
  - [TypeAliasDeclaration](#typealiasdeclaration)
- [Parsing Implementation](#parsing-implementation)
  - [Declaration Dispatch](#declaration-dispatch)
  - [parseTypeAliasDeclaration](#parsetypealiasdeclaration)
- [Semantic Analysis](#semantic-analysis)
  - [Pass 1 — Eager Registration](#pass-1--eager-registration)
  - [Pass 2 — Full Analysis](#pass-2--full-analysis)
  - [Alias Transparency](#alias-transparency)
- [Type Registry Alias Mechanism](#type-registry-alias-mechanism)
  - [typeAliases Map](#typealiases-map)
  - [registerType Auto-Aliasing](#registertype-auto-aliasing)
  - [registerAlias](#registeralias)
  - [resolveAlias](#resolvealias)
  - [lookupType Alias Fallback](#lookuptype-alias-fallback)
- [Module System Integration](#module-system-integration)
- [Compilation Pipeline](#compilation-pipeline)
  - [Erasure After Semantic Analysis](#erasure-after-semantic-analysis)
- [AST Printer](#ast-printer)
- [Examples](#examples)
  - [Simple Type Aliases](#simple-type-aliases)
  - [Generic Type Aliases](#generic-type-aliases)
  - [Aliases for Complex Types](#aliases-for-complex-types)

---

## Overview

Type aliases provide alternative names for existing types without creating new types. The alias is completely transparent — it resolves to the same `TypeSharedPointer` instance as the original type. This means:

- No runtime cost — aliases are erased before code generation.
- No new type identity — the alias and the original type are interchangeable in all contexts (assignability, comparability, method dispatch).
- No separate representation in HIR, MIR, or LLVM IR.

---

## Type Alias Syntax

### Basic Aliases

The basic syntax declares a new name for an existing type using the `type` keyword and the `=` assignment operator:

```
type Integer = I64
type Text = String
type Flag = Bool
```

After these declarations, `Integer`, `Text`, and `Flag` can be used anywhere the original types are expected.

### Access Modifiers

Type aliases support the same access modifiers as other declarations:

```
public type NodeId = I64
private type InternalBuffer = Memory<U8>
protect type SharedState = HashMap<String, I64>
```

A `public` type alias can be imported by other modules. `private` aliases are visible only within the declaring module. `protect` aliases follow the same visibility rules as protected class members.

### Generic Aliases

Type aliases can carry generic parameters, allowing parameterized shorthand:

```
type Pair<K, V> = HashMap<K, V>
type List<E> = ArrayList<E>
type Result<T> = Optional<T>
```

Generic parameters are parsed through the same `parseGenericParameters()` method used by class and function declarations, supporting constraints and defaults.

### Complex Target Types

The target type can be any valid type expression — unions, optionals, pointers, references, function types:

```
type StringOrInt = String | I64
type MaybeString = String?
type RawBuffer = *mut U8
type Callback = Callable<Void, <String, I64>>
type Transform<T> = Callable<T, <T>>
```

---

## AST Representation

### TypeAliasDeclaration

Defined in `src/uranite/ast/node.hpp:2790-2818`:

```cpp
struct TypeAliasDeclaration : Declaration {

    TypeNodeSharedPointer aliasedTypeNode;

    std::vector<GenericParameterSharedPointer> genericParameters;

    std::string name;

    TypeAliasDeclaration(
        const std::string& name,
        TypeNodeSharedPointer type,
        const lookup::SourceSharedPointer& source
    ) : Declaration( Node::Kind::TypeAliasDeclaration, source ),
        aliasedTypeNode( std::move( type ) ),
        name( name ) {
    }

};

using TypeAliasDeclarationSharedPointer =
    std::shared_ptr<TypeAliasDeclaration>;
```

| Field | Type | Description |
|---|---|---|
| `name` | `std::string` | Alias identifier (the new name) |
| `aliasedTypeNode` | `TypeNodeSharedPointer` | AST node for the target type being aliased |
| `genericParameters` | `std::vector<GenericParameterSharedPointer>` | Generic parameters if the alias is parameterized |
| `access` | `ast::AccessModifier` (inherited) | Visibility modifier (`public`, `private`, `protect`, `Default`) |

The `aliasedTypeNode` can be any type node — `SimpleTypeNode`, `GenericTypeNode`, `UnionTypeNode`, `OptionalTypeNode`, `PointerTypeNode`, `ReferenceTypeNode`, or `FunctionTypeNode`. The `genericParameters` vector is populated only when the alias declares its own type parameters.

---

## Parsing Implementation

### Declaration Dispatch

Type alias declarations are dispatched from the top-level declaration parser in `src/uranite/parser/parser.cpp:828-829`:

```cpp
case token::Type::KeywordType:
    return this->parseTypeAliasDeclaration( access );
```

When the parser encounters the `type` keyword at declaration level (after any access modifier), it delegates to `parseTypeAliasDeclaration`. The `access` parameter carries any preceding access modifier (`public`, `private`, `protect`).

### parseTypeAliasDeclaration

Defined in `src/uranite/parser/parser.cpp:2839-2851`:

```cpp
ast::nodes::TypeAliasDeclarationSharedPointer
Parser::parseTypeAliasDeclaration( ast::AccessModifier access ) {
    lookup::SourceSharedPointer aliasSource = this->current().source;
    this->expect( token::Type::KeywordType, "" );
    std::string aliasName = this->expect(
        token::Type::Identifier, "expected type name" ).value;
    std::vector<ast::nodes::GenericParameterSharedPointer> genericParams =
        this->parseGenericParameters();
    this->expect( token::Type::Assignment, "expected '=' in type alias" );
    ast::nodes::TypeNodeSharedPointer targetType = this->parseTypeNode();
    ast::nodes::TypeAliasDeclarationSharedPointer aliasDeclaration =
        std::make_shared<ast::nodes::TypeAliasDeclaration>(
            aliasName, targetType, aliasSource );
    aliasDeclaration->access = access;
    aliasDeclaration->genericParameters = std::move( genericParams );
    this->expectNewline( "type alias" );
    return aliasDeclaration;
}
```

Parsing steps:

1. **Consume `type` keyword**: `expect(KeywordType)` consumes the `type` token.
2. **Parse alias name**: `expect(Identifier)` reads the new type name.
3. **Parse generic parameters**: `parseGenericParameters()` handles optional `<T, U>` parameter list with constraints and defaults. Returns empty vector if no angle bracket follows.
4. **Consume `=`**: `expect(Assignment)` requires the `=` token separating the name from the target type.
5. **Parse target type**: `parseTypeNode()` reads the full target type expression, supporting unions (`|`), optionals (`?`), pointers (`*`), references (`&`), generics (`<>`), and nested combinations.
6. **Construct node**: Creates `TypeAliasDeclaration` with the alias name, target type node, and source location. Sets access modifier and generic parameters.
7. **Expect newline**: `expectNewline("type alias")` enforces line termination after the declaration.

---

## Semantic Analysis

Type aliases are analyzed in both passes of the two-pass semantic analyzer.

### Pass 1 — Eager Registration

During `analyzeModuleRegistration()` (the first pass that eagerly registers all types and symbols), type aliases are handled in `src/uranite/semantic/analyzer.cpp:923-929`:

```cpp
case ast::Node::Kind::TypeAliasDeclaration: {
    ast::nodes::TypeAliasDeclaration& aliasDeclaration =
        static_cast<ast::nodes::TypeAliasDeclaration&>( *declaration );
    TypeSharedPointer resolvedAliasType =
        this->resolveType( aliasDeclaration.aliasedTypeNode );
    if( resolvedAliasType ) {
        this->typeRegistry.registerType(
            aliasDeclaration.name, resolvedAliasType );
    }
    break;
}
```

In the first pass, the analyzer resolves the target type and registers the alias name as pointing to that resolved type. This makes the alias available for use by subsequent declarations in the same module and by other modules that import from this one.

### Pass 2 — Full Analysis

During the second pass (full declaration analysis), type aliases are re-analyzed in `src/uranite/semantic/analyzer.cpp:1972-1973` dispatching to `analyzeTypeAliasDeclaration` in `src/uranite/semantic/analyzer.cpp:3939-3943`:

```cpp
void Analyzer::analyzeTypeAliasDeclaration(
    ast::nodes::TypeAliasDeclaration& declaration ) {
    TypeSharedPointer resolvedType =
        this->resolveType( declaration.aliasedTypeNode );
    if( resolvedType ) {
        this->typeRegistry.registerType( declaration.name, resolvedType );
    }
}
```

The second pass performs identical logic — resolve the target type and register. This re-registration ensures the alias is up to date if the target type was refined between passes (for example, if the target type is a class that gained additional methods or interface implementations during the second pass). The idempotent `registerType` call safely overwrites the first-pass registration.

### Alias Transparency

Because `registerType` stores the *resolved target type* under the alias name (not a wrapper or proxy), the alias is completely transparent. When code later uses the alias name, `lookupType("AliasName")` returns the exact same `TypeSharedPointer` as `lookupType("OriginalType")`. There is no "alias type" in the type system — the alias name simply maps to the underlying type.

---

## Type Registry Alias Mechanism

The type registry maintains a dedicated alias map alongside the main type map. These work together to resolve type names through multiple indirection levels.

### typeAliases Map

Defined in `src/uranite/semantic/typeref.hpp:1173-1174`:

```cpp
std::unordered_map<std::string, std::string> typeAliases;
```

Maps short names to qualified names. When a type is registered with a name that differs from its qualified name, the short name is automatically added as an alias.

### registerType Auto-Aliasing

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

When registering a type under a name that differs from its qualified name, `registerType` does three things:

1. Stores the type under the given `name` in `userTypesType`.
2. Also stores the type under its `qualified` name in `userTypesType`.
3. Creates an alias entry mapping `name → qualified` in `typeAliases`.

This automatic aliasing means every `registerType` call with a short name creates a lookup path from short name through to the qualified name. For type alias declarations specifically, this stores the *target type* under the *alias name*, effectively creating the mapping `AliasName → TargetType`.

### registerAlias

Direct alias registration in `src/uranite/semantic/typeref.cpp:664-666`:

```cpp
void Registry::registerAlias(
    const std::string& shortName,
    const std::string& qualifiedName ) {
    this->typeAliases[shortName] = qualifiedName;
}
```

Adds a pure name-to-name mapping without registering a type. Used by the import system when an imported entity needs a local alias name.

### resolveAlias

Alias resolution in `src/uranite/semantic/typeref.cpp:668-673`:

```cpp
std::string Registry::resolveAlias( const std::string& name ) const {
    std::unordered_map<std::string,std::string>::const_iterator
        aliasIterator = this->typeAliases.find( name );
    if( aliasIterator != this->typeAliases.end() ) {
        return aliasIterator->second;
    }
    return name;
}
```

Resolves a name through the alias map. If the name is an alias, returns the qualified name it points to. If not found, returns the name unchanged. This provides single-level alias resolution — it does not chain through multiple alias hops.

### lookupType Alias Fallback

The `lookupType` method uses the alias map as a fallback when direct lookup fails, in `src/uranite/semantic/typeref.cpp:593-606`:

```cpp
TypeSharedPointer Registry::lookupType(
    const std::string& name ) const {
    std::unordered_map<std::string,TypeSharedPointer>::const_iterator
        typeIterator = this->userTypesType.find( name );
    if( typeIterator != this->userTypesType.end() ) {
        return typeIterator->second;
    }
    std::unordered_map<std::string,std::string>::const_iterator
        aliasIterator = this->typeAliases.find( name );
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

1. **Direct lookup**: Search `userTypesType` by the given name. If found, return immediately.
2. **Alias lookup**: Search `typeAliases` by the given name. If an alias exists, look up the qualified name in `userTypesType`.
3. **Primitive fallback**: If neither direct nor alias lookup succeeds, search the `primitivesTypes` map via `lookupPrimitive`.

This three-tier resolution ensures that type aliases work transparently — code using the alias name resolves to the same type as code using the original qualified name.

---

## Module System Integration

Type alias declarations participate in the module import/export system. The compiler driver identifies type alias declarations during module resolution in `src/uranite/compiler/driver.cpp:328-329` and `src/uranite/compiler/driver.cpp:362-363`:

```cpp
else if( existingDeclaration->kind ==
         ast::Node::Kind::TypeAliasDeclaration ) {
    existingIdentifier =
        static_cast<const ast::nodes::TypeAliasDeclaration&>(
            *existingDeclaration ).name;
}
```

```cpp
else if( moduleDeclaration->kind ==
         ast::Node::Kind::TypeAliasDeclaration ) {
    declarationIdentifier =
        static_cast<ast::nodes::TypeAliasDeclaration&>(
            *moduleDeclaration ).name;
}
```

The driver extracts the alias name for duplicate detection (preventing two modules from exporting the same name) and for matching against selective import lists. A `public type` alias in module A can be imported by module B using standard `from moduleA import AliasName` syntax.

When an imported type alias is analyzed, the `registerType` call in `analyzeModuleRegistration` makes the alias name available in the importing module scope. The alias resolves to the same underlying type regardless of which module performs the lookup.

---

## Compilation Pipeline

### Erasure After Semantic Analysis

Type aliases are fully erased after semantic analysis. They produce no representation in:

- **HIR**: No `HIRTypeAlias` node kind exists. The alias has already been resolved to its target type during semantic analysis, so HIR expressions and declarations carry the resolved `TypeSharedPointer` directly.
- **MIR**: No `MIRTypeAlias` instruction exists. All type references in MIR use the underlying resolved type.
- **LLVM IR**: No codegen handling for type aliases. The `toLLVMType` method never sees alias names — it receives already-resolved `TypeSharedPointer` instances.

This erasure is a direct consequence of the registration mechanism: `registerType(aliasName, resolvedTargetType)` stores the target type under the alias name, so any subsequent `resolveType` or `lookupType` call returns the target type itself. The alias name is just a lookup key that maps to an existing type — it never creates a new type entity.

---

## AST Printer

The AST printer outputs type alias declarations in `src/uranite/visitors/printer.cpp:586-588`:

```cpp
void ASTPrinter::visit( ast::nodes::TypeAliasDeclaration& node ) {
    writeLine("(TypeAlias " + node.name + ")");
}
```

Produces output like `(TypeAlias Integer)` in the `--dump-ast` diagnostic mode. The target type is not printed — only the alias name.

---

## Examples

### Simple Type Aliases

Basic shorthand names for common types:

```
package examples

from uranite.io.console import puts

type Integer = I64
type Text = String
type Flag = Bool

public function describe(Text label, Integer count, Flag enabled) -> Text:
    if enabled:
        return label + ": " + count.toString()
    return label + ": disabled"

public function main() -> I32:
    Text result = describe("items", 42, true)
    puts(result)
    return 0
```

**Compilation trace**:
1. **Parser**: `type Integer = I64` parses as `TypeAliasDeclaration(name="Integer", aliasedTypeNode=SimpleTypeNode("I64"))`. No generic parameters.
2. **Semantic pass 1**: `resolveType(SimpleTypeNode("I64"))` returns the `IntegerType` for `I64`. `registerType("Integer", I64Type)` stores `I64Type` under the name "Integer" and creates alias entry `"Integer" → "I64"` in `typeAliases`.
3. **Semantic pass 2**: Re-executes the same `resolveType` + `registerType`. Idempotent.
4. **Usage resolution**: `describe(Text label, Integer count, Flag enabled)` — `resolveType("Integer")` calls `lookupType("Integer")`, finds `I64Type` directly in `userTypesType`. Parameters resolve to `I64`, `String`, `Bool`. No alias indirection needed at call sites.
5. **HIR/MIR/Codegen**: The alias names never appear. All type references are the resolved primitives.

### Generic Type Aliases

Parameterized aliases that forward generic arguments:

```
package examples

from uranite.collection.array-list import ArrayList
from uranite.collection.hash-map import HashMap
from uranite.io.console import puts

type List<E> = ArrayList<E>
type Dict<K, V> = HashMap<K, V>

public function main() -> I32:
    List<String> names = new ArrayList<String>()
    names.add("Alice")
    names.add("Bob")

    Dict<String, I64> ages = new HashMap<String, I64>()
    ages.put("Alice", 30)
    ages.put("Bob", 25)

    puts(names.size().toString())
    puts(ages.size().toString())
    return 0
```

**Compilation trace**:
1. **Parser**: `type List<E> = ArrayList<E>` parses with `genericParameters = [GenericParameter("E")]` and `aliasedTypeNode = GenericTypeNode("ArrayList", [SimpleTypeNode("E")])`.
2. **Semantic pass 1**: `resolveType(GenericTypeNode("ArrayList", [E]))` resolves the generic `ArrayList<E>`. The generic parameter `E` in the alias context maps to the `ArrayList` class generic parameter. `registerType("List", resolvedArrayListType)` creates the alias.
3. **Usage**: `List<String>` is looked up as `lookupType("List")`, returning `ArrayList` type. The generic argument `String` triggers monomorphization of `ArrayList<String>` through the standard generic resolution path.

### Aliases for Complex Types

Aliases wrapping union types, optionals, and function types:

```
package examples

from uranite.io.console import puts

type StringOrInt = String | I64
type MaybeString = String?
type Formatter = Callable<String, <I64>>

public function formatNumber(Formatter formatter, I64 value) -> StringOrInt:
    return formatter(value)

public function defaultFormat(I64 number) -> String:
    return number.toString()

public function main() -> I32:
    StringOrInt result = formatNumber(defaultFormat, 42)

    MaybeString optional = "hello"
    MaybeString empty = None

    return 0
```

**Compilation trace**:
1. **Parser**: `type StringOrInt = String | I64` — target type parses as `UnionTypeNode([SimpleTypeNode("String"), SimpleTypeNode("I64")])`. `type MaybeString = String?` — parses as `OptionalTypeNode(SimpleTypeNode("String"))`. `type Formatter = Callable<String, <I64>>` — parses as `GenericTypeNode("Callable", [String, <I64>])`.
2. **Semantic analysis**: Each alias resolves its target type:
   - `StringOrInt` → `UnionType([StringType, I64Type])`, name `Union<String,I64>`
   - `MaybeString` → `OptionalType(inner=StringType)`, name `String?`
   - `Formatter` → `CallableType(returnType=String, parameterTypes=[I64])`, name `Callable<String,<I64>>`
3. **Registration**: `registerType("StringOrInt", unionType)` stores the union type under the alias. Since `unionType->qualified` is `Union<String,I64>` (differs from "StringOrInt"), the alias entry `"StringOrInt" → "Union<String,I64>"` is created.
4. **Function parameter**: `formatNumber(Formatter formatter, ...)` — `lookupType("Formatter")` returns the `CallableType`. Parameter type resolution proceeds identically to using `Callable<String, <I64>>` directly.
5. **Codegen**: No alias names survive. `StringOrInt` becomes the tagged union `{i32, widest_member}` struct. `MaybeString` becomes the optional representation. `Formatter` becomes an opaque pointer.
