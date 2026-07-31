# Callable Types

Callable types represent first-class function references — values that can be stored in variables, passed as arguments, returned from functions, and invoked at call sites. Uranite distinguishes between two related but separate type kinds: `FunctionType` (the concrete signature of a declared or lambda function) and `CallableType` (the user-facing type annotation for function-typed values). Both compile to opaque pointers in LLVM IR and are cross-assignable.

---

## Syntax

### Callable Type Annotation

The `Callable` type uses a nested angle-bracket syntax:

```
Callable<ReturnType, <ParamType1, ParamType2, ...>>
```

The outer angle brackets enclose the return type and a comma-separated inner angle-bracket group containing parameter types:

```uranite
Callable<I64, <I64>> doubler = lambda I64 x: x * 2

Callable<I64, <I64, I64>> adder = lambda I64 a, I64 b: a + b

Callable<Void, <String>> printer = lambda String message: puts(message)

Callable<I64, <>> supplier = lambda: 42
```

The inner `<>` is always required, even when no parameters exist (`Callable<I64, <>>`).

### Higher-Order Functions

Callable types appear in function parameter and return positions to express higher-order function signatures:

```uranite
function apply(Callable<I64, <I64>> operation, I64 value) -> I64:
    return operation(value)

function applyBinary(Callable<I64, <I64, I64>> operation, I64 a, I64 b) -> I64:
    return operation(a, b)
```

### Callable Invocation

A value of callable type is invoked with standard call syntax — parentheses with arguments:

```uranite
Callable<I64, <I64>> doubler = lambda I64 x: x * 2
I64 result = doubler(21)
```

---

## Type System Representation

### CallableType

The semantic representation of a callable type annotation.

**Defined in**: `src/uranite/semantic/typeref.hpp:1038-1067`

```cpp
struct CallableType : Type {
    std::vector<TypeSharedPointer> parameterTypes;
    TypeSharedPointer returnType;

    CallableType(
        TypeSharedPointer returnType,
        std::vector<TypeSharedPointer> parameterTypes
    ) : Type( Type::Kind::Callable, "Callable" ),
        parameterTypes( std::move( parameterTypes ) ),
        returnType( std::move( returnType ) ) {
        std::string parameters;
        for( size_t index=0; index<this->parameterTypes.size(); index++ ) {
            if( index > 0 ) {
                parameters+= ",";
            }
            parameters+= this->parameterTypes[index]->toString();
        }
        this->name = fmt::format( "Callable<{},<{}>>",
            this->returnType->toString(), parameters );
    }
};
```

| Field | Type | Description |
|---|---|---|
| `parameterTypes` | `vector<TypeSharedPointer>` | Types of parameters the callable accepts |
| `returnType` | `TypeSharedPointer` | Type of value returned when invoked |
| `name` | `string` (computed) | Formatted as `"Callable<ReturnType,<Param1,Param2>>"` |
| `kind` | `Type::Kind` (inherited) | Always `Kind::Callable` |

### FunctionType (Contrast)

`FunctionType` is the internal representation produced by function declarations and lambda expressions. It carries richer metadata than `CallableType`.

**Defined in**: `src/uranite/semantic/typeref.hpp:334-411`

| Field | Type | Description |
|---|---|---|
| `parameterTypes` | `vector<TypeSharedPointer>` | Types of all parameters |
| `returnType` | `TypeSharedPointer` | Return type |
| `isVariadic` | `bool` | Whether function accepts variable arguments (extern `...` only) |
| `variadicParameterIndex` | `int` | Index of the `Type name[]` parameter, or -1 |
| `keywordParameterIndex` | `int` | Index of the `Type name{}` parameter, or -1 |
| `variadicElementType` | `TypeSharedPointer` | Element type T of the variadic `Args<T>` parameter |
| `keywordValueType` | `TypeSharedPointer` | Value type T of the keyword `Kwargs<T>` parameter |
| `keywordOnlyParamNames` | `vector<string>` | Names of keyword-only parameters |
| `keywordOnlyParamTypes` | `vector<TypeSharedPointer>` | Types of keyword-only parameters |
| `genericParameterNames` | `vector<string>` | Names of generic type parameters |
| `parameterNames` | `vector<string>` | Parameter names for snippet generation |
| `exceptionTypes` | `vector<TypeSharedPointer>` | Types declared in `raises` clause |

`FunctionType` carries variadic, keyword, generic, and exception metadata. `CallableType` is deliberately simpler — it captures only what matters for invocation: parameter types and return type. This separation reflects their distinct roles: `FunctionType` represents a concrete declaration with full semantic context; `CallableType` represents a type-level contract for "anything invocable with this signature."

### String Representations

| Type | Format | Example |
|---|---|---|
| `CallableType` | `Callable<Return,<Params>>` | `Callable<I64,<I64,I64>>` |
| `FunctionType` | `function( Params ) -> Return` | `function( I64,I64 ) -> I64` |

---

## AST Representation

### CallableTypeNode

Produced when the parser encounters `Callable<Return, <Params>>` in a type annotation.

**Defined in**: `src/uranite/ast/node.hpp:475-498`

```cpp
struct CallableTypeNode : TypeNode {
    std::vector<TypeNodeSharedPointer> parameterTypes;
    TypeNodeSharedPointer returnType;

    CallableTypeNode(
        TypeNodeSharedPointer returnType,
        std::vector<TypeNodeSharedPointer> parameters,
        const lookup::SourceSharedPointer& source
    ) : TypeNode( Node::Kind::CallableType, source ),
        parameterTypes( std::move( parameters ) ),
        returnType( std::move( returnType ) ) {}
};
```

| Field | Type | Description |
|---|---|---|
| `parameterTypes` | `vector<TypeNodeSharedPointer>` | Parameter type nodes from inner `<...>` |
| `returnType` | `TypeNodeSharedPointer` | Return type node (first type argument) |

### FunctionTypeNode

Produced when the parser encounters an anonymous function type signature (less common in user code; used internally).

**Defined in**: `src/uranite/ast/node.hpp:503-526`

```cpp
struct FunctionTypeNode : TypeNode {
    std::vector<TypeNodeSharedPointer> parameterTypes;
    TypeNodeSharedPointer returnType;

    FunctionTypeNode(
        std::vector<TypeNodeSharedPointer> parameters,
        TypeNodeSharedPointer returnType,
        const lookup::SourceSharedPointer& source
    ) : TypeNode( Node::Kind::FunctionType, source ),
        parameterTypes( std::move( parameters ) ),
        returnType( std::move( returnType ) ) {}
};
```

The key structural difference: `CallableTypeNode` takes return type first (matching `Callable<Return, <Params>>` syntax), while `FunctionTypeNode` takes parameters first (matching internal `function(Params) -> Return` convention).

---

## Parsing

### Callable Type Parsing

When the parser encounters the identifier "Callable" followed by `<`, it triggers callable-specific parsing rather than treating it as a generic type.

**Defined in**: `src/uranite/parser/parser.cpp:545-559`

**Algorithm**:

1. Match identifier "Callable" followed by `<`
2. Consume `<`
3. Parse return type node
4. Expect `,` separator
5. Expect `<` opening inner parameter group
6. If next token is not `>` (non-empty parameter list):
   - Parse first parameter type
   - While `,` follows, parse additional parameter types
7. Expect `>` closing inner parameter group
8. Expect `>` closing outer Callable type
9. Return `CallableTypeNode(returnType, parameterTypes)`

**Grammar**:
```
CallableType  ::= "Callable" "<" Type "," "<" ParameterList? ">" ">"
ParameterList ::= Type ("," Type)*
```

The parser uses the constant `semantic::qualname::identifier::Callable` (value: "Callable") for recognition, defined in `src/uranite/semantic/qualnames.hpp:31`.

**Edge case — `>>`**: When inner parameter types include a generic type (e.g., `Callable<Void, <ArrayList<I64>>>`), the closing sequence produces `>>`. The parser handles this via `check({ token::Type::GreaterThan, token::Type::ShiftRight }, false)` which prevents the lexer's `>>` token from consuming both closing brackets prematurely.

---

## Semantic Analysis

### Type Resolution

There is no explicit `case ast::Node::Kind::CallableType` in `resolveType`. Instead, the parser produces a `CallableTypeNode`, and type resolution is handled through the standard name-based lookup path. The `Callable` identifier is recognized during parsing and produces a `CallableTypeNode` directly, bypassing the generic type resolution path.

The `CallableType` is constructed via the registry factory method:

**Defined in**: `src/uranite/semantic/typeref.cpp:612-613`

```cpp
TypeSharedPointer Registry::makeCallable(
    TypeSharedPointer returnType,
    std::vector<TypeSharedPointer> parameters
) {
    return std::make_shared<CallableType>(
        std::move( returnType ), std::move( parameters ) );
}
```

### Lambda Expression Type

Lambda expressions produce a `FunctionType`, not a `CallableType`:

**Defined in**: `src/uranite/semantic/analyzer.cpp:2344-2357`

When a lambda expression is analyzed:

1. Each parameter type is resolved
2. Return type is resolved from annotation, or inferred from body
3. If no return type can be inferred, defaults to `Void`
4. Creates `FunctionType(parameterTypes, returnType)` via `makeFunction`

This means `Callable<I64, <I64>> doubler = lambda I64 x: x * 2` involves an assignment where the right-hand side has type `FunctionType` and the left-hand side expects `CallableType`. This succeeds because `FunctionType` is assignable to `CallableType` (see assignability rules below).

### Call Expression Analysis

When a call expression targets a value of `CallableType`, the analyzer performs argument type checking:

**Defined in**: `src/uranite/semantic/analyzer.cpp:1276-1288`

**Algorithm**:

1. Analyze the callee expression to obtain its type
2. If callee type is `Kind::Callable`:
   - Cast to `CallableTypeSharedPointer`
   - For each argument, analyze the argument expression
   - Check that argument type is assignable to the corresponding parameter type
   - If mismatch, emit error: `"argument type mismatch: expected \"X\" but found \"Y\""`
   - Return `callableType->returnType` as the expression result type
3. If callee type is `Kind::Function`: handled by separate function-call logic with overload resolution
4. Otherwise: error (not callable)

Callable call analysis is simpler than function call analysis because callable types carry no overload information, no variadic/keyword parameters, and no generic parameters. The callee is always a single resolved callable value.

---

## Assignability Rules

The type registry implements bidirectional assignability between `CallableType` and `FunctionType`, plus `CallableType`-to-`CallableType` compatibility.

**Defined in**: `src/uranite/semantic/typeref.cpp:498-536`

### Callable to Function (target: Callable, source: Function)

A `FunctionType` is assignable to a `CallableType` when:

1. Parameter count matches exactly
2. Each parameter type is pairwise assignable (target parameter assignable from source parameter)
3. Return type is assignable (target return assignable from source return)

This rule enables assigning lambda expressions and function references to callable-typed variables.

### Function to Callable (target: Function, source: Callable)

A `CallableType` is assignable to a `FunctionType` when the same three conditions hold in the reverse direction. This enables passing callable values where function types are expected.

### Callable to Callable (target: Callable, source: Callable)

Two `CallableType` values are assignable when:

1. Parameter count matches
2. Each parameter type is pairwise assignable
3. Return type is assignable

### Invariant Parameter Checking

Parameter assignability is checked from **target to source** — `isAssignable(targetParam, sourceParam)`. This means parameter types follow covariant checking in the current implementation. Both parameter types and return types use the same `isAssignable` direction.

---

## Generic Substitution

When `CallableType` appears within a generic type that is being monomorphized, the substitution pipeline handles it:

**Defined in**: `src/uranite/semantic/analyzer.cpp:4879-4885`

Within the `substituteType` lambda during monomorphization:

```cpp
if( targetType->kind == Type::Kind::Callable ) {
    CallableTypeSharedPointer callableType =
        std::static_pointer_cast<CallableType>( targetType );
    std::vector<TypeSharedPointer> substitutedParameters;
    for( TypeSharedPointer& parameter : callableType->parameterTypes ) {
        substitutedParameters.push_back(
            substituteType( parameter, substitutionMap ) );
    }
    return this->typeRegistry.makeCallable(
        substituteType( callableType->returnType, substitutionMap ),
        substitutedParameters );
}
```

This enables generic types containing callable fields or method parameters to properly substitute type parameters. For example, a class `EventBus<T>` with a field `Callable<Void, <T>>` will have the callable parameter type substituted when `EventBus<String>` is monomorphized, producing `Callable<Void, <String>>`.

---

## OOP Wrapper

The `Callable` class in the standard library serves as the OOP wrapper for callable types.

**Defined in**: `stdlibs/language/callable.urn`

```uranite
final class Callable:

    """
    First-class function type for function pointers and closures.
    Represents an invocable entity that can be called at runtime.
    """

    public function call(self) -> Void:
        """ Invoke this callable, executing the underlying function or closure. """
```

The OOP wrapper is minimal — a `final class` with a `call` method. The actual callable behavior (invocation, argument passing, closure capture) is handled entirely by the compiler through function pointer mechanics, not through the OOP wrapper. The class exists to provide a type identity in the `uranite.language` package hierarchy and to enable method-based invocation patterns.

---

## Code Generation

### LLVM Type Mapping

Both `CallableType` and `FunctionType` map to the same LLVM type — an opaque pointer:

**Defined in**: `src/uranite/ir/mir/codegen.cpp:6661-6663`

```cpp
case semantic::Type::Kind::Function:
case semantic::Type::Kind::Callable:
    return llvm::PointerType::getUnqual( this->llvmContext );
```

This produces LLVM's opaque pointer type (`ptr`). Functions are represented as pointers to their code, and callables are stored as the same pointer type. No boxing or wrapping occurs.

### Function Reference Assignment

When a named function is assigned to a callable variable, the codegen converts the function pointer to an integer representation:

**Defined in**: `src/uranite/ir/mir/codegen.cpp:1610-1614`

```cpp
if( referencedFunction != nullptr ) {
    llvm::Value* functionPointer = this->irBuilder.CreatePtrToInt(
        referencedFunction,
        llvm::Type::getInt64Ty( this->llvmContext ),
        "fn.addr"
    );
    this->setVariableValue( instruction.destinationVariable, functionPointer );
}
```

The function pointer is stored as an `i64` value (pointer-to-integer conversion), which allows uniform storage in the variable table alongside other integer-sized values.

### Indirect Call (Callable Invocation)

When a callable value is invoked (the callee is a variable rather than a named function), the codegen falls through to the indirect call path:

**Defined in**: `src/uranite/ir/mir/codegen.cpp:4519-4551`

**Algorithm**:

1. Named function lookup fails (callee is a variable, not a declared function name)
2. Search the variable descriptor table for a variable matching the called name
3. Load the variable value (the stored function pointer)
4. Build LLVM `FunctionType` from argument types and return type:
   - Collect each argument value and its LLVM type
   - Determine return type from `instruction.operandType` (or default to `i64`)
5. If the loaded value is not already a pointer, convert with `IntToPtr`:
   ```cpp
   calleePointer = this->irBuilder.CreateIntToPtr(
       calleePointer,
       llvm::PointerType::getUnqual( this->llvmContext ),
       "fn.ptr"
   );
   ```
6. Emit `CreateCall` with the indirect function type and pointer:
   ```cpp
   llvm::Value* callResult = this->irBuilder.CreateCall(
       indirectCallType, calleePointer, arguments );
   ```
7. Store result in destination variable

This path handles both `Callable`-typed variables and any other variable holding a function pointer. The indirect call mechanism is type-agnostic at the LLVM level — the function type is reconstructed from argument types at the call site.

### LLVM IR Example

For the code:

```uranite
Callable<I64, <I64>> doubler = lambda I64 x: x * 2
I64 result = doubler(21)
```

The generated LLVM IR (simplified) looks like:

```llvm
@__lambda_0 = internal global ptr null

define i64 @"lambda.0"(i64 %x) {
    %mul = mul i64 %x, 2
    ret i64 %mul
}

; In main:
%fn.addr = ptrtoint ptr @"lambda.0" to i64
store i64 %fn.addr, ptr %doubler
%loaded = load i64, ptr %doubler
%fn.ptr = inttoptr i64 %loaded to ptr
%result = call i64 %fn.ptr(i64 21)
```

The lambda compiles to a standalone function. The callable variable stores the function address as an integer. Invocation converts back to a pointer and emits an indirect call.

---

## Callable vs Function Type: When to Use Each

| Scenario | Type Used | Reason |
|---|---|---|
| **Variable holding a lambda** | `Callable<R, <Params>>` | Explicit user-facing type annotation |
| **Function parameter accepting a callback** | `Callable<R, <Params>>` | Expresses "any function matching this signature" |
| **Function declaration analysis** | `FunctionType` | Carries full metadata (variadic, keyword, generic, exceptions) |
| **Lambda expression result** | `FunctionType` | Internal representation; assignable to `Callable` |
| **Overload resolution** | `FunctionType` | Only `FunctionType` participates in overload scoring |
| **Type annotation in stdlib** | `Callable<R, <Params>>` | Used in `Sequence.map`, `Sequence.forEach`, `SimpleTask`, etc. |

In practice, `Callable` is the user-facing type for function-typed values, and `FunctionType` is the compiler-internal representation. The assignability rules bridge between them seamlessly.

---

## Standard Library Usage

Callable types appear throughout the standard library for callback-accepting APIs:

**Collection operations** (`stdlibs/collection/sequence.urn`):
```uranite
public function sorted(self, Callable<Int, <E, E>> comparator) -> Sequence<E>
public function map(self, Callable<E, <E>> transform) -> Sequence<E>
public function forEach(self, Callable<Void, <E>> action) -> Void
```

**Coroutine tasks** (`stdlibs/coroutine/task.urn`):
```uranite
public function SimpleTask(self, I64 taskId, Callable<I64, <>> entryFn) -> Void
```

These demonstrate the pattern: `Callable` parameterized with generic type parameters from the enclosing class, allowing type-safe callbacks that respect container element types.

---

## Practical Examples

### Lambda Assignment and Invocation

```uranite
package examples.callable

from uranite.io.console import puts

public function main() -> I32:
    Callable<I64, <I64>> doubler = lambda I64 x: x * 2
    I64 result = doubler(21)
    puts("doubler(21) = ", result)
    return 0
```

**Compilation trace**:
- Parser sees `Callable<I64, <I64>>` and produces `CallableTypeNode(returnType=I64, params=[I64])`
- Lambda `lambda I64 x: x * 2` analyzed, produces `FunctionType([I64], I64)`
- Assignment check: `isAssignable(CallableType, FunctionType)` — parameter count matches (1), `I64` assignable to `I64`, return `I64` assignable to `I64` — passes
- Codegen: lambda compiled as standalone function, pointer stored in `doubler` variable
- Invocation: indirect call via `IntToPtr` + `CreateCall`

### Higher-Order Functions

```uranite
package examples.callable

from uranite.io.console import puts

function apply(Callable<I64, <I64>> operation, I64 value) -> I64:
    return operation(value)

function applyBinary(Callable<I64, <I64, I64>> operation, I64 a, I64 b) -> I64:
    return operation(a, b)

public function main() -> I32:
    I64 squared = apply(lambda I64 x: x * x, 5)
    puts("apply(square, 5) = ", squared)

    I64 sum = applyBinary(lambda I64 a, I64 b: a + b, 10, 20)
    puts("applyBinary(add, 10, 20) = ", sum)
    return 0
```

**Compilation trace**:
- `apply` parameter `operation` has type `Callable<I64, <I64>>`
- At call site `apply(lambda I64 x: x * x, 5)`:
  - Lambda produces `FunctionType([I64], I64)`
  - Argument check: `FunctionType` assignable to `Callable<I64, <I64>>` — passes
  - Lambda compiled as standalone function, pointer passed to `apply`
- Inside `apply`, `operation(value)` triggers callable invocation path — indirect call

### Closure Capture with Callable

```uranite
package examples.callable

from uranite.io.console import puts

public function main() -> I32:
    I64 factor = 3
    Callable<I64, <I64>> multiplier = lambda I64 x: x * factor
    puts("multiplier(7) = ", multiplier(7))
    return 0
```

**Compilation trace**:
- Lambda captures `factor` from enclosing scope
- Compiler generates a closure: a global variable stores the captured `factor` value, and a wrapper function reads it
- The wrapper function pointer is stored in the `multiplier` callable variable
- Invocation emits indirect call to the wrapper, which accesses the captured value

---

## Compilation Pipeline Summary

| Stage | Callable Type Handling |
|---|---|
| **Lexer** | No special tokens — "Callable" is a standard identifier |
| **Parser** | Recognizes "Callable" + `<` pattern, parses `Callable<Return, <Params>>` into `CallableTypeNode` with return type and parameter types |
| **Semantic Analysis** | `CallableType` created via `makeCallable` factory; callable invocations type-checked against parameter types; `FunctionType` assignable to `CallableType` bidirectionally |
| **HIR Lowering** | No callable-specific handling — standard type propagation |
| **MIR Lowering** | No callable-specific handling — uses standard `CallFunction` instruction |
| **MIR Codegen** | Maps to `PointerType::getUnqual` (opaque pointer); function references stored via `PtrToInt`; invocations use indirect call path (`IntToPtr` + `CreateCall`) |
| **LLVM IR** | Function pointer stored as `i64`, converted to `ptr` for indirect calls; no boxing or vtable involved |

---

## Design Rationale

### Why Two Separate Types

`CallableType` and `FunctionType` serve different purposes:

- **`FunctionType`** is the compiler's internal representation of a concrete function. It carries variadic parameters, keyword parameters, generic parameters, parameter names, exception declarations — all the metadata needed for overload resolution, error diagnostics, and code generation of the function itself.

- **`CallableType`** is the user-facing type annotation for "a function matching this signature." It deliberately strips away metadata irrelevant to invocation — a caller does not care whether the underlying function is variadic, has keyword parameters, or declares exceptions. The caller only needs to know what types to pass and what type comes back.

This separation means `Callable<I64, <I64>>` can hold a plain function, a lambda, a closure, or a method reference — any callable matching the signature — without the type system needing to track the underlying function's full metadata.

### Why Opaque Pointers

Both types compile to LLVM opaque pointers because functions in LLVM are inherently pointer-typed values. No wrapping, boxing, or indirection layer exists. A callable variable is a raw function pointer — the most efficient possible representation for first-class functions. The `PtrToInt`/`IntToPtr` round-trip exists because the variable storage system uses integer-sized slots uniformly, and the conversion is zero-cost on all architectures where pointer and integer sizes match (which includes all supported targets).
