# While Loops

- [Table of Contents](#table-of-contents)

## Table of Contents

- [While Loops](#while-loops)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Basic While Loop](#basic-while-loop)
  - [Printing Each Iteration](#printing-each-iteration)
  - [Accumulating a Sum](#accumulating-a-sum)
  - [Infinite Loop with Break](#infinite-loop-with-break)
  - [Skipping Iterations with Continue](#skipping-iterations-with-continue)
  - [Nested While Loops](#nested-while-loops)
  - [Convergence Loop](#convergence-loop)
  - [While Loop in a Function](#while-loop-in-a-function)

## Overview

The `while` statement repeatedly executes a block as long as a Boolean condition remains `True`. The condition is evaluated before each iteration. When the condition is `False` on the first check, the body never executes.

Every condition must be an explicit Boolean expression. Uranite does not support implicit truthiness.

The `break` statement exits the loop immediately. The `continue` statement skips the rest of the current iteration and re-evaluates the condition.

## Basic While Loop

A `while` loop executes its indented body repeatedly until the condition becomes `False`.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 count = 0
    while count < 5:
        count = count + 1
    puts( "done" )
    return 0
```

The loop increments `count` from `0` through `4`. When `count` reaches `5`, the condition `count < 5` is `False` and the loop exits. Execution continues with the `puts` call, printing `done`.

## Printing Each Iteration

Each iteration of the loop body can perform multiple operations, including function calls.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 index = 1
    while index <= 3:
        puts( index )
        index = index + 1
    return 0
```

This prints `1`, `2`, and `3` on separate lines. The loop variable `index` starts at `1` and increments after each print. When `index` reaches `4`, the condition fails and the loop ends.

## Accumulating a Sum

A while loop can accumulate a result across iterations by updating a variable declared before the loop.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 total = 0
    I64 number = 1
    while number <= 10:
        total = total + number
        number = number + 1
    puts( total )
    return 0
```

This computes the sum of integers from `1` to `10` and prints `55`. The variable `total` accumulates the running sum, while `number` controls the iteration count.

## Infinite Loop with Break

Using `True` as the condition creates an infinite loop. The `break` statement provides the only exit path.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 value = 0
    while True:
        value = value + 1
        if value == 5:
            break
    puts( value )
    return 0
```

The loop runs indefinitely until `value` equals `5`, at which point `break` exits the loop. The program prints `5`.

## Skipping Iterations with Continue

The `continue` statement skips the remaining statements in the current iteration and jumps back to the condition check.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 index = 0
    I64 total = 0
    while index < 10:
        index = index + 1
        if index == 3 or index == 7:
            continue
        total = total + index
    puts( total )
    return 0
```

This sums all integers from `1` to `10` except `3` and `7`. When `index` equals `3` or `7`, the `continue` statement skips the addition. The result is `55 - 3 - 7 = 45`.

## Nested While Loops

A while loop can contain another while loop. The inner loop completes all its iterations for each iteration of the outer loop.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 outer = 0
    I64 total = 0
    while outer < 3:
        I64 inner = 0
        while inner < 3:
            total = total + 1
            inner = inner + 1
        outer = outer + 1
    puts( total )
    return 0
```

The outer loop runs `3` times, and the inner loop runs `3` times for each outer iteration. The total count is `3 * 3 = 9`.

## Convergence Loop

A while loop can drive a value toward a threshold. The loop exits when the value crosses the boundary defined by the condition.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 value = 100
    while value > 1:
        value = value / 2
    puts( value )
    return 0
```

Starting from `100`, the value is repeatedly halved. Integer division produces the sequence `100, 50, 25, 12, 6, 3, 1`. When `value` reaches `1`, the condition `value > 1` is `False` and the loop exits, printing `1`.

## While Loop in a Function

While loops work inside functions and can drive the return value computation.

```uranite
package testing

from uranite.io.console import puts

public function countDown( I64 start ) -> I64:
    I64 current = start
    I64 steps = 0
    while current > 0:
        current = current - 1
        steps = steps + 1
    return steps

public function main() -> I32:
    I64 result = countDown( 7 )
    puts( result )
    return 0
```

The `countDown` function counts how many decrements are needed to reach zero. Calling `countDown( 7 )` returns `7`, which is then printed.
