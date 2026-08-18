# Defer

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Defer](#defer)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Basic Defer](#basic-defer)
  - [Execution Order](#execution-order)
  - [Defer Block](#defer-block)
  - [Defer in Functions](#defer-in-functions)
  - [Defer with Early Return](#defer-with-early-return)
  - [Interleaved Defer](#interleaved-defer)

## Overview

The `defer` statement schedules code to execute when the enclosing function returns. Deferred statements run in last-in, first-out order: the most recently deferred statement executes first.

Defer has two forms. The single-statement form defers one statement:

```
defer statement
```

The block form defers multiple statements:

```
defer:
    statement one
    statement two
```

Deferred code runs before every `return` in the function, including early returns. This guarantees cleanup code executes regardless of which path the function takes.

## Basic Defer

A deferred statement executes after the rest of the function body completes, just before the function returns.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    defer puts( "deferred" )
    puts( "first" )
    return 0
```

The `defer` schedules `puts( "deferred" )` for execution at function exit. The function body runs first, printing `first`. Then the deferred statement runs, printing `deferred`.

## Execution Order

When multiple `defer` statements appear, they execute in last-in, first-out order. The last statement deferred is the first to execute at function exit.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    defer puts( "third" )
    defer puts( "second" )
    defer puts( "first" )
    puts( "main" )
    return 0
```

Three deferred statements are registered in order. At function exit, they execute in reverse: `first`, then `second`, then `third`. The output is `main`, `first`, `second`, `third`.

## Defer Block

The block form defers multiple statements as a single unit. The block begins with `defer:` followed by an indented body.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    defer:
        puts( "block deferred a" )
        puts( "block deferred b" )
    puts( "main body" )
    return 0
```

The entire block is deferred as one unit. The main body runs first, printing `main body`. Then the deferred block executes, printing `block deferred a` followed by `block deferred b`.

## Defer in Functions

Deferred statements are scoped to the function they appear in. When a function with a defer returns, its deferred code runs before control returns to the caller.

```uranite
package testing

from uranite.io.console import puts

public function doWork() -> Void:
    defer puts( "cleanup" )
    puts( "working" )

public function main() -> I32:
    doWork()
    puts( "after doWork" )
    return 0
```

Inside `doWork`, the defer schedules `cleanup` for function exit. The function prints `working`, then the deferred `cleanup` runs before returning. Back in `main`, `after doWork` prints. The output is `working`, `cleanup`, `after doWork`.

## Defer with Early Return

Deferred statements run before every return point in a function, including early returns from conditional branches. This guarantees cleanup regardless of which path exits the function.

```uranite
package testing

from uranite.io.console import puts

public function earlyReturn( Boolean flag ) -> Void:
    defer puts( "deferred cleanup" )
    if flag:
        puts( "early exit" )
        return
    puts( "normal path" )

public function main() -> I32:
    earlyReturn( True )
    return 0
```

The `defer` is registered before the conditional. When `flag` is `True`, the function takes the early return path. The deferred statement still executes before the return. The output is `early exit` followed by `deferred cleanup`.

## Interleaved Defer

Defer statements can appear anywhere in the function body, interleaved with regular statements. Each defer is registered at the point it appears but executes at function exit in reverse order.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    defer puts( "last" )
    puts( "start" )
    defer puts( "middle" )
    puts( "end" )
    return 0
```

The first defer registers `last`, then `start` prints immediately. The second defer registers `middle`, then `end` prints. At function exit, deferred statements run in reverse: `middle` first, then `last`. The full output is `start`, `end`, `middle`, `last`.
