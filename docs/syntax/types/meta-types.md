# Meta Types, instanceof, and subclassof

Uranite provides three type-level operations that treat types as first-class entities at the semantic layer: `Meta<T>` (type-as-value), `instanceof` (runtime type checking), and `subclassof` (compile-time inheritance testing). `Meta<T>` wraps any type into a value that can be stored in variables, passed as arguments, and compared — created by the `TypeReferenceExpression` AST node when a type name appears in expression context, resolved to `MetaType` by the semantic analyzer via `Registry::makeMeta`. `instanceof` checks whether a value belongs to a specific type, producing a `Boolean` result through `InstanceofExpression` AST, `HIRInstanceOf` HIR, `InstanceOfCheck` MIR instruction, and compile-time LLVM constant folding with inheritance chain traversal. `subclassof` checks inheritance relationships between two types, producing a `Boolean` via `SubclassofExpression` AST and `HIRSubclassOf` HIR — currently without MIR lowering or codegen implementation.

This document covers all three type-level operations: `Meta<T>` type wrapping with `MetaType` struct and covariant assignability, `instanceof` from parsing through MIR codegen with compile-time constant folding, and `subclassof` with its current AST/semantic/HIR-only implementation status.

---

## Table of Contents

- [Meta Types — Type as Value](#meta-types--type-as-value)
  - [Overview](#overview)
  - [Syntax](#syntax)
  - [AST Representation — TypeReferenceExpression](#ast-representation--typereferenceexpression)
  - [Semantic Analysis](#semantic-analysis)
  - [MetaType Struct](#metatype-struct)
  - [Registry::makeMeta](#registrymakemeta)
  - [Assignability Rules](#assignability-rules)
  - [Comparability Rules](#comparability-rules)
  - [Variable Declaration Auto-Wrapping](#variable-declaration-auto-wrapping)
  - [HIR Stage — HIRTypeReference](#hir-stage--hirtypereference)
  - [MIR and Codegen Status](#mir-and-codegen-status)
- [instanceof Operator](#instanceof-operator)
  - [Syntax](#instanceof-syntax)
  - [AST Representation — InstanceofExpression](#ast-representation--instanceofexpression)
  - [Parsing](#instanceof-parsing)
  - [Semantic Analysis](#instanceof-semantic-analysis)
  - [HIR Stage — HIRInstanceOf](#hir-stage--hirinstanceof)
  - [MIR Lowering — InstanceOfCheck](#mir-lowering--instanceofcheck)
  - [MIR Codegen — Compile-Time Constant Folding](#mir-codegen--compile-time-constant-folding)
- [subclassof Operator](#subclassof-operator)
  - [Syntax](#subclassof-syntax)
  - [AST Representation — SubclassofExpression](#ast-representation--subclassofexpression)
  - [Parsing](#subclassof-parsing)
  - [Semantic Analysis](#subclassof-semantic-analysis)
  - [HIR Stage — HIRSubclassOf](#hir-stage--hirsubclassof)
  - [MIR and Codegen Status](#subclassof-mir-and-codegen-status)
- [Type Kind Enumeration](#type-kind-enumeration)
- [Examples](#examples)

---

## Meta Types — Type as Value

### Overview

`Meta<T>` represents a type itself used as a value. When a type name appears in expression context (not as a type annotation), the semantic analyzer wraps it in `MetaType`, producing a value whose semantic type is `Meta<T>` where `T` is the referenced type. This enables storing types in variables, passing them as function arguments, and performing type comparisons at the value level.

### Syntax

```
Meta<String> myType = String
Meta<I64> numericType = I64

function acceptType(Meta<Object> typeValue) -> Void:
    pass
```

When a bare type name appears on the right side of an assignment or in expression context, it becomes a `TypeReferenceExpression` that resolves to `Meta<ThatType>`.

### AST Representation — TypeReferenceExpression

Defined in `src/uranite/ast/node.hpp:1866-1883`:

```cpp
struct TypeReferenceExpression : Expression {

    std::string typeName;

    TypeReferenceExpression(
        const std::string& name,
        const lookup::SourceSharedPointer& source
    ) : Expression( Node::Kind::TypeReferenceExpression,
            source ),
        typeName( name ) {
    }

};
```

The `TypeReferenceExpression` stores only the type name as a string. It represents the appearance of a type identifier in expression position — not a type annotation. The semantic analyzer resolves this name and wraps the result in `MetaType`.

### Semantic Analysis

At `src/uranite/semantic/analyzer.cpp:2426-2438`:

```cpp
case ast::Node::Kind::TypeReferenceExpression: {
    ast::nodes::TypeReferenceExpression&
        typeReferenceExpression =
        static_cast<ast::nodes::TypeReferenceExpression&>(
            *expression );
    TypeSharedPointer resolvedReferenceType =
        this->typeRegistry.lookupType(
            typeReferenceExpression.typeName );
    if( resolvedReferenceType ) {
        expressionType =
            this->typeRegistry.makeMeta(
                resolvedReferenceType );
    }
    else {
        std::string unknownTypeErrorMessage =
            fmt::format( "unknown type \"{}\"",
                typeReferenceExpression.typeName );
        this->diagnostic.error( expression->source,
            unknownTypeErrorMessage );
        expressionType = this->typeRegistry.getError();
    }
    break;
}
```

Resolution steps:
1. Extract the `typeName` from `TypeReferenceExpression`.
2. Look up the type in the registry via `lookupType` — follows the standard 3-tier resolution (user types, aliases, primitives).
3. If found, wrap in `Meta<T>` via `Registry::makeMeta`.
4. If not found, emit diagnostic error and fall back to `Error` sentinel type.

### MetaType Struct

Defined in `src/uranite/semantic/typeref.hpp:1016-1028`:

```cpp
struct MetaType : Type {

    TypeSharedPointer innerType;

    MetaType( TypeSharedPointer innerType )
        : Type( Type::Kind::Meta,
            fmt::format( "Meta<{}>",
                innerType->name ) ),
          innerType( std::move( innerType ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `innerType` | `TypeSharedPointer` | The wrapped type that this meta-type represents as a value |

The `name` is constructed as `"Meta<InnerTypeName>"` using `fmt::format`. The `Kind` is `Type::Kind::Meta` (enum value at `typeref.hpp:58`).

### Registry::makeMeta

Factory method at `src/uranite/semantic/typeref.cpp:628-630`:

```cpp
TypeSharedPointer Registry::makeMeta(
    TypeSharedPointer inner ) {
    return std::make_shared<MetaType>(
        std::move( inner ) );
}
```

Declared in `src/uranite/semantic/typeref.hpp:1413`. Creates a fresh `MetaType` wrapping the provided inner type. No caching or deduplication — each call produces a new `MetaType` instance. Follows the same pattern as `makeOptional`, `makeFuture`, and `makeGenerator`.

### Assignability Rules

At `src/uranite/semantic/typeref.cpp:467-471`:

```cpp
if( target->kind == Type::Kind::Meta &&
    source->kind == Type::Kind::Meta ) {
    MetaTypeSharedPointer targetMetaType =
        std::static_pointer_cast<MetaType>( target );
    MetaTypeSharedPointer sourceMetaType =
        std::static_pointer_cast<MetaType>( source );
    return this->isAssignable(
        targetMetaType->innerType,
        sourceMetaType->innerType );
}
```

`Meta<T>` assignability is **covariant**: `Meta<A>` is assignable to `Meta<B>` if and only if `A` is assignable to `B`. This means `Meta<String>` is assignable to `Meta<Object>` (since `String` is assignable to `Object`), but `Meta<Object>` is not assignable to `Meta<String>`.

Both operands must have `Kind::Meta` — there is no implicit wrapping or unwrapping at the assignability level. The wrapping logic exists only in variable declaration analysis (see below).

### Comparability Rules

At `src/uranite/semantic/typeref.cpp:576-578`:

```cpp
if( x->kind == Type::Kind::Meta &&
    y->kind == Type::Kind::Meta ) {
    return true;
}
```

All `Meta<T>` values are comparable with each other, regardless of inner type. `Meta<String>` can be compared with `Meta<I64>` — the comparison checks type identity, not type compatibility. This unconditional comparability enables runtime type comparison patterns.

### Variable Declaration Auto-Wrapping

At `src/uranite/semantic/analyzer.cpp:4040-4041`:

```cpp
if( variableType &&
    variableType->kind == Type::Kind::Meta &&
    initializerType &&
    initializerType->kind != Type::Kind::Meta ) {
    initializerType =
        this->typeRegistry.makeMeta( initializerType );
}
```

When a variable is declared with an explicit `Meta<T>` type annotation and the initializer expression resolves to a non-Meta type, the analyzer auto-wraps the initializer type in `Meta<>`. This enables the pattern:

```
Meta<String> myType = String
```

Where `String` on the right side resolves as a type reference but might not yet be wrapped — the analyzer ensures the initializer type is lifted to `Meta<String>` before the assignability check at `analyzer.cpp:4046`.

### HIR Stage — HIRTypeReference

Defined in `src/uranite/ir/hir.hpp:933-945`:

```cpp
struct HIRTypeReference : HIRNode {

    std::string referencedTypeName;

    HIRTypeReference(
        const std::string& referencedTypeName,
        semantic::TypeSharedPointer resolvedType,
        const lookup::SourceSharedPointer& sourceLocation
    ) : HIRNode( HIRNodeKind::TypeReference,
            std::move( resolvedType ), sourceLocation ),
        referencedTypeName( referencedTypeName ) {
    }

};
```

HIR lowering at `src/uranite/ir/hir/lowering.cpp:1097-1099`:

```cpp
case ast::Node::Kind::TypeReferenceExpression: {
    ast::nodes::TypeReferenceExpression& typeReference =
        static_cast<ast::nodes::TypeReferenceExpression&>(
            *expression );
    return std::make_shared<HIRTypeReference>(
        typeReference.typeName,
        expression->semanticType,
        expression->source );
}
```

The HIR node preserves the type name string and carries the resolved `MetaType` as `resolvedType`.

### MIR and Codegen Status

`HIRTypeReference` has **no corresponding MIR lowering or codegen implementation**. The `TypeReference` HIR node kind is not handled in the MIR lowering switch statement at `src/uranite/ir/mir/lowering.cpp`. This means `Meta<T>` values exist only at the semantic and HIR levels — they are not yet emitted as runtime values in the MIR/LLVM pipeline.

---

## instanceof Operator

### Syntax {#instanceof-syntax}

```
if value instanceof MyClass:
    pass

Boolean check = someObject instanceof String
```

The `instanceof` operator tests whether a value's type matches a specified target type. Returns `Boolean`.

### AST Representation — InstanceofExpression

Defined in `src/uranite/ast/node.hpp:1479-1502`:

```cpp
struct InstanceofExpression : Expression {

    ExpressionSharedPointer object;
    TypeNodeSharedPointer targetType;

    InstanceofExpression(
        ExpressionSharedPointer object,
        TypeNodeSharedPointer targetType,
        const lookup::SourceSharedPointer& source
    ) : Expression( Node::Kind::InstanceofExpression,
            source ),
        object( std::move( object ) ),
        targetType( std::move( targetType ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `object` | `ExpressionSharedPointer` | Value expression being type-checked |
| `targetType` | `TypeNodeSharedPointer` | Type to check against |

### Parsing {#instanceof-parsing}

At `src/uranite/parser/parser.cpp:2161-2166`:

```cpp
if( kind == token::Type::KeywordInstanceOf ) {
    lookup::SourceSharedPointer source =
        this->current().source;
    this->advance();
    ast::nodes::TypeNodeSharedPointer type =
        this->parseTypeNode();
    left = std::make_shared<
        ast::nodes::InstanceofExpression>(
        left, type, source );
    continue;
}
```

Parsed as an infix operator. The left operand is the value expression, the right operand is a type node parsed via `parseTypeNode()`. Sits in the binary expression parsing loop alongside `as` (cast) and `subclassof`.

### Semantic Analysis {#instanceof-semantic-analysis}

At `src/uranite/semantic/analyzer.cpp:2321-2326`:

```cpp
case ast::Node::Kind::InstanceofExpression: {
    ast::nodes::InstanceofExpression&
        instanceOfExpression =
        static_cast<ast::nodes::InstanceofExpression&>(
            *expression );
    this->analyzeExpression(
        instanceOfExpression.object );
    this->resolveType(
        instanceOfExpression.targetType );
    expressionType = this->typeRegistry.getBool();
    break;
}
```

Analysis steps:
1. Analyze the object expression (for side effects and type resolution).
2. Resolve the target type annotation.
3. Result type is unconditionally `Boolean` — no compile-time validation of whether the check is meaningful.

### HIR Stage — HIRInstanceOf

Defined in `src/uranite/ir/hir.hpp:948-962`:

```cpp
struct HIRInstanceOf : HIRNode {

    HIRNodeSharedPointer checkedExpression;
    semantic::TypeSharedPointer checkedType;

    HIRInstanceOf(
        HIRNodeSharedPointer checkedExpression,
        semantic::TypeSharedPointer checkedType,
        const lookup::SourceSharedPointer& sourceLocation
    ) : HIRNode( HIRNodeKind::InstanceOf,
            nullptr, sourceLocation ),
        checkedExpression(
            std::move( checkedExpression ) ),
        checkedType( std::move( checkedType ) ) {
    }

};
```

HIR lowering at `src/uranite/ir/hir/lowering.cpp:1101-1105`:

```cpp
case ast::Node::Kind::InstanceofExpression: {
    ast::nodes::InstanceofExpression&
        instanceofExpression =
        static_cast<ast::nodes::InstanceofExpression&>(
            *expression );
    HIRNodeSharedPointer checkedExpression =
        this->lowerExpression(
            instanceofExpression.object );
    semantic::TypeSharedPointer checkedType =
        this->resolveTypeNode(
            instanceofExpression.targetType );
    return std::make_shared<HIRInstanceOf>(
        std::move( checkedExpression ),
        checkedType, expression->source );
}
```

### MIR Lowering — InstanceOfCheck

At `src/uranite/ir/mir/lowering.cpp:2479-2489`:

```cpp
case hir::HIRNodeKind::InstanceOf: {
    hir::HIRInstanceOf& instanceNode =
        static_cast<hir::HIRInstanceOf&>(
            *hirExpression );
    MIRVariableIdentifier checkedVariable =
        this->lowerExpression(
            instanceNode.checkedExpression );
    MIRInstruction checkInstruction(
        MIRInstructionKind::InstanceOfCheck );
    checkInstruction.sourceOperands.push_back(
        checkedVariable );
    checkInstruction.operandType =
        instanceNode.checkedType;
    checkInstruction.sourceLocation =
        instanceNode.sourceLocation;
    semantic::TypeSharedPointer boolType =
        std::make_shared<semantic::Type>(
            semantic::Type::Kind::Bool,
            semantic::qualname::classes::boolean::Name );
    MIRVariableIdentifier resultVariable =
        this->currentFunction->allocateVariable(
            "_instanceof", boolType, false );
    checkInstruction.destinationVariable = resultVariable;
    return this->emitInstruction( checkInstruction );
}
```

MIR lowering:
1. Lower the checked expression to get a variable identifier.
2. Create `InstanceOfCheck` MIR instruction with the checked variable as source operand.
3. Store the target type in `operandType`.
4. Allocate a `_instanceof` result variable with `Boolean` type.
5. Emit instruction; result is the destination variable.

### MIR Codegen — Compile-Time Constant Folding

At `src/uranite/ir/mir/codegen.cpp:1331-1387`, `InstanceOfCheck` is resolved entirely at compile time through constant folding — no runtime type information (RTTI) is emitted:

```cpp
case MIRInstructionKind::InstanceOfCheck: {
    if( instruction.destinationVariable != 0 ) {
        bool isMatch = false;
        if( instruction.operandType != nullptr &&
            instruction.sourceOperands.empty() == false ) {
            std::string targetTypeName =
                instruction.operandType->name;
            size_t bracketPos =
                targetTypeName.find( '<' );
            if( bracketPos != std::string::npos ) {
                targetTypeName = targetTypeName.substr(
                    0, bracketPos );
            }
            MIRVariableIdentifier sourceVar =
                instruction.sourceOperands[0];
            // ... lookup source type from descriptor table ...
        }
        this->setVariableValue(
            instruction.destinationVariable,
            isMatch
                ? llvm::ConstantInt::getTrue(
                    this->llvmContext )
                : llvm::ConstantInt::getFalse(
                    this->llvmContext ) );
    }
    break;
}
```

Compile-time resolution algorithm:

1. **Strip generic parameters**: Remove `<...>` suffix from both source and target type names. `ArrayList<String>` becomes `ArrayList`, `HashMap<K,V>` becomes `HashMap`. This means `instanceof` checks base type identity, not generic parameter compatibility.

2. **Direct name match**: Compare stripped source type name against stripped target type name. If equal, `isMatch = true`.

3. **Inheritance chain traversal** (for `Kind::Class` source types): If direct match fails and source is a class, walk the `baseClass` chain:
   ```cpp
   semantic::TypeSharedPointer current =
       classPtr->baseClass;
   while( current != nullptr &&
          isMatch == false ) {
       std::string baseName = current->name;
       // strip generics from baseName
       if( baseName == targetTypeName ) {
           isMatch = true;
       }
       if( current->kind ==
           semantic::Type::Kind::Class ) {
           current = static_cast<
               semantic::ClassType*>(
               current.get() )->baseClass;
       }
       else {
           break;
       }
   }
   ```

4. **Object fallback**: If source type name equals "Object" or matches target, `isMatch = true`.

5. **Missing descriptor fallback**: If the source variable has no type descriptor in the variable descriptor table, defaults to `isMatch = true` (optimistic).

6. **Emit constant**: Result is `llvm::ConstantInt::getTrue` or `llvm::ConstantInt::getFalse` — a compile-time constant, never a runtime check.

**Key limitation**: Since `instanceof` resolves at compile time, it only works with statically known types. Dynamic dispatch scenarios where the runtime type differs from the static type (e.g., a `Dog` stored as `Animal`) will check against the static type `Animal`, not the runtime type `Dog`.

---

## subclassof Operator

### Syntax {#subclassof-syntax}

```
Boolean result = ChildClass subclassof ParentClass
```

The `subclassof` operator tests whether one type is a subclass of another. Both operands are type names (not value expressions). Returns `Boolean`.

### AST Representation — SubclassofExpression

Defined in `src/uranite/ast/node.hpp:1776-1799`:

```cpp
struct SubclassofExpression : Expression {

    TypeNodeSharedPointer sourceType;
    TypeNodeSharedPointer targetType;

    SubclassofExpression(
        TypeNodeSharedPointer sourceType,
        TypeNodeSharedPointer target,
        const lookup::SourceSharedPointer& source
    ) : Expression( Node::Kind::SubclassofExpression,
            source ),
        sourceType( std::move( sourceType ) ),
        targetType( std::move( target ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `sourceType` | `TypeNodeSharedPointer` | Child type (left operand) |
| `targetType` | `TypeNodeSharedPointer` | Parent type to check against (right operand) |

Unlike `instanceof`, both operands are type nodes — `subclassof` operates on types, not values.

### Parsing {#subclassof-parsing}

At `src/uranite/parser/parser.cpp:2168-2180`:

```cpp
if( kind == token::Type::KeywordSubclassOf ) {
    lookup::SourceSharedPointer source =
        this->current().source;
    this->advance();
    ast::nodes::TypeNodeSharedPointer type =
        this->parseTypeNode();
    left = std::make_shared<
        ast::nodes::SubclassofExpression>(
        std::make_shared<ast::nodes::SimpleTypeNode>(
            static_cast<
                ast::nodes::IdentifierExpression&>(
                *left ).name,
            left->source
        ),
        type,
        source
    );
    continue;
}
```

Parsing details:
1. Left operand must be an `IdentifierExpression` (a type name) — cast directly to extract the name.
2. Left operand is wrapped in a `SimpleTypeNode` to convert from expression to type node.
3. Right operand is parsed via `parseTypeNode()`.
4. Both operands become `TypeNodeSharedPointer` in the `SubclassofExpression`.

### Semantic Analysis {#subclassof-semantic-analysis}

At `src/uranite/semantic/analyzer.cpp:2419-2424`:

```cpp
case ast::Node::Kind::SubclassofExpression: {
    ast::nodes::SubclassofExpression&
        subclassOfExpression =
        static_cast<ast::nodes::SubclassofExpression&>(
            *expression );
    this->resolveType( subclassOfExpression.sourceType );
    this->resolveType( subclassOfExpression.targetType );
    expressionType = this->typeRegistry.getBool();
    break;
}
```

Both type operands are resolved. Result type is unconditionally `Boolean`. No compile-time evaluation of the inheritance relationship occurs during semantic analysis.

### HIR Stage — HIRSubclassOf

Defined in `src/uranite/ir/hir.hpp:965-979`:

```cpp
struct HIRSubclassOf : HIRNode {

    semantic::TypeSharedPointer childType;
    semantic::TypeSharedPointer parentType;

    HIRSubclassOf(
        semantic::TypeSharedPointer childType,
        semantic::TypeSharedPointer parentType,
        const lookup::SourceSharedPointer& sourceLocation
    ) : HIRNode( HIRNodeKind::SubclassOf,
            nullptr, sourceLocation ),
        childType( std::move( childType ) ),
        parentType( std::move( parentType ) ) {
    }

};
```

HIR lowering at `src/uranite/ir/hir/lowering.cpp:1107-1111`:

```cpp
case ast::Node::Kind::SubclassofExpression: {
    ast::nodes::SubclassofExpression&
        subclassofExpression =
        static_cast<ast::nodes::SubclassofExpression&>(
            *expression );
    semantic::TypeSharedPointer childType =
        this->resolveTypeNode(
            subclassofExpression.sourceType );
    semantic::TypeSharedPointer parentType =
        this->resolveTypeNode(
            subclassofExpression.targetType );
    return std::make_shared<HIRSubclassOf>(
        childType, parentType, expression->source );
}
```

Both types are resolved to semantic `TypeSharedPointer` values. The `HIRSubclassOf` node carries the fully resolved child and parent types.

### MIR and Codegen Status {#subclassof-mir-and-codegen-status}

`HIRSubclassOf` has **no corresponding MIR lowering or codegen implementation**. The `SubclassOf` HIR node kind is not handled in the MIR lowering switch at `src/uranite/ir/mir/lowering.cpp`. Using `subclassof` in code that reaches MIR codegen will fall through to the default case and produce an invalid result.

---

## Type Kind Enumeration

The `Type::Kind` enum at `src/uranite/semantic/typeref.hpp:50-69` includes the `Meta` kind alongside all other type kinds:

| Kind | Value | Description |
|---|---|---|
| `Meta` | 58 | `Meta<T>` — type treated as a value |

Other related kinds for context:

| Kind | Description |
|---|---|
| `Class` | Class types — support `instanceof` with inheritance traversal |
| `Struct` | Struct types — support `instanceof` with direct name match only |
| `Interface` | Interface types |
| `Bool` | Result type of `instanceof` and `subclassof` |

---

## Examples

### Meta Type Variables

```
package examples

Meta<String> stringType = String
Meta<I64> integerType = I64
Meta<Object> objectType = Object

Meta<Object> anyType = String
```

**Compilation trace**:
1. **Parser**: `String` in expression position is parsed as an `IdentifierExpression`. During semantic analysis, the identifier is resolved as a type name and wrapped in `TypeReferenceExpression`.
2. **Semantic analysis**: `TypeReferenceExpression("String")` triggers `lookupType("String")` which resolves the `String` type, then `makeMeta` wraps it into `Meta<String>`. The declared variable type `Meta<String>` matches.
3. **Auto-wrapping**: For `Meta<Object> anyType = String`, if the initializer resolves as a non-Meta type, the analyzer auto-wraps it via `makeMeta` at `analyzer.cpp:4040-4041` before the assignability check.
4. **Assignability**: `Meta<String>` is assignable to `Meta<Object>` because `String` is assignable to `Object` (covariant).

### instanceof Checks

```
package examples

from uranite.io.console import puts

public class Animal:
    public String name

    public function Animal(self, String name) -> Void:
        self.name = name

public class Dog extends Animal:
    public function Dog(self, String name) -> Void:
        super(name)

public function main() -> I32:
    Dog rex = new Dog("Rex")

    if rex instanceof Dog:
        puts("Rex is a Dog")

    if rex instanceof Animal:
        puts("Rex is an Animal")

    return 0
```

**Compilation trace**:
1. **Parser**: `rex instanceof Dog` parsed as `InstanceofExpression(IdentifierExpression("rex"), SimpleTypeNode("Dog"))`.
2. **Semantic analysis**: Analyze `rex` (resolves to `Dog` type), resolve `Dog` type node. Result type: `Boolean`.
3. **HIR**: `HIRInstanceOf(checkedExpression=HIRVariableAccess("rex"), checkedType=Dog)`.
4. **MIR**: `InstanceOfCheck` instruction with `rex` as source operand and `Dog` as operand type.
5. **MIR codegen**: Source type `Dog` matches target type `Dog` by name — emit `ConstantInt::getTrue`. For `rex instanceof Animal`, source type `Dog` does not match directly, but inheritance chain traversal finds `Animal` as base class — emit `ConstantInt::getTrue`. Both checks fold to compile-time constants.

### subclassof Checks

```
package examples

public class Vehicle:
    pass

public class Car extends Vehicle:
    pass

public function main() -> I32:
    Boolean check = Car subclassof Vehicle
    return 0
```

**Compilation trace**:
1. **Parser**: `Car subclassof Vehicle` parsed as `SubclassofExpression(SimpleTypeNode("Car"), SimpleTypeNode("Vehicle"))`.
2. **Semantic analysis**: Both types resolved. Result type: `Boolean`.
3. **HIR**: `HIRSubclassOf(childType=Car, parentType=Vehicle)`.
4. **MIR**: Not yet implemented — `SubclassOf` HIR node has no MIR handler. Falls through to default case.

### Comparison Between instanceof and subclassof

| Aspect | `instanceof` | `subclassof` |
|---|---|---|
| Left operand | Value expression | Type name |
| Right operand | Type name | Type name |
| Checks | Value's type against target type | Type inheritance relationship |
| MIR instruction | `InstanceOfCheck` | Not implemented |
| Codegen | Compile-time constant folding | Not implemented |
| Inheritance traversal | Yes (walks `baseClass` chain) | N/A (no codegen) |
| Generic handling | Strips `<...>` before comparison | N/A |
