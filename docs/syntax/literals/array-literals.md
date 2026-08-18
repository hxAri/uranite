# Array Literals

Array literals create an `ArrayList<T>` from a comma-separated list of values enclosed in square brackets. The element type is inferred from the first element.

---

## Table of Contents

- [Array Literals](#array-literals)
  - [Table of Contents](#table-of-contents)
  - [Syntax](#syntax)
  - [Type Inference](#type-inference)
  - [Nested Arrays](#nested-arrays)
  - [Example](#example)

---

## Syntax

```
[element1, element2, element3]
```

Each element is an expression. All elements must have the same type. The result is an `ArrayList<T>` where `T` is the element type:

```uranite
ArrayList<I64> numbers = [10, 20, 30]
```

---

## Type Inference

The compiler infers the element type from the first element in the list. All subsequent elements must be compatible with that type:

```uranite
ArrayList<String> names = ["Alice", "Bob", "Charlie"]
ArrayList<F64> values = [1.5, 2.7, 3.14]
```

---

## Nested Arrays

Array literals can contain other array literals to create nested collections:

```uranite
ArrayList<ArrayList<I64>> matrix = [[1, 2], [3, 4], [5, 6]]
```

---

## Example

```uranite
package testing

from uranite.io.console import puts
from uranite.collection.array-list import ArrayList

public function main() -> I32:
    ArrayList<I64> numbers = [10, 20, 30]
    puts( numbers.get( 0 ) )
    puts( numbers.get( 1 ) )
    puts( numbers.get( 2 ) )
    return 0
```

Output:

```
10
20
30
```

The array literal `[10, 20, 30]` creates an `ArrayList<I64>` with three elements. Elements are accessed by index through the `get` method.
