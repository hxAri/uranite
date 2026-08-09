# Callable Types

---

## Table of Contents

- [Callable Types](#callable-types)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Type Syntax](#type-syntax)
    - [Basic Callable Types](#basic-callable-types)
    - [No-Parameter Callables](#no-parameter-callables)
    - [Multi-Parameter Callables](#multi-parameter-callables)
  - [Storing Functions in Variables](#storing-functions-in-variables)
  - [Callable Parameters](#callable-parameters)
  - [Returning Callables](#returning-callables)
  - [Lambda Expressions](#lambda-expressions)
    - [Single Parameter](#single-parameter)
    - [Multiple Parameters](#multiple-parameters)
    - [Passing Lambdas to Functions](#passing-lambdas-to-functions)
  - [Method Reference](#method-reference)
    - [Type Syntax Reference](#type-syntax-reference)
    - [Lambda Syntax Reference](#lambda-syntax-reference)
  - [Examples](#examples)
    - [Function Dispatch Table](#function-dispatch-table)
    - [Transform Pipeline](#transform-pipeline)
    - [Predicate Filtering](#predicate-filtering)
    - [Callback Pattern](#callback-pattern)

---

## Overview

The `Callable` type represents a reference to a function. It encodes both the return type and parameter types, enabling type-safe storage and invocation of function values. Any named function can be assigned to a `Callable` variable, passed as an argument, or returned from a function.

Lambda expressions create anonymous functions that can be passed directly as `Callable` arguments.

---

## Type Syntax

### Basic Callable Types

A `Callable` type is written with nested angle brackets. The first type argument is the return type. The second is a nested angle-bracket group containing the parameter types.

```uranite
Callable<ReturnType, <ParamType1, ParamType2>>
```

A function that takes one `I64` and returns an `I64`:

```uranite
Callable<I64, <I64>>
```

A function that takes two `I64` values and returns an `I64`:

```uranite
Callable<I64, <I64, I64>>
```

### No-Parameter Callables

For functions with no parameters, use empty inner angle brackets.

```uranite
Callable<I64, <>>
```

### Multi-Parameter Callables

Separate parameter types with commas inside the inner angle brackets.

```uranite
Callable<String, <I64, String, Boolean>>
```

---

## Storing Functions in Variables

Any named function can be assigned to a `Callable` variable with a matching signature.

```uranite
from uranite.io.console import puts

public function tripleIt( I64 x ) -> I64:
    return x * 3

public function main() -> I32:
    Callable<I64, <I64>> operation = tripleIt
    I64 result = operation( 7 )
    puts( result.toString() )
    return 0
```

Output:

```
21
```

The variable `operation` holds a reference to `tripleIt`. Calling `operation( 7 )` invokes `tripleIt( 7 )`.

---

## Callable Parameters

Functions can accept `Callable` parameters, enabling higher-order programming.

```uranite
from uranite.io.console import puts

public function apply( Callable<I64, <I64>> transform, I64 value ) -> I64:
    return transform( value )

public function doubleIt( I64 x ) -> I64:
    return x * 2

public function squareIt( I64 x ) -> I64:
    return x * x

public function main() -> I32:
    puts( apply( doubleIt, 5 ).toString() )
    puts( apply( squareIt, 4 ).toString() )
    return 0
```

Output:

```
10
16
```

Different functions can be passed to the same higher-order function, producing different behavior.

---

## Returning Callables

Functions can return `Callable` values.

```uranite
from uranite.io.console import puts

public function addTen( I64 x ) -> I64:
    return x + 10

public function getTransform() -> Callable<I64, <I64>>:
    return addTen

public function main() -> I32:
    Callable<I64, <I64>> transform = getTransform()
    puts( transform( 5 ).toString() )
    return 0
```

Output:

```
15
```

---

## Lambda Expressions

Lambdas create anonymous functions using the `lambda` keyword. They are most commonly passed directly as `Callable` arguments.

### Single Parameter

```uranite
lambda I64 x: x * 2
```

The syntax is `lambda Type name: expression`. The expression after the colon is the return value.

### Multiple Parameters

```uranite
lambda I64 x, I64 y: x + y
```

Separate parameters with commas. Each parameter needs a type and a name.

### Passing Lambdas to Functions

Lambdas work best when passed directly as function arguments.

```uranite
from uranite.io.console import puts

public function apply( Callable<I64, <I64>> transform, I64 value ) -> I64:
    return transform( value )

public function main() -> I32:
    I64 doubled = apply( lambda I64 x: x * 2, 5 )
    puts( doubled.toString() )

    I64 squared = apply( lambda I64 x: x * x, 4 )
    puts( squared.toString() )

    I64 incremented = apply( lambda I64 x: x + 1, 99 )
    puts( incremented.toString() )
    return 0
```

Output:

```
10
16
100
```

---

## Method Reference

### Type Syntax Reference

| Type | Description |
|---|---|
| `Callable<I64, <I64>>` | Takes one `I64`, returns `I64` |
| `Callable<I64, <I64, I64>>` | Takes two `I64`, returns `I64` |
| `Callable<String, <I64>>` | Takes one `I64`, returns `String` |
| `Callable<Void, <String>>` | Takes one `String`, returns nothing |
| `Callable<Boolean, <I64, I64>>` | Takes two `I64`, returns `Boolean` |
| `Callable<I64, <>>` | Takes no parameters, returns `I64` |

### Lambda Syntax Reference

| Syntax | Description |
|---|---|
| `lambda I64 x: x * 2` | Single parameter lambda |
| `lambda I64 x, I64 y: x + y` | Multi-parameter lambda |

---

## Examples

### Function Dispatch Table

```uranite
from uranite.io.console import puts

public function addOp( I64 first, I64 second ) -> I64:
    return first + second

public function subOp( I64 first, I64 second ) -> I64:
    return first - second

public function mulOp( I64 first, I64 second ) -> I64:
    return first * second

public function execute( Callable<I64, <I64, I64>> operation, I64 first, I64 second ) -> I64:
    return operation( first, second )

public function main() -> I32:
    puts( execute( addOp, 10, 3 ).toString() )
    puts( execute( subOp, 10, 3 ).toString() )
    puts( execute( mulOp, 10, 3 ).toString() )
    return 0
```

Output:

```
13
7
30
```

### Transform Pipeline

```uranite
from uranite.io.console import puts

public function apply( Callable<I64, <I64>> transform, I64 value ) -> I64:
    return transform( value )

public function doubleIt( I64 x ) -> I64:
    return x * 2

public function addFive( I64 x ) -> I64:
    return x + 5

public function squareIt( I64 x ) -> I64:
    return x * x

public function main() -> I32:
    I64 value = 3
    I64 step1 = apply( doubleIt, value )
    I64 step2 = apply( addFive, step1 )
    I64 step3 = apply( squareIt, step2 )
    puts( step1.toString() )
    puts( step2.toString() )
    puts( step3.toString() )
    return 0
```

Output:

```
6
11
121
```

### Predicate Filtering

```uranite
from uranite.io.console import puts

public function check( Callable<Boolean, <I64>> predicate, I64 value ) -> Boolean:
    return predicate( value )

public function isPositive( I64 x ) -> Boolean:
    return x > 0

public function isEven( I64 x ) -> Boolean:
    I64 remainder = x % 2
    return remainder == 0

public function main() -> I32:
    puts( check( isPositive, 5 ).toString() )
    puts( check( isPositive, -3 ).toString() )
    puts( check( isEven, 4 ).toString() )
    puts( check( isEven, 7 ).toString() )
    return 0
```

Output:

```
True
False
True
False
```

### Callback Pattern

```uranite
from uranite.io.console import puts

public function processValue( I64 value, Callable<Void, <I64>> callback ) -> Void:
    I64 result = value * 10
    callback( result )

public function printResult( I64 value ) -> Void:
    puts( value.toString() )

public function main() -> I32:
    processValue( 5, printResult )
    processValue( 12, printResult )
    return 0
```

Output:

```
50
120
```
