# Type Inference

Uranite uses local type inference to deduce types from context when explicit type annotations are omitted. The compiler does not perform global or Hindley-Milner-style inference — all inference is flow-local, operating within a single expression or statement. Variable declarations without type annotations infer their type from the initializer expression. Lambda return types are inferred from the first return statement body. For-in loop variables are inferred from the iterable element type. Collection literal element types are inferred from the first non-error element. Match expression result types are inferred from the first arm value. Generic type arguments on function calls are *not* inferred — the compiler requires explicit type arguments for generic functions. This design keeps inference predictable and error messages precise: every inferred type can be traced to a single local source.

This document covers variable type inference (initializer-driven typing with fallback to error type), literal expression typing (integer, float, string, char, boolean, none literals and their OOP wrapper class lookups), lambda return type inference (first return statement analysis), for-in loop variable inference (range, array, string, generator, and iterator interface protocol resolution via `hasIteratorInterface`), pair destructuring inference (two-variable for-in with `Pair<K,V>` field extraction), collection literal element inference (array, tuple, set expressions using first element monomorphization), comprehension result type inference (list, set, map comprehension body type propagation), match expression result type inference (first arm value typing), and the explicit type argument requirement for generic functions.

---

## Table of Contents

- [Overview](#overview)
- [Variable Type Inference](#variable-type-inference)
  - [Initializer-Driven Inference](#initializer-driven-inference)
  - [Missing Type and Initializer](#missing-type-and-initializer)
  - [Type Annotation with Initializer](#type-annotation-with-initializer)
- [Literal Expression Typing](#literal-expression-typing)
  - [Integer Literals](#integer-literals)
  - [Float Literals](#float-literals)
  - [String Literals](#string-literals)
  - [Char Literals](#char-literals)
  - [Boolean Literals](#boolean-literals)
  - [None Literal](#none-literal)
  - [Range Expressions](#range-expressions)
- [Lambda Return Type Inference](#lambda-return-type-inference)
- [For-In Loop Variable Inference](#for-in-loop-variable-inference)
  - [Explicit Type Annotation](#explicit-type-annotation)
  - [Range Iteration](#range-iteration)
  - [Array Iteration](#array-iteration)
  - [String Iteration](#string-iteration)
  - [Generator Iteration](#generator-iteration)
  - [Iterator Interface Protocol](#iterator-interface-protocol)
  - [Pair Destructuring](#pair-destructuring)
- [Collection Literal Inference](#collection-literal-inference)
  - [Array Expressions](#array-expressions)
  - [Tuple Expressions](#tuple-expressions)
  - [Set Expressions](#set-expressions)
  - [Map Expressions](#map-expressions)
- [Comprehension Result Type Inference](#comprehension-result-type-inference)
  - [List Comprehensions](#list-comprehensions)
  - [Set Comprehensions](#set-comprehensions)
  - [Map Comprehensions](#map-comprehensions)
  - [Comprehension Variable Inference](#comprehension-variable-inference)
- [Match Expression Type Inference](#match-expression-type-inference)
- [Generic Type Argument Requirement](#generic-type-argument-requirement)
  - [Standalone Generic Functions](#standalone-generic-functions)
  - [Generic Methods on Generic Classes](#generic-methods-on-generic-classes)
- [Examples](#examples)
  - [Variable Inference Patterns](#variable-inference-patterns)
  - [Loop and Collection Inference](#loop-and-collection-inference)
  - [Lambda and Match Inference](#lambda-and-match-inference)

---

## Overview

Uranite type inference is strictly local — each inference site resolves its type from immediately available context without consulting distant code. The inference rules are:

| Context | Inferred From | Fallback |
|---|---|---|
| Variable without type annotation | Initializer expression type | Error type |
| Lambda without return type | First return statement value type | Void |
| For-in loop variable | Iterable element type | Error type |
| Array/Tuple/Set literal | First non-error element type | Unparameterized collection type |
| Comprehension result | Body expression type | Unparameterized collection type |
| Match expression result | First arm value type | Error type |
| Generic function call | Not inferred — explicit required | Compile error |

---

## Variable Type Inference

### Initializer-Driven Inference

When a variable declaration omits the type annotation but provides an initializer, the variable type is inferred from the initializer expression type. Implemented in `src/uranite/semantic/analyzer.cpp:4033-4061`:

```cpp
void Analyzer::analyzeVariableStatement(
    ast::nodes::VariableStatement& statement ) {
    TypeSharedPointer variableType;
    if( statement.type ) {
        variableType = this->resolveType( statement.type );
    }
    if( statement.initializer ) {
        TypeSharedPointer initializerType =
            this->analyzeExpression( statement.initializer );
        if( variableType == nullptr ) {
            variableType = initializerType;
        }
        else if( initializerType &&
                 this->typeRegistry.isAssignable(
                     variableType, initializerType ) == false ) {
            this->diagnostic.error( statement.source,
                fmt::format(
                    "cannot assign value of type \"{}\" "
                    "to variable of type \"{}\"",
                    initializerType->toString(),
                    variableType->toString() ) );
        }
    }
    if( variableType == nullptr ) {
        variableType = this->typeRegistry.getError();
    }
    // ... register symbol with variableType
}
```

The algorithm:

1. If a type annotation is present (`statement.type` is non-null), resolve it to a `TypeSharedPointer`.
2. If an initializer is present, analyze it to get `initializerType`.
3. **Inference**: If `variableType` is still `nullptr` (no type annotation), set `variableType = initializerType`. The initializer type becomes the variable type.
4. **Validation**: If both annotation and initializer are present, check assignability. Mismatch produces a compile error.
5. **Fallback**: If no type could be determined (no annotation, no initializer, or failed resolution), use the error type to allow error recovery.

The `VariableStatement` AST node defined in `src/uranite/ast/node.hpp:2499-2540` has an optional `type` field (`TypeNodeSharedPointer type`) — when this field is `nullptr`, the parser did not find a type annotation, triggering inference.

### Missing Type and Initializer

If a variable has neither type annotation nor initializer, `variableType` remains `nullptr` through both checks and falls through to the error type fallback. This produces a variable with error type, allowing downstream analysis to continue without cascading errors.

### Type Annotation with Initializer

When both are present, the explicit annotation takes precedence. The initializer type is checked for assignability against the annotation type. If compatible, the annotation type is used (not the initializer type). This means explicit annotations can be wider than the initializer type — for example, annotating a union type when the initializer is a specific member.

---

## Literal Expression Typing

Every literal expression has a fixed type determined by its syntactic form. The analyzer uses OOP wrapper class lookup with primitive type fallback.

### Integer Literals

In `src/uranite/semantic/analyzer.cpp:2328-2331`:

```cpp
case ast::Node::Kind::IntegerLiteral: {
    TypeSharedPointer integerClassType =
        this->typeRegistry.lookupType(
            qualname::classes::i64::Name );
    expressionType = integerClassType
        ? integerClassType
        : this->typeRegistry.getInteger64();
    break;
}
```

Integer literals (e.g., `42`, `0xFF`, `0b1010`) resolve to the `I64` OOP wrapper class type. If the wrapper class is not loaded (early compilation before stdlib import), falls back to the primitive `Integer64` type. All integer literals are `I64` — there is no literal suffix system for selecting `I32`, `U8`, etc.

### Float Literals

In `src/uranite/semantic/analyzer.cpp:2308-2311`:

```cpp
case ast::Node::Kind::FloatLiteral: {
    TypeSharedPointer floatClassType =
        this->typeRegistry.lookupType(
            qualname::classes::f64::Name );
    expressionType = floatClassType
        ? floatClassType
        : this->typeRegistry.getFloat64();
    break;
}
```

Float literals (e.g., `3.14`, `1.0e10`) resolve to the `F64` OOP wrapper class type, falling back to primitive `Float64`.

### String Literals

In `src/uranite/semantic/analyzer.cpp:2414-2417`:

```cpp
case ast::Node::Kind::StringLiteral: {
    TypeSharedPointer stringClassType =
        this->typeRegistry.lookupType(
            qualname::classes::string::Name );
    expressionType = stringClassType
        ? stringClassType
        : this->typeRegistry.getString();
    break;
}
```

String literals (e.g., `"hello"`) resolve to the `String` OOP wrapper class type, falling back to primitive `String`.

### Char Literals

In `src/uranite/semantic/analyzer.cpp:2144-2147`:

Char literals (e.g., `'a'`) resolve to the `Char` OOP wrapper class type, falling back to primitive `Char`.

### Boolean Literals

In `src/uranite/semantic/analyzer.cpp:2129-2132`:

Boolean literals (`true`, `false`) resolve to the `Boolean` OOP wrapper class type, falling back to primitive `Bool`.

### None Literal

In `src/uranite/semantic/analyzer.cpp:2388-2390`:

```cpp
case ast::Node::Kind::NoneLiteral: {
    expressionType = this->typeRegistry.getNone();
    break;
}
```

The `None` literal resolves directly to the `None` type. No OOP wrapper lookup — `None` has no wrapper class.

### Range Expressions

In `src/uranite/semantic/analyzer.cpp:2392-2397`:

```cpp
case ast::Node::Kind::RangeExpression: {
    ast::nodes::RangeExpression& rangeExpression =
        static_cast<ast::nodes::RangeExpression&>( *expression );
    this->analyzeExpression( rangeExpression.start );
    this->analyzeExpression( rangeExpression.end );
    expressionType =
        this->typeRegistry.makeArray( this->typeRegistry.getInteger64() );
    break;
}
```

Range expressions (e.g., `0..10`) always produce `Array<I64>` regardless of the start/end expression types.

---

## Lambda Return Type Inference

Lambda expressions infer their return type from the first return statement when no explicit return type annotation is provided. Implemented in `src/uranite/semantic/analyzer.cpp:2344-2357`:

```cpp
TypeSharedPointer returnType = lambdaExpression.returnType
    ? this->resolveType( lambdaExpression.returnType )
    : TypeSharedPointer( nullptr );
if( returnType == nullptr &&
    lambdaExpression.body.empty() == false ) {
    if( lambdaExpression.body[0] &&
        lambdaExpression.body[0]->kind ==
            ast::Node::Kind::ReturnStatement ) {
        ast::nodes::ReturnStatement& returnStatement =
            static_cast<ast::nodes::ReturnStatement&>(
                *lambdaExpression.body[0] );
        if( returnStatement.value ) {
            returnType =
                this->analyzeExpression( returnStatement.value );
        }
    }
}
if( returnType == nullptr ) {
    returnType = this->typeRegistry.getVoid();
}
```

The inference algorithm:

1. If an explicit return type annotation is present, resolve it and use it.
2. If no annotation and the body is non-empty, check if the first statement is a `ReturnStatement`.
3. If the first statement is a return with a value, analyze the value expression and use its type as the return type.
4. If no return type can be determined (empty body, no return statement, or return without value), default to `Void`.

This inference is intentionally limited — only the *first statement* is checked, and only if it is a `ReturnStatement`. Multi-statement lambdas with returns deeper in the body do not trigger inference; they require an explicit return type annotation.

---

## For-In Loop Variable Inference

For-in loops infer the loop variable type from the iterable when no explicit type annotation is provided. The inference depends on the iterable kind.

### Explicit Type Annotation

When the loop variable has an explicit type, it is used directly with no inference, in `src/uranite/semantic/analyzer.cpp:2575-2576`:

```cpp
if( statement.variableType ) {
    variableType = this->resolveType( statement.variableType );
```

### Range Iteration

In `src/uranite/semantic/analyzer.cpp:2594-2595`:

```cpp
if( isRangeExpression ) {
    variableType = this->typeRegistry.getInteger64();
}
```

Iterating over a range expression (`for x in 0..10`) infers the variable as `I64`.

### Array Iteration

In `src/uranite/semantic/analyzer.cpp:2597-2598`:

```cpp
else if( iterableType->kind == Type::Kind::Array ) {
    variableType =
        std::static_pointer_cast<ArrayType>( iterableType )->elementType;
}
```

Iterating over an array extracts the `elementType` from the `ArrayType`.

### String Iteration

In `src/uranite/semantic/analyzer.cpp:2600-2601`:

```cpp
else if( iterableType->kind == Type::Kind::String ||
         iterableType->qualified == qualname::String ) {
    variableType = this->typeRegistry.getChar();
}
```

Iterating over a `String` infers the variable as `Char` — each iteration yields one character.

### Generator Iteration

In `src/uranite/semantic/analyzer.cpp:2603-2604`:

```cpp
else if( iterableType->kind == Type::Kind::Generator ) {
    variableType =
        std::static_pointer_cast<GeneratorType>( iterableType )->yieldType;
}
```

Iterating over a `Generator<T>` infers the variable as `T` — the generator yield type.

### Iterator Interface Protocol

When the iterable is none of the above built-in types, the analyzer checks for iterator interface implementation via the `hasIteratorInterface` lambda in `src/uranite/semantic/analyzer.cpp:2510-2573`:

The `hasIteratorInterface` function performs the following resolution:

1. Check if the iterable type (class or interface) implements `Iterator` or `Iterable` (via transitive interface check with `isIteratorOrIterable`).
2. **Direct Iterator**: If the type has a `next()` method, return its return type. The loop variable type is the return type of `next()`, unwrapped from `Optional` if present.
3. **Iterable protocol**: If the type has an `iterator()` method, get its return type (the iterator object type), then find `next()` on that iterator type and return its return type.

After `hasIteratorInterface` returns, in `src/uranite/semantic/analyzer.cpp:2607-2614`:

```cpp
TypeSharedPointer nextReturnType = hasIteratorInterface( iterableType );
if( nextReturnType ) {
    if( nextReturnType->kind == Type::Kind::Optional ) {
        variableType =
            std::static_pointer_cast<OptionalType>( nextReturnType )->inner;
    }
    else {
        variableType = nextReturnType;
    }
}
```

If `next()` returns `Optional<T>`, the loop variable type is `T` (unwrapped). If `next()` returns `T` directly, the variable type is `T`.

If the type does not implement either interface, a compile error is emitted: "type X is not iterable; class must implement Iterator or Iterable interface".

### Pair Destructuring

Two-variable for-in loops (`for key, value in map`) infer both variable types from `Pair<K,V>` struct fields, in `src/uranite/semantic/analyzer.cpp:2630-2669`:

When `statement.variable2` is non-empty and the inferred element type is a `StructType` whose base name matches `Pair`:

1. Look up `key` and `value` fields on the `Pair` struct.
2. If the field types are `GenericParameter` types, resolve them through the iterable class `typeSubstitutions` map (e.g., `HashMap<String, I64>` substitutes `K→String`, `V→I64`).
3. First variable gets the resolved key type; second variable gets the resolved value type.

This enables:

```
for String name, I64 age in personMap:
    puts(name)
```

Where `personMap` is `HashMap<String, I64>`, the iterator yields `Pair<String, I64>`, and destructuring infers `name: String`, `age: I64`.

---

## Collection Literal Inference

Collection literals infer their element type from the first non-error element, then monomorphize the appropriate collection class.

### Array Expressions

In `src/uranite/semantic/analyzer.cpp:2088-2103`:

```cpp
case ast::Node::Kind::ArrayExpression: {
    ast::nodes::ArrayExpression& arrayExpression =
        static_cast<ast::nodes::ArrayExpression&>( *expression );
    TypeSharedPointer inferredElementType;
    for( ast::nodes::ExpressionSharedPointer& element :
             arrayExpression.elements ) {
        TypeSharedPointer elementType =
            this->analyzeExpression( element );
        if( inferredElementType == nullptr &&
            elementType != nullptr &&
            elementType->isError() == false ) {
            inferredElementType = elementType;
        }
    }
    if( inferredElementType != nullptr ) {
        expressionType = this->monomorphizeGenericType(
            qualname::classes::arraylist::Name,
            { inferredElementType }, expression->source );
    }
    else {
        TypeSharedPointer arrayListType =
            this->typeRegistry.lookupType(
                qualname::classes::arraylist::Name );
        expressionType = arrayListType
            ? arrayListType
            : std::make_shared<Type>(
                  Type::Kind::Class,
                  qualname::classes::arraylist::Name );
    }
    break;
}
```

Array literals (`[1, 2, 3]`) iterate all elements, analyze each, and take the first non-error element type as the element type. Then monomorphize `ArrayList<E>` with that element type. If all elements are errors or the array is empty, produce an unparameterized `ArrayList`.

### Tuple Expressions

In `src/uranite/semantic/analyzer.cpp:2223-2238`:

Same pattern as array expressions: analyze elements, take first non-error type, monomorphize `Tuple<E>`. If no valid element type, produce unparameterized `Tuple`.

### Set Expressions

In `src/uranite/semantic/analyzer.cpp:2264-2278`:

Same pattern: first non-error element type monomorphizes `HashSet<E>`.

### Map Expressions

In `src/uranite/semantic/analyzer.cpp:2242-2260`:

Map literals (`{"key": value}`) analyze the first key-value pair. Both the key expression type and value expression type are extracted, then used to monomorphize `HashMap<K, V>`.

---

## Comprehension Result Type Inference

Comprehensions infer their result collection type from the body expression type and the comprehension kind.

### List Comprehensions

Default comprehension kind, in `src/uranite/semantic/analyzer.cpp:2210-2217`:

```cpp
default: {
    if( bodyType != nullptr && bodyType->isError() == false ) {
        expressionType = this->monomorphizeGenericType(
            qualname::classes::arraylist::Name,
            { bodyType }, expression->source );
    }
    // ...
}
```

`[expr for x in iter]` — body expression type becomes the element type for `ArrayList<E>` monomorphization.

### Set Comprehensions

In `src/uranite/semantic/analyzer.cpp:2200-2207`:

`{expr for x in iter}` — body type monomorphizes `HashSet<E>`.

### Map Comprehensions

In `src/uranite/semantic/analyzer.cpp:2188-2197`:

`{key: value for x in iter}` — key expression type and body type (value) monomorphize `HashMap<K, V>`.

### Comprehension Variable Inference

Comprehension loop variables use the same inference logic as for-in loops, in `src/uranite/semantic/analyzer.cpp:2153-2172`:

- If `variableType` annotation is present, use it.
- If absent and iterable is `Array`, extract `elementType`.
- If iterable is `String`, infer `Char`.
- If iterable is `Generator`, extract `yieldType`.
- Otherwise, default to `I64` (for range-like iterables).

---

## Match Expression Type Inference

Match expressions infer their result type from the first arm value, in `src/uranite/semantic/analyzer.cpp:2360-2378`:

```cpp
case ast::Node::Kind::MatchExpression: {
    ast::nodes::MatchExpression& matchExpression =
        static_cast<ast::nodes::MatchExpression&>( *expression );
    this->analyzeExpression( matchExpression.subject );
    TypeSharedPointer commonResultType;
    for( size_t patternIndex = 0;
         patternIndex < matchExpression.patterns.size();
         patternIndex++ ) {
        this->analyzeExpression(
            matchExpression.patterns[patternIndex] );
        TypeSharedPointer patternValueType =
            this->analyzeExpression(
                matchExpression.values[patternIndex] );
        if( commonResultType == nullptr && patternValueType ) {
            commonResultType = patternValueType;
        }
    }
    if( matchExpression.defaultValue ) {
        TypeSharedPointer defaultValueType =
            this->analyzeExpression( matchExpression.defaultValue );
        if( commonResultType == nullptr && defaultValueType ) {
            commonResultType = defaultValueType;
        }
    }
    expressionType = commonResultType
        ? commonResultType
        : this->typeRegistry.getError();
    break;
}
```

The first non-null arm value type becomes the match expression result type. If no arm produces a type, the default arm value type is used. If even the default produces no type, the result is the error type.

The compiler does not check that all arms return the same type — the first arm type wins. Mismatched arm types may produce type errors at the assignment site rather than at the match expression itself.

---

## Generic Type Argument Requirement

Uranite does not infer generic type arguments on function calls. Generic functions require explicit type arguments at call sites.

### Standalone Generic Functions

When calling a generic function without type arguments, the compiler emits an error, in `src/uranite/semantic/analyzer.cpp:1320-1324`:

```cpp
if( expression.typeArguments.empty() ) {
    std::string genericErrorMessage = fmt::format(
        "generic function \"{}\" requires explicit type arguments: "
        "{}<{}>(…)",
        calleeName, calleeName,
        fmt::join( functionType->genericParameterNames, ", " ) );
    this->diagnostic.error( expression.source, genericErrorMessage );
    return this->typeRegistry.getError();
}
```

The error message helpfully shows the expected syntax: `functionName<TypeArgs>(...)`.

### Generic Methods on Generic Classes

For methods on generic classes, method generic parameters can be covered by the class type substitutions, in `src/uranite/semantic/analyzer.cpp:3500-3513`:

```cpp
else if( functionType->genericParameterNames.empty() == false &&
         expression.typeArguments.empty() ) {
    bool allCoveredByClass = true;
    for( const std::string& genericName :
             functionType->genericParameterNames ) {
        if( classType->typeSubstitutions.find( genericName ) ==
            classType->typeSubstitutions.end() ) {
            allCoveredByClass = false;
            break;
        }
    }
    if( allCoveredByClass == false ) {
        // error: generic method requires explicit type arguments
    }
}
```

If all generic parameter names on the method are already present in the class `typeSubstitutions` (because they are class-level generic parameters, not method-level), the call is allowed without explicit type arguments. Only method-level generic parameters that are *not* covered by the class require explicit arguments.

For example, `ArrayList<I64>` has `E=I64` in its `typeSubstitutions`. Calling `list.add(42)` where `add` has generic parameter `E` does not require explicit type arguments — `E` is already resolved from the class.

---

## Examples

### Variable Inference Patterns

```
package examples

from uranite.io.console import puts

public function main() -> I32:
    I64 explicit = 42
    mutable inferred = 100
    mutable name = "Uranite"
    mutable flag = true
    mutable pi = 3.14159
    mutable letter = 'U'
    mutable empty = None

    puts(inferred.toString())
    puts(name)

    return 0
```

**Compilation trace**:
1. `explicit` — has type annotation `I64`, no inference needed.
2. `inferred` — no type annotation, initializer `100` is `IntegerLiteral` → type `I64`. Variable type inferred as `I64`.
3. `name` — initializer `"Uranite"` is `StringLiteral` → type `String`. Inferred as `String`.
4. `flag` — initializer `true` is `BooleanLiteral` → type `Boolean`. Inferred as `Boolean`.
5. `pi` — initializer `3.14159` is `FloatLiteral` → type `F64`. Inferred as `F64`.
6. `letter` — initializer `'U'` is `CharLiteral` → type `Char`. Inferred as `Char`.
7. `empty` — initializer `None` is `NoneLiteral` → type `None`. Inferred as `None`.

### Loop and Collection Inference

```
package examples

from uranite.collection.array-list import ArrayList
from uranite.collection.hash-map import HashMap
from uranite.io.console import puts

public function main() -> I32:
    mutable numbers = [10, 20, 30]
    mutable pairs = {"alice": 1, "bob": 2}

    for element in numbers:
        puts(element.toString())

    for key, value in pairs:
        puts(key + ": " + value.toString())

    mutable squares = [x * x for I64 x in 0..5]

    return 0
```

**Compilation trace**:
1. `numbers` — array literal `[10, 20, 30]`, first element `10` is `I64`. Monomorphize `ArrayList<I64>`. Variable inferred as `ArrayList<I64>`.
2. `pairs` — map literal `{"alice": 1, "bob": 2}`, key `"alice"` is `String`, value `1` is `I64`. Monomorphize `HashMap<String, I64>`. Inferred as `HashMap<String, I64>`.
3. `for element in numbers` — no type annotation. `numbers` is `ArrayList<I64>`, implements `Iterable`. `hasIteratorInterface` finds `next()` returning `Optional<I64>`, unwraps to `I64`. Variable `element` inferred as `I64`.
4. `for key, value in pairs` — two-variable destructuring. `pairs` is `HashMap<String, I64>`, iterator yields `Pair<String, I64>`. Pair fields extracted: `key` field type `String`, `value` field type `I64`. First variable `key` inferred as `String`, second variable `value` inferred as `I64`.
5. `squares` — list comprehension `[x * x for I64 x in 0..5]`. Loop variable `x` explicitly typed `I64`. Body expression `x * x` is `I64`. Monomorphize `ArrayList<I64>`. Variable inferred as `ArrayList<I64>`.

### Lambda and Match Inference

```
package examples

from uranite.io.console import puts

public function apply(Callable<I64, <I64>> operation, I64 value) -> I64:
    return operation(value)

public function main() -> I32:
    mutable doubler = lambda (I64 number) => return number * 2
    mutable result = apply(doubler, 21)

    mutable label = match result:
        42 => "answer"
        0 => "zero"
        default => "other"

    puts(label)
    return 0
```

**Compilation trace**:
1. `doubler` — lambda expression with no return type annotation. First body statement is `return number * 2`. Analyzer evaluates `number * 2` where `number` is `I64` → result is `I64`. Lambda return type inferred as `I64`. Lambda type is `FunctionType([I64], I64)`. Variable inferred as `FunctionType`.
2. `result` — `apply(doubler, 21)` returns `I64` (function return type). Variable inferred as `I64`.
3. `label` — match expression. First arm value `"answer"` is `StringLiteral` → type `String`. Match result type inferred as `String`. Variable inferred as `String`.
