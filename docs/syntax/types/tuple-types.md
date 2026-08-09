# Tuple Types

---

## Table of Contents

- [Tuple Types](#tuple-types)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Creating Tuples](#creating-tuples)
    - [Constructor](#constructor)
    - [Setting Elements](#setting-elements)
  - [Accessing Elements](#accessing-elements)
    - [By Index](#by-index)
    - [By Component](#by-component)
  - [Length and Size](#length-and-size)
  - [Iterating Over Tuples](#iterating-over-tuples)
    - [For-In Loops](#for-in-loops)
    - [Accumulation](#accumulation)
  - [Concatenation](#concatenation)
  - [Passing Tuples to Functions](#passing-tuples-to-functions)
  - [Returning Tuples from Functions](#returning-tuples-from-functions)
  - [Method Reference](#method-reference)
    - [Constructors](#constructors)
    - [Element Access](#element-access)
    - [Modification](#modification)
    - [Query](#query)
    - [Properties](#properties)
  - [Examples](#examples)
    - [Coordinate Pair](#coordinate-pair)
    - [Sum of Tuple Elements](#sum-of-tuple-elements)
    - [Merging Two Tuples](#merging-two-tuples)
    - [Multiple Return Values](#multiple-return-values)

---

## Overview

A `Tuple<E>` is a fixed-size, ordered sequence of elements. Unlike `ArrayList<E>`, tuples do not grow or shrink after creation. The size is set at construction time and remains constant. Tuples are useful for grouping a known number of values together, such as coordinates, pairs, or fixed records.

`Tuple<E>` is a generic class in the standard library. The type parameter `E` determines the element type. Tuples implement `Iterable<E>`, so they work with `for-in` loops.

To use tuples, import from `uranite.collection.tuple`:

```uranite
from uranite.collection.tuple import Tuple
```

---

## Creating Tuples

### Constructor

Create a tuple by specifying its size. The constructor allocates space for exactly that many elements.

```uranite
from uranite.collection.tuple import Tuple

Tuple<I64> point = new Tuple<I64>( 3 )
```

This creates a tuple that holds exactly 3 integer elements.

### Setting Elements

After construction, populate elements using `set( index, value )`. Indices start at zero.

```uranite
from uranite.collection.tuple import Tuple

Tuple<I64> point = new Tuple<I64>( 3 )
point.set( 0, 10 )
point.set( 1, 20 )
point.set( 2, 30 )
```

---

## Accessing Elements

### By Index

Use `get( index )` to retrieve an element at a given position.

```uranite
from uranite.collection.tuple import Tuple

Tuple<I64> point = new Tuple<I64>( 3 )
point.set( 0, 10 )
point.set( 1, 20 )
point.set( 2, 30 )

I64 first = point.get( 0 )
I64 second = point.get( 1 )
I64 third = point.get( 2 )
```

### By Component

The `component( index )` method is equivalent to `get( index )`. Both return the element at the specified position.

```uranite
from uranite.collection.tuple import Tuple

Tuple<I64> data = new Tuple<I64>( 2 )
data.set( 0, 42 )
data.set( 1, 99 )

I64 value = data.component( 1 )
```

---

## Length and Size

The `length` property and `size()` method both return the number of elements in the tuple. They are equivalent.

```uranite
from uranite.collection.tuple import Tuple

Tuple<I64> point = new Tuple<I64>( 3 )
point.set( 0, 10 )
point.set( 1, 20 )
point.set( 2, 30 )

I64 count = point.length
I64 alsoCount = point.size()
```

Both return `3`.

---

## Iterating Over Tuples

### For-In Loops

`Tuple<E>` implements `Iterable<E>`, so tuples work directly with `for-in` loops.

```uranite
from uranite.collection.tuple import Tuple

Tuple<I64> numbers = new Tuple<I64>( 5 )
numbers.set( 0, 10 )
numbers.set( 1, 20 )
numbers.set( 2, 30 )
numbers.set( 3, 40 )
numbers.set( 4, 50 )

for I64 value in numbers:
    puts( value.toString() )
```

Output:

```
10
20
30
40
50
```

### Accumulation

Use a loop variable to compute a result from all tuple elements.

```uranite
from uranite.collection.tuple import Tuple

Tuple<I64> scores = new Tuple<I64>( 4 )
scores.set( 0, 90 )
scores.set( 1, 85 )
scores.set( 2, 92 )
scores.set( 3, 78 )

I64 total = 0
for I64 score in scores:
    total = total + score
```

After the loop, `total` holds `345`.

---

## Concatenation

The `concatenate( other )` method creates a new tuple containing all elements from both tuples. Neither original tuple is modified.

```uranite
from uranite.collection.tuple import Tuple

Tuple<I64> first = new Tuple<I64>( 3 )
first.set( 0, 1 )
first.set( 1, 2 )
first.set( 2, 3 )

Tuple<I64> second = new Tuple<I64>( 2 )
second.set( 0, 4 )
second.set( 1, 5 )

Tuple<I64> combined = first.concatenate( second )
```

The result has 5 elements: `1, 2, 3, 4, 5`.

---

## Passing Tuples to Functions

Tuples are passed to functions by reference. The function receives the same tuple object.

```uranite
from uranite.io.console import puts
from uranite.collection.tuple import Tuple

public function printTuple( Tuple<I64> values ) -> Void:
    for I64 value in values:
        puts( value.toString() )
```

---

## Returning Tuples from Functions

Functions can create and return tuples. This is useful for returning multiple values from a function.

```uranite
from uranite.collection.tuple import Tuple

public function makePair( I64 first, I64 second ) -> Tuple<I64>:
    Tuple<I64> pair = new Tuple<I64>( 2 )
    pair.set( 0, first )
    pair.set( 1, second )
    return pair
```

---

## Method Reference

### Constructors

| Constructor | Description |
|---|---|
| `Tuple<E>( Int size )` | Creates a tuple with the given fixed size |

### Element Access

| Method | Return Type | Description |
|---|---|---|
| `get( Int index )` | `E` | Returns element at the given index |
| `component( Int index )` | `E` | Returns element at the given index |

### Modification

| Method | Return Type | Description |
|---|---|---|
| `set( Int index, E value )` | `Void` | Stores a value at the given index |

### Query

| Method | Return Type | Description |
|---|---|---|
| `size()` | `Int` | Returns the number of elements |
| `concatenate( Tuple<E> other )` | `Tuple<E>` | Returns a new tuple with elements from both |
| `equals( Object other )` | `Boolean` | Compares for equality |
| `hashCode()` | `Int` | Returns a hash code |

### Properties

| Property | Type | Description |
|---|---|---|
| `length` | `Int` | The number of elements |
| `iterator` | `Iterator<E>` | An iterator for traversing elements |

---

## Examples

### Coordinate Pair

```uranite
from uranite.io.console import puts
from uranite.collection.tuple import Tuple

public function main() -> I32:
    Tuple<I64> point = new Tuple<I64>( 2 )
    point.set( 0, 150 )
    point.set( 1, 300 )

    puts( point.get( 0 ).toString() )
    puts( point.get( 1 ).toString() )
    puts( point.size().toString() )
    return 0
```

Output:

```
150
300
2
```

### Sum of Tuple Elements

```uranite
from uranite.io.console import puts
from uranite.collection.tuple import Tuple

public function sumTuple( Tuple<I64> values ) -> I64:
    I64 total = 0
    for I64 value in values:
        total = total + value
    return total

public function main() -> I32:
    Tuple<I64> data = new Tuple<I64>( 4 )
    data.set( 0, 10 )
    data.set( 1, 20 )
    data.set( 2, 30 )
    data.set( 3, 40 )

    I64 result = sumTuple( data )
    puts( result.toString() )
    return 0
```

Output:

```
100
```

### Merging Two Tuples

```uranite
from uranite.io.console import puts
from uranite.collection.tuple import Tuple

public function main() -> I32:
    Tuple<I64> first = new Tuple<I64>( 3 )
    first.set( 0, 1 )
    first.set( 1, 2 )
    first.set( 2, 3 )

    Tuple<I64> second = new Tuple<I64>( 2 )
    second.set( 0, 4 )
    second.set( 1, 5 )

    Tuple<I64> merged = first.concatenate( second )
    puts( merged.size().toString() )
    for I64 value in merged:
        puts( value.toString() )

    return 0
```

Output:

```
5
1
2
3
4
5
```

### Multiple Return Values

```uranite
from uranite.io.console import puts
from uranite.collection.tuple import Tuple

public function divideWithRemainder( I64 dividend, I64 divisor ) -> Tuple<I64>:
    I64 quotient = dividend / divisor
    I64 remainder = dividend % divisor
    Tuple<I64> result = new Tuple<I64>( 2 )
    result.set( 0, quotient )
    result.set( 1, remainder )
    return result

public function main() -> I32:
    Tuple<I64> answer = divideWithRemainder( 17, 5 )
    I64 quotient = answer.get( 0 )
    I64 remainder = answer.get( 1 )
    puts( quotient.toString() )
    puts( remainder.toString() )
    return 0
```

Output:

```
3
2
```
