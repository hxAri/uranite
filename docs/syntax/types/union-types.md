# Union Types

---

## Table of Contents

- [Union Types](#union-types)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Union Parameters](#union-parameters)
    - [Two-Member Union](#two-member-union)
    - [Three-Member Union](#three-member-union)
    - [Passing Different Types](#passing-different-types)
  - [Union Parameters with Return Values](#union-parameters-with-return-values)
  - [Multiple Except Types](#multiple-except-types)
  - [Method Reference](#method-reference)
    - [Declaration Syntax](#declaration-syntax)
    - [Except Clause Syntax](#except-clause-syntax)
  - [Examples](#examples)
    - [Flexible Input Processing](#flexible-input-processing)
    - [Multi-Type Accept Function](#multi-type-accept-function)
    - [Multi-Exception Handling](#multi-exception-handling)
    - [Union Parameter Dispatch](#union-parameter-dispatch)

---

## Overview

A union type allows a parameter to accept values of more than one type. Union types are written with the pipe (`|`) operator between type names in function parameter declarations. A function with a parameter typed `I64 | String` accepts either an `I64` value or a `String` value at the call site.

The pipe operator also appears in `except` clauses to catch multiple exception types in a single handler.

---

## Union Parameters

### Two-Member Union

A function parameter can accept two types by separating them with `|`.

```uranite
from uranite.io.console import puts

public function acceptMixed( I64 | String value ) -> Void:
    puts( "received" )

public function main() -> I32:
    acceptMixed( 42 )
    acceptMixed( "hello" )
    return 0
```

Output:

```
received
received
```

The function `acceptMixed` accepts both `I64` and `String` values. The compiler automatically boxes each argument into the union representation.

### Three-Member Union

Union parameters can include three or more types.

```uranite
from uranite.io.console import puts

public function acceptAny( I64 | String | Boolean value ) -> Void:
    puts( "received value" )

public function main() -> I32:
    acceptAny( 100 )
    acceptAny( "text" )
    acceptAny( False )
    return 0
```

Output:

```
received value
received value
received value
```

### Passing Different Types

Each call to a union-typed function can pass a different member type. The compiler determines which member type matches and boxes the value accordingly.

```uranite
from uranite.io.console import puts

public function flexPrint( I64 | String value ) -> Void:
    puts( "flex" )

public function main() -> I32:
    flexPrint( 1 )
    flexPrint( "a" )
    flexPrint( 2 )
    flexPrint( "b" )
    return 0
```

Output:

```
flex
flex
flex
flex
```

---

## Union Parameters with Return Values

Functions with union parameters can return concrete types.

```uranite
from uranite.io.console import puts

public function process( I64 | String value ) -> String:
    return "processed"

public function main() -> I32:
    String result1 = process( 42 )
    String result2 = process( "hello" )
    puts( result1 )
    puts( result2 )
    return 0
```

Output:

```
processed
processed
```

The function accepts either type as input and returns a fixed `String` result regardless of which member type was passed.

---

## Multiple Except Types

The pipe operator in `except` clauses catches multiple exception types in a single handler.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    try:
        raise new ArithmeticError( "test error" )
    except ArithmeticError | Exception:
        puts( "caught" )
    return 0
```

Output:

```
caught
```

The handler `except ArithmeticError | Exception:` catches either `ArithmeticError` or `Exception`. This is not a union type — the pipe here is a delimiter that lists separate exception types.

Multiple except clauses can also be used separately.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    try:
        raise new Exception( "test" )
    except ArithmeticError:
        puts( "arithmetic" )
    except Exception:
        puts( "general" )
    return 0
```

Output:

```
general
```

---

## Method Reference

### Declaration Syntax

| Syntax | Description |
|---|---|
| `I64 \| String value` | Parameter accepting `I64` or `String` |
| `I64 \| String \| Boolean value` | Parameter accepting three types |
| `function name( I64 \| String value ) -> Type:` | Function with union parameter |

### Except Clause Syntax

| Syntax | Description |
|---|---|
| `except TypeA \| TypeB:` | Catch either exception type |
| `except TypeA:` | Catch single exception type |

---

## Examples

### Flexible Input Processing

```uranite
from uranite.io.console import puts

public function processInput( I64 | String input ) -> Void:
    puts( "input received" )

public function main() -> I32:
    processInput( 42 )
    processInput( "hello" )
    processInput( 100 )
    processInput( "world" )
    return 0
```

Output:

```
input received
input received
input received
input received
```

### Multi-Type Accept Function

```uranite
from uranite.io.console import puts

public function acceptTwo( I64 | String value ) -> Void:
    puts( "two-type" )

public function acceptThree( I64 | String | Boolean value ) -> Void:
    puts( "three-type" )

public function main() -> I32:
    acceptTwo( 42 )
    acceptTwo( "hello" )
    acceptThree( 42 )
    acceptThree( "world" )
    acceptThree( True )
    return 0
```

Output:

```
two-type
two-type
three-type
three-type
three-type
```

### Multi-Exception Handling

```uranite
from uranite.io.console import puts

public function throwArith() -> Void:
    raise new ArithmeticError( "math error" )

public function throwGeneral() -> Void:
    raise new Exception( "general error" )

public function main() -> I32:
    try:
        throwArith()
    except ArithmeticError | Exception:
        puts( "caught first" )

    try:
        throwGeneral()
    except ArithmeticError | Exception:
        puts( "caught second" )
    return 0
```

Output:

```
caught first
caught second
```

### Union Parameter Dispatch

```uranite
from uranite.io.console import puts

public function handleValue( I64 | String value ) -> String:
    return "handled"

public function handleFlag( I64 | Boolean value ) -> String:
    return "flagged"

public function main() -> I32:
    puts( handleValue( 42 ) )
    puts( handleValue( "text" ) )
    puts( handleFlag( 100 ) )
    puts( handleFlag( True ) )
    return 0
```

Output:

```
handled
handled
flagged
flagged
```
