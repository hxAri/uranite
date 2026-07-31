# Pointer and Reference Types

Uranite provides two indirection mechanisms: pointers and references. Pointers (`*T`, `*mut T`) represent raw memory addresses and require `unsafe` blocks for dereference operations, placing the safety burden on the programmer. References (`&T`, `&mut T`) represent borrowed views into existing values and can be dereferenced freely without `unsafe`, because the borrow checker guarantees their validity at compile time. Both types map to LLVM opaque pointers (`PointerType::getUnqual`) at the code generation level — the distinction between pointers and references is purely a semantic-layer concern, enforced during analysis and erased during lowering. Mutability is tracked as a boolean flag on both types: immutable variants (`*T`, `&T`) prevent writes through the indirection, while mutable variants (`*mut T`, `&mut T`) permit modification of the pointed-to or referenced value.

This document covers the `PointerType` and `ReferenceType` structs with their internal fields, pointer and reference type syntax with mutability modifiers, the address-of (`&`) and dereference (`*`) unary operators, parsing implementation for type annotations and operator expressions, semantic analysis (type resolution via `makePointer`/`makeReference` factories, dereference safety enforcement through `isInsideUnsafe` checks, assignability rules with reference unwrapping, comparability rules), generic substitution for pointer and reference type parameters, HIR representation, MIR lowering, codegen mapping to LLVM opaque pointers, borrow checker integration (reference lifetime validation, return-of-local detection), and practical examples demonstrating raw pointer manipulation in unsafe blocks, reference-based function parameters, and mutable reference patterns.

---

## Table of Contents

- [Type Identity](#type-identity)
- [The PointerType Struct](#the-pointertype-struct)
- [The ReferenceType Struct](#the-referencetype-struct)
- [Pointer Syntax](#pointer-syntax)
  - [Immutable Pointers](#immutable-pointers)
  - [Mutable Pointers](#mutable-pointers)
- [Reference Syntax](#reference-syntax)
  - [Immutable References](#immutable-references)
  - [Mutable References](#mutable-references)
- [Self Parameter References](#self-parameter-references)
- [Operators](#operators)
  - [Address-Of Operator](#address-of-operator)
  - [Dereference Operator](#dereference-operator)
  - [Unsafe Requirement for Pointer Dereference](#unsafe-requirement-for-pointer-dereference)
- [AST Representation](#ast-representation)
  - [PointerTypeNode](#pointertypenode)
  - [ReferenceTypeNode](#referencetypenode)
- [Parsing Implementation](#parsing-implementation)
  - [Type Annotation Parsing](#type-annotation-parsing)
  - [Unary Operator Parsing](#unary-operator-parsing)
- [Semantic Analysis](#semantic-analysis)
  - [Type Resolution](#type-resolution)
  - [Factory Methods](#factory-methods)
  - [Unary Operator Analysis](#unary-operator-analysis)
  - [Unsafe Block Scope](#unsafe-block-scope)
  - [Assignability Rules](#assignability-rules)
  - [Comparability Rules](#comparability-rules)
- [Generic Substitution](#generic-substitution)
  - [substituteType Lambda](#substitutetype-lambda)
  - [substituteGenericParameters](#substitutegenericparameters)
- [Compilation Pipeline](#compilation-pipeline)
  - [HIR Stage](#hir-stage)
  - [MIR Stage](#mir-stage)
  - [Codegen — toLLVMType](#codegen--tollvmtype)
- [Borrow Checker Integration](#borrow-checker-integration)
  - [Reference Lifetime Validation](#reference-lifetime-validation)
  - [Return-of-Local Detection](#return-of-local-detection)
- [Examples](#examples)
  - [Raw Pointer Manipulation](#raw-pointer-manipulation)
  - [Reference Parameters](#reference-parameters)
  - [Mutable References](#mutable-references-1)

---

## Type Identity

| Property | Value |
|---|---|
| Pointer Kind | `Type::Kind::Pointer` |
| Reference Kind | `Type::Kind::Reference` |
| LLVM Type | `PointerType::getUnqual(context)` (opaque pointer for both) |
| Pointer String (immutable) | `*T` |
| Pointer String (mutable) | `*mut T` |
| Reference String (immutable) | `&T` |
| Reference String (mutable) | `&mut T` |
| Dereference Safety | Pointer requires `unsafe` block; Reference does not |
| Assignability | Reference target type unwrapped before comparison |

---

## The PointerType Struct

Defined in `src/uranite/semantic/typeref.hpp:443-474`:

```cpp
struct PointerType : Type {
    TypeSharedPointer inner;
    bool isMutable;

    PointerType(
        TypeSharedPointer inner,
        bool isMutable
    ) : Type( Kind::Pointer ),
        inner( std::move( inner ) ),
        isMutable( isMutable ) {
        this->name = this->toString();
        this->qualified = this->toString();
    }

    std::string toString() const {
        if( this->isMutable ) {
            return fmt::format( "*mut {}", this->inner->toString() );
        }
        return fmt::format( "*{}", this->inner->toString() );
    }
};
```

`PointerType` wraps any existing type with raw-pointer indirection. The `inner` field holds the pointed-to type. The `isMutable` flag distinguishes `*T` (read-only pointer) from `*mut T` (read-write pointer). Both `name` and `qualified` are derived from `toString()`, which formats as `*mut T` or `*T` depending on mutability. The `Kind::Pointer` discriminator identifies this type throughout the compiler pipeline.

---

## The ReferenceType Struct

Defined in `src/uranite/semantic/typeref.hpp:479-510`:

```cpp
struct ReferenceType : Type {
    TypeSharedPointer inner;
    bool isMutable;

    ReferenceType(
        TypeSharedPointer inner,
        bool isMutable
    ) : Type( Kind::Reference ),
        inner( std::move( inner ) ),
        isMutable( isMutable ) {
        this->name = this->toString();
        this->qualified = this->toString();
    }

    std::string toString() const {
        if( this->isMutable ) {
            return fmt::format( "&mut {}", this->inner->toString() );
        }
        return fmt::format( "&{}", this->inner->toString() );
    }
};
```

`ReferenceType` mirrors `PointerType` structurally — same `inner` and `isMutable` fields, same `toString()` pattern — but uses `Kind::Reference` and formats with `&` instead of `*`. The semantic difference is enforcement: references carry borrow-checker guarantees that pointers do not.

---

## Pointer Syntax

### Immutable Pointers

Immutable pointers use the `*T` syntax. They allow reading the pointed-to value but not writing through the pointer:

```
*I64 readOnlyPointer
*String textPointer
*ArrayList<I64> listPointer
```

### Mutable Pointers

Mutable pointers use the `*mut T` syntax. They allow both reading and writing through the pointer:

```
*mut I64 writablePointer
*mut Char characterPointer
*mut Memory<U8> bufferPointer
```

The `mut` keyword appears between the `*` sigil and the inner type, forming a three-token sequence: `*`, `mut`, `T`.

---

## Reference Syntax

### Immutable References

Immutable references use the `&T` syntax. They borrow a value for reading without transferring ownership:

```
&I64 borrowedInteger
&String borrowedText
&ArrayList<I64> borrowedList
```

### Mutable References

Mutable references use the `&mut T` syntax. They borrow a value for both reading and writing:

```
&mut I64 mutableBorrow
&mut String mutableTextBorrow
&mut ArrayList<I64> mutableListBorrow
```

Like pointers, the `mut` keyword appears between the `&` sigil and the inner type.

---

## Self Parameter References

Method signatures support reference self parameters using `&self` syntax. When a method parameter is `&self`, the compiler sets `isSelf = true` and `isReference = true` on the `FunctionParameterNode`, indicating the method borrows rather than consumes its receiver.

Parsed in `src/uranite/parser/parser.cpp:1448-1455`:

```cpp
if( this->check( token::Type::Ampersand ) &&
    this->peek().type == token::Type::KeywordSelf ) {
    this->advance();
    this->advance();
    ast::nodes::FunctionParameterSharedPointer parameter =
        std::make_shared<ast::nodes::FunctionParameterNode>(
            semantic::qualname::identifier::Self, nullptr, source );
    parameter->isSelf = true;
    parameter->isReference = true;
    return parameter;
}
```

This enables the distinction between consuming methods (`self`) and borrowing methods (`&self`):

```
public class Buffer:
    Memory<U8> data
    I64 length

    public function size(&self) -> I64:
        return self.length

    public function consume(self) -> Memory<U8>:
        return self.data
```

The `size` method borrows the receiver immutably — calling it does not consume the `Buffer`. The `consume` method takes ownership — calling it moves the `Buffer` value into the method.

---

## Operators

### Address-Of Operator

The unary `&` operator creates a reference to a value. Applied to an expression of type `T`, it produces a value of type `&T`:

```
I64 value = 42
&I64 reference = &value
```

Semantic analysis for the address-of operator in `src/uranite/semantic/analyzer.cpp:3990-3993`:

When the unary operator is `&`, the analyzer calls `makeReference(operandType)` to create a `ReferenceType` wrapping the operand type. The resulting expression type is `&T` where `T` is the operand type. The default mutability is immutable.

### Dereference Operator

The unary `*` operator dereferences a pointer or reference, extracting the inner value. Applied to a pointer of type `*T` or a reference of type `&T`, it produces a value of type `T`:

```
&I64 reference = &someValue
I64 dereferenced = *reference
```

Semantic analysis for the dereference operator in `src/uranite/semantic/analyzer.cpp:3995-4008`:

```cpp
if( operandType->kind == Type::Kind::Pointer ) {
    if( this->isInsideUnsafe() == false ) {
        this->reportError(
            expression,
            "dereferencing raw pointer requires \"unsafe\" block"
        );
    }
    PointerTypeSharedPointer pointerType =
        std::static_pointer_cast<PointerType>( operandType );
    expressionType = pointerType->inner;
}
else if( operandType->kind == Type::Kind::Reference ) {
    ReferenceTypeSharedPointer referenceType =
        std::static_pointer_cast<ReferenceType>( operandType );
    expressionType = referenceType->inner;
}
else {
    this->reportError(
        expression,
        fmt::format( "cannot dereference type {}", operandType->toString() )
    );
}
```

Three branches handle dereference:

1. **Pointer dereference**: Checks `isInsideUnsafe()`. If not inside an `unsafe` block, reports error "dereferencing raw pointer requires \"unsafe\" block". If inside unsafe, unwraps `inner` type.
2. **Reference dereference**: Unconditionally unwraps `inner` type. No unsafe requirement — references are guaranteed valid by the borrow checker.
3. **Other types**: Reports error "cannot dereference type X". Only pointer and reference types support the `*` operator.

### Unsafe Requirement for Pointer Dereference

Pointer dereference is the primary operation gated by `unsafe` blocks. The `isInsideUnsafe()` method traverses the scope stack looking for `Scope::Kind::Unsafe`:

The `analyzeUnsafeBlockStatement` method in `src/uranite/semantic/analyzer.cpp:4025-4031`:

```cpp
void Analyzer::analyzeUnsafeBlockStatement(
    ast::nodes::UnsafeBlockStatement& statement ) {
    this->pushScope( Scope::Kind::Unsafe );
    for( ast::nodes::StatementSharedPointer& blockStatement : statement.body ) {
        this->analyzeStatement( blockStatement );
    }
    this->popScope();
}
```

Entering an `unsafe` block pushes a `Scope::Kind::Unsafe` scope onto the scope stack. All statements within the block are analyzed with this scope active. When `isInsideUnsafe()` is called during pointer dereference analysis, it walks up the scope chain looking for any `Unsafe` scope. If found, the dereference is permitted. If not found, the compiler emits the safety error.

Usage pattern:

```
*mut I64 rawPointer = getPointer()
unsafe:
    I64 value = *rawPointer
    *rawPointer = value + 1
```

Without the `unsafe` block wrapper, both dereference operations would produce compile-time errors.

---

## AST Representation

### PointerTypeNode

Defined in `src/uranite/ast/node.hpp:603-626`:

```cpp
struct PointerTypeNode : TypeNode {
    TypeNodeSharedPointer innerType;
    bool isMutable;

    PointerTypeNode(
        TypeNodeSharedPointer innerType,
        bool isMutable,
        const lookup::SourceSharedPointer& source
    ) : TypeNode( Node::Kind::PointerType, source ),
        innerType( std::move( innerType ) ),
        isMutable( isMutable ) {}
};
```

`PointerTypeNode` represents a pointer type annotation in the AST. The `innerType` field holds the AST node for the pointed-to type (which may itself be a complex type like `ArrayList<I64>`). The `isMutable` flag records whether the `mut` keyword was present between the `*` sigil and the inner type.

### ReferenceTypeNode

Defined in `src/uranite/ast/node.hpp:631-654`:

```cpp
struct ReferenceTypeNode : TypeNode {
    TypeNodeSharedPointer innerType;
    bool isMutable;

    ReferenceTypeNode(
        TypeNodeSharedPointer innerType,
        bool isMutable,
        const lookup::SourceSharedPointer& source
    ) : TypeNode( Node::Kind::ReferenceType, source ),
        innerType( std::move( innerType ) ),
        isMutable( isMutable ) {}
};
```

Structurally identical to `PointerTypeNode` except for `Node::Kind::ReferenceType`. The parser produces `ReferenceTypeNode` for `&T` and `&mut T` annotations, and `PointerTypeNode` for `*T` and `*mut T` annotations.

---

## Parsing Implementation

### Type Annotation Parsing

Both pointer and reference type annotations are parsed in `parseBaseTypeNode` in `src/uranite/parser/parser.cpp:474-484`:

```cpp
if( this->check( token::Type::Ampersand ) ) {
    this->advance();
    bool isMutable = this->match( token::Type::KeywordMutable );
    ast::nodes::TypeNodeSharedPointer innerType = this->parseTypeNode();
    return std::make_shared<ast::nodes::ReferenceTypeNode>(
        innerType, isMutable, source );
}
if( this->check( token::Type::Star ) ) {
    this->advance();
    bool isMutable = this->match( token::Type::KeywordMutable );
    ast::nodes::TypeNodeSharedPointer innerType = this->parseTypeNode();
    return std::make_shared<ast::nodes::PointerTypeNode>(
        innerType, isMutable, source );
}
```

Parsing follows the same two-step pattern for both types:

1. **Detect sigil**: Check for `&` (Ampersand) or `*` (Star) token.
2. **Check mutability**: Attempt to match the `mut` keyword. If present, `isMutable` is `true`.
3. **Parse inner type**: Recursively call `parseTypeNode()` to parse the pointed-to or referenced type. This supports arbitrarily nested types: `*mut &ArrayList<I64>` parses as a mutable pointer to an immutable reference to `ArrayList<I64>`.
4. **Create node**: Construct `ReferenceTypeNode` or `PointerTypeNode` with the parsed inner type and mutability flag.

The recursive `parseTypeNode()` call means pointer and reference types compose freely with all other type constructors. Nested indirections are valid: `&&I64` (reference to reference), `**I64` (pointer to pointer), `*&I64` (pointer to reference), `&*I64` (reference to pointer).

### Unary Operator Parsing

The `&` and `*` tokens in expression position (as opposed to type annotation position) are parsed as unary prefix operators. The parser dispatches to `parseUnaryExpression`, which constructs a `UnaryExpression` node with the operator token and the operand expression. The semantic analyzer then distinguishes between address-of (`&`) and dereference (`*`) based on the operator token type.

---

## Semantic Analysis

### Type Resolution

Pointer and reference types are resolved in the `resolveType` method in `src/uranite/semantic/analyzer.cpp:5152-5160`:

```cpp
case ast::Node::Kind::ReferenceType: {
    ast::nodes::ReferenceTypeNode& referenceTypeNode =
        static_cast<ast::nodes::ReferenceTypeNode&>( *typeNode );
    TypeSharedPointer innerType = this->resolveType( referenceTypeNode.innerType );
    return this->typeRegistry.makeReference( innerType, referenceTypeNode.isMutable );
}
case ast::Node::Kind::PointerType: {
    ast::nodes::PointerTypeNode& pointerTypeNode =
        static_cast<ast::nodes::PointerTypeNode&>( *typeNode );
    TypeSharedPointer innerType = this->resolveType( pointerTypeNode.innerType );
    return this->typeRegistry.makePointer( innerType, pointerTypeNode.isMutable );
}
```

Both cases follow the same pattern: recursively resolve the inner type node, then call the appropriate factory method (`makeReference` or `makePointer`) with the resolved inner type and the mutability flag.

### Factory Methods

Defined in `src/uranite/semantic/typeref.cpp:636-641`:

```cpp
TypeSharedPointer Registry::makePointer(
    TypeSharedPointer inner, bool isMutable ) {
    return std::make_shared<PointerType>( inner, isMutable );
}

TypeSharedPointer Registry::makeReference(
    TypeSharedPointer inner, bool isMutable ) {
    return std::make_shared<ReferenceType>( inner, isMutable );
}
```

Factory methods create fresh `PointerType` and `ReferenceType` instances. Unlike monomorphized generic types, pointer and reference types are not cached — each factory call produces a new instance. The `toString()` method on each struct generates the `name` and `qualified` fields at construction time.

### Unary Operator Analysis

The semantic analyzer handles the `&` and `*` unary operators in `analyzeExpression` for `UnaryExpression` nodes, in `src/uranite/semantic/analyzer.cpp:3990-4008`:

| Operator | Operand Kind | Result | Safety |
|---|---|---|---|
| `&` | Any type `T` | `&T` (via `makeReference`) | Always safe |
| `*` | `*T` (Pointer) | `T` (inner type) | Requires `unsafe` block |
| `*` | `&T` (Reference) | `T` (inner type) | Always safe |
| `*` | Other | Compile error | N/A |

The address-of operator (`&`) unconditionally wraps the operand type in `ReferenceType`. The dereference operator (`*`) checks the operand kind: for pointers, it gates on `isInsideUnsafe()`; for references, it proceeds directly; for anything else, it reports a type error.

### Unsafe Block Scope

The `unsafe` block statement pushes `Scope::Kind::Unsafe` onto the analyzer scope stack. The `isInsideUnsafe()` method walks up through parent scopes checking for this kind. This means `unsafe` blocks nest naturally — an `unsafe` block inside a function body, inside a loop, inside a conditional, all work correctly because the scope walk traverses through all enclosing scopes.

### Assignability Rules

Reference types participate in assignability checking with a special unwrapping rule, defined in `src/uranite/semantic/typeref.cpp:399-401`:

```cpp
if( target->kind == Type::Kind::Reference ) {
    ReferenceTypeSharedPointer referenceTargetType =
        std::static_pointer_cast<ReferenceType>( target );
    return this->isAssignable( referenceTargetType->inner, source );
}
```

When the target type (left side of assignment) is a reference, the checker unwraps the reference and recurses with the inner type. This means a value of type `T` is assignable to a variable of type `&T` — the compiler automatically creates the reference. This unwrapping is unidirectional: it applies when the *target* is a reference, not when the *source* is a reference.

Examples of assignability:

| Source Type | Target Type | Assignable | Reason |
|---|---|---|---|
| `I64` | `&I64` | Yes | Reference target unwrapped to `I64`, then `I64` matches `I64` |
| `String` | `&String` | Yes | Same unwrapping rule |
| `&I64` | `I64` | No | Source is reference but target is not — no unwrapping |
| `&I64` | `&I64` | Yes | Both references, inner types match |
| `ArrayList<I64>` | `&ArrayList<I64>` | Yes | Unwrapping applies to generic types |

### Comparability Rules

Reference types are also unwrapped during comparability checking, defined in `src/uranite/semantic/typeref.cpp:548-551`:

```cpp
if( source->kind == Type::Kind::Reference ) {
    ReferenceTypeSharedPointer referenceSourceType =
        std::static_pointer_cast<ReferenceType>( source );
    return this->isComparable( referenceSourceType->inner, target );
}
if( target->kind == Type::Kind::Reference ) {
    ReferenceTypeSharedPointer referenceTargetType =
        std::static_pointer_cast<ReferenceType>( target );
    return this->isComparable( source, referenceTargetType->inner );
}
```

Unlike assignability (which only unwraps the target), comparability unwraps references on *both* sides. This means `&I64` and `I64` are comparable regardless of which side the reference is on, and `&I64` and `&I64` are comparable by unwrapping both to `I64`.

---

## Generic Substitution

When pointer or reference types appear inside generic definitions, they must be recursively substituted during monomorphization. Both the `substituteType` lambda and the `substituteGenericParameters` member function handle these types.

### substituteType Lambda

Defined in the `resolveType` monomorphization path, `src/uranite/semantic/analyzer.cpp:4863-4869`:

```cpp
case Type::Kind::Reference: {
    ReferenceTypeSharedPointer referenceType =
        std::static_pointer_cast<ReferenceType>( type );
    TypeSharedPointer substitutedInner = substituteType( referenceType->inner );
    return this->typeRegistry.makeReference( substitutedInner, referenceType->isMutable );
}
case Type::Kind::Pointer: {
    PointerTypeSharedPointer pointerType =
        std::static_pointer_cast<PointerType>( type );
    TypeSharedPointer substitutedInner = substituteType( pointerType->inner );
    return this->typeRegistry.makePointer( substitutedInner, pointerType->isMutable );
}
```

For both types, the lambda recurses into the `inner` type to substitute any generic parameters, then constructs a new pointer or reference type with the substituted inner type. The `isMutable` flag is preserved unchanged — mutability is a property of the pointer/reference itself, not of the generic parameter.

For example, given a generic class `Container<T>` with a field of type `&T`, monomorphizing `Container<I64>` substitutes `T` with `I64` inside the reference, producing `&I64`.

### substituteGenericParameters

The `substituteGenericParameters` member function follows the same pattern, in `src/uranite/semantic/analyzer.cpp:5382-5388`:

```cpp
case Type::Kind::Reference: {
    ReferenceTypeSharedPointer referenceType =
        std::static_pointer_cast<ReferenceType>( type );
    TypeSharedPointer substitutedInner =
        this->substituteGenericParameters( referenceType->inner, substitutions );
    return this->typeRegistry.makeReference( substitutedInner, referenceType->isMutable );
}
case Type::Kind::Pointer: {
    PointerTypeSharedPointer pointerType =
        std::static_pointer_cast<PointerType>( type );
    TypeSharedPointer substitutedInner =
        this->substituteGenericParameters( pointerType->inner, substitutions );
    return this->typeRegistry.makePointer( substitutedInner, pointerType->isMutable );
}
```

This method takes an explicit substitution map and performs the same recursive inner-type substitution. Used during re-monomorphization of nested generic types.

---

## Compilation Pipeline

### HIR Stage

During HIR lowering, pointer and reference types flow through without requiring special HIR node kinds. The type information is preserved on HIR expression nodes via their `resolvedType` fields, which carry the `PointerType` or `ReferenceType` from semantic analysis. Unary dereference and address-of operations lower to `HIRUnaryExpression` nodes with their operator preserved.

### MIR Stage

MIR lowering treats pointer and reference values as opaque pointer variables. Dereference operations produce `LoadVariable` MIR instructions (loading the value at the pointer address), and address-of operations produce `ComputeAddress` or equivalent instructions (taking the address of a local variable). The MIR representation does not distinguish between pointer and reference at the instruction level — both are pointer-width values.

### Codegen — toLLVMType

Both `PointerType` and `ReferenceType` map to the same LLVM type in `src/uranite/ir/mir/codegen.cpp:6576-6578`:

```cpp
case Type::Kind::Pointer:
case Type::Kind::Reference:
    return llvm::PointerType::getUnqual( this->context );
```

The two cases fall through to the same LLVM opaque pointer type. At the LLVM IR level, there is no distinction between a pointer and a reference — both are represented as `ptr` (opaque pointer). All safety guarantees have been enforced during semantic analysis; the codegen layer simply emits pointer operations.

This means the following Uranite types all produce identical LLVM IR types:

| Uranite Type | LLVM Type |
|---|---|
| `*I64` | `ptr` |
| `*mut I64` | `ptr` |
| `&I64` | `ptr` |
| `&mut I64` | `ptr` |
| `*ArrayList<String>` | `ptr` |
| `&mut HashMap<I64, String>` | `ptr` |

Mutability and safety distinctions exist only in the Uranite type system and are fully erased at LLVM IR emission.

---

## Borrow Checker Integration

### Reference Lifetime Validation

The borrow checker validates that references do not outlive their referents. When a function returns a reference, the checker verifies that the referenced value lives long enough — it must not be a local variable that will be deallocated when the function returns.

### Return-of-Local Detection

The borrow checker detects attempts to return references to local variables, in `src/uranite/semantic/borrow.cpp:344-345`:

```
error: cannot return reference to local variable
note: the variable will be dropped when the function returns
```

This error fires when a function attempts to return `&localVariable` — the reference would dangle after the function stack frame is destroyed. The borrow checker walks return statements, identifies reference-typed return values, and traces the reference back to its origin. If the origin is a local variable (stack-allocated within the current function), the error is emitted.

Valid reference returns include references to:
- Heap-allocated objects (their lifetime extends beyond the function)
- Parameters passed by reference (their lifetime is the caller scope)
- Global or module-level values (their lifetime is the program)

Invalid reference returns include references to:
- Local variables declared within the function body
- Temporary values created during expression evaluation

---

## Examples

### Raw Pointer Manipulation

Raw pointers require `unsafe` blocks for dereference. This example demonstrates pointer arithmetic with `Memory<T>`:

```
package examples

from uranite.io.console import puts
from uranite.memory import Memory

public function swapValues(*mut I64 first, *mut I64 second) -> Void:
    unsafe:
        I64 temporary = *first
        *first = *second
        *second = temporary

public function main() -> I32:
    Memory<I64> buffer = new Memory<I64>(2)
    buffer.set(0, 100)
    buffer.set(1, 200)

    *mut I64 firstPointer = buffer.addressOf(0)
    *mut I64 secondPointer = buffer.addressOf(1)

    swapValues(firstPointer, secondPointer)

    unsafe:
        I64 firstValue = *firstPointer
        I64 secondValue = *secondPointer
        puts(firstValue.toString())
        puts(secondValue.toString())

    return 0
```

**Compilation trace**:
1. **Parser**: `*mut I64` tokens parse as `PointerTypeNode(innerType=I64, isMutable=true)`. The `*firstPointer` expressions parse as `UnaryExpression(op=Star, operand=firstPointer)`.
2. **Semantic analysis**: `swapValues` parameters resolve to `PointerType(inner=I64, isMutable=true)`. Each `*firstPointer` dereference checks `isInsideUnsafe()` — passes because both are inside `unsafe:` blocks.
3. **Codegen**: All pointer types map to `ptr`. Dereference operations emit LLVM `load` instructions; address-of operations emit `getelementptr` or direct address computation.

### Reference Parameters

References enable borrowing without ownership transfer:

```
package examples

from uranite.io.console import puts

public function printLength(&String text) -> Void:
    puts(text.length().toString())

public function appendExclamation(&mut String text) -> Void:
    text = text + "!"

public function main() -> I32:
    String greeting = "Hello, World"

    printLength(&greeting)

    appendExclamation(&greeting)
    puts(greeting)

    return 0
```

**Compilation trace**:
1. **Parser**: `&String` parses as `ReferenceTypeNode(innerType=String, isMutable=false)`. `&mut String` parses as `ReferenceTypeNode(innerType=String, isMutable=true)`. The `&greeting` expression parses as `UnaryExpression(op=Ampersand, operand=greeting)`.
2. **Semantic analysis**: `printLength` parameter resolves to `ReferenceType(inner=String, isMutable=false)`. `appendExclamation` parameter resolves to `ReferenceType(inner=String, isMutable=true)`. The `&greeting` expression creates `ReferenceType(inner=String)` via `makeReference`. No `unsafe` check needed — these are references, not pointers.
3. **Assignability**: `&greeting` (type `&String`) is assignable to parameter type `&String` — direct match. For `printLength`, the immutable reference prevents modification of `text` inside the function body. For `appendExclamation`, the mutable reference permits the reassignment `text = text + "!"`.
4. **Codegen**: Both `&String` and `&mut String` map to `ptr`. The mutability distinction is erased at LLVM level.

### Mutable References

Mutable references combined with generic types:

```
package examples

from uranite.io.console import puts

public class Container<T>:
    T value

    public function Container(self, T initialValue) -> Void:
        self.value = initialValue

    public function get(&self) -> &T:
        return &self.value

    public function set(&mut self, T newValue) -> Void:
        self.value = newValue

public function incrementInPlace(&mut Container<I64> container) -> Void:
    I64 current = *container.get()
    container.set(current + 1)

public function main() -> I32:
    Container<I64> counter = new Container<I64>(0)

    incrementInPlace(&counter)
    incrementInPlace(&counter)
    incrementInPlace(&counter)

    I64 finalValue = *counter.get()
    puts(finalValue.toString())

    return 0
```

**Compilation trace**:
1. **Parser**: `&self` in `get` parses as a self parameter with `isReference=true`. `&mut self` in `set` parses similarly with `isMutable=true` on the reference. `&mut Container<I64>` parses as `ReferenceTypeNode(innerType=Container<I64>, isMutable=true)`. `&T` as return type parses as `ReferenceTypeNode(innerType=T, isMutable=false)`.
2. **Semantic analysis**: During monomorphization of `Container<I64>`, the generic substitution recurses into `&T` return type: the `substituteType` lambda matches `Kind::Reference`, substitutes `T` with `I64` in the inner type, and produces `&I64` via `makeReference(I64Type, false)`. The `isMutable` flag (`false`) is preserved.
3. **Borrow checker**: The `get` method returns `&self.value` — a reference to a field of the receiver. Since `self` is passed by reference (its lifetime extends to the caller scope), this is valid. If `get` tried to return a reference to a local variable, the borrow checker would emit "cannot return reference to local variable".
4. **Dereference**: `*container.get()` dereferences the `&I64` return value. Since the operand is `Kind::Reference` (not `Kind::Pointer`), no `unsafe` block is required.
5. **Codegen**: All reference types map to `ptr`. The `get` method returns a pointer to the `value` field. The dereference loads the `I64` value from that pointer.
