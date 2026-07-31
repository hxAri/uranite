# Index Expressions

Uranite supports subscript access via the bracket operator `object[index]`. An index expression `object[index]` produces an `IndexExpression` AST node carrying two sub-expressions: the object being indexed and the index value. The semantic analyzer resolves index expressions through three distinct paths depending on the object type: arrays use direct element access with an integer check, classes and structs traverse the interface hierarchy searching for the `Indexable` interface and dispatch to its `get` method, and interfaces perform the same `Indexable` lookup with generic type substitution support. At MIR level, class and interface index access desugars to a `CallFunction` (or exception-safe `InvokeFunction`) targeting `"OwnerClass.get(self, index)"`, while array and pointer index access emits a `ComputeIndexAddress` instruction. At LLVM codegen, `ComputeIndexAddress` maps to a `CreateGEP` (GetElementPtr) instruction that computes the address of the indexed element in memory.

---

## Table of Contents

- [Syntax](#syntax)
- [AST Representation — IndexExpression](#ast-representation--indexexpression)
- [Parsing — Postfix Operator](#parsing--postfix-operator)
- [Semantic Analysis — Three-Path Resolution](#semantic-analysis--three-path-resolution)
  - [Path 1: Array Types](#path-1-array-types)
  - [Path 2: Class and Struct Types](#path-2-class-and-struct-types)
  - [Path 3: Interface Types](#path-3-interface-types)
  - [Error: Not Indexable](#error-not-indexable)
- [The Indexable Interface](#the-indexable-interface)
- [HIR Representation — HIRIndexAccess](#hir-representation--hirindexaccess)
- [MIR Lowering — lowerIndexAccess](#mir-lowering--lowerindexaccess)
  - [Class and Interface Path — Method Dispatch](#class-and-interface-path--method-dispatch)
  - [Array and Pointer Path — ComputeIndexAddress](#array-and-pointer-path--computeindexaddress)
- [LLVM Codegen — generateComputeIndexAddress](#llvm-codegen--generatecomputeindexaddress)
- [Examples](#examples)

---

## Syntax

```
object[index]
```

The bracket operator `[]` is a postfix operator that accesses an element of a collection, array, or any type implementing the `Indexable` interface. The object expression appears on the left. The index expression appears between square brackets.

```
Memory<I64> numbers = [10, 20, 30]
I64 second = numbers[1]

ArrayList<String> names = new ArrayList<>()
String first = names[0]
```

Index expressions can chain:

```
Memory<Memory<I64>> matrix = ...
I64 element = matrix[row][column]
```

---

## AST Representation — IndexExpression

Defined at `src/uranite/ast/node.hpp:1451-1474`:

```cpp
struct IndexExpression : Expression {

    ExpressionSharedPointer object;
    ExpressionSharedPointer index;

    IndexExpression(
        ExpressionSharedPointer object,
        ExpressionSharedPointer index,
        const lookup::SourceSharedPointer& source
    ) : Expression( Node::Kind::IndexExpression, source ),
        object( std::move( object ) ),
        index( std::move( index ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `object` | `ExpressionSharedPointer` | Collection or array being indexed |
| `index` | `ExpressionSharedPointer` | Index value expression |

---

## Parsing — Postfix Operator

Index expressions are parsed as postfix operators at `src/uranite/parser/parser.cpp:2095-2100`, inside `parsePostfixExpression`:

```cpp
if( this->check( token::Type::LeftBracket ) ) {
    this->advance();
    ast::nodes::ExpressionSharedPointer index =
        this->parseExpression();
    this->expect( token::Type::RightBracket );
    expression = std::make_shared<ast::nodes::IndexExpression>(
        expression, index, source );
}
```

Key details:
- When the current token is `LeftBracket` (`[`), the parser advances past it, parses the inner expression as the index, expects a `RightBracket` (`]`), and wraps the result in an `IndexExpression`.
- Postfix operators bind tighter than binary operators. `array[0] + 1` parses as `(array[0]) + 1`, not `array[(0 + 1)]`.
- The index expression is a full `parseExpression()` call — any valid expression can appear inside brackets: variables, arithmetic, function calls, other index expressions.
- Multiple postfix operators chain left-to-right. `matrix[row][column]` first creates `IndexExpression(matrix, row)`, then wraps that in `IndexExpression(IndexExpression(matrix, row), column)`.

---

## Semantic Analysis — Three-Path Resolution

At `src/uranite/semantic/analyzer.cpp:3058-3184`, `analyzeIndexExpression` resolves index expressions through three paths depending on the object type.

### Path 1: Array Types

When the object type has `Kind::Array`:

```cpp
if( objectType->kind == semantic::TypeKind::Array ) {
    if( indexType->isIntegral() == false ) {
        this->diagnostic.error( expression.source,
            "array index must be an integer" );
        return this->typeRegistry.getError();
    }
    return objectType->elementType;
}
```

- Index must pass `isIntegral()` — only integer types (`I8`, `I16`, `I32`, `I64`, `U8`, `U16`, `U32`, `U64`) accepted.
- Result type is the array element type. `Memory<String>` indexed by `I64` yields `String`.
- Non-integer index (float, string, boolean) produces error: "array index must be an integer".
- No bounds checking at compile time — array bounds are a runtime concern.

### Path 2: Class and Struct Types

When the object type has `Kind::Class`:

```cpp
if( objectType->kind == semantic::TypeKind::Class ) {
    // Traverse interface hierarchy looking for Indexable
}
```

The analyzer performs an **iterative breadth-first traversal** of the class interface hierarchy:

1. Start with the class declaration node.
2. Collect all interfaces the class implements (direct `implements` list).
3. For each interface, check if it matches the `Indexable` interface — by comparing the qualified name against `qualname::Indexable` (qualified prefix) or checking the name against `qualname::interfaces::indexable::Name` ("Indexable").
4. If `Indexable` is not found directly, traverse each interface's super-interfaces recursively. This is an **iterative BFS** — interfaces are added to a queue, and their super-interfaces are explored level by level.
5. When `Indexable` is found, look up its `get` method via `findMethod(qualname::interfaces::indexable::methods::Get)` (method name "get").
6. Return the `get` method's `returnType` as the result type of the index expression.

This hierarchy traversal means a class does not need to directly implement `Indexable` — implementing an interface that extends `Indexable` is sufficient. For example, if `Sequence` extends `Indexable` and `ArrayList` implements `Sequence`, then `ArrayList` objects are indexable.

### Path 3: Interface Types

When the object type has `Kind::Interface`:

```cpp
if( objectType->kind == semantic::TypeKind::Interface ) {
    // Check if interface is or extends Indexable
}
```

Same `Indexable` search as class types, but starting from the interface itself and its super-interfaces. Additionally supports **generic type substitution**: if the `get` method's return type is a `GenericParameter`, the analyzer looks up the concrete type in the `typeSubstitutions` map. This handles cases like `Indexable<String>` where `get` returns `E` and `E` is substituted with `String`.

### Error: Not Indexable

If none of the three paths match:

```
error: type X is not indexable
```

Types that are not arrays, do not implement `Indexable`, and are not interfaces extending `Indexable` produce this error. Primitive types (`I64`, `F64`, `Boolean`), plain classes without `Indexable`, and function types are not indexable.

---

## The Indexable Interface

The `Indexable` interface is the protocol that makes a type subscriptable with `[]`. Qualified name constants used by the semantic analyzer:

| Constant | Value | Purpose |
|---|---|---|
| `qualname::Indexable` | Qualified prefix | Matching interface qualified names |
| `qualname::interfaces::indexable::Name` | `"Indexable"` | Matching interface short names |
| `qualname::interfaces::indexable::methods::Get` | `"get"` | Looking up the subscript method |

Any class implementing `Indexable` (directly or through an interface chain) becomes subscriptable. The `get` method's return type determines the result type of the index expression.

Standard library types implementing `Indexable`:
- `ArrayList<E>` — `get(index)` returns `E`
- `HashMap<K, V>` — `get(key)` returns `V`
- `String` — `at(index)` / character access
- `Memory<T>` — direct element access (array path, not interface dispatch)

---

## HIR Representation — HIRIndexAccess

Defined at `src/uranite/ir/hir.hpp:727-742`:

```cpp
struct HIRIndexAccess : HIRNode {

    HIRNodeSharedPointer objectExpression;
    HIRNodeSharedPointer indexExpression;

    HIRIndexAccess(
        HIRNodeSharedPointer objectExpression,
        HIRNodeSharedPointer indexExpression,
        semantic::TypeSharedPointer resolvedType,
        const lookup::SourceSharedPointer& sourceLocation
    ) : HIRNode( HIRNodeKind::IndexAccess,
            std::move( resolvedType ), sourceLocation ),
        objectExpression( std::move( objectExpression ) ),
        indexExpression( std::move( indexExpression ) ) {
    }

};
```

| Field | Type | Description |
|---|---|---|
| `objectExpression` | `HIRNodeSharedPointer` | Lowered collection/array expression |
| `indexExpression` | `HIRNodeSharedPointer` | Lowered index value expression |

HIR lowering at `src/uranite/ir/hir/lowering.cpp:998-1002` is straightforward — lower both sub-expressions from AST to HIR nodes, create `HIRIndexAccess`:

```cpp
case ast::Node::Kind::IndexExpression: {
    ast::nodes::IndexExpression& indexExpression =
        static_cast<ast::nodes::IndexExpression&>( *expression );
    hir::HIRNodeSharedPointer objectNode =
        this->lowerExpression( indexExpression.object );
    hir::HIRNodeSharedPointer indexNode =
        this->lowerExpression( indexExpression.index );
    return std::make_shared<hir::HIRIndexAccess>(
        objectNode, indexNode,
        indexExpression.resolvedType,
        indexExpression.source );
}
```

---

## MIR Lowering — lowerIndexAccess

At `src/uranite/ir/mir/lowering.cpp:3852-3905`, `lowerIndexAccess` takes two distinct paths depending on the object type.

### Class and Interface Path — Method Dispatch

When the object type has `Kind::Class` or `Kind::Interface`:

```cpp
if( objectType->kind == semantic::TypeKind::Class ||
    objectType->kind == semantic::TypeKind::Interface ) {

    std::string className = objectType->name;
    // Strip generic brackets: "ArrayList<String>" → "ArrayList"
    size_t bracketPosition = className.find( '<' );
    if( bracketPosition != std::string::npos ) {
        className = className.substr( 0, bracketPosition );
    }

    std::string qualifiedName = /* resolve via typeRegistry */;
    std::string methodName =
        qualifiedName + ".get";

    MIRVariableIdentifier resultVariable =
        this->createVariable( "_index", node.resolvedType );

    // Check if exception handling is active
    if( this->currentLandingPad != "" ) {
        // Exception-safe path: InvokeFunction
        std::string continuationBlock =
            this->createBlock( "index.cont" );
        this->emitInstruction(
            MIRInstructionKind::InvokeFunction,
            resultVariable,
            { objectVariable, indexVariable },
            methodName,
            this->currentLandingPad,
            continuationBlock );
        this->switchToBlock( continuationBlock );
    }
    else {
        // Normal path: CallFunction
        this->emitInstruction(
            MIRInstructionKind::CallFunction,
            resultVariable,
            { objectVariable, indexVariable },
            methodName );
    }

    return resultVariable;
}
```

Key details:

1. **Generic bracket stripping**: The class name is cleaned of generic parameters. `"ArrayList<String>"` becomes `"ArrayList"`, then resolved to its qualified name via the type registry.

2. **Method target**: The call targets `"QualifiedClassName.get"` — the `get` method from the `Indexable` interface, resolved on the concrete class. Operands are `[objectVariable, indexVariable]` — self and index.

3. **Exception-safe dispatch**: When a landing pad is active (inside a `try` block), `InvokeFunction` replaces `CallFunction`. `InvokeFunction` creates a continuation block (`index.cont`) and routes exceptions to the landing pad. If `get` throws (e.g., `IndexOutOfBoundsException`), control flow transfers to the exception handler rather than crashing.

4. **Normal dispatch**: Outside exception contexts, plain `CallFunction` emits a direct call. No exception routing overhead.

### Array and Pointer Path — ComputeIndexAddress

Fallback path for arrays and raw pointers:

```cpp
// Array/Pointer fallback
MIRVariableIdentifier resultVariable =
    this->createVariable( "_index", node.resolvedType );

this->emitInstruction(
    MIRInstructionKind::ComputeIndexAddress,
    resultVariable,
    { objectVariable, indexVariable } );

return resultVariable;
```

`ComputeIndexAddress` computes the memory address of element at `index` within the array pointer. No method dispatch, no function call — direct pointer arithmetic via GEP at codegen time.

### Path Selection Summary

| Object Type | MIR Instruction | Target | Overhead |
|---|---|---|---|
| Class (`Kind::Class`) | `CallFunction` / `InvokeFunction` | `"ClassName.get"` | Virtual dispatch + possible exception routing |
| Interface (`Kind::Interface`) | `CallFunction` / `InvokeFunction` | `"InterfaceName.get"` | Interface dispatch + possible exception routing |
| Array (`Kind::Array`) | `ComputeIndexAddress` | GEP instruction | Zero-cost pointer arithmetic |
| Pointer | `ComputeIndexAddress` | GEP instruction | Zero-cost pointer arithmetic |

---

## LLVM Codegen — generateComputeIndexAddress

At `src/uranite/ir/mir/codegen.cpp:6042-6065`, `generateComputeIndexAddress` translates the MIR instruction to LLVM IR:

```cpp
void MIRCodegen::generateComputeIndexAddress(
    const MIRInstruction& instruction
) {
    if( instruction.destinationVariable ==
            INVALID_VARIABLE_IDENTIFIER ||
        instruction.sourceOperands.size() < 2 ) {
        return;
    }
    llvm::Value* basePointer = this->loadVariableValue(
        instruction.sourceOperands[0] );
    llvm::Value* indexValue = this->loadVariableValue(
        instruction.sourceOperands[1] );
    if( basePointer == nullptr || indexValue == nullptr ) {
        return;
    }
    if( basePointer->getType()->isPointerTy() ) {
        llvm::Type* elementType =
            llvm::Type::getInt8Ty( this->llvmContext );
        if( instruction.operandType != nullptr ) {
            elementType =
                this->toLLVMType( instruction.operandType );
        }
        llvm::Value* elementPointer =
            this->irBuilder.CreateGEP(
                elementType,
                basePointer,
                indexValue,
                "gep.idx"
            );
        this->setVariableValue(
            instruction.destinationVariable,
            elementPointer );
    }
}
```

Execution flow:

1. **Guard checks**: Bail if destination variable is invalid, fewer than 2 source operands, or either operand resolves to null.
2. **Base pointer**: Load the first operand — the array/pointer being indexed.
3. **Index value**: Load the second operand — the integer index.
4. **Pointer type check**: Only proceed if the base is a pointer type (arrays in Uranite are heap-allocated pointers).
5. **Element type resolution**: If `instruction.operandType` is set (carries the semantic type from MIR lowering), convert it to an LLVM type via `toLLVMType`. Otherwise, default to `i8` (byte-level addressing).
6. **GEP emission**: `CreateGEP(elementType, basePointer, indexValue, "gep.idx")` computes `basePointer + indexValue * sizeof(elementType)`. This is pure pointer arithmetic — no memory access, no bounds check.
7. **Result storage**: The computed element pointer is stored as the destination variable. Subsequent `LoadVariable` or `StoreVariable` instructions dereference or write to this address.

### LLVM IR Output

For `Memory<I64> numbers = [10, 20, 30]` and `I64 second = numbers[1]`:

```llvm
%gep.idx = getelementptr i64, ptr %numbers, i64 1
%second = load i64, ptr %gep.idx
```

The `getelementptr` instruction computes the address of element 1 (offset = 1 * 8 bytes = 8 bytes from base). The subsequent `load` reads the value at that address.

### Class/Interface Path — No generateComputeIndexAddress

Class and interface index expressions do not reach `generateComputeIndexAddress`. They use `CallFunction` or `InvokeFunction`, which route through the standard function call codegen — the `get` method is compiled like any other method, and its implementation handles the actual element retrieval internally.

---

## Examples

### Array Index Access

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    Memory<I64> numbers = [10, 20, 30, 40, 50]
    I64 third = numbers[2]
    puts(third.toString())
    return 0
```

Output: `30`.

**Compilation trace**:
1. **Parsing**: `numbers[2]` — `LeftBracket` after identifier triggers postfix index parse. Creates `IndexExpression(object=numbers, index=2)`.
2. **Semantic analysis**: `numbers` is `Memory<I64>` (array kind). Index `2` is `I64`, passes `isIntegral()`. Result type: `I64` (element type).
3. **MIR lowering**: Array path — emits `ComputeIndexAddress _index = numbers, 2`.
4. **LLVM codegen**: `getelementptr i64, ptr %numbers, i64 2` computes address at offset 16 bytes.

### ArrayList Index Access

```
package examples

from uranite.io.console import puts
from uranite.collection.array-list import ArrayList

public function main() -> I32:
    ArrayList<String> names = new ArrayList<>()
    names.add("Alice")
    names.add("Bob")
    names.add("Charlie")

    String second = names[1]
    puts(second)
    return 0
```

Output: `Bob`.

**Compilation trace**:
1. **Parsing**: `names[1]` — same postfix parse as array case. Creates `IndexExpression(object=names, index=1)`.
2. **Semantic analysis**: `names` is `ArrayList<String>` (class kind). Analyzer traverses interface hierarchy: `ArrayList` implements `Sequence`, `Sequence` extends `Indexable`. `Indexable` found — looks up `get` method. Return type of `get` with type substitution `E→String` yields `String`.
3. **MIR lowering**: Class path — emits `CallFunction _index = names, 1` targeting `"_UR_collection_ArrayList.get"`.
4. **LLVM codegen**: Standard function call codegen for `get` method. ArrayList internally does bounds check + GEP on backing array.

### Index in Exception Context

```
package examples

from uranite.io.console import puts
from uranite.collection.array-list import ArrayList

public function main() -> I32:
    ArrayList<I64> values = new ArrayList<>()
    values.add(100)

    try:
        I64 element = values[5]
        puts(element.toString())
    catch Exception error:
        puts("Index out of bounds")

    return 0
```

**Compilation trace**:
1. **Semantic analysis**: Same class path — `ArrayList` has `Indexable` via `Sequence`.
2. **MIR lowering**: Inside `try` block, landing pad is active. Emits `InvokeFunction` instead of `CallFunction`. Creates continuation block `index.cont`. If `get` throws `IndexOutOfBoundsException`, control transfers to landing pad (catch block). If successful, control continues at `index.cont`.

### Chained Index Access

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    Memory<Memory<I64>> matrix = [
        [1, 2, 3],
        [4, 5, 6],
        [7, 8, 9]
    ]

    I64 center = matrix[1][2]
    puts(center.toString())
    return 0
```

Output: `6`.

**Compilation trace**:
1. **Parsing**: `matrix[1][2]` — first postfix creates `IndexExpression(matrix, 1)`. Second postfix wraps that: `IndexExpression(IndexExpression(matrix, 1), 2)`.
2. **MIR lowering**: Two sequential `ComputeIndexAddress` instructions:
   - `ComputeIndexAddress _index1 = matrix, 1` — gets pointer to row 1
   - `ComputeIndexAddress _index2 = _index1, 2` — gets pointer to element 2 within row 1
3. **LLVM codegen**: Two GEP instructions chained:
   - `%row = getelementptr ptr, ptr %matrix, i64 1`
   - `%elem = getelementptr i64, ptr %row, i64 2`

### Interface-Typed Index Access

```
package examples

from uranite.io.console import puts
from uranite.collection.array-list import ArrayList

public function printFirst(Indexable<String> collection) -> Void:
    String first = collection[0]
    puts(first)

public function main() -> I32:
    ArrayList<String> names = new ArrayList<>()
    names.add("Alice")
    printFirst(names)
    return 0
```

Output: `Alice`.

**Compilation trace**:
1. **Semantic analysis**: `collection` has type `Indexable<String>` (interface kind). Interface itself is `Indexable` — direct match. `get` method return type is generic parameter `E`, substituted with `String` via `typeSubstitutions`. Result type: `String`.
2. **MIR lowering**: Interface path — same as class path. Emits `CallFunction` (or `InvokeFunction`) targeting interface dispatch for `get`.

### Computed Index

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    Memory<I64> data = [100, 200, 300, 400, 500]
    I64 offset = 2
    I64 value = data[offset + 1]
    puts(value.toString())
    return 0
```

Output: `400`.

**Compilation trace**:
1. **Parsing**: `data[offset + 1]` — index is a full expression. `parseExpression()` inside brackets parses `offset + 1` as `BinaryExpression(+, offset, 1)`.
2. **MIR lowering**: First lowers `offset + 1` to `AddInteger _binop = offset, 1`, then emits `ComputeIndexAddress _index = data, _binop`.
3. **LLVM codegen**: `%idx = add i64 %offset, 1` then `getelementptr i64, ptr %data, i64 %idx`.

### Error Case — Non-Indexable Type

```
package examples

public function main() -> I32:
    I64 number = 42
    I64 digit = number[0]
    return 0
```

**Diagnostic output**:
```
error: type I64 is not indexable
```

Primitive types do not implement `Indexable` and are not arrays. The semantic analyzer rejects the expression.

### Error Case — Non-Integer Array Index

```
package examples

public function main() -> I32:
    Memory<I64> data = [1, 2, 3]
    I64 value = data[1.5]
    return 0
```

**Diagnostic output**:
```
error: array index must be an integer
```

Array index access requires `isIntegral()`. Float values are rejected.
