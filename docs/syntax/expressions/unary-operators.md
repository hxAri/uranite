# Unary Operators

---

## Table of Contents

- [Unary Operators](#unary-operators)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Operator Reference](#operator-reference)
  - [Arithmetic Negation](#arithmetic-negation)
    - [Integer Negation](#integer-negation)
    - [Float Negation](#float-negation)
  - [Logical NOT](#logical-not)
  - [Bitwise NOT](#bitwise-not)
  - [Increment and Decrement](#increment-and-decrement)
    - [Postfix Increment and Decrement](#postfix-increment-and-decrement)
    - [Prefix Increment and Decrement](#prefix-increment-and-decrement)
    - [Multiple Increments](#multiple-increments)
  - [Move](#move)
    - [Moving Primitives](#moving-primitives)
    - [Moving Objects](#moving-objects)
  - [Method Reference](#method-reference)
    - [Operators](#operators)
  - [Examples](#examples)
    - [Negation and Bitwise NOT](#negation-and-bitwise-not)
    - [Increment and Decrement Showcase](#increment-and-decrement-showcase)
    - [Logical NOT Patterns](#logical-not-patterns)
    - [Move Semantics](#move-semantics)

---

## Overview

Uranite provides several prefix unary operators that operate on a single operand:

- `-` — arithmetic negation
- `not` — logical negation
- `~` — bitwise complement
- `++` / `--` — increment and decrement (prefix and postfix)
- `move` — explicit ownership transfer

Unary operators have higher precedence than all binary operators.

---

## Operator Reference

| Operator | Operation | Operand Type | Result Type |
|---|---|---|---|
| `-` | Arithmetic negation | Numeric | Same numeric type |
| `not` | Logical NOT | `Boolean` | `Boolean` |
| `~` | Bitwise NOT | Integer | Integer |
| `++` | Increment by 1 | Integer | Integer |
| `--` | Decrement by 1 | Integer | Integer |
| `move` | Ownership transfer | Any | Same type |

---

## Arithmetic Negation

### Integer Negation

The `-` operator negates an integer value.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 positive = 42
    I64 negated = -positive
    puts( negated.toString() )

    I64 negative = -100
    I64 absolute = -negative
    puts( absolute.toString() )
    return 0
```

Output:

```
-42
100
```

Negating a negative value produces a positive result.

### Float Negation

The `-` operator works on floating-point values.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 temperature = 15.5
    F64 below = -temperature
    puts( below.toString() )
    return 0
```

Output:

```
-15.5
```

---

## Logical NOT

The `not` keyword inverts a `Boolean` value. See [Logical Operators](logical-operators.md) for full details.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Boolean active = True
    Boolean inactive = not active
    puts( inactive.toString() )
    return 0
```

Output:

```
False
```

---

## Bitwise NOT

The `~` operator inverts all bits of an integer value. See [Bitwise Operators](bitwise-operators.md) for full details.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 result = ~0
    puts( result.toString() )
    I64 flipped = ~15
    puts( flipped.toString() )
    return 0
```

Output:

```
-1
-16
```

---

## Increment and Decrement

The `++` operator adds 1 to an integer variable. The `--` operator subtracts 1. Both have prefix and postfix forms.

### Postfix Increment and Decrement

Postfix form places the operator after the variable name.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 value = 10
    value++
    puts( value.toString() )
    value--
    puts( value.toString() )
    return 0
```

Output:

```
11
10
```

### Prefix Increment and Decrement

Prefix form places the operator before the variable name.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 value = 10
    ++value
    puts( value.toString() )
    --value
    puts( value.toString() )
    return 0
```

Output:

```
11
10
```

### Multiple Increments

Increment and decrement can be applied multiple times.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 counter = 0
    counter++
    counter++
    counter++
    puts( counter.toString() )
    counter--
    puts( counter.toString() )
    return 0
```

Output:

```
3
2
```

---

## Move

The `move` keyword explicitly transfers ownership of a value. After a move, the source variable is consumed and cannot be used again.

### Moving Primitives

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 alpha = 100
    I64 beta = move alpha
    puts( beta.toString() )
    return 0
```

Output:

```
100
```

After `move alpha`, the value is transferred to `beta`. Using `alpha` after this point would produce a compile-time error.

### Moving Objects

```uranite
from uranite.io.console import puts

class Container:

    public String content

    public function Container( self, String content ) -> Void:
        self.content = content

public function main() -> I32:
    Container source = new Container( "important data" )
    Container destination = move source
    puts( destination.content )
    return 0
```

Output:

```
important data
```

The `Container` object is moved from `source` to `destination`. The `source` variable is consumed after the move.

---

## Method Reference

### Operators

| Operator | Position | Description |
|---|---|---|
| `-value` | Prefix | Negates numeric value |
| `not value` | Prefix | Inverts boolean |
| `~value` | Prefix | Inverts all bits |
| `value++` | Postfix | Increment by 1 |
| `value--` | Postfix | Decrement by 1 |
| `++value` | Prefix | Increment by 1 |
| `--value` | Prefix | Decrement by 1 |
| `move value` | Prefix | Transfer ownership |

---

## Examples

### Negation and Bitwise NOT

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 positive = 42
    I64 negated = -positive
    puts( negated.toString() )

    F64 temperature = 15.5
    F64 below = -temperature
    puts( below.toString() )

    I64 negative = -100
    I64 absolute = -negative
    puts( absolute.toString() )

    I64 bitFlip = ~0
    puts( bitFlip.toString() )
    I64 bitFlip2 = ~15
    puts( bitFlip2.toString() )
    return 0
```

Output:

```
-42
-15.5
100
-1
-16
```

### Increment and Decrement Showcase

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 counter = 0
    counter++
    counter++
    counter++
    puts( counter.toString() )

    counter--
    puts( counter.toString() )

    I64 value = 10
    ++value
    puts( value.toString() )
    --value
    puts( value.toString() )
    return 0
```

Output:

```
3
2
11
10
```

### Logical NOT Patterns

```uranite
from uranite.io.console import puts

public function main() -> I32:
    Boolean active = True
    Boolean inactive = not active
    puts( inactive.toString() )

    Boolean empty = False
    Boolean notEmpty = not empty
    puts( notEmpty.toString() )

    I64 age = 25
    if not (age < 18):
        puts( "adult" )

    Boolean ready = True
    Boolean willing = True
    if not ready or not willing:
        puts( "not prepared" )
    else:
        puts( "prepared" )
    return 0
```

Output:

```
False
True
adult
prepared
```

### Move Semantics

```uranite
from uranite.io.console import puts

class Container:

    public String content

    public function Container( self, String content ) -> Void:
        self.content = content

public function main() -> I32:
    Container source = new Container( "important data" )
    Container destination = move source
    puts( destination.content )

    I64 alpha = 100
    I64 beta = move alpha
    puts( beta.toString() )
    return 0
```

Output:

```
important data
100
```
