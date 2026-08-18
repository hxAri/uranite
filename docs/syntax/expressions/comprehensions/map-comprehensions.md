# Map Comprehensions

- [Table of Contents](#table-of-contents)

## Table of Contents

- [Map Comprehensions](#map-comprehensions)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Basic Map Comprehension](#basic-map-comprehension)
  - [Value Expressions](#value-expressions)
  - [Inclusive Range](#inclusive-range)
  - [Filtered Map Comprehension](#filtered-map-comprehension)
  - [Accessing Entries](#accessing-entries)
  - [Comprehension in a Function](#comprehension-in-a-function)

## Overview

A map comprehension builds a `HashMap<K, V>` from an inline iteration expression. It uses curly braces with a key-value pair separated by a colon in the body.

The general form is:

```
{key: value for Type variable in iterable}
{key: value for Type variable in iterable if condition}
```

The `key` and `value` expressions evaluate once per iteration. Each pair is inserted into the resulting `HashMap`. The optional `if` clause filters iterations: only those where the condition evaluates to `True` produce an entry.

Map comprehensions iterate over ranges using `..` for exclusive upper bounds and `...` for inclusive upper bounds.

## Basic Map Comprehension

A map comprehension with the loop variable as the key and a transformed expression as the value creates a `HashMap` in a single expression.

```uranite
package testing

from uranite.io.console import puts
from uranite.collection.hash-map import HashMap

public function main() -> I32:
    HashMap<I64, I64> doubled = {x: x * 2 for I64 x in 0..3}
    puts( "map comp done" )
    return 0
```

The comprehension iterates `x` from `0` to `2`. Each iteration inserts a key-value pair where the key is `x` and the value is `x * 2`. The program prints `map comp done`.

## Value Expressions

The value expression can be any expression involving the loop variable. The key and value are independent expressions.

```uranite
package testing

from uranite.io.console import puts
from uranite.collection.hash-map import HashMap

public function main() -> I32:
    HashMap<I64, I64> squares = {x: x * x for I64 x in 0..5}
    I64 sz = squares.size()
    puts( sz )
    return 0
```

The comprehension maps each key `x` to its square `x * x`. The resulting `HashMap` has 5 entries. The program prints `5`.

## Inclusive Range

Using `...` instead of `..` makes the upper bound inclusive, adding the endpoint to the iteration.

```uranite
package testing

from uranite.io.console import puts
from uranite.collection.hash-map import HashMap

public function main() -> I32:
    HashMap<I64, I64> mapped = {x: x + 10 for I64 x in 0...3}
    I64 sz = mapped.size()
    puts( sz )
    return 0
```

The inclusive range `0...3` iterates from `0` through `3`, producing 4 entries. Each value is the key plus `10`. The program prints `4`.

## Filtered Map Comprehension

An `if` clause filters which iterations produce entries. Only iterations where the condition evaluates to `True` insert a key-value pair.

```uranite
package testing

from uranite.io.console import puts
from uranite.collection.hash-map import HashMap

public function main() -> I32:
    HashMap<I64, I64> evenSquares = {x: x * x for I64 x in 0..10 if x % 2 == 0}
    I64 sz = evenSquares.size()
    puts( sz )
    return 0
```

The condition `x % 2 == 0` selects only even values from the range `0` through `9`. The even values `0`, `2`, `4`, `6`, `8` produce 5 entries. The program prints `5`.

## Accessing Entries

Entries in the resulting `HashMap` are accessed by key using the `get` method.

```uranite
package testing

from uranite.io.console import puts
from uranite.collection.hash-map import HashMap

public function main() -> I32:
    HashMap<I64, I64> squares = {x: x * x for I64 x in 0..5}
    I64 val = squares.get( 3 )
    puts( val )
    return 0
```

The comprehension maps each key to its square. Accessing key `3` returns `9`. The program prints `9`.

## Comprehension in a Function

A map comprehension can appear inside a function and be returned as the function result.

```uranite
package testing

from uranite.io.console import puts
from uranite.collection.hash-map import HashMap

public function buildMap( I64 limit ) -> HashMap<I64, I64>:
    HashMap<I64, I64> result = {x: x * 3 for I64 x in 0..limit}
    return result

public function main() -> I32:
    HashMap<I64, I64> mapped = buildMap( 4 )
    I64 sz = mapped.size()
    puts( sz )
    return 0
```

The function `buildMap` creates a `HashMap` using a comprehension with a variable upper bound. The caller receives a map with 4 entries. The program prints `4`.
