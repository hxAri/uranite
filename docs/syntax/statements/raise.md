# Raise

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Raise](#raise)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Raising an Exception](#raising-an-exception)
  - [Raising from a Function](#raising-from-a-function)
  - [The Raises Annotation](#the-raises-annotation)
  - [Conditional Raise](#conditional-raise)
  - [Raising Without Raises Annotation](#raising-without-raises-annotation)
  - [Chained Exceptions](#chained-exceptions)
  - [Exception Traceback](#exception-traceback)

## Overview

The `raise` statement throws an exception object up the call stack. Execution transfers immediately to the nearest enclosing `except` clause that matches the exception type.

The `raise` keyword is followed by an expression that evaluates to an object implementing the `Throwable` interface. The standard library provides `Exception` and `Error` as concrete classes that implement `Throwable`.

The `Exception` constructor takes three parameters: a `String` message, an `I64` error code, and an optional `?Throwable` previous exception for chaining.

The `raises` annotation on a function signature declares which exception types the function may throw. It appears after the return type, before the colon.

## Raising an Exception

The `raise` statement throws a new exception object. The exception must be constructed with the `new` keyword. The caught exception object provides methods to access its message, error code, previous cause, and traceback.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    try:
        raise new Exception( "something went wrong", 0, None )
    except Exception as error:
        puts( error.getMessage() )
    return 0
```

The `raise` statement constructs an `Exception` and transfers control to the matching `except` clause. The `error` variable holds the caught exception, and `getMessage()` returns the message string. The program prints `something went wrong`.

## Raising from a Function

A function can raise an exception that propagates to the caller. The caller must handle the exception with a `try`/`except` block.

```uranite
package testing

from uranite.io.console import puts

public function riskyAction() -> Void raises Exception:
    raise new Exception( "failed", 0, None )

public function main() -> I32:
    try:
        riskyAction()
    except Exception as error:
        puts( "caught from function" )
    return 0
```

The function `riskyAction` raises an exception. The caller catches it in the `except` block and prints `caught from function`.

## The Raises Annotation

The `raises` keyword in a function signature declares which exception types the function may throw. It appears after the return type and before the colon. Multiple exception types are separated by `|`.

```uranite
public function parse( String input ) -> I64 raises Exception:
    if input == "":
        raise new Exception( "empty input", 0, None )
    return 0
```

The `raises Exception` annotation documents that `parse` may throw an `Exception`. Functions with a `raises` annotation signal to callers that exception handling may be needed.

## Conditional Raise

A function can raise an exception only when certain conditions are met. When no exception is raised, the function returns normally.

```uranite
package testing

from uranite.io.console import puts

public function checkValue( I64 value ) -> Void raises Exception:
    if value < 0:
        raise new Exception( "negative", 0, None )

public function main() -> I32:
    try:
        checkValue( 5 )
        puts( "passed" )
        checkValue( -1 )
        puts( "should not reach" )
    except Exception as error:
        puts( "caught negative" )
    return 0
```

The first call `checkValue( 5 )` succeeds because `5` is not negative, so `passed` is printed. The second call `checkValue( -1 )` raises an exception. Execution jumps to the `except` clause, skipping the second `puts`. The output is `passed` followed by `caught negative`.

## Raising Without Raises Annotation

A function can raise an exception even without a `raises` annotation. The annotation is informational and does not affect whether the exception propagates.

```uranite
package testing

from uranite.io.console import puts

public function boom() -> Void:
    raise new Exception( "no annotation", 0, None )

public function main() -> I32:
    try:
        boom()
    except Exception as error:
        puts( "caught without raises" )
    return 0
```

The function `boom` has no `raises` annotation but still throws an exception. The caller catches it and prints `caught without raises`.

## Chained Exceptions

An exception can wrap a previous exception by passing it as the third constructor parameter. This creates a chain of exceptions that preserves the original cause. The `getPrevious()` method returns the chained cause.

```uranite
package testing

from uranite.io.console import puts

public function failWithCause() -> Void raises Exception:
    Exception original = new Exception( "root cause", 1, None )
    raise new Exception( "wrapper error", 2, original )

public function main() -> I32:
    try:
        failWithCause()
    except Exception as error:
        puts( error.getMessage() )
        puts( error.getCode() )
        ?Throwable cause = error.getPrevious()
        if cause is not None:
            puts( cause.getMessage() )
            puts( cause.getCode() )
    return 0
```

The function `failWithCause` creates an original exception and wraps it in a second exception. The `except` handler accesses the wrapper's message and code, then follows the chain with `getPrevious()` to access the original cause. The output is `wrapper error`, `2`, `root cause`, `1`.

## Exception Traceback

When an exception is raised, the runtime captures a traceback containing the stack frames at the point of the raise. The `getTraceback()` method on `Exception` returns a `?Traceback` object. The `Traceback` class implements `Iterable<Frame>`, so frames can be traversed with a `for-in` loop. Each `Frame` provides `file`, `line`, `functionName`, and `moduleName` fields.

```uranite
package testing

from uranite.io.console import puts
from uranite.errors.traceback.traceback import Traceback
from uranite.errors.traceback.frame import Frame

public function inner() -> Void raises Exception:
    raise new Exception( "deep failure", 0, None )

public function outer() -> Void raises Exception:
    inner()

public function printTraceback( Traceback tb ) -> Void:
    I64 count = tb.length()
    puts( count )
    for Frame frame in tb:
        puts( frame.functionName )

public function main() -> I32:
    try:
        outer()
    except Exception as error:
        puts( error.getMessage() )
        ?Traceback tb = error.getTraceback()
        if tb is not None:
            printTraceback( tb )
    return 0
```

The exception is raised in `inner`, propagates through `outer`, and is caught in `main`. The traceback contains three frames corresponding to `main`, `outer`, and `inner`. The `printTraceback` function iterates the frames and prints each function name. The output is `deep failure`, `3`, `main`, `outer`, `inner`.
