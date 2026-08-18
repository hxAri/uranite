# Continue

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Continue](#continue)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Continue in While Loops](#continue-in-while-loops)
  - [Continue in For-In Loops](#continue-in-for-in-loops)
  - [Continue in Nested Loops](#continue-in-nested-loops)
  - [Continue with Break](#continue-with-break)

## Overview

The `continue` statement skips the remaining statements in the current iteration and advances to the next iteration of the innermost enclosing loop. In a `while` loop, this re-evaluates the condition. In a `for-in` loop, this advances to the next value in the range or iterable.

The `continue` keyword takes no arguments. In a nested loop structure, `continue` affects only the immediately enclosing loop, not any outer loops.

## Continue in While Loops

The `continue` statement skips the rest of the loop body and re-evaluates the while condition.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 index = 0
    I64 total = 0
    while index < 6:
        index = index + 1
        if index == 3:
            continue
        total = total + index
    puts( total )
    return 0
```

When `index` equals `3`, `continue` skips the addition. The sum is `1 + 2 + 4 + 5 + 6 = 18`.

## Continue in For-In Loops

The `continue` statement skips the rest of the loop body and advances to the next value in the range.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 total = 0
    for I64 index in 0..10:
        if index == 2 or index == 5 or index == 8:
            continue
        total = total + index
    puts( total )
    return 0
```

The sum of `0` through `9` is `45`. Skipping `2`, `5`, and `8` subtracts `15`, producing `30`.

## Continue in Nested Loops

The `continue` statement skips one iteration of the innermost loop without affecting the outer loop.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 total = 0
    for I64 row in 0..3:
        for I64 col in 0..4:
            if col == 1:
                continue
            total = total + 1
    puts( total )
    return 0
```

The inner loop has `4` iterations per row. Skipping `col == 1` leaves `3` counted iterations per row. With `3` outer iterations, the total is `3 * 3 = 9`.

## Continue with Break

Both `continue` and `break` can appear in the same loop. The `continue` skips specific iterations while `break` provides an early exit condition.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 total = 0
    for I64 index in 0..20:
        if index == 15:
            break
        if index == 5 or index == 10:
            continue
        total = total + index
    puts( total )
    return 0
```

The loop processes values `0` through `14` (breaking at `15`). Values `5` and `10` are skipped by `continue`. The sum of `0` through `14` is `105`, minus `5` and `10` equals `90`.
