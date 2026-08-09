# Function Types

---

## Table of Contents

- [Function Types](#function-types)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Declaring Functions](#declaring-functions)
    - [Basic Functions](#basic-functions)
    - [Void Functions](#void-functions)
    - [The pass Statement](#the-pass-statement)
  - [Parameters](#parameters)
    - [Typed Parameters](#typed-parameters)
    - [Multiple Parameters](#multiple-parameters)
    - [Default Values](#default-values)
  - [Return Values](#return-values)
  - [Function Overloading](#function-overloading)
  - [Recursive Functions](#recursive-functions)
  - [Nested Functions](#nested-functions)
  - [Variadic Parameters](#variadic-parameters)
  - [Keyword Parameters](#keyword-parameters)
  - [Lambda Expressions](#lambda-expressions)
    - [Lambda Syntax](#lambda-syntax)
    - [Lambda as Argument](#lambda-as-argument)
  - [Callable Type](#callable-type)
    - [Callable Variables](#callable-variables)
    - [Callable Parameters](#callable-parameters)
    - [Returning Callables](#returning-callables)
  - [Generic Functions](#generic-functions)
  - [Method Reference](#method-reference)
    - [Declaration Syntax](#declaration-syntax)
    - [Parameter Kinds](#parameter-kinds)
    - [Callable Type Syntax](#callable-type-syntax)
  - [Examples](#examples)
    - [Mathematical Operations](#mathematical-operations)
    - [Higher-Order Functions](#higher-order-functions)
    - [Variadic Sum](#variadic-sum)
    - [Configuration with Keywords](#configuration-with-keywords)

---

## Overview

Functions are reusable blocks of code that accept parameters and return values. Uranite functions are first-class values — they can be stored in variables, passed as arguments, and returned from other functions using the `Callable` type.

Functions support overloading by parameter count, default parameter values, variadic parameters, keyword parameters, nested definitions, lambda expressions, and generic type parameters.

---

## Declaring Functions

### Basic Functions

A function declaration starts with an optional access modifier, the `function` keyword, a name, parameters in parentheses, a return type after `->`, and an indented body.

```uranite
public function add( I64 first, I64 second ) -> I64:
    return first + second
```

### Void Functions

Functions that do not return a value use `Void` as the return type.

```uranite
from uranite.io.console import puts

public function printValue( I64 value ) -> Void:
    puts( value.toString() )
```

### The pass Statement

Use `pass` for functions with no body.

```uranite
public function placeholder() -> Void:
    pass
```

---

## Parameters

### Typed Parameters

Each parameter specifies a type followed by a name.

```uranite
public function greet( String name ) -> String:
    return name
```

### Multiple Parameters

Separate parameters with commas.

```uranite
public function multiply( I64 first, I64 second ) -> I64:
    return first * second
```

### Default Values

Parameters can have default values using `=`. When calling the function, parameters with defaults can be omitted.

```uranite
from uranite.io.console import puts

public function greet( String name, String prefix = "Hello" ) -> Void:
    puts( prefix )
    puts( name )

public function main() -> I32:
    greet( "Alice" )
    greet( "Bob", "Hi" )
    return 0
```

Output:

```
Hello
Alice
Hi
Bob
```

---

## Return Values

Use `return` to produce a value from a function. The return type is declared after `->`.

```uranite
public function square( I64 value ) -> I64:
    return value * value
```

Functions can have multiple return points with conditional logic.

```uranite
public function absolute( I64 value ) -> I64:
    if value < 0:
        return 0 - value
    return value
```

---

## Function Overloading

Multiple functions can share the same name if they have different numbers of parameters.

```uranite
from uranite.io.console import puts

public function compute( I64 value ) -> I64:
    return value * 2

public function compute( I64 first, I64 second ) -> I64:
    return first + second

public function compute( I64 first, I64 second, I64 third ) -> I64:
    return first + second + third

public function main() -> I32:
    puts( compute( 5 ).toString() )
    puts( compute( 10, 20 ).toString() )
    puts( compute( 1, 2, 3 ).toString() )
    return 0
```

Output:

```
10
30
6
```

The compiler selects the correct overload based on the number of arguments at the call site.

---

## Recursive Functions

Functions can call themselves recursively.

```uranite
from uranite.io.console import puts

public function factorial( I64 number ) -> I64:
    if number <= 1:
        return 1
    return number * factorial( number - 1 )

public function main() -> I32:
    puts( factorial( 5 ).toString() )
    puts( factorial( 10 ).toString() )
    return 0
```

Output:

```
120
3628800
```

---

## Nested Functions

Functions can be defined inside other functions. Nested functions do not use access modifiers. They can access variables from the enclosing function.

```uranite
from uranite.io.console import puts

public function outer() -> I64:
    I64 factor = 10
    function inner( I64 value ) -> I64:
        return value * factor
    return inner( 5 )

public function main() -> I32:
    puts( outer().toString() )
    return 0
```

Output:

```
50
```

The nested `inner` function captures `factor` from its enclosing scope.

---

## Variadic Parameters

A variadic parameter accepts any number of arguments of the same type. Declare it with `[]` after the parameter name.

```uranite
from uranite.io.console import puts

public function sumAll( I64 values[] ) -> I64:
    I64 total = 0
    for I64 value in values:
        total = total + value
    return total

public function main() -> I32:
    puts( sumAll( 1, 2, 3, 4, 5 ).toString() )
    return 0
```

Output:

```
15
```

The variadic parameter collects all arguments into an iterable sequence. Use a `for-in` loop to process each value.

---

## Keyword Parameters

A keyword parameter accepts named arguments as key-value pairs. Declare it with `{}` after the parameter name.

```uranite
from uranite.io.console import puts

public function configure( String options{} ) -> Void:
    for String key, String value in options:
        puts( key )
        puts( value )

public function main() -> I32:
    configure( name = "App", version = "1.0" )
    return 0
```

Output:

```
name
App
version
1.0
```

At the call site, pass keyword arguments using `name = value` syntax. The keyword parameter receives all named arguments as pairs.

---

## Lambda Expressions

### Lambda Syntax

Lambdas are anonymous functions defined with the `lambda` keyword. The syntax is `lambda Type name: expression`.

```uranite
lambda I64 x: x * 2
```

For multiple parameters, separate them with commas:

```uranite
lambda I64 x, I64 y: x + y
```

### Lambda as Argument

Lambdas are most commonly passed directly as function arguments.

```uranite
from uranite.io.console import puts

public function apply( Callable<I64, <I64>> transform, I64 value ) -> I64:
    return transform( value )

public function main() -> I32:
    I64 doubled = apply( lambda I64 x: x * 2, 5 )
    puts( doubled.toString() )

    I64 squared = apply( lambda I64 x: x * x, 4 )
    puts( squared.toString() )
    return 0
```

Output:

```
10
16
```

---

## Callable Type

The `Callable` type represents a function reference. It encodes the return type and parameter types.

The syntax is `Callable<ReturnType, <ParamType1, ParamType2>>`.

### Callable Variables

Store a function reference in a variable typed as `Callable`.

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

### Callable Parameters

Functions can accept other functions as parameters using the `Callable` type.

```uranite
from uranite.io.console import puts

public function applyTwice( Callable<I64, <I64>> transform, I64 value ) -> I64:
    I64 first = transform( value )
    return transform( first )

public function doubleIt( I64 x ) -> I64:
    return x * 2

public function main() -> I32:
    I64 result = applyTwice( doubleIt, 3 )
    puts( result.toString() )
    return 0
```

Output:

```
12
```

The value `3` is doubled to `6`, then doubled again to `12`.

### Returning Callables

Functions can return function references.

```uranite
from uranite.io.console import puts

public function addN( I64 x ) -> I64:
    return x + 10

public function getAdder() -> Callable<I64, <I64>>:
    return addN

public function main() -> I32:
    Callable<I64, <I64>> adder = getAdder()
    puts( adder( 5 ).toString() )
    return 0
```

Output:

```
15
```

---

## Generic Functions

Functions can declare type parameters in angle brackets after the function name.

```uranite
from uranite.io.console import puts

public function identity<T>( T value ) -> T:
    return value

public function main() -> I32:
    I64 number = identity<I64>( 42 )
    puts( number.toString() )

    String text = identity<String>( "hello" )
    puts( text )
    return 0
```

Output:

```
42
hello
```

Specify the concrete type in angle brackets at the call site.

---

## Method Reference

### Declaration Syntax

| Syntax | Description |
|---|---|
| `public function name( params ) -> Type:` | Public function declaration |
| `function name( params ) -> Type:` | Default visibility function |
| `function name( params ) -> Void:` | Function with no return value |
| `function name<T>( T param ) -> T:` | Generic function |

### Parameter Kinds

| Syntax | Description |
|---|---|
| `Type name` | Standard typed parameter |
| `Type name = value` | Parameter with default value |
| `Type name[]` | Variadic parameter (collects multiple arguments) |
| `Type name{}` | Keyword parameter (collects named arguments) |

### Callable Type Syntax

| Syntax | Description |
|---|---|
| `Callable<I64, <I64>>` | Function taking one `I64`, returning `I64` |
| `Callable<String, <I64, I64>>` | Function taking two `I64`, returning `String` |
| `Callable<Void, <String>>` | Function taking one `String`, returning nothing |

---

## Examples

### Mathematical Operations

```uranite
from uranite.io.console import puts

public function power( I64 base, I64 exponent ) -> I64:
    if exponent <= 0:
        return 1
    return base * power( base, exponent - 1 )

public function maximum( I64 first, I64 second ) -> I64:
    if first >= second:
        return first
    return second

public function minimum( I64 first, I64 second ) -> I64:
    if first <= second:
        return first
    return second

public function main() -> I32:
    puts( power( 2, 10 ).toString() )
    puts( maximum( 42, 17 ).toString() )
    puts( minimum( 42, 17 ).toString() )
    return 0
```

Output:

```
1024
42
17
```

### Higher-Order Functions

```uranite
from uranite.io.console import puts

public function apply( Callable<I64, <I64>> transform, I64 value ) -> I64:
    return transform( value )

public function doubleIt( I64 x ) -> I64:
    return x * 2

public function squareIt( I64 x ) -> I64:
    return x * x

public function negateIt( I64 x ) -> I64:
    return 0 - x

public function main() -> I32:
    puts( apply( doubleIt, 5 ).toString() )
    puts( apply( squareIt, 4 ).toString() )
    puts( apply( negateIt, 7 ).toString() )

    I64 lambdaResult = apply( lambda I64 x: x + 100, 5 )
    puts( lambdaResult.toString() )
    return 0
```

Output:

```
10
16
-7
105
```

### Variadic Sum

```uranite
from uranite.io.console import puts

public function sumAll( I64 values[] ) -> I64:
    I64 total = 0
    for I64 value in values:
        total = total + value
    return total

public function countAll( I64 values[] ) -> I64:
    I64 count = 0
    for I64 value in values:
        count = count + 1
    return count

public function main() -> I32:
    puts( sumAll( 10, 20, 30 ).toString() )
    puts( sumAll( 1, 2, 3, 4, 5 ).toString() )
    puts( countAll( 100, 200, 300, 400 ).toString() )
    return 0
```

Output:

```
60
15
4
```

### Configuration with Keywords

```uranite
from uranite.io.console import puts

public function configure( String options{} ) -> Void:
    for String key, String value in options:
        puts( key )
        puts( value )

public function main() -> I32:
    configure( host = "localhost", port = "8080", debug = "true" )
    return 0
```

Output:

```
host
localhost
port
8080
debug
true
```
