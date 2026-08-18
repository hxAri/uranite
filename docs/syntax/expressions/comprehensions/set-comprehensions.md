# Set Comprehensions

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Set Comprehensions](#set-comprehensions)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Basic Set Comprehension](#basic-set-comprehension)
  - [Expression Body](#expression-body)
  - [Inclusive Range](#inclusive-range)
  - [Filtered Set Comprehension](#filtered-set-comprehension)
  - [Automatic Deduplication](#automatic-deduplication)

## Overview

A set comprehension builds a `HashSet<T>` from an inline iteration expression. It uses curly braces with a single value expression in the body, distinguished from map comprehensions by the absence of a colon separator.

The general form is:

```
{body for Type variable in iterable}
{body for Type variable in iterable if condition}
```

The `body` expression evaluates once per iteration and its result is added to the set. Duplicate values are automatically discarded. The optional `if` clause filters iterations: only those where the condition evaluates to `True` contribute an element.

Set comprehensions iterate over ranges using `..` for exclusive upper bounds and `...` for inclusive upper bounds.

## Basic Set Comprehension

A set comprehension with an identity body collects the iteration values directly into a `HashSet`.

```uranite
package testing

from uranite.io.console import puts
from uranite.collection.hash-set import HashSet

public function main() -> I32:
    HashSet<I64> nums = {x for I64 x in 0..5}
    I64 sz = nums.size()
    puts( sz )
    return 0
```

The comprehension iterates `x` from `0` to `4` (exclusive upper bound `5`). Each value is added to the set. The resulting `HashSet` has 5 elements. The program prints `5`.

## Expression Body

The body expression transforms each iteration value before adding it to the set.

```uranite
package testing

from uranite.io.console import puts
from uranite.collection.hash-set import HashSet

public function main() -> I32:
    HashSet<I64> doubled = {x * 2 for I64 x in 0..4}
    I64 sz = doubled.size()
    puts( sz )
    return 0
```

The body expression `x * 2` doubles each value. The resulting set contains `0`, `2`, `4`, `6`. The program prints `4`.

## Inclusive Range

Using `...` instead of `..` makes the upper bound inclusive, adding the endpoint value to the iteration.

```uranite
package testing

from uranite.io.console import puts
from uranite.collection.hash-set import HashSet

public function main() -> I32:
    HashSet<I64> nums = {x for I64 x in 0...4}
    I64 sz = nums.size()
    puts( sz )
    return 0
```

The inclusive range `0...4` iterates from `0` through `4`, producing 5 elements. The program prints `5`.

## Filtered Set Comprehension

An `if` clause filters which iterations contribute elements. Only iterations where the condition evaluates to `True` add a value to the set.

```uranite
package testing

from uranite.io.console import puts
from uranite.collection.hash-set import HashSet

public function main() -> I32:
    HashSet<I64> evens = {x for I64 x in 0..10 if x % 2 == 0}
    I64 sz = evens.size()
    puts( sz )
    return 0
```

The condition `x % 2 == 0` selects only even values from the range `0` through `9`. The even values `0`, `2`, `4`, `6`, `8` produce 5 elements. The program prints `5`.

## Automatic Deduplication

A `HashSet` discards duplicate values. When the body expression produces repeated results, only unique values remain in the set.

```uranite
package testing

from uranite.io.console import puts
from uranite.collection.hash-set import HashSet

public function main() -> I32:
    HashSet<I64> mods = {x % 3 for I64 x in 0..9}
    I64 sz = mods.size()
    puts( sz )
    return 0
```

The expression `x % 3` produces values `0`, `1`, `2`, `0`, `1`, `2`, `0`, `1`, `2` across 9 iterations. The set keeps only the unique values `0`, `1`, `2`. The program prints `3`.
