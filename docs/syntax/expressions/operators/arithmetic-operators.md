# Arithmetic Operators

---

## Table of Contents

- [Arithmetic Operators](#arithmetic-operators)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Operator Reference](#operator-reference)
  - [Integer Arithmetic](#integer-arithmetic)
    - [Addition](#addition)
    - [Subtraction](#subtraction)
    - [Multiplication](#multiplication)
    - [Division](#division)
    - [Modulo](#modulo)
  - [Float Arithmetic](#float-arithmetic)
    - [Float Operations](#float-operations)
    - [Integer vs Float Division](#integer-vs-float-division)
  - [Negation](#negation)
  - [Precedence and Parentheses](#precedence-and-parentheses)
    - [Operator Precedence](#operator-precedence)
    - [Grouping with Parentheses](#grouping-with-parentheses)
  - [Compound Assignment](#compound-assignment)
  - [String Concatenation](#string-concatenation)
  - [Arithmetic in Functions](#arithmetic-in-functions)
  - [Division Safety](#division-safety)
  - [Method Reference](#method-reference)
    - [Operators](#operators)
    - [Compound Operators](#compound-operators)
    - [Precedence Table](#precedence-table)
  - [Examples](#examples)
    - [Integer Arithmetic Showcase](#integer-arithmetic-showcase)
    - [Float Arithmetic Showcase](#float-arithmetic-showcase)
    - [Compound Assignment and Negation](#compound-assignment-and-negation)
    - [String Concatenation Showcase](#string-concatenation-showcase)

---

## Overview

Uranite provides five arithmetic operators for numeric computation: addition (`+`), subtraction (`-`), multiplication (`*`), division (`/`), and modulo (`%`). These operators work on both integer and floating-point types.

The `+` operator is also used for string concatenation when the operands are `String` values.

Compound assignment operators (`+=`, `-=`, `*=`, `/=`) combine an arithmetic operation with assignment.

---

## Operator Reference

| Operator | Operation | Integer | Float |
|---|---|---|---|
| `+` | Addition | Yes | Yes |
| `-` | Subtraction | Yes | Yes |
| `*` | Multiplication | Yes | Yes |
| `/` | Division | Yes | Yes |
| `%` | Modulo | Yes | No |

---

## Integer Arithmetic

### Addition

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 sum = 10 + 20
    puts( sum.toString() )
    return 0
```

Output:

```
30
```

### Subtraction

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 diff = 50 - 15
    puts( diff.toString() )
    return 0
```

Output:

```
35
```

### Multiplication

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 product = 6 * 7
    puts( product.toString() )
    return 0
```

Output:

```
42
```

### Division

Integer division truncates the result toward zero.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 quotient = 100 / 4
    puts( quotient.toString() )
    I64 truncated = 7 / 2
    puts( truncated.toString() )
    return 0
```

Output:

```
25
3
```

`7 / 2` produces `3`, not `3.5`. The fractional part is discarded.

### Modulo

The modulo operator returns the remainder after division.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 remainder = 17 % 5
    puts( remainder.toString() )
    return 0
```

Output:

```
2
```

`17 % 5` is `2` because 17 divided by 5 is 3 with remainder 2.

---

## Float Arithmetic

### Float Operations

Addition, subtraction, multiplication, and division work on `F64` values with full floating-point precision.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    F64 sum = 1.5 + 2.3
    puts( sum.toString() )
    F64 diff = 10.0 - 3.7
    puts( diff.toString() )
    F64 product = 2.5 * 4.0
    puts( product.toString() )
    F64 quotient = 9.0 / 2.0
    puts( quotient.toString() )
    return 0
```

Output:

```
3.8
6.3
10
4.5
```

### Integer vs Float Division

Integer division truncates. Float division preserves the fractional part.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 intDiv = 7 / 2
    puts( intDiv.toString() )
    F64 floatDiv = 7.0 / 2.0
    puts( floatDiv.toString() )
    return 0
```

Output:

```
3
3.5
```

---

## Negation

The unary `-` operator negates a numeric value.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 negative = -42
    puts( negative.toString() )

    F64 negFloat = -3.14
    puts( negFloat.toString() )

    I64 positive = 100
    I64 negated = -positive
    puts( negated.toString() )
    return 0
```

Output:

```
-42
-3.14
-100
```

Negation works on both literal values and variables.

---

## Precedence and Parentheses

### Operator Precedence

Multiplication, division, and modulo bind tighter than addition and subtraction, following standard mathematical rules.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 result = 2 + 3 * 4
    puts( result.toString() )
    return 0
```

Output:

```
14
```

`3 * 4` is evaluated first (producing `12`), then `2 + 12` produces `14`.

### Grouping with Parentheses

Parentheses override the default precedence.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 grouped = (2 + 3) * 4
    puts( grouped.toString() )
    return 0
```

Output:

```
20
```

Parentheses force `2 + 3` to evaluate first (producing `5`), then `5 * 4` produces `20`.

---

## Compound Assignment

Compound assignment operators combine an arithmetic operation with assignment to the same variable.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 counter = 0
    counter += 10
    puts( counter.toString() )
    counter -= 3
    puts( counter.toString() )
    counter *= 4
    puts( counter.toString() )
    counter /= 7
    puts( counter.toString() )
    return 0
```

Output:

```
10
7
28
4
```

Each compound operator reads the current value, performs the operation, and stores the result back.

| Operator | Equivalent |
|---|---|
| `x += y` | `x = x + y` |
| `x -= y` | `x = x - y` |
| `x *= y` | `x = x * y` |
| `x /= y` | `x = x / y` |

---

## String Concatenation

The `+` operator concatenates `String` values.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String greeting = "Hello" + " " + "World"
    puts( greeting )

    String name = "Alice"
    String message = "Hello " + name
    puts( message )
    return 0
```

Output:

```
Hello World
Hello Alice
```

To include non-string values in a concatenation, convert them with `toString()`.

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String name = "Alice"
    I64 age = 30
    String message = name + " is " + age.toString() + " years old"
    puts( message )
    return 0
```

Output:

```
Alice is 30 years old
```

---

## Arithmetic in Functions

Arithmetic expressions work in function parameters, return values, and function bodies.

```uranite
from uranite.io.console import puts

public function add( I64 first, I64 second ) -> I64:
    return first + second

public function multiply( I64 first, I64 second ) -> I64:
    return first * second

public function main() -> I32:
    I64 sum = add( 15, 27 )
    puts( sum.toString() )
    I64 product = multiply( 6, 7 )
    puts( product.toString() )
    I64 combined = add( multiply( 3, 4 ), 8 )
    puts( combined.toString() )
    return 0
```

Output:

```
42
42
20
```

---

## Division Safety

Integer division and modulo by zero are checked at compile time. The compiler automatically inserts zero-division checks before integer division and modulo operations.

---

## Method Reference

### Operators

| Operator | Operation |
|---|---|
| `+` | Addition (numeric) or concatenation (string) |
| `-` | Subtraction (binary) or negation (unary) |
| `*` | Multiplication |
| `/` | Division |
| `%` | Modulo (remainder) |

### Compound Operators

| Operator | Equivalent |
|---|---|
| `+=` | Add and assign |
| `-=` | Subtract and assign |
| `*=` | Multiply and assign |
| `/=` | Divide and assign |

### Precedence Table

| Precedence | Operators | Description |
|---|---|---|
| Highest | `*`, `/`, `%` | Multiplicative |
| Lower | `+`, `-` | Additive |
| Lowest | `+=`, `-=`, `*=`, `/=` | Compound assignment |

Parentheses `()` override any precedence level.

---

## Examples

### Integer Arithmetic Showcase

```uranite
from uranite.io.console import puts

public function calculateTotal( I64 price, I64 quantity, I64 discount ) -> I64:
    I64 subtotal = price * quantity
    I64 savings = subtotal * discount / 100
    I64 total = subtotal - savings
    return total

public function main() -> I32:
    I64 total = calculateTotal( 50, 3, 10 )
    puts( total.toString() )

    I64 remainder = 100 % 7
    puts( remainder.toString() )

    I64 combined = 10 + 20 * 3
    puts( combined.toString() )

    I64 grouped = (10 + 20) * 3
    puts( grouped.toString() )
    return 0
```

Output:

```
135
2
70
90
```

### Float Arithmetic Showcase

```uranite
from uranite.io.console import puts

public function average( F64 first, F64 second, F64 third ) -> F64:
    F64 sum = first + second + third
    return sum / 3.0

public function main() -> I32:
    F64 result = average( 10.0, 20.0, 30.0 )
    puts( result.toString() )

    F64 product = 2.5 * 4.0
    puts( product.toString() )

    F64 difference = 100.5 - 37.3
    puts( difference.toString() )

    I64 intDiv = 7 / 2
    puts( intDiv.toString() )

    F64 floatDiv = 7.0 / 2.0
    puts( floatDiv.toString() )
    return 0
```

Output:

```
20
10
63.2
3
3.5
```

### Compound Assignment and Negation

```uranite
from uranite.io.console import puts

public function main() -> I32:
    I64 counter = 0
    counter += 10
    puts( counter.toString() )
    counter -= 3
    puts( counter.toString() )
    counter *= 4
    puts( counter.toString() )
    counter /= 7
    puts( counter.toString() )

    I64 negative = -42
    puts( negative.toString() )

    I64 positive = 100
    I64 negated = -positive
    puts( negated.toString() )
    return 0
```

Output:

```
10
7
28
4
-42
-100
```

### String Concatenation Showcase

```uranite
from uranite.io.console import puts

public function main() -> I32:
    String first = "Hello"
    String second = " "
    String third = "World"
    String greeting = first + second + third
    puts( greeting )

    String name = "Alice"
    I64 age = 30
    String message = name + " is " + age.toString() + " years old"
    puts( message )

    String repeated = "ha" + "ha" + "ha"
    puts( repeated )
    return 0
```

Output:

```
Hello World
Alice is 30 years old
hahaha
```
