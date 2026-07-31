# Array Types

Uranite has two distinct array-related abstractions: the raw `Memory<T>` intrinsic and the `ArrayList<E>` collection class. The type system represents both through different kinds — `Type::Kind::Array` for statically-typed array annotations, and `Type::Kind::Class` for the `ArrayList<E>` that array literals actually produce. Array literal syntax `[1, 2, 3]` desugars entirely to `ArrayList<E>` construction in MIR lowering — there is no raw-array literal path. For raw memory, developers use `Memory<T>` directly with explicit allocation and element access via compiler-intrinsic `get()`/`set()` methods that map to GEP+Load/Store LLVM instructions.

This document covers the complete array type specification — the `ArrayType` struct and its dual-mode sizing, array type annotation parsing via `[Type; Size]` syntax, array literal expression parsing and semantic analysis with element type inference, the full compilation pipeline from `ArrayExpression` through HIR to MIR `ArrayList<E>` desugaring, the `Memory<T>` compiler intrinsic with `calloc`-based allocation and GEP-based element access, comprehension expressions with range iteration and conditional filtering, for-in loop element type extraction, and the relationship between raw arrays and collection types.

---

## Table of Contents

- [Type Identity](#type-identity)
- [The ArrayType Struct](#the-arraytype-struct)
  - [Structure](#structure)
  - [Display Representation](#display-representation)
  - [Factory Method](#factory-method)
- [Array Type Annotations](#array-type-annotations)
  - [Fixed-Size Syntax](#fixed-size-syntax)
  - [Dynamic-Size Syntax](#dynamic-size-syntax)
  - [Variadic Parameter Arrays](#variadic-parameter-arrays)
  - [Parsing Implementation](#parsing-implementation)
  - [Semantic Resolution](#semantic-resolution)
- [Array Literal Expressions](#array-literal-expressions)
  - [Syntax](#syntax)
  - [AST Representation](#ast-representation)
  - [Semantic Analysis and Type Inference](#semantic-analysis-and-type-inference)
- [Compilation Pipeline](#compilation-pipeline)
  - [Parser Stage](#parser-stage)
  - [Semantic Stage](#semantic-stage)
  - [HIR Stage](#hir-stage)
  - [MIR Stage — ArrayList Desugaring](#mir-stage--arraylist-desugaring)
  - [Codegen Stage](#codegen-stage)
- [Memory\<T\> — Raw Array Intrinsic](#memoryt--raw-array-intrinsic)
  - [Type Identity](#memory-type-identity)
  - [LLVM Representation](#llvm-representation)
  - [Constructor — calloc Allocation](#constructor--calloc-allocation)
  - [Element Access — get()](#element-access--get)
  - [Element Mutation — set()](#element-mutation--set)
  - [Deallocation — free()](#deallocation--free)
  - [Bulk Copy — copyTo()](#bulk-copy--copyto)
  - [Element Type Resolution](#element-type-resolution)
- [Comprehension Expressions](#comprehension-expressions)
  - [Syntax](#comprehension-syntax)
  - [Semantic Analysis](#comprehension-semantic-analysis)
  - [MIR Lowering — Loop Desugaring](#mir-lowering--loop-desugaring)
  - [Conditional Filtering](#conditional-filtering)
  - [Map and Set Comprehensions](#map-and-set-comprehensions)
- [Iteration](#iteration)
  - [For-In Loop Element Extraction](#for-in-loop-element-extraction)
  - [Comprehension Iterator Variable](#comprehension-iterator-variable)
- [Assignability Rules](#assignability-rules)
- [Heap Allocation and Index Addressing](#heap-allocation-and-index-addressing)
  - [generateHeapAllocate](#generateheapallocate)
  - [generateComputeIndexAddress](#generatecomputeindexaddress)
  - [generateHeapFree](#generateheapfree)
- [Examples](#examples)
  - [Array Literals](#array-literals)
  - [Raw Memory Arrays](#raw-memory-arrays)
  - [Comprehensions](#comprehensions)
  - [Iteration Patterns](#iteration-patterns)
  - [Practical Usage](#practical-usage)

---

## Type Identity

| Property | Value |
|---|---|
| Type Kind (annotation) | `Type::Kind::Array` |
| Type Kind (literal) | `Type::Kind::Class` (monomorphized `ArrayList<E>`) |
| Annotation Syntax | `[Type; Size]` or `[Type]` |
| Literal Syntax | `[expr, expr, ...]` |
| Raw Intrinsic | `Memory<T>` |
| Qualified Name (Memory) | `uranite.memory.memory.Memory` |
| Qualified Name (ArrayList) | `uranite.collection.arraylist.ArrayList` |

The array type system spans two distinct representations. `Type::Kind::Array` with the `ArrayType` struct is used for type annotations — parameter types, variable type declarations, and return types. Array literal expressions `[1, 2, 3]` resolve to `ArrayList<E>` — a monomorphized class type. The `Memory<T>` intrinsic provides raw pointer-based array access for performance-critical code and stdlib internals.

---

## The ArrayType Struct

### Structure

Defined in `src/uranite/semantic/typeref.hpp:263-295`:

```
struct ArrayType : Type
    elementType : TypeSharedPointer
    size        : int64_t
```

| Field | Type | Description |
|---|---|---|
| `elementType` | `TypeSharedPointer` | Type of individual elements |
| `size` | `int64_t` | Static element count; `-1` for dynamic arrays |

The constructor sets `Kind::Array` with an empty name string:

```
ArrayType(element, size) → Type(Type::Kind::Array, "")
    .elementType = element
    .size = size
```

### Display Representation

The `toString()` method produces two formats based on whether the size is known:

| Condition | Format | Example |
|---|---|---|
| `size >= 0` | `[ Type; Size ]` | `[ I64; 10 ]` |
| `size < 0` (dynamic) | `[ Type ]` | `[ I64 ]` |

### Factory Method

The `Registry::makeArray()` method creates `ArrayType` instances:

```
TypeSharedPointer Registry::makeArray(TypeSharedPointer element, int64_t size = -1)
    → std::make_shared<ArrayType>(element, size)
```

The default `size` parameter of `-1` means arrays are dynamic unless an explicit size is provided. This factory is called from the semantic analyzer when resolving `ArrayTypeNode` AST nodes.

---

## Array Type Annotations

### Fixed-Size Syntax

Fixed-size arrays specify an element type and a size expression separated by a semicolon inside brackets:

```uranite
[I64; 10] fixedBuffer
[Char; 256] characterBuffer
[F64; 3] coordinates
```

The size expression is a full expression — it can be a literal integer, a constant reference, or any compile-time-evaluable expression.

### Dynamic-Size Syntax

Dynamic arrays omit the size expression, leaving only the element type inside brackets:

```uranite
[I64] dynamicArray
[String] names
```

Dynamic arrays have `size = -1` in the `ArrayType` struct. They represent arrays whose length is determined at runtime.

### Variadic Parameter Arrays

Function parameters with trailing `[]` syntax produce `ArrayTypeNode` with the parameter's type as the element type:

```uranite
public function sum( I64 values[] ) -> I64:
    I64 total = 0
    for I64 value in values:
        total = total + value
    return total
```

The parser at `parser.cpp:1483-1488` detects the `[]` suffix after a parameter name and wraps the parameter type in an `ArrayTypeNode`. The semantic analyzer extracts `elementType` from the resulting `ArrayType` for variadic element type tracking.

### Parsing Implementation

The parser handles array type annotations at `parser.cpp:486-494`:

```
if check(LeftBracket):
    advance()
    elementType = parseTypeNode()
    size = null
    if match(Semicolon):
        size = parseExpression()
    expect(RightBracket, "expected ']' in array type")
    return ArrayTypeNode(elementType, size, source)
```

Key details:
- `LeftBracket` (`[`) triggers array type parsing
- Element type is parsed as a full type node (supports generics, optionals, nested arrays)
- Semicolon presence distinguishes fixed-size from dynamic
- Size is parsed as a full expression when present

### Semantic Resolution

The semantic analyzer resolves `ArrayTypeNode` to `ArrayType` at `analyzer.cpp:5162-5165`:

```
case ArrayType:
    elementType = resolveType(arrayNode.elementType)
    return elementType ? makeArray(elementType) : getError()
```

Note: the static size from the AST is not propagated — `makeArray()` is called with default `size = -1`. Array type annotations currently resolve to dynamic arrays regardless of whether a size expression was specified in the source.

---

## Array Literal Expressions

### Syntax

Array literals use bracket-enclosed, comma-separated expressions:

```uranite
[1, 2, 3]
["hello", "world"]
[true, false, true]
[getX(), getY(), getZ()]
```

Empty array literals `[]` are valid but produce an untyped `ArrayList` without generic monomorphization.

### AST Representation

The `ArrayExpression` AST node at `node.hpp:1153-1170`:

```
struct ArrayExpression : Expression
    elements : std::vector<ExpressionSharedPointer>
```

Each element is a full expression — literals, function calls, binary operations, member accesses, and any other expression form are valid array elements.

### Semantic Analysis and Type Inference

The semantic analyzer processes array literals at `analyzer.cpp:2088-2104`:

1. Iterate each element expression and analyze it
2. Infer element type from the first non-error element
3. Monomorphize `ArrayList` with the inferred element type
4. Fall back to unparameterized `ArrayList` if no element type can be inferred

```
case ArrayExpression:
    inferredElementType = null
    for element in elements:
        elementType = analyzeExpression(element)
        if inferredElementType is null and elementType is not error:
            inferredElementType = elementType
    if inferredElementType is not null:
        expressionType = monomorphizeGenericType("ArrayList", [inferredElementType])
    else:
        expressionType = lookupType("ArrayList") or Class("ArrayList")
```

The first element determines the collection's generic parameter. Subsequent elements are analyzed but do not override the inferred type — type compatibility between elements is enforced by the assignment and method call rules of `ArrayList<E>.add()`.

---

## Compilation Pipeline

### Parser Stage

The parser encounters `LeftBracket` in expression context and dispatches to array literal parsing. Each comma-separated expression between `[` and `]` is parsed via `parseExpression()` and collected into the `elements` vector of `ArrayExpression`.

For comprehension syntax `[expr for var in iter]`, the parser detects the `for` keyword after the first expression and produces a `ComprehensionExpression` instead of `ArrayExpression`.

### Semantic Stage

Array literal `[1, 2, 3]` resolves to `ArrayList<I64>` via monomorphization:

```
Source:  [1, 2, 3]
Type:    ArrayList<I64> (monomorphized class type)
```

The element type is inferred from the first element. The expression's `semanticType` is set to the monomorphized `ArrayList<E>` class type, which carries through HIR and MIR.

### HIR Stage

HIR lowering at `hir/lowering.cpp:1081-1087` creates an `HIRArrayLiteral` node:

```
case ArrayExpression:
    elementExpressions = [lowerExpression(element) for element in elements]
    return HIRArrayLiteral(elementExpressions, semanticType, source)
```

Each element expression is recursively lowered to HIR form. The `resolvedType` carries the `ArrayList<E>` type from semantic analysis.

### MIR Stage — ArrayList Desugaring

MIR lowering at `mir/lowering.cpp:2811-2863` desugars `HIRArrayLiteral` into `ArrayList<E>` construction and per-element `add()` calls:

**Step 1 — Extract class name.** The resolved type's qualified name (or plain name) is extracted, stripping any generic bracket suffix to get the base class name (e.g., "ArrayList").

**Step 2 — Emit capacity constant.** A `ConstantInteger` instruction emits the initial capacity:

```
capacity = max(elementCount, 16)
```

Minimum capacity of 16 ensures small arrays do not immediately trigger resize. Arrays with more than 16 elements use exact count as capacity.

**Step 3 — Construct ArrayList.** A `ConstructObject` instruction creates the `ArrayList<E>` instance with the capacity operand:

```
ConstructObject(operandType=ArrayList<E>, calledFunctionQualifiedName="ArrayList")
    sourceOperands = [capacityVariable]
    → listVariable
```

**Step 4 — Add each element.** For each element, a `CallFunction` instruction calls `ArrayList.add()`:

```
for elementIndex in 0..elementCount:
    CallFunction(calledFunctionQualifiedName="ArrayList.add")
        sourceOperands = [listVariable, elementVariables[elementIndex]]
        operandType = Void
        → addResult
```

If an active landing pad exists (inside a try block), each `add()` call gets a `landingPadTarget` for exception propagation.

**Complete desugaring example for `[10, 20, 30]`:**

```
ConstantInteger(value=16)               → _arr_cap
ConstructObject("ArrayList")            → _arr_literal
    sourceOperands = [_arr_cap]
CallFunction("ArrayList.add")           → _arr_add
    sourceOperands = [_arr_literal, elem_0]
CallFunction("ArrayList.add")           → _arr_add
    sourceOperands = [_arr_literal, elem_1]
CallFunction("ArrayList.add")           → _arr_add
    sourceOperands = [_arr_literal, elem_2]
return _arr_literal
```

### Codegen Stage

The desugared MIR instructions are standard `ConstructObject` and `CallFunction` instructions — no array-specific codegen path exists. The `ArrayList` class constructor and `add()` method are compiled through the normal class method dispatch pipeline, including vtable lookup and itable-based virtual dispatch if the `ArrayList` implements collection interfaces.

---

## Memory\<T\> — Raw Array Intrinsic

### Memory Type Identity

| Property | Value |
|---|---|
| Type Kind | `Type::Kind::Class` |
| Class Modifier | `final native` |
| Qualified Name | `uranite.memory.memory.Memory` |
| Package | `uranite.memory.memory` |
| LLVM Representation | Opaque pointer (`ptr`) |
| Backing Allocation | `calloc(capacity, sizeof(T))` |

`Memory<T>` is declared as `public final native class Memory<T>` in `stdlibs/memory/memory.urn`. The `native` modifier signals the compiler that all methods are compiler intrinsics — their implementations are generated directly as LLVM IR instructions, not compiled from Uranite source.

### LLVM Representation

`Memory<T>` instances are raw pointers at the LLVM level. No struct wrapping, no vtable, no metadata — a `Memory<I64>` is a single `ptr` pointing to a `calloc`-allocated buffer.

### Constructor — calloc Allocation

When the codegen encounters a `ConstructObject` instruction for `Memory`, it bypasses normal class construction and emits a `calloc` call at `codegen.cpp:6152-6178`:

```
elementType = resolveMemoryElementType(operandType)
capacityValue = loadVariableValue(sourceOperands[0]) or default 16
callocFunction = getOrCreateCalloc()
elementSize = DataLayout.getTypeAllocSize(elementType)
rawPointer = CreateCall(calloc, [capacityValue, elementSizeValue], "mem.raw")
setVariableValue(destination, rawPointer)
memoryElementTypes[destination] = elementType
```

Key details:
- Uses `calloc` (not `malloc`) — memory is zero-initialized
- Element size is computed via LLVM's `DataLayout.getTypeAllocSize()` for correct alignment
- Default capacity of 16 when no operand is provided
- The element type is tracked in `memoryElementTypes` map for subsequent `get()`/`set()` calls

### Element Access — get()

The `Memory.get(index)` method at `codegen.cpp:2756-2791` emits a GEP (GetElementPtr) followed by a Load:

```
Memory.get(self, index):
    elementType = resolve from memoryElementTypes or variable descriptor or default i64
    if index is not integer: FPToSI conversion
    gepValue = CreateGEP(elementType, selfPointer, indexValue, "mem.get.gep")
    loadedValue = CreateLoad(elementType, gepValue, "mem.get.val")
    return loadedValue
```

Element type resolution follows a priority chain:
1. `memoryElementTypes` map (cached from constructor)
2. Variable descriptor table in current MIR function
3. `operandType` from the instruction
4. Default `i64`

The GEP instruction computes element-stride-aware pointer arithmetic. For `Memory<I64>`, `get(3)` computes `base + 3 * 8` bytes. For `Memory<I32>`, `get(3)` computes `base + 3 * 4` bytes.

### Element Mutation — set()

The `Memory.set(index, value)` method at `codegen.cpp:2793-2836` emits a GEP followed by a Store:

```
Memory.set(self, index, value):
    elementType = resolve from memoryElementTypes or variable descriptor or default i64
    if index is not integer: FPToSI conversion
    if value type ≠ elementType:
        integer-to-integer: CreateIntCast
        integer-to-float: CreateSIToFP
        float-to-integer: CreateFPToSI
    gepValue = CreateGEP(elementType, selfPointer, indexValue, "mem.set.gep")
    CreateStore(storeValue, gepValue)
```

Automatic type coercion handles mismatched element types — if you store an `I32` into a `Memory<I64>`, the value is automatically widened via `CreateIntCast`. Float-to-integer and integer-to-float conversions are also handled transparently.

### Deallocation — free()

The `Memory.free()` method at `codegen.cpp:2838-2847` calls the C library `free()`:

```
Memory.free(self):
    castPointer = CreateBitCast(selfPointer, ptr, "mem.free.cast")
    CreateCall(free, [castPointer])
```

After calling `free()`, the `Memory<T>` instance must not be used again — there is no use-after-free detection at the `Memory<T>` level (the borrow checker handles this at a higher level).

### Bulk Copy — copyTo()

The `Memory.copyTo(destination, length)` method at `codegen.cpp:2849-2853` emits an `llvm.memcpy` intrinsic for optimized bulk memory copy.

### Element Type Resolution

The `resolveMemoryElementType()` function resolves the generic parameter `T` in `Memory<T>` to an LLVM type. It extracts the generic argument from the type's qualified name or class type and maps it through `toLLVMType()`. Results are cached in `memoryElementTypes` to avoid repeated resolution for the same variable.

---

## Comprehension Expressions

### Comprehension Syntax

List comprehensions produce `ArrayList<E>` instances from iteration expressions:

```uranite
[expression for Type variable in iterable]
[expression for Type variable in iterable if condition]
[expression for Type variable in start..end]
[expression for Type variable in start..=end]
```

Components:
- **body expression** — evaluated per iteration, result added to output collection
- **iterator variable** — bound to each element/index
- **iterable** — range expression, array, or any iterable type
- **condition** (optional) — boolean filter; element added only when true

### Comprehension Semantic Analysis

The semantic analyzer at `analyzer.cpp:2149-2220` processes comprehensions:

1. Push a new block scope for the loop variable
2. Analyze the iterable expression to determine iteration type
3. Infer iterator variable type from iterable:
   - `Type::Kind::Array` → extract `elementType`
   - `Type::Kind::String` → `Char`
   - `Type::Kind::Generator` → extract `yieldType`
   - Range expression → `I64`
   - Otherwise → `I64` (default)
4. Define the loop variable symbol in the block scope
5. Analyze the body expression to determine output element type
6. Analyze condition expression (if present) — must be boolean
7. Determine result type based on comprehension kind:

| Comprehension Kind | Result Type | Monomorphization |
|---|---|---|
| `List` (default) | `ArrayList<BodyType>` | `monomorphizeGenericType("ArrayList", [bodyType])` |
| `Set` | `HashSet<BodyType>` | `monomorphizeGenericType("HashSet", [bodyType])` |
| `Map` | `HashMap<KeyType, BodyType>` | `monomorphizeGenericType("HashMap", [keyType, bodyType])` |

### MIR Lowering — Loop Desugaring

Comprehension MIR lowering at `mir/lowering.cpp:2929-3107` produces a 4-block loop structure for range-based iteration:

**Step 1 — Construct output collection.** A `ConstructObject` instruction creates the target collection (ArrayList, HashSet, or HashMap).

**Step 2 — Initialize loop variable.** An `AllocateLocal` + `StoreVariable` pair initializes the loop variable with the range start value. The variable is registered in `variableNameMap` so the body expression can reference it.

**Step 3 — Create control flow blocks:**

```
comp.header  — loop condition check
comp.body    — body expression evaluation + element insertion
comp.update  — loop variable increment
comp.exit    — post-loop continuation
```

**Step 4 — Header block.** Load the loop variable, compare against the end value:
- Exclusive range (`start..end`): `CompareLessThan`
- Inclusive range (`start..=end`): `CompareLessEqual`

Branch to `comp.body` if true, `comp.exit` if false.

**Step 5 — Body block.** Lower the body expression (and key expression for map comprehensions). If a condition is present, evaluate it and branch to either a `comp.add` block or directly to `comp.update`. Insert the result into the collection:

| Kind | Insertion Method |
|---|---|
| List/Set | `CallFunction(className + ".add", [collection, bodyResult])` |
| Map | `CallFunction(className + ".put", [collection, keyResult, bodyResult])` |

**Step 6 — Update block.** Load the loop variable, add constant `1` (via `AddInteger`), store the incremented value back, and jump unconditionally to `comp.header`.

**Step 7 — Exit block.** Return the collection variable.

**Complete MIR flow for `[x * 2 for I64 x in 0..5]`:**

```
ConstructObject("ArrayList")                → _comp_list
AllocateLocal("x", I64)                     → loopVar
StoreVariable(loopVar, startValue)

comp.header:
    LoadVariable(loopVar)                   → _comp_cur
    CompareLessThan(_comp_cur, endValue)     → _comp_cond
    BranchConditional(_comp_cond, comp.body, comp.exit)

comp.body:
    [lower body: x * 2]                     → bodyResult
    CallFunction("ArrayList.add", [_comp_list, bodyResult])
    JumpUnconditional(comp.update)

comp.update:
    LoadVariable(loopVar)                   → _comp_cur_upd
    ConstantInteger(1)                      → _comp_one
    AddInteger(_comp_cur_upd, _comp_one)    → _comp_next
    StoreVariable(loopVar, _comp_next)
    JumpUnconditional(comp.header)

comp.exit:
    return _comp_list
```

### Conditional Filtering

When a comprehension includes an `if condition` clause, an additional `comp.add` block is inserted between the condition check and the element insertion:

```
comp.body:
    [lower body expression]                 → bodyResult
    [lower condition expression]            → condResult
    BranchConditional(condResult, comp.add, comp.update)

comp.add:
    CallFunction("ArrayList.add", [_comp_list, bodyResult])
    JumpUnconditional(comp.update)
```

Elements that fail the condition skip directly to the update block without insertion.

### Map and Set Comprehensions

Comprehension kind determines the insertion method:

```
if comprehensionKind == Map:
    CallFunction(className + ".put", [collection, keyResult, bodyResult])
else:
    CallFunction(className + ".add", [collection, bodyResult])
```

Map comprehensions require both a key expression and a body expression. Set comprehensions use the same `.add()` insertion as list comprehensions but produce a `HashSet<E>` instead of `ArrayList<E>`.

---

## Iteration

### For-In Loop Element Extraction

The semantic analyzer extracts element types from array types for `for-in` loops at `analyzer.cpp:2597-2598`:

```
if iterableType.kind == Kind::Array:
    variableType = static_pointer_cast<ArrayType>(iterableType).elementType
```

This enables direct iteration over arrays with automatic element type binding:

```uranite
[I64] numbers = getNumbers()
for I64 number in numbers:
    process( number )
```

The same extraction logic applies in comprehension variable type inference at `analyzer.cpp:2158-2159`.

The semantic analyzer validates that the iterable type is actually iterable at `analyzer.cpp:2577-2590`. Types that are not `Array`, `String`, `Generator`, and do not implement `Iterator` or `Iterable` interface produce an error diagnostic:

```
"type \"TypeName\" is not iterable; class must implement Iterator or Iterable interface"
```

### Comprehension Iterator Variable

Comprehension iterator variables follow the same type inference as for-in loops. When the iterator variable has an explicit type annotation, that type is used directly. When no annotation is provided, the type is inferred from the iterable:

| Iterable Kind | Inferred Variable Type |
|---|---|
| `Type::Kind::Array` | `elementType` from `ArrayType` |
| `Type::Kind::String` | `Char` |
| `Type::Kind::Generator` | `yieldType` from `GeneratorType` |
| Range expression | `I64` |
| Other | `I64` (default) |

---

## Assignability Rules

The `isAssignable()` function in `typeref.cpp` has no explicit `Kind::Array` handling. Array type assignability relies on the general rules:

| Assignment | Rule | Mechanism |
|---|---|---|
| `[T]` → `[T]` | Identity match via `equals()` | Both are `ArrayType` with matching `elementType` |
| `ArrayList<E>` → `ArrayList<E>` | Class identity match | Qualified name comparison |
| `ArrayList<E>` → `Iterable<E>` | Interface implementation | Interface hierarchy walk |
| `ArrayList<E>` → `Object` | Universal supertype | `Object` target accepts any class |
| `[T; N]` → `[T]` | Not explicitly handled | Array type identity uses both `elementType` and `size` |
| `GenericParameter` | Always assignable | Any source/target with `Kind::GenericParameter` passes |

Because array literals desugar to `ArrayList<E>`, assignability for `[1, 2, 3]` is governed by class assignability rules, not array type rules. The `ArrayList<E>` class inherits from collection interfaces, enabling assignment to `Iterable<E>`, `Sequence<E>`, and other interface targets.

---

## Heap Allocation and Index Addressing

These low-level MIR instructions are used by `Memory<T>` and raw array operations at the codegen level.

### generateHeapAllocate

At `codegen.cpp:6067-6082`, the `HeapAllocate` instruction emits a `malloc` call:

```
mallocFunction = getOrCreateMalloc()
allocationSize = instruction.integerConstantValue > 0 ? integerConstantValue : 8
sizeValue = ConstantInt::get(i64, allocationSize)
mallocResult = CreateCall(malloc, [sizeValue], "heap")
setVariableValue(destination, mallocResult)
```

Default allocation size is 8 bytes when no explicit size is provided. The result is a raw pointer stored in the destination variable.

### generateComputeIndexAddress

At `codegen.cpp:6042-6064`, the `ComputeIndexAddress` instruction computes an element pointer:

```
basePointer = loadVariableValue(sourceOperands[0])
indexValue = loadVariableValue(sourceOperands[1])
if basePointer is pointer:
    elementType = operandType ? toLLVMType(operandType) : i8
    elementPointer = CreateGEP(elementType, basePointer, indexValue, "gep.idx")
    setVariableValue(destination, elementPointer)
```

This is a general-purpose GEP wrapper. The element type defaults to `i8` (byte-addressed) unless an `operandType` provides the actual element type for stride-aware addressing.

### generateHeapFree

At `codegen.cpp:6084-6097`, the `HeapFree` instruction emits a `free` call:

```
pointer = loadVariableValue(sourceOperands[0])
freeFunction = getOrCreateFree()
castPointer = CreateBitCast(pointer, ptr, "free.cast")
CreateCall(free, [castPointer])
```

The pointer is bitcast to an unqualified pointer type before passing to `free()`.

---

## Examples

### Array Literals

```uranite
from uranite.io.console import puts

ArrayList<I64> numbers = [10, 20, 30, 40, 50]
ArrayList<String> names = ["Alice", "Bob", "Charlie"]
ArrayList<Boolean> flags = [true, false, true]

puts( numbers.size().toString() )
puts( names.get( 0 ) )

ArrayList<F64> empty = []
```

Array literals desugar to `ArrayList<E>` construction. Each element is individually added via `.add()`. The compiler infers the generic parameter from the first element — `[10, 20, 30]` produces `ArrayList<I64>`, `["Alice", "Bob"]` produces `ArrayList<String>`.

### Raw Memory Arrays

```uranite
from uranite.memory.memory import Memory
from uranite.io.console import puts

Memory<I64> buffer = new Memory<I64>( 100 )
buffer.set( 0, 42 )
buffer.set( 1, 99 )

I64 first = buffer.get( 0 )
I64 second = buffer.get( 1 )
puts( first.toString() )
puts( second.toString() )

Memory<I64> copy = new Memory<I64>( 100 )
buffer.copyTo( copy, 2 )

buffer.free()
copy.free()
```

`Memory<T>` provides raw pointer access. Construction calls `calloc(capacity, sizeof(T))`. `get()` and `set()` compile to single GEP+Load and GEP+Store instruction pairs with zero overhead. The developer is responsible for calling `free()` — there is no automatic deallocation.

### Comprehensions

```uranite
from uranite.io.console import puts

ArrayList<I64> squares = [x * x for I64 x in 0..10]

ArrayList<I64> evens = [x for I64 x in 0..20 if x % 2 == 0]

ArrayList<String> labels = ["item_" + index.toString() for I64 index in 0..5]

ArrayList<I64> inclusive = [x for I64 x in 1..=100]
```

Comprehensions desugar to `ArrayList` construction plus a loop that calls `.add()` per iteration. The `if` condition creates a conditional branch — elements that fail the condition skip insertion entirely.

### Iteration Patterns

```uranite
from uranite.io.console import puts

ArrayList<String> fruits = ["apple", "banana", "cherry"]
for String fruit in fruits:
    puts( fruit )

[I64; 5] fixedArray
for I64 element in fixedArray:
    puts( element.toString() )

ArrayList<I64> doubled = [x * 2 for I64 x in 0..10]
for I64 value in doubled:
    puts( value.toString() )
```

For-in loops extract element types from array types automatically. The semantic analyzer infers the loop variable type from the iterable's `elementType` field.

### Practical Usage

```uranite
from uranite.memory.memory import Memory
from uranite.io.console import puts

public function dotProduct( Memory<F64> vectorA, Memory<F64> vectorB, I64 length ) -> F64:
    F64 sum = 0.0
    I64 index = 0
    while index < length:
        F64 product = vectorA.get( index ) * vectorB.get( index )
        sum = sum + product
        index = index + 1
    return sum

public function filterPositive( ArrayList<I64> values ) -> ArrayList<I64>:
    return [value for I64 value in values if value > 0]

public function matrixRow( Memory<F64> matrix, I64 row, I64 columns ) -> Memory<F64>:
    Memory<F64> rowData = new Memory<F64>( columns )
    I64 columnIndex = 0
    while columnIndex < columns:
        rowData.set( columnIndex, matrix.get( row * columns + columnIndex ) )
        columnIndex = columnIndex + 1
    return rowData

public function main() -> I32:
    Memory<F64> vecA = new Memory<F64>( 3 )
    Memory<F64> vecB = new Memory<F64>( 3 )
    vecA.set( 0, 1.0 )
    vecA.set( 1, 2.0 )
    vecA.set( 2, 3.0 )
    vecB.set( 0, 4.0 )
    vecB.set( 1, 5.0 )
    vecB.set( 2, 6.0 )

    F64 result = dotProduct( vecA, vecB, 3 )
    puts( result.toString() )

    vecA.free()
    vecB.free()

    ArrayList<I64> mixed = [-3, 7, -1, 4, 0, -8, 12]
    ArrayList<I64> positive = filterPositive( mixed )
    for I64 value in positive:
        puts( value.toString() )

    return 0
```

This example demonstrates both array models: `Memory<F64>` for performance-critical numerical computation (dot product, matrix row extraction) where GEP+Load/Store instructions compile to single machine instructions, and `ArrayList<I64>` with comprehension-based filtering for higher-level collection manipulation. The two models serve different niches — `Memory<T>` for stdlib internals and performance-sensitive code, `ArrayList<E>` for general-purpose collection usage with automatic growth and iterator support.
