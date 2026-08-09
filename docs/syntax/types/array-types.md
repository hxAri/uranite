# Array Types

---

## Table of Contents

- [Array Types](#array-types)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Creating Arrays](#creating-arrays)
    - [Array Literals](#array-literals)
    - [Empty Arrays](#empty-arrays)
    - [Typed Declarations](#typed-declarations)
  - [Accessing Elements](#accessing-elements)
    - [By Index](#by-index)
    - [First and Last](#first-and-last)
    - [Bounds Checking](#bounds-checking)
  - [Modifying Arrays](#modifying-arrays)
    - [Adding Elements](#adding-elements)
    - [Inserting Elements](#inserting-elements)
    - [Updating Elements](#updating-elements)
    - [Removing Elements](#removing-elements)
    - [Clearing](#clearing)
  - [Querying Arrays](#querying-arrays)
    - [Length and Size](#length-and-size)
    - [Emptiness Checks](#emptiness-checks)
    - [Searching](#searching)
  - [Iterating Over Arrays](#iterating-over-arrays)
    - [For-In Loops](#for-in-loops)
    - [Accumulation Patterns](#accumulation-patterns)
  - [List Comprehensions](#list-comprehensions)
    - [Basic Comprehensions](#basic-comprehensions)
    - [Filtered Comprehensions](#filtered-comprehensions)
    - [Range Iteration](#range-iteration)
  - [Transforming Arrays](#transforming-arrays)
    - [Slicing](#slicing)
    - [Copying](#copying)
    - [Reversing](#reversing)
    - [Removing Duplicates](#removing-duplicates)
    - [Taking and Dropping](#taking-and-dropping)
    - [Chunking](#chunking)
  - [Nested Arrays](#nested-arrays)
  - [Passing Arrays to Functions](#passing-arrays-to-functions)
  - [Returning Arrays from Functions](#returning-arrays-from-functions)
  - [Method Reference](#method-reference)
    - [Constructors](#constructors)
    - [Element Access](#element-access)
    - [Modification](#modification)
    - [Query](#query)
    - [Transformation](#transformation)
    - [Properties](#properties)
  - [Examples](#examples)
    - [Sum of Elements](#sum-of-elements)
    - [Filtering Values](#filtering-values)
    - [Building a Frequency Counter](#building-a-frequency-counter)
    - [Matrix as Nested Arrays](#matrix-as-nested-arrays)

---

## Overview

Arrays in Uranite are represented by `ArrayList<E>`, a resizable, generic collection that grows automatically as elements are added. Array literals like `[1, 2, 3]` produce `ArrayList` instances directly. The type parameter `E` determines what kind of elements the array holds.

`ArrayList<E>` is part of the standard library collection system. It supports indexed access, insertion, removal, searching, iteration, slicing, and list comprehensions.

---

## Creating Arrays

### Array Literals

Array literals use brackets with comma-separated values. The element type is inferred from the first element.

```uranite
ArrayList<I64> numbers = [10, 20, 30, 40, 50]
ArrayList<String> names = ["Alice", "Bob", "Charlie"]
ArrayList<Boolean> flags = [True, False, True]
```

### Empty Arrays

Empty array literals produce an untyped `ArrayList`. Declare the variable type explicitly when creating empty arrays.

```uranite
ArrayList<I64> empty = []
empty.add( 42 )
```

### Typed Declarations

The generic parameter specifies the element type. All elements added to the array must match this type.

```uranite
ArrayList<I64> scores = [95, 87, 73, 91]
ArrayList<String> labels = ["alpha", "beta", "gamma"]
```

---

## Accessing Elements

### By Index

Use `get( index )` to retrieve an element at a given position. Indices start at zero.

```uranite
ArrayList<I64> numbers = [10, 20, 30, 40, 50]
I64 first = numbers.get( 0 )
I64 third = numbers.get( 2 )
I64 last = numbers.get( 4 )
```

### First and Last

The `first` and `last` properties return the first and last elements without requiring an index.

```uranite
ArrayList<I64> numbers = [10, 20, 30, 40, 50]
I64 head = numbers.first
I64 tail = numbers.last
```

### Bounds Checking

Use `hasIndex( index )` to check whether an index is valid before accessing it.

```uranite
ArrayList<I64> numbers = [10, 20, 30]
Boolean valid = numbers.hasIndex( 2 )
Boolean invalid = numbers.hasIndex( 3 )
```

---

## Modifying Arrays

### Adding Elements

`add( element )` appends a single element to the end. `addAll( otherList )` appends all elements from another array.

```uranite
ArrayList<I64> numbers = [1, 2, 3]
numbers.add( 4 )

ArrayList<I64> more = [5, 6, 7]
numbers.addAll( more )
```

### Inserting Elements

`insert( index, element )` places an element at the specified position, shifting subsequent elements forward.

```uranite
ArrayList<I64> numbers = [10, 20, 30]
numbers.insert( 1, 15 )
```

After insertion, the array contains `[10, 15, 20, 30]`.

### Updating Elements

`set( index, element )` replaces the element at the given index.

```uranite
ArrayList<I64> numbers = [10, 20, 30]
numbers.set( 0, 999 )
```

After the update, index `0` holds `999`.

### Removing Elements

Several removal methods are available:

```uranite
ArrayList<I64> numbers = [10, 20, 30, 40, 50]
I64 removed = numbers.removeAt( 2 )
I64 head = numbers.removeFirst()
I64 tail = numbers.removeLast()
Boolean didRemove = numbers.removeElement( 20 )
```

`removeAt( index )` removes and returns the element at that index. `removeFirst()` and `removeLast()` remove and return the first or last element. `removeElement( element )` removes the first occurrence of the given value and returns whether it was found.

### Clearing

`clear()` removes all elements from the array.

```uranite
ArrayList<I64> numbers = [1, 2, 3]
numbers.clear()
```

After clearing, `length()` returns `0`.

---

## Querying Arrays

### Length and Size

Both `length()` and the `size` property return the number of elements. They are equivalent.

```uranite
ArrayList<I64> numbers = [10, 20, 30, 40, 50]
I64 count = numbers.length()
I64 alsoCount = numbers.size
```

### Emptiness Checks

The `isEmpty` and `isNotEmpty` properties check whether the array has elements.

```uranite
ArrayList<I64> numbers = [10, 20, 30]
Boolean notEmpty = numbers.isNotEmpty
Boolean empty = numbers.isEmpty
```

### Searching

`contains( element )` and `exists( element )` both check whether a value exists in the array. `indexOf( element )` returns the index of the first occurrence, or `-1` if not found.

```uranite
ArrayList<I64> numbers = [10, 20, 30, 40, 50]
Boolean found = numbers.contains( 30 )
Boolean missing = numbers.contains( 99 )
I64 position = numbers.indexOf( 30 )
I64 notFound = numbers.indexOf( 999 )
```

`indexOf` returns `2` for `30` and `-1` for `999`.

---

## Iterating Over Arrays

### For-In Loops

The `for-in` loop iterates over each element in the array. Declare the element type and a variable name.

```uranite
ArrayList<String> fruits = ["apple", "banana", "cherry"]
for String fruit in fruits:
    puts( fruit )
```

### Accumulation Patterns

Use a loop variable to accumulate a result across all elements.

```uranite
ArrayList<I64> numbers = [10, 20, 30, 40, 50]
I64 total = 0
for I64 number in numbers:
    total = total + number
```

After the loop, `total` holds `150`.

---

## List Comprehensions

List comprehensions create new arrays by transforming or filtering ranges and collections in a single expression.

### Basic Comprehensions

A comprehension applies an expression to each value in a range.

```uranite
ArrayList<I64> squares = [x * x for I64 x in 0..5]
```

This produces `[0, 1, 4, 9, 16]` — the squares of `0` through `4`. The range `0..5` is exclusive of the end value.

### Filtered Comprehensions

Add an `if` condition to include only elements that pass the filter.

```uranite
ArrayList<I64> evens = [x for I64 x in 0..20 if x % 2 == 0]
```

This produces `[0, 2, 4, 6, 8, 10, 12, 14, 16, 18]`.

```uranite
ArrayList<I64> bigOnly = [x for I64 x in 0..10 if x > 5]
```

This produces `[6, 7, 8, 9]`.

### Range Iteration

Ranges use the `..` operator. The start value is inclusive and the end value is exclusive.

```uranite
ArrayList<I64> tripled = [x * 3 for I64 x in 1..6]
```

This produces `[3, 6, 9, 12, 15]`.

---

## Transforming Arrays

### Slicing

`slice( start, end )` returns a new array containing elements from index `start` up to but not including index `end`.

```uranite
ArrayList<I64> numbers = [10, 20, 30, 40, 50]
ArrayList<I64> middle = numbers.slice( 1, 4 )
```

The result is `[20, 30, 40]`.

### Copying

The `copy` property creates an independent copy of the array. Modifying the copy does not affect the original.

```uranite
ArrayList<I64> original = [1, 2, 3]
ArrayList<I64> cloned = original.copy
cloned.add( 4 )
```

After this, `original` still has `3` elements while `cloned` has `4`.

### Reversing

The `reversed` property returns a new array with elements in reverse order. The original array is unchanged.

```uranite
ArrayList<I64> numbers = [1, 2, 3, 4, 5]
ArrayList<I64> backward = numbers.reversed
```

The result is `[5, 4, 3, 2, 1]`.

### Removing Duplicates

`distinct()` returns a new array with duplicate elements removed, preserving the order of first occurrences.

```uranite
ArrayList<I64> duplicates = [1, 2, 3, 2, 1, 4, 3, 5]
ArrayList<I64> unique = duplicates.distinct()
```

The result is `[1, 2, 3, 4, 5]`.

### Taking and Dropping

`take( count )` returns a new array with the first `count` elements. `drop( count )` returns a new array without the first `count` elements.

```uranite
ArrayList<I64> numbers = [10, 20, 30, 40, 50]
ArrayList<I64> firstThree = numbers.take( 3 )
ArrayList<I64> afterTwo = numbers.drop( 2 )
```

`firstThree` contains `[10, 20, 30]`. `afterTwo` contains `[30, 40, 50]`.

### Chunking

`chunked( size )` splits the array into smaller arrays of the given size. The last chunk may be smaller if the array length is not evenly divisible.

```uranite
ArrayList<I64> numbers = [1, 2, 3, 4, 5, 6, 7]
ArrayList<ArrayList<I64>> chunks = numbers.chunked( 3 )
```

This produces three chunks: `[1, 2, 3]`, `[4, 5, 6]`, and `[7]`.

---

## Nested Arrays

Arrays can hold other arrays. Use `ArrayList<ArrayList<E>>` for two-dimensional structures.

```uranite
ArrayList<ArrayList<I64>> matrix = []
ArrayList<I64> row1 = [1, 2, 3]
ArrayList<I64> row2 = [4, 5, 6]
matrix.add( row1 )
matrix.add( row2 )

ArrayList<I64> firstRow = matrix.get( 0 )
I64 topLeft = firstRow.get( 0 )
I64 topRight = firstRow.get( 2 )
```

---

## Passing Arrays to Functions

Arrays are passed to functions by reference. The function receives the same array object, not a copy.

```uranite
public function printAll( ArrayList<I64> items ) -> Void:
    for I64 item in items:
        puts( item.toString() )

public function main() -> I32:
    ArrayList<I64> data = [100, 200, 300]
    printAll( data )
    return 0
```

---

## Returning Arrays from Functions

Functions can create and return arrays. The caller receives the array created inside the function.

```uranite
public function makeList() -> ArrayList<I64>:
    ArrayList<I64> result = [1, 2, 3, 4, 5]
    return result

public function main() -> I32:
    ArrayList<I64> list = makeList()
    puts( list.length().toString() )
    puts( list.get( 0 ).toString() )
    return 0
```

---

## Method Reference

### Constructors

| Constructor | Description |
|---|---|
| `ArrayList<E>()` | Creates an empty array with default capacity |

### Element Access

| Method | Return Type | Description |
|---|---|---|
| `get( Int index )` | `E` | Returns element at the given index |
| `hasIndex( Int index )` | `Boolean` | Returns whether the index is valid |
| `indexOf( E element )` | `Int` | Returns index of first occurrence, or `-1` |
| `lastIndexOf( E element )` | `Int` | Returns index of last occurrence, or `-1` |

### Modification

| Method | Return Type | Description |
|---|---|---|
| `add( E element )` | `Void` | Appends element to the end |
| `addAll( ArrayList<E> elements )` | `Void` | Appends all elements from another array |
| `insert( Int index, E element )` | `Void` | Inserts element at the given index |
| `set( Int index, E element )` | `Void` | Replaces element at the given index |
| `removeAt( Int index )` | `E` | Removes and returns element at the index |
| `removeFirst()` | `E` | Removes and returns the first element |
| `removeLast()` | `E` | Removes and returns the last element |
| `removeElement( E element )` | `Boolean` | Removes first occurrence, returns whether found |
| `swap( Int indexA, Int indexB )` | `Void` | Swaps elements at the two indices |
| `clear()` | `Void` | Removes all elements |

### Query

| Method | Return Type | Description |
|---|---|---|
| `length()` | `Int` | Returns the number of elements |
| `contains( E element )` | `Boolean` | Returns whether the element exists |
| `exists( E element )` | `Boolean` | Returns whether the element exists |

### Transformation

| Method | Return Type | Description |
|---|---|---|
| `slice( Int start, Int end )` | `ArrayList<E>` | Returns elements from start to end (exclusive) |
| `take( Int count )` | `ArrayList<E>` | Returns first count elements |
| `drop( Int count )` | `ArrayList<E>` | Returns elements after dropping the first count |
| `distinct()` | `ArrayList<E>` | Returns array with duplicates removed |
| `chunked( Int size )` | `ArrayList<ArrayList<E>>` | Splits into chunks of the given size |
| `toString()` | `String` | Returns string representation of the array |

### Properties

| Property | Type | Description |
|---|---|---|
| `first` | `E` | The first element |
| `last` | `E` | The last element |
| `size` | `Int` | The number of elements |
| `isEmpty` | `Boolean` | Whether the array has no elements |
| `isNotEmpty` | `Boolean` | Whether the array has elements |
| `copy` | `ArrayList<E>` | An independent copy of the array |
| `reversed` | `ArrayList<E>` | A new array with elements in reverse order |
| `iterator` | `Iterator<E>` | An iterator for traversing elements |

---

## Examples

### Sum of Elements

```uranite
from uranite.io.console import puts

public function sumList( ArrayList<I64> numbers ) -> I64:
    I64 total = 0
    for I64 number in numbers:
        total = total + number
    return total

public function main() -> I32:
    ArrayList<I64> values = [10, 20, 30, 40, 50]
    I64 result = sumList( values )
    puts( result.toString() )
    return 0
```

Output:

```
150
```

### Filtering Values

```uranite
from uranite.io.console import puts

public function main() -> I32:
    ArrayList<I64> numbers = [5, 3, 8, 1, 9, 2, 7, 4, 6]

    ArrayList<I64> large = []
    for I64 value in numbers:
        if value > 5:
            large.add( value )

    for I64 value in large:
        puts( value.toString() )

    return 0
```

Output:

```
8
9
7
6
```

### Building a Frequency Counter

```uranite
from uranite.io.console import puts

public function countOccurrences( ArrayList<I64> data, I64 target ) -> I64:
    I64 count = 0
    for I64 element in data:
        if element == target:
            count = count + 1
    return count

public function main() -> I32:
    ArrayList<I64> rolls = [3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5]
    I64 fiveCount = countOccurrences( rolls, 5 )
    puts( fiveCount.toString() )
    return 0
```

Output:

```
3
```

### Matrix as Nested Arrays

```uranite
from uranite.io.console import puts

public function main() -> I32:
    ArrayList<ArrayList<I64>> grid = []

    ArrayList<I64> row0 = [1, 2, 3]
    ArrayList<I64> row1 = [4, 5, 6]
    ArrayList<I64> row2 = [7, 8, 9]
    grid.add( row0 )
    grid.add( row1 )
    grid.add( row2 )

    I64 rowIndex = 0
    while rowIndex < grid.length():
        ArrayList<I64> row = grid.get( rowIndex )
        I64 colIndex = 0
        I64 rowSum = 0
        while colIndex < row.length():
            rowSum = rowSum + row.get( colIndex )
            colIndex = colIndex + 1
        puts( rowSum.toString() )
        rowIndex = rowIndex + 1
    return 0
```

Output:

```
6
15
24
```
