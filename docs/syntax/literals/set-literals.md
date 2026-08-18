# Set Literals

Set literals create a `HashSet<E>` from comma-separated values enclosed in curly braces without colons. The element type is inferred from the first element.

---

## Table of Contents

- [Set Literals](#set-literals)
  - [Table of Contents](#table-of-contents)
  - [Syntax](#syntax)
  - [Disambiguation from Maps](#disambiguation-from-maps)
  - [Type Inference](#type-inference)
  - [Example](#example)

---

## Syntax

```
{element1, element2, element3}
```

Each element is an expression. All elements must have the same type. The result is a `HashSet<E>` where `E` is the element type:

```uranite
HashSet<I64> numbers = {1, 2, 3}
```

---

## Disambiguation from Maps

The parser distinguishes set literals from map literals by the presence of a colon after the first expression:

- `{1, 2, 3}` — no colon after first element, parsed as a set literal
- `{"a": 1}` — colon after first element, parsed as a map literal
- `{}` — empty curly braces, parsed as an empty map literal

To create an empty `HashSet`, use the constructor:

```uranite
HashSet<I64> empty = new HashSet<I64>()
```

---

## Type Inference

The compiler infers the element type from the first element. All subsequent elements must be compatible with that type:

```uranite
HashSet<String> tags = {"urgent", "review", "backend"}
```

---

## Example

```uranite
package testing

from uranite.io.console import puts
from uranite.collection.hash-set import HashSet

public function main() -> I32:
    HashSet<I64> numbers = {1, 2, 3}
    puts( numbers.size() )
    return 0
```

Output:

```
3
```

The set literal `{1, 2, 3}` creates a `HashSet<I64>` with three elements. Duplicate values are automatically deduplicated by the set.
