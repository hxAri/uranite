# Generic Types

Generic types enable parameterized type definitions where classes, structs, interfaces, enums, and functions accept type parameters that are substituted with concrete types at usage sites. Uranite implements generics through **monomorphization** — each distinct combination of type arguments produces a fully specialized copy of the generic definition, with all type parameters replaced by their concrete types at compile time. No generic type survives into the final executable; every instance is a concrete, specialized type.

---

## Syntax

### Generic Parameter Declaration

Generic parameters are declared with angle brackets after the name of a class, struct, interface, enum, or function:

```uranite
class Box<T>:
    public T value

    public function Box(self, T value) -> Void:
        self.value = value

    public function get(self) -> T:
        return self.value
```

Multiple type parameters are separated by commas:

```uranite
class Pair<K, V>:
    public K key
    public V value

    public function Pair(self, K key, V value) -> Void:
        self.key = key
        self.value = value
```

### Constrained Generic Parameters

Type parameters can carry interface or trait constraints using the colon syntax. Multiple constraints are joined with `+`:

```uranite
class SortedList<T: Comparable + Hashable>:
    public function add(self, T element) -> Void:
        pass
```

Constraints restrict which concrete types may substitute for the parameter. Only types that implement all specified interfaces satisfy the constraint.

### Default Type Arguments

A generic parameter may specify a default type using `=`:

```uranite
class Cache<K: Hashable = String, V = Object>:
    public function put(self, K key, V value) -> Void:
        pass
```

When a type argument is omitted at the usage site, the default fills in.

### Generic Type Usage

At usage sites, angle brackets supply concrete type arguments:

```uranite
Box<I64> intBox = new Box<I64>(42)
Pair<String, I64> entry = new Pair<String, I64>("age", 30)
SortedList<I64> numbers = new SortedList<I64>()
```

### Generic Functions

Functions declare their own type parameters independently from any enclosing class:

```uranite
public function identity<T>(T value) -> T:
    return value

public function swap<A, B>(Pair<A, B> pair) -> Pair<B, A>:
    return new Pair<B, A>(pair.value, pair.key)
```

### Generic Structs

Structs follow the same pattern as classes:

```uranite
struct Point<T>:
    public T x
    public T y

    public function magnitude(self) -> T:
        return self.x + self.y
```

### Generic Interfaces

Interfaces use type parameters to define type-polymorphic contracts:

```uranite
public interface Container<E>:
    public function add(self, E element) -> Void;
    public function get(self, I64 index) -> E;
    public function size(self) -> I64;
```

### Generic Enums

Enums can also carry generic parameters:

```uranite
public enum Result<T, E>:
    unit Ok T
    unit Err E
```

---

## Type System Representation

### GenericParameterType

During semantic analysis, each declared type parameter produces a `GenericParameterType` instance. This type inhabits the type registry while the generic definition body is being analyzed, acting as a placeholder that will later be substituted with a concrete type.

**Defined in**: `src/uranite/semantic/typeref.hpp:994-1008`

```cpp
struct GenericParameterType : Type {
    std::vector<TypeSharedPointer> constraints;

    GenericParameterType( const std::string& name )
        : Type( Type::Kind::GenericParameter, name ) {}
};
```

| Field | Type | Description |
|---|---|---|
| `constraints` | `vector<TypeSharedPointer>` | Interface or trait types this parameter must satisfy |
| `name` | `string` (inherited) | Identifier name of the parameter (e.g., "T", "K", "V") |
| `kind` | `Type::Kind` (inherited) | Always `Kind::GenericParameter` |

`GenericParameterType` is never a resolved type in the final program. It exists only during analysis of the generic definition body and during monomorphization. After substitution, every occurrence of a `GenericParameterType` is replaced with the concrete type argument.

### typeSubstitutions Map

Every monomorphized `ClassType`, `InterfaceType`, and `StructType` carries a `typeSubstitutions` map:

```cpp
std::unordered_map<std::string, TypeSharedPointer> typeSubstitutions;
```

This map records which concrete type each generic parameter was substituted with. For `Box<I64>`, the map contains `{"T" -> I64Type}`. The map is used during codegen to resolve element types for collection operations and during recursive substitution of nested generic types.

---

## AST Representation

### GenericParameterNode

Each declared type parameter in angle brackets produces a `GenericParameterNode`.

**Defined in**: `src/uranite/ast/node.hpp:786-811`

| Field | Type | Description |
|---|---|---|
| `name` | `string` | Parameter identifier (e.g., "T") |
| `constraints` | `vector<TypeNodeSharedPointer>` | Interface/trait constraints from `T: Constraint1 + Constraint2` |
| `defaultType` | `TypeNodeSharedPointer` | Default type from `T = DefaultType`, or null |

### GenericTypeNode

When a generic type is used with arguments (e.g., `Box<I64>`), the parser produces a `GenericTypeNode`.

**Defined in**: `src/uranite/ast/node.hpp:531-554`

| Field | Type | Description |
|---|---|---|
| `name` | `string` | Base type name (e.g., "Box", "HashMap") |
| `typeArguments` | `vector<TypeNodeSharedPointer>` | Supplied type arguments (e.g., `[I64]`, `[String, I64]`) |

---

## Parsing

### parseGenericParameters

Parses the declaration-side generic parameter list `<T: Constraint1 + Constraint2 = DefaultType, U>`.

**Defined in**: `src/uranite/parser/parser.cpp:1558-1583`

**Algorithm**:

1. Consume `<` (less-than token)
2. Loop until `>`:
   - Parse identifier as parameter name
   - If `:` follows, parse constraint types separated by `+`
   - If `=` follows, parse default type
   - Create `GenericParameterNode` with name, constraints, default
   - If `,` follows, consume and continue; otherwise expect `>`
3. Return vector of `GenericParameterNode`

**Grammar**:
```
GenericParameters  ::= "<" GenericParam ("," GenericParam)* ">"
GenericParam       ::= IDENTIFIER (":" Constraints)? ("=" Type)?
Constraints        ::= Type ("+" Type)*
```

### parseGenericArguments

Parses the usage-side type argument list `<Type1, Type2>`.

**Defined in**: `src/uranite/parser/parser.cpp:1543-1556`

**Algorithm**:

1. Consume `<`
2. Loop: parse each type node, separated by `,`
3. Consume `>`
4. Return vector of `TypeNodeSharedPointer`

**Grammar**:
```
GenericArguments   ::= "<" Type ("," Type)* ">"
```

---

## Semantic Analysis

### Generic Parameter Registration

When a class (or struct, interface, enum, function) with generic parameters enters semantic analysis, the analyzer registers each parameter as a `GenericParameterType` in the type registry. This makes parameter names available as valid types during body analysis.

**Defined in**: `src/uranite/semantic/analyzer.cpp:1636-1654`

**Algorithm**:

1. Clear existing `genericParameters` on the class type
2. Save current type registry bindings for parameter names (to restore after analysis)
3. For each `GenericParameterNode`:
   - Create `GenericParameterType` with the parameter name
   - For each constraint, resolve to a type:
     - If resolved type is not `Kind::Interface` and not `Kind::Trait`, emit warning: "generic constraint X is not an interface or trait"
     - Otherwise, push resolved constraint into `genericParameterType->constraints`
   - Push the `GenericParameterType` into `classType->genericParameters`
   - Register the parameter name in the type registry (so `T` resolves during body analysis)
4. After body analysis completes, restore saved type bindings

This save-and-restore pattern prevents generic parameter names from leaking into sibling or parent scopes. Within the body, `T` resolves to the `GenericParameterType` placeholder; outside the body, `T` reverts to whatever it was before (typically unregistered).

### Monomorphization Pipeline

When a `GenericTypeNode` (e.g., `Box<I64>`) is encountered during type resolution, the `resolveType` method triggers the monomorphization pipeline.

**Defined in**: `src/uranite/semantic/analyzer.cpp:4790-5150`

**Algorithm**:

1. **Special-case builtins**: `Generator<T>` and `Future<T>` are handled by dedicated factory methods (`makeGenerator`, `makeFuture`) and bypass general monomorphization

2. **Resolve base type**: Look up the unparameterized name (e.g., "Box") in the type registry. Fail if not found.

3. **Resolve type arguments**: Resolve each type argument node into a concrete semantic type

4. **Build monomorphized name**: Construct the specialized name by joining base name and type argument string representations:
   ```
   Box<I64>
   HashMap<String,I64>
   Pair<uranite.language.String,uranite.language.I64>  (qualified form)
   ```
   Both short and fully-qualified forms are built. The qualified form uses `->qualified` on each type argument for cross-module uniqueness.

5. **Check cache**: Look up the monomorphized name in the type registry. If already registered, return the cached type (this prevents duplicate monomorphization of the same specialization). For cached interface types, `populateInterfaceMethods` is called to ensure method tables are complete.

6. **Create substitution map**: Build `unordered_map<string, TypeSharedPointer>` mapping each generic parameter name to its corresponding type argument:
   ```
   {"T" -> I64Type}  for Box<I64>
   {"K" -> StringType, "V" -> I64Type}  for HashMap<String, I64>
   ```

7. **Validate constraints**: For each generic parameter with constraints:
   - Resolve each constraint to an interface type
   - Check whether the type argument implements that interface (by iterating `argClass->interfaces` and calling `iface->equals(constraintType)`)
   - If the type argument does not satisfy a constraint, emit error: `"type \"X\" does not satisfy constraint \"Y\" on generic parameter \"Z\""`

8. **Create monomorphized copy**: Depending on the base type kind:

   **For Class types** (`analyzer.cpp:5001-5049`):
   - Create new `ClassType` with monomorphized name
   - Copy `package`, set `qualified` to qualified monomorphized name
   - Preserve `astDeclaration` reference (shared with base type)
   - Substitute base class type through `substituteType`
   - Substitute each implemented interface through `substituteType`
   - Store `substitutionMap` as `typeSubstitutions`
   - For each field: copy `FieldInfo`, substitute field type
   - For each method: copy `MethodInfo`, substitute method type
   - Register in type registry under monomorphized name

   **For Interface types** (`analyzer.cpp:5051-5098`):
   - Same pattern as classes
   - Additionally copies `methodOrder` from base interface
   - Assigns sequential `interfaceTableIndex` values to methods
   - Calls `populateInterfaceMethods` on base before copying methods

   **For Struct types** (`analyzer.cpp:5100-5148`):
   - Same pattern as classes
   - Substitutes fields and methods with concrete types

9. **Register and return**: The monomorphized type is registered in the type registry and returned. Subsequent references to the same specialization will hit the cache at step 5.

### substituteType (Local Lambda)

The monomorphization pipeline defines a recursive local lambda `substituteType` that performs deep type substitution within the monomorphized copy. This lambda handles all type kinds that can contain generic parameters.

**Defined in**: `src/uranite/semantic/analyzer.cpp:4839-5000`

| Input Kind | Substitution Action |
|---|---|
| `GenericParameter` | Direct lookup in substitution map; return concrete type or pass through |
| `Optional` | Recurse into `inner` type |
| `Array` | Recurse into `elementType` |
| `Future` | Recurse into `innerType` |
| `Generator` | Recurse into `yieldType` |
| `Reference` | Recurse into `inner`, preserve `isMutable` |
| `Pointer` | Recurse into `inner`, preserve `isMutable` |
| `Function` | Recurse into each `parameterType` and `returnType` |
| `Callable` | Recurse into each `parameterType` and `returnType` |
| `Class/Interface/Struct` | Extract base name and existing substitutions, recursively substitute each generic argument, re-monomorphize with new arguments if any changed |

The Class/Interface/Struct case is the most complex. When a monomorphized type contains a field whose type is itself generic (e.g., `ArrayList<T>` inside a `HashMap<K, V>`), the lambda:
1. Extracts the base type name from the `astDeclaration`
2. Looks up the base type in the registry to get its generic parameters
3. For each generic parameter, finds the current substitution and recursively applies the outer substitution map
4. If any substitution changed, constructs a new monomorphized name and either returns a cached version or triggers a fresh monomorphization

This recursive re-monomorphization enables deeply nested generics like `HashMap<String, ArrayList<Pair<I64, I64>>>` to fully resolve.

### substituteGenericParameters (Member Function)

A separate member function provides the same recursive substitution logic for use outside the monomorphization pipeline (e.g., during call expression analysis).

**Defined in**: `src/uranite/semantic/analyzer.cpp:5357-5443`

The function follows the same pattern as the local lambda, handling all type kinds. For Class/Interface/Struct, it delegates to `monomorphizeGenericType` for re-monomorphization.

| Input Kind | Substitution Action |
|---|---|
| `GenericParameter` | Direct map lookup |
| `Optional` | Recurse into inner |
| `Array` | Recurse into element type |
| `Future` | Recurse into inner type |
| `Generator` | Recurse into yield type |
| `Reference` | Recurse into inner, preserve mutability |
| `Pointer` | Recurse into inner, preserve mutability |
| `Function` | Recurse into all parameter types and return type |
| `Class/Interface/Struct` | Extract base name, check existing substitutions, recurse on each generic argument, call `monomorphizeGenericType` if changed |

### monomorphizeGenericType (Convenience Wrapper)

**Defined in**: `src/uranite/semantic/analyzer.cpp:5239-5247`

Creates a synthetic `GenericTypeNode` from a base name and type argument list, then calls `resolveType` to trigger the full monomorphization pipeline:

```cpp
TypeSharedPointer Analyzer::monomorphizeGenericType(
    const std::string& baseName,
    const std::vector<TypeSharedPointer>& typeArgs,
    const SourceSharedPointer& source
) {
    std::vector<TypeNodeSharedPointer> argNodes;
    for( TypeSharedPointer typeArg : typeArgs ) {
        std::string argName = this->semanticTypeToRegistryName( typeArg );
        argNodes.push_back( std::make_shared<SimpleTypeNode>( argName, source ) );
    }
    TypeNodeSharedPointer genericNode =
        std::make_shared<GenericTypeNode>( baseName, std::move( argNodes ), source );
    return this->resolveType( genericNode );
}
```

This wrapper exists so that code outside the `resolveType` switch (such as `substituteGenericParameters`) can trigger monomorphization without manually constructing AST nodes.

---

## HIR Lowering

### HIRGenericParameterDescriptor

Generic parameters are preserved in HIR for validation and diagnostic purposes.

**Defined in**: `src/uranite/ir/hir.hpp:169-173`

```cpp
struct HIRGenericParameterDescriptor {
    std::string parameterName;
    std::vector<semantic::TypeSharedPointer> constraintTypes;
    semantic::TypeSharedPointer defaultType;
};
```

| Field | Type | Description |
|---|---|---|
| `parameterName` | `string` | Parameter identifier (e.g., "T") |
| `constraintTypes` | `vector<TypeSharedPointer>` | Resolved interface/trait constraints |
| `defaultType` | `TypeSharedPointer` | Resolved default type, or null |

### HIR Structs Carrying Generic Parameters

Five HIR definition structs carry a `genericParameters` vector:

| HIR Struct | Location |
|---|---|
| `HIRFunctionDefinition` | `hir.hpp:1061` |
| `HIRClassDefinition` | `hir.hpp:1096` |
| `HIRStructDefinition` | `hir.hpp:1121` |
| `HIREnumDefinition` | `hir.hpp:1154` |
| `HIRInterfaceDefinition` | `hir.hpp:1173` |

All five store `std::vector<HIRGenericParameterDescriptor> genericParameters`.

### lowerGenericParameter

**Defined in**: `src/uranite/ir/hir/lowering.cpp:1264-1275`

Converts an AST `GenericParameterNode` into an `HIRGenericParameterDescriptor`:

1. Copy parameter name
2. Resolve `defaultType` through `resolveTypeNode`
3. For each constraint, resolve through `resolveTypeNode` and push into `constraintTypes`

### Generic Parameter Scope in HIR Lowering

When lowering a function, class, struct, enum, or interface declaration, the HIR lowering pass performs the same save-and-restore pattern as semantic analysis:

**For functions** (`lowering.cpp:244-295`):
1. Save current type registry bindings for each generic parameter name
2. Register fresh `GenericParameterType` instances for each parameter
3. Lower return type, parameters, and body (all can reference the generic parameter names)
4. Lower generic parameters into `HIRGenericParameterDescriptor` entries
5. Restore saved type bindings

**For classes** (`lowering.cpp:298-340`):
1. Same save-and-restore pattern around the class body lowering
2. Generic parameters registered before field and method lowering
3. Lowered descriptors stored on `hirClass->genericParameters`

**For structs** (`lowering.cpp:409-411`) and **enums** (`lowering.cpp:441-443`):
Same pattern — generic parameter descriptors are lowered and stored.

---

## MIR and Code Generation

### Monomorphization Erases Generic Parameters

By the time code reaches MIR lowering and codegen, monomorphization has already occurred during semantic analysis. Every reference to a generic type like `Box<I64>` has been replaced with the concrete monomorphized `ClassType` named "Box<I64>" with fully substituted fields and methods. No `GenericParameterType` instances remain in the resolved types used by MIR.

### toLLVMType for GenericParameter

As a fallback, if a `GenericParameterType` somehow reaches code generation (e.g., in an unmonomorphized generic function body being compiled directly), the codegen maps it to an opaque pointer:

**Defined in**: `src/uranite/ir/mir/codegen.cpp:6670-6671`

```cpp
case semantic::Type::Kind::GenericParameter:
    return llvm::PointerType::getUnqual( this->llvmContext );
```

This produces LLVM's opaque pointer type (`ptr`), which acts as a type-erased representation. In practice, this path is only reached for unresolved generic parameters; all resolved uses go through their concrete LLVM type.

### typeSubstitutions in Codegen

During code generation, the `typeSubstitutions` map on monomorphized class types is consulted to determine element types for operations on generic containers. The codegen inspects `classType->typeSubstitutions` to extract the concrete element type:

**Defined in**: `src/uranite/ir/mir/codegen.cpp:6812-6821`

```cpp
if( classType != nullptr && classType->typeSubstitutions.empty() == false ) {
    for( const auto& substitution : classType->typeSubstitutions ) {
        if( substitution.second != nullptr ) {
            elementSemaType = substitution.second;
            elementClassName = substitution.second->name;
            break;
        }
    }
}
```

If `typeSubstitutions` is empty or unavailable, a fallback parses the monomorphized name string to extract the type argument between `<` and `>`:

```cpp
size_t openBracket = typeName.find( '<' );
size_t closeBracket = typeName.rfind( '>' );
if( openBracket != std::string::npos && closeBracket != std::string::npos ) {
    elementClassName = typeName.substr( openBracket + 1, closeBracket - openBracket - 1 );
}
```

This two-tier resolution ensures codegen can always determine element types even when the substitution map is not propagated.

---

## Constraint Validation

Constraints are validated at monomorphization time, not at the generic definition site. When `Box<MyType>` is instantiated and `Box<T: Comparable>` requires `Comparable`, the compiler checks whether `MyType` implements `Comparable`.

### Validation Algorithm

**Defined in**: `src/uranite/semantic/analyzer.cpp:5007-5030`

For each generic parameter with constraints:

1. Iterate the AST `genericParameters` of the base declaration
2. For each constraint node on the parameter:
   - Resolve constraint to a semantic type
   - Skip if not `Kind::Interface` (non-interface constraints are caught during registration)
   - Check if the type argument implements the interface:
     - If type argument is `Kind::Class`, iterate its `interfaces` vector
     - Call `iface->equals(constraintType)` for each implemented interface
   - If no matching interface found, emit error

**Error format**:
```
type "I64" does not satisfy constraint "Comparable" on generic parameter "T"
```

### Constraint Restrictions

Only interface and trait types are valid constraints. During generic parameter registration, if a constraint resolves to a non-interface, non-trait type, the compiler emits a warning:

```
generic constraint "I64" is not an interface or trait
```

The constraint is still recorded but will not be usable for satisfaction checking (since the `satisfiesConstraint` check requires `Kind::Interface`).

---

## Monomorphization Caching

The type registry serves as the monomorphization cache. Before creating a new monomorphized type, the pipeline checks:

```cpp
TypeSharedPointer existingType = this->typeRegistry.lookupType( monomorphizedName );
if( existingType ) {
    return existingType;
}
```

The monomorphized name is the cache key — `"Box<I64>"`, `"HashMap<String,I64>"`, etc. This means:

- **Same specialization used in multiple files**: Only one monomorphized type is created. The first usage triggers monomorphization; subsequent usages return the cached result.
- **Order-independent**: Whether `Box<I64>` is first used in a function signature, a field declaration, or a local variable, the result is identical.
- **Qualified-name variant**: The qualified monomorphized name (e.g., `"uranite.examples.Box<uranite.language.I64>"`) is stored on the type's `qualified` field for cross-module identity.

---

## Recursive and Nested Generics

### Nested Generic Types

Monomorphization handles arbitrary nesting depths. A type like `HashMap<String, ArrayList<Pair<I64, Bool>>>` triggers a chain of monomorphizations:

1. `Pair<I64, Bool>` is monomorphized first (innermost)
2. `ArrayList<Pair<I64,Bool>>` is monomorphized, with its element type being the already-cached `Pair<I64,Bool>`
3. `HashMap<String, ArrayList<Pair<I64,Bool>>>` is monomorphized, with its value type being the cached `ArrayList<Pair<I64,Bool>>`

Each level uses `substituteType` recursively. When `substituteType` encounters a Class/Interface/Struct with generic parameters, it re-monomorphizes with the substituted arguments, potentially triggering deeper monomorphization.

### Self-Referential Generic Types

A class can reference its own generic type in fields:

```uranite
class TreeNode<T>:
    public T value
    public TreeNode<T>? left
    public TreeNode<T>? right
```

This works because `TreeNode<T>` inside the body, during monomorphization of e.g. `TreeNode<I64>`, resolves `T` to `I64`, producing `TreeNode<I64>?` — which hits the cache since the type is already being registered. The registration happens before field substitution, so the cache lookup succeeds.

---

## Practical Examples

### Generic Container Class

```uranite
package examples.generics

from uranite.io.console import puts

class Box<T>:

    public T value

    public function Box(self, T value) -> Void:
        self.value = value

    public function get(self) -> T:
        return self.value

public function main() -> I32:
    Box<I64> intBox = new Box<I64>(42)
    puts("Box value = ", intBox.get())

    Box<String> strBox = new Box<String>("hello")
    puts("Box value = ", strBox.get())
    return 0
```

**Compilation trace**:
- Parser produces `ClassDeclaration` with one `GenericParameterNode` named "T"
- Semantic analysis registers `GenericParameterType("T")` during class body analysis
- At `new Box<I64>(42)`, parser produces `GenericTypeNode("Box", [SimpleTypeNode("I64")])`
- `resolveType` triggers monomorphization:
  - Resolves base type "Box" to the generic `ClassType`
  - Resolves type argument to `I64`
  - Builds name "Box<I64>"
  - Checks cache — not found
  - Creates `ClassType("Box<I64>")` with substitution map `{"T" -> I64}`
  - Substitutes field type: `T` becomes `I64`
  - Substitutes method return/parameter types
  - Registers "Box<I64>" in type registry
- At `new Box<String>(...)`, same pipeline produces "Box<String>" — a separate monomorphized type
- HIR lowering preserves `HIRGenericParameterDescriptor{parameterName: "T"}` on the class definition
- MIR codegen generates two distinct class layouts: one with `I64` field, one with `String` (pointer) field

### Constrained Generic Function

```uranite
package examples.generics

from uranite.io.console import puts

public function findMax<T: Comparable>(T first, T second) -> T:
    if first.greaterThan(second):
        return first
    return second

public function main() -> I32:
    I64 result = findMax<I64>(10, 20)
    puts("Max: ", result)
    return 0
```

**Compilation trace**:
- `GenericParameterNode` for "T" carries one constraint: `SimpleTypeNode("Comparable")`
- During registration, constraint resolves to `InterfaceType("Comparable")` — valid, no warning
- At call site `findMax<I64>(10, 20)`:
  - Type argument `I64` is checked against constraint `Comparable`
  - `I64` OOP wrapper class implements `Comparable` interface
  - Constraint satisfied — monomorphization proceeds
  - Function type specialized with `T -> I64` throughout

### Generic Interface with Implementation

```uranite
package examples.generics

public interface Container<E>:
    public function add(self, E element) -> Void;
    public function get(self, I64 index) -> E;
    public function size(self) -> I64;

class SimpleList<E> implements Container<E>:

    public I64 count

    public function SimpleList(self) -> Void:
        self.count = 0

    public override function add(self, E element) -> Void:
        self.count = self.count + 1

    public override function get(self, I64 index) -> E:
        pass

    public override function size(self) -> I64:
        return self.count
```

**Compilation trace**:
- `Container<E>` is a generic interface with one parameter
- `SimpleList<E> implements Container<E>` — the `Container<E>` usage triggers monomorphization with the same generic parameter (during `SimpleList` analysis, `E` is a `GenericParameterType`)
- When `SimpleList<String>` is instantiated:
  - `SimpleList` is monomorphized with `{"E" -> String}`
  - `Container<E>` in the implements clause is re-monomorphized via `substituteType`: `E` resolves to `String`, producing `Container<String>`
  - Both `SimpleList<String>` and `Container<String>` are registered in the type registry
  - All method signatures (`add(self, E element)`) have `E` replaced with `String`

---

## Compilation Pipeline Summary

| Stage | Generic Type Handling |
|---|---|
| **Lexer** | No special tokens — `<` and `>` are standard comparison operators, disambiguated by parser context |
| **Parser** | `parseGenericParameters` extracts `<T: Constraint = Default>` into `GenericParameterNode` list; `parseGenericArguments` extracts `<Type1, Type2>` into type argument list |
| **Semantic Analysis** | Registers `GenericParameterType` placeholders during definition analysis; triggers monomorphization pipeline when `GenericTypeNode` is resolved; validates constraints; creates and caches specialized copies |
| **HIR Lowering** | Saves/restores type registry around generic scopes; lowers parameters to `HIRGenericParameterDescriptor`; preserves generic parameter info on all five definition types |
| **HIR Validation** | Validates type consistency within monomorphized bodies |
| **MIR Lowering** | Operates on already-monomorphized types — no generic-specific handling needed |
| **MIR Codegen** | Maps unresolved `GenericParameter` to opaque pointer as fallback; consults `typeSubstitutions` map for element type resolution in container operations |
| **LLVM IR** | No generics survive — every type is concrete. `Box<I64>` compiles to a struct with an `i64` field; `Box<String>` compiles to a struct with a pointer field |

---

## Design Rationale

### Why Monomorphization

Uranite uses monomorphization (like C++ templates and Rust generics) rather than type erasure (like Java generics) or boxing (like Go pre-1.18). This choice:

- **Eliminates runtime overhead**: No vtable dispatch, no boxing/unboxing, no type metadata checks. `Box<I64>.get()` returns a raw `i64`, not a heap-allocated object.
- **Enables type-specific optimizations**: LLVM sees concrete types and can optimize each specialization independently.
- **Preserves type safety**: Every field, parameter, and return type is fully resolved. No unsafe casts.
- **Trade-off — code size**: Each distinct specialization produces a separate copy of the code. `Box<I64>` and `Box<String>` generate two complete class implementations. For heavily parameterized libraries with many type combinations, this increases binary size.

### Why Constraints Use Interfaces Only

Constraints are restricted to interfaces and traits because they define behavioral contracts — method signatures that the compiler can verify. A constraint `T: Comparable` guarantees that `T` has `lessThan`, `greaterThan`, and related methods. Primitive types and classes cannot serve as constraints because they define implementation, not contracts.

### Why Cache by Monomorphized Name

Using the string name `"Box<I64>"` as the cache key is simple and effective. The name uniquely identifies the specialization because type arguments are serialized using their `toString()` representation. Qualified names (`"uranite.examples.Box<uranite.language.I64>"`) handle cross-module identity. The string-based approach avoids needing structural type comparison for cache lookups.
