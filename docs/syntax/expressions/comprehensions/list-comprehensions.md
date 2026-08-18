# List Comprehensions

- [Table of Contents](#table-of-contents)

## Table of Contents

- [List Comprehensions](#list-comprehensions)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Basic List Comprehension](#basic-list-comprehension)
  - [Expression Body](#expression-body)
  - [Inclusive Range](#inclusive-range)
  - [Filtered Comprehension](#filtered-comprehension)
  - [Accessing Elements](#accessing-elements)
  - [Comprehension in a Function](#comprehension-in-a-function)

## Overview

A list comprehension builds an `ArrayList<T>` from an inline iteration expression. It combines creating a collection, iterating over a range, transforming each element, and optionally filtering, into a single expression enclosed in square brackets.

The general form is:

```
[body for Type variable in iterable]
[body for Type variable in iterable if condition]
```

The `body` expression evaluates once per iteration and its result is added to the list. The optional `if` clause filters elements: only iterations where the condition evaluates to `True` contribute to the result.

List comprehensions iterate over ranges using `..` for exclusive upper bounds and `...` for inclusive upper bounds.

## Basic List Comprehension

A list comprehension with an identity body collects the iteration values directly into an `ArrayList`.

```uranite
package testing

from uranite.io.console import puts
from uranite.collection.array-list import ArrayList

public function main() -> I32:
    ArrayList<I64> numbers = [x for I64 x in 0..5]
    I64 sz = numbers.size()
    puts( sz )
    return 0
```

The comprehension iterates `x` from `0` to `4` (exclusive upper bound `5`). Each value is added to the list. The resulting `ArrayList` has 5 elements. The program prints `5`.

## Expression Body

The body expression transforms each iteration value before adding it to the list. Any expression using the loop variable is valid.

```uranite
package testing

from uranite.io.console import puts
from uranite.collection.array-list import ArrayList

public function main() -> I32:
    ArrayList<I64> doubled = [x * 2 for I64 x in 0..4]
    I64 sz = doubled.size()
    puts( sz )
    return 0
```

The body expression `x * 2` doubles each value. The resulting list contains `0`, `2`, `4`, `6`. The program prints `4`.

## Inclusive Range

Using `...` instead of `..` makes the upper bound inclusive, adding the endpoint value to the iteration.

```uranite
package testing

from uranite.io.console import puts
from uranite.collection.array-list import ArrayList

public function main() -> I32:
    ArrayList<I64> numbers = [x for I64 x in 0...4]
    I64 sz = numbers.size()
    puts( sz )
    return 0
```

The inclusive range `0...4` iterates from `0` through `4`, producing 5 elements. The program prints `5`.

## Filtered Comprehension

An `if` clause after the iterable filters which elements are added to the list. Only iterations where the condition evaluates to `True` contribute an element.

```uranite
package testing

from uranite.io.console import puts
from uranite.collection.array-list import ArrayList

public function main() -> I32:
    ArrayList<I64> evens = [x for I64 x in 0..10 if x % 2 == 0]
    I64 sz = evens.size()
    puts( sz )
    return 0
```

The condition `x % 2 == 0` selects only even values. From the range `0` through `9`, the even values are `0`, `2`, `4`, `6`, `8`. The program prints `5`.

## Accessing Elements

Elements in the resulting `ArrayList` are accessed by index using the `get` method. The list preserves insertion order.

```uranite
package testing

from uranite.io.console import puts
from uranite.collection.array-list import ArrayList

public function main() -> I32:
    ArrayList<I64> squares = [x * x for I64 x in 0..5]
    I64 first = squares.get( 0 )
    I64 last = squares.get( 4 )
    puts( first )
    puts( last )
    return 0
```

The comprehension produces squares: `0`, `1`, `4`, `9`, `16`. The first element is `0` and the last is `16`. The program prints `0` followed by `16`.

## Comprehension in a Function

A list comprehension can appear inside a function and be returned as the function result.

```uranite
package testing

from uranite.io.console import puts
from uranite.collection.array-list import ArrayList

public function makeRange( I64 limit ) -> ArrayList<I64>:
    ArrayList<I64> result = [x for I64 x in 0..limit]
    return result

public function main() -> I32:
    ArrayList<I64> items = makeRange( 3 )
    I64 sz = items.size()
    puts( sz )
    return 0
```

The function `makeRange` creates an `ArrayList` using a comprehension with a variable upper bound. The caller receives a list with 3 elements. The program prints `3`.
