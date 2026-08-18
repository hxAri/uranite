# Break

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Break](#break)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Break in While Loops](#break-in-while-loops)
  - [Break in For-In Loops](#break-in-for-in-loops)
  - [Break in Nested Loops](#break-in-nested-loops)
  - [Break in a Search Function](#break-in-a-search-function)

## Overview

The `break` statement exits the innermost enclosing loop immediately. Execution continues with the first statement after the loop.

The `break` keyword takes no arguments. In a nested loop structure, `break` affects only the immediately enclosing loop, not any outer loops.

## Break in While Loops

The `break` statement exits a while loop, even when the loop condition is still `True`.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 count = 0
    while True:
        count = count + 1
        if count == 3:
            break
    puts( count )
    return 0
```

The `while True` loop would run indefinitely, but `break` exits when `count` reaches `3`. The program prints `3`.

## Break in For-In Loops

The `break` statement exits a for-in loop before the range is exhausted.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 total = 0
    for I64 index in 0..100:
        if index == 4:
            break
        total = total + index
    puts( total )
    return 0
```

The range covers `0` to `99`, but `break` exits at `index == 4`. Only values `0`, `1`, `2`, `3` are summed, producing `6`.

## Break in Nested Loops

The `break` statement exits only the innermost loop. The outer loop continues its iterations.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 total = 0
    for I64 row in 0..5:
        for I64 col in 0..5:
            if col == 3:
                break
            total = total + 1
    puts( total )
    return 0
```

The inner loop breaks at `col == 3`, so each inner iteration counts `3` values (`0`, `1`, `2`). The outer loop runs `5` times. The total is `5 * 3 = 15`.

## Break in a Search Function

A common pattern uses `break` to stop searching once a condition is met. The loop variable retains its value after the loop exits.

```uranite
package testing

from uranite.io.console import puts

public function findFirst( I64 threshold ) -> I64:
    I64 value = 0
    while value < 100:
        if value * value > threshold:
            break
        value = value + 1
    return value

public function main() -> I32:
    puts( findFirst( 50 ) )
    return 0
```

The function finds the first integer whose square exceeds the threshold. For threshold `50`, the value `8` satisfies `64 > 50`, so the function returns `8`.
