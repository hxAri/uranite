# Tuple Types

Uranite's tuple type represents a fixed-size, ordered sequence of elements. The type system has two representations: `Type::Kind::Tuple` with the `TupleType` struct for type annotations containing heterogeneous element types, and `Type::Kind::Class` with the monomorphized `Tuple<E>` class for tuple literal expressions. Tuple literal syntax `(1, 2, 3)` desugars to `Tuple<E>` construction in MIR lowering — the concrete `Tuple<E>` class in `stdlibs/collection/tuple.urn` backs tuples with `Memory<E>` for contiguous element storage and implements `Iterable<E>` for for-in loop support.

This document covers the complete tuple type specification — the `TupleType` struct with heterogeneous element types, tuple type annotation parsing via parenthesized syntax, tuple literal expression parsing with single-element disambiguation, semantic analysis with element type inference and `Tuple<E>` monomorphization, the full compilation pipeline from AST through HIR to MIR `Tuple.set()` desugaring, the `Tuple<E>` stdlib implementation backed by `Memory<E>`, the `TupleIterator<E>` for iteration support, and the relationship between the annotation-level `TupleType` and the runtime `Tuple<E>` class.

---

## Table of Contents

- [Type Identity](#type-identity)
- [The TupleType Struct](#the-tupletype-struct)
  - [Structure](#structure)
  - [Display Representation](#display-representation)
  - [Factory Method](#factory-method)
- [Tuple Type Annotations](#tuple-type-annotations)
  - [Syntax](#syntax)
  - [Single-Element Rule](#single-element-rule)
  - [Parsing Implementation](#parsing-implementation)
  - [Semantic Resolution](#semantic-resolution)
- [Tuple Literal Expressions](#tuple-literal-expressions)
  - [Syntax](#literal-syntax)
  - [AST Representation](#ast-representation)
  - [Disambiguation from Grouping](#disambiguation-from-grouping)
  - [Semantic Analysis and Type Inference](#semantic-analysis-and-type-inference)
- [Compilation Pipeline](#compilation-pipeline)
  - [Parser Stage](#parser-stage)
  - [Semantic Stage](#semantic-stage)
  - [HIR Stage](#hir-stage)
  - [MIR Stage — Tuple Desugaring](#mir-stage--tuple-desugaring)
  - [Codegen Stage](#codegen-stage)
- [The Tuple\<E\> Stdlib Class](#the-tuplee-stdlib-class)
  - [Class Definition](#class-definition)
  - [Internal Storage](#internal-storage)
  - [Constructor](#constructor)
  - [Element Access Methods](#element-access-methods)
  - [Size Methods](#size-methods)
  - [Concatenation](#concatenation)
  - [Object Protocol Methods](#object-protocol-methods)
- [TupleIterator\<E\>](#tupleiteratore)
  - [Iterator Protocol](#iterator-protocol)
  - [For-In Loop Integration](#for-in-loop-integration)
- [Assignability Rules](#assignability-rules)
- [Examples](#examples)
  - [Tuple Literals](#tuple-literals)
  - [Element Access](#element-access)
  - [Iteration](#iteration)
  - [Concatenation](#concatenation-examples)
  - [Practical Usage](#practical-usage)

---

## Type Identity

| Property | Value |
|---|---|
| Type Kind (annotation) | `Type::Kind::Tuple` |
| Type Kind (literal) | `Type::Kind::Class` (monomorphized `Tuple<E>`) |
| Annotation Syntax | `(Type, Type, ...)` |
| Literal Syntax | `(expr, expr, ...)` |
| Qualified Name | `uranite.collection.tuple.Tuple` |
| Package | `uranite.collection.tuple` |
| LLVM Representation | Opaque pointer (`ptr`) to `Memory<E>` buffer |

Two distinct representations serve different roles. The `TupleType` struct (Kind::Tuple) holds heterogeneous element types for type annotations — `(I64, String, Boolean)` stores three different types. Tuple literal expressions resolve to monomorphized `Tuple<E>` class type — `(1, 2, 3)` becomes `Tuple<I64>` with a single generic parameter inferred from the first element.

---

## The TupleType Struct

### Structure

Defined in `src/uranite/semantic/typeref.hpp:302-329`:

```
struct TupleType : Type
    elements : std::vector<TypeSharedPointer>
```

| Field | Type | Description |
|---|---|---|
| `elements` | `std::vector<TypeSharedPointer>` | Ordered collection of element types |

The constructor sets `Kind::Tuple` with an empty name string:

```
TupleType(elements) → Type(Type::Kind::Tuple, "")
    .elements = elements
```

Unlike `ArrayType` which has a single `elementType`, `TupleType` stores a vector of potentially different types — supporting heterogeneous tuples like `(I64, String, Boolean)`.

### Display Representation

The `toString()` method at `typeref.hpp:311-318` produces comma-separated element types in parentheses:

```
toString():
    result = ""
    for i in 0..elements.size():
        if i > 0: result += ", "
        result += elements[i].toString()
    return "( result )"
```

| Input Elements | Output |
|---|---|
| `[I64, String]` | `( I64, String )` |
| `[Boolean]` | `( Boolean )` |
| `[I64, F64, Char]` | `( I64, F64, Char )` |

### Factory Method

The `Registry::makeTuple()` method at `typeref.cpp:644-645` creates `TupleType` instances:

```
TypeSharedPointer Registry::makeTuple(std::vector<TypeSharedPointer> elements)
    → std::make_shared<TupleType>(elements)
```

Called from the semantic analyzer when resolving `TupleTypeNode` AST nodes.

---

## Tuple Type Annotations

### Syntax

Tuple type annotations use parenthesized, comma-separated types:

```uranite
(I64, String) pair
(I64, I64, I64) triple
(String, Boolean, F64) mixed
```

Each position holds an independent type — tuples are heterogeneous by design.

### Single-Element Rule

A single type in parentheses is **not** a tuple — it is a grouping expression that returns the inner type directly:

```uranite
(I64)     → resolves to I64, not (I64,)
(I64,)    → not valid syntax
```

The parser enforces this: when `elements.size() == 1`, the single element type node is returned directly rather than wrapping in a `TupleTypeNode`. This prevents ambiguity between parenthesized types for precedence grouping and single-element tuples.

### Parsing Implementation

The parser handles tuple type annotations at `parser.cpp:496-509`:

```
if check(LeftParenthesis):
    advance()
    elements = []
    if check(RightParenthesis) is false:
        elements.push_back(parseTypeNode())
        while match(Comma):
            elements.push_back(parseTypeNode())
    expect(RightParenthesis, "expected ')'")
    if elements.size() == 1:
        return elements[0]
    return TupleTypeNode(elements, source)
```

Key details:
- `LeftParenthesis` (`(`) in type context triggers tuple type parsing
- Each element is parsed as a full type node (generics, optionals, nested tuples all supported)
- Single-element parenthesized types collapse to the inner type
- Empty parentheses `()` produce an empty `TupleTypeNode`

### Semantic Resolution

The semantic analyzer resolves `TupleTypeNode` to `TupleType` at `analyzer.cpp:5167-5174`:

```
case TupleType:
    elementTypes = []
    for elementNode in tupleNode.elements:
        resolvedElement = resolveType(elementNode)
        elementTypes.push_back(resolvedElement or getError())
    return typeRegistry.makeTuple(elementTypes)
```

Each element type is resolved independently. Failed resolutions produce error types rather than aborting — partial type information is preserved.

---

## Tuple Literal Expressions

### Literal Syntax

Tuple literals use parenthesized, comma-separated expressions:

```uranite
(1, 2, 3)
("hello", 42, true)
(getX(), getY())
```

### AST Representation

The `TupleExpression` AST node at `node.hpp:1818-1835`:

```
struct TupleExpression : Expression
    elements : std::vector<ExpressionSharedPointer>
```

Each element is a full expression — literals, function calls, binary operations, member accesses, and any other expression form are valid tuple elements.

### Disambiguation from Grouping

The parser at `parser.cpp:2289-2308` distinguishes tuple literals from parenthesized grouping expressions:

```
case LeftParenthesis:
    advance()
    if check(RightParenthesis):
        advance()
        return TupleExpression(empty)
    expression = parseExpression()
    if match(Comma):
        elements = [expression]
        if check(RightParenthesis) is false:
            do:
                elements.push_back(parseExpression())
            while match(Comma)
        expect(RightParenthesis)
        return TupleExpression(elements)
    expect(RightParenthesis)
    return expression
```

Disambiguation rules:
- `()` — empty tuple literal
- `(expr)` — parenthesized grouping (returns inner expression, **not** a tuple)
- `(expr,)` — single-element tuple (trailing comma triggers tuple path)
- `(expr, expr, ...)` — multi-element tuple literal

The critical token is the comma after the first expression. Without a comma, parentheses serve as grouping operators. With a comma, the parser commits to the tuple literal path.

### Semantic Analysis and Type Inference

The semantic analyzer processes tuple literals at `analyzer.cpp:2223-2239`:

```
case TupleExpression:
    inferredElementType = null
    for element in tupleExpression.elements:
        elementType = analyzeExpression(element)
        if inferredElementType is null and elementType is not error:
            inferredElementType = elementType
    if inferredElementType is not null:
        expressionType = monomorphizeGenericType("Tuple", [inferredElementType])
    else:
        expressionType = lookupType("Tuple") or Class("Tuple")
```

Key behavior:
- First non-error element determines the generic parameter
- Result is monomorphized `Tuple<E>` — a class type, not a `TupleType`
- Homogeneous type inference: `(1, 2, 3)` becomes `Tuple<I64>`, not `(I64, I64, I64)`
- Falls back to unparameterized `Tuple` when no element type can be inferred

This means tuple literals are **homogeneous** at the runtime level — all elements share the same type parameter `E`. The heterogeneous `TupleType` is for type annotations only. At runtime, `(1, "hello")` would infer `Tuple<I64>` from the first element, and the string would need to be type-compatible with the inferred element type.

---

## Compilation Pipeline

### Parser Stage

The parser encounters `LeftParenthesis` in expression context. If the first expression is followed by a comma (or the parentheses are empty), it produces a `TupleExpression`. Otherwise, it returns the inner expression as a grouping.

### Semantic Stage

Tuple literal `(10, 20, 30)` resolves to `Tuple<I64>` via monomorphization:

```
Source:  (10, 20, 30)
Type:    Tuple<I64> (monomorphized class type)
```

The element type `I64` is inferred from the first element. The expression's `semanticType` is set to the monomorphized `Tuple<E>` class type.

### HIR Stage

HIR lowering at `hir/lowering.cpp:1089-1095` creates an `HIRTupleLiteral` node:

```
case TupleExpression:
    elementExpressions = [lowerExpression(element) for element in elements]
    return HIRTupleLiteral(elementExpressions, semanticType, source)
```

The `HIRTupleLiteral` struct at `hir.hpp:918-930`:

```
struct HIRTupleLiteral : HIRNode
    elementExpressions : std::vector<HIRNodeSharedPointer>
```

Each element expression is recursively lowered. The `resolvedType` carries the `Tuple<E>` type from semantic analysis.

### MIR Stage — Tuple Desugaring

MIR lowering at `mir/lowering.cpp:2865-2928` desugars `HIRTupleLiteral` into `Tuple<E>` construction and per-element `set()` calls:

**Step 1 — Lower element expressions.** Each element is lowered to a MIR variable:

```
elementVariables = [lowerExpression(element) for element in tupleLiteral.elementExpressions]
elementCount = elementVariables.size()
```

**Step 2 — Emit count constant.** A `ConstantInteger` instruction emits the element count (used as the `Tuple(size)` constructor argument):

```
ConstantInteger(value=elementCount, type=I64)  → _tuple_count
```

Unlike array literals which use `max(count, 16)` for capacity, tuples use the exact element count — they are fixed-size and never grow.

**Step 3 — Construct Tuple.** A `ConstructObject` instruction creates the `Tuple<E>` instance with the count operand:

```
ConstructObject(operandType=Tuple<E>, calledFunctionQualifiedName="Tuple")
    sourceOperands = [countVariable]
    → _tuple_literal
```

**Step 4 — Set each element.** For each element, an index constant and a `CallFunction` instruction call `Tuple.set()`:

```
for elementIndex in 0..elementCount:
    ConstantInteger(value=elementIndex, type=I64)  → _tuple_idx
    CallFunction(calledFunctionQualifiedName="Tuple.set")
        sourceOperands = [tupleVariable, idxVariable, elementVariables[elementIndex]]
        operandType = Void
        → _tuple_set
```

Each `set()` call includes a `landingPadTarget` if inside a try block.

**Complete desugaring example for `(10, 20, 30)`:**

```
ConstantInteger(value=3)                → _tuple_count
ConstructObject("Tuple")                → _tuple_literal
    sourceOperands = [_tuple_count]

ConstantInteger(value=0)                → _tuple_idx
CallFunction("Tuple.set")              → _tuple_set
    sourceOperands = [_tuple_literal, _tuple_idx, elem_0]

ConstantInteger(value=1)                → _tuple_idx
CallFunction("Tuple.set")              → _tuple_set
    sourceOperands = [_tuple_literal, _tuple_idx, elem_1]

ConstantInteger(value=2)                → _tuple_idx
CallFunction("Tuple.set")              → _tuple_set
    sourceOperands = [_tuple_literal, _tuple_idx, elem_2]

return _tuple_literal
```

### Codegen Stage

No tuple-specific codegen exists. Desugared MIR instructions are standard `ConstructObject` and `CallFunction` instructions processed through normal class method dispatch. The `Tuple<E>` constructor and `set()` method compile through the standard class pipeline — constructor allocates `Memory<E>`, `set()` delegates to `Memory.set()` which emits GEP+Store.

---

## The Tuple\<E\> Stdlib Class

### Class Definition

Defined in `stdlibs/collection/tuple.urn`:

```uranite
public class Tuple<E> implements Iterable<E>:
    private Memory<E> data
    private Int count
```

`Tuple<E>` implements `Iterable<E>`, enabling for-in loop iteration and compatibility with any API accepting iterables.

### Internal Storage

| Field | Type | Description |
|---|---|---|
| `data` | `Memory<E>` | Raw typed pointer to contiguous element buffer |
| `count` | `Int` | Fixed element count, set at construction |

Elements are stored contiguously in a `Memory<E>` buffer — same raw pointer abstraction used by `ArrayList` and other collection types. At the LLVM level, `data` is a `calloc`-allocated pointer and `count` is an `i64`.

### Constructor

```uranite
public function Tuple( self, Int size ) -> Void:
    self.count = size
    self.data = new Memory<E>( size )
```

Allocates exactly `size` elements worth of memory via `Memory<E>(size)`, which emits `calloc(size, sizeof(E))`. No over-allocation — tuple capacity equals element count.

### Element Access Methods

| Method | Signature | Description |
|---|---|---|
| `component(index)` | `(Int) -> E` | Positional access by index |
| `get(index)` | `(Int) -> E` | Retrieve element at index |
| `set(index, value)` | `(Int, E) -> Void` | Store element at index |

Both `component()` and `get()` delegate to `self.data.get(index)` — a GEP+Load at the LLVM level. `set()` delegates to `self.data.set(index, value)` — a GEP+Store. All operations are O(1) with zero indirection overhead beyond the GEP computation.

The `set()` method is labeled as used "during construction from literal desugaring to populate tuple slots." While tuples are conceptually immutable after construction, `set()` is public to allow the MIR desugaring to populate elements after constructing the object.

### Size Methods

| Method | Signature | Description |
|---|---|---|
| `length` (property) | `() -> Int` | Element count |
| `size()` | `() -> Int` | Element count |

Both return `self.count`. The `length` property provides attribute-style access (`myTuple.length`), while `size()` matches the collection method convention.

### Concatenation

```uranite
public function concatenate( self, Tuple<E> other ) -> Tuple<E>:
    Int totalSize = self.count + other.count
    Tuple<E> result = new Tuple<E>( totalSize )
    Int writeIndex = 0
    while writeIndex < self.count:
        result.set( writeIndex, self.data.get( writeIndex ) )
        writeIndex = writeIndex + 1
    Int otherIndex = 0
    while otherIndex < other.count:
        result.set( writeIndex, other.get( otherIndex ) )
        writeIndex = writeIndex + 1
        otherIndex = otherIndex + 1
    return result
```

Creates a new tuple with combined elements. Time and space complexity: O(n + m) where n and m are the sizes of the two tuples. Neither source tuple is modified.

### Object Protocol Methods

| Method | Signature | Current Implementation |
|---|---|---|
| `equals(other)` | `(Object) -> Boolean` | Returns `False` (stub) |
| `hashCode()` | `() -> Int` | Index-based hash: `17 * 31 + index` per element |
| `toString()` | `() -> String` | Returns `"Tuple"` (stub) |

The `equals()` and `toString()` methods are stubs — `equals()` always returns `False` and `toString()` returns the string `"Tuple"` without element representation. The `hashCode()` method computes a hash from element indices (not element values), using the standard `hash = hash * 31 + elementIndex` accumulation pattern.

---

## TupleIterator\<E\>

### Iterator Protocol

Defined in `stdlibs/collection/tuple.urn:242-295`:

```uranite
public class TupleIterator<E> implements Iterator<E>:
    private Tuple<E> source
    private Int position
```

| Field | Type | Description |
|---|---|---|
| `source` | `Tuple<E>` | Reference to the tuple being iterated |
| `position` | `Int` | Current cursor position, starts at 0 |

| Method | Signature | Description |
|---|---|---|
| `has` (property) | `() -> Boolean` | `position < source.length` |
| `next` (property) | `() -> E` | Returns element at `position`, then increments |

The iterator follows the standard Uranite iterator protocol — `has` checks whether more elements remain, `next` returns the current element and advances. Both are properties (not methods), accessed as `iterator.has` and `iterator.next`.

### For-In Loop Integration

`Tuple<E>` implements `Iterable<E>` with an `iterator` property:

```uranite
public property iterator( self ) -> Iterator<E>:
    return new TupleIterator<E>( self )
```

This enables direct for-in loop usage:

```uranite
Tuple<I64> values = (10, 20, 30)
for I64 value in values:
    process( value )
```

The for-in loop calls `iterator` to get a `TupleIterator<E>`, then uses `has`/`next` to traverse elements.

---

## Assignability Rules

Tuple assignability depends on which representation is involved:

**TupleType (Kind::Tuple) — type annotations:**

No explicit `Kind::Tuple` handling exists in `isAssignable()`. Tuple type annotations rely on the `equals()` identity check — two `TupleType` instances match when they have the same element types in the same order.

**Tuple\<E\> (Kind::Class) — tuple literals:**

Since tuple literals monomorphize to `Tuple<E>` class type, standard class assignability rules apply:

| Assignment | Rule | Mechanism |
|---|---|---|
| `Tuple<E>` → `Tuple<E>` | Class identity match | Qualified name comparison |
| `Tuple<E>` → `Iterable<E>` | Interface implementation | `Tuple` implements `Iterable` |
| `Tuple<E>` → `Object` | Universal supertype | `Object` target accepts any class |
| `GenericParameter` | Always assignable | Any source/target with `Kind::GenericParameter` passes |

---

## Examples

### Tuple Literals

```uranite
from uranite.io.console import puts

Tuple<I64> point = (10, 20)
Tuple<String> names = ("Alice", "Bob", "Charlie")
Tuple<Boolean> flags = (true, false, true)

Tuple<F64> empty = ()

puts( point.size().toString() )
puts( names.get( 0 ) )
```

Tuple literals desugar to `Tuple<E>` construction. Each element is individually stored via `.set()`. The compiler infers the generic parameter from the first element.

### Element Access

```uranite
from uranite.io.console import puts

Tuple<I64> coordinates = (100, 200, 300)

I64 x = coordinates.get( 0 )
I64 y = coordinates.get( 1 )
I64 z = coordinates.get( 2 )

I64 first = coordinates.component( 0 )

puts( x.toString() )
puts( y.toString() )
puts( z.toString() )
```

Both `get()` and `component()` provide O(1) element access via `Memory.get()`, which compiles to a single GEP+Load instruction pair.

### Iteration

```uranite
from uranite.io.console import puts

Tuple<String> languages = ("Uranite", "Rust", "C++")
for String language in languages:
    puts( language )

Tuple<I64> numbers = (1, 2, 3, 4, 5)
I64 total = 0
for I64 number in numbers:
    total = total + number
puts( total.toString() )
```

For-in loops work because `Tuple<E>` implements `Iterable<E>`. The `TupleIterator<E>` traverses elements sequentially from index 0 to `count - 1`.

### Concatenation Examples

```uranite
from uranite.io.console import puts

Tuple<I64> first = (1, 2, 3)
Tuple<I64> second = (4, 5, 6)
Tuple<I64> combined = first.concatenate( second )

puts( combined.size().toString() )

for I64 value in combined:
    puts( value.toString() )
```

`concatenate()` creates a new tuple with all elements from both sources. Neither original tuple is modified.

### Practical Usage

```uranite
from uranite.io.console import puts
from uranite.collection.tuple import Tuple

public function makeRange( I64 start, I64 end ) -> Tuple<I64>:
    Tuple<I64> result = new Tuple<I64>( 2 )
    result.set( 0, start )
    result.set( 1, end )
    return result

public function sumTuple( Tuple<I64> values ) -> I64:
    I64 total = 0
    for I64 value in values:
        total = total + value
    return total

public function zipIndices( Tuple<String> items ) -> Void:
    I64 index = 0
    for String item in items:
        puts( index.toString() + ": " + item )
        index = index + 1

public function main() -> I32:
    Tuple<I64> range = makeRange( 10, 50 )
    puts( range.get( 0 ).toString() )
    puts( range.get( 1 ).toString() )

    Tuple<I64> scores = (95, 87, 92, 78, 100)
    I64 total = sumTuple( scores )
    puts( "Total: " + total.toString() )

    Tuple<String> colors = ("red", "green", "blue")
    zipIndices( colors )

    Tuple<I64> first = (1, 2, 3)
    Tuple<I64> second = (4, 5, 6)
    Tuple<I64> all = first.concatenate( second )
    puts( "Combined size: " + all.size().toString() )

    return 0
```

This example demonstrates tuple construction (both literal and manual), element access via `get()` and `set()`, iteration with for-in loops via the `Iterable<E>` implementation, summation over tuple elements, index-paired traversal, and tuple concatenation — all backed by `Memory<E>` for O(1) element access with zero overhead beyond GEP pointer arithmetic.
