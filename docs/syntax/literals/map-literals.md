# Map Literals

Map literals create a `HashMap<K, V>` from comma-separated key-value pairs enclosed in curly braces. Keys and values are separated by colons. The key type is inferred from the first key, and the value type from the first value.

---

## Table of Contents

- [Map Literals](#map-literals)
  - [Table of Contents](#table-of-contents)
  - [Syntax](#syntax)
  - [Empty Map](#empty-map)
  - [Type Inference](#type-inference)
  - [Example](#example)

---

## Syntax

```
{key1: value1, key2: value2}
```

Each entry is a key expression followed by a colon and a value expression. The result is a `HashMap<K, V>`:

```uranite
HashMap<String, I64> ages = {"Alice": 30, "Bob": 25}
```

---

## Empty Map

An empty pair of curly braces creates an empty `HashMap`:

```uranite
HashMap<String, I64> empty = {}
```

The type must be declared explicitly on the variable since there are no elements to infer from.

---

## Type Inference

The compiler infers the key type from the first key and the value type from the first value. All entries must have compatible types:

```uranite
HashMap<String, String> config = {"host": "localhost", "port": "8080"}
```

---

## Example

```uranite
package testing

from uranite.io.console import puts
from uranite.collection.hash-map import HashMap

public function main() -> I32:
    HashMap<String, I64> ages = {"Alice": 30, "Bob": 25}
    puts( ages.get( "Alice" ) )
    return 0
```

Output:

```
30
```

The map literal `{"Alice": 30, "Bob": 25}` creates a `HashMap<String, I64>` with two entries. Values are retrieved by key through the `get` method.
