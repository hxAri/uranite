# For-In Loops

- [Table of Contents](#table-of-contents)

## Table of Contents

- [For-In Loops](#for-in-loops)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Iterating Over a Range](#iterating-over-a-range)
  - [Untyped Loop Variable](#untyped-loop-variable)
  - [Inclusive Ranges](#inclusive-ranges)
  - [Printing Each Iteration](#printing-each-iteration)
  - [Break in For-In Loops](#break-in-for-in-loops)
  - [Continue in For-In Loops](#continue-in-for-in-loops)
  - [Nested For-In Loops](#nested-for-in-loops)
  - [C-Style For Loops](#c-style-for-loops)
  - [C-Style Decrement Loop](#c-style-decrement-loop)
  - [For-In Loop in a Function](#for-in-loop-in-a-function)

## Overview

The `for` statement iterates over a range or iterable. The loop variable takes on each value in the sequence, and the indented body executes once per value.

Uranite supports two forms of `for` loop:

**For-in loops** iterate over a range expression or iterable object. The loop variable can include an explicit type annotation or omit it for type inference.

**C-style for loops** use an initializer, a condition, and an update expression separated by semicolons. The update expression supports `++` (increment) and `--` (decrement) operators, as well as compound assignment.

## Iterating Over a Range

A for-in loop with a typed variable iterates over an exclusive range. The range `start..end` produces values from `start` up to but not including `end`.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 total = 0
    for I64 index in 0..5:
        total = total + index
    puts( total )
    return 0
```

The range `0..5` produces values `0`, `1`, `2`, `3`, `4`. The sum is `10`.

## Untyped Loop Variable

The type annotation on the loop variable is optional. When omitted, the compiler infers the type from the iterable expression.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 total = 0
    for index in 0..4:
        total = total + index
    puts( total )
    return 0
```

The variable `index` is inferred as `I64` from the integer range. The sum of `0`, `1`, `2`, `3` is `6`.

## Inclusive Ranges

The `...` operator creates an inclusive range that includes both endpoints. The range `start...end` produces values from `start` up to and including `end`.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 total = 0
    for I64 index in 0...5:
        total = total + index
    puts( total )
    return 0
```

The range `0...5` produces values `0`, `1`, `2`, `3`, `4`, `5`. The sum is `15`.

## Printing Each Iteration

The loop body can contain any statements, including function calls that execute on each iteration.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    for I64 index in 1..4:
        puts( index )
    return 0
```

This prints `1`, `2`, and `3` on separate lines. The exclusive range `1..4` stops before `4`.

## Break in For-In Loops

The `break` statement exits a for-in loop immediately. Execution continues with the statement following the loop.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 total = 0
    for I64 index in 0..100:
        if index == 5:
            break
        total = total + index
    puts( total )
    return 0
```

The loop would iterate from `0` to `99`, but `break` exits when `index` reaches `5`. Only values `0` through `4` are summed, producing `10`.

## Continue in For-In Loops

The `continue` statement skips the remaining body of the current iteration and advances to the next value.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 total = 0
    for I64 index in 0..10:
        if index == 3 or index == 7:
            continue
        total = total + index
    puts( total )
    return 0
```

The sum of `0` through `9` is `45`. Skipping `3` and `7` subtracts `10`, producing `35`.

## Nested For-In Loops

A for-in loop can contain another for-in loop. The inner loop completes all iterations for each iteration of the outer loop.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 total = 0
    for I64 row in 0..3:
        for I64 col in 0..4:
            total = total + 1
    puts( total )
    return 0
```

The outer loop runs `3` times and the inner loop runs `4` times per outer iteration. The total count is `3 * 4 = 12`.

## C-Style For Loops

A C-style for loop uses three parts separated by semicolons: an initializer, a condition, and an update expression. The `++` operator increments the loop variable after each iteration.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 total = 0
    for I64 index = 0; index < 5; index++:
        total = total + index
    puts( total )
    return 0
```

The variable `index` starts at `0`, the loop continues while `index < 5`, and `index++` increments after each iteration. The sum of `0` through `4` is `10`.

## C-Style Decrement Loop

The `--` operator decrements the loop variable, allowing reverse iteration.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 total = 0
    for I64 index = 10; index > 0; index--:
        total = total + index
    puts( total )
    return 0
```

The variable `index` starts at `10` and decrements until it reaches `0`. The sum of `10` down to `1` is `55`.

## For-In Loop in a Function

For-in loops work inside functions. The range endpoints can be function parameters, making the iteration bounds dynamic.

```uranite
package testing

from uranite.io.console import puts

public function sumRange( I64 start, I64 end ) -> I64:
    I64 total = 0
    for I64 index in start..end:
        total = total + index
    return total

public function main() -> I32:
    puts( sumRange( 1, 11 ) )
    return 0
```

The function `sumRange` computes the sum of integers in the exclusive range `start..end`. Calling `sumRange( 1, 11 )` sums `1` through `10`, producing `55`.
