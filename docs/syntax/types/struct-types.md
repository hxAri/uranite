# Struct Types

Uranite structs are value-oriented compound types that group named fields with associated methods. Unlike classes, structs do not participate in inheritance hierarchies, have no virtual dispatch (no vtable), and are not heap-allocated by default — they compile to flat LLVM `StructType` values with fields laid out sequentially at 8-byte offsets. Structs support generic parameters, trait composition via `use`, nested type declarations (classes, enums, interfaces, other structs), access modifiers on fields and methods, static methods, properties, and forward declarations. The `struct` keyword introduces a struct declaration parsed by `parseStructDeclaration`, which produces a `StructDeclaration` AST node flowing through two-pass semantic analysis (eager registration in `registerTypeDeclaration`, full analysis in `analyzeStructDeclaration` with trait field/method merging), HIR lowering to `HIRStructDefinition`, MIR lowering to a `TypeLayoutDescriptor` with per-field byte offsets, and MIR codegen to an LLVM named `StructType` with `structTypeCache` deduplication.

This document covers the `struct` keyword syntax with access modifiers and generic parameters, the `StructDeclaration` AST node, the `StructType` semantic type, parsing implementation with field/method/nested declaration/trait handling, two-pass semantic analysis (registration and analysis phases), trait composition, HIR lowering to `HIRStructDefinition`, MIR lowering to `TypeLayoutDescriptor`, MIR codegen to LLVM `StructType` via `structTypeCache`, field access via `StructGEP`, and practical examples.

---

## Table of Contents

- [Overview](#overview)
- [Syntax](#syntax)
  - [Basic Struct](#basic-struct)
  - [Generic Structs](#generic-structs)
  - [Trait Composition](#trait-composition)
  - [Forward Declarations](#forward-declarations)
  - [Nested Declarations](#nested-declarations)
- [AST Representation](#ast-representation)
  - [StructDeclaration](#structdeclaration)
  - [FieldDeclarationNode](#fielddeclarationnode)
- [Semantic Type — StructType](#semantic-type--structtype)
- [Parsing Implementation](#parsing-implementation)
- [Semantic Analysis](#semantic-analysis)
  - [Pass 1 — Eager Registration](#pass-1--eager-registration)
  - [Pass 2 — Full Analysis](#pass-2--full-analysis)
  - [Trait Merging](#trait-merging)
  - [Field Resolution](#field-resolution)
  - [Method Resolution](#method-resolution)
  - [Self Parameter Validation](#self-parameter-validation)
- [Compilation Pipeline](#compilation-pipeline)
  - [HIR Stage — HIRStructDefinition](#hir-stage--hirstructdefinition)
  - [MIR Lowering — TypeLayoutDescriptor](#mir-lowering--typelayoutdescriptor)
  - [MIR Codegen — LLVM StructType](#mir-codegen--llvm-structtype)
- [Field Access](#field-access)
- [Struct vs Class](#struct-vs-class)
- [Examples](#examples)
  - [Basic Value Struct](#basic-value-struct)
  - [Generic Struct with Methods](#generic-struct-with-methods)
  - [Struct with Trait Composition](#struct-with-trait-composition)

---

## Overview

Structs are lightweight compound types designed for grouping related data without the overhead of class-based OOP:

| Feature | Struct | Class |
|---|---|---|
| Inheritance | No | Single parent class |
| Virtual dispatch | No (no vtable) | Yes (vtable) |
| Interface implementation | No | Yes (`implements`) |
| Trait composition | Yes (`use`) | No |
| Default allocation | Value (stack/inline) | Heap (pointer) |
| Generic parameters | Yes | Yes |
| Nested declarations | Yes | Yes |
| Access modifiers | Yes | Yes |
| Methods | Yes | Yes |

---

## Syntax

### Basic Struct

```
public struct Point:
    public I64 xCoord
    public I64 yCoord

    public function Point(self, I64 xCoord, I64 yCoord) -> Void:
        self.xCoord = xCoord
        self.yCoord = yCoord

    public function distanceTo(self, Point other) -> F64:
        I64 deltaX = self.xCoord - other.xCoord
        I64 deltaY = self.yCoord - other.yCoord
        return ((deltaX * deltaX + deltaY * deltaY) as F64).sqrt()
```

Struct declaration syntax: `[access] struct Name[<generics>]:` followed by an indented body containing fields, methods, nested declarations, and trait usage.

### Generic Structs

```
public struct Pair<K, V>:
    public K key
    public V value

    public function Pair(self, K key, V value) -> Void:
        self.key = key
        self.value = value
```

Generic parameters are declared after the struct name in angle brackets and are available throughout the struct body for field types, method parameters, and return types.

### Trait Composition

```
public trait Displayable:
    public function display(self) -> String;

public trait Serializable:
    public function serialize(self) -> String;

public struct Config:
    use Displayable, Serializable
    public String name
    public String value

    public function display(self) -> String:
        return self.name + ": " + self.value

    public function serialize(self) -> String:
        return self.name + "=" + self.value
```

The `use` keyword incorporates trait fields and methods into the struct. Multiple traits are comma-separated. If the struct already defines a field or method with the same name as a trait member, the struct's version takes precedence.

### Forward Declarations

```
public struct Node;
```

A semicolon after the struct name (with no colon or body) creates a forward declaration. This allows recursive struct references where two structs need to reference each other.

### Nested Declarations

Structs can contain nested classes, enums, interfaces, and other structs:

```
public struct Container:
    public I64 size

    public enum Status:
        unit Active
        unit Inactive

    public struct Metadata:
        public String label
```

---

## AST Representation

### StructDeclaration

Defined in `src/uranite/ast/node.hpp:2692-2726`:

```cpp
struct StructDeclaration : Declaration {

    std::vector<FieldDeclarationSharedPointer> fields;
    std::vector<GenericParameterSharedPointer> genericParameters;
    std::vector<DeclarationSharedPointer> methods;
    std::string name;
    std::vector<DeclarationSharedPointer> nestedDeclarations;
    std::vector<std::string> usedTraits;

    StructDeclaration(
        const std::string& name,
        const lookup::SourceSharedPointer& source
    ) : Declaration( Node::Kind::StructDeclaration, source ),
        name( name ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `fields` | `vector<FieldDeclarationSharedPointer>` | Data members (type + name + modifiers) |
| `genericParameters` | `vector<GenericParameterSharedPointer>` | Generic type parameters (`<K, V>`) |
| `methods` | `vector<DeclarationSharedPointer>` | Function/property declarations |
| `name` | `string` | Struct identifier |
| `nestedDeclarations` | `vector<DeclarationSharedPointer>` | Nested classes, enums, interfaces, structs |
| `usedTraits` | `vector<string>` | Trait names from `use` statements |

### FieldDeclarationNode

Defined in `src/uranite/ast/node.hpp:830-854`:

```cpp
struct FieldDeclarationNode : Node {

    bool isFinal = false;
    bool isReadonly = false;
    bool isStatic = false;
    std::string name;
    TypeNodeSharedPointer type;

    FieldDeclarationNode(
        const std::string& name,
        TypeNodeSharedPointer type,
        const lookup::SourceSharedPointer& source
    ) : Node( Node::Kind::FieldDeclaration, source ),
        name( name ),
        type( std::move( type ) ) {
    }

};
```

Fields carry access, mutability, and static modifiers. The type is a type annotation node resolved during semantic analysis.

---

## Semantic Type — StructType

Defined in `src/uranite/semantic/typeref.hpp:671-726`:

```cpp
struct StructType : Type {

    int alignment = 8;
    ast::nodes::StructDeclaration* astDeclaration = nullptr;
    std::vector<FieldInfo> fields;
    std::vector<TypeSharedPointer> genericParameters;
    std::vector<MethodInfo> methods;
    int objectSize = 0;
    std::unordered_map<std::string, TypeSharedPointer>
        typeSubstitutions;

    StructType( const std::string& name )
        : Type( Type::Kind::Struct, name ) {
    }

    FieldInfo* findField( const std::string& name );
    MethodInfo* findMethod( const std::string& name );

};
```

| Field | Type | Description |
|---|---|---|
| `alignment` | `int` | Memory alignment in bytes (default 8) |
| `astDeclaration` | `StructDeclaration*` | Back-pointer to AST for monomorphization |
| `fields` | `vector<FieldInfo>` | Resolved field metadata with types and indices |
| `genericParameters` | `vector<TypeSharedPointer>` | Generic parameter types |
| `methods` | `vector<MethodInfo>` | Resolved method metadata with function types |
| `objectSize` | `int` | Total size in bytes after layout |
| `typeSubstitutions` | `unordered_map<string, TypeSharedPointer>` | Generic parameter to concrete type mapping |

`findField` and `findMethod` perform linear search by name through their respective vectors.

---

## Parsing Implementation

The `parseStructDeclaration` method in `src/uranite/parser/parser.cpp:2579-2706`:

```cpp
ast::nodes::StructDeclarationSharedPointer
    Parser::parseStructDeclaration(
        ast::AccessModifier access ) {
    this->expect( token::Type::KeywordStruct, "" );
    token::Token structNameToken =
        this->expect( token::Type::Identifier,
            "expected struct name" );
    // ... struct creation ...
    structDeclaration->genericParameters =
        this->parseGenericParameters();
    if( this->match( token::Type::Semicolon ) ) {
        this->expectNewline( "struct forward declaration" );
        return structDeclaration;
    }
    this->expect( token::Type::Colon,
        "expected ':' or ';' after struct declaration" );
    // ... body parsing ...
}
```

Parsing steps:

1. Consume `struct` keyword.
2. Consume struct name identifier.
3. Parse optional generic parameters (`<K, V>`).
4. Check for forward declaration (semicolon) — return immediately if found.
5. Expect colon and newline for body start.
6. Enter indented block and parse members:

The body parsing loop handles member modifiers and dispatches based on the next keyword:

| Token | Action |
|---|---|
| `function` or `property` | Parse method via `parseFunctionDeclaration` |
| `class` | Parse nested class via `parseClassDeclaration` |
| `enum` | Parse nested enum via `parseEnumDeclaration` |
| `interface` | Parse nested interface via `parseInterfaceDeclaration` |
| `struct` | Parse nested struct recursively |
| `use` | Parse trait name list, add to `usedTraits` |
| Identifier or `?` | Parse field declaration via `parseFieldDeclaration` |

Member modifier keywords parsed before each member: `abstract`, `async`, `const`, `final`, `native`, `override`, `readonly`, `static`, `virtual`. These modify the subsequent field or method declaration.

---

## Semantic Analysis

### Pass 1 — Eager Registration

In `registerTypeDeclaration` at `src/uranite/semantic/analyzer.cpp:4659-4688`:

```cpp
case ast::Node::Kind::StructDeclaration: {
    ast::nodes::StructDeclaration& structDeclaration =
        static_cast<ast::nodes::StructDeclaration&>(
            *declaration );
    std::string thisQualified =
        buildQualified( structDeclaration.name );
    // Check for existing registration (from imports)
    // ...
    StructTypeSharedPointer structType =
        std::make_shared<StructType>(
            structDeclaration.name );
    structType->astDeclaration = &structDeclaration;
    structType->package = this->currentPackageName;
    structType->qualified = thisQualified;
    for( auto& genericParameter :
         structDeclaration.genericParameters ) {
        structType->genericParameters.push_back(
            std::make_shared<GenericParameterType>(
                genericParameter->name ) );
    }
    this->typeRegistry.registerType(
        structDeclaration.name, structType );
    for( auto& nestedDeclaration :
         structDeclaration.nestedDeclarations ) {
        this->registerTypeDeclaration(
            nestedDeclaration, thisQualified );
    }
    break;
}
```

Pass 1 creates the `StructType`, assigns its qualified name (`package.StructName`), registers generic parameters, and registers it in the type registry. If the struct was already imported, it updates the existing registration with the current AST declaration pointer and propagates the qualified name if not yet set.

Nested declarations are recursively registered with the struct's qualified name as parent — e.g., `Container.Metadata` gets qualified name `"myapp.Container.Metadata"`.

### Pass 2 — Full Analysis

In `analyzeStructDeclaration` at `src/uranite/semantic/analyzer.cpp:3765-3891`:

Pass 2 performs full semantic analysis in this order:

1. Look up the pre-registered `StructType` from pass 1.
2. Merge trait fields and methods.
3. Push a `Class`-kind scope (structs share the same scope kind).
4. Register generic parameters in the type registry (saving/restoring any shadowed names).
5. Resolve all field types and build `FieldInfo` entries with sequential indices.
6. Analyze all methods — validate self parameters, resolve parameter/return types, build `MethodInfo` entries.
7. Analyze nested declarations.
8. Restore shadowed generic parameter names and pop scope.

### Trait Merging

At `src/uranite/semantic/analyzer.cpp:3770-3804`, trait composition is resolved before field/method analysis:

```cpp
for( std::string& traitName : declaration.usedTraits ) {
    TraitTypeSharedPointer traitType =
        std::dynamic_pointer_cast<TraitType>(
            this->typeRegistry.lookupType( traitName ) );
    if( traitType == nullptr ) {
        this->diagnostic.error( declaration.source,
            fmt::format( "unknown trait \"{}\"",
                traitName ) );
        continue;
    }
    if( traitType->astDeclaration ) {
        for( auto& traitField :
             traitType->astDeclaration->fields ) {
            bool fieldExists = false;
            for( auto& structField : declaration.fields ) {
                if( structField->name == traitField->name ) {
                    fieldExists = true;
                    break;
                }
            }
            if( fieldExists == false ) {
                declaration.fields.push_back( traitField );
            }
        }
        // Same for methods...
    }
}
```

Trait merging adds trait fields and methods to the struct's AST declaration directly. The struct's own definitions take precedence — if a field or method name already exists, the trait version is skipped. This is a flat composition model, not mixin inheritance.

### Field Resolution

At `src/uranite/semantic/analyzer.cpp:3821-3831`:

```cpp
structType->fields.clear();
int currentFieldIndex = 0;
for( auto& fieldDeclaration : declaration.fields ) {
    TypeSharedPointer fieldType =
        this->resolveType( fieldDeclaration->type );
    FieldInfo fieldInformation;
    fieldInformation.name = fieldDeclaration->name;
    fieldInformation.type = fieldType ?
        fieldType : this->typeRegistry.getError();
    fieldInformation.access = fieldDeclaration->access;
    fieldInformation.isStatic = fieldDeclaration->isStatic;
    fieldInformation.index = currentFieldIndex++;
    structType->fields.push_back( fieldInformation );
}
```

Fields are assigned sequential zero-based indices. Each field type is resolved from the type annotation. Unresolvable types fall back to the `Error` sentinel type.

### Method Resolution

At `src/uranite/semantic/analyzer.cpp:3833-3883`, methods are analyzed via the standard `analyzeDeclaration` path, then their `MethodInfo` entries are constructed with resolved parameter types and return types. Each method gets a `FunctionType` with parameter names.

### Self Parameter Validation

The analyzer validates `self` parameter requirements at `src/uranite/semantic/analyzer.cpp:3836-3854`:

- Non-static methods **must** have `self` as first parameter — error: "non-static method must have self as first parameter, or be declared static".
- Static methods **must not** have `self` — error: "static method must not have self parameter".

---

## Compilation Pipeline

### HIR Stage — HIRStructDefinition

Defined in `src/uranite/ir/hir.hpp:1114-1132`:

```cpp
struct HIRStructDefinition : HIRNode {

    std::string structName;
    std::string structQualifiedName;
    ast::AccessModifier accessModifier =
        ast::AccessModifier::Default;
    std::vector<HIRFieldDescriptor> fieldDescriptors;
    std::vector<std::shared_ptr<HIRFunctionDefinition>>
        methodDefinitions;
    std::vector<HIRGenericParameterDescriptor>
        genericParameters;
    std::vector<HIRNodeSharedPointer> nestedDeclarations;
    std::vector<std::string> usedTraitNames;

    HIRStructDefinition(
        const std::string& structName,
        const lookup::SourceSharedPointer& sourceLocation
    ) : HIRNode( HIRNodeKind::StructDefinition,
            nullptr, sourceLocation ),
        structName( structName ) {
    }

};
```

HIR lowering from AST in `src/uranite/ir/hir/lowering.cpp:392-430`:

1. Look up the semantic `StructType` for the qualified name.
2. Create `HIRStructDefinition` with the struct name.
3. Lower each field to an `HIRFieldDescriptor` with index.
4. Lower each generic parameter.
5. Lower each method to `HIRFunctionDefinition`, setting `ownerClassName` to the struct's qualified name.
6. Set self parameter types on methods to the struct type.

### MIR Lowering — TypeLayoutDescriptor

In `src/uranite/ir/mir/lowering.cpp:745-776`:

```cpp
void MIRLowering::lowerStructDefinition(
    hir::HIRStructDefinition& hirStruct ) {
    std::string structLayoutKey =
        hirStruct.structQualifiedName.empty() == false
        ? hirStruct.structQualifiedName
        : hirStruct.structName;
    TypeLayoutDescriptor typeLayout;
    typeLayout.typeQualifiedName = structLayoutKey;
    typeLayout.hasVirtualTable = false;
    int fieldIndex = 0;
    for( const hir::HIRFieldDescriptor& fieldDescriptor :
         hirStruct.fieldDescriptors ) {
        typeLayout.fieldByteOffsets.push_back(
            fieldIndex * 8 );
        typeLayout.fieldNames.push_back(
            fieldDescriptor.fieldName );
        typeLayout.fieldTypes.push_back(
            fieldDescriptor.fieldType );
        fieldIndex++;
    }
    typeLayout.typeSizeInBytes = fieldIndex * 8;
    typeLayout.typeAlignmentInBytes = 8;
    this->currentModule->typeLayoutTable[structLayoutKey]
        = typeLayout;
    // ... lower methods with ownerClassName set ...
}
```

Key properties of struct layout:
- **No vtable**: `hasVirtualTable = false` — structs skip virtual dispatch entirely.
- **8-byte field stride**: Each field occupies 8 bytes (`fieldIndex * 8`), regardless of the field's actual type. This simplifies alignment at the cost of padding for smaller types.
- **Total size**: `fieldCount * 8` bytes.
- **Dual registration**: The layout is registered under both the qualified name and short name for lookup flexibility.

Methods are lowered as regular function definitions with `ownerClassName` set for name mangling.

### MIR Codegen — LLVM StructType

In `src/uranite/ir/mir/codegen.cpp:148-184`:

```cpp
std::string canonicalName =
    typeLayout.typeQualifiedName.empty() == false
    ? typeLayout.typeQualifiedName : typeName;
// Skip if already processed
llvm::StructType* existingType =
    llvm::StructType::getTypeByName(
        this->llvmContext, canonicalName );
if( existingType != nullptr ) {
    this->structTypeCache[canonicalName] = existingType;
    this->structTypeCache[typeName] = existingType;
    continue;
}
std::vector<llvm::Type*> fieldLLVMTypes;
for( const semantic::TypeSharedPointer& fieldType :
     typeLayout.fieldTypes ) {
    llvm::Type* llvmFieldType =
        this->toLLVMType( fieldType );
    if( llvmFieldType->isVoidTy() ) {
        llvmFieldType =
            llvm::PointerType::getUnqual( this->llvmContext );
    }
    fieldLLVMTypes.push_back( llvmFieldType );
}
if( fieldLLVMTypes.empty() ) {
    fieldLLVMTypes.push_back(
        llvm::Type::getInt8Ty( this->llvmContext ) );
}
llvm::StructType* structType = llvm::StructType::create(
    this->llvmContext, fieldLLVMTypes, canonicalName
);
this->structTypeCache[canonicalName] = structType;
```

LLVM struct creation:
1. Use the canonical (qualified) name as the LLVM struct name for deduplication.
2. Check `structTypeCache` and LLVM's own type registry for existing definitions.
3. Convert each field type to its LLVM equivalent via `toLLVMType`. Void fields become opaque pointers.
4. Empty structs get a placeholder `i8` field (LLVM requires at least one element).
5. Create a named `llvm::StructType` and cache it under both the canonical name and short name.

The `structTypeCache` (an `unordered_map<string, llvm::StructType*>`) is shared between classes and structs — the same cache and creation path handles both, with the only difference being that class layouts set `hasVirtualTable = true`.

---

## Field Access

When accessing struct fields via member expressions (e.g., `point.xCoord`), the semantic analyzer looks up the field in the `StructType` at `src/uranite/semantic/analyzer.cpp:3319-3325`:

```cpp
else if( objectType->kind == Type::Kind::Struct ) {
    StructTypeSharedPointer structType =
        std::static_pointer_cast<StructType>( objectType );
    // Direct field lookup, then base template fallback
    fieldInformation = structType->findField(
        expression.member );
    if( fieldInformation == nullptr &&
        structType->astDeclaration ) {
        StructTypeSharedPointer baseStructType =
            std::dynamic_pointer_cast<StructType>(
                this->typeRegistry.lookupType(
                    structType->astDeclaration->name ) );
        if( baseStructType ) {
            fieldInformation = baseStructType->findField(
                expression.member );
        }
    }
}
```

Field lookup tries the monomorphized struct first, then falls back to the base template struct. At MIR codegen, field access uses `CreateStructGEP` with the field index from the `TypeLayoutDescriptor` to compute the field pointer within the struct value.

---

## Struct vs Class

| Aspect | Struct | Class |
|---|---|---|
| Keyword | `struct` | `class` |
| Inheritance | None | Single parent via `extends` |
| Interfaces | None | Multiple via `implements` |
| Traits | Yes via `use` | None |
| Virtual methods | No | Yes (vtable generated) |
| Abstract methods | Not supported | Supported |
| Droper (destructor) | No | Yes (via `Droper` interface) |
| Memory layout | Flat fields, no vtable pointer | Vtable pointer + fields |
| Identity kind | `Type::Kind::Struct` | `Type::Kind::Class` |
| Scope kind | `Scope::Kind::Class` (shared) | `Scope::Kind::Class` |
| Assignability | Struct-to-struct via identity only | Inheritance + interface conformance |

Structs are best for plain data containers, pairs, coordinate types, and other value-oriented data. Classes are best for polymorphic objects that need inheritance, virtual dispatch, and interface conformance.

---

## Examples

### Basic Value Struct

A simple struct with fields and methods:

```
package examples

from uranite.io.console import puts

public struct Color:
    public I64 red
    public I64 green
    public I64 blue

    public function Color(
        self, I64 red, I64 green, I64 blue) -> Void:
        self.red = red
        self.green = green
        self.blue = blue

    public function toString(self) -> String:
        return "rgb(" + self.red.toString() + ", " +
               self.green.toString() + ", " +
               self.blue.toString() + ")"

    public static function white() -> Color:
        return new Color(255, 255, 255)

public function main() -> I32:
    Color primary = new Color(255, 0, 0)
    puts(primary.toString())

    Color background = Color.white()
    puts(background.toString())

    return 0
```

**Compilation trace**:
1. **Parser**: `parseStructDeclaration` consumes `struct Color:`, parses three field declarations (`red`, `green`, `blue`), two instance methods (`Color` constructor, `toString`), and one static method (`white`).
2. **Semantic pass 1**: `registerTypeDeclaration` creates `StructType("Color")` with qualified name `"examples.Color"`, registers in type registry.
3. **Semantic pass 2**: `analyzeStructDeclaration` resolves field types to `I64`, validates `self` on instance methods, validates no `self` on static method, builds `FieldInfo` entries with indices 0, 1, 2.
4. **HIR**: `HIRStructDefinition` with 3 field descriptors and 3 method definitions.
5. **MIR**: `TypeLayoutDescriptor` with `hasVirtualTable=false`, fields at byte offsets 0, 8, 16, total size 24 bytes.
6. **LLVM**: `%examples.Color = type { i64, i64, i64 }` — three i64 fields, no vtable pointer.

### Generic Struct with Methods

A generic key-value pair struct:

```
package examples

from uranite.io.console import puts

public struct Entry<K, V>:
    public K key
    public V value
    public Boolean active

    public function Entry(
        self, K key, V value) -> Void:
        self.key = key
        self.value = value
        self.active = true

    public function isActive(self) -> Boolean:
        return self.active

    public function deactivate(self) -> Void:
        self.active = false

public function main() -> I32:
    Entry<String, I64> score = new Entry<String, I64>(
        "player1", 100)
    puts(score.key)

    if score.isActive():
        score.deactivate()

    return 0
```

**Compilation trace**:
1. **Semantic pass 1**: `StructType("Entry")` created with two `GenericParameterType` entries for `K` and `V`.
2. **Semantic pass 2**: Generic parameters `K` and `V` temporarily registered in the type registry. Field types resolve to the generic parameter types. At instantiation `Entry<String, I64>`, `typeSubstitutions` maps `K` to `String` and `V` to `I64`.
3. **MIR**: Layout has 3 fields at offsets 0, 8, 16. For the monomorphized instance, field types become `String`, `I64`, `Boolean` — but byte offsets remain at 8-byte stride.
4. **LLVM**: `%examples.Entry = type { ptr, i64, i1 }` (or similar depending on monomorphization).

### Struct with Trait Composition

A struct composing multiple traits for shared behavior:

```
package examples

from uranite.io.console import puts

public trait Printable:
    public function print(self) -> Void;

public trait Measurable:
    public I64 size

    public function getSize(self) -> I64:
        return self.size

public struct Document:
    use Printable, Measurable
    public String title
    public String content

    public function Document(
        self, String title, String content) -> Void:
        self.title = title
        self.content = content
        self.size = content.length()

    public function print(self) -> Void:
        puts("[" + self.title + "] " + self.content)

public function main() -> I32:
    Document readme = new Document(
        "README", "Welcome to Uranite")
    readme.print()
    puts("Size: " + readme.getSize().toString())

    return 0
```

**Compilation trace**:
1. **Trait merging**: `analyzeStructDeclaration` processes `use Printable, Measurable`:
   - `Printable` has method `print` — struct already defines `print`, so trait version is skipped.
   - `Measurable` has field `size` — struct does not define `size`, so it is appended to `declaration.fields`.
   - `Measurable` has method `getSize` — struct does not define it, so it is appended to `declaration.methods`.
2. **Field resolution**: Fields become `title` (index 0), `content` (index 1), `size` (index 2, from trait) — three fields at 8-byte offsets.
3. **Method resolution**: `Document` constructor, `print` (struct's own version), and `getSize` (from trait) are all analyzed and added to `structType->methods`.
