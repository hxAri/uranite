# Class Types

Uranite classes are heap-allocated, reference-counted compound types with fields, methods, single inheritance, multiple interface implementation, trait composition, generic parameters, and virtual dispatch via interface tables. At the LLVM level, a class instance is a `malloc`-allocated LLVM `StructType` accessed through an opaque pointer. Classes that implement interfaces or inherit from abstract parents carry a vtable pointer in their first struct field, enabling interface method dispatch through function pointer arrays. Constructors are ordinary methods named after the class, overloaded by parameter arity. Field layout follows 8-byte alignment with per-type packing for primitives.

This document covers the complete class type specification — the `ClassType`, `FieldInfo`, and `MethodInfo` structs, class declaration syntax with all modifiers (`abstract`, `final`, `readonly`, `native`), constructor patterns including property parameters, inheritance with field merging and method override rules, interface implementation and validation, trait composition via `use`, virtual table construction and interface table generation, two-pass semantic analysis with topological class ordering, memory layout computation, HIR and MIR lowering with `TypeLayoutDescriptor` creation, codegen with struct type creation and `ConstructObject` emission, field access via `CreateStructGEP`, virtual dispatch via itable lookup, and practical usage patterns.

---

## Table of Contents

- [Type Identity](#type-identity)
- [The ClassType Struct](#the-classtype-struct)
  - [FieldInfo](#fieldinfo)
  - [MethodInfo](#methodinfo)
  - [Field and Method Lookup](#field-and-method-lookup)
- [Class Declaration Syntax](#class-declaration-syntax)
  - [Basic Class](#basic-class)
  - [Constructors](#constructors)
  - [Property Parameters](#property-parameters)
  - [Fields](#fields)
  - [Methods](#methods)
  - [Properties](#properties)
  - [Abstract Classes](#abstract-classes)
  - [Final Classes](#final-classes)
  - [Readonly Classes](#readonly-classes)
  - [Native Classes](#native-classes)
  - [Generic Classes](#generic-classes)
  - [Forward Declarations](#forward-declarations)
  - [Nested Declarations](#nested-declarations)
  - [Trait Composition](#trait-composition)
- [Inheritance](#inheritance)
  - [Single Inheritance](#single-inheritance)
  - [Extends Keyword](#extends-keyword)
  - [Field Inheritance](#field-inheritance)
  - [Method Override](#method-override)
  - [Final Method Protection](#final-method-protection)
- [Interface Implementation](#interface-implementation)
  - [Implements Keyword](#implements-keyword)
  - [Interface Validation](#interface-validation)
  - [Droper Interface](#droper-interface)
- [AST Representation](#ast-representation)
  - [ClassDeclaration Node](#classdeclaration-node)
  - [FieldDeclarationNode](#fielddeclarationnode)
  - [FunctionParameterNode — Property Fields](#functionparameternode--property-fields)
- [Parsing Implementation](#parsing-implementation)
  - [Class Header Parsing](#class-header-parsing)
  - [Class Body Parsing](#class-body-parsing)
  - [Member Modifier Parsing](#member-modifier-parsing)
- [Semantic Analysis](#semantic-analysis)
  - [Two-Pass Registration](#two-pass-registration)
  - [Analysis Order](#analysis-order)
  - [Full Class Analysis](#full-class-analysis)
  - [Generic Parameter Registration](#generic-parameter-registration)
  - [Base Class Resolution](#base-class-resolution)
  - [Field Analysis](#field-analysis)
  - [Method Analysis](#method-analysis)
  - [Virtual Table Construction](#virtual-table-construction)
  - [Memory Layout Computation](#memory-layout-computation)
- [Compilation Pipeline](#compilation-pipeline)
  - [HIR Stage](#hir-stage)
  - [MIR Stage — Type Layout Descriptor](#mir-stage--type-layout-descriptor)
  - [MIR Stage — Inheritance Layout Merge](#mir-stage--inheritance-layout-merge)
  - [MIR Stage — Method Lowering](#mir-stage--method-lowering)
  - [Codegen Stage — LLVM Struct Type Creation](#codegen-stage--llvm-struct-type-creation)
  - [Codegen Stage — Interface Table Generation](#codegen-stage--interface-table-generation)
  - [Codegen Stage — ConstructObject](#codegen-stage--constructobject)
  - [Codegen Stage — Field Access](#codegen-stage--field-access)
  - [Codegen Stage — Virtual Dispatch](#codegen-stage--virtual-dispatch)
  - [Codegen Stage — toLLVMType](#codegen-stage--tollvmtype)
- [Assignability Rules](#assignability-rules)
- [Examples](#examples)
  - [Basic Classes](#basic-classes)
  - [Inheritance and Override](#inheritance-and-override)
  - [Readonly and Generic Classes](#readonly-and-generic-classes)
  - [Forward Declaration and Nested Classes](#forward-declaration-and-nested-classes)

---

## Type Identity

| Property | Value |
|---|---|
| Type Kind | `Type::Kind::Class` |
| LLVM Type | `PointerType::getUnqual(StructType)` |
| Allocation | `malloc` (minimum 64 bytes) |
| Default Alignment | 8 bytes |
| Vtable Slot | Field 0 (when interfaces present or abstract) |
| Constructor Name | `ClassName.ClassName` |
| Overload Suffix | `#N` (N = total parameter count including self) |

Classes are reference types. Every class variable holds a pointer to a heap-allocated struct. The struct layout is determined by the `TypeLayoutDescriptor` computed during MIR lowering — each field occupies a slot in the LLVM `StructType`, with a vtable pointer at index 0 when virtual dispatch is needed.

---

## The ClassType Struct

Defined in `src/uranite/semantic/typeref.hpp:580-661`:

```
struct ClassType : Type
    astDeclaration     : ClassDeclaration* (default nullptr)
    alignment          : int (default 8)
    baseClass          : TypeSharedPointer
    fields             : std::vector<FieldInfo>
    genericParameters  : std::vector<TypeSharedPointer>
    implementsDroper   : bool (default false)
    interfaces         : std::vector<TypeSharedPointer>
    isAbstract         : bool
    isFinal            : bool (default false)
    isReadonly         : bool (default false)
    methods            : std::vector<MethodInfo>
    objectSize         : int (default 0)
    typeSubstitutions  : std::unordered_map<std::string, TypeSharedPointer>
    virtualTable       : std::vector<MethodInfo*>
```

| Field | Description |
|---|---|
| `astDeclaration` | Back-pointer to AST for codegen access and monomorphization |
| `alignment` | Memory alignment requirement in bytes (default 8) |
| `baseClass` | Parent class via single inheritance |
| `fields` | Ordered list of instance and static fields |
| `genericParameters` | Generic type parameters for template-like classes |
| `implementsDroper` | Whether class implements `Droper` interface for automatic cleanup |
| `interfaces` | List of implemented interfaces |
| `isAbstract` | Cannot be instantiated; must be subclassed |
| `isFinal` | Cannot be extended by other classes |
| `isReadonly` | All fields become readonly after construction |
| `methods` | All methods (own + inherited overrides) |
| `objectSize` | Total memory size after layout computation |
| `typeSubstitutions` | Generic placeholder to concrete type bindings |
| `virtualTable` | Ordered function pointer table for virtual dispatch |

Constructor sets `Kind::Class`:

```
ClassType(name) → Type(Type::Kind::Class, name), isAbstract(false)
```

### FieldInfo

Defined in `src/uranite/semantic/typeref.hpp:515-535`:

```
struct FieldInfo
    access     : AccessModifier
    index      : int
    isReadonly : bool (default false)
    isStatic   : bool (default false)
    name       : std::string
    type       : TypeSharedPointer
```

| Field | Description |
|---|---|
| `access` | `public`, `private`, `protect`, or `Default` |
| `index` | Position in struct layout (-1 for static fields) |
| `isReadonly` | Immutable after initialization |
| `isStatic` | Class-level field, not per-instance |
| `name` | Field identifier |
| `type` | Field data type |

### MethodInfo

Defined in `src/uranite/semantic/typeref.hpp:540-572`:

```
struct MethodInfo
    access              : AccessModifier
    interfaceTableIndex : int (default -1)
    isFinal             : bool (default false)
    isOverride          : bool
    isProperty          : bool (default false)
    isStatic            : bool
    isVirtual           : bool
    name                : std::string
    type                : TypeSharedPointer
    virtualTableIndex   : int
```

| Field | Description |
|---|---|
| `access` | Visibility modifier |
| `interfaceTableIndex` | Position in interface method table (-1 if not interface method) |
| `isFinal` | Cannot be overridden further |
| `isOverride` | Overrides a parent class method |
| `isProperty` | Acts as property getter/setter |
| `isStatic` | Class-level method (no `self` parameter) |
| `isVirtual` | Dispatchable through vtable |
| `name` | Method identifier |
| `type` | `FunctionType` with parameter and return types |
| `virtualTableIndex` | Position in vtable (-1 if not virtual) |

### Field and Method Lookup

`findField(name)` at `typeref.hpp:636-643` and `findMethod(name)` at `typeref.hpp:650-657` provide linear search:

```
FieldInfo* findField(name):
    for field in fields:
        if field.name == name: return &field
    return nullptr

MethodInfo* findMethod(name):
    for method in methods:
        if method.name == name: return &method
    return nullptr
```

---

## Class Declaration Syntax

### Basic Class

Classes use indentation-based bodies after a colon:

```uranite
class Point:
    public I64 x
    public I64 y

    public function Point( self, I64 x, I64 y ) -> Void:
        self.x = x
        self.y = y

    public function sum( self ) -> I64:
        return self.x + self.y
```

### Constructors

Constructors are methods named identically to the class. They receive `self` as the first parameter and return `Void`:

```uranite
class Animal:
    public String name
    public I64 age

    public function Animal( self, String name, I64 age ) -> Void:
        self.name = name
        self.age = age
```

At codegen, constructors are resolved as `QualifiedName.ShortName` — for example, `uranite.examples.Animal.Animal`. Multiple constructors with different parameter counts are supported via arity-based overloading, resolved as `ConstructorName#N` where N is the total argument count including `self`.

### Property Parameters

Constructor parameters can be marked as `isProperty` in the AST, which automatically creates corresponding fields on the class. Property parameters have their own `propertyAccess` modifier. The semantic analyzer at `analyzer.cpp:1778-1797` detects these and inserts `FieldInfo` entries:

```
for constructor in declaration.methods:
    if constructor.name == declaration.name:
        for parameter in constructor.parameters:
            if parameter.isProperty and parameter.type:
                fieldInfo.name = parameter.name
                fieldInfo.type = resolveType(parameter.type)
                fieldInfo.access = parameter.propertyAccess
                fieldInfo.index = fieldIndex++
                classType.fields.push_back(fieldInfo)
```

### Fields

Fields are declared with a type and name, preceded by an access modifier:

```uranite
class Config:
    public I64 port
    public String host
    private I64 maxRetries
```

Fields require explicit visibility modifiers (`public`, `private`, or `protect`) on non-builtin classes. Missing modifiers emit a diagnostic error:

```
"field \"port\" in class \"Config\" must have an explicit visibility modifier (public, private, or protect)"
```

Field modifiers: `final` (immutable after init), `readonly` (immutable), `static` (class-level, index set to -1).

### Methods

Methods are declared with `function` keyword. Non-static methods must have `self` as their first parameter:

```uranite
class Calculator:
    public function add( self, I64 a, I64 b ) -> I64:
        return a + b

    public static function create() -> Calculator:
        return new Calculator()
```

Non-static methods without `self` emit:
```
"non-static method \"add\" in class \"Calculator\" must have \"self\" as first parameter, or be declared \"static\""
```

Static methods with `self` emit:
```
"static method \"create\" in class \"Calculator\" must not have \"self\" parameter"
```

Methods also require explicit visibility modifiers on non-builtin classes.

Method modifiers: `abstract`, `async`, `final`, `native`, `override`, `static`, `virtual`.

### Properties

Property methods use the `property` keyword instead of `function`. They act as getters/setters accessed with field syntax:

```uranite
class Circle:
    private F64 radius

    public property area( self ) -> F64:
        return 3.14159 * self.radius * self.radius
```

The `isProperty` flag on `MethodInfo` controls dispatch — property calls omit parentheses at the call site.

### Abstract Classes

Abstract classes cannot be instantiated directly. Methods marked `abstract` have no body:

```uranite
abstract class Shape:
    public abstract function area( self ) -> F64;
    public abstract function perimeter( self ) -> F64;
```

Abstract classes always get a vtable pointer in their struct layout, even without explicit interface implementation. The MIR lowering checks the class and all ancestors for `isAbstract` at `mir/lowering.cpp:125-141`.

### Final Classes

Final classes cannot be extended:

```uranite
final class Singleton:
    public function Singleton( self ) -> Void:
        pass
```

Attempting to extend a final class emits:
```
"cannot extend final class \"Singleton\""
```

### Readonly Classes

The `Readonly` modifier makes all fields immutable after construction:

```uranite
Readonly class Config:
    public I64 port
    public String host

    public function Config( self, I64 port, String host ) -> Void:
        self.port = port
        self.host = host
```

At `analyzer.cpp:1917-1921`, readonly classes propagate `isReadonly = true` to every field.

### Native Classes

Classes marked `native` indicate implementation via C runtime or FFI:

```uranite
native class FileHandle:
    public native function read( self, I64 size ) -> String;
```

The `isNative` flag is carried through HIR (`isNativeClass`) for codegen to emit extern linkage.

### Generic Classes

Classes support generic type parameters with optional constraints:

```uranite
class Box<T>:
    public T value

    public function Box( self, T value ) -> Void:
        self.value = value

    public function get( self ) -> T:
        return self.value
```

Generic parameters are registered as `GenericParameterType` instances. Constraints restrict type arguments to interfaces or traits:

```uranite
class SortedBox<T extends Comparable>:
    public T value
```

Non-interface/non-trait constraints emit a warning:
```
"generic constraint \"SomeClass\" is not an interface or trait"
```

### Forward Declarations

Classes can be forward-declared with a semicolon for mutual reference:

```uranite
class LinkedNode;
```

Forward declarations create the `ClassType` in the type registry without populating fields or methods.

### Nested Declarations

Class bodies can contain nested type declarations:

```uranite
class Container:
    public I64 size

    public class Item:
        public I64 id

    public interface Sortable:
        public function compare( self, Object other ) -> I64;

    public struct Metadata:
        public String key
        public String value

    public enum Status:
        unit Active
        unit Inactive
```

Nested classes, interfaces, structs, and enums are parsed recursively and stored in `nestedDeclarations`.

### Trait Composition

The `use` keyword imports trait fields and methods into a class:

```uranite
class Widget:
    use Printable, Cloneable
```

At `analyzer.cpp:1687-1721`, trait composition copies fields and methods from `TraitType` AST declarations into the class, skipping duplicates (existing fields/methods with matching names take precedence).

---

## Inheritance

### Single Inheritance

Uranite supports single class inheritance. A class can extend exactly one parent class:

```uranite
class Animal:
    public String name

    public function Animal( self, String name ) -> Void:
        self.name = name

    public virtual function speak( self ) -> Void:
        puts( "..." )

class Dog extends Animal:
    public String breed

    public function Dog( self, String name, String breed ) -> Void:
        self.name = name
        self.breed = breed

    public override function speak( self ) -> Void:
        puts( "Woof!" )
```

### Extends Keyword

The `extends` keyword specifies the parent class. If the specified base type resolves to an interface instead of a class, it is automatically added to the interfaces list rather than being set as `baseClass`:

```
if resolvedBaseType.kind == Interface:
    classType.interfaces.push_back(resolvedBaseType)
elif resolvedBaseType.kind == Class:
    classType.baseClass = resolvedBaseType
else:
    error: "base type is not a class or interface"
```

Multiple comma-separated types after `extends` are treated as interfaces after the first base class.

### Field Inheritance

Inherited fields are collected from all ancestors in reverse order (root ancestor first) at `analyzer.cpp:1727-1748`:

```
ancestors = []
baseTracker = classType.baseClass
while baseTracker:
    ancestors.push(baseTracker)
    baseTracker = baseTracker.baseClass

for ancestor in reversed(ancestors):
    for field in ancestor.fields:
        if field.name not in addedFields:
            classType.fields.push_back(field)
            fieldIndex++
```

This ensures field ordering matches the ancestor chain — root class fields appear first, immediate parent fields next, then own fields last. Field shadowing (declaring a field with the same name as an inherited one) emits a warning:

```
"field \"name\" in class \"Dog\" shadows inherited field"
```

### Method Override

Methods marked `override` replace parent method entries. At `analyzer.cpp:1871-1910`, the analyzer searches for existing methods with matching name and parameter types:

```
if override and baseClass exists:
    baseMethod = baseClass.findMethod(name)
    if baseMethod and baseMethod.isFinal:
        error: "cannot override final method"

existingMethod = find method with same name and matching parameter types
if existingMethod:
    *existingMethod = newMethodInfo    // replace in-place
else:
    classType.methods.push_back(newMethodInfo)    // add new
```

Method overloading by parameter arity is supported — methods with the same name but different parameter counts coexist.

### Final Method Protection

Methods marked `final` cannot be overridden. Attempting to override emits:

```
"cannot override final method \"speak\" from class \"Animal\""
```

---

## Interface Implementation

### Implements Keyword

Classes implement interfaces via the `implements` keyword:

```uranite
class ArrayList<E> implements Iterable<E>, Indexable<E>:
    ...
```

Non-interface types after `implements` emit:
```
"\"SomeClass\" is not an interface"
```

### Interface Validation

After vtable construction, `validateInterfaceImplementation()` at `analyzer.cpp:5289` checks that every interface method has a corresponding implementation in the class. Missing methods emit diagnostic errors.

### Droper Interface

When a class implements the `Droper` interface (qualified name from `qualnames.hpp`), `implementsDroper` is set to `true`:

```
for interface in classType.interfaces:
    if interface.qualified == qname::Droper or interface.name == "Droper":
        classType.implementsDroper = true
```

This flag enables automatic cleanup — the compiler inserts `drop()` calls when objects leave scope.

---

## AST Representation

### ClassDeclaration Node

Defined in `src/uranite/ast/node.hpp:859-906`:

```
struct ClassDeclaration : Declaration
    baseClassType      : TypeNodeSharedPointer
    fields             : std::vector<FieldDeclarationSharedPointer>
    genericParameters  : std::vector<GenericParameterSharedPointer>
    interfaces         : std::vector<TypeNodeSharedPointer>
    isAbstract         : bool (default false)
    isFinal            : bool (default false)
    isNative           : bool (default false)
    isReadonly         : bool (default false)
    methods            : std::vector<DeclarationSharedPointer>
    name               : std::string
    nestedDeclarations : std::vector<DeclarationSharedPointer>
    usedTraits         : std::vector<std::string>
```

### FieldDeclarationNode

Defined in `src/uranite/ast/node.hpp:820-854`:

```
struct FieldDeclarationNode : Node
    access       : AccessModifier (default Default)
    defaultValue : ExpressionSharedPointer
    isFinal      : bool (default false)
    isReadonly   : bool (default false)
    isStatic     : bool (default false)
    name         : std::string
    type         : TypeNodeSharedPointer
```

### FunctionParameterNode — Property Fields

Defined in `src/uranite/ast/node.hpp:729-779`:

```
struct FunctionParameterNode : Node
    propertyAccess : AccessModifier (default Default)
    defaultValue   : ExpressionSharedPointer
    isMutable      : bool (default false)
    isProperty     : bool (default false)
    isReadonly     : bool (default false)
    isReference    : bool (default false)
    isSelf         : bool (default false)
    isVariadic     : bool (default false)
    isKeyword      : bool (default false)
    name           : std::string
    type           : TypeNodeSharedPointer
```

When `isProperty` is true, the parameter automatically generates a `FieldInfo` on the class during semantic analysis.

---

## Parsing Implementation

### Class Header Parsing

The parser at `parser.cpp:574-598` handles class declaration headers:

```
parseClassDeclaration(access):
    expect(KeywordClass)
    name = expect(Identifier)
    declaration.access = access
    declaration.genericParameters = parseGenericParameters()
    if check(KeywordExtends):
        advance()
        declaration.baseClassType = parseTypeNode()
        while match(Comma):
            declaration.interfaces.push_back(parseTypeNode())
    if check(KeywordImplements):
        advance()
        declaration.interfaces.push_back(parseTypeNode())
        while match(Comma):
            declaration.interfaces.push_back(parseTypeNode())
    if match(Semicolon):
        return declaration    // forward declaration
    expect(Colon)
    expectNewline()
```

Header order: `[modifier] class Name<Generic> extends Base implements Interface1, Interface2:`.

Note: Multiple types after `extends` are parsed as additional interfaces via the comma loop. The first type becomes `baseClassType`, subsequent comma-separated types go into `interfaces`.

### Class Body Parsing

Inside the indented body at `parser.cpp:600-723`:

```
while not Dedent or Eof:
    memberAccess = parseAccessModifier()
    parse member modifiers (abstract, async, final, native, override, readonly, static, virtual)

    if KeywordFunction or KeywordProperty:
        method = parseFunctionDeclaration(access, virtual, override, abstract, static)
        if property: method.isProperty = true
        if async: method.isAsync = true
        if final: method.isFinal = true
        if native: method.isNative = true
        declaration.methods.push_back(method)
    elif KeywordClass:
        nestedClass = parseClassDeclaration(memberAccess)
        declaration.nestedDeclarations.push_back(nestedClass)
    elif KeywordInterface:
        declaration.nestedDeclarations.push_back(parseInterfaceDeclaration(memberAccess))
    elif KeywordStruct:
        declaration.nestedDeclarations.push_back(parseStructDeclaration(memberAccess))
    elif KeywordEnum:
        declaration.nestedDeclarations.push_back(parseEnumDeclaration(memberAccess))
    elif KeywordUse:
        advance()
        traitName = expect(Identifier)
        declaration.usedTraits.push_back(traitName)
        while match(Comma):
            declaration.usedTraits.push_back(expect(Identifier))
    elif Identifier or Question:
        field = parseFieldDeclaration(memberAccess)
        if final: field.isFinal = true
        if readonly: field.isReadonly = true
        if static: field.isStatic = true
        declaration.fields.push_back(field)
    else:
        error: "unexpected token in class body"
```

### Member Modifier Parsing

Member modifiers are parsed in a loop before the member declaration. The parser recognizes: `abstract`, `async`, `constant` (consumed but unused), `final`, `native`, `override`, `readonly`, `static`, `virtual`. Multiple modifiers can combine — `public virtual override function` is valid.

---

## Semantic Analysis

### Two-Pass Registration

**Pass 1 — Module Registration** (`analyzer.cpp:441-450`): Calls `populateClassMembers()` at `analyzer.cpp:4304-4383` to eagerly populate fields and methods for cross-reference resolution. Resolves base class, interfaces, collects fields (including from ancestor chain), and registers method signatures.

**Pass 2 — Full Analysis** (`analyzer.cpp:1625-1942`): Complete analysis with constraint checking, scope management, type validation, vtable construction, memory layout, and interface validation.

### Analysis Order

Classes are analyzed in topological order at `analyzer.cpp:1037-1063`:

1. Structs first
2. Classes without a base class (no `extends`)
3. Enums
4. Classes with a base class (have `extends`)

This ensures parent types are fully analyzed before child types access inherited fields and methods.

### Full Class Analysis

The `analyzeClassDeclaration()` at `analyzer.cpp:1625-1942` performs:

1. Look up `ClassType` from registry
2. Set `isAbstract`, `isFinal`, `isReadonly` flags
3. Register generic parameters with constraints
4. Resolve base class — add to `interfaces` if interface, set `baseClass` if class, error otherwise
5. Resolve `implements` interfaces — error if not interface kind
6. Apply `use` traits — copy missing fields and methods from trait AST
7. Push class scope with `classType` reference
8. Collect inherited fields from ancestor chain (root-first)
9. Declare own fields (require explicit visibility, warn on shadow)
10. Extract property parameters from constructors as fields
11. Analyze methods (self checks, generic parameters, overload resolution)
12. Build vtable
13. Compute memory layout
14. Analyze nested declarations
15. Validate interface implementations
16. Check Droper interface
17. Pop scope and restore generic parameters

### Generic Parameter Registration

At `analyzer.cpp:1636-1654`, generic parameters are saved, registered as `GenericParameterType`, resolved with constraints:

```
for genericParameter in declaration.genericParameters:
    save existing type binding
    genericParameterType = GenericParameterType(name)
    for constraint in genericParameter.constraints:
        resolvedConstraint = resolveType(constraint)
        if resolvedConstraint.kind not in {Interface, Trait}:
            warning: "generic constraint is not an interface or trait"
        genericParameterType.constraints.push_back(resolvedConstraint)
    classType.genericParameters.push_back(genericParameterType)
    typeRegistry.register(name, genericParameterType)
```

Static methods temporarily unregister class-level generic parameters to prevent invalid references.

### Base Class Resolution

At `analyzer.cpp:1655-1673`:

```
if declaration.baseClassType:
    resolvedBaseType = resolveType(declaration.baseClassType)
    if resolvedBaseType.kind == Interface:
        classType.interfaces.push_back(resolvedBaseType)
    elif resolvedBaseType.kind == Class:
        classType.baseClass = resolvedBaseType
        if baseClass.isFinal:
            error: "cannot extend final class"
    else:
        error: "base type is not a class or interface"
```

### Field Analysis

At `analyzer.cpp:1749-1776`:

```
for fieldDeclaration in declaration.fields:
    if not builtin and access == Default:
        error: "field must have explicit visibility modifier"
    if field.name matches inherited field:
        warning: "field shadows inherited field"
    fieldInfo.type = resolveType(field.type)
    fieldInfo.access = field.access
    fieldInfo.isStatic = field.isStatic
    if static:
        fieldInfo.index = -1
    else:
        fieldInfo.index = fieldIndex++
    classType.fields.push_back(fieldInfo)
    scope.define(field.name, fieldSymbol)
```

### Method Analysis

At `analyzer.cpp:1799-1911`:

- Non-static methods without `self` parameter emit error
- Static methods with `self` parameter emit error
- Method-level generic parameters registered and resolved
- Parameter types and return type resolved
- `MethodInfo` constructed with all modifier flags
- Override checks: if overriding, look up base method — error if base method is `isFinal`
- Overload resolution: find existing method with matching name AND matching parameter types (by count and type string comparison). If found, replace in-place. If not found, append.

### Virtual Table Construction

`buildVTable()` at `analyzer.cpp:4075-4098`:

```
if baseClass exists:
    classType.virtualTable = copy of baseClass.virtualTable

for method in classType.methods:
    if not method.isVirtual and not method.isOverride:
        continue
    found = false
    for vtableIndex, vtableEntry in classType.virtualTable:
        if vtableEntry.name == method.name:
            classType.virtualTable[vtableIndex] = &method
            method.virtualTableIndex = vtableIndex
            found = true
            break
    if not found:
        method.virtualTableIndex = virtualTable.size()
        classType.virtualTable.push_back(&method)
```

The vtable is inherited from the base class, then updated with new or overriding virtual methods. Non-virtual, non-override methods are excluded.

### Memory Layout Computation

`computeMemoryLayout()` at `analyzer.cpp:4264-4296`:

```
currentOffset = 0
if virtualTable not empty:
    currentOffset = 8    // vtable pointer

for field in classType.fields:
    fieldSize = 8        // default
    fieldAlign = 8       // default

    if Integer:    fieldSize = bitWidth / 8, fieldAlign = fieldSize
    if Float:      fieldSize = bitWidth / 8, fieldAlign = fieldSize
    if Bool:       fieldSize = 1, fieldAlign = 1
    if Char:       fieldSize = 4, fieldAlign = 4

    currentOffset = (currentOffset + fieldAlign - 1) & ~(fieldAlign - 1)
    currentOffset += fieldSize

classType.objectSize = (currentOffset + alignment - 1) & ~(alignment - 1)
```

| Type | Size | Alignment |
|---|---|---|
| `I8` / `U8` | 1 byte | 1 |
| `I16` / `U16` | 2 bytes | 2 |
| `I32` / `U32` | 4 bytes | 4 |
| `I64` / `U64` | 8 bytes | 8 |
| `F32` | 4 bytes | 4 |
| `F64` | 8 bytes | 8 |
| `Boolean` | 1 byte | 1 |
| `Char` | 4 bytes | 4 |
| Pointers / References / Classes | 8 bytes | 8 |

Vtable pointer (when present) always occupies bytes 0-7.

---

## Compilation Pipeline

### HIR Stage

HIR lowering at `hir/lowering.cpp:298-390` creates `HIRClassDefinition`:

```
struct HIRClassDefinition : HIRNode
    className                          : std::string
    classQualifiedName                 : std::string
    parentClassQualifiedName           : std::string
    accessModifier                     : AccessModifier
    fieldDescriptors                   : std::vector<HIRFieldDescriptor>
    methodDefinitions                  : std::vector<HIRFunctionDefinition>
    implementedInterfaceQualifiedNames : std::vector<std::string>
    genericParameters                  : std::vector<HIRGenericParameterDescriptor>
    nestedDeclarations                 : std::vector<HIRNodeSharedPointer>
    usedTraitNames                     : std::vector<std::string>
    isFinalClass                       : bool
    isAbstractClass                    : bool
    isReadonlyClass                    : bool
    isNativeClass                      : bool
```

Each field is lowered to `HIRFieldDescriptor`:

```
struct HIRFieldDescriptor
    fieldName  : std::string
    fieldType  : TypeSharedPointer
    fieldIndex : int (default -1)
    fieldAccess: AccessModifier
```

Method lowering sets `ownerClassName` to the class qualified name and resolves `self` parameter types to the class type.

### MIR Stage — Type Layout Descriptor

MIR lowering at `mir/lowering.cpp:82-162` creates a `TypeLayoutDescriptor` per class:

```
typeLayout.typeQualifiedName = classQualifiedName
typeLayout.hasVirtualTable = (interfaces non-empty)
    // except: Args<T>, error hierarchy classes excluded
    // abstract classes and ancestors of abstract classes: forced true

if hasVirtualTable:
    fieldNames.push_back("__vtable")
    fieldTypes.push_back(PointerType)
    fieldIndex++

for field in classDefinition.fieldDescriptors:
    fieldByteOffsets.push_back(fieldIndex * 8)
    fieldNames.push_back(field.fieldName)
    fieldTypes.push_back(field.fieldType)
    fieldIndex++

typeLayout.typeSizeInBytes = fieldIndex * 8
typeLayout.typeAlignmentInBytes = 8
typeLayoutTable[layoutKey] = typeLayout
```

The vtable decision examines:
1. Interface list non-empty (primary trigger)
2. Except `Args<T>` (variadic parameter struct, never dispatched)
3. Except error hierarchy classes (Exception, Error, etc.)
4. Abstract classes always get vtable, even without interfaces
5. Non-abstract classes inheriting from abstract ancestors get vtable

### MIR Stage — Inheritance Layout Merge

At `mir/lowering.cpp:168-206`, child class layouts merge parent fields:

```
for class with parentClassQualifiedName:
    parentLayout = typeLayoutTable[parentName]
    childLayout = typeLayoutTable[childName]

    merged = []
    for field in parentLayout:
        merged.push_back(field)

    for field in childLayout:
        if field == "__vtable" and parent has vtable:
            skip    // avoid duplicate vtable slot
        merged.push_back(field)

    childLayout.fields = merged
    childLayout.typeSizeInBytes = merged.size() * 8
```

Parent fields appear first in the merged layout, preserving field index compatibility with base class code.

### MIR Stage — Method Lowering

At `mir/lowering.cpp:730-743`:

```
lowerClassDefinition(hirClass):
    currentClassName = hirClass.classQualifiedName
    currentParentClassName = hirClass.parentClassQualifiedName
    for method in hirClass.methodDefinitions:
        lowerFunctionDefinition(method)
    restore className, parentClassName
```

Each method is lowered as a standard function with the class context available for `self` resolution.

### Codegen Stage — LLVM Struct Type Creation

At `codegen.cpp:148-185`, LLVM struct types are created from the type layout table:

```
for (typeName, typeLayout) in mirModule.typeLayoutTable:
    canonicalName = typeLayout.typeQualifiedName or typeName
    if already processed: alias and skip
    if StructType::getTypeByName exists: cache and skip

    fieldLLVMTypes = []
    for fieldType in typeLayout.fieldTypes:
        llvmType = toLLVMType(fieldType)
        if void: use opaque pointer
        fieldLLVMTypes.push_back(llvmType)

    if empty: push i8 (empty structs need at least one element)

    structType = StructType::create(context, fieldLLVMTypes, canonicalName)
    structTypeCache[canonicalName] = structType
```

Classes with vtable get their first field type resolved to a pointer (`__vtable` field).

### Codegen Stage — Interface Table Generation

At `codegen.cpp:380-468`, interface tables (itables) are generated per class:

```
for (className, classType) in userTypes where kind == Class:
    if classType.interfaces empty: skip
    for interface in classType.interfaces:
        itableEntries = []
        for methodName in interface.methodOrder:
            implFunc = find function "ClassName.methodName" in functionResolutionMap
            if found: itableEntries.push_back(bitcast(implFunc, funcPtrType))
            else: itableEntries.push_back(null)

        itableGlobal = GlobalVariable(
            ArrayType(funcPtrType, entryCount),
            InternalLinkage,
            "_MIR_itable_ClassName_InterfaceName"
        )
        interfaceTableMap[className] = itableGlobal
```

Each itable is a constant array of function pointers, one per interface method, ordered by the interface's `methodOrder`. Methods not found get null entries.

### Codegen Stage — ConstructObject

`generateConstructObject()` at `codegen.cpp:6132-6431` handles object creation:

**Special intrinsics:**
- `Memory<T>`: `calloc(capacity, elementSize)` — raw typed pointer, no struct
- `Arena<T>`: Struct `{ptr, count, capacity}` — arena allocator
- OOP wrappers (`Integer`, `Float`, etc.): Unwrap to primitive value

**General classes:**

```
structType = structTypeCache[typeName]
mallocFunction = getOrCreateMalloc()
typeSize = max(dataLayout.getTypeAllocSize(structType), 64)
rawPointer = call malloc(typeSize)
typedPointer = bitcast(rawPointer, pointer-to-structType)

if class has vtable:
    itableGlobal = interfaceTableMap[typeName]
    vtableSlot = CreateStructGEP(structType, typedPointer, 0)
    CreateStore(bitcast(itableGlobal, ptr), vtableSlot)

constructorFunction = find "QualifiedName.ShortName"
if arity mismatch: try "QualifiedName.ShortName#N"
if constructorFunction found:
    args = [typedPointer (self), ...sourceOperands]
    call constructorFunction(args)
```

Minimum allocation is 64 bytes. Constructor resolution tries multiple naming patterns (qualified, short, arity-suffixed) to find the right overload.

### Codegen Stage — Field Access

`generateComputeFieldAddress()` at `codegen.cpp:5710-5919`:

```
basePointer = loadVariableValue(sourceOperands[0])

// resolve struct type from multiple sources:
// 1. AllocaInst allocated type
// 2. GEP result element type
// 3. Variable descriptor type → structTypeCache lookup
// 4. concreteClassMap → structTypeCache lookup
// 5. Brute-force layout table field name search

if structType found:
    fieldIndex = instruction.fieldLayoutIndex
    if fieldIndex out of bounds:
        search layout table for fieldAccessName → correct index

    fieldPointer = CreateStructGEP(structType, basePointer, fieldIndex)
    setVariableValue(destination, fieldPointer)
```

Field access generates a GEP (GetElementPtr) instruction into the struct at the field's index, returning a pointer to the field value.

### Codegen Stage — Virtual Dispatch

At `codegen.cpp:4330-4390`, interface method calls use vtable dispatch:

```
receiverPtr = loadVariableValue(sourceOperands[0])

// load vtable pointer from first struct field
vtableSlotPtr = CreateStructGEP(wrapperStruct, receiverPtr, 0)
vtablePtr = CreateLoad(ptrType, vtableSlotPtr)

// index into vtable by method slot
funcSlotPtr = CreateGEP(funcPtrType, vtablePtr, methodSlotIndex)
funcPtr = CreateLoad(funcPtrType, funcSlotPtr)

// cast to expected function type and call
castedFunc = CreateBitCast(funcPtr, ptr-to-callType)
callResult = CreateCall(callType, castedFunc, [receiverPtr, ...args])
```

The vtable is accessed via the first struct field (index 0). The method slot index comes from the interface's `methodOrder`. This enables runtime polymorphism — the same call site dispatches to different implementations depending on the concrete class.

### Codegen Stage — toLLVMType

At `codegen.cpp:6630-6646`, class types map to LLVM types:

```
case Kind::Class:
    if String: return PointerType::getUnqual    // string is opaque pointer
    if Void: return Type::getVoidTy
    if Object: return PointerType::getUnqual    // universal supertype
    structType = StructType::getTypeByName(className)
    if found: return PointerType::getUnqual(structType)
    return PointerType::getUnqual    // fallback opaque pointer
```

All class types are pointer types at LLVM level — either typed pointers to named struct types or opaque pointers for unresolved types.

---

## Assignability Rules

| Assignment | Rule |
|---|---|
| `ClassName` → `ClassName` | Identity match via `equals()` or qualified name comparison |
| `ChildClass` → `ParentClass` | Allowed via inheritance chain |
| `ClassName` → `InterfaceName` | Allowed if class implements interface |
| `ClassName` → `Object` | Always allowed (universal supertype) |
| `GenericParameter` | Always assignable |
| `ClassName` → unrelated `ClassName` | Not allowed |

Operator dispatch on class types uses interface implementation checks:
- Equality (`==`, `!=`): class must implement `Equatable` or be an OOP wrapper
- Comparison (`<`, `>`, `<=`, `>=`): class must implement `Comparable` or be an OOP wrapper
- Containment (`in`): class must implement `Indexable`

---

## Examples

### Basic Classes

```uranite
from uranite.io.console import puts

class Point:

    public I64 x
    public I64 y

    public function Point( self, I64 x, I64 y ) -> Void:
        self.x = x
        self.y = y

    public function sum( self ) -> I64:
        return self.x + self.y

public function main() -> I32:
    Point pt = new Point( 10, 20 )
    puts( "Point(", pt.x, ", ", pt.y, ") sum = ", pt.sum() )
    return 0
```

`new Point(10, 20)` emits `ConstructObject` with the constructor args. The struct is `malloc`-allocated with fields `x` (index 0) and `y` (index 1). Field access `pt.x` generates `CreateStructGEP(PointStruct, ptPtr, 0)`.

### Inheritance and Override

```uranite
from uranite.io.console import puts

class Animal:

    public String name
    public I64 age

    public function Animal( self, String name, I64 age ) -> Void:
        self.name = name
        self.age = age

    public virtual function speak( self ) -> Void:
        puts( "..." )

class Dog extends Animal:

    public String breed

    public function Dog( self, String name, I64 age, String breed ) -> Void:
        self.name = name
        self.age = age
        self.breed = breed

    public override function speak( self ) -> Void:
        puts( "Woof!" )

class Cat extends Animal:

    public function Cat( self, String name, I64 age ) -> Void:
        self.name = name
        self.age = age

    public override function speak( self ) -> Void:
        puts( "Meow!" )

public function main() -> I32:
    Dog dog = new Dog( "Rex", 5, "Husky" )
    puts( "Dog: ", dog.name, ", age ", dog.age )
    dog.speak()
    Cat cat = new Cat( "Whiskers", 3 )
    cat.speak()
    return 0
```

`Dog` inherits `name` and `age` fields from `Animal`. Its struct layout is: `[name, age, breed]` (parent fields first). `speak()` is virtual on `Animal` and overridden on `Dog` and `Cat`. If called through an `Animal` reference with interface dispatch, the vtable selects the correct override at runtime.

### Readonly and Generic Classes

```uranite
from uranite.io.console import puts

Readonly class Config:

    public I64 port
    public String host

    public function Config( self, I64 port, String host ) -> Void:
        self.port = port
        self.host = host

    public function describe( self ) -> Void:
        puts( "Config: ", self.host, ":", self.port )

class Box<T>:

    public T value

    public function Box( self, T value ) -> Void:
        self.value = value

    public function get( self ) -> T:
        return self.value

public function main() -> I32:
    Config cfg = new Config( 8080, "localhost" )
    cfg.describe()
    Box<I64> intBox = new Box<I64>( 42 )
    puts( "Box value = ", intBox.get() )
    return 0
```

`Config` is readonly — all fields have `isReadonly = true` after semantic analysis. `Box<T>` is a generic class — `Box<I64>` monomorphizes `T` to `I64`, and the `value` field stores an `I64`.

### Forward Declaration and Nested Classes

```uranite
from uranite.io.console import puts

class LinkedNode;

class Container:

    public I64 size

    public class Item:
        public I64 id

class LinkedNode:

    public I64 value

    public function LinkedNode( self, I64 v ) -> Void:
        self.value = v

public function main() -> I32:
    LinkedNode node = new LinkedNode( 99 )
    puts( "LinkedNode value = ", node.value )
    return 0
```

`LinkedNode` is forward-declared (semicolon, no body), then fully defined later. `Container.Item` is a nested class declaration — parsed recursively and stored in `nestedDeclarations`. Both compile through the same struct creation and field access pipeline.
