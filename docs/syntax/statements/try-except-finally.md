# Try, Except, and Finally

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Try, Except, and Finally](#try-except-and-finally)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Basic Try and Except](#basic-try-and-except)
  - [Catching a Raised Exception](#catching-a-raised-exception)
  - [Except Without Variable Binding](#except-without-variable-binding)
  - [Bare Except](#bare-except)
  - [The Finally Block](#the-finally-block)
  - [Finally with Exception](#finally-with-exception)
  - [Try with Finally Only](#try-with-finally-only)
  - [Nested Try Blocks](#nested-try-blocks)
  - [Union Type Except](#union-type-except)
  - [No Exception Path](#no-exception-path)

## Overview

The `try` statement establishes an exception-handling context. Code inside the `try` block is monitored for exceptions. When an exception occurs, execution transfers to the first matching `except` clause.

The `except` clause declares which exception type to handle. It optionally binds the caught exception to a variable using `as`. Multiple exception types can be combined with `|` in a single clause.

The `finally` block runs after the `try` block completes, whether an exception occurred or not. It guarantees cleanup code executes regardless of the outcome.

The general form is:

```
try:
    monitored code
except ExceptionType as variable:
    handler code
finally:
    cleanup code
```

Both `except` and `finally` are optional, but at least one must be present. Multiple `except` clauses can appear before a single `finally`.

## Basic Try and Except

A `try` block wraps code that may raise an exception. The `except` clause handles the exception if one occurs. When no exception is raised, the `except` block is skipped.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    try:
        puts( "inside try" )
    except Exception as error:
        puts( "caught" )
    return 0
```

No exception is raised, so only `inside try` is printed. The `except` block is skipped entirely.

## Catching a Raised Exception

When a `raise` statement executes inside a `try` block, execution immediately transfers to the matching `except` clause. Code after the `raise` within the `try` block does not execute. The caught exception object provides `getMessage()` and `getCode()` to access its data.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    try:
        raise new Exception( "connection failed", 503, None )
    except Exception as error:
        puts( error.getMessage() )
        puts( error.getCode() )
    return 0
```

The `raise` statement creates an `Exception` and transfers control to the `except` clause. The handler accesses the exception's message and error code. The output is `connection failed` followed by `503`.

## Except Without Variable Binding

The `as` keyword and variable name are optional. When the handler does not need access to the exception object, the type alone is sufficient.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    try:
        raise new Exception( "test", 0, None )
    except Exception:
        puts( "caught without binding" )
    return 0
```

The `except Exception:` clause catches the exception without binding it to a variable. The program prints `caught without binding`.

## Bare Except

An `except` clause with no type and no variable catches any exception. This acts as a catch-all handler.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    try:
        raise new Exception( "error", 0, None )
    except:
        puts( "bare catch" )
    return 0
```

The bare `except:` clause catches the exception regardless of its type. The program prints `bare catch`.

## The Finally Block

The `finally` block executes after the `try` block completes, regardless of whether an exception was raised. It runs after `try` when no exception occurs, and after the `except` handler when one does.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    try:
        puts( "try block" )
    except Exception as error:
        puts( "caught" )
    finally:
        puts( "finally runs" )
    return 0
```

No exception is raised, so the `except` block is skipped. The `finally` block runs after the `try` block. The output is `try block` followed by `finally runs`.

## Finally with Exception

When an exception is raised and caught, the `finally` block still executes after the `except` handler completes.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    try:
        raise new Exception( "timeout", 0, None )
    except Exception as error:
        puts( error.getMessage() )
    finally:
        puts( "cleanup done" )
    return 0
```

The exception is raised, caught by the `except` clause which prints the message, and then the `finally` block runs. The output is `timeout` followed by `cleanup done`.

## Try with Finally Only

A `try` block can pair directly with a `finally` block without any `except` clauses. This guarantees cleanup code runs even if no exception handling is needed at this level.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    try:
        puts( "try only" )
    finally:
        puts( "finally only" )
    return 0
```

The `try` block runs, then the `finally` block runs unconditionally. The output is `try only` followed by `finally only`.

## Nested Try Blocks

`try` blocks can be nested. An inner `except` clause handles exceptions raised in the inner `try` block. If the inner handler succeeds, execution continues in the outer `try` block.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    try:
        try:
            raise new Exception( "inner", 0, None )
        except Exception as inner:
            puts( "inner caught" )
        puts( "after inner try" )
    except Exception as outer:
        puts( "outer caught" )
    return 0
```

The inner `try` raises an exception, which the inner `except` handles. Execution resumes after the inner `try` block, printing `after inner try`. The outer `except` is never reached. The output is `inner caught` followed by `after inner try`.

## Union Type Except

A single `except` clause can handle multiple exception types by separating them with `|`. The handler executes if the raised exception matches any of the listed types.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    try:
        raise new Exception( "test", 0, None )
    except Exception | Error as error:
        puts( "union caught" )
    return 0
```

The `except Exception | Error as error:` clause catches either `Exception` or `Error` types. The program prints `union caught`.

## No Exception Path

When the `try` block completes without raising an exception, the `except` clause is skipped and the `finally` block runs. This demonstrates the normal control flow through a complete `try`/`except`/`finally` statement.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    try:
        puts( "no exception" )
    except Exception as error:
        puts( "should not print" )
    finally:
        puts( "always runs" )
    return 0
```

The `try` block succeeds, the `except` block is skipped, and the `finally` block runs. The output is `no exception` followed by `always runs`.
