# Tuple Literals

Tuple literals create a `Tuple<E>` from comma-separated values enclosed in parentheses. Tuples are fixed-size containers with indexed element access.

---

## Table of Contents

- [Tuple Literals](#tuple-literals)
  - [Table of Contents](#table-of-contents)
  - [Syntax](#syntax)
  - [Element Access](#element-access)
  - [Size](#size)
  - [Example](#example)

---

## Syntax

```
(element1, element2, element3)
```

Each element is an expression. The result is a `Tuple<E>`:

```uranite
Tuple<I64> pair = (10, 20)
Tuple<I64> triple = (1, 2, 3)
```

---

## Element Access

Individual elements are accessed by index through the `component` method. Indices are zero-based:

```uranite
Tuple<I64> pair = (10, 20)
I64 first = pair.component( 0 )
I64 second = pair.component( 1 )
```

---

## Size

The `size` method returns the number of elements in the tuple:

```uranite
Tuple<I64> triple = (10, 20, 30)
I64 count = triple.size()
```

---

## Example

```uranite
package testing

from uranite.io.console import puts
from uranite.collection.tuple import Tuple

public function main() -> I32:
    Tuple<I64> triple = (10, 20, 30)
    I64 first = triple.component( 0 )
    I64 second = triple.component( 1 )
    puts( first )
    puts( second )
    puts( triple.size() )
    return 0
```

Output:

```
10
20
3
```

The tuple literal `(10, 20, 30)` creates a `Tuple<I64>` with three elements. Elements are accessed by zero-based index through `component`, and the element count is returned by `size`.
