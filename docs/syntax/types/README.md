# Types

Uranite is a statically typed language with strict compile-time type checking. Every variable, parameter, return value, and expression has a type determined during semantic analysis — no value exists at runtime without a known type. This section documents the complete type system: primitive types, composite types, user-defined types, generics, type inference, and the internal machinery that resolves, validates, and emits types through the compilation pipeline.

---

## Table of Contents

### Fundamentals

- [Primitive Types](primitive-types.md) — The built-in scalar types: integers (`I8`, `I16`, `I32`, `I64`), unsigned integers (`U8`, `U16`, `U32`, `U64`), floats (`F32`, `F64`), `Boolean`, `Char`, `String`, `Void`, and `None`.
- [Integer Types](integer-types.md) — Signed and unsigned integer type hierarchy, bit widths, overflow behavior, wrapping arithmetic, and LLVM integer type emission.
- [Floating-Point Types](floating-point-types.md) — `F32` and `F64` types, IEEE 754 semantics, precision, type aliases (`Float`, `Double`), and LLVM float/double emission.
- [Boolean Type](boolean-type.md) — The `Boolean` type, `True`/`False` literals, logical operators (`and`, `or`, `not`), truthiness rules, and `i1` representation.
- [Char Type](char-type.md) — The `Char` type, 32-bit Unicode scalar values, character classification, case conversion, and `i32` representation.
- [String Type](string-type.md) — The `String` type, immutable byte sequences, null-terminated storage, concatenation codegen, and the `String` OOP wrapper class.
- [Void and None Types](void-and-none-types.md) — The `Void` return type, the `None` absence-of-value literal, and the distinction between "no return value" and "no value present."

### Composite Types

- [Array Types](array-types.md) — Raw `Memory<T>` arrays, static sizing, heap allocation, element access via `ComputeIndexAddress`, and the `ArrayType` representation.
- [Tuple Types](tuple-types.md) — Heterogeneous fixed-size collections, the `TupleType` struct, element access, and tuple literal syntax `(a, b, c)`.
- [Optional and Nullable Types](optional-and-nullable-types.md) — The `?T` syntax, `OptionalType` wrapper, `None` assignment, null-pointer representation, and None-comparison rules.
- [Function Types](function-types.md) — The `FunctionType` and `CallableType` representations, first-class function references, parameter types, return types, variadic signatures, and LLVM function pointer emission.
- [Collection Types](collection-types.md) — `ArrayList<E>`, `HashMap<K,V>`, `HashSet<E>`, and their relationship to generic type parameters and the standard library.

### User-Defined Types

- [Classes](classes.md) — Class declarations, fields, methods, constructors, visibility modifiers (`public`, `protect`, `private`), inheritance (`extends`), the `final` modifier, and `ClassType` representation.
- [Structs](structs.md) — Value-type struct declarations, field layout, the `StructType` representation, and LLVM struct emission.
- [Interfaces](interfaces.md) — Interface declarations, method signatures, multiple interface conformance (`implements`), transitive interface inheritance, the `InterfaceType` representation, and itable-based virtual dispatch.
- [Traits](traits.md) — Trait declarations, default method implementations, the `TraitType` representation, and trait-based polymorphism.
- [Enums](enums.md) — Enum declarations with `unit` variants, backed enums with explicit values, the `EnumType` representation, and `i32` LLVM emission.

### Advanced Type Features

- [Generics and Type Parameters](generics-and-type-parameters.md) — Generic type parameters (`<T>`), type constraints, monomorphization, the `GenericParameterType` representation, and type argument substitution.
- [Type Inference and Diamond Syntax](type-inference-and-diamond-syntax.md) — Diamond syntax (`new ArrayList<>()`), type inference from context, explicit versus inferred typing, and the inference algorithm.
- [Union Types](union-types.md) — The `UnionType` representation, multi-type alternatives, and union member containment checking.
- [Pointer and Reference Types](pointer-and-reference-types.md) — Raw `PointerType`, `ReferenceType`, mutability flags, and their role in ownership-aware memory management.
- [Future and Generator Types](future-and-generator-types.md) — `Future<T>` for async return values, `Generator<T>` for lazy yield sequences, and their type wrapping behavior.
- [Meta Types](meta-types.md) — `Meta<T>` for treating types as values, type-level programming, and the `MetaType` representation.

---

## Type System Philosophy

### Strict Compile-Time Checking

Every expression in Uranite has a type determined at compile time. The semantic analyzer resolves all types during its two-pass analysis, and the compiler rejects programs with type mismatches before any code is generated. There is no dynamic typing, no runtime type coercion (except explicit `as` casts), and no implicit conversions between unrelated types.

```uranite
I64 count = 42
String name = "hello"
Boolean active = True
```

Each variable declaration specifies its type explicitly. The compiler verifies that the assigned value is compatible with the declared type.

### Explicit Typing with Diamond Inference

Uranite requires explicit type annotations on variable declarations, function parameters, and return types. The one exception is diamond syntax for constructor calls, where the type arguments can be inferred from context:

```uranite
ArrayList<String> names = new ArrayList<>()
HashMap<String, I64> scores = new HashMap<>()
```

The `<>` in `new ArrayList<>()` tells the compiler to infer the type parameters from the left-hand side of the assignment. This is syntactic sugar — the compiler resolves `ArrayList<>` to `ArrayList<String>` based on the declared variable type.

### Zero-Cost Abstractions

OOP wrapper types (`Boolean`, `Char`, `String`, `Int`, `Float`, etc.) carry no runtime overhead. The `toLLVMType()` function in codegen recognizes OOP wrapper class names and maps them directly to their primitive LLVM types. A `Boolean` class value and a primitive `bool` generate identical LLVM IR — both become `i1`. There is no boxing, no heap allocation, and no indirection for primitive-backed wrapper types.

### Ownership-Aware Type System

Types interact with Uranite's ownership model. Move semantics are the default — assigning a value transfers ownership. The borrow checker enforces that references do not outlive the values they reference, and the type system tracks mutability through `PointerType` and `ReferenceType` mutability flags.

---

## The Type Kind Hierarchy

The `Type::Kind` enum defines 27 distinct type categories:

| Kind | Description | LLVM Representation |
|---|---|---|
| `Array` | Fixed or dynamic arrays | Heap pointer (`ptr`) |
| `Bool` | Boolean truth value | `i1` |
| `Callable` | Higher-order function reference | `ptr` (function pointer) |
| `Char` | Unicode scalar value | `i32` |
| `Class` | User-defined class | `ptr` (struct pointer) or primitive for OOP wrappers |
| `Enum` | Enumeration with variants | `i32` |
| `Error` | Sentinel for error recovery | (internal only) |
| `Float` | Floating-point number | `float` (32-bit) or `double` (64-bit) |
| `Function` | Function signature | `ptr` (function pointer) |
| `Future` | Async return wrapper | `i64` |
| `Generator` | Lazy yield sequence | `ptr` |
| `GenericParameter` | Unresolved type parameter | `ptr` (erased) |
| `Integer` | Signed/unsigned integer | `i8`, `i16`, `i32`, or `i64` |
| `Interface` | Interface contract | `ptr` |
| `Meta` | Type-as-value | (compile-time only) |
| `None` | Absence of value | `ptr null` |
| `Optional` | Nullable wrapper (`?T`) | `ptr` |
| `Pointer` | Raw pointer | `ptr` |
| `Reference` | Borrowed reference | `ptr` |
| `String` | Immutable byte sequence | `ptr` (null-terminated C string) |
| `Struct` | Value-type struct | `ptr` (struct pointer) |
| `Trait` | Trait with default methods | `ptr` |
| `Tuple` | Heterogeneous fixed-size collection | (composite) |
| `Union` | Multi-type alternative | (composite) |
| `Unresolved` | Not yet resolved | (internal only) |
| `Void` | No return value | `void` |

Each type kind has a corresponding struct in `typeref.hpp` that extends the base `Type` struct with kind-specific fields.

---

## The Type Registry

The type registry (`semantic::Registry`) is the central authority for type creation, storage, and lookup. It maintains two maps and a set of pre-created primitive type instances.

### Dual-Map Architecture

| Map | Purpose | Lookup Priority |
|---|---|---|
| `userTypesType` | User-defined types (classes, interfaces, enums, structs, traits, monomorphized generics) | Checked first |
| `primitivesTypes` | Built-in primitive types and OOP wrapper aliases | Checked second (fallback) |

The `lookupType(name)` method searches `userTypesType` first. If no match is found, it falls back to `primitivesTypes`. This ordering ensures that user-defined types can shadow primitive names (though this is strongly discouraged).

### Pre-Created Primitives

The registry constructor initializes all built-in primitive types:

| Field | Type Kind | Short Name | Qualified Name |
|---|---|---|---|
| `booleanType` | `Bool` | `bool` | `uranite.builtin.bool` |
| `charType` | `Char` | `char` | `uranite.builtin.char` |
| `stringType` | `String` | `str` | `uranite.builtin.str` |
| `voidType` | `Void` | `void` | `uranite.builtin.void` |
| `integer8Type` | `Integer` | `i8` | `uranite.builtin.i8` |
| `integer16Type` | `Integer` | `i16` | `uranite.builtin.i16` |
| `integer32Type` | `Integer` | `i32` | `uranite.builtin.i32` |
| `integer64Type` | `Integer` | `i64` | `uranite.builtin.i64` |
| `unsigned8Type` | `Integer` | `u8` | `uranite.builtin.u8` |
| `unsigned16Type` | `Integer` | `u16` | `uranite.builtin.u16` |
| `unsigned32Type` | `Integer` | `u32` | `uranite.builtin.u32` |
| `unsigned64Type` | `Integer` | `u64` | `uranite.builtin.u64` |
| `float32Type` | `Float` | `f32` | `uranite.builtin.f32` |
| `float64Type` | `Float` | `f64` | `uranite.builtin.f64` |
| `objectType` | `Class` | `Object` | `uranite.builtin.object` |
| `errorType` | `Error` | `Error` | (internal sentinel) |

The `None` type is a singleton created on first access via `getNone()`.

### Factory Methods

The registry provides factory methods for creating composite types:

| Method | Creates |
|---|---|
| `makeArray(element, size)` | `ArrayType` with element type and optional static size |
| `makeCallable(returnType, params)` | `CallableType` for higher-order function references |
| `makeFunction(params, returnType, isVariadic)` | `FunctionType` for function signatures |
| `makeFuture(inner)` | `FutureType` wrapping an async return value |
| `makeGenerator(yieldType)` | `GeneratorType` wrapping a yield sequence |
| `makeMeta(inner)` | `MetaType` for type-as-value |
| `makeOptional(inner)` | `OptionalType` wrapping a nullable value |
| `makePointer(inner, mutable)` | `PointerType` with mutability flag |
| `makeReference(inner, mutable)` | `ReferenceType` with mutability flag |
| `makeTuple(elements)` | `TupleType` with heterogeneous element types |
| `makeUnion(types)` | `UnionType` with multiple alternative types |

### Type Registration

| Method | Description |
|---|---|
| `registerType(name, type)` | Add a user-defined type to `userTypesType`. |
| `unregisterType(name)` | Remove a type from `userTypesType`. |
| `registerAlias(shortName, qualifiedName)` | Create an alias mapping short name to qualified name. |

---

## Type Resolution Pipeline

Types flow through a multi-stage pipeline from source code to LLVM IR.

### Stage 1: AST Type Nodes

The parser creates `TypeNode` AST nodes for every type annotation in the source code. Seven type node kinds exist:

| AST Node | Example Syntax | Description |
|---|---|---|
| `SimpleTypeNode` | `I64`, `String`, `Boolean` | Named type reference. |
| `GenericTypeNode` | `ArrayList<String>`, `HashMap<K,V>` | Generic type with type arguments. |
| `ArrayTypeNode` | `Memory<I64>` | Array type with element type. |
| `OptionalTypeNode` | `?String`, `String?` | Optional type wrapper. |
| `FunctionTypeNode` | `(I64, String) -> Boolean` | Function signature type. |
| `TupleTypeNode` | `(I64, String, Boolean)` | Tuple type with element types. |
| `CallableTypeNode` | `Callable<Boolean, I64, String>` | Higher-order callable type. |

### Stage 2: Semantic Type Resolution

The `Analyzer::resolveType()` method converts AST type nodes into semantic `Type` objects. The resolution logic dispatches on the node kind:

**SimpleType**: Look up the name in the type registry via `lookupType()`. If not found, check if the name is "Self" inside a class context (resolves to the current class type). If still not found, emit an "unknown type" error. Single-letter uppercase names get a hint suggesting they may be generic type parameters.

**GenericType**: Resolve the base type and all type arguments. Special cases: `Generator<T>` and `Future<T>` are handled directly via `makeGenerator()` and `makeFuture()`. For all other generics, construct a monomorphized type name (`ArrayList<String>`), check if it already exists in the registry, and if not, create a new monomorphized class by substituting type parameters.

**ArrayType**: Resolve the element type, then call `makeArray()`.

**OptionalType**: Resolve the inner type, then call `makeOptional()`.

**FunctionType**: Resolve all parameter types and the return type, then call `makeFunction()`.

**TupleType**: Resolve all element types, then call `makeTuple()`.

### Stage 3: Generic Monomorphization

When resolving a generic type like `HashMap<String, I64>`, the analyzer:

1. Looks up the base type (`HashMap`).
2. Resolves each type argument (`String`, `I64`).
3. Builds a monomorphized name: `"HashMap<String,I64>"`.
4. Checks if this monomorphized type already exists in the registry.
5. If not, creates a new `ClassType` by cloning the base class and substituting all `GenericParameter` occurrences with the concrete type arguments.

The substitution function recursively walks all type positions — fields, method parameters, return types, interface conformances — replacing `GenericParameter` types with their concrete substitutions. This handles nested generics (`ArrayList<HashMap<String, I64>>`), optional generics (`?T`), array generics, future/generator generics, and function type generics.

### Stage 4: Type Assignment

During expression analysis, the semantic analyzer assigns a resolved type to every expression node via `expression->semanticType = resolvedType`. This type flows into HIR and MIR lowering, where it guides instruction selection and variable allocation.

### Stage 5: LLVM Type Emission

The `MIRCodegen::toLLVMType()` method maps semantic types to LLVM types. The mapping is deterministic:

| Semantic Kind | LLVM Type |
|---|---|
| `Bool` | `i1` |
| `Char` | `i32` |
| `Float(32)` | `float` |
| `Float(64)` | `double` |
| `Integer(8)` | `i8` |
| `Integer(16)` | `i16` |
| `Integer(32)` | `i32` |
| `Integer(64)` | `i64` |
| `String` | `ptr` (opaque pointer) |
| `Void` | `void` |
| `None` | `ptr` (null pointer) |
| `Class` (OOP wrapper) | Mapped to underlying primitive (e.g., `Boolean` class becomes `i1`) |
| `Class` (user-defined) | `ptr` to named LLVM `StructType`, or `ptr` if no struct exists |
| `Interface` | `ptr` |
| `Enum` | `i32` |
| `Struct` | `ptr` to named LLVM `StructType` |
| `Function` / `Callable` | `ptr` (function pointer) |
| `Optional` | `ptr` |
| `Future` | `i64` |
| `Generator` | `ptr` |
| `GenericParameter` | `ptr` (type-erased) |
| `Pointer` / `Reference` | `ptr` |
| Default (unrecognized) | `i64` |

OOP wrapper classes receive special treatment. When `toLLVMType()` encounters a `Class` kind, it checks the qualified name against all known OOP wrapper types (`Int`, `I64`, `I32`, `I16`, `I8`, `Byte`, `UInt`, `U64`, `U32`, `U16`, `U8`, `Long`, `Integer`, `F32`, `F64`, `Float`, `Double`, `Char`, `Boolean`, `String`, `Void`, `Object`). If a match is found, the class is mapped to its primitive LLVM type — no pointer indirection. This is the mechanism behind zero-cost OOP wrappers.

---

## Type Identity

Type identity in Uranite is determined by **fully-qualified names**, not by short names. The `Type::equals()` method compares `qualified` fields when both are non-empty:

```
"uranite.language.string.String" == "uranite.language.string.String"    → True
"String" compared by name only                                         → fallback for primitives
```

The `qualnames.hpp` header centralizes all qualified name constants. All type comparisons throughout the compiler must use `->qualified` (the fully-qualified name), never `->name` (the short name). This prevents collisions between types with the same short name in different packages.

---

## Type Compatibility

The registry provides two key compatibility methods:

### isAssignable(target, source)

Determines whether a value of type `source` can be assigned to a variable of type `target`. Handles:

- Exact type match (qualified names equal).
- Error recovery passthrough (error type is assignable to anything).
- Void compatibility (primitive and OOP wrapper treated as equivalent).
- `None` to nullable (`None` is assignable to any `Optional<T>`).
- Numeric widening (smaller integer to larger integer, integer to float).
- `Object` target (accepts any type).
- Class inheritance chains (subclass assignable to superclass).
- Interface conformance (class implementing interface assignable to interface variable).
- Union member containment (any union member type assignable to the union).
- Monomorphized generic class matching.

### isComparable(x, y)

Determines whether two types can be compared using equality (`==`, `!=`) or relational (`<`, `>`, `<=`, `>=`) operators.

---

## Examples

### Primitive Type Declarations

```uranite
I64 count = 42
F64 ratio = 3.14
Boolean active = True
Char letter = 'A'
String name = "hello"
Void noReturn
```

### Composite Type Declarations

```uranite
Memory<I64> numbers = [1, 2, 3]
?String maybeName = None
ArrayList<String> names = new ArrayList<>()
HashMap<String, I64> scores = new HashMap<>()
```

### Generic Type Usage

```uranite
public function identity<T>( T value ) -> T:
    return value

public function swap<A, B>( A first, B second ) -> (B, A):
    return ( second, first )
```

### Type Checking in Practice

```uranite
I64 count = 42
F64 widened = count as F64

String name = "hello"
?String optional = name
?String absent = None

ArrayList<I64> numbers = new ArrayList<>()
numbers.add( 1 )
numbers.add( 2 )
```

The type system enforces that `count as F64` is a valid widening cast, that `name` is assignable to `?String` (wrapping in optional), that `None` is assignable to `?String`, and that `numbers.add()` accepts only `I64` arguments matching the type parameter.
