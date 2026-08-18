# Functions

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Functions](#functions)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Function Declarations](#function-declarations)
    - [Basic Syntax](#basic-syntax)
    - [Visibility Modifiers](#visibility-modifiers)
  - [Parameters and Return Types](#parameters-and-return-types)
    - [Typed Parameters](#typed-parameters)
    - [Return Types](#return-types)
    - [Void Functions](#void-functions)
  - [Default Parameters](#default-parameters)
  - [Variadic Parameters](#variadic-parameters)
  - [Keyword Parameters](#keyword-parameters)
  - [Mutable Parameters](#mutable-parameters)
  - [Nested Functions](#nested-functions)
  - [Recursion](#recursion)
  - [Exception Specification](#exception-specification)
  - [Extern Functions](#extern-functions)

## Overview

Functions are the primary unit of behavior in Uranite. Every function has a name, a parameter list, and a return type. Functions use indentation-based blocks opened by a colon, and support features like default parameters, variadic arguments, keyword arguments, mutable parameters, nested scoping, exception specifications, and foreign function linkage.

## Function Declarations

### Basic Syntax

A function declaration starts with the `function` keyword, followed by the name, parameter list, return type annotation, and an indented body.

```uranite
package testing

from uranite.io.console import puts

public function greet( String name ) -> Void:
    puts( "Hello " + name )

public function add( I64 first, I64 second ) -> I64:
    return first + second

public function main() -> I32:
    greet( "Uranite" )
    I64 result = add( 10, 20 )
    puts( result )
    return 0
```

Output:

```
Hello Uranite
30
```

### Visibility Modifiers

Functions support the same visibility modifiers as other declarations.

| Modifier | Scope |
|---|---|
| `public` | Accessible from any module |
| `protect` | Accessible from the same module and subclasses |
| `private` | Accessible only within the enclosing class or file |

Top-level functions intended for use outside their module must be declared `public`.

## Parameters and Return Types

### Typed Parameters

Every parameter requires an explicit type annotation. Multiple parameters are separated by commas.

```uranite
public function multiply( I64 left, I64 right ) -> I64:
    return left * right
```

### Return Types

The return type is specified after the `->` arrow. Every function must declare its return type explicitly.

```uranite
public function square( I64 value ) -> I64:
    return value * value
```

### Void Functions

Functions that produce no return value use `Void` as their return type.

```uranite
public function greetUser( String name ) -> Void:
    puts( "Welcome " + name )
```

## Default Parameters

Parameters can have default values using the `=` operator. Parameters with defaults must appear after all required parameters.

```uranite
package testing

from uranite.io.console import puts

public function compute( I64 base, I64 multiplier = 5 ) -> I64:
    return base * multiplier

public function main() -> I32:
    I64 withDefault = compute( 10 )
    puts( withDefault )
    I64 withExplicit = compute( 10, 3 )
    puts( withExplicit )
    return 0
```

Output:

```
50
30
```

When calling with only the required argument, the default value for `multiplier` is used. Providing an explicit value overrides the default.

## Variadic Parameters

A function can accept a variable number of arguments using array parameter syntax. The parameter type is followed by `[]` to indicate it accepts zero or more values of that type.

```uranite
package testing

from uranite.io.console import puts

public function printAll( String items[] ) -> Void:
    for String item in items:
        puts( item )

public function main() -> I32:
    printAll( "one", "two", "three" )
    return 0
```

Output:

```
one
two
three
```

At the call site, arguments are passed as individual comma-separated values. The compiler packs them into an `Args<T>` struct internally. A function can have at most one variadic parameter, and it must be the last positional parameter.

## Keyword Parameters

Keyword parameters accept named arguments as key-value pairs. The parameter type is followed by `{}` to indicate it accepts named values.

```uranite
package testing

from uranite.io.console import puts

public function configure( String options{} ) -> Void:
    for String key, String value in options:
        puts( key )
        puts( value )

public function main() -> I32:
    configure( host="localhost", port="8080" )
    return 0
```

Output:

```
host
localhost
port
8080
```

At the call site, keyword arguments use `name=value` syntax. The compiler packs them into a `Kwargs<String, T>` struct internally. Keyword parameters enable flexible configuration-style calling patterns.

## Mutable Parameters

By default, function parameters are immutable within the function body. Adding the `mut` keyword before the type allows modification of the parameter inside the function. This creates a local mutable copy; it does not modify the original value at the call site.

```uranite
package testing

from uranite.io.console import puts

public function doubleValue( mut I64 value ) -> I64:
    value = value * 2
    return value

public function main() -> I32:
    I64 result = doubleValue( 5 )
    puts( result )
    return 0
```

Output:

```
10
```

## Nested Functions

Functions can be declared inside other functions. Nested functions have access to the enclosing function's parameters and local variables through closure capture.

```uranite
package testing

from uranite.io.console import puts

public function outer( I64 base ) -> I64:
    function inner( I64 offset ) -> I64:
        return base + offset
    return inner( 10 )

public function main() -> I32:
    I64 result = outer( 5 )
    puts( result )
    return 0
```

Output:

```
15
```

Nested functions are only visible within the scope of their enclosing function. They cannot be called from outside.

## Recursion

Functions can call themselves. Uranite supports standard recursive patterns without special syntax.

```uranite
package testing

from uranite.io.console import puts

public function factorial( I64 number ) -> I64:
    if number <= 1:
        return 1
    return number * factorial( number - 1 )

public function main() -> I32:
    I64 result = factorial( 5 )
    puts( result )
    return 0
```

Output:

```
120
```

## Exception Specification

The `raises` keyword declares which exceptions a function may raise. It appears after the return type and before the colon. Exceptions are constructed with the `new` keyword and raised with `raise`.

```uranite
package testing

from uranite.io.console import puts

public function divide( I64 numerator, I64 denominator ) -> I64 raises Exception:
    if denominator == 0:
        raise new Exception( "Division by zero", 0, None )
    return numerator / denominator

public function main() -> I32:
    try:
        I64 result = divide( 10, 0 )
        puts( result )
    except Exception as error:
        puts( "Caught exception" )
    return 0
```

Output:

```
Caught exception
```

The `raises` clause informs callers about which exception types to expect. Callers handle exceptions using `try`/`except` blocks, where `except` binds the caught exception with `as`.

## Extern Functions

The `extern` keyword declares functions with external linkage, typically for calling C library functions. Extern declarations end with a semicolon and have no body.

```uranite
package testing

from uranite.io.console import puts

extern function abs( I32 value ) -> I32;

public function main() -> I32:
    I32 result = abs( -42 )
    puts( result )
    return 0
```

Output:

```
42
```

Extern functions link against symbols available at link time. They enable interoperation with C libraries and system calls without wrapper code.
