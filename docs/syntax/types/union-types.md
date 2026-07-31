# Union Types

Uranite supports union types — types that can hold a value of one of several distinct member types. A union type is written as `TypeA | TypeB | TypeC` using the pipe (`|`) operator between type annotations. The compiler represents unions internally as `UnionType` with a `Kind::Union` discriminator and a vector of member types. At the LLVM level, unions lower to tagged union structs: a two-field struct containing an `i32` tag (identifying which member type the value currently holds) and a payload field sized to the largest member type. The tag value corresponds to the zero-based index of the member type in the union declaration order. Union types interact with the assignability system bidirectionally: a value of any single member type is assignable to the union (the source matches at least one member), and a union is assignable to a target only if every member type in the union is assignable to the target.

This document covers the `UnionType` struct with its member type vector and `containsType` membership test, union type syntax using the pipe operator, the `UnionTypeNode` AST representation, parsing implementation (pipe-delimited type list in `parseTypeNode`), semantic analysis (type resolution via `makeUnion` factory, bidirectional assignability rules for union targets and union sources), the legacy AST codegen tagged union layout (`{i32, widest_member}` struct with tag-based boxing), function parameter union boxing (tag computation, alloca, GEP store), the pipe token dual role (type union operator in type annotations, bitwise OR operator in expressions), interaction with optional types, and practical examples.

---

## Table of Contents

- [Type Identity](#type-identity)
- [The UnionType Struct](#the-uniontype-struct)
  - [Fields](#fields)
  - [Name Generation](#name-generation)
  - [containsType Method](#containstype-method)
- [Union Syntax](#union-syntax)
  - [Basic Union Types](#basic-union-types)
  - [Multi-Member Unions](#multi-member-unions)
  - [Union with Optional](#union-with-optional)
- [AST Representation](#ast-representation)
  - [UnionTypeNode](#uniontypenode)
- [Parsing Implementation](#parsing-implementation)
  - [Type Annotation Parsing](#type-annotation-parsing)
  - [Pipe Token Dual Role](#pipe-token-dual-role)
  - [Raises Clause Unions](#raises-clause-unions)
  - [Except Clause Unions](#except-clause-unions)
- [Semantic Analysis](#semantic-analysis)
  - [Type Resolution](#type-resolution)
  - [Factory Method](#factory-method)
  - [Assignability Rules](#assignability-rules)
- [Compilation Pipeline](#compilation-pipeline)
  - [HIR and MIR Stages](#hir-and-mir-stages)
  - [Codegen — toLLVMType](#codegen--tollvmtype)
  - [Codegen — Tagged Union Layout](#codegen--tagged-union-layout)
  - [Codegen — Function Parameter Boxing](#codegen--function-parameter-boxing)
- [Interaction with Other Types](#interaction-with-other-types)
  - [Union and Optional](#union-and-optional)
  - [Union and Generics](#union-and-generics)
- [Examples](#examples)
  - [Union-Typed Variables](#union-typed-variables)
  - [Union Function Parameters](#union-function-parameters)
  - [Union with Exception Types](#union-with-exception-types)

---

## Type Identity

| Property | Value |
|---|---|
| Type Kind | `Type::Kind::Union` |
| LLVM Type | `{i32, widest_member_type}` (tagged union struct) |
| String Format | `Union<TypeA,TypeB,TypeC>` |
| Syntax | `TypeA \| TypeB \| TypeC` |
| Tag Field | `i32` at struct index 0, value = zero-based member index |
| Payload Field | Widest member LLVM type at struct index 1 |
| Assignability (target) | Source assignable to any member → assignable to union |
| Assignability (source) | All members assignable to target → union assignable to target |

---

## The UnionType Struct

Defined in `src/uranite/semantic/typeref.hpp:948-987`:

```cpp
struct UnionType : Type {

    std::vector<TypeSharedPointer> types;

    UnionType( std::vector<TypeSharedPointer> types )
        : Type( Type::Kind::Union, "" ),
          types( std::move( types ) ) {
        std::string typeNames;
        for( size_t i=0; i<this->types.size(); i++ ) {
            if( i > 0 ) {
                typeNames+= ",";
            }
            typeNames+= this->types[i]->toString();
        }
        this->name = fmt::format( "Union<{}>", typeNames );
    }

    bool containsType( const TypeSharedPointer& type ) const {
        for( const TypeSharedPointer& memberType : this->types ) {
            if( memberType->qualified.empty() == false &&
                type->qualified.empty() == false ) {
                if( memberType->qualified == type->qualified ) {
                    return true;
                }
            }
            else if( memberType->name == type->name &&
                     memberType->kind == type->kind ) {
                return true;
            }
        }
        return false;
    }

};

using UnionTypeSharedPointer = std::shared_ptr<UnionType>;
```

### Fields

| Field | Type | Description |
|---|---|---|
| `types` | `std::vector<TypeSharedPointer>` | Ordered list of member types comprising the union |
| `name` | `std::string` (inherited) | Generated display name in `Union<A,B,C>` format |
| `kind` | `Type::Kind` (inherited) | Always `Type::Kind::Union` |

### Name Generation

The constructor builds the display name by iterating all member types, joining their `toString()` representations with commas, and wrapping in `Union<...>`. For a union `I64 | String | Bool`, the generated name is `Union<I64,String,Bool>`. This name is used for diagnostics and error messages — it does not appear in source code syntax.

### containsType Method

The `containsType` method checks whether a given type is a member of the union. It uses a two-tier comparison strategy:

1. **Qualified name comparison** (primary): If both the member type and the query type have non-empty `qualified` fields, compare qualified names. This handles monomorphized generic types correctly — `ArrayList<I64>` and `ArrayList<String>` have distinct qualified names.
2. **Name + kind comparison** (fallback): If either type lacks a qualified name, fall back to comparing `name` and `kind` together. This handles primitive types and error recovery types that may not have qualified names set.

---

## Union Syntax

### Basic Union Types

Union types are written with the pipe (`|`) operator between type names:

```
I64 | String mixedValue
```

This declares `mixedValue` as a variable that can hold either an `I64` or a `String`.

### Multi-Member Unions

Unions can contain any number of member types:

```
I64 | Float | String | Bool flexibleValue
Bool | I64 | None nullableResult
```

Member types are ordered — the tag value assigned to each member corresponds to its position in the declaration (zero-indexed from left to right).

### Union with Optional

Union types can be combined with the optional suffix operator `?` to create an optional union:

```
I64 | String? optionalMixed
```

The parser processes the pipe-delimited union first, then wraps the result in `OptionalTypeNode` if a trailing `?` is present. This means `I64 | String?` is equivalent to `Optional<Union<I64,String>>` — the entire union becomes optional, not just the last member.

---

## AST Representation

### UnionTypeNode

Defined in `src/uranite/ast/node.hpp:703-720`:

```cpp
struct UnionTypeNode : TypeNode {

    std::vector<TypeNodeSharedPointer> types;

    UnionTypeNode(
        std::vector<TypeNodeSharedPointer> types,
        const lookup::SourceSharedPointer& source
    ) : TypeNode( Node::Kind::UnionType, source ),
        types( std::move( types ) ) {
    }

};
```

`UnionTypeNode` stores a vector of `TypeNodeSharedPointer` entries — one for each member type in the union. The source location is taken from the first member type. Each member type node can be any valid type node: `SimpleTypeNode`, `GenericTypeNode`, `PointerTypeNode`, `ReferenceTypeNode`, or even another `UnionTypeNode` (though nested unions flatten during semantic resolution).

---

## Parsing Implementation

### Type Annotation Parsing

Union type parsing is integrated into `parseTypeNode()` in `src/uranite/parser/parser.cpp:2860-2868`:

```cpp
ast::nodes::TypeNodeSharedPointer baseType = this->parseBaseTypeNode();
if( this->check( token::Type::Pipe ) ) {
    lookup::SourceSharedPointer unionSource = baseType->source;
    std::vector<ast::nodes::TypeNodeSharedPointer> memberTypes;
    memberTypes.push_back( baseType );
    while( this->match( token::Type::Pipe ) ) {
        memberTypes.push_back( this->parseBaseTypeNode() );
    }
    baseType = std::make_shared<ast::nodes::UnionTypeNode>(
        std::move( memberTypes ), unionSource );
}
```

The parsing algorithm:

1. Parse the first type via `parseBaseTypeNode()` — this becomes the initial `baseType`.
2. Check if the next token is a `Pipe` (`|`). If not, return `baseType` as-is (no union).
3. If pipe found, start collecting member types: push `baseType` as the first member.
4. Loop: consume each `Pipe` token, parse the next base type, push it into the member vector.
5. Once no more pipes follow, construct a `UnionTypeNode` wrapping all collected members.
6. After union construction, the parser checks for a trailing `?` to optionally wrap the union.

This means union types have lower precedence than base type parsing — `*mut I64 | String` is parsed as `(*mut I64) | String`, not `*(mut I64 | String)`.

### Pipe Token Dual Role

The `|` (Pipe) token serves two distinct roles depending on context:

| Context | Role | Precedence |
|---|---|---|
| Type annotation | Union type separator | Parsed after base type |
| Expression | Bitwise OR operator | Precedence level 3 |

In type annotation position (`parseTypeNode`), the pipe creates a union type. In expression position (`parseBinaryExpression`), the pipe acts as the bitwise OR operator with precedence 3 (below arithmetic, above comparisons). The parser disambiguates by context — type annotations and expressions are parsed by different entry points.

### Raises Clause Unions

The `raises` clause on function declarations uses the same pipe operator to list multiple exception types that a function may throw, in `src/uranite/parser/parser.cpp:1402-1407`:

```cpp
if( this->check( token::Type::KeywordRaises ) ) {
    this->advance();
    declaration->raisesTypes.push_back( this->parseTypeNode() );
    while( this->match( token::Type::Pipe ) ) {
        declaration->raisesTypes.push_back( this->parseTypeNode() );
    }
}
```

This differs from type-level unions: instead of creating a `UnionTypeNode`, the raises clause stores each exception type as a separate entry in the `raisesTypes` vector. The pipe here is a delimiter between distinct exception types, not a union type constructor.

```
public function riskyOperation() -> I64 raises IOException | ParseError:
    ...
```

### Except Clause Unions

Similarly, `except` clauses in try-except blocks use the pipe to list multiple exception types to catch, in `src/uranite/parser/parser.cpp:2811-2820`:

```cpp
exceptionClause.exceptionTypes.push_back(
    std::make_shared<ast::nodes::SimpleTypeNode>(
        this->current().value, this->current().source ) );
this->advance();
while( this->match( token::Type::Pipe ) ) {
    exceptionClause.exceptionTypes.push_back(
        std::make_shared<ast::nodes::SimpleTypeNode>(
            this->current().value, this->current().source ) );
    this->advance();
}
```

```
try:
    riskyOperation()
except IOException | ParseError as error:
    handleError(error)
```

Again, this produces separate entries in the `exceptionTypes` vector rather than a `UnionTypeNode`.

---

## Semantic Analysis

### Type Resolution

Union types are resolved in the `resolveType` method in `src/uranite/semantic/analyzer.cpp:5191-5198`:

```cpp
case ast::Node::Kind::UnionType: {
    ast::nodes::UnionTypeNode& unionNode =
        static_cast<ast::nodes::UnionTypeNode&>( *typeNode );
    std::vector<TypeSharedPointer> resolvedUnionTypes;
    for( ast::nodes::TypeNodeSharedPointer& memberNode : unionNode.types ) {
        TypeSharedPointer resolvedMember = this->resolveType( memberNode );
        resolvedUnionTypes.push_back(
            resolvedMember ? resolvedMember : this->typeRegistry.getError() );
    }
    return this->typeRegistry.makeUnion( std::move( resolvedUnionTypes ) );
}
```

Each member type node is resolved individually via recursive `resolveType` calls. If a member type fails to resolve (returns `nullptr`), an error type is substituted to maintain the union structure for downstream error recovery. The resolved member types are collected and passed to the `makeUnion` factory method.

### Factory Method

Defined in `src/uranite/semantic/typeref.cpp:648-649`:

```cpp
TypeSharedPointer Registry::makeUnion( std::vector<TypeSharedPointer> types ) {
    return std::make_shared<UnionType>( std::move( types ) );
}
```

Creates a fresh `UnionType` instance. Like pointer and reference types, union types are not cached — each call produces a new instance. The `UnionType` constructor generates the `Union<A,B,C>` display name from the member types.

### Assignability Rules

Union types have bidirectional assignability rules, defined in `src/uranite/semantic/typeref.cpp:477-497`:

**When the target (left side) is a union:**

```cpp
if( target->kind == Type::Kind::Union ) {
    UnionTypeSharedPointer unionTargetType =
        std::static_pointer_cast<UnionType>( target );
    for( TypeSharedPointer& memberType : unionTargetType->types ) {
        if( this->isAssignable( memberType, source ) ) {
            return true;
        }
    }
}
```

If the target is a union, the source is assignable if it is assignable to *any one* of the union member types. This allows a value of type `I64` to be assigned to a variable of type `I64 | String` — the `I64` matches the first member.

**When the source (right side) is a union:**

```cpp
if( source->kind == Type::Kind::Union ) {
    UnionTypeSharedPointer unionSourceType =
        std::static_pointer_cast<UnionType>( source );
    bool isAllMemberAssignable = true;
    for( TypeSharedPointer& memberType : unionSourceType->types ) {
        if( this->isAssignable( target, memberType ) == false ) {
            isAllMemberAssignable = false;
            break;
        }
    }
    if( isAllMemberAssignable ) {
        return true;
    }
}
```

If the source is a union, the union is assignable to the target only if *every* member type is assignable to the target. This prevents unsafe narrowing — a `I64 | String` union cannot be assigned to a plain `I64` variable, because the `String` member is not assignable to `I64`.

**Assignability summary:**

| Source | Target | Assignable | Rule |
|---|---|---|---|
| `I64` | `I64 \| String` | Yes | Source matches member `I64` |
| `String` | `I64 \| String` | Yes | Source matches member `String` |
| `Bool` | `I64 \| String` | No | Source matches no member |
| `I64 \| String` | `Object` | Yes | Both `I64` and `String` assignable to `Object` |
| `I64 \| String` | `I64` | No | `String` not assignable to `I64` |
| `I64 \| String` | `I64 \| String \| Bool` | Yes | Both members assignable to superset union |
| `I64 \| String \| Bool` | `I64 \| String` | No | `Bool` not assignable to `I64 \| String` |

---

## Compilation Pipeline

### HIR and MIR Stages

Union types currently have no dedicated HIR or MIR node kinds. The union type information is preserved through the `resolvedType` field on expressions and declarations, which carries the `UnionType` through HIR lowering and MIR lowering. The tagged union boxing and unboxing is handled at the codegen stage.

### Codegen — toLLVMType

The `toLLVMType` method in the legacy AST codegen converts `UnionType` to a tagged union LLVM struct, in `src/uranite/codegen/codegen.cpp:8929-8943`:

```cpp
case semantic::Type::Kind::Union: {
    semantic::UnionTypeSharedPointer unionType =
        std::static_pointer_cast<semantic::UnionType>( type );

    unsigned int unionMaxSize = 0;
    llvm::Type* unionWidestLLVMType =
        llvm::Type::getInt64Ty( this->context );
    for( semantic::TypeSharedPointer& item : unionType->types ) {
        llvm::Type* itemLLVMType = this->toLLVMType( item );
        llvm::TypeSize itemSize =
            this->module->getDataLayout().getTypeAllocSize( itemLLVMType );
        if( itemSize > unionMaxSize ) {
            unionMaxSize = static_cast<unsigned int>( itemSize );
            unionWidestLLVMType = itemLLVMType;
        }
    }
    return llvm::StructType::get( this->context, {
        llvm::Type::getInt32Ty( this->context ),
        unionWidestLLVMType
    });
}
```

The tagged union layout algorithm:

1. Iterate all member types, converting each to its LLVM type via `toLLVMType`.
2. For each member, query the data layout for its allocation size (`getTypeAllocSize`).
3. Track the widest (largest) member type by byte size.
4. Construct a two-field LLVM struct: `{i32 tag, widest_member_type payload}`.

The `i32` tag field identifies which member type the value currently holds (zero-based index). The payload field is sized to the largest member — smaller members are stored in the same space with unused bytes.

For example, `I64 | Bool` produces `{i32, i64}` — the `i64` payload accommodates both `I64` (8 bytes) and `Bool` (1 byte, padded).

### Codegen — Tagged Union Layout

The same tagged union layout is also computed from AST type nodes (for AST-direct codegen path), in `src/uranite/codegen/codegen.cpp:8367-8382`:

```cpp
else if( typeNode->kind == ast::Node::Kind::UnionType ) {
    uint64_t unionMaximumTypeSize = 0;
    ast::nodes::UnionTypeNode& unionTypeNode =
        static_cast<ast::nodes::UnionTypeNode&>( *typeNode );
    llvm::Type* unionWidestLLVMType =
        llvm::Type::getInt64Ty( this->context );
    for( const ast::nodes::TypeNodeSharedPointer&
             unionMemberTypeReference : unionTypeNode.types ) {
        llvm::Type* unionMemberLLVMType =
            this->resolveAstType( unionMemberTypeReference );
        uint64_t unionMemberAllocatedSize =
            this->module->getDataLayout().getTypeAllocSize(
                unionMemberLLVMType );
        if( unionMemberAllocatedSize > unionMaximumTypeSize ) {
            unionMaximumTypeSize = unionMemberAllocatedSize;
            unionWidestLLVMType = unionMemberLLVMType;
        }
    }
    return llvm::StructType::get( this->context, {
        llvm::Type::getInt32Ty( this->context ),
        unionWidestLLVMType
    });
}
```

Identical algorithm operating on AST nodes instead of semantic types. Both paths produce the same `{i32, widest_type}` struct layout.

### Codegen — Function Parameter Boxing

When a function parameter has a union type and the argument value is not already boxed, the codegen automatically boxes the value into the tagged union struct, in `src/uranite/codegen/codegen.cpp:1522-1551`:

```cpp
if( expectedType->isPointerTy() &&
    valueLLVM->getType()->isPointerTy() == false &&
    functionSemantic &&
    parameterIndex < functionSemantic->parameterTypes.size() &&
    functionSemantic->parameterTypes[parameterIndex]->kind ==
        semantic::Type::Kind::Union ) {

    semantic::UnionTypeSharedPointer unionParamType =
        std::static_pointer_cast<semantic::UnionType>(
            functionSemantic->parameterTypes[parameterIndex] );

    uint64_t unionMaxSize = 0;
    llvm::Type* unionWidestLLVMType =
        llvm::Type::getInt64Ty( this->context );
    for( semantic::TypeSharedPointer& unionMemberType :
             unionParamType->types ) {
        llvm::Type* unionMemberLLVMType =
            this->toLLVMType( unionMemberType );
        uint64_t unionMemberSize =
            this->module->getDataLayout().getTypeAllocSize(
                unionMemberLLVMType );
        if( unionMemberSize > unionMaxSize ) {
            unionMaxSize = unionMemberSize;
            unionWidestLLVMType = unionMemberLLVMType;
        }
    }

    llvm::StructType* unionStructType = llvm::StructType::get(
        this->context, {
            llvm::Type::getInt32Ty( this->context ),
            unionWidestLLVMType
        });

    int unionTag = 0;
    for( size_t unionIdx = 0;
         unionIdx < unionParamType->types.size(); unionIdx++ ) {
        llvm::Type* unionMemberLLVMType =
            this->toLLVMType( unionParamType->types[unionIdx] );
        if( unionMemberLLVMType == valueLLVM->getType() ) {
            unionTag = static_cast<int>( unionIdx );
            break;
        }
    }

    llvm::Value* unionAlloca = this->createEntryBlockAllocation(
        this->currentFunction, "union.box", unionStructType );
    llvm::Value* unionTagPtr = this->builder.CreateStructGEP(
        unionStructType, unionAlloca, 0, "union.tag.ptr" );
    this->builder.CreateStore(
        llvm::ConstantInt::get(
            llvm::Type::getInt32Ty( this->context ), unionTag ),
        unionTagPtr );
    llvm::Value* unionValPtr = this->builder.CreateStructGEP(
        unionStructType, unionAlloca, 1, "union.val.ptr" );
    this->builder.CreateStore( valueLLVM, unionValPtr );
    valueLLVM = unionAlloca;
}
```

The boxing process:

1. **Detect**: Check if the expected parameter type is a pointer (union struct pointer) but the argument value is not a pointer (it is a raw scalar like `i64`), and the semantic parameter type is `Kind::Union`.
2. **Compute layout**: Rebuild the tagged union struct type by finding the widest member.
3. **Determine tag**: Iterate union member types, comparing each member LLVM type to the argument value type. The matching index becomes the tag value.
4. **Allocate**: Create a stack allocation (`alloca`) for the union struct named `union.box`.
5. **Store tag**: GEP to struct index 0 (`union.tag.ptr`), store the tag constant.
6. **Store value**: GEP to struct index 1 (`union.val.ptr`), store the argument value.
7. **Replace**: The boxed union struct pointer replaces the original scalar argument value.

---

## Interaction with Other Types

### Union and Optional

Union types and optional types compose through the parser ordering. The parser processes pipes first (building the union), then checks for a trailing `?` (wrapping in optional):

```
I64 | String?
```

Parses as `Optional<Union<I64,String>>`. The optional wraps the entire union, not just the last member.

To make only the last member optional, use parenthetical grouping if supported, or use explicit optional syntax:

```
I64 | Optional<String>
```

This creates `Union<I64,Optional<String>>` — a union where one member happens to be optional.

### Union and Generics

Union types can appear as generic type arguments:

```
ArrayList<I64 | String> mixedList
HashMap<String, I64 | Bool> flexibleMap
```

During monomorphization, the union type is treated as a single type argument. `ArrayList<I64 | String>` monomorphizes with `Union<I64,String>` as the element type. The generic substitution system does not currently have a dedicated case for `Kind::Union` in the `substituteType` lambda or `substituteGenericParameters` — union types inside generic definitions pass through without special handling since the union itself (not its members individually) is the type being substituted.

---

## Examples

### Union-Typed Variables

Basic union variable declarations and assignments:

```
package examples

from uranite.io.console import puts

public function describeValue(I64 | String value) -> String:
    return "mixed value"

public function main() -> I32:
    I64 | String result = 42
    puts(describeValue(result))

    result = "hello"
    puts(describeValue(result))

    return 0
```

**Compilation trace**:
1. **Parser**: `I64 | String` tokens parse as `parseBaseTypeNode()` → `SimpleTypeNode("I64")`, then `Pipe` detected, loop collects `SimpleTypeNode("String")`, constructs `UnionTypeNode([I64, String])`.
2. **Semantic analysis**: `resolveType` for `UnionTypeNode` resolves each member (`I64` → `IntegerType`, `String` → `ClassType`) and calls `makeUnion([I64Type, StringType])` → creates `UnionType` with name `Union<I64,String>`.
3. **Assignability**: `42` (type `I64`) assigned to `I64 | String` — target is union, source `I64` is assignable to member `I64` → valid. `"hello"` (type `String`) assigned to same union — source `String` is assignable to member `String` → valid.
4. **Codegen**: `toLLVMType(Union<I64,String>)` computes `sizeof(I64) = 8`, `sizeof(String) = ptr size`. Widest member determines payload type. Result: `{i32, widest}` struct. Assignment `42` boxes into struct: tag = 0 (I64 is index 0), payload = `42`.

### Union Function Parameters

Functions accepting union parameters with automatic boxing:

```
package examples

from uranite.io.console import puts

public function formatValue(I64 | Float | String value) -> String:
    return "formatted"

public function main() -> I32:
    puts(formatValue(42))
    puts(formatValue(3.14))
    puts(formatValue("text"))
    return 0
```

**Compilation trace**:
1. **Parser**: `I64 | Float | String` collects three member types into `UnionTypeNode`.
2. **Semantic analysis**: Resolves to `UnionType` with members `[I64, Float, String]`.
3. **Codegen — parameter boxing**: When calling `formatValue(42)`:
   - Expected parameter type is the union struct pointer.
   - Argument `42` is `i64` (not a pointer).
   - Tag computation: iterate members — `toLLVMType(I64)` = `i64` matches argument type → tag = 0.
   - Emit: `alloca {i32, widest}`, store tag 0, store value 42, pass struct pointer.
   - For `formatValue(3.14)`: tag = 1 (Float is index 1), stores `double`.
   - For `formatValue("text")`: tag = 2 (String is index 2), stores `ptr`.

### Union with Exception Types

Union types in raises and except clauses:

```
package examples

from uranite.io.console import puts

public function parseAndValidate(String input) -> I64 raises ParseError | ValidationError:
    if input.length() is 0:
        throw new ParseError("empty input")
    I64 value = parseInt(input)
    if value < 0:
        throw new ValidationError("negative value")
    return value

public function main() -> I32:
    try:
        I64 result = parseAndValidate("42")
        puts(result.toString())
    except ParseError | ValidationError as error:
        puts(error.toString())
    return 0
```

**Compilation trace**:
1. **Parser (raises)**: After `raises` keyword, parses `ParseError`, encounters `Pipe`, parses `ValidationError`. Both are stored as separate entries in `raisesTypes` vector — this is not a `UnionTypeNode`.
2. **Parser (except)**: After `except`, parses `ParseError`, encounters `Pipe`, parses `ValidationError`. Both stored as separate entries in `exceptionTypes` vector.
3. **Semantic analysis**: Each exception type in the raises clause and except clause is resolved independently. The pipe here is purely syntactic sugar for listing multiple types — it does not create a `UnionType` in the type system.
