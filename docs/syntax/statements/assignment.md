# Assignment

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Assignment](#assignment)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Simple Assignment](#simple-assignment)
  - [String Reassignment](#string-reassignment)
  - [Compound Assignment](#compound-assignment)
  - [Float Compound Assignment](#float-compound-assignment)
  - [Increment and Decrement](#increment-and-decrement)
  - [Field Assignment](#field-assignment)
  - [Compound Field Assignment](#compound-field-assignment)
  - [Assignment in Loops](#assignment-in-loops)

## Overview

Assignment replaces the value bound to an existing variable. Simple assignment uses `=`. Compound assignment operators (`+=`, `-=`, `*=`, `/=`) combine an arithmetic operation with assignment in a single step. Increment (`++`) and decrement (`--`) provide shorthand for adding or subtracting one.

The right-hand side of an assignment must be type-compatible with the declared type of the variable.

## Simple Assignment

Assignment uses `=` to replace the current value of a variable. The variable must already be declared.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 count = 0
    puts( count )
    count = 10
    puts( count )
    count = 25
    puts( count )
    return 0
```

Output:

```
0
10
25
```

Each assignment replaces the previous value entirely.

## String Reassignment

String variables can be reassigned to new string values.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    String label = "initial"
    puts( label )
    label = "updated"
    puts( label )
    return 0
```

Output:

```
initial
updated
```

## Compound Assignment

Compound assignment operators perform an arithmetic operation and assign the result back to the variable in a single step.

| Operator | Equivalent |
|---|---|
| `x += y` | `x = x + y` |
| `x -= y` | `x = x - y` |
| `x *= y` | `x = x * y` |
| `x /= y` | `x = x / y` |

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 total = 100
    total += 25
    puts( total )
    total -= 10
    puts( total )
    total *= 2
    puts( total )
    total /= 5
    puts( total )
    return 0
```

Output:

```
125
115
230
46
```

Each compound operator reads the current value, applies the operation with the right-hand operand, and stores the result.

## Float Compound Assignment

Compound assignment operators also work with floating-point types.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    F64 ratio = 0.5
    puts( ratio )
    ratio += 0.25
    puts( ratio )
    ratio *= 2.0
    puts( ratio )
    return 0
```

Output:

```
0.5
0.75
1.5
```

## Increment and Decrement

The `++` operator adds one to an integer variable. The `--` operator subtracts one.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 counter = 0
    counter++
    puts( counter )
    counter++
    puts( counter )
    counter++
    puts( counter )
    counter--
    puts( counter )
    return 0
```

Output:

```
1
2
3
2
```

Both `++` and `--` are statements, not expressions. They cannot be used inside other expressions.

## Field Assignment

Fields on class instances can be assigned directly using dot notation.

```uranite
package testing

from uranite.io.console import puts

public class Counter:
    public I64 count

    public function Counter( self, I64 count ) -> Void:
        self.count = count

public function main() -> I32:
    Counter counter = new Counter( 0 )
    puts( counter.count )
    counter.count = 42
    puts( counter.count )
    return 0
```

Output:

```
0
42
```

The field must be `public` to assign from outside the class.

## Compound Field Assignment

Compound assignment operators work on fields through dot notation.

```uranite
package testing

from uranite.io.console import puts

public class Score:
    public I64 value

    public function Score( self, I64 value ) -> Void:
        self.value = value

public function main() -> I32:
    Score score = new Score( 10 )
    score.value += 5
    puts( score.value )
    score.value *= 3
    puts( score.value )
    return 0
```

Output:

```
15
45
```

## Assignment in Loops

Assignment and compound assignment are commonly used inside loops to accumulate values.

```uranite
package testing

from uranite.io.console import puts

public function main() -> I32:
    I64 sum = 0
    for I64 index in 0..5:
        sum += index
    puts( sum )
    return 0
```

Output:

```
10
```

The loop accumulates `0 + 1 + 2 + 3 + 4 = 10` into `sum` using compound assignment.
