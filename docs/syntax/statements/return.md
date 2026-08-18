# Return

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Return](#return)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Returning a Value](#returning-a-value)
  - [Void Return](#void-return)
  - [Returning Expressions](#returning-expressions)
  - [Conditional Return](#conditional-return)
  - [Early Return](#early-return)
  - [Returning a Boolean](#returning-a-boolean)
  - [Return from a Loop](#return-from-a-loop)
  - [Return with Nested Calls](#return-with-nested-calls)

## Overview

The `return` statement exits the current function and optionally provides a value to the caller. Every function with a non-`Void` return type must return a value on all execution paths.

When a function returns `Void`, the `return` statement is written without an expression. The `return` keyword alone exits the function immediately.

The expression after `return` is evaluated before the function exits. This expression can be a variable, a literal, an arithmetic computation, or a function call.

## Returning a Value

A function with a typed return value uses `return` followed by an expression that matches the declared return type.

```uranite
package testing

from uranite.io.console import puts

public function add( I64 left, I64 right ) -> I64:
    return left + right

public function main() -> I32:
    puts( add( 3, 4 ) )
    return 0
```

The function `add` computes `left + right` and returns the result. The caller receives `7`, which is printed.

## Void Return

A function returning `Void` uses `return` without an expression. This is useful for exiting a function early before the end of its body.

```uranite
package testing

from uranite.io.console import puts

public function greet( String name ) -> Void:
    puts( name )
    return

public function main() -> I32:
    greet( "hello" )
    return 0
```

The `return` statement ends the function after printing. In `Void` functions, the `return` at the end of the body is optional but permitted.

## Returning Expressions

The value after `return` can be any expression, including arithmetic operations and function calls.

```uranite
package testing

from uranite.io.console import puts

public function square( I64 value ) -> I64:
    return value * value

public function cube( I64 value ) -> I64:
    return value * value * value

public function main() -> I32:
    puts( square( 5 ) )
    puts( cube( 3 ) )
    return 0
```

The `square` function returns `25` and `cube` returns `27`. The multiplication is computed before the value is returned.

## Conditional Return

A function can have multiple `return` statements in different branches of a conditional. The first branch that executes determines the return value.

```uranite
package testing

from uranite.io.console import puts

public function label( I64 code ) -> String:
    if code == 0:
        return "zero"
    elif code == 1:
        return "one"
    return "other"

public function main() -> I32:
    puts( label( 0 ) )
    puts( label( 1 ) )
    puts( label( 5 ) )
    return 0
```

Each branch returns a different string. The final `return "other"` acts as the default path when no condition matches. This prints `zero`, `one`, and `other`.

## Early Return

Multiple `return` statements can serve as guard clauses, exiting the function early when boundary conditions are met.

```uranite
package testing

from uranite.io.console import puts

public function clamp( I64 value, I64 low, I64 high ) -> I64:
    if value < low:
        return low
    if value > high:
        return high
    return value

public function main() -> I32:
    puts( clamp( -5, 0, 100 ) )
    puts( clamp( 50, 0, 100 ) )
    puts( clamp( 200, 0, 100 ) )
    return 0
```

The `clamp` function checks boundary conditions first and returns early. For `-5`, it returns the lower bound `0`. For `200`, it returns the upper bound `100`. For `50`, neither guard triggers and the original value is returned.

## Returning a Boolean

A function can return `Boolean` values for use in conditional expressions.

```uranite
package testing

from uranite.io.console import puts

public function isEven( I64 value ) -> Boolean:
    if value % 2 == 0:
        return True
    return False

public function main() -> I32:
    if isEven( 4 ):
        puts( "even" )
    else:
        puts( "odd" )
    if isEven( 7 ):
        puts( "even" )
    else:
        puts( "odd" )
    return 0
```

The function `isEven` returns `True` when the value is divisible by `2`, and `False` otherwise. The caller uses the returned Boolean directly as an `if` condition. This prints `even` and `odd`.

## Return from a Loop

A `return` statement inside a loop exits both the loop and the enclosing function immediately.

```uranite
package testing

from uranite.io.console import puts

public function findIndex( I64 target ) -> I64:
    for I64 index in 0..10:
        if index == target:
            return index
    return -1

public function main() -> I32:
    puts( findIndex( 5 ) )
    puts( findIndex( 99 ) )
    return 0
```

When `target` is found in the range, `return index` exits both the loop and the function. When the loop completes without finding the target, `return -1` provides a sentinel value. This prints `5` and `-1`.

## Return with Nested Calls

The expression after `return` can include calls to other functions. The inner function is evaluated first, and its result becomes the return value.

```uranite
package testing

from uranite.io.console import puts

public function absolute( I64 value ) -> I64:
    if value < 0:
        return value * -1
    return value

public function difference( I64 left, I64 right ) -> I64:
    return absolute( left - right )

public function main() -> I32:
    puts( difference( 10, 3 ) )
    puts( difference( 3, 10 ) )
    return 0
```

The `difference` function computes `left - right` and passes the result to `absolute` before returning. Both `difference( 10, 3 )` and `difference( 3, 10 )` return `7`.
